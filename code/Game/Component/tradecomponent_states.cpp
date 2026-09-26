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
//     enter   a800  build the merchant's manifest, arm the stay timer, auto-trade
//     trans38 a780  per-frame: stay timer expired and not mid-trade -> 2
//     update  a6d0  leaving: auto-trade, release the boat, clear the manifest
//                   (both in tradecomponent_dock.cpp — layer 3)
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
//   0x14005b240  dismiss event — retail inlines the whole of StateMachine::Tick here.
//   (0x14005a850, the dock event-log line, WAS in this list — ported 2026-08-22 once the
//    boat state machine gave it a caller; it reuses eventlog.cpp's AddEvent.)
//
// So with this layer alone the machine runs, sits in state 2, and re-posts trade work
// on its own; it does not yet advance to 0x12/0x13. That is a real behaviour change
// (today NOTHING posts trade work except the player clicking the toggle) and a
// deliberately visible stopping point, not a silent partial.
//
// GATED: BANISHED_LIVE_TRADE_CTRL. GRADUATED 2026-08-22 into the default-on table, so it is
// now a KILL-SWITCH (=0), not an opt-in. Proven end to end: timer -> merchant spawn ->
// route -> dock -> manifest -> auto-trade delivering goods.

#include "Component/controller_state_table.h"
#include "Component/TradeComponent.h"
#include "engine_funcs.h"
#include "resolve.h"
#include "Object/factory_registration.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "Work/tradework_funcs.h"
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// Layer 2 lives in boatcomponent_route.cpp (retail 0x14000eb20 / 0x1400456e0).
extern "C" void* BoatRoute_FindBoatComponent(int64_t entity, void* outIBP);
extern "C" void  BoatComponent_SetDestination(int64_t boatComp, int64_t* destEntityIBP);

// Layer 3 lives in tradecomponent_dock.cpp (retail 0x14005a800 / 0x14005a6d0).
extern "C" char TradeCtrl_State13_Enter(void* self, void* sm);    // 0x14005a800
extern "C" char TradeCtrl_State13_Update(void* self, void* sm);   // 0x14005a6d0

// TradeCtrlTag lives in TradeComponent.h (the member type needs it).
using TradeDispatch = ::ControllerLive::Dispatcher<TradeCtrlTag>;

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
typedef void*    (*fn_EntityCopyCtor_t)(void* mem, void* templateEntity);
typedef void     (*fn_EntityListAdd_t)(void* entityList, void* entityIBP, int category);
typedef void     (*fn_StateMachineTick_t)(void* sm, void* time, char a, char b);
typedef void     (*fn_ListErase_t)(void* listBase, void* valuePtr);

fn_Find_t         s_FindWorkPlace = nullptr;   // 0x140002e10
fn_Find_t         s_FindSched     = nullptr;   // 0x140003050
fn_HasWorkSlot_t  s_HasWorkSlot   = nullptr;   // 0x14012ead0
fn_WorkCtor25_t   s_WorkCtor25    = nullptr;   // 0x1400f72e0
fn_PostWork_t     s_PostWork      = nullptr;   // 0x14012ec60
fn_TimeCtor_t     s_TimeCtor      = nullptr;   // dispatch slot [0x140152b00]
// Layer 2 (merchant spawn / teardown) leaves.
fn_EntityCopyCtor_t   s_EntityCopyCtor   = nullptr;   // 0x1400c3720
fn_EntityListAdd_t    s_EntityListAdd    = nullptr;   // 0x1400c48a0
fn_StateMachineTick_t s_StateMachineTick = nullptr;   // 0x1400a9660
fn_ListErase_t        s_ListErase        = nullptr;   // 0x140002cc0
bool              s_resolved      = false;

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    s_FindWorkPlace = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140002e10));
    s_FindSched     = reinterpret_cast<fn_Find_t>(ResolveAddr(Module::Exe, 0x140003050));
    s_HasWorkSlot   = reinterpret_cast<fn_HasWorkSlot_t>(ResolveAddr(Module::Exe, 0x14012ead0));
    s_WorkCtor25    = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_PostWork      = reinterpret_cast<fn_PostWork_t>(ResolveAddr(Module::Exe, 0x14012ec60));
    s_EntityCopyCtor   = reinterpret_cast<fn_EntityCopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c3720));
    s_EntityListAdd    = reinterpret_cast<fn_EntityListAdd_t>(ResolveAddr(Module::Exe, 0x1400c48a0));
    s_StateMachineTick = reinterpret_cast<fn_StateMachineTick_t>(ResolveAddr(Module::Exe, 0x1400a9660));
    s_ListErase        = reinterpret_cast<fn_ListErase_t>(ResolveAddr(Module::Exe, 0x140002cc0));
    if (uintptr_t slot = ResolveAddr(Module::Exe, 0x140152b00))
        s_TimeCtor = *reinterpret_cast<fn_TimeCtor_t*>(slot);

    // Report the OUTCOME of every dependency. A single null here turns the whole
    // state machine into a machine that runs and does nothing — believable and wrong.
    std::fprintf(stderr,
        "[TradeCtrl] deps: findWP=%p findSched=%p hasWork=%p workCtor=%p post=%p "
        "timeCtor=%p entityCopy=%p entityAdd=%p smTick=%p erase=%p\n",
        reinterpret_cast<void*>(s_FindWorkPlace), reinterpret_cast<void*>(s_FindSched),
        reinterpret_cast<void*>(s_HasWorkSlot),   reinterpret_cast<void*>(s_WorkCtor25),
        reinterpret_cast<void*>(s_PostWork),      reinterpret_cast<void*>(s_TimeCtor),
        reinterpret_cast<void*>(s_EntityCopyCtor), reinterpret_cast<void*>(s_EntityListAdd),
        reinterpret_cast<void*>(s_StateMachineTick), reinterpret_cast<void*>(s_ListErase));
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
    float seconds = (tol * *reinterpret_cast<float*>(rec + 8) +
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
//
// ENDGAME B.3.8 MI-B: the object is now the REAL TradeWork (code/Game/Work/TradeWork.h):
// its default ctor is the base init + `+0x48 = +0x50 = 0` + TradeWork_ResolveFuncs, the
// compiler stamps TradeWork's own vtable, and Work::InitFromEntity is the body of the
// entity-seeded base ctor 0x1400f72e0 (Work_Ctor25) — the same bytes the hand build
// wrote, with the compiler's table in place of the 0x140161a10 read-back. The extern "C"
// name stays: standalone/reg_trade_ctrl.cpp registers it at 0x1400f6ea0.
extern "C" void* TradeWork_CtorFromEntity(void* mem, void* entityIBP) {
    if (!mem) return nullptr;
    uint64_t ibp[2] = {*reinterpret_cast<uint64_t*>(entityIBP), 0};
    TradeWork* work = new (mem) TradeWork();
    work->InitFromEntity(ibp);
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

    int64_t wpc[2] = {0, 0};
    s_FindWorkPlace(*reinterpret_cast<int64_t*>(comp + 0x10), wpc);

    int64_t schedBuf[2] = {0, 0};
    // The finder RETURNS its out-param; use the returned pointer, never the local.
    // See reference_outparam_finder_recycled_ibp.
    int64_t* sched = static_cast<int64_t*>(
        s_FindSched(*reinterpret_cast<int64_t*>(comp + 0x10), schedBuf));

    if (s_HasWorkSlot(*sched)) {          // already holds work — nothing to post
        TradeCtrlReport(comp, false, nullptr, true);
        TradeComponent_ArmMerchantTimer(selfV);
        return 0;
    }

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
        if (fn_InternalBasePtr_Ctor) new (workIBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(work));   // B.3.3 bridge

        int64_t schedBuf2[2] = {0, 0};
        int64_t* sched2 = static_cast<int64_t*>(
            s_FindSched(*reinterpret_cast<int64_t*>(comp + 0x10), schedBuf2));
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
// state 0x12 event 9 — 0x14005a850   THE MERCHANT HAS DOCKED
// ============================================================
// Raised by the BOAT, not by anything on this side: BoatComponent's state-2 enter
// (0x140044f70, boatcomponent_states.cpp) posts event 9 to this component's
// StateMachineComponent the moment the hull is within 0.2 of the pier. State 0x12 has no
// per-frame handler at all, so until something raised this event the post could never
// leave "merchant inbound" and the whole of layer 3 was unreachable.
//
// Retail INLINES the event-log writer here rather than calling it, but the entry it
// builds is field-for-field the 4-arg AddEvent overload (0x140095580) that eventlog.cpp
// already ports, with eventType 0xd and severity 2 — checked, not assumed:
//     color   0xff8080ff == EventLog_SeverityColor(2)
//     +0x10   0xd        == eventType
//     +0x14   2          == severity
//     +0x18   tileIdx    == EventLog_TileIndex(&entityIBP)   (comp+0x10)
//     +0x1c   5          == EventLog_IconIndex(0xd)
//     +0x20   -1         == EventLog_PopupIndex(0xd)
// so reusing AddEvent reproduces the inlined body exactly instead of duplicating it.
//
// The event log lives at gameWorld+0x138 (g_pGameWorldSingleton IS the GameUpdateLoop).
extern "C" void EventLog_AddEvent(void* eventLog, int eventType, int severity, void* entityIBP);

extern "C" char TradeCtrl_Event09_MerchantDocked(void* selfV, void* sm) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton
                                                                 : nullptr;
    if (gw) {
        void* eventLog = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0x138);
        // Retail copies the owner IBP to a local and passes its ADDRESS (the tile-index
        // helper takes an InternalBasePtr*, not the entity).
        int64_t entityIBP = *reinterpret_cast<int64_t*>(comp + 0x10);
        EventLog_AddEvent(eventLog, 0xd, 2, &entityIBP);
    }

    ::ControllerLive::RequestStateTransition(sm, 0x13);
    std::fprintf(stderr, "[TradeCtrl] event 9: MERCHANT DOCKED comp=%p -> state 0x13\n",
                 (void*)comp);
    std::fflush(stderr);
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
    if (fn_InternalBasePtr_Ctor) new (nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    *reinterpret_cast<uint64_t*>(comp + 0x80) = nullIBP[0];
    ::ControllerLive::RequestStateTransition(sm, 2);
    return 1;
}

// ============================================================
// state 0x13 event 0xa — 0x14005b240   PLAYER DISMISSED THE MERCHANT
// ============================================================
// Retail inlines the whole of StateMachine::Tick after the request, which is why
// the body is ~90 decompiled lines for what is a two-step handler. Read against
// our own StateMachine_TickReimpl (statemachinecomponent.cpp:455) the inlined
// loop matches EXACTLY, with time = System::Time(0.0), param3 = 0 (it dispatches
// node slot +0x30, not +0x38) and param4 = 1 (the `bVar2 = true` that is cleared
// after one pass). So retail 0x14005b240 is, in full:
//
//   local = 2;                                   // the desired state
//   if (sm[+9] < 0) StateMachine_Tick(sm, Time(0), 0, 0);   // drain a pending enter
//   i = node->slot08(node, sm[+9], &local);
//   if (i >= 0) { sm[+0xb] = i; sm[+0xa] = local; }         // == RequestStateTransition(sm, 2)
//   StateMachine_Tick(sm, Time(0), 0, 1);                   // run it NOW, not next frame
//   return 1;
//
// RequestStateTransition (controller_state_table.h:317) already replicates the
// first four lines including the nested drain tick and the node re-read, so the
// only thing this handler adds over its 0xb sibling is the immediate tick — which
// is the point: dismissing a merchant has to take effect on the click, not on the
// next simulation frame.
extern "C" char TradeCtrl_Event0A_Dismissed(void* selfV, void* smV) {
    (void)selfV;   // retail ignores param_1 entirely
    if (!smV) return 1;
    ::ControllerLive::RequestStateTransition(smV, 2);
    ::StateMachine_TickReimpl(smV, /*time*/ 0, /*param3*/ 0, /*param4*/ 1);
    return 1;
}

// ============================================================
// state 2 trans38 (per frame) - 0x14005aa60   THE MERCHANT SPAWN
// ============================================================
// Counts the merchant timer at comp+0x78 down; when it reaches zero it CLONES the
// TraderBoat entity out of the TradeDescription, hands the clone to the route builder,
// registers it with the world entity list and asks for state 0x12 (inbound).
//
// This is the handler layer 1 left as a null: it was blocked on the route builder
// (0x1400456e0), because a boat that is created but never routed is worse than no boat.
// That builder is now ported in boatcomponent_route.cpp.
//
// RETURN VALUE: retail always returns a value whose LOW BYTE is zero - the not-yet
// branch returns the delta-time with the low byte masked off, and the fired branch masks
// the transition result the same way. So: 0, on every path.
extern "C" char TradeCtrl_State2_Tick(void* selfV, void* smV, void* deltaTime) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);
    const int64_t dt = deltaTime ? *reinterpret_cast<int64_t*>(deltaTime) : 0;
    *reinterpret_cast<int64_t*>(comp + 0x78) -= dt;
    if (*reinterpret_cast<int64_t*>(comp + 0x78) >= 1) return 0;

    void* desc = TradeDesc(comp);

    // Clone the merchant entity from the description's template (desc+0x30).
    void* boat = nullptr;
    void* mem = fn_MemAllocStatic ? fn_MemAllocStatic(0x48, 8) : nullptr;
    if (mem && s_EntityCopyCtor)
        boat = s_EntityCopyCtor(mem, *reinterpret_cast<void**>(
                                         reinterpret_cast<uintptr_t>(desc) + 0x30));

    uint64_t boatIBP[2] = {0, 0};
    if (fn_InternalBasePtr_Ctor) new (boatIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(boat));   // B.3.3 bridge
    *reinterpret_cast<uint64_t*>(comp + 0x80) = boatIBP[0];

    // Route it to THIS post. The destination is the trading post's own entity (comp+0x10).
    int64_t destIBP = *reinterpret_cast<int64_t*>(comp + 0x10);
    uint64_t boatCompIBP[2] = {0, 0};
    void* boatComp = BoatRoute_FindBoatComponent(*reinterpret_cast<int64_t*>(comp + 0x80),
                                                 boatCompIBP);
    // eb20 returns its OUT-PARAM, so `boatComp` is always non-null; the component is the
    // value inside it. Retail derefs that value unguarded because a merchant template
    // always carries a BoatComponent.
    //
    // The guard below is a STANDALONE-ONLY difference, not a softened retail deref: the
    // BoatComponent type-id slot (0x1401c4020) is seeded behind BANISHED_LIVE_TRADE_TIDS,
    // and with that gate off the finder matches nothing and hands back 0. Crashing on a
    // gate the player can turn off is not a faithful reproduction of anything, so say
    // which gate is missing instead. If this fires with TRADE_TIDS on, the merchant
    // template genuinely lacks the component and THAT is the bug.
    const int64_t boatCompPtr = *reinterpret_cast<int64_t*>(boatComp);
    if (boatCompPtr) {
        BoatComponent_SetDestination(boatCompPtr, &destIBP);
    } else {
        std::fprintf(stderr, "[TradeCtrl] merchant has NO BoatComponent (type-id slot "
                             "0x1401c4020 unseeded? needs BANISHED_LIVE_TRADE_TIDS) — "
                             "boat spawned with no route\n");
        std::fflush(stderr);
    }

    // Publish the entity. The category is the template's own (template+0x20).
    int64_t regIBP = *reinterpret_cast<int64_t*>(comp + 0x80);
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (gw && s_EntityListAdd && desc) {
        void* tmpl = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(desc) + 0x30);
        s_EntityListAdd(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gw) + 0xe8),
                        &regIBP,
                        *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(tmpl) + 0x20));
    }

    // OUTCOME, not intent: the route point count is the only thing that says the boat can
    // actually sail. "A boat was allocated" and "a boat has a route" are different claims.

    // Retail ticks the machine once first when it has no current state yet
    // (`if (*(char*)(sm+9) < 0)` at 14005ab6a), then requests 0x12.
    uint8_t* sm = reinterpret_cast<uint8_t*>(smV);
    if (sm && static_cast<int8_t>(sm[9]) < 0 && s_StateMachineTick) {
        uint8_t zeroTime[8] = {};
        if (s_TimeCtor) s_TimeCtor(zeroTime, 0.0f);
        s_StateMachineTick(smV, zeroTime, 0, 0);
    }
    ::ControllerLive::RequestStateTransition(smV, 0x12);
    return 0;
}

// ============================================================
// shared transition 3 - 0x14005b160   TEARDOWN
// ============================================================
// Carried by every state. Unregisters the post from the world's trading-post list, and if
// a merchant is present hands the boat back (a NULL destination is what tells the route
// builder to release it), frees the post's item array and clears the docked merchant
// index.
//
// comp+0x50 / +0x58 here are the TradeComponent's ITEM array base and count - NOT the
// BoatComponent's route head, which occupies the same offsets on a different object. The
// two are easy to confuse when reading b160 and 56e0 side by side.
extern "C" char TradeCtrl_Teardown(void* selfV) {
    Resolve();
    const uintptr_t comp = reinterpret_cast<uintptr_t>(selfV);

    int64_t entityIBP = *reinterpret_cast<int64_t*>(comp + 0x10);
    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (gw && s_ListErase) {
        s_ListErase(reinterpret_cast<void*>(
                        *reinterpret_cast<uintptr_t*>(
                            reinterpret_cast<uintptr_t>(gw) + 0xf0) + 0x30),
                    &entityIBP);
    }

    if (*reinterpret_cast<int64_t*>(comp + 0x80) != 0) {
        int64_t nullDest = 0;
        if (fn_InternalBasePtr_Ctor) new (&nullDest) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        uint64_t boatCompIBP[2] = {0, 0};
        void* boatComp = BoatRoute_FindBoatComponent(*reinterpret_cast<int64_t*>(comp + 0x80),
                                                     boatCompIBP);
        const int64_t boatCompPtr = *reinterpret_cast<int64_t*>(boatComp);
        if (boatCompPtr) BoatComponent_SetDestination(boatCompPtr, &nullDest);

        if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(comp + 0x50));
        *reinterpret_cast<uint64_t*>(comp + 0x50) = 0;
        *reinterpret_cast<uint64_t*>(comp + 0x58) = 0;
        *reinterpret_cast<int32_t*>(comp + 0x2c) = -1;      // no merchant docked

        uint64_t clearIBP[2] = {0, 0};
        if (fn_InternalBasePtr_Ctor) new (clearIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<uint64_t*>(comp + 0x80) = clearIBP[0];
    }
    return 1;
}

// ============================================================
// Table build — replicates 0x14005c650
// ============================================================
// Built ONCE at static-init time, as retail's 0x14005c650 does (B38 D6/D7).
static void TradeCtrl_PopulateTable() {
    ::ControllerLive::StateTable& t = TradeDispatch::table;
    if (t.count != 0) return;
    using namespace ::ControllerLive;

    // state 2 — idle / waiting for the next merchant
    TableRegisterState(t, 2,
        reinterpret_cast<void*>(&TradeCtrl_State2_Enter),   // +0x08 enter  (abd0)
        nullptr,                                            // +0x10 trans30 (retail: 0)
        reinterpret_cast<void*>(&TradeCtrl_State2_Tick),    // +0x18 trans38 (aa60)
        nullptr);                                           // +0x20 update  (retail: 0)
    TableAddTransition(t, 3, reinterpret_cast<void*>(&TradeCtrl_Teardown));   // b160
    TableAddTransition(t, 0x12, nullptr);

    // state 0x12 — merchant inbound
    TableRegisterState(t, 0x12,
        reinterpret_cast<void*>(&TradeCtrl_State12_Enter),  // aa50
        nullptr, nullptr, nullptr);
    TableAddTransition(t, 3, reinterpret_cast<void*>(&TradeCtrl_Teardown));   // b160
    TableAddTransition(t, 0x13, nullptr);
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 9, reinterpret_cast<void*>(&TradeCtrl_Event09_MerchantDocked));  // a850
    TableAddEvent(t, 0xb, reinterpret_cast<void*>(&TradeCtrl_Event0B_MerchantLost));

    // state 0x13 — merchant docked
    TableRegisterState(t, 0x13,
        reinterpret_cast<void*>(&TradeCtrl_State13_Enter),  // a800  (layer 3)
        nullptr,                                            // retail: 0
        reinterpret_cast<void*>(&TradeCtrl_State13_Tick),   // a780
        reinterpret_cast<void*>(&TradeCtrl_State13_Update));// a6d0  (layer 3)
    TableAddTransition(t, 3, reinterpret_cast<void*>(&TradeCtrl_Teardown));   // b160
    TableAddTransition(t, 2, nullptr);
    TableAddEvent(t, 0xa, reinterpret_cast<void*>(&TradeCtrl_Event0A_Dismissed));  // 0x14005b240

    std::fprintf(stderr, "[TradeCtrl] table built: %d states (2, 0x12, 0x13)\n", t.count);
    std::fflush(stderr);
}

// Built ONCE at static-init time (B38 D6/D7). Before, TradeComponent_InstallController
// re-ran it from every factory call on both threads. The "[TradeCtrl] table built"
// line above now prints at static init, before standalone_error.log is opened.
static const bool s_tradeStateTableBuilt = (TradeCtrl_PopulateTable(), true);
