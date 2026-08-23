#pragma once
// code/Game/Component/fire_trace.h
// Shared low-volume `[Fire]` trace for the fire disaster lane.
//
// Policy (reference_diagnostics_must_report_outcomes): every line reports an
// OUTCOME, not an intent — the roll that fired AND the roll that did not, the
// spread attempt that found a neighbour AND the one that found none, and which of
// Extinguish's two conditions actually tripped. Lines are budgeted so a long soak
// does not flood the log; the per-tick ignition roll is the only high-frequency
// site and it is sampled, not printed every tick.
//
// OPT-IN: set BANISHED_FIRE_TRACE=1 to turn it on.
//
// It used to default to "follow the lane gate" by reading BANISHED_LIVE_FIRE with a RAW
// getenv. That was fine while the lane was opt-in and is a lie now that the lane is in
// kDefaultOnFlags: the var is normally unset, so the raw read sees nothing and reports
// the lane as off while it is in fact running (the exact trap recorded in
// reference_harness_env_gotchas / the trade-checkout gate). A graduated mechanic should
// not flood a normal run's log either, so the fallback is now simply OFF.

#include <cstdio>
#include <cstdlib>

namespace FireTrace {

inline bool On() {
    static int v = -1;
    if (v < 0) {
        const char* e = std::getenv("BANISHED_FIRE_TRACE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return v == 1;
}

// Budgeted print: at most `cap` lines for this call site over the whole run.
#define FIRE_TRACE_N(cap, ...)                                                  \
    do {                                                                        \
        if (::FireTrace::On()) {                                                \
            static int s_fireTraceN = 0;                                        \
            if (s_fireTraceN < (cap)) {                                         \
                ++s_fireTraceN;                                                 \
                std::fprintf(stderr, __VA_ARGS__);                              \
                std::fflush(stderr);                                            \
            }                                                                   \
        }                                                                       \
    } while (0)

// Unbudgeted (use only for events that are rare by construction: ignite,
// extinguish, a bucket landing).
#define FIRE_TRACE(...)                                                         \
    do {                                                                        \
        if (::FireTrace::On()) {                                                \
            std::fprintf(stderr, __VA_ARGS__);                                  \
            std::fflush(stderr);                                                \
        }                                                                       \
    } while (0)

}  // namespace FireTrace
