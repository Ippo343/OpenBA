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
// Enabled whenever the lane is enabled (BANISHED_LIVE_FIRE); set
// BANISHED_FIRE_TRACE=0 to silence it while keeping the mechanic live.

#include <cstdio>
#include <cstdlib>

namespace FireTrace {

inline bool On() {
    static int v = -1;
    if (v < 0) {
        const char* e = std::getenv("BANISHED_FIRE_TRACE");
        v = (e && e[0] && e[0] != '0') ? 1 : 0;
        if (!e) {                       // unset => follow the lane gate
            const char* g = std::getenv("BANISHED_LIVE_FIRE");
            v = (g && g[0] && g[0] != '0') ? 1 : 0;
        }
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
