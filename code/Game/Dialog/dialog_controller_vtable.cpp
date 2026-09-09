// code/Game/Dialog/dialog_controller_vtable.cpp — Wave 10D lane VTFID
//
// See dialog_controller_vtable.h for why this exists and what it deliberately
// does not touch.

#include "dialog_controller_vtable.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "resolve.h"   // ResolveAddr / ResolveExport

namespace {

// ---------------------------------------------------------------------------
// The eight inherited base virtuals, by vtable slot.
//
// Slots 0/1/2 are absent on purpose: retail overrides them in every subclass,
// so no derived table ever inherits them and there is nothing to copy.
//
// Slots 4..10 are UI::DialogController's own, addressed by their DLL addresses.
// ResolveAddr(Module::Runtime, …) consults the PORT REGISTRY FIRST
// (reference_resolveaddr_consults_port_registry_first), and all seven are
// registered unconditionally by RtUiDialogControllerBase_Register() in
// runtime/src/ui/dialog_controller_base.cpp — so this resolves to OUR ported
// body, which is the whole point. The addresses are the identity of bodies this
// tree already owns, not a door into unported DLL internals.
//
// Slot 3 is different, and getting it wrong would be easy: the thunk retail's
// derived tables hold there does NOT name a DialogController virtual, it names
// ?Persist@Internal@Object@@ — Object::Internal's, inherited through
// DialogController rather than defined by it. That one is resolved BY NAME; it
// is a plain exported symbol and runtime/src/object/internal.cpp registers a
// port for it with RegisterRuntimeExport.
// ---------------------------------------------------------------------------
struct BaseSlot {
    int         slot;
    uintptr_t   dllAddr;      // 0 => resolve by name instead
    const char* name;         // for the log, and the resolver for slot 3
};

const BaseSlot kBaseSlots[] = {
    {  3, 0,             "?Persist@Internal@Object@@UEAAXAEAVStream@IO@@@Z" },
    {  4, 0x1800cd2b0ull, "?OnCreated@DialogController@UI@@"                },
    {  5, 0x1800cd2a0ull, "?OnOpened@DialogController@UI@@"                 },
    {  6, 0x1800cd290ull, "?OnClosed@DialogController@UI@@"                 },
    {  7, 0x1800cd280ull, "?OnUpdateLayout@DialogController@UI@@"           },
    {  8, 0x1800cd270ull, "?OnUpdate@DialogController@UI@@"                 },
    {  9, 0x1800cd260ull, "?OnElementEvent@DialogController@UI@@"           },
    { 10, 0x1800cd250ull, "?OnSerializeSettings@DialogController@UI@@"      },
};
constexpr int kBaseSlotCount = sizeof(kBaseSlots) / sizeof(kBaseSlots[0]);

// ---------------------------------------------------------------------------
// Per class, the mask of slots RETAIL'S OWN derived vtable inherits.
//
// GENERATED — do not hand-edit. Regenerate with
//     python tools/dlgctrl_vtable_audit.py --gen
// which reads Application-x64.exe: it locates each class's table by that
// class's own slot-1 TypeName stub (which returns BaseTypeInfo::name, so the
// table names itself) and marks a slot inherited when it holds an `FF 25`
// import thunk. Bit i of the mask = slot i.
//
// Spot check against the one table this tree had already written down by hand:
// destroyui.cpp:547 records "slots 3,5,6,8,10 are inherited base thunks" for
// DestroyUI — and DestroyUI's mask here is 0x568 = bits 3,5,6,8,10.
// ---------------------------------------------------------------------------
struct ClassMask { const char* cls; unsigned mask; };

const ClassMask kMasks[] = {
    { "AttendanceUI", 0x768 },        // retail vtable 0x14015b070
    { "BuildRequirementUI", 0x7e0 },  // retail vtable 0x14015b1d0
    { "BuildUI", 0x568 },             // retail vtable 0x14015f210
    { "CemeteryUI", 0x768 },          // retail vtable 0x14015b240
    { "CitizenUI", 0x528 },           // retail vtable 0x14015ba00
    { "ConsumeProduceUI", 0x568 },    // retail vtable 0x14015b2e0
    { "ControlUI", 0x568 },           // retail vtable 0x14015b380
    { "CropUI", 0x468 },              // retail vtable 0x14015f270
    { "DebugUI", 0x428 },             // retail vtable 0x14015b870
    { "DestroyUI", 0x568 },           // retail vtable 0x14015bb00
    { "EntityUI", 0x420 },            // retail vtable 0x14015bb90
    { "EventLogUI", 0x518 },          // retail vtable 0x14015bc40
    { "HelpUI", 0x548 },              // retail vtable 0x14015bf30
    { "HotKeyToolTipUI", 0x7e0 },     // retail vtable 0x14015bfc0
    { "ImmigrationUI", 0x568 },       // retail vtable 0x14015c080
    { "MapUI", 0x5e0 },               // retail vtable 0x14015c350
    { "ModBrowseUI", 0x7f0 },         // retail vtable 0x14015c500
    { "NoiseUI", 0x5e0 },             // retail vtable 0x14015ca80
    { "OrchardUI", 0x468 },           // retail vtable 0x14015cf30
    { "PageToggleUI", 0x560 },        // retail vtable 0x14015cfb0
    { "PastureUI", 0x468 },           // retail vtable 0x14015f610
    { "ProductionUI", 0x760 },        // retail vtable 0x14015d070
    { "ProfessionUI", 0x4d8 },        // retail vtable 0x14015d0f0
    { "RadiusUI", 0x798 },            // retail vtable 0x14015d160
    { "ResidenceUI", 0x560 },         // retail vtable 0x14015f6d0
    { "ResourceLimitUI", 0x568 },     // retail vtable 0x14015d1f0
    { "SelectResourceUI", 0x468 },    // retail vtable 0x14015d330
    { "SpriteCaptureUI", 0x4e0 },     // retail vtable 0x14015d4f0
    { "StatusBarUI", 0x518 },         // retail vtable 0x14015d810
    { "StatusIconUI", 0x768 },        // retail vtable 0x14015d8a0
    { "StorageCapacityUI", 0x760 },   // retail vtable 0x14015d950
    { "StorageUI", 0x760 },           // retail vtable 0x14015f970
    { "TitleDescToolTipUI", 0x7e0 },  // retail vtable 0x14015d9f0
    { "ToolUI", 0x4e0 },              // retail vtable 0x14015da70
    { "TownHallUI", 0x068 },          // retail vtable 0x14015dce0
    { "TradeUI", 0x008 },             // retail vtable 0x14015de80
    { "TutorialUI", 0x560 },          // retail vtable 0x140160f30
    { "UpgradeUI", 0x568 },           // retail vtable 0x14015df00
    { "WorkPlaceUI", 0x568 },         // retail vtable 0x14015e030
};
constexpr int kMaskCount = sizeof(kMasks) / sizeof(kMasks[0]);

// ---------------------------------------------------------------------------
// The registration list. Static-init safe: plain PODs in a fixed array, with no
// constructor of their own, so no initialisation-order dependency between this
// TU and the 38 that register into it.
// ---------------------------------------------------------------------------
struct Entry { void** vt; int nslots; const char* cls; void* stub; };
constexpr int kMaxEntries = 64;
Entry g_entries[kMaxEntries];
int   g_count = 0;

UIDialogControllerVtable::Stats g_stats = { 0, 0, 0, 0, 0 };
bool g_applied = false;

unsigned MaskFor(const char* cls) {
    for (int i = 0; i < kMaskCount; ++i)
        if (std::strcmp(kMasks[i].cls, cls) == 0) return kMasks[i].mask;
    return 0xFFFFFFFFu;   // sentinel: name not in the retail table
}

}  // namespace

namespace UIDialogControllerVtable {

void Register(void** vt, int nslots, const char* cls, void* stub) {
    if (!vt || !cls || g_count >= kMaxEntries) return;
    g_entries[g_count].vt     = vt;
    g_entries[g_count].nslots = nslots;
    g_entries[g_count].cls    = cls;
    g_entries[g_count].stub   = stub;
    ++g_count;
}

int Apply() {
    if (g_applied) return g_stats.filled;
    g_applied = true;

    // DEFAULT-ON fidelity fix, with a kill switch — the project's standard
    // shape for a live feature (kDefaultOnFlags in component_factory_base.cpp).
    // It exists so the A/B runs on ONE BINARY: with a control that is a
    // different build, "the tables changed" and "the exe changed" are the same
    // observation and cannot be told apart.
    if (const char* e = std::getenv("BANISHED_DLGCTRL_VT")) {
        if (e[0] == '0' || std::strcmp(e, "false") == 0) {
            std::fprintf(stderr,
                "[DlgCtrlVt] DISABLED by BANISHED_DLGCTRL_VT=0 — %d registered "
                "tables keep their file-local stubs in every inherited slot "
                "(the pre-Wave-10D behaviour).\n", g_count);
            g_stats.tables = g_count;
            return 0;
        }
    }

    // Resolve the eight base bodies once.
    void* body[11] = { nullptr };
    for (int i = 0; i < kBaseSlotCount; ++i) {
        const BaseSlot& b = kBaseSlots[i];
        void* p = b.dllAddr
                    ? reinterpret_cast<void*>(ResolveAddr(Module::Runtime, b.dllAddr))
                    : ResolveExport(b.name);
        body[b.slot] = p;
        if (!p) {
            std::fprintf(stderr,
                "[DlgCtrlVt] WARN slot %d (%s) did not resolve — every table "
                "that inherits it keeps its file-local stub.\n", b.slot, b.name);
        }
    }

    g_stats.tables = g_count;
    const bool trace = std::getenv("BANISHED_DLGCTRL_VT_TRACE") != nullptr;

    for (int e = 0; e < g_count; ++e) {
        const Entry& en = g_entries[e];
        const unsigned mask = MaskFor(en.cls);
        if (mask == 0xFFFFFFFFu) {
            ++g_stats.unknown;
            std::fprintf(stderr,
                "[DlgCtrlVt] WARN \"%s\" registered a table but is not a known "
                "UI::DialogController subclass in the retail mask table — "
                "nothing filled.\n", en.cls);
            continue;
        }
        int filled = 0, kept = 0, unres = 0;
        for (int s = 0; s < 11 && s < en.nslots; ++s) {
            if (!((mask >> s) & 1u)) continue;      // retail overrides it
            if (!body[s]) { ++unres; continue; }
            // Only overwrite a slot that still holds THIS FILE's stub. A slot
            // the file deliberately overrode is a standalone deviation we keep.
            if (en.vt[s] != en.stub) { ++kept; continue; }
            en.vt[s] = body[s];
            ++filled;
        }
        g_stats.filled       += filled;
        g_stats.keptOverride += kept;
        g_stats.unresolved   += unres;
        if (trace) {
            std::fprintf(stderr, "[DlgCtrlVt] %-22s mask=0x%03x filled=%d "
                         "keptOverride=%d unresolved=%d\n",
                         en.cls, mask, filled, kept, unres);
        }
    }

    std::fprintf(stderr,
        "[DlgCtrlVt] tables=%d unknown=%d inheritedSlotsFilled=%d "
        "keptOverride=%d unresolved=%d\n",
        g_stats.tables, g_stats.unknown, g_stats.filled,
        g_stats.keptOverride, g_stats.unresolved);
    return g_stats.filled;
}

Stats GetStats() { return g_stats; }

}  // namespace UIDialogControllerVtable
