// ============================================================================
// runtime/funcs/rt_diff.cpp — differential oracle for the Runtime port
// ============================================================================
// See rt_diff.h for the contract and for what this can and cannot check.

#include "rt_diff.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>

// ---- Enable flag -----------------------------------------------------------

bool RtDiffEnabled() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_RT_DIFF");
        on = (e && *e && strcmp(e, "0") != 0 && e[0] != 'f' && e[0] != 'F') ? 1 : 0;
        if (on) {
            fprintf(stderr, "[RtDiff] ON — every diffed port also runs the original "
                            "and compares. Slower by design; do not benchmark this.\n");
            fflush(stderr);
        }
    }
    return on != 0;
}

// ---- Site list -------------------------------------------------------------
//
// A fixed array, not a std::vector: sites register from inside ported Runtime
// functions, which can run before or after any static constructor and (once the
// allocator is ours) inside allocation itself. Nothing here may allocate.

namespace {

constexpr size_t kMaxSites = 1024;
RtDiffSite*      g_sites[kMaxSites];
size_t           g_siteCount = 0;

// How many divergences to log in full per site before falling back to counting.
constexpr uint64_t kMaxVerboseReports = 3;

} // namespace

void RtDiffRegister(RtDiffSite* site) {
    if (!site || site->linked) return;
    if (g_siteCount < kMaxSites) {
        // Log the index -> address -> name pairing as each site appears. This is
        // what makes a later corruption diagnosable: RtDiffSite objects are
        // function-local statics scattered across the ported translation units,
        // so once one has been overwritten the summary can still name the INDEX
        // but has no safe way to recover the name from the wreckage. Recording
        // the pairing here, while the site is provably intact, turns "the
        // summary crashed" into "site N, which is <name>, was clobbered".
        fprintf(stderr, "[RtDiff] site[%zu] @%p = %s\n",
                g_siteCount, (const void*)site,
                site->name ? site->name : "(unnamed)");
        g_sites[g_siteCount++] = site;
        site->linked = true;
    }
}

// Cheap sanity test for a site that a stray write may have overwritten.
//
// A live site has a plausible name pointer and cannot have diverged more times
// than it was called. Anything else means the 32-byte static was scribbled on,
// and dereferencing its name is how the summary previously turned a memory bug
// into an ACCESS_VIOLATION that destroyed the entire report.
static bool RtDiffSiteLooksLive(const RtDiffSite* s) {
    if (!s || !s->name) return false;
    // Names are string literals in .rdata. A float or small integer written
    // over the field is not a plausible pointer; 0x10000 is below any image base.
    if (reinterpret_cast<uintptr_t>(s->name) < 0x10000u) return false;
    if (s->divergences > s->calls) return false;
    return true;
}

// ---- Reporting -------------------------------------------------------------

void RtDiffReport(RtDiffSite* site, bool equal, const char* oursDesc, const char* theirsDesc) {
    if (!site) return;
    site->calls++;
    if (equal) return;

    site->divergences++;
    if (site->divergences <= kMaxVerboseReports) {
        fprintf(stderr, "[RtDiff] DIVERGENCE %s  call #%llu  ours=%s  theirs=%s\n",
                site->name ? site->name : "(unnamed)",
                (unsigned long long)site->calls,
                oursDesc ? oursDesc : "?",
                theirsDesc ? theirsDesc : "?");
        fflush(stderr);
        site->reported = true;
    } else if (site->divergences == kMaxVerboseReports + 1) {
        fprintf(stderr, "[RtDiff] %s: further divergences counted only\n",
                site->name ? site->name : "(unnamed)");
        fflush(stderr);
    }
}

// ============================================================================
// Wave 9 lane M — the three instruments. See rt_diff.h for the contract and
// WAVE9_LANE_M.md Finding 1 for the defect that motivated each.
// ============================================================================

// ---- (1) The did-work counter ---------------------------------------------

void RtDiffNoteWork(RtDiffSite* site, bool didWork) {
    if (!site) return;
    site->workDeclared = true;
    if (didWork) site->worked++;
}

// ---- (2) BANISHED_RT_DIFF_INVERT ------------------------------------------

bool RtDiffInvertReplay() {
    static int on = -1;
    if (on < 0) {
        const char* e = getenv("BANISHED_RT_DIFF_INVERT");
        on = (e && *e && strcmp(e, "0") != 0 && e[0] != 'f' && e[0] != 'F') ? 1 : 0;
        if (on) {
            fprintf(stderr,
                "[RtDiff] INVERT ON - converted prefix-replay sites run the DLL's body FIRST.\n"
                "[RtDiff] INVERT: a CRASH under this flag is a finding about the HARNESS, not\n"
                "[RtDiff] INVERT: a port defect. The shipped order hides double-managed\n"
                "[RtDiff] INVERT: allocations; inverting only stops hiding them. The last\n"
                "[RtDiff] INVERT: '[RtDiff] INVERT arm:' line below names the guilty site.\n");
            fflush(stderr);
        }
    }
    return on != 0;
}

// Named BEFORE the original is called, and flushed, so a crash inside the
// replay identifies its own function. A crash with no name is a bisection job;
// this is the difference between a twenty-second answer and five runs.
void RtDiffInvertArm(RtDiffSite* site) {
    if (!site) return;
    fprintf(stderr, "[RtDiff] INVERT arm: %s (call #%llu)\n",
            site->name ? site->name : "(unnamed)",
            (unsigned long long)(site->calls + 1));
    fflush(stderr);
}

// ---- (3) The released-pointer detector -------------------------------------

static bool (*g_readable)(const void*, size_t) = nullptr;

void RtDiffSetReadableProbe(bool (*fn)(const void* p, size_t n)) { g_readable = fn; }

// A System::String's data pointer carries a {length, refcount} header in the
// two dwords at [-8] (measured: a live "4" reads len=1 rc=1; the block our body
// had just released read 0,0 in BOTH dwords). A LIVE string can never have
// refcount 0, so {0,0} at [-8] is a released block.
static bool RtDiffLooksReleased(const void* p) {
    if (!g_readable) return false;                       // probe not installed: skip
    const unsigned char* b = static_cast<const unsigned char*>(p);
    if (!g_readable(b - 8, 8)) return false;
    unsigned int len = 0, rc = 0;
    memcpy(&len, b - 8, 4);
    memcpy(&rc,  b - 4, 4);
    return len == 0 && rc == 0;
}

static bool RtDiffPlausibleHeapPtr(uint64_t v) {
    // 8-aligned, above the lowest user page, below the 47-bit user-space top,
    // and not a small integer wearing a pointer's clothes.
    if (v & 7u) return false;
    if (v < 0x10000ull) return false;
    if (v > 0x00007FFFFFFFFFFFull) return false;
    return true;
}

// Called from RtDiffReportBytes when the two buffers differ. Reports the case
// that matters and ONLY that case:
//
//   * the same 8-byte slot holds a different pointer on each side, AND
//   * the side the DLL's body ran on points at a RELEASED block, AND
//   * our side points at a live one.
//
// All three conditions together are Finding 1's exact signature and are very
// unlikely to arise any other way. Two of them alone are not enough: a slot
// that merely differs is an ordinary divergence, and a released block on BOTH
// sides is not evidence the replay caused it.
static void RtDiffScanForReleasedPointers(RtDiffSite* site, const void* ours,
                                          const void* theirs, size_t n,
                                          bool verbose = true) {
    if (!g_readable || n < 8) return;
    const unsigned char* a = static_cast<const unsigned char*>(ours);
    const unsigned char* b = static_cast<const unsigned char*>(theirs);
    for (size_t off = 0; off + 8 <= n; off += 8) {
        uint64_t va = 0, vb = 0;
        memcpy(&va, a + off, 8);
        memcpy(&vb, b + off, 8);
        if (va == vb) continue;
        if (!RtDiffPlausibleHeapPtr(va) || !RtDiffPlausibleHeapPtr(vb)) continue;
        const void* pa = reinterpret_cast<const void*>(static_cast<uintptr_t>(va));
        const void* pb = reinterpret_cast<const void*>(static_cast<uintptr_t>(vb));
        if (!RtDiffLooksReleased(pb) || RtDiffLooksReleased(pa)) continue;

        site->freedPtrHits++;
        if (verbose && site->freedPtrHits <= 3) {
            fprintf(stderr,
                "[RtDiff] *** HARNESS DEFECT (not a port defect) *** %s call #%llu\n"
                "[RtDiff]     +0x%zx: the DLL's body ran on a scratch still holding %p,\n"
                "[RtDiff]     which OUR body had already RELEASED (header at [-8] reads\n"
                "[RtDiff]     {len=0,rc=0}); ours holds the live %p. This site prefix-replays\n"
                "[RtDiff]     a body that swaps a REFCOUNTED pointer, which the oracle\n"
                "[RtDiff]     contract forbids. Make it a NO-ORACLE port -- do NOT exclude\n"
                "[RtDiff]     these bytes from the compare, which would hide the\n"
                "[RtDiff]     use-after-free while turning the report green.\n",
                site->name ? site->name : "(unnamed)",
                // +1: the scan runs BEFORE RtDiffReport increments `calls`, and
                // RtDiffReport prints the post-increment value. Without this the
                // two lines about the same call disagree by one, which is how a
                // reader loses ten minutes correlating them.
                (unsigned long long)site->calls + 1, off, pb, pa);
            fflush(stderr);
        }
    }
}

// ---- The detector's POSITIVE CONTROL --------------------------------------
//
// A detector nobody can prove is alive is worse than none: it converts silence
// into false reassurance, which is the failure mode this whole instrument was
// built to close. Its only natural positive control is
// `UI::NumberElement::SetValueInt` on tutorials 1-3 — and that control
// DISAPPEARS the moment lane N fixes it, leaving a detector with no way to
// distinguish "nothing is wrong" from "I stopped working".
//
// So the control is synthetic and runs every time the oracle is enabled: build
// one buffer pair that IS the defect (a slot holding a pointer whose {len,rc}
// header reads {0,0}, against a live one) and one that is NOT (both live), and
// require the detector to fire on the first and stay silent on the second.
// Both halves matter — a detector that fires on everything is equally useless.
static void RtDiffDetectorSelfTest() {
    if (!g_readable) {
        fprintf(stderr, "[RtDiff] released-pointer detector: DISABLED "
                        "(no readable-memory probe installed) — it will find NOTHING, "
                        "and its silence means nothing.\n");
        fflush(stderr);
        return;
    }
    // Two 32-byte blocks, each an 8-byte {len,rc} header followed by payload.
    static unsigned char liveA[32], liveB[32], dead[32];
    memset(liveA, 0, sizeof(liveA)); memset(liveB, 0, sizeof(liveB));
    memset(dead,  0, sizeof(dead));
    unsigned int one = 1;
    memcpy(liveA, &one, 4); memcpy(liveA + 4, &one, 4);   // len=1 rc=1  -> live
    memcpy(liveB, &one, 4); memcpy(liveB + 4, &one, 4);   // len=1 rc=1  -> live
    // `dead` keeps {0,0} -> released.

    void* oursSlot[1]   = { liveA + 8 };
    void* deadSlot[1]   = { dead  + 8 };
    void* liveSlot[1]   = { liveB + 8 };

    RtDiffSite probe = { "[selftest]", 0, 0, false, true, 0, false, 0 };
    RtDiffScanForReleasedPointers(&probe, oursSlot, deadSlot, sizeof(oursSlot), false);
    const uint64_t positive = probe.freedPtrHits;
    probe.freedPtrHits = 0;
    RtDiffScanForReleasedPointers(&probe, oursSlot, liveSlot, sizeof(oursSlot), false);
    const uint64_t negative = probe.freedPtrHits;

    const bool ok = (positive == 1 && negative == 0);
    fprintf(stderr, "[RtDiff] released-pointer detector self-test: %s "
                    "(fires on a released slot: %llu/1; stays silent on a live one: %llu/0)\n",
            ok ? "ALIVE" : "*** BROKEN — its silence means NOTHING ***",
            (unsigned long long)positive, (unsigned long long)negative);
    fflush(stderr);
}

void RtDiffReportBytes(RtDiffSite* site, const void* ours, const void* theirs, size_t n) {
    if (!site) return;
    if (!ours || !theirs) {
        RtDiffReport(site, false, ours ? "(ptr)" : "(null)", theirs ? "(ptr)" : "(null)");
        return;
    }
    if (memcmp(ours, theirs, n) == 0) {
        RtDiffReport(site, true, nullptr, nullptr);
        return;
    }

    // The detector's self-test runs once, here, the first time anything is
    // byte-compared -- early enough that a run which later crashes has already
    // said on the record whether the detector was alive.
    static bool selfTested = false;
    if (!selfTested) { selfTested = true; RtDiffDetectorSelfTest(); }

    // Before naming the difference, ask whether the ORACLE caused it: a scratch
    // still holding a pointer our body released is a harness defect, and it
    // would otherwise be reported as a port divergence. Lane M, Finding 1.
    RtDiffScanForReleasedPointers(site, ours, theirs, n);

    // Name the first mismatching byte — "the buffers differ" is not actionable,
    // the offset usually names the field.
    const uint8_t* a = static_cast<const uint8_t*>(ours);
    const uint8_t* b = static_cast<const uint8_t*>(theirs);
    size_t off = 0;
    while (off < n && a[off] == b[off]) off++;

    char oursBuf[96], theirsBuf[96];
    int oa = snprintf(oursBuf, sizeof(oursBuf), "+0x%zx:", off);
    int ob = snprintf(theirsBuf, sizeof(theirsBuf), "+0x%zx:", off);
    for (size_t i = off; i < n && i < off + 8; i++) {
        oa += snprintf(oursBuf + oa, sizeof(oursBuf) - (size_t)oa, " %02x", a[i]);
        ob += snprintf(theirsBuf + ob, sizeof(theirsBuf) - (size_t)ob, " %02x", b[i]);
    }
    RtDiffReport(site, false, oursBuf, theirsBuf);
}

void RtDiffSummary() {
    if (!RtDiffEnabled()) return;
    uint64_t totalCalls = 0, totalDiv = 0, totalWorked = 0;
    size_t   dirtySites = 0, undeclared = 0, harnessSites = 0;
    size_t corrupt = 0;
    for (size_t i = 0; i < g_siteCount; i++) {
        RtDiffSite* s = g_sites[i];
        if (!RtDiffSiteLooksLive(s)) { corrupt++; continue; }
        totalCalls += s->calls;
        totalDiv   += s->divergences;
        if (s->divergences)   dirtySites++;
        if (s->freedPtrHits)  harnessSites++;
        if (s->workDeclared)  totalWorked += s->worked; else undeclared++;
    }
    fprintf(stderr, "[RtDiff] ==== %zu sites, %llu calls, %llu divergences in %zu sites ====\n",
            g_siteCount, (unsigned long long)totalCalls,
            (unsigned long long)totalDiv, dirtySites);
    for (size_t i = 0; i < g_siteCount; i++) {
        RtDiffSite* s = g_sites[i];
        if (!RtDiffSiteLooksLive(s)) {
            // Do NOT dereference s->name here — that is exactly what turned one
            // corrupted site into a fault inside the summary and threw away the
            // whole report. Print the raw fields and cross-reference the index
            // against the "[RtDiff] site[N] @addr = name" line logged at
            // registration to find out which function's site was hit.
            fprintf(stderr, "[RtDiff]   site[%zu] @%p CORRUPTED — name=%p calls=%llu "
                            "divergences=%llu (see the site[%zu] line above for its "
                            "name; something wrote over this static)\n",
                    i, (const void*)s, (const void*)(s ? s->name : nullptr),
                    (unsigned long long)(s ? s->calls : 0),
                    (unsigned long long)(s ? s->divergences : 0), i);
            continue;
        }
        // `worked=` is the did-work column (lane M). It sits BEFORE
        // `divergences=` on purpose: this project's tools -- and
        // tools/rt_matrix_cell.sh -- match `divergences=<n>` at END OF LINE to
        // pick out the dirty sites, and moving it would silently break that the
        // way a `tail -80` once broke the whole report.
        //
        // An UNDECLARED site prints `worked=?`, never `worked=0`. `calls` is
        // calls ENTERED, and a large `calls` is not coverage: lane M's Finding 1
        // sat behind `calls=12896` on a site where TEN calls did anything at
        // all. A blank or zero column would read as "fine"; a `?` reads as the
        // unknown it is.
        char workBuf[32];
        if (s->workDeclared)
            snprintf(workBuf, sizeof(workBuf), "%llu", (unsigned long long)s->worked);
        else
            snprintf(workBuf, sizeof(workBuf), "?");
        fprintf(stderr, "[RtDiff]   %-48s calls=%-10llu worked=%-10s divergences=%llu%s\n",
                s->name,
                (unsigned long long)s->calls,
                workBuf,
                (unsigned long long)s->divergences,
                s->freedPtrHits ? "  *** HARNESS DEFECT: released pointer replayed ***" : "");
    }
    // The did-work rollup. `undeclared` is the honest headline: until a site
    // declares RT_DIFF_WORK, nobody knows whether its calls did anything, and
    // that unknown is what let Finding 1 hide behind a busy-looking counter.
    fprintf(stderr, "[RtDiff] ==== did-work: %llu calls did work across %zu declaring sites; "
                    "%zu sites have NOT declared (their coverage is UNKNOWN, not zero) ====\n",
            (unsigned long long)totalWorked, g_siteCount - undeclared, undeclared);
    if (harnessSites)
        fprintf(stderr, "[RtDiff] ==== %zu sites hit the RELEASED-POINTER detector. These are "
                        "HARNESS defects, NOT port defects: the oracle handed the DLL's body a "
                        "pointer our body had already released. Make each a NO-ORACLE port; do "
                        "not exclude the bytes. ====\n", harnessSites);
    if (corrupt)
        fprintf(stderr, "[RtDiff] ==== %zu of %zu sites were CORRUPTED — a stray write hit "
                        "the harness's own statics; the totals above exclude them ====\n",
                corrupt, g_siteCount);
    fflush(stderr);
}

// ---- Formatting ------------------------------------------------------------

void RtDiffFormat(char* buf, size_t n, int v)                { snprintf(buf, n, "%d", v); }
void RtDiffFormat(char* buf, size_t n, unsigned int v)       { snprintf(buf, n, "%u (0x%x)", v, v); }
void RtDiffFormat(char* buf, size_t n, long long v)          { snprintf(buf, n, "%lld", v); }
void RtDiffFormat(char* buf, size_t n, unsigned long long v) { snprintf(buf, n, "%llu (0x%llx)", v, v); }
void RtDiffFormat(char* buf, size_t n, bool v)               { snprintf(buf, n, "%s", v ? "true" : "false"); }
void RtDiffFormat(char* buf, size_t n, const void* v)        { snprintf(buf, n, "%p", v); }

// Floats print with their raw bits too: two values that render identically at
// %g can still differ in the last mantissa bit, and for a byte-faithful port
// that difference is the whole finding.
void RtDiffFormat(char* buf, size_t n, float v) {
    uint32_t bits;
    memcpy(&bits, &v, sizeof(bits));
    snprintf(buf, n, "%.9g [%08x]", (double)v, bits);
}
void RtDiffFormat(char* buf, size_t n, double v) {
    uint64_t bits;
    memcpy(&bits, &v, sizeof(bits));
    snprintf(buf, n, "%.17g [%016llx]", v, (unsigned long long)bits);
}

bool RtDiffFloatEqual(float ours, float theirs, float tol) {
    if (std::isnan(ours) && std::isnan(theirs)) return true;
    if (tol == 0.0f) {
        uint32_t a, b;
        memcpy(&a, &ours, sizeof(a));
        memcpy(&b, &theirs, sizeof(b));
        return a == b;  // bit equality, so -0.0f != 0.0f is visible
    }
    return std::fabs(ours - theirs) <= tol;
}
