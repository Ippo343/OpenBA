// ============================================================================
// standalone/dlgctrl_route.cpp — Wave 10C lane ROUTES
//
// A ROUTE to the nine ported `??_7DialogController@UI@@6B@` bodies, and the
// two-counter instrument that says whether it worked.
//
// OFF BY DEFAULT. Prints nothing unless BANISHED_DLGCTRL_ROUTE is set.
//     BANISHED_DLGCTRL_ROUTE=probe   report only — patch nothing
//     BANISHED_DLGCTRL_ROUTE=1       report AND install the route
//
// ----------------------------------------------------------------------------
// WHY THE RECORDED BLOCKER IS THE WRONG MECHANISM
// ----------------------------------------------------------------------------
// Wave 10B lane DLGV left `??_7DialogController@UI@@6B@` READY/TRAP=0 but NOT
// promoted, because its nine-way ATTENDANCE counter printed
//
//     [DlgCtrl] ATTENDANCE: none of the nine bodies was entered
//
// in both halves of both A/B pairs. Its recorded diagnosis (WAVE10B.md, and
// reg_mint_promoted.cpp's "NOT PROMOTED, and why" block) is:
//
//     "Every UI::DialogController subclass in code/Game/Dialog/ carries its own
//      exe-side vtable; nothing this lane could drive constructs an object
//      holding the Runtime BASE table."
//
// The first clause is TRUE. The conclusion does not follow from it, and the
// counter-example is written down three files away in this same tree —
// code/Game/Dialog/destroyui.cpp:547, describing RETAIL's exe-side vtable:
//
//     "Retail vtable @ 0x14015bb00 (11 slots): overrides 0,1,2,4,7,9; slots
//      3,5,6,8,10 are inherited UI::DialogController base thunks in the exe."
//
// That is how C++ vtables work: a derived table does not omit the virtuals it
// does not override, it COPIES THE BASE ENTRY. So in retail an object carrying
// DestroyUI's own exe-side table still dispatches
// `?OnOpened@DialogController@UI@@` and `?OnClosed@DialogController@UI@@` —
// the very bodies lane DLGV ported — out of slots 5 and 6 of that table.
// Holding the base table is NOT the precondition for entering a base body;
// inheriting the slot is, and every one of these subclasses inherits most of
// them.
//
// So why zero? Because OUR standalone subclass vtables do not reproduce the
// inheritance. All 27 of them are built by the same idiom, e.g.
// code/Game/Dialog/destroyui.cpp:
//
//     for (int i = 0; i < kSlots; ++i) g_vt[i] = (void*)&StubNop;
//     g_vt[0] = &Dtor; g_vt[2] = &GetTypeId; g_vt[4] = ...; g_vt[7] = ...;
//
// — every slot the subclass does not override is a FILE-LOCAL no-op, not the
// inherited base thunk. The nine bodies are therefore unreachable for a reason
// that is ours and is fixable, not a property of the DLL. That is a different
// finding from DLGV's, and it makes the row's blocker a routing gap rather than
// a wall. (Three recorded blockers in this project have gone stale in a single
// session; this is a fourth. Re-derive, do not inherit.)
//
// The two are behaviourally indistinguishable, which is exactly why nobody
// noticed: `StubNop` does nothing and the seven inherited base bodies are
// literally `C2 00 00` — `RET 0` — verified byte-wise and one disassembly at a
// time by lane DLGV (runtime/src/ui/dialog_controller_base.cpp). Substituting
// one no-op for another changes no observable behaviour. It changes only
// WHOSE no-op runs, and therefore whether the port is exercised at all.
//
// ----------------------------------------------------------------------------
// WHAT THIS ROUTE PATCHES, AND WHAT IT DELIBERATELY DOES NOT
// ----------------------------------------------------------------------------
// Only slots 5, 6 and 10 — `OnOpened` (+0x28), `OnClosed` (+0x30) and
// `OnSerializeSettings` (+0x50) — and only where the slot still holds the
// file's own stub. Those three are chosen because they are the three the ported
// `DialogInstance` bodies actually dispatch on each controller:
//
//     runtime/src/ui/dialog_settings.cpp:373   VSlot(c, kCtrlVtblOnOpened)   0x28
//     runtime/src/ui/dialog_settings.cpp:262   VSlot(c, kCtrlVtblOnClosed)   0x30
//     runtime/src/ui/dialog_settings.cpp:238   VSlot(c, kCtrlVtblOnSerializeSettings) 0x50
//
// and because all three base bodies are `RET 0`, so the substitution provably
// cannot change behaviour.
//
// Slot 1 (`TypeName`) is NOT patched even though most of these files leave it a
// stub, and the reason is worth stating rather than leaving as an omission:
// retail's DestroyUI table OVERRIDES slot 1 (see the quote above). A slot our
// file stubs is not necessarily a slot retail inherits, and this route has the
// retail override map for exactly one of the 27 subclasses. Patching on "our
// slot is a stub" alone would be inventing an inheritance we have not read.
// Slots 5/6/10 are safe under that same doubt only because the base body there
// is a no-op either way — the patch is behaviour-preserving whether retail
// inherits the slot or not.
//
// Slot 8 (`OnUpdate`) is NOT patched either. Retail leaves it inherited for
// DestroyUI, but OUR destroyui/residenceui/workplaceui deliberately drive their
// per-frame refresh from it (a documented standalone pattern). Overwriting it
// would break live panels to feed a counter — the exact inversion this lane
// exists to prevent.
//
// ----------------------------------------------------------------------------
// TWO COUNTERS, SCORED SEPARATELY
// ----------------------------------------------------------------------------
// The route installs THUNKS, not the ported bodies directly, so there are two
// independent readings rather than one:
//
//   [DlgCtrlRoute] dispatches=  how many times a patched slot was CALLED. This
//                               is a property of the game, not of the port: it
//                               answers "is any DialogInstance's _controllers
//                               list non-empty when OnOpened/OnClosed/
//                               SaveSettings runs?"
//   [DlgCtrl] ATTENDANCE        how many times a PORTED BODY was entered
//                               (runtime/src/ui/dialog_controller_base.cpp).
//
// Reading them together is the point. One number cannot distinguish the three
// outcomes, and they invert the conclusion:
//
//   dispatches=0, attendance=0   nothing ever dispatches these slots — the
//                                controller lists are empty and the route is
//                                not the missing piece. NO ROUTE.
//   dispatches>0, attendance=0   the slots are dispatched but resolution
//                                handed back something other than our port
//                                (a broken route, not a missing one).
//   dispatches>0, attendance>0   ROUTE FOUND; the promotion's gate (b) is
//                                informative for the first time.
//
// A single union counter would read the first and second cases identically
// (reference_union_control_hides_a_blind_screen).
//
// ----------------------------------------------------------------------------
// HOW THE STUB IS IDENTIFIED — a reading, not an assumption
// ----------------------------------------------------------------------------
// Each of the 27 files has its OWN `StubNop`, at its own address, so there is no
// single value to compare against. The stub is found as the MODE (most frequent
// pointer) of slots 0..23 of the freshly-built table. That is sound because the
// idiom fills all 48 slots with the stub and then overwrites at most ~9 of them,
// so the stub is the strict majority by a wide margin. The route REPORTS the
// mode's multiplicity for every type, so a table that does not follow the idiom
// (a low count) is visible rather than silently mispatched, and it refuses to
// patch any table whose mode does not hold a clear majority.
// ============================================================================

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "Object/factory_registration.h"
#include "options.h"

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module module, uintptr_t ghidraAddr);

namespace {

// ---------------------------------------------------------------------------
// The three ported base bodies this route wires in. Addresses are the DLL's;
// ResolveAddr(Module::Runtime, …) consults the PORT REGISTRY FIRST, so for a
// registered address it hands back OUR body rather than the DLL's
// (reference_resolveaddr_consults_port_registry_first). All three are
// registered unconditionally by RtUiDialogControllerBase_Register() in
// runtime/src/ui/dialog_controller_base.cpp, so this is a name-free but not an
// address DOOR: the address is the identity of a body this tree already owns.
// ---------------------------------------------------------------------------
struct SlotSpec {
    int         index;      // vtable slot index
    int         byteOff;    // for the log, so it can be matched to kCtrlVtbl*
    uintptr_t   dllAddr;
    const char* name;
};
constexpr SlotSpec kSpecs[] = {
    {  5, 0x28, 0x1800cd2a0ull, "OnOpened"            },
    {  6, 0x30, 0x1800cd290ull, "OnClosed"            },
    { 10, 0x50, 0x1800cd250ull, "OnSerializeSettings" },
};
constexpr int kSpecCount = 3;

// Resolved targets, filled at arm time.
void* g_target[kSpecCount] = { nullptr, nullptr, nullptr };

// ---- counter screen 1: DISPATCHES (a property of the game) -----------------
unsigned long long g_dispatch[kSpecCount] = { 0, 0, 0 };
int  g_patchedTables  = 0;
int  g_patchedSlots   = 0;
int  g_skippedTables  = 0;
bool g_armed          = false;
bool g_patchMode      = false;

struct RouteReport {
    ~RouteReport() {
        if (!g_armed) return;   // silent when unset — the standing rule
        const unsigned long long total =
            g_dispatch[0] + g_dispatch[1] + g_dispatch[2];
        printf("[DlgCtrlRoute] %s: tables patched=%d (skipped=%d) "
                    "slots patched=%d\n",
                    g_patchMode ? "PATCH" : "PROBE (no patch installed)",
                    g_patchedTables, g_skippedTables, g_patchedSlots);
        if (!g_patchMode) return;
        if (total == 0) {
            printf("[DlgCtrlRoute] dispatches=0 -- NOT ONE patched slot was "
                        "called. The controller lists are empty on every route "
                        "this run drove; the stub substitution is not what was "
                        "keeping the nine bodies unentered.\n");
            return;
        }
        printf("[DlgCtrlRoute] dispatches=%llu OnOpened=%llu OnClosed=%llu "
                    "OnSerializeSettings=%llu\n",
                    total, g_dispatch[0], g_dispatch[1], g_dispatch[2]);
    }
};
RouteReport g_report;

// ---- the thunks ------------------------------------------------------------
// Each counts, then tail-calls the resolved (ported) base body. Arity matters
// only for the third; the first two take `self` alone.
using VoidVFn   = void (*)(void*);
using StreamVFn = void (*)(void*, void*);

void Thunk_OnOpened(void* self) {
    ++g_dispatch[0];
    if (g_target[0]) reinterpret_cast<VoidVFn>(g_target[0])(self);
}
void Thunk_OnClosed(void* self) {
    ++g_dispatch[1];
    if (g_target[1]) reinterpret_cast<VoidVFn>(g_target[1])(self);
}
void Thunk_OnSerializeSettings(void* self, void* stream) {
    ++g_dispatch[2];
    if (g_target[2]) reinterpret_cast<StreamVFn>(g_target[2])(self, stream);
}
void* const kThunk[kSpecCount] = {
    reinterpret_cast<void*>(&Thunk_OnOpened),
    reinterpret_cast<void*>(&Thunk_OnClosed),
    reinterpret_cast<void*>(&Thunk_OnSerializeSettings),
};

// ---------------------------------------------------------------------------
// The visitor. One object per DialogController subclass is built and LEAKED —
// the same cost and the same precedent as Object::DumpSlot3Inventory, which
// walks every Component type this way. We need a real instance because the
// vtable array itself is a file-local static with no external symbol; the only
// handle on it is an object's vptr.
// ---------------------------------------------------------------------------
constexpr int kScanSlots = 24;   // idiom fills 48; 24 is ample for a majority

void Visit(const char* typeName, const char* parentName,
           ::Object::FactoryFn factory, void* /*ctx*/) {
    if (!typeName || !parentName || !factory) return;
    if (std::strcmp(parentName, "UI::DialogController") != 0) return;

    void* obj = factory();
    if (!obj) {
        printf("[DlgCtrlRoute] %-24s factory returned null -- skipped\n",
                    typeName);
        ++g_skippedTables;
        return;
    }
    void** vt = *reinterpret_cast<void***>(obj);
    if (!vt) {
        printf("[DlgCtrlRoute] %-24s null vptr -- skipped\n", typeName);
        ++g_skippedTables;
        return;
    }

    // Mode of slots 0..kScanSlots-1 = this file's own StubNop.
    void* stub  = nullptr;
    int   best  = 0;
    for (int i = 0; i < kScanSlots; ++i) {
        int c = 0;
        for (int j = 0; j < kScanSlots; ++j) if (vt[j] == vt[i]) ++c;
        if (c > best) { best = c; stub = vt[i]; }
    }
    // Refuse a table that does not follow the fill-then-override idiom: without
    // a clear majority the "mode" is not evidence of anything, and patching on
    // it would be inventing an inheritance. Reported, not silently skipped.
    if (best * 2 <= kScanSlots) {
        printf("[DlgCtrlRoute] %-24s stub mode=%p count=%d/%d -- NO clear "
                    "majority, table does not follow the fill idiom; skipped\n",
                    typeName, stub, best, kScanSlots);
        ++g_skippedTables;
        return;
    }

    int hits = 0;
    char detail[192];
    int  dn = 0;
    detail[0] = '\0';
    for (int s = 0; s < kSpecCount; ++s) {
        const bool isStub = (vt[kSpecs[s].index] == stub);
        dn += std::snprintf(detail + dn, sizeof(detail) - (size_t)dn, "%s%s=%s",
                            dn ? " " : "", kSpecs[s].name,
                            isStub ? "stub" : "OVERRIDDEN");
        if (!isStub) continue;
        ++hits;
        if (g_patchMode) vt[kSpecs[s].index] = kThunk[s];
    }
    printf("[DlgCtrlRoute] %-24s stub=%p (%d/%d slots) %s -> %s %d slot(s)\n",
                typeName, stub, best, kScanSlots, detail,
                g_patchMode ? "patched" : "would patch", hits);
    if (hits) { ++g_patchedTables; g_patchedSlots += hits; }
}

}  // namespace

// ---------------------------------------------------------------------------
// Entry point. Idempotent; safe to call every frame. Must run after
// Object::Factory::Startup (factories cache type ids) and before any dialog is
// opened — the menu Update tick satisfies both, which is the same reason
// DumpSlot3Inventory is called from there.
// ---------------------------------------------------------------------------
void DlgCtrlRoute_Arm() {
    static bool s_done = false;
    if (s_done) return;
    s_done = true;

    const char* e = Opt().dlgCtrlRoute;
    if (!e || !*e || std::strcmp(e, "0") == 0) return;   // silent when unset
    g_armed = true;
    g_patchMode = (std::strcmp(e, "probe") != 0);

    for (int s = 0; s < kSpecCount; ++s) {
        g_target[s] = reinterpret_cast<void*>(
            ResolveAddr(Module::Runtime, kSpecs[s].dllAddr));
        const uintptr_t raw = ResolveAddr(Module::Runtime, kSpecs[s].dllAddr);
        printf("[DlgCtrlRoute] target %-20s dll=0x%llx -> %p%s\n",
                    kSpecs[s].name,
                    (unsigned long long)kSpecs[s].dllAddr, g_target[s],
                    raw ? "" : "  <== NULL, route cannot work");
    }

    ::Object::ForEachRegistration(&Visit, nullptr);
    fflush(stdout);
}
