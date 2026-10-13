// src/game/Component/sm_memo.h -- SMMEMO: the exact no-op skip of the state machine's
// slot-8 tick (StateMachineComponent::Tick 0x14012b640 = StateMachine::Tick(t, 0, 1), from
// the update-tree walk). docs/extensions/handins/SMMEMO.md is the proof.
//
// Faithful perf work, not an extension: no registry row, no settings key, no flag. With no
// pending state, the slot-8 tick only runs StateMachine::Tick's run phase: from `current`
// up to the root, node slot 6 (the state table's trans30 column, RootController's 0) at the
// state's index, FindState(currentInput) on each parent. Only BoatComponent's table has a
// trans30 handler; for every other machine every level finds a null handler, returns 0,
// and nothing is written. Which handlers that walk reaches is a function of the four state
// bytes and the controller chain (the chain is linked once, `current` is a function of the
// chain and currentInput, the tables are static), so:
//
//   the MEMO is the machine's state word (currentInput, activeState, pendingState,
//   transitionState as one little-endian dword) as it stood at the END of a full slot-8
//   tick whose run phase called no trans30 handler, stored inverted (see kNone). It lives
//   in the StateMachine's tail padding (component +0x54, never written by retail and never
//   serialized).
//   A tick whose live word equals the memo is skipped: the full tick would walk the same
//   nodes with the same indices, reach the same null handlers and write nothing.
//
// The memo can only decline on any change of the four bytes (a request sets pendingState,
// so the transition still runs on this tick, as in retail). Header-only and free of game
// types so tests/unit/sm_memo_test.cpp checks the same code the tick runs.
#pragma once
#include <cstdint>

namespace SmMemo {

// The kill switch (like StillPath::kEnabled): false = every slot-8 tick runs in full, the
// memo is never written, the counters stay 0. Shipped value = shipped code.
static constexpr bool kEnabled = true;

// Non-null trans30 handlers called (ControllerLive::Dispatcher::Trans30's handler branch,
// controller_state_table.h; only BoatComponent registers one). A slot-8 tick that leaves
// it unchanged called none. Main thread only, wraps harmlessly (compared for equality).
inline uint32_t g_trans30Calls = 0;

// Slot-8 ticks skipped (or, under BANISHED_STRESS_COMPPROF=4, the ticks the SmTickNoop
// shadow ran in full instead). Read by the [COMPPROF] /sm row (stress_harness.cpp).
inline long long g_skipped = 0;

// The four signed state bytes, in their memory order (StateMachine +0x08..+0x0B), as one
// dword.
inline uint32_t StateWord(int8_t currentInput, int8_t activeState, int8_t pendingState,
                          int8_t transitionState) {
    return static_cast<uint32_t>(static_cast<uint8_t>(currentInput)) |
           static_cast<uint32_t>(static_cast<uint8_t>(activeState)) << 8 |
           static_cast<uint32_t>(static_cast<uint8_t>(pendingState)) << 16 |
           static_cast<uint32_t>(static_cast<uint8_t>(transitionState)) << 24;
}

inline bool PendingOf(uint32_t word) { return static_cast<int8_t>(static_cast<uint8_t>(word >> 16)) >= 0; }

// The memo is stored INVERTED (~word), so that the 0 a new or cleared memo holds (kNone)
// matches only the word 0xffffffff: input, active, pending and transition all -1, on which
// the full tick is a no-op anyway (nothing pending: no transition phase; input < 0: no run
// phase). Stored plainly, 0 would match the real word (0, 0, 0, 0), a PENDING state 0.
static constexpr uint32_t kNone = 0u;

// The memo a full slot-8 tick leaves: its final word (inverted) when its run phase called no
// trans30 handler (calls unchanged) and nothing is pending (StateMachine::Tick only returns
// with a negative pendingState; the test keeps a memo from ever covering a pending state),
// else kNone.
inline uint32_t Record(uint32_t callsBefore, uint32_t callsAfter, uint32_t wordAfter) {
    return (callsBefore == callsAfter && !PendingOf(wordAfter)) ? ~wordAfter : kNone;
}

// The skip test: one compare (and a NOT). Matches only a word Record stored, or the all -1
// no-op word.
inline bool Hit(uint32_t word, uint32_t memo) { return ~word == memo; }

// SmTickNoop shadow (COMPPROF=4): what a skipped tick claimed, checked after the full tick
// ran. Bit 0 a trans30 handler was called, bit 1 the state word changed, bit 2 `current`
// moved. 0 = the claim held.
inline long long ShadowDiff(uint32_t callsBefore, uint32_t callsAfter, uint32_t wordBefore,
                            uint32_t wordAfter, const void* currentBefore, const void* currentAfter) {
    long long d = 0;
    if (callsBefore != callsAfter) d |= 1;
    if (wordBefore != wordAfter) d |= 2;
    if (currentBefore != currentAfter) d |= 4;
    return d;
}

// The [COMPPROF] /sm row's window (BANISHED_STRESS_COMPPROF=2): the StateMachineComponent
// slot-8 ticks the profiler bracketed, split into skipped and full. Ticks are the
// harness's RDTSC ticks.
struct ProfWindow {
    long long calls = 0, ticks = 0;          // every StateMachineComponent slot-8 tick
    long long skips = 0, skipTicks = 0;      // of which skipped
    void Reset() { *this = ProfWindow(); }
    void Add(long long t, bool skipped) {
        ++calls; ticks += t;
        if (skipped) { ++skips; skipTicks += t; }
    }
    long long Full() const { return calls - skips; }
    long long FullTicks() const { return ticks - skipTicks; }
    // The indicative saving: skipped ticks x (mean full tick - mean skipped tick). The full
    // population carries the transitions and Boat's handler ticks, so it overstates.
    double SavedTicks() const {
        if (skips <= 0 || Full() <= 0) return 0.0;
        const double perFull = (double)FullTicks() / (double)Full();
        const double perSkip = (double)skipTicks / (double)skips;
        return (perFull - perSkip) * (double)skips;
    }
};

} // namespace SmMemo
