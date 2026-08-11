// code/Game/Component/resourceproducecomponent_states.cpp
// The SHARED ResourceProduceComponent controller state machine.
//
// Unlike Orchard/Pasture/CropField — which each own a private state table — every
// ResourceProduceComponent subclass INHERITS the base controller vtable
// (0x1401643a0) and therefore shares ONE state table (retail DAT_1401db920,
// count DAT_1401dc8a0). So this single file drives the Fishing Dock, the Hunting
// Cabin and the Gatherer's Hut (HerbalistComponent), plus the other RPC subclasses
// (Forester, Market, ConsumeProduce) once those are promoted off inert.
//
// Populated in retail by ResourceProduceDescription_helper_b230 (0x14012b230),
// reached from ResourceProduceDescription_Setup (0x14014f970). The table has
// exactly three states:
//
//   state 2  (idle)      enter 0x14012b1b0             — pick 13 or 14 from +0x48
//                        ->13 action 0x14012af90       — stamp the work positions
//                        ->14, ->3 (no action)
//   state 13 (producing) enter 0x14012ae60             — accept workers + post work
//                        trans38 0x14012b190           — per-frame produce tick
//                        ->3  action 0x14012af70       — stop producing
//                        event 3 0x14012adf0           — panel pause -> 14
//   state 14 (paused)    enter 0x14012ad20             — stop accepting + clear work
//                        ->13, ->3 (no action)
//                        event 3 0x14012acb0           — panel resume -> 13
//
// State 3 is the shared teardown id: a transition target only, with no body — so it
// is deliberately NOT registered here, exactly as retail leaves it.
//
// The per-class behaviour hangs off the COMPONENT vtable, not off this table:
//   slot 18 (+0x90) CreateWork      — _purecall in the base; every subclass overrides
//   slot 19 (+0x98) OnEnterProducing— base no-op 0x140006b10
//   slot 20 (+0xa0) OnExitProducing — base no-op 0x140006b10
//   slot 21 (+0xa8) ProduceTick     — base no-op 0x140006b10
// See notes/FOOD_PRODUCTION_WAVE_BRIEF.md for the byte-level derivation.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace { struct RPCtrlTag {}; }
using RPDispatch = ::ControllerLive::Dispatcher<RPCtrlTag>;

extern "C" void** RPCtrl_NodeVtable() { return RPDispatch::Vtable(); }

// ── Resolved engine helpers ───────────────────────────────────────────────────────
// 0x140003050 FindComponent_Work(entity, outIBP)  — the work-ledger component that
//             CitizenAI_impl_ec60 posts against (same finder BuildComponent state 1 uses).
// 0x140002e10 FindComponent_WorkPlace(entity, outIBP) -> WorkPlaceComponent.
// 0x14012ec60 CitizenAI_impl_ec60(workComp, code, flag, workIBP) — post to the ledger.
// 0x14012fc00 WorkPlace SetAccepting(wpc, accepting) — drives WPC+0x50.
// 0x14012ead0 / 0x14012eb50 — work-slot occupied test / release (citizen work scheduler).
typedef void  (*fn_FindComp_t)(uintptr_t entity, uintptr_t* outIBP);
typedef void  (*fn_PostWork_t)(uintptr_t workComp, uint32_t code, char flag, void* workIBP);
typedef void  (*fn_SetAccepting_t)(uintptr_t wpc, char accepting);
typedef bool  (*fn_SlotOccupied_t)(uintptr_t ai);
typedef void  (*fn_SlotRelease_t)(uintptr_t ai);

static fn_FindComp_t     s_findWork      = nullptr;   // 0x140003050
static fn_FindComp_t     s_findWpc       = nullptr;   // 0x140002e10
static fn_PostWork_t     s_postWork      = nullptr;   // 0x14012ec60
static fn_SetAccepting_t s_setAccepting  = nullptr;   // 0x14012fc00
static fn_SlotOccupied_t s_slotOccupied  = nullptr;   // 0x14012ead0
static fn_SlotRelease_t  s_slotRelease   = nullptr;   // 0x14012eb50
static fn_FindComp_t     s_findSM        = nullptr;   // 0x140003ea0 FindComponent_StateMachine
static bool              s_resolved      = false;

static void RP_Resolve() {
    if (s_resolved) return;
    s_findWork     = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_findWpc      = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_postWork     = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_setAccepting = reinterpret_cast<fn_SetAccepting_t>(ResolveAddr(Module::Exe, 0x14012fc00));
    s_slotOccupied = reinterpret_cast<fn_SlotOccupied_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_slotRelease  = reinterpret_cast<fn_SlotRelease_t>(ResolveAddr(Module::Exe, 0x14012eb50));
    s_findSM       = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_resolved = true;
}

// One-shot bring-up trace. Default off; BANISHED_RESOURCEPRODUCE_TRACE=1 enables.
// Reports OUTCOMES (which state, whether the post/accept actually happened), never
// just intent — a silent handler and an absent handler must not look the same.
static bool RP_Trace() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_RESOURCEPRODUCE_TRACE");
        s = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return s == 1;
}

// The component's own vtable slot, by index. The RPC vtable is 22 slots wide; the
// production hooks are 18..21. Slot 18 is _purecall in the base, so a subclass that
// forgot to override it would abort() — we null-check instead and report it.
static void* RP_VSlot(void* self, int idx) {
    if (!self) return nullptr;
    void** vt = *reinterpret_cast<void***>(self);
    return vt ? vt[idx] : nullptr;
}

// ── state 2 enter — 0x14012b1b0 ───────────────────────────────────────────────────
// target = (self+0x48 == 0) ? 13 : 14, then request it. +0x48 is the produceFlag
// (the panel's pause bit), zeroed by ResourceProduceComponent_Ctor.
static char RP_State2Enter(void* self, void* sm) {
    const uint8_t paused = *reinterpret_cast<uint8_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x48);
    const int target = paused ? 14 : 13;
    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace] State2Enter self=%p paused=%u -> request %d\n",
                     self, (unsigned)paused, target);
    ::ControllerLive::RequestStateTransition(sm, target);
    return 0;
}

// ── state 13 enter — 0x14012ae60 ──────────────────────────────────────────────────
// If the produceFlag is set, bounce straight to 14 (retail's early-out on param_1[9],
// which is the qword at +0x48 read as a byte). Otherwise:
//   work = self->vtable[18](self, &out)      CreateWork — per-class
//   CitizenAI_impl_ec60(workComp, 1, 0, &work)
//   SetAccepting(wpc, 1)
//   return self->vtable[19](self)            OnEnterProducing
static char RP_State13Enter(void* self, void* sm) {
    RP_Resolve();
    uintptr_t comp = reinterpret_cast<uintptr_t>(self);

    // Entry trace as well as the outcome trace below: without it a crash anywhere in
    // this body is indistinguishable from the state never being entered at all.
    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace] State13Enter ENTER self=%p\n", self);

    if (*reinterpret_cast<uint8_t*>(comp + 0x48) != 0) {
        if (RP_Trace())
            std::fprintf(stderr, "[RPTrace] State13Enter self=%p paused -> request 14\n", self);
        ::ControllerLive::RequestStateTransition(sm, 14);
        return 1;
    }

    const uintptr_t owner = *reinterpret_cast<uintptr_t*>(comp + 0x10);

    // CreateWork (slot 18) — allocates the class's Work object and hands back an
    // InternalBasePtr. Abstract in the base, so a null here means the subclass did
    // not override it: report rather than crash on a _purecall.
    uintptr_t workIbp = 0;
    typedef uintptr_t* (*fn_CreateWork_t)(void* self, uintptr_t* outIBP);
    fn_CreateWork_t createWork = reinterpret_cast<fn_CreateWork_t>(RP_VSlot(self, 18));
    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace]   step1 owner=%p createWork=%p\n",
                     (void*)owner, (void*)createWork);
    if (createWork) {
        createWork(self, &workIbp);
        if (RP_Trace()) std::fprintf(stderr, "[RPTrace]   step2 work=%p\n", (void*)workIbp);
    } else if (RP_Trace()) {
        std::fprintf(stderr, "[RPTrace] State13Enter self=%p has NO slot-18 CreateWork\n", self);
    }

    // Post it to the reservation ledger so an idle citizen claims it.
    uintptr_t workCompIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    if (s_findWork) s_findWork(owner, &workCompIbp);
    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace]   step3 workComp=%p post=%p\n",
                     (void*)workCompIbp, (void*)s_postWork);
    bool posted = false;
    if (s_postWork && workCompIbp != 0 && workIbp != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIbp);
        s_postWork(workCompIbp, 1, 0, &workIbp);
        posted = true;
    }
    if (RP_Trace()) std::fprintf(stderr, "[RPTrace]   step4 posted=%d\n", (int)posted);

    // WorkPlaceComponent +0x50 = accepting. Without this the building has workers
    // assigned but every gather bails "no target": the same trap the pasture wave hit.
    uintptr_t wpcIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    if (s_findWpc) s_findWpc(owner, &wpcIbp);
    bool accepted = false;
    if (wpcIbp != 0 && s_setAccepting) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIbp);
        // wpcIbp ALREADY holds the component pointer: the finder writes it into the
        // out-param and returns the out-param's ADDRESS, so retail's `local_res18`
        // and this local are the same value. Dereferencing again would pass the
        // component's VTABLE pointer as the workplace — which is exactly what
        // crashed here first time round.
        s_setAccepting(wpcIbp, 1);
        accepted = true;
    }

    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace] State13Enter self=%p work=%p posted=%d accepting=%d\n",
                     self, (void*)workIbp, (int)posted, (int)accepted);

    typedef char (*fn_Hook_t)(void* self);
    fn_Hook_t onEnter = reinterpret_cast<fn_Hook_t>(RP_VSlot(self, 19));
    return onEnter ? onEnter(self) : 0;
}

// ── state 14 enter — 0x14012ad20 ──────────────────────────────────────────────────
// Release the held work slot if occupied, stop accepting, then OnExitProducing.
static char RP_State14Enter(void* self, void* /*sm*/) {
    RP_Resolve();
    uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    const uintptr_t owner = *reinterpret_cast<uintptr_t*>(comp + 0x10);

    uintptr_t workCompIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    if (s_findWork) s_findWork(owner, &workCompIbp);
    bool released = false;
    if (workCompIbp != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&workCompIbp);
        // Same out-param convention as State13Enter: this local is the component.
        if (s_slotOccupied && s_slotRelease && s_slotOccupied(workCompIbp)) {
            s_slotRelease(workCompIbp);
            released = true;
        }
    }

    uintptr_t wpcIbp = 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    if (s_findWpc) s_findWpc(owner, &wpcIbp);
    if (wpcIbp != 0 && s_setAccepting) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&wpcIbp);
        s_setAccepting(*reinterpret_cast<uintptr_t*>(wpcIbp), 0);
    }

    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace] State14Enter self=%p releasedWork=%d\n",
                     self, (int)released);

    typedef char (*fn_Hook_t)(void* self);
    fn_Hook_t onExit = reinterpret_cast<fn_Hook_t>(RP_VSlot(self, 20));
    if (onExit) onExit(self);
    return 1;
}

// ── state 13 per-frame — 0x14012b190 ──────────────────────────────────────────────
// A one-line forward to the component's ProduceTick (slot 21). The base leaves it a
// no-op; FishingComponent is the only subclass in this wave that installs a real one.
static char RP_State13PerFrame(void* self, void* /*sm*/, void* time) {
    typedef char (*fn_Tick_t)(void* self, void* time);
    fn_Tick_t tick = reinterpret_cast<fn_Tick_t>(RP_VSlot(self, 21));
    return tick ? tick(self, time) : 0;
}

// ── transition 13 -> 3 action — 0x14012af70 ───────────────────────────────────────
// Teardown path: run OnExitProducing so the class drops whatever the produce tick
// was holding. Dispatched through OnState, so the signature is void(data).
static void RP_Trans13to3(void* self) {
    typedef char (*fn_Hook_t)(void* self);
    fn_Hook_t onExit = reinterpret_cast<fn_Hook_t>(RP_VSlot(self, 20));
    if (onExit) onExit(self);
}

// ── event 3 on state 13 — 0x14012adf0 — panel PAUSE ───────────────────────────────
static char RP_Event3Pause(void* self, void* sm) {
    if (RP_Trace())
        std::fprintf(stderr, "[RPTrace] Event3 on state13 self=%p -> request 14 (pause)\n", self);
    ::ControllerLive::RequestStateTransition(sm, 14);
    return 1;
}

// ── event 3 on state 14 — 0x14012acb0 — panel RESUME ──────────────────────────────
// Retail gates the resume on +0x48 == 0: a component whose produceFlag is set stays
// paused, so the event is a no-op rather than a toggle. Faithful, not a guard.
static char RP_Event3Resume(void* self, void* sm) {
    const uint8_t paused = *reinterpret_cast<uint8_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x48);
    if (paused == 0) {
        if (RP_Trace())
            std::fprintf(stderr, "[RPTrace] Event3 on state14 self=%p -> request 13 (resume)\n", self);
        ::ControllerLive::RequestStateTransition(sm, 13);
    } else if (RP_Trace()) {
        std::fprintf(stderr, "[RPTrace] Event3 on state14 self=%p IGNORED (+0x48=%u)\n",
                     self, (unsigned)paused);
    }
    return 1;
}

// ── component vtable slot 13 — retail 0x14012b520 — the panel COMMAND MASK ────────
// Shared by every ResourceProduceComponent subclass (Ghidra labels it
// "HerbalistComponent_Slot13", but the byte lives on the base vtable and all three food
// subclasses inherit it — see notes/FOOD_PRODUCTION_WAVE_BRIEF.md).
//
// ⚠ This is NOT CanCommand(self, id). Retail's body takes only `self` and returns a
// BITMASK of which panel commands are currently available:
//     smc  = FindComponent_StateMachine(owner)                  (0x140003ea0)
//     cur  = smc+0x52 (pending state); if negative, smc+0x50 (current state)
//     mask = (cur != 14) ? 2 : 0                                 // pause offered
//     if (self+0x48 == 0) mask |= ((unsigned)(cur - 13) < 2)     // resume offered
// So bit 1 is "pause" (offered unless already paused in state 14) and bit 0 is
// "resume" (offered while in state 13 or 14 and not flagged). Any extra argument a
// caller passes is ignored, exactly as retail ignores it.
extern "C" uint8_t ResourceProduceComponent_CommandMask(void* self) {
    RP_Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (!s_findSM) return 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t smcIbp = 0;
    s_findSM(*reinterpret_cast<uintptr_t*>(comp + 0x10), &smcIbp);
    if (!smcIbp) return 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&smcIbp);

    int8_t cur = *reinterpret_cast<int8_t*>(smcIbp + 0x52);
    if (cur < 0) cur = *reinterpret_cast<int8_t*>(smcIbp + 0x50);

    uint8_t mask = (cur != 14) ? 2 : 0;
    if (*reinterpret_cast<uint8_t*>(comp + 0x48) == 0)
        mask |= (static_cast<uint32_t>(cur - 13) < 2u) ? 1 : 0;
    return mask;
}

// ── component vtable slot 14 — retail 0x14012b5a0 — the panel COMMAND ─────────────
//     smc = FindComponent_StateMachine(owner)
//     node = *(smc+0x48); node->vtable[2](node, smc+0x48, smc+0x51, 0)   // PostEvent
//     smc->vtable[8](smc, Time(0.0))                                      // force a tick
//
// ⚠ The event id is a hard-coded 0 (`XOR R9D,R9D` in the disassembly) and NO command id
// is read — the slot takes only `self`. The shared state table registers event 3, not 0,
// so this PostEvent finds no handler and returns 0 gracefully; the observable effect is
// the forced tick that follows. That mismatch is retail's, reproduced rather than
// "corrected": posting 3 here would be inventing behaviour the game does not have.
extern "C" void ResourceProduceComponent_Command(void* self) {
    RP_Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    if (!s_findSM) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    uintptr_t smcIbp = 0;
    s_findSM(*reinterpret_cast<uintptr_t*>(comp + 0x10), &smcIbp);
    if (!smcIbp) return;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&smcIbp);

    const uintptr_t sm = smcIbp + 0x48;
    void* node = *reinterpret_cast<void**>(sm);
    if (node) {
        void** nvt = *reinterpret_cast<void***>(node);
        if (nvt && nvt[2]) {
            const int curIdx = *reinterpret_cast<int8_t*>(smcIbp + 0x51);
            reinterpret_cast<char (*)(void*, void*, int, int)>(nvt[2])(
                node, reinterpret_cast<void*>(sm), curIdx, 0);
        }
    }

    // Force an immediate StateMachineComponent tick (its vtable slot 8) with Time(0).
    void** svt = *reinterpret_cast<void***>(smcIbp);
    if (svt && svt[8]) {
        int64_t zeroTime = 0;
        if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&zeroTime, 0.0f);
        reinterpret_cast<void (*)(void*, void*)>(svt[8])(
            reinterpret_cast<void*>(smcIbp), &zeroTime);
    }
}

// ── table population — replica of ResourceProduceDescription_helper_b230 ──────────
// Registration ORDER matters: it fixes the state INDEX that StateMachine::Tick carries
// in its +0x09/+0x0a/+0x0b bytes, so it mirrors retail's call order exactly.
extern "C" void RPCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = RPDispatch::table;
    if (t.count != 0) return;                       // idempotent — runs per instance

    // ── state 2 — DEFAULT / idle ──────────────────────────────────────────────────
    TableRegisterState(t, 2, reinterpret_cast<void*>(&RP_State2Enter), nullptr,
                       nullptr, nullptr);
    // ->13 carries retail's 0x14012af90, which stamps the building's work positions
    // into the map (terrain byte +0x29 = 2) for as many slots as the workplace wants.
    // NOT yet ported — registered null, which ControllerLive::Dispatcher treats as a
    // graceful no-op, exactly as the orchard table did on its first pass. The state
    // machine still runs the real graph; only the map stamping is absent. Tracked as
    // the next step of this wave, and the trace above will show 13 being entered.
    TableAddTransition(t, 13, nullptr);
    TableAddTransition(t, 14, nullptr);
    TableAddTransition(t, 3,  nullptr);

    // ── state 13 — PRODUCING ──────────────────────────────────────────────────────
    TableRegisterState(t, 13, reinterpret_cast<void*>(&RP_State13Enter), nullptr,
                       reinterpret_cast<void*>(&RP_State13PerFrame), nullptr);
    TableAddTransition(t, 14, nullptr);
    TableAddTransition(t, 3,  reinterpret_cast<void*>(&RP_Trans13to3));
    TableAddEvent(t, 3, reinterpret_cast<void*>(&RP_Event3Pause));

    // ── state 14 — PAUSED / not accepting ─────────────────────────────────────────
    TableRegisterState(t, 14, reinterpret_cast<void*>(&RP_State14Enter), nullptr,
                       nullptr, nullptr);
    TableAddTransition(t, 13, nullptr);
    TableAddTransition(t, 3,  nullptr);
    TableAddEvent(t, 3, reinterpret_cast<void*>(&RP_Event3Resume));
}
