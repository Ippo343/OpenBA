#pragma once
// ============================================================================
// runtime/funcs/rt_diff.h — differential oracle for the Runtime port
// ============================================================================
//
// The exe port never had an in-process oracle: the original exe was not in the
// process, so "is our port right?" could only be answered by playing the game
// and watching for something to go wrong. The runtime port is different.
// Runtime-x64.dll stays loaded until cutover (PLAN.md §4.1 decision 4), so for
// any function without side effects we can run BOTH implementations on the same
// inputs and compare the results, on every call, in a normal play session.
//
// USAGE (a pure leaf — the Wave 1 shape):
//
//   float Math_Vector3_Length(const Vector3* v) {
//       RT_DIFF_BEGIN("Math::Vector3::Length");
//       float ours = /* our reimplementation */;
//       RT_DIFF_SCALAR(ours, g_orig_Vector3_Length(v));
//       return ours;
//   }
//
// With BANISHED_RT_DIFF unset (the default, and every shipping run) the macro
// expands to nothing but the call to ours: zero cost. With BANISHED_RT_DIFF=1
// it also calls the original and reports the first divergence per site, with a
// stack trace, then keeps counting silently.
//
// WHAT IT CAN AND CANNOT CHECK
//
//   * Pure functions — full oracle. Math, most of System, codecs, hashes.
//   * Functions that write through an out-parameter — use RT_DIFF_BYTES on a
//     scratch copy of the output buffer; run ours into a copy, theirs into
//     another, and compare. Never let the original write into live state.
//   * Anything that mutates shared state (allocator, factory, filesystem, UI)
//     — NOT diffable this way; running both mutates twice. For those use
//     "shadow" mode: ours computes into a private replica, theirs executes for
//     real, and the port asserts its replica matches (PLAN.md §4.3 risk 2).
//     RT_DIFF_SHADOW is the marker for that; it does not call the original.
//
// Every reported divergence names the function, the call ordinal and the two
// values. `RtDiffSummary()` prints per-site totals at exit, so a run that
// diverges once in ten thousand calls is still visible.
// ============================================================================

#include <cstdint>
#include <cstddef>

// True when BANISHED_RT_DIFF is set to a non-"0" value. Cached after the first
// call; getenv is process-cached anyway (standalone/env_cache.h).
bool RtDiffEnabled();

// One comparison site. Static per call site; holds the counters.
struct RtDiffSite {
    const char* name;
    uint64_t    calls;
    uint64_t    divergences;
    bool        reported;   // first divergence has been logged with a stack
    bool        linked;     // present in the summary list
};

// Register a site (idempotent) so it appears in the summary even with 0 calls.
void RtDiffRegister(RtDiffSite* site);

// Report one comparison. `equal` is the caller's verdict; the two `desc`
// strings are pre-formatted renderings of ours/theirs for the log.
void RtDiffReport(RtDiffSite* site, bool equal, const char* oursDesc, const char* theirsDesc);

// Byte comparison helper: formats a hex diff of the first mismatching bytes.
void RtDiffReportBytes(RtDiffSite* site, const void* ours, const void* theirs, size_t n);

// Print per-site totals. Called at shutdown; safe to call any time.
void RtDiffSummary();

// ---- Macros ---------------------------------------------------------------

#define RT_DIFF_BEGIN(NAME)                                                    \
    static RtDiffSite rt_diff_site__ = { NAME, 0, 0, false, false };           \
    const bool rt_diff_on__ = RtDiffEnabled();                                 \
    if (rt_diff_on__) RtDiffRegister(&rt_diff_site__)

// Scalar comparison. THEIRS is only evaluated when diffing is on, so the
// original is not called in a normal run.
#define RT_DIFF_SCALAR(OURS, THEIRS)                                           \
    do {                                                                       \
        if (rt_diff_on__) {                                                    \
            auto rt_diff_theirs__ = (THEIRS);                                  \
            char a__[64], b__[64];                                             \
            RtDiffFormat(a__, sizeof(a__), (OURS));                            \
            RtDiffFormat(b__, sizeof(b__), rt_diff_theirs__);                  \
            RtDiffReport(&rt_diff_site__, (OURS) == rt_diff_theirs__, a__, b__);\
        }                                                                      \
    } while (0)

// Float comparison with an explicit tolerance. Use 0.0f to demand bit equality
// — which is the default expectation: a byte-faithful port of a float routine
// should reproduce the original's exact rounding, and a tolerance here is an
// admission that it does not. State why in a comment when you pass one.
#define RT_DIFF_FLOAT(OURS, THEIRS, TOL)                                       \
    do {                                                                       \
        if (rt_diff_on__) {                                                    \
            float rt_diff_theirs__ = (THEIRS);                                 \
            float rt_diff_ours__   = (OURS);                                   \
            char a__[64], b__[64];                                             \
            RtDiffFormat(a__, sizeof(a__), rt_diff_ours__);                    \
            RtDiffFormat(b__, sizeof(b__), rt_diff_theirs__);                  \
            RtDiffReport(&rt_diff_site__,                                      \
                         RtDiffFloatEqual(rt_diff_ours__, rt_diff_theirs__, (TOL)), \
                         a__, b__);                                            \
        }                                                                      \
    } while (0)

// Buffer comparison. Both pointers must be private scratch copies; the
// original must never have written into live state to produce THEIRS.
#define RT_DIFF_BYTES(OURS_PTR, THEIRS_PTR, N)                                 \
    do {                                                                       \
        if (rt_diff_on__)                                                      \
            RtDiffReportBytes(&rt_diff_site__, (OURS_PTR), (THEIRS_PTR), (N)); \
    } while (0)

// Shadow mode marker for stateful ports: ours computed a replica, theirs did
// the real work, and EQUAL is the port's own verdict on whether the replica
// still matches. No call into the original happens here.
#define RT_DIFF_SHADOW(EQUAL, OURS_DESC, THEIRS_DESC)                          \
    do {                                                                       \
        if (rt_diff_on__)                                                      \
            RtDiffReport(&rt_diff_site__, (EQUAL), (OURS_DESC), (THEIRS_DESC));\
    } while (0)

// ---- Formatting overloads used by the macros ------------------------------

void RtDiffFormat(char* buf, size_t n, int          v);
void RtDiffFormat(char* buf, size_t n, unsigned int v);
void RtDiffFormat(char* buf, size_t n, long long    v);
void RtDiffFormat(char* buf, size_t n, unsigned long long v);
void RtDiffFormat(char* buf, size_t n, float        v);
void RtDiffFormat(char* buf, size_t n, double       v);
void RtDiffFormat(char* buf, size_t n, bool         v);
void RtDiffFormat(char* buf, size_t n, const void*  v);

// Bit-exact when tol == 0 (NaN==NaN counts as equal: a faithful port should
// reproduce a NaN where the original produced one).
bool RtDiffFloatEqual(float ours, float theirs, float tol);
