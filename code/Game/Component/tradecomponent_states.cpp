// code/Game/Component/tradecomponent_states.cpp
// TradeComponent's controller state machine — the thing that makes a trading post
// drive itself instead of waiting for the player to poke the work toggle.
//
// tradecomponent.cpp deliberately left the controller sub-object inert:
//   "DELIBERATELY NOT applied: +0x88 (the controller sub-object's own vtable) and
//    +0xa0 (its self back-pointer). ... They stay inert until someone ports the
//    controller."
// This file is that port. Node offset 0x88 and default state 2 are not guesses —
// TradeComponent's retail vtable 0x14015acd0 holds slot 16 = 0x1400026c0
// (`MOV EAX,2; RET`) and slot 17 = 0x14000dba0 (`LEA RAX,[RCX+0x88]; RET`).
//
// ── The retail table (TradeDescription_Setup -> 0x14005c650) ──────────────────────
// c650 builds the table with the same three primitives controller_state_table.h
// already models: 0x14000dd70 = register state, 0x14000de90 = add transition,
// 0x14000df40 = add event. Argument mapping read off c650's disassembly
// (dd70's 5th argument arrives on the stack at [RSP+0x20], which the decompiler
// shows as `in_R9` noise — it is the trans38/per-frame handler, not R9):
//
//   dd70(2,    0x14005abd0, 0,           _, 0x14005aa60)
//     de90(3, 0x14005b160)   de90(0x12, 0)
//   dd70(0x12, 0x14005aa50, 0,           _, 0)
//     de90(3, 0x14005b160)   de90(0x13, 0)   de90(2, 0)
//     df40(9, 0x14005a850)   df40(0xb, 0x14005a9c0)
//   dd70(0x13, 0x14005a800, 0x14005a6d0, _, 0x14005a780)
//     de90(3, 0x14005b160)   de90(2, 0)
//     df40(0xa, 0x14005b240)
//
// so, in StateEntry terms (+0x08 enter, +0x18 trans38, +0x20 update):
//
//   state 2    IDLE / waiting for the next merchant
//     enter   abd0  post a TradeWork if the post is accepting, then arm the timer
//     trans38 aa60  per-frame: count the timer down, spawn the merchant, -> 0x12
//   state 0x12 MERCHANT INBOUND (boat en route)
//     enter   aa50  `MOV AL,1; RET` — nothing to do
//     event 9    a850  boat docked: write the event-log line, -> 0x13
//     event 0xb  a9c0  boat lost/despawned: drop the merchant ref, -> 2
//   state 0x13 MERCHANT DOCKED (the panel can trade)
//     enter   a800  build the merchant's manifest, arm the stay timer, auto-buy
//     trans38 a780  per-frame: stay timer expired and not mid-trade -> 2
//     update  a6d0  leaving: auto-sell, release the boat, clear the manifest
//     event 0xa  b240  player dismissed the merchant -> 2 (inline SM tick)
//   every state carries transition 3 -> b160, the shared teardown.
//
// ── WHAT IS PORTED HERE, AND WHAT IS NOT ─────────────────────────────────────────
// This is layer 1 of the wave: the table, the node wiring, and the state-2 self-drive
// path. Ported and wired: abd0, aa50, a780, a9c0, plus the leaves abd0 needs
// (0x14005b450 timer arm, 0x14005b4f0 tier pick, 0x1400f6ea0 TradeWork ctor).
//
// NOT ported, and therefore registered as NULL handlers (which the dispatcher treats
// as a graceful no-op, exactly as retail treats an unregistered slot):
//   0x14005aa60  state-2 per-frame merchant spawn — blocked on 0x1400456e0, the river
//                route builder (~1.5 KB, two 4096-int stack maps). Without it a boat
//                can be created but not routed, so the whole spawn is held back rather
//                than half-done.
//   0x14005b160  shared teardown — also calls 0x1400456e0 (with a null ref, its
//                "release the boat" branch).
//   0x14005a800 / 0x14005a6d0  dock enter/leave — blocked on 0x14005b580 (the merchant
//                manifest generator) and 0x140059ad0 (auto-buy, ~2.5 KB).
//   0x14005a850  dock event-log line — needs the EventLog string/format chain.
//   0x14005b240  dismiss event — retail inlines the whole of StateMachine::Tick here.
//
// So with this layer alone the machine runs, sits in state 2, and re-posts trade work
// on its own; it does not yet advance to 0x12/0x13. That is a real behaviour change
// (today NOTHING posts trade work except the player clicking the toggle) and a
// deliberately visible stopping point, not a silent partial.
//
// GATED: BANISHED_LIVE_TRADE_CTRL, opt-in and NOT in kDefaultOnFlags. Unproven.

#include "Component/controller_state_table.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Work/tradework_funcs.h"

namespace { struct TradeCtrlTag {}; }
using TradeDispatch = ::ControllerLive::Dispatcher<TradeCtrlTag>;

extern "C" void** TradeCtrl_NodeVtable() { return TradeDispatch::Vtable(); }

namespace {

// ── Resolved leaves ──────────────────────────────────────────────────────────────
// Bound by ADDRESS, not through named wrappers: 0x140002e10 and 0x140003050 both
// carry misleading names in our tree (see the note in orchardcomponent_states.cpp),
// and binding by address keeps these ports immune to the naming question.
typedef void*    (*fn_Find_t)(int64_t entity, void* outIBP);
typedef char     (*fn_HasWorkSlot_t)(int64_t sched);
typedef void*    (*fn_WorkCtor25_t)(void* work, void* entityIBP);
typedef uint64_t (*fn_PostWork_t)(int64_t sched, int code, char flag, void* workIBP);
typedef void*    (*fn_TimeCtor_t)(void* outTime, float seconds);

fn_Find_t         s_FindWorkPlace = nullptr;   // 0x140002e10
fn_Find_t         s_FindSched     = nullptr;   // 0x140003050
fn_HasWorkSlot_t  s_HasWorkSlot   = nullptr;   // 0x14012ead0
fn_WorkCtor25_t   s_WorkCtor25    = nullptr;   // 0x1400f72e0
fn_PostWork_t     s_PostWork      = nullptr;   // 0x14012ec60
fn_TimeCtor_t     s_TimeCtor      = nullptr;   // dispatch slot [0x140152b00]
void**            s_TradeWorkVtbl = nullptr;   // 0x140161a10
bool              s_resolved      = false;

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    s_FindWorkPlace = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_FindSched     = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_HasWorkSlot   = reinterpret_cast<fn_HasWorkSlot_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_WorkCtor25    = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_PostWork      = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_TradeWorkVtbl = reinterpret_cast<void**>(ResolveAddr(Module::Exe, 0x140161a10));
    if (uintptr_t slot = ResolveAddr(Module::Exe, 0x140152b00))
        s_TimeCtor = *reinterpret_cast<fn_TimeCtor_t*>(slot);

    // Report the OUTCOME of every dependency. A single null here turns the whole
    // state machine into a machine that runs and does nothing — believable and wrong.
    std::fprintf(stderr,
        "[TradeCtrl] deps: findWP=%p findSched=%p hasWork=%p workCtor=%p post=%p "
        "timeCtor=%p tradeWorkVtbl=%p\n",
        reinterpret_cast<void*>(s_FindWorkPlace), reinterpret_cast<void*>(s_FindSched),
        reinterpret_cast<void*>(s_HasWorkSlot),   reinterpret_cast<void*>(s_WorkCtor25),
        reinterpret_cast<void*>(s_PostWork),      reinterpret_cast<void*>(s_TimeCtor),
        reinterpret_cast<void*>(s_TradeWorkVtbl));
    std::fflush(stderr);
}

// The TradeDescription off component+0x18, cast exactly as retail does it.
// The deref of the result is left UNGUARDED below, as retail leaves it: retail
// relies on the cast succeeding, and a guard here would skip the behaviour rather
// than restore it. If this ever faults, the description on +0x18 is the bug.
void* TradeDesc(uintptr_t comp) {
    void* raw = *reinterpret_cast<void**>(comp + 0x18);
    if (!g_TypeInfo_TradeDescription || !fn_IsDerivedFrom) return nullptr;
    return fn_IsDerivedFrom(raw, *g_TypeInfo_TradeDescription) ? raw : nullptr;
}

// Rate-limited outcome trace for the state-2 enter. Prints on EVERY branch, including
// the ones that post nothing, so a silent log means the handler never ran rather than
// that it ran and succeeded.
void TradeCtrlReport(uintptr_t comp, bool tried, void* work, bool schedHasWork) {
    static int n = 0;
    if (n >= 16) return;
    ++n;
    std::fprintf(stderr,
        "[TradeCtrl] state2 enter comp=%p accepting=%s work=%p schedHasWork=%s "
        "timer(+0x78)=%lld\n",
        reinterpret_cast<void*>(comp), tried ? "yes" : "no", work,
        schedHasWork ? "yes" : "no",
        static_cast<long long>(*reinterpret_cast<int64_t*>(comp + 0x78)));
    std::fflush(stderr);
}

} // namespace

// ============================================================
// 0x14005b4f0 — pick the merchant tier for the town's current size
// ============================================================
// Walks the description's tier array (desc+0x58, stride 0x14) and returns the index
// of the first tier whose threshold (+0x00) is above the town's current population
// figure at *(gameWorld+0xf0)+0xa8; if none is, the last tier. A negative population
// short-circuits to the current index, which is retail's behaviour on an unbuilt town.
extern "C" int TradeComponent_PickMerchantTier(void* selfV) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    uint8_t* desc = static_cast<uint8_t*>(TradeDesc(comp));

    const int count = *reinterpret_cast<int*>(desc + 0x60);
    const int64_t tiers = *reinterpret_cast<int64_t*>(desc + 0x58);
    int idx = 0;
    for (int i = 0; i < count; ++i) {
        const int64_t townState = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf0);
        const int pop = *reinterpret_cast<int*>(townState + 0xa8);
        if (pop >= 0 && pop < *reinterpret_cast<int*>(tiers + static_cast<int64_t>(i) * 0x14))
            return idx;
        ++idx;
    }
    return count - 1;
}

// ============================================================
// 0x14005b450 — arm the countdown to the next merchant
// ============================================================
// comp+0x78 = Time( (RandFloatTol() * tier.spread + tier.base) * 300.0f )
// The tier record is 0x14 bytes: +0x00 population threshold, +0x04 base delay,
// +0x08 tolerance. DAT_140172234 is the literal 300.0f (0x43960000).
extern "C" void TradeComponent_ArmMerchantTimer(void* selfV) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    uint8_t* desc = static_cast<uint8_t*>(TradeDesc(comp));

    const int tier = TradeComponent_PickMerchantTier(selfV);
    const int64_t tiers = *reinterpret_cast<int64_t*>(desc + 0x58);
    const int64_t rec = tiers + static_cast<int64_t>(tier) * 0x14;

    void* rng = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xd8);
    const float tol = fn_Random_RandFloatTol ? fn_Random_RandFloatTol(rng) : 0.0f;
    const float seconds = (tol * *reinterpret_cast<float*>(rec + 8) +
                                 *reinterpret_cast<float*>(rec + 4)) * 300.0f;

    if (!s_TimeCtor) return;
    uint64_t t[4] = {0, 0, 0, 0};
    uint64_t* out = reinterpret_cast<uint64_t*>(s_TimeCtor(t, seconds));
    *reinterpret_cast<int64_t*>(comp + 0x78) = static_cast<int64_t>(*out);

    // OUTCOME: the armed countdown read back off the component, not the seconds we
    // computed. A zero here means the arm silently did nothing, which is exactly the
    // state that would make a post sit idle forever while looking configured.
    static int reported = 0;
    if (reported < 8) {
        ++reported;
        std::fprintf(stderr,
            "[TradeCtrl] arm timer comp=%p tier=%d base=%.2f tol=%.2f -> %.1fs "
            "timer(+0x78)=%lld\n",
            reinterpret_cast<void*>(comp), tier,
            *reinterpret_cast<float*>(rec + 4), *reinterpret_cast<float*>(rec + 8),
            seconds,
            static_cast<long long>(*reinterpret_cast<int64_t*>(comp + 0x78)));
        std::fflush(stderr);
    }
}

// ============================================================
// 0x1400f6ea0 — TradeWork::TradeWork(entityIBP)
// ============================================================
// The real ctor. tradecomponent.cpp's slot-14 toggle hand-rolled this inline and
// had to call TradeWork_ResolveFuncs() itself to compensate; going through the ctor
// keeps the one construction path. See reference_creatework_must_call_work_resolvefuncs.
extern "C" void* TradeWork_CtorFromEntity(void* mem, void* entityIBP) {
    Resolve();
    if (!mem || !s_WorkCtor25 || !s_TradeWorkVtbl) return nullptr;
    TradeWork_ResolveFuncs();
    uint64_t ibp[2] = {*reinterpret_cast<uint64_t*>(entityIBP), 0};
    s_WorkCtor25(mem, ibp);
    *reinterpret_cast<void***>(mem) = s_TradeWorkVtbl;
    *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(mem) + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(mem) + 0x50) = 0;
    return mem;
}

// ============================================================
// state 2 ENTER — 0x14005abd0
// ============================================================
// THE self-drive step. On entering idle: if the scheduler is not already holding a
// work slot and the workplace is accepting, allocate a TradeWork and post it. Then
// arm the merchant countdown. Today nothing does this without the player clicking
// the work toggle (TradeComponent_Slot14), which is why a post that has never been
// toggled never fetches anything.
//
// Returns 0 in AL — retail's tail is `CALL 0x14005b450; XOR AL,AL; RET`.
extern "C" char TradeCtrl_State2_Enter(void* selfV, void* /*sm*/) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    if (!s_FindWorkPlace || !s_FindSched || !s_HasWorkSlot) return 0;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    int64_t wpc[2] = {0, 0};
    s_FindWorkPlace(*reinterpret_cast<int64_t*>(comp + 0x10), wpc);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
    int64_t schedBuf[2] = {0, 0};
    // The finder RETURNS its out-param; use the returned pointer, never the local.
    // See reference_outparam_finder_recycled_ibp.
    int64_t* sched = static_cast<int64_t*>(
        s_FindSched(*reinterpret_cast<int64_t*>(comp + 0x10), schedBuf));
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(sched);

    if (s_HasWorkSlot(*sched)) {          // already holds work — nothing to post
        TradeCtrlReport(comp, false, nullptr, true);
        TradeComponent_ArmMerchantTimer(selfV);
        return 0;
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(wpc);
    if (*reinterpret_cast<char*>(wpc[0] + 0x50) != 0) {   // workplace accepting
        void* work = nullptr;
        if (fn_MemAllocStatic) {
            void* mem = fn_MemAllocStatic(0x58, 8);
            if (mem) {
                int64_t entityIBP = *reinterpret_cast<int64_t*>(comp + 0x10);
                work = TradeWork_CtorFromEntity(mem, &entityIBP);
            }
        }
        uint64_t workIBP[2] = {0, 0};
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(workIBP, work);

        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(comp + 0x10));
        int64_t schedBuf2[2] = {0, 0};
        int64_t* sched2 = static_cast<int64_t*>(
            s_FindSched(*reinterpret_cast<int64_t*>(comp + 0x10), schedBuf2));
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(sched2);
        if (s_PostWork) s_PostWork(*sched2, 1, '\0', workIBP);

        // OUTCOME, not intent: report whether a TradeWork was really constructed and
        // whether the scheduler is holding one AFTER the post. "accepting was set" and
        // "a work object exists" are different claims, and only the readback proves the
        // second. See reference_diagnostics_must_report_outcomes.
        TradeCtrlReport(comp, true, work, s_HasWorkSlot(*sched2) != 0);
    } else {
        TradeCtrlReport(comp, false, nullptr, false);
    }

    TradeComponent_ArmMerchantTimer(selfV);
    return 0;
}

// ============================================================
// state 0x12 ENTER — 0x14005aa50
// ============================================================
// Retail is literally `MOV AL,0x1; RET` (two bytes, which is why Ghidra never made it
// a function and c650 refers to it as LAB_14005aa50). Nothing to do on entry to
// "merchant inbound" — the work happens in the events.
extern "C" char TradeCtrl_State12_Enter(void* /*self*/, void* /*sm*/) { return 1; }

// ============================================================
// state 0x13 trans38 (per frame) — 0x14005a780
// ============================================================
// Counts the merchant's stay down. When it expires and the post is not mid-trade
// (comp+0x71), request state 2. comp+0x71 is what stops a merchant vanishing out
// from under an open checkout.
extern "C" char TradeCtrl_State13_Tick(void* selfV, void* sm, void* deltaTime) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    const int64_t dt = deltaTime ? *reinterpret_cast<int64_t*>(deltaTime) : 0;
    *reinterpret_cast<int64_t*>(comp + 0x78) -= dt;
    if (*reinterpret_cast<int64_t*>(comp + 0x78) < 1 &&
        *reinterpret_cast<char*>(comp + 0x71) == '\0') {
        ::ControllerLive::RequestStateTransition(sm, 2);
    }
    return 1;
}

// ============================================================
// state 0x12 event 0xb — 0x14005a9c0
// ============================================================
// The inbound boat went away without docking: clear the merchant reference at
// comp+0x80 and go back to idle.
extern "C" char TradeCtrl_Event0B_MerchantLost(void* selfV, void* sm) {
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    uint64_t nullIBP[2] = {0, 0};
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(nullIBP, nullptr);
    *reinterpret_cast<uint64_t*>(comp + 0x80) = nullIBP[0];
    ::ControllerLive::RequestStateTransition(sm, 2);
    return 1;
}

// ============================================================
// Table build — replicates 0x14005c650
// ============================================================
// Idempotent: InstallLive runs once per component instance.
extern "C" void TradeCtrl_PopulateTable() {
    ::ControllerLive::StateTable& t = TradeDispatch::table;
    if (t.count != 0) return;
    using namespace ::ControllerLive;

    // state 2 — idle / waiting for the next merchant
    TableRegisterState(t, 2,
        reinterpret_cast<void*>(&TradeCtrl_State2_Enter),   // +0x08 enter  (abd0)
        nullptr,                                            // +0x10 trans30 (retail: 0)
        nullptr,                                            // +0x18 trans38 (aa60 — NOT PORTED)
        nullptr);                                           // +0x20 update  (retail: 0)
    TableAddTransition(t, 3, nullptr);                      // b160 — NOT PORTED
    TableAddTransition(t, 0x12, nullptr);

    // state 0x12 — merchant inbound
    TableRegisterState(t, 0x12,
        reinterpret_cast<void*>(&TradeCtrl_State12_Enter),  // aa50
        nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, nullptr);                      // b160 — NOT PORTED
    TableAddTransition(t, 0x13, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 9, nullptr);                           // a850 — NOT PORTED
    TableAddEvent(t, 0xb, reinterpret_cast<void*>(&TradeCtrl_Event0B_MerchantLost));

    // state 0x13 — merchant docked
    TableRegisterState(t, 0x13,
        nullptr,                                            // a800 — NOT PORTED
        nullptr,
        reinterpret_cast<void*>(&TradeCtrl_State13_Tick),   // a780
        nullptr);                                           // a6d0 — NOT PORTED
    TableAddTransition(t, 3, nullptr);                      // b160 — NOT PORTED
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 0xa, nullptr);                         // b240 — NOT PORTED

    std::fprintf(stderr, "[TradeCtrl] table built: %d states (2, 0x12, 0x13)\n", t.count);
    std::fflush(stderr);
}
