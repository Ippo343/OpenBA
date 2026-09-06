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
    uint64_t totalCalls = 0, totalDiv = 0;
    size_t   dirtySites = 0;
    size_t corrupt = 0;
    for (size_t i = 0; i < g_siteCount; i++) {
        RtDiffSite* s = g_sites[i];
        if (!RtDiffSiteLooksLive(s)) { corrupt++; continue; }
        totalCalls += s->calls;
        totalDiv   += s->divergences;
        if (s->divergences) dirtySites++;
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
        fprintf(stderr, "[RtDiff]   %-48s calls=%-10llu divergences=%llu\n",
                s->name,
                (unsigned long long)s->calls,
                (unsigned long long)s->divergences);
    }
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
