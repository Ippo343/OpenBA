// ============================================================================
// runtime/src/ui/tooltipdialog.cpp — Wave 10D lane TIP
// ============================================================================
//
// `UI::ToolTipDialog` — the four census rows Wave 10C's lane UIF6 created when
// it ported `?BeginToolTip@UserInterface@UI@@` and resolved that body's callees
// BY NAME rather than opening silent address doors
// (reference_porting_can_grow_the_cutover_surface.md). This file is the
// follow-through, plus the three CODE slots of that class's vtable.
//
//   0x18004cfd0  ??2ToolTipDialog@UI@@SAPEAX_K@Z                (class op new)
//   0x1800356a0  ?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ
//   0x1800dab00  ?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z
//   0x18004cfc0  ?TypeName@ToolTipDialog@UI@@UEBAPEBDXZ     vtable slot 1
//   0x18004cfa0  ?Type@ToolTipDialog@UI@@UEBAIXZ            vtable slot 2
//
// Vtable slot 0 (0x18004d270, the unexported VECTOR deleting destructor) is NOT
// here: it is row 39 of the one-body family in
// runtime/src/object/deleting_dtor.cpp, exactly as
// lane DLG8 put `??_7Dialog@UI@@6B@`'s slot 0 there. That file's table is
// re-derived from the shipped DLL by tools/rt_deleting_dtor_verify.py, which is
// the whole reason a 39th hand-written copy does not belong in this file.
//
// ---------------------------------------------------------------------------
// ABSENCE ESTABLISHED BEFORE A LINE WAS WRITTEN
// ---------------------------------------------------------------------------
// All five addresses and all five mangled names were grepped across `code/`,
// `runtime/`, `engine/` and `standalone/` before any of this was written
// (reference_grep_for_the_port_before_writing_it.md — three lanes in one wave
// re-ported what the tree already had, and a duplicate registration is
// last-binding-wins, which silently makes one body dead code). The only hits
// were CONSUMERS: `RT_EXPORT_ONCE` call sites inside
// runtime/src/ui/userinterface_hover.cpp, plus one comment. A body that is
// ported but never REGISTERED is indistinguishable from an absent one to a
// census (reference_half_registered_port_reads_as_absent.md), so the grep was
// by ADDRESS *and* by NAME, not by class string.
//
// ---------------------------------------------------------------------------
// `??_7ToolTipDialog@UI@@6B@` IS NOT IN THIS FILE, AND WHY
// ---------------------------------------------------------------------------
// It is exported DATA (0x180157880) and it is NOT a row in
// standalone/rt_data_mint_tables.inc. It becomes one the moment
// tools/rt_mint_gen.py is re-run — that generator's source-scan rule matches
// the literal `"??_7ToolTipDialog@UI@@6B@"` which userinterface_hover.cpp:410
// already spells, so the generated table is STALE (it predates Wave 10C), not
// the rule wrong. Regenerating is a whole-file change that re-runs the census
// and would capture three other live lanes mid-wave, so this lane did not do it
// silently: the wave owner regenerates once at wave close.
//
// **Minting is not porting.** Lane MINT established that by measurement in Wave
// 10B: it minted `??_7Dialog@UI@@6B@` and the UI census set-diff came back
// EMPTY. A `??_7` row leaves the census only when it is PROMOTED — registered
// from standalone/reg_mint_promoted.cpp with the symbol spelled as a literal.
// The promotion warrant this lane owes is written out in
// docs/runtime_port/WAVE10D_LANE_TIP.md, ready to apply.
//
// What THIS file buys that regeneration alone does not: the minted row will
// read `READY … TRAP=0` immediately, because the vtable's ten slots are
//
//   slot 0  0x18004d270  vector deleting dtor   -> deleting_dtor.cpp row 39
//   slot 1  0x18004cfc0  TypeName               -> here
//   slot 2  0x18004cfa0  Type                   -> here
//   slot 3  0x180114fa0  Persist               |
//   slot 4  0x180114fb0  OnOpened              |
//   slot 5  0x180115330  OnClosed              |  DialogInstance's OWN slot
//   slot 6  0x1801153a0  OnUpdateLayout        }  targets, byte-identical,
//   slot 7  0x180115400  OnUpdate              |  and `??_7DialogInstance` is
//   slot 8  0x180114ed0  OnElementEvent        |  already 10/10 and PROMOTED
//   slot 9  0x180115470  OnSerializeSettings   |
//
// and slots 3..9 were COMPARED BY ADDRESS against
// `??_7DialogInstance@UI@@6B@` (0x180156d70), not assumed to be inherited.
// All seven match exactly; ToolTipDialog overrides nothing but slots 0/1/2.
// The 10-slot extent is pinned by the adjacent export: 0x180157880 + 10*8 ==
// 0x1801578d0 == `??_7DialogLoaderLoopT@Engine@@6B@`.
//
// ---------------------------------------------------------------------------
// EVERY BODY BELOW WAS READ FROM THE DISASSEMBLY
// ---------------------------------------------------------------------------
// Ghidra's decompiled C drops dereferences and prints MOVSXD as a zero
// extension (reference_ghidra_c_output_vs_disassembly.md). The decompiler
// output was used only to name things; every branch, every offset and every
// signedness below comes from the instruction listing, which is transcribed
// inline against each statement.
// ============================================================================

#include <cstdint>
#include <cstdio>

#include "../../funcs/rt_port.h"
#include "../../types/system/String.h"
#include "../../types/ui/Element.h"
#include "../../types/ui/TextElement.h"
#include "../system/string.h"   // RtSystem_String_Set / _Assign, RtString_*

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h> and so
// must not be included from runtime/src (rt_port.h explains why). Declared here
// exactly as ~30 sibling runtime files already declare it.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module m, uintptr_t ghidra_addr);

namespace {

using UI::Element;
namespace TE = UI::TextElementOffsets;

// ---------------------------------------------------------------------------
// ENTRY COUNTERS — attendance, and NOT a verification of anything.
// ---------------------------------------------------------------------------
// These count ENTRIES, unconditionally, on every build, and print
// unconditionally at exit.
//
// They exist because a green readiness screen is not a route. `READY / TRAP=0`
// on the minted vtable says the table is safe to stamp, not that anything
// dispatches it — two UI vtables are green-and-unpromoted right now for exactly
// that reason. A zero here means the row is registered and DEAD whatever else
// the board says.
//
// The EFFECTIVENESS CONTROL for each counter is named beside it: a quantity
// that must VARY with the thing being counted, so the instrument cannot report
// a constant and be believed (Wave 10C's DialogController gate counted bodies
// that were registered unconditionally, and only satisfying it exposed that).
uint64_t g_newEntries      = 0;
uint64_t g_newBytes        = 0;   // control: the size retail asks for (0xc0)
uint64_t g_getTipEntries   = 0;
uint64_t g_getTipNonNull   = 0;   // control: took the "element has a string" arm
uint64_t g_setTextEntries  = 0;
uint64_t g_setTextRootHit  = 0;   // control: the ROOT itself was named textToolTip
uint64_t g_setTextFound    = 0;   // control: a text element was found at all
uint64_t g_setTextWrote    = 0;   // control: reached the String::operator= tail
uint64_t g_typeNameEntries = 0;
uint64_t g_typeEntries     = 0;

struct TipDlgSummary {
    ~TipDlgSummary() {
        const unsigned long long total =
            g_newEntries + g_getTipEntries + g_setTextEntries +
            g_typeNameEntries + g_typeEntries;
        if (total == 0) {
            printf("[RtPort] UI::ToolTipDialog ATTENDANCE: none of the five "
                   "bodies was entered in this run.\n");
            fflush(stdout);
            return;
        }
        printf("[RtPort] UI::ToolTipDialog ATTENDANCE (entries, not "
               "comparisons): total=%llu operatorNew=%llu (bytes=%llu) "
               "GetToolTipString=%llu (nonNull=%llu) SetText=%llu (rootHit=%llu "
               "found=%llu wrote=%llu) TypeName=%llu Type=%llu\n",
               total,
               (unsigned long long)g_newEntries,      (unsigned long long)g_newBytes,
               (unsigned long long)g_getTipEntries,   (unsigned long long)g_getTipNonNull,
               (unsigned long long)g_setTextEntries,  (unsigned long long)g_setTextRootHit,
               (unsigned long long)g_setTextFound,    (unsigned long long)g_setTextWrote,
               (unsigned long long)g_typeNameEntries, (unsigned long long)g_typeEntries);
        fflush(stdout);
    }
};
TipDlgSummary g_summary;

// ---------------------------------------------------------------------------
// Dependencies these bodies CALL rather than reimplement.
// ---------------------------------------------------------------------------

// `?MemAlloc@MemoryAllocator@System@@SAPEAXKH@Z`, 0x1800c7a50. See the
// operator-new banner: the two functions are the SAME nineteen instructions and
// differ only in whether the alignment is a parameter or the immediate 8.
// Reached by ADDRESS because ResolveAddr consults the port registry FIRST
// (reference_resolveaddr_consults_port_registry_first.md), so when the
// allocator port is armed this goes through our body, and it is the same door
// runtime/src/video/context_ctor.cpp already uses for the same equivalence.
void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

// `?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z`
// (0x180115650) — PORTED, runtime/src/ui/element.cpp. HIDDEN RETURN SLOT in
// RDX: (this, ret, name), returns that same pointer in RAX. Reached by NAME.
using FindElementFn = void* (*)(void* self, void** ret, const char* const* name);

// `?CacheText@TextElement@UI@@AEAAXXZ` (0x18011ec10) — private but EXPORTED
// (AEAA means PRIVATE, not unexported; this DLL exports its private members —
// reference_aeaa_means_private_not_unexported.md). NOT reimplemented: it owns
// the cached-formatted-text pointer at +0xF8 and its refcount, which is why
// runtime/src/ui/widgets_layout.cpp calls through to it too.
void CacheText(void* self) {
    using Fn = void (*)(void*);
    RT_EXPORT_ONCE(Fn, "?CacheText@TextElement@UI@@AEAAXXZ")(self);
}

// `?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A` (0x1801fa4a0). Already
// a MINTED row (standalone/rt_data_mint_tables.inc), so RT_DATA_SLOT is the
// right door: with the DLL present it is the DLL's live node, and at cutover it
// is the minted one. `Object::BaseTypeInfo` is +0x00 typeId (uint32), +0x04 an
// unused word, +0x08 `const char* name` — spelled as constants here for the same
// reason dialog_controller_base.cpp spells them: that header cannot be pulled
// into this TU without dragging the object-model dependency chain with it.
constexpr int kBaseTypeInfoTypeId = 0x00;
constexpr int kBaseTypeInfoName   = 0x08;

inline const uint8_t* ToolTipDialogTypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A");
}

// UI::DialogInstance::_root, +0x50. Confirmed by `?GetRoot@` (0x180045580),
// which is the single line `*out = this[0x50]`
// (runtime/types/ui/DialogInstance.h:233). Spelled as a constant rather than
// including DialogInstance.h, which this TU needs nothing else from.
constexpr int kDialogInstanceRoot = 0x50;

template <typename T> inline T& Field(void* p, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}

}  // namespace

// ============================================================================
// ??2ToolTipDialog@UI@@SAPEAX_K@Z                          retail 0x18004cfd0
// ============================================================================
// The class `operator new`. Nineteen instructions:
//
//     PUSH RDI / SUB RSP,0x30 / MOV [RSP+0x20],-2      MSVC unwind marker
//     RDI = [0x1801fc718] + 0x640                      the allocator's lock
//     CALL [0x18014b390]                               EnterCriticalSection
//     MOV R8D,0x8 / RDX=size / RCX=[0x1801fc718]
//     CALL 0x1800c6e60                                 Allocate(alloc,size,8)
//     CALL [0x18014b388]                               LeaveCriticalSection
//     return RAX
//
// **ALIGNMENT IS THE WHOLE PARAMETER, AND IT WAS CHECKED BY BODY.** This is
// instruction-for-instruction `?MemAlloc@MemoryAllocator@System@@` (0x1800c7a50)
// with the alignment argument frozen to the immediate `MOV R8D,0x8`; both were
// disassembled side by side for this lane. Reading one and assuming the other
// matches by resemblance is exactly how this family has been got wrong before
// (runtime/src/video/context_ctor.cpp records the same check, and found a
// sibling using align 4).
//
// The lock is NOT reproduced here for the same reason context_ctor.cpp does not
// reproduce it: it is INSIDE MemAlloc, which is the door being called.
// ============================================================================
extern "C" void* RtUi_ToolTipDialog_OperatorNew(uint64_t bytes) {
    ++g_newEntries;
    g_newBytes = bytes;                       // retail's only caller passes 0xc0
    return MemAlloc(bytes, 8);                // MOV R8D,0x8
}

// ============================================================================
// ?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ    retail 0x1800356a0
// ============================================================================
// Returns the element's tooltip-text override by value, so the hidden return
// slot is RDX and the function returns that same pointer in RAX.
//
//     MOV [RDX],RAX          with RAX=0     ret->data = nullptr
//     MOV RAX,[RCX+0x58]                    self->_toolTipString
//     TEST RAX,RAX / JZ out
//     MOV [RDX],RAX                         ret->data = it
//     INC dword ptr [RAX + -0x4]            ... and ADD A REFERENCE
//     MOV RAX,RDX / RET
//
// That is `RtString_AdoptInline` exactly — the AddRef half of the copy ctor
// (0x180006cb0) — and `data[-4]` is the REFCOUNT, `data[-8]` the length
// (runtime/types/system/String.h). Element +0x58 is `_toolTipString`, pinned by
// `?SetToolTipString@` and asserted in runtime/types/ui/Element.h:251.
//
// The `MOV dword ptr [RSP],0/1` pair at 0x1800356b4 / 0x1800356c9 is MSVC's
// SEH state variable for the unwind funclet, not a program value; it is not
// reproduced, and nothing reads it.
//
// CALLER'S OBLIGATION, and it is retail's own: this hands back a reference the
// caller must release. `BeginToolTip` calls it TWICE and releases both
// (userinterface_hover.cpp:424 / :436).
// ============================================================================
extern "C" RtSystem::String* RtUi_Element_GetToolTipString(const Element* self,
                                                          RtSystem::String* ret) {
    ++g_getTipEntries;
    ret->data = nullptr;                                       // 0x1800356b7
    char* const s = static_cast<char*>(self->_toolTipString);  // 0x1800356ba, +0x58
    if (s != nullptr) {                                        // 0x1800356be
        ++g_getTipNonNull;
        ret->data = s;                                         // 0x1800356c3
        ++RtSystem::StringRefCount(s);                         // 0x1800356c6
    }
    return ret;                                                // 0x1800356d0
}

// ============================================================================
// ?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z      retail 0x1800dab00
// ============================================================================
// Finds the dialog's `textToolTip` element and writes `text` into parameter 0.
// It is `Element::FindElement` with its FIRST LEVEL INLINED, followed by the
// tail of `?SetParameter@TextElement@UI@@` with the index frozen to 0.
//
// STEP 1 (0x1800dab22..0x1800dab37). A local `System::String` is built from the
// literal at 0x180157868, which reads `textToolTip\0` (verified from the DLL
// image, and the vtable this file's banner tabulates begins in the very next
// qword at 0x180157880). `System::String::Set` (0x1800c95d0) is PORTED, so this
// goes through RtSystem_String_Set.
//
// STEP 2 (0x1800dab3d). `MOV RBX,[RBX+0x50]` — the DialogInstance root element,
// read UNCONDITIONALLY and UNGUARDED. **Left unguarded deliberately.** This port
// is mature enough that a faithful crash here means something upstream handed us
// a ToolTipDialog with a null root, and a guard would skip the behaviour rather
// than restore it (CLAUDE.md, "Don't 'fix' a faithful crash with a null check").
//
// STEP 3 (0x1800dab41..0x1800dab7b) — the inlined self-match on the ROOT. It is
// `FindElement`'s own head test, transcribed:
//   * an EMPTY query never matches: `TEST RDX,RDX / JZ` on the data and
//     `TEST EAX,EAX / JZ` on the length both jump straight to the child scan;
//   * lengths are compared FIRST (`CMP R8D,EAX / JNZ`), and only then, when
//     this element's name is non-null and non-empty, does `StrCompare` run;
//   * a NULL name of matching length (i.e. length 0) counts as a MATCH
//     (`TEST RCX,RCX / JZ found`) — unreachable here only because the length
//     test above has already excluded a zero-length query.
// Reproduced as retail spells it, not as it "meant to".
//
// STEP 4 (0x1800dab7d..0x1800dabb9) — the child scan, which calls the REAL
// `Element::FindElement` (0x180115650, ported) on each non-null child of the
// root and stops at the first hit. Note the loop bound `[RBX+0xa8]` is read into
// EBP ONCE, before the loop, and `JLE` makes it a SIGNED test: a negative child
// count scans nothing.
//
// STEP 5 (0x1800dabbd..0x1800dabee) — the local string is released with retail's
// inlined idiom, INCLUDING its asymmetry: on the freeing branch the object is
// left holding `data - 8`, a dangling pointer, and only the sharing branch nulls
// it. runtime/types/system/String.h says in so many words "Reproduce it; do not
// tidy it", and RtString_ReleaseInline is that transcription.
//
// STEP 6 (0x1800dabee..0x1800dac28) — the `SetParameter(0, text)` tail:
//   * guard `CMP dword ptr [RBX+0xd0],0 / JLE` — SIGNED, on the parameter
//     Array<String>'s SIZE (runtime/types/ui/TextElement.h);
//   * the dirty walk, a DO-WHILE that ORs 0x8000000 into `_flags` (+0x34) on the
//     found element and then on every ancestor through `_parent` (+0x98);
//   * `System::String::operator=(paramArray[0], text)` — the array's DATA
//     pointer is at +0xc8 and retail passes it with no index arithmetic at all,
//     which is index 0;
//   * `CacheText(found)`.
//
// NO ORACLE, and the reason is the one widgets_layout.cpp already wrote out for
// `SetParameter`: running retail afterwards is VACUOUS (retail's own equality
// check inside `String::operator=` sees the slot already holding `text` and
// takes its own no-op path), and restoring the slot first to make it non-vacuous
// is a use-after-free risk on a shared, refcounted String. The instrument here
// is the attendance counter, which reports what actually ran.
// ============================================================================
extern "C" void RtUi_ToolTipDialog_SetText(void* self, const RtSystem::String* text) {
    ++g_setTextEntries;

    // ---- STEP 1 -------------------------------------------------------------
    RtSystem::String name = { nullptr };                       // 0x1800dab22
    RtSystem::RtSystem_String_Set(&name, "textToolTip");       // 0x1800dab37

    // ---- STEP 2: unguarded, deliberately ------------------------------------
    void* found = Field<void*>(self, kDialogInstanceRoot);     // 0x1800dab3d
    Element* const root = reinterpret_cast<Element*>(found);

    // ---- STEP 3: the inlined self-match on the root --------------------------
    bool matched = false;
    if (name.data != nullptr &&                                // 0x1800dab46
        RtSystem::StringLength(name.data) != 0) {              // 0x1800dab4e
        const int32_t want = RtSystem::StringLength(name.data);
        char* const mine = static_cast<char*>(root->_name);    // 0x1800dab52, +0x10
        const int32_t lm = mine ? RtSystem::StringLength(mine) : 0;  // 0x1800dab5b
        if (lm == want) {                                      // 0x1800dab64
            if (mine == nullptr ||                             // 0x1800dab6c
                RtSystem::StringLength(mine) == 0) {           // 0x1800dab6e
                matched = true;
            } else {
                matched = RtSystem::RtString_StrCompare(mine, name.data) == 0;  // 0x1800dab74
            }
        }
    }
    if (matched) ++g_setTextRootHit;

    // ---- STEP 4: the child scan ---------------------------------------------
    if (!matched) {
        found = nullptr;
        const int32_t count = root->_children._size;           // 0x1800dab7f, +0xa8
        for (int32_t i = 0; i < count; ++i) {                  // signed (JLE/JL)
            Element* const child = root->_children._data[i];   // 0x1800dab90, +0xa0
            if (child == nullptr) continue;                    // 0x1800dab93
            void* hit = nullptr;
            // Spelled as ONE literal on ONE line on purpose: a SPLIT string
            // literal is invisible to a naive grep for the mangled name, which
            // is precisely how `?BeginToolTip@` hid from one search last wave
            // (userinterface_hover.cpp:23 records it).
            RT_EXPORT_ONCE(FindElementFn, "?FindElement@Element@UI@@QEAA?AV?$InternalPtr@VElement@UI@@@Object@@AEBVString@System@@@Z")(child, &hit, &name.data);  // 0x1800daba2
            if (hit != nullptr) { found = hit; break; }        // 0x1800dabaf
        }
    }
    if (found != nullptr) ++g_setTextFound;

    // ---- STEP 5: release the local, asymmetry and all ------------------------
    RtSystem::RtString_ReleaseInline(&name);                   // 0x1800dabc7

    // ---- STEP 6: the SetParameter(0, text) tail ------------------------------
    if (found != nullptr &&                                    // 0x1800dabee
        *reinterpret_cast<const int32_t*>(
            static_cast<const uint8_t*>(found) + TE::kParamCount) > 0) {  // 0x1800dabf3
        ++g_setTextWrote;
        // The dirty walk: the found element and every ancestor.
        for (void* node = found; node != nullptr;              // 0x1800dac00
             node = Field<void*>(node, TE::kParent))           // +0x98
            Field<uint32_t>(node, TE::kDirtyFlags) |= TE::kDirtyBit;   // +0x34, 0x8000000
        void* const params = Field<void*>(found, TE::kParamArrayData); // 0x1800dac19, +0xc8
        RtSystem::RtSystem_String_Assign(reinterpret_cast<RtSystem::String*>(params),
                                         text);                // 0x1800dac20
        CacheText(found);                                      // 0x1800dac28
    }
}

// ============================================================================
// ?TypeName@ToolTipDialog@UI@@UEBAPEBDXZ    retail 0x18004cfc0   vtable slot 1
//     MOV RAX,qword ptr [0x1801fa4a8] / RET
// ============================================================================
// 0x1801fa4a8 is `?_typeInfo@ToolTipDialog@UI@@0VBaseTypeInfo@Object@@A`
// (0x1801fa4a0) plus 0x08, i.e. the node's `name` field. Two instructions, and
// the address is a LOAD OF THE FIELD, not of the node — spelled that way here.
// ============================================================================
extern "C" const char* RtUi_ToolTipDialog_TypeName(const void* /*self*/) {
    ++g_typeNameEntries;
    return *reinterpret_cast<const char* const*>(
        ToolTipDialogTypeInfo() + kBaseTypeInfoName);
}

// ============================================================================
// ?Type@ToolTipDialog@UI@@UEBAIXZ           retail 0x18004cfa0   vtable slot 2
//     MOV EAX,dword ptr [0x1801fa4a0] / RET
// ============================================================================
// A DWORD load — `MOV EAX`, not `MOV RAX` — of the node's `typeId` at +0x00.
// ============================================================================
extern "C" uint32_t RtUi_ToolTipDialog_Type(const void* /*self*/) {
    ++g_typeEntries;
    return *reinterpret_cast<const uint32_t*>(
        ToolTipDialogTypeInfo() + kBaseTypeInfoTypeId);
}

// ============================================================================
// Registration. Both doors for every row — BY NAME and BY ADDRESS.
// ============================================================================
// The address door is not redundant with the name door: a caller that reaches a
// body by address silently keeps using the DLL when only the name is registered
// (reference_bypassed_port_needs_a_source_checker.md), and a minted vtable's
// slots are matched by ADDRESS. Both are spelled as literals so
// tools/runtime_census.py can see them
// (reference_census_cannot_see_nonliteral_registrations.md).
// ============================================================================
int RtUiToolTipDialogRegister() {
    int n = 0;

    RegisterRuntimeExport("??2ToolTipDialog@UI@@SAPEAX_K@Z",
                          (void*)&RtUi_ToolTipDialog_OperatorNew);
    RegisterRuntimeAddr(0x18004cfd0, (void*)&RtUi_ToolTipDialog_OperatorNew);   ++n;

    RegisterRuntimeExport("?GetToolTipString@Element@UI@@QEBA?AVString@System@@XZ",
                          (void*)&RtUi_Element_GetToolTipString);
    RegisterRuntimeAddr(0x1800356a0, (void*)&RtUi_Element_GetToolTipString);    ++n;

    RegisterRuntimeExport("?SetText@ToolTipDialog@UI@@QEAAXAEBVString@System@@@Z",
                          (void*)&RtUi_ToolTipDialog_SetText);
    RegisterRuntimeAddr(0x1800dab00, (void*)&RtUi_ToolTipDialog_SetText);       ++n;

    RegisterRuntimeExport("?TypeName@ToolTipDialog@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ToolTipDialog_TypeName);
    RegisterRuntimeAddr(0x18004cfc0, (void*)&RtUi_ToolTipDialog_TypeName);      ++n;

    RegisterRuntimeExport("?Type@ToolTipDialog@UI@@UEBAIXZ",
                          (void*)&RtUi_ToolTipDialog_Type);
    RegisterRuntimeAddr(0x18004cfa0, (void*)&RtUi_ToolTipDialog_Type);          ++n;

    return n;
}
