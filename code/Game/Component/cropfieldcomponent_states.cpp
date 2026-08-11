// code/Game/Component/cropfieldcomponent_states.cpp
// CropFieldComponent controller state machine.
//
// Structurally the Orchard's twin: a ControllerComponent (0x68) driving a drag-created
// area through plant / tend / harvest, with its own private state table. Where the
// orchard has six states (2/9/10/12/11/20) the crop field has FOUR bodies (2/9/10/11)
// plus two transition-only targets (3 and 21).
//
// ── How this table was recovered ──────────────────────────────────────────────────
// CropFieldDescription_Setup (0x14014edb0) tail-calls the populator. Ghidra decompiles
// that call as `TownHallUI_Ctor()` and names the whole helper family `TownHallUI_*`,
// which is WRONG — the disassembly is unambiguous:
//     14014edb4: CALL 0x14011b150
//     14014edb9: MOV dword ptr [0x1401d7ad8],EAX
// So the real populator is 0x14011b150, the state array is 0x1401d6b50 and the count is
// 0x1401d7ad0. (0x1401d7ad8, the thing Setup writes, is a separate "registered ok" flag
// — do not mistake it for the count.) Every handler address below was read from that
// function's DISASSEMBLY operands, not from Ghidra's symbol names.
//
// ── Register-helper argument order (0x1400330d0), verified from its own disassembly ──
// It stages the new entry in a stack buffer at RSP+0x30 and memcpy-style installs it:
//     [RSP+0x30] = ECX          -> +0x00 id
//     [RSP+0x38] = RDX          -> +0x08 enter
//     [RSP+0x40] = 0            -> +0x10 trans30   (always zero for this controller)
//     [RSP+0x48] = [RSP+0x170]  -> +0x18 trans38   (the 5th/stack argument)
//     [RSP+0x50] = R8           -> +0x20 update
// i.e. the shape is sub_330d0(id, enter, UPDATE, _, TRANS38) — update and trans38 are
// NOT in source order, and reading them the other way round would silently swap the
// per-frame handler with the update handler. Companions: 0x1400331f0 adds a transition,
// 0x1400332a0 adds an event.
//
// ── The recovered table ───────────────────────────────────────────────────────────
//   state 2   enter 0x14011a920  trans38 0x14011a860   -> 3, 9        event 3 -> 0x14011a7b0
//   state 9   enter 0x14011a310  trans38 0x14011a1e0   -> 3, 10
//   state 10  enter 0x14011a100  trans38 0x140119fd0
//                                update  0x140119f70   -> 3, 21, 11   event 4 -> 0x140119f00
//   state 11  enter 0x140119e60  trans38 0x140119d80   -> 3, 21
// States 3 and 21 are transition TARGETS with no registered body — exactly as the
// orchard table leaves its state 3 — so they are deliberately absent here too.
//
// ⚠ FIRST PASS: the handler BODIES are not ported yet, so every handler is registered
// as nullptr. ControllerLive::Dispatcher treats a null handler as a graceful no-op, so
// the state machine still runs the REAL structure — Step returns real indices and
// transitions are validated against the real graph — it simply performs no per-state
// work. This is the same staged approach the orchard table went through, and it makes
// the component observable (BANISHED_FOOD_DIAG reports its state) before any behaviour
// is added. Do NOT read "crop field does nothing" as a defect until the bodies land.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

namespace { struct CropFieldCtrlTag {}; }
using CropFieldDispatch = ::ControllerLive::Dispatcher<CropFieldCtrlTag>;

extern "C" void** CropFieldCtrl_NodeVtable() { return CropFieldDispatch::Vtable(); }

// ── Resolved helpers ──────────────────────────────────────────────────────────────
// 0x140002e10 FindComponent_WorkPlace(entity, outIBP) -> WorkPlaceComponent.
// 0x14012fc00 WorkPlace SetAccepting(wpc, accepting)  -> drives WPC+0x50.
namespace {

typedef void (*fn_FindComp_t)(uintptr_t entity, uintptr_t* out);
typedef void (*fn_SetAccepting_t)(uintptr_t wpc, char accepting);

fn_FindComp_t     s_findWpc      = nullptr;   // 0x140002e10
fn_SetAccepting_t s_setAccepting = nullptr;   // 0x14012fc00
const float*      s_fTempMin     = nullptr;   // 0x1401721d4 = 33.0f
const float*      s_fYearSeconds = nullptr;   // 0x14017225c = 3600.0f
const float*      s_fYearMonths  = nullptr;   // 0x1401721b8 = 12.0f
bool              s_resolved     = false;

void CF_Resolve() {
    if (s_resolved) return;
    s_findWpc      = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_setAccepting = reinterpret_cast<fn_SetAccepting_t>(ResolveAddr(Module::Exe, 0x14012fc00));
    s_fTempMin     = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721d4));
    s_fYearSeconds = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017225c));
    s_fYearMonths  = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721b8));
    s_resolved = true;
}

// Push the component's +0x48 master switch into the workplace's accepting flag.
// Shared by state-2 enter and the event-3 plant command, both of which do exactly
// this before doing anything else.
void CF_PushAccepting(uintptr_t comp) {
    CF_Resolve();
    if (!s_findWpc || !s_setAccepting) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t wpcIbp = 0;
    s_findWpc(*reinterpret_cast<uintptr_t*>(comp + 0x10), &wpcIbp);
    if (!wpcIbp) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIbp);
    // wpcIbp already IS the component — the finder returns its out-param's address, so
    // there is no second dereference here (the trap that crashed the RP family).
    s_setAccepting(wpcIbp, *reinterpret_cast<char*>(comp + 0x48));
}

}  // namespace

// ── state 2 enter — 0x14011a920 ───────────────────────────────────────────────────
// Nothing but "publish my master switch to the workplace".
static char CropField_State2Enter(void* self, void* /*sm*/) {
    CF_PushAccepting(reinterpret_cast<uintptr_t>(self));
    return 0;
}

// ── event 3 on state 2 — 0x14011a7b0 — the panel's PLANT command ──────────────────
// Publish accepting, then request state 9.
static char CropField_Event3Plant(void* self, void* sm) {
    CF_PushAccepting(reinterpret_cast<uintptr_t>(self));
    ::ControllerLive::RequestStateTransition(sm, 9);
    return 1;
}

// ── state 2 per-frame — 0x14011a860 — the SEASONAL PLANTING TRIGGER ───────────────
// Faithful shape:
//   if (self+0x48 == 0) return 1;                       // field switched off
//   if (weather(gw+0x130)+0x18 < 33.0f) return 1;       // too cold
//   v = (int)((Time::AsSeconds(gw+0x128 + 8) / 3600.0f) * 12.0f);
//   if (v < 0) return 1;
//   if ((unsigned)(v - 3) > 8) Event3Plant(self, sm);   // -> state 9
//
// ⚠ That last test is an UNSIGNED compare on `v - 3`, so it is true both when v > 11
// AND when v < 3 (the subtraction underflows to a huge unsigned). It is retail's
// idiomatic "outside the closed window [3, 11]" check — i.e. plant in the cold months
// at either end of the year, not mid-season. Writing it as a signed `v - 3 > 8` would
// silently drop the entire v < 3 arm and the field would only ever plant late in the
// year. Kept as an explicit unsigned cast so the behaviour cannot drift.
static char CropField_State2PerFrame(void* self, void* sm, void* /*time*/) {
    CF_Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (*reinterpret_cast<char*>(comp + 0x48) == 0) return 1;
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;
    if (!fn_Time_AsSeconds || !s_fTempMin || !s_fYearSeconds || !s_fYearMonths) return 1;

    const uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
    const uintptr_t seasonMgr = *reinterpret_cast<uintptr_t*>(gw + 0x128);
    const uintptr_t weather   = *reinterpret_cast<uintptr_t*>(gw + 0x130);
    if (!seasonMgr || !weather) return 1;

    if (*reinterpret_cast<float*>(weather + 0x18) < *s_fTempMin) return 1;

    const float secs = fn_Time_AsSeconds(reinterpret_cast<void*>(seasonMgr + 8));
    const int v = static_cast<int>((secs / *s_fYearSeconds) * *s_fYearMonths);
    if (v < 0) return 1;
    if (static_cast<uint32_t>(v - 3) > 8u)
        CropField_Event3Plant(self, sm);
    return 1;
}

// Replica of the populator at 0x14011b150, in registration order. Order matters: it
// fixes the state INDEX that StateMachine::Tick carries in its +0x09/+0x0a/+0x0b bytes.
extern "C" void CropFieldCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = CropFieldDispatch::table;
    if (t.count != 0) return;                       // idempotent — runs per instance

    // ── state 2 — DEFAULT / idle ──────────────────────────────────────────────────
    // Slot 16 (GetDefaultState) is the shared 0x1400026c0 = `MOV EAX,2; RET`, so this
    // is where a new field starts. enter 0x14011a920 / trans38 0x14011a860; the event-3
    // body 0x14011a7b0 is the panel's plant command, mirroring the orchard's event 3.
    TableRegisterState(t, 2, reinterpret_cast<void*>(&CropField_State2Enter), nullptr,
                       reinterpret_cast<void*>(&CropField_State2PerFrame), nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 9,  nullptr);
    TableAddEvent(t, 3, reinterpret_cast<void*>(&CropField_Event3Plant));

    // ── state 9 — PLANT ───────────────────────────────────────────────────────────
    // enter 0x14011a310 / trans38 0x14011a1e0.
    TableRegisterState(t, 9, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 10, nullptr);

    // ── state 10 — TEND / GROW ────────────────────────────────────────────────────
    // The only state with an UPDATE handler (0x140119f70) as well as a per-frame
    // trans38 (0x140119fd0); enter 0x14011a100. Event 4 (0x140119f00) is the harvest
    // command, and 21 is the clear/cut target.
    TableRegisterState(t, 10, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 21, nullptr);
    TableAddTransition(t, 11, nullptr);
    TableAddEvent(t, 4, nullptr);

    // ── state 11 — HARVEST ────────────────────────────────────────────────────────
    // enter 0x140119e60 / trans38 0x140119d80.
    TableRegisterState(t, 11, nullptr, nullptr, nullptr, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddTransition(t, 21, nullptr);
}
