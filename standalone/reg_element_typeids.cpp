// standalone/reg_element_typeids.cpp
// Registers the WHOLE UI-element type-id dispatch-slot table as ONE batch.
//
// ── What these slots are ─────────────────────────────────────────────────────
// The exe's .rdata holds one dispatch slot per UI element class. Each slot's
// *initial* content is the RVA of the mangled name of a Runtime DLL data export:
//
//     0x140151510 -> "?_typeInfo@TableElement@UI@@0VBaseTypeInfo@Object@@A"
//
// Retail's startup name-resolution overwrites the slot with the ADDRESS of that
// Object::BaseTypeInfo object, whose first dword is the type id. Every reader in
// the tree therefore does a double deref — `MOV RAX,[slot]; MOV EDX,[RAX]` — which
// is what `ReadDispatchTypeId` (tradeui.cpp:45) and the ~13 `void** exref` blocks
// in engine_funcs.cpp:2207-3567 reimplement.
//
// In standalone NOTHING ever registered these addresses, so `ResolveAddr` returned
// 0 for all of them, every `exref` block failed its `if (exref && *exref)` guard,
// and every `g_TypeId_*Element` global stayed NULL for the life of the project.
//
// ── Why the whole table at once ──────────────────────────────────────────────
// `TradeUI_FindTypedElement` (tradeui.cpp:161) is `if (elem && typeId)`: a null
// type-id returns null EVEN WHEN THE ELEMENT EXISTS. TradeUI_Serialize looks up 15
// named elements across six different element classes, so with the table null the
// entire element set (s+0xC0/0xC8/0xD0/0xD8/0xE0/0xE8/0xF0/0xF8/0x100/0x108/0x110/
// 0x118/0x120/0x138/0x140) is null. It crashes at +0x138 only because that is the
// first one *dereferenced* rather than merely stored. Registering these one at a
// time therefore buys exactly one crash each — hence one batch.
//
// ── Names are from the binary, not from our labels ───────────────────────────
// Every name below was dumped from Application-x64.exe's own .rdata by following
// slot -> RVA -> C string, then confirmed to be an export of Runtime-x64.dll.
// Two long-standing labels WERE wrong; the ENDGAME B.3.6 closing pass renamed
// the globals to the class the slot names (values unchanged):
//
//   0x1401516c8  g_TypeId_SpinnerElement -> g_TypeId_NumberElement   (UI::NumberElement)
//   0x140151cf0  g_TypeId_SliderElement  -> g_TypeId_SpinnerElement  (UI::SpinnerElement)
//   (and g_TypeId_UIElementBase -> _ButtonElement, _ToolbarElement ->
//    _CheckElement, _ObjectInternal -> _Element, g_pExternalTypeId -> _Dialog)
//
// The real UI::SliderElement slot is 0x1401516d0 and had never been referenced by
// anything in the tree. Note what that meant for tradeui.cpp's SLOT_SLIDER_ELEM (an
// unused #define, deleted by the B.3.6 closing pass)
// and the "SliderElement has NO exported SetMinMax" comment at tradeui.cpp:61 —
// those were reasoning about the SpinnerElement slot under a Slider name. Nothing
// here changes their behaviour; they are flagged, not fixed.
//
// The full table was derived mechanically: walk 0x140151000-0x140152000 eight bytes
// at a time, treat each qword as an RVA, and keep the ones whose target string
// matches `?_typeInfo@<Class>@<Ns>@@0VBaseTypeInfo@Object@@A`. That yields 34 slots,
// of which the UI element classes are the ones below.
//
// (townhallui.cpp:2497 had already noted the first of those.) The label being
// wrong does not change what gets registered — the slot address is the identity —
// but do not "fix" a caller by swapping globals on the strength of the name.
//
// ── Why ResolveExport and not GetTypeIdByName ────────────────────────────────
// These are Runtime-registered types, not exe REGISTER_TYPE ones, so
// `Object::GetTypeIdByName` returns 0 for all of them (the trap
// toolbar_vtable_standalone.cpp:135 documents). Pointing the slot at the Runtime's
// own BaseTypeInfo export is both simpler and strictly more faithful: the Runtime
// fills the id inside its own Startup exactly as in retail, so there is no
// post-Startup fill hook here and no window where the storage holds 0xFFFFFFFF.
// It also means this must run AFTER ResolveInit — it does: AddressRegistryInit is
// called at main.cpp:1526, ResolveInit at 1498, EngineFuncsInit (the first reader)
// at 1531.
//
// ── Why it is GATED ──────────────────────────────────────────────────────────
// Filling this table wakes dormant readers well outside trade: toolbardialog,
// optionsdialog, saveloaddialog, helpui, townhallui, spritecaptureui and
// tutorialgoalcount all currently take a null-guarded skip on these globals. That
// is a broad default-path change, so it is opt-in for its first runs:
// BANISHED_LIVE_ELEMENT_TIDS=1. Gate off = no registration = byte-for-byte
// identical to today.
#include "resolve.h"
#include <cstdint>
#include <cstdio>

namespace {

struct ElementTypeSlot {
    uintptr_t   slotAddr;   // exe .rdata dispatch slot (Ghidra address)
    const char* mangled;    // Runtime-x64.dll BaseTypeInfo data export
    const char* label;      // our engine_funcs global name, for the trace line
};

// Backing storage for the slots. Each entry holds the Object::BaseTypeInfo* that
// retail's resolver would have written; readers deref it to reach the type id.
const ElementTypeSlot kElementTypeSlots[] = {
    {0x1401514f8, "?_typeInfo@ButtonElement@UI@@0VBaseTypeInfo@Object@@A",       "HelpButtonElem/ButtonElement"},
    {0x140151500, "?_typeInfo@ImageElement@UI@@0VBaseTypeInfo@Object@@A",        "ImageElement"},
    {0x140151508, "?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A",         "TextElement"},
    {0x140151510, "?_typeInfo@TableElement@UI@@0VBaseTypeInfo@Object@@A",        "TableElement"},
    {0x140151668, "?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A",        "LabelElement"},
    {0x140151690, "?_typeInfo@ComboElement@UI@@0VBaseTypeInfo@Object@@A",        "HelpComboElem/ComboElement"},
    {0x1401516a8, "?_typeInfo@CheckElement@UI@@0VBaseTypeInfo@Object@@A",        "CheckElement/CheckboxElement"},
    {0x1401516c8, "?_typeInfo@NumberElement@UI@@0VBaseTypeInfo@Object@@A",       "NumberElement"},
    {0x1401516d0, "?_typeInfo@SliderElement@UI@@0VBaseTypeInfo@Object@@A",       "SliderElement (the REAL one)"},
    {0x1401517e8, "?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A",       "HelpFirstElem/ScrollElement"},
    {0x1401517f0, "?_typeInfo@DocumentTextElement@UI@@0VBaseTypeInfo@Object@@A", "HelpDocTextElem"},
    {0x1401517f8, "?_typeInfo@ContainerElement@UI@@0VBaseTypeInfo@Object@@A",    "HelpContainerElem"},
    {0x140151cb0, "?_typeInfo@NumberEditElement@UI@@0VBaseTypeInfo@Object@@A",   "NumberEditElement"},
    {0x140151cf0, "?_typeInfo@SpinnerElement@UI@@0VBaseTypeInfo@Object@@A",      "SpinnerElement"},
    // Remainder of the UI element table. No current reader needs these, but the
    // whole point of this file is that a partial table costs one crash per missing
    // entry, and the derivation below is mechanical, so completing it is free.
    {0x140151660, "?_typeInfo@Element@UI@@0VBaseTypeInfo@Object@@A",             "Element (base)"},
    {0x1401515b8, "?_typeInfo@ProgressElement@UI@@0VBaseTypeInfo@Object@@A",     "ProgressElement"},
    {0x140151720, "?_typeInfo@RibbonElement@UI@@0VBaseTypeInfo@Object@@A",       "RibbonElement"},
    {0x140151a58, "?_typeInfo@BorderElement@UI@@0VBaseTypeInfo@Object@@A",       "BorderElement"},
    {0x140151b50, "?_typeInfo@EditElement@UI@@0VBaseTypeInfo@Object@@A",         "EditElement"},
    {0x140151dd8, "?_typeInfo@GraphElement@UI@@0VBaseTypeInfo@Object@@A",        "GraphElement"},
    {0x140151de0, "?_typeInfo@TabElement@UI@@0VBaseTypeInfo@Object@@A",          "TabElement"},
};

constexpr int kElementTypeSlotCount =
    static_cast<int>(sizeof(kElementTypeSlots) / sizeof(kElementTypeSlots[0]));

void* g_elementTypeInfo[kElementTypeSlotCount] = {};

}  // namespace

void RegisterElementTypeIds() {
    // This function runs from AddressRegistryInit (main.cpp:1531), which is BEFORE
    // main.cpp:1683 rebinds stderr to standalone_error.log — so every fprintf(stderr)
    // below goes to a console nobody reads and is lost. That made the report useless
    // exactly when it mattered: "no [ElemTid] line" looked like "the gate is off" when
    // it could equally have been "the gate is on and the report was discarded".
    // Mirror everything to our own file, which has no such ordering dependency.
    FILE* rep = fopen("standalone_elemtid.log", "w");

    if (rep) fprintf(rep, "[ElemTid] registering %d slots\n",
                     kElementTypeSlotCount);

    int resolved = 0;
    for (int i = 0; i < kElementTypeSlotCount; i++) {
        const ElementTypeSlot& s = kElementTypeSlots[i];
        g_elementTypeInfo[i] = ResolveExport(s.mangled);
        if (!g_elementTypeInfo[i]) {
            // Report the OUTCOME, not the attempt: leave the slot UNREGISTERED so
            // every reader keeps its existing null-guarded skip rather than
            // IsDerivedFrom'ing against a garbage id.
            fprintf(stderr, "[ElemTid] %s (%s): export NOT FOUND — slot left unregistered\n",
                    s.label, s.mangled);
            if (rep) fprintf(rep, "[ElemTid] MISS %-32s %s  (export NOT FOUND)\n",
                             s.label, s.mangled);
            continue;
        }
        RegisterExeAddr(s.slotAddr, reinterpret_cast<uintptr_t>(&g_elementTypeInfo[i]));
        resolved++;
        if (rep) fprintf(rep, "[ElemTid] OK   %-32s slot=0x%llx typeInfo=%p id=%u\n",
                         s.label, (unsigned long long)s.slotAddr, g_elementTypeInfo[i],
                         *reinterpret_cast<uint32_t*>(g_elementTypeInfo[i]));
    }

    fprintf(stderr, "[ElemTid] registered %d/%d element type-id slots\n",
            resolved, kElementTypeSlotCount);
    if (rep) {
        fprintf(rep, "[ElemTid] registered %d/%d element type-id slots\n",
                resolved, kElementTypeSlotCount);
        fclose(rep);
    }
    fflush(stderr);
}

// Post-Engine::Startup trace. The ids live in the Runtime's own BaseTypeInfo
// objects and are assigned by the Runtime's Startup, so there is nothing to fill
// here — this only PROVES the ids became non-zero, which is the difference
// between "registered" and "working" that reference_registered_but_never_executed
// is about. Any zero printed here means the double-deref reaches a live object
// whose id was never assigned, which would be a real finding.
extern "C" void ReportTypeIdGlobals_elements() {
    for (int i = 0; i < kElementTypeSlotCount; i++) {
        if (!g_elementTypeInfo[i]) continue;
        fprintf(stderr, "[ElemTid] %-32s slot=%p typeInfo=%p id=%u\n",
                kElementTypeSlots[i].label,
                reinterpret_cast<void*>(kElementTypeSlots[i].slotAddr),
                g_elementTypeInfo[i],
                *reinterpret_cast<uint32_t*>(g_elementTypeInfo[i]));
    }
    fflush(stderr);
}
