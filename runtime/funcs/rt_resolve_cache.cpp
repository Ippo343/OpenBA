// ============================================================================
// runtime/funcs/rt_resolve_cache.cpp — resolve-and-cache that never caches null
// ============================================================================
//
// Wave 9 lane H (hardening). The tree's standard idiom for "resolve a Runtime
// symbol once and keep it" was a function-local static:
//
//     static T* p = reinterpret_cast<T*>(ResolveExport("?_singleton@..."));
//     return p;
//
// which has a defect that is invisible today and fatal at cutover: **the FIRST
// caller fixes the answer for the whole process.** If the resolve misses, the
// null is frozen, and a registration that lands later — which is exactly how
// cutover mints the exported DATA symbols the DLL used to provide — cannot
// repair it. Worse, nothing says which symbol was missing: the consumer either
// swallows the null behind an `if` and silently does nothing, or dereferences
// it several call frames away, where the fault address names nothing. The
// allocator's `?_singleton@MemoryAllocator` is the worst instance because
// cutover executes it FIRST, and its fault is a read of `null + 0x640` in
// MemAlloc with no mention of `_singleton` anywhere near it
// (docs/runtime_port/WAVE9_LANE_E3.md §8).
//
// The correct form already existed in the tree — runtime/src/object/internal.cpp
// and runtime/src/object/type_system.cpp both write `if (!g_x) g_x = Resolve(…)`
// — so nothing here is invented, only applied.
//
// WHAT THIS DOES, AND WHAT IT DELIBERATELY DOES NOT
//
//   * A non-null answer is cached and returned on every later call, so the cost
//     profile is unchanged: one plain load, and no GetProcAddress in steady
//     state. That matters — ResolveExportRaw IS GetProcAddress and two calls in
//     System::Time's ctor once cost 5.6 points of sim time
//     (reference_rtoriginal_is_getprocaddress.md).
//   * A NULL answer is not cached. The next call retries, so a late
//     registration is picked up.
//   * A miss is NAMED, once per symbol, and counted. `RtResolveMissCount()`
//     reading zero across a run is the measurement that a conversion changed
//     nothing (WAVE9_LANE_H.md §4).
//   * It does NOT fix the other half of the frozen-static problem: a NON-null
//     answer is still frozen, so a site whose first call happens before
//     RuntimePortInit() captures the DLL's address permanently and bypasses a
//     port registered afterwards. That is a separate defect class, it is live
//     today rather than at cutover, and this file does not address it. See
//     WAVE9_LANE_H.md §5.
//
// The benign race on `*cache` is intentional: two threads may both resolve and
// both store, and they store the same value. A lock here would put a mutex in
// front of every singleton accessor in the engine for no benefit.
// ============================================================================

#include <cstdio>
#include <cstring>

#include "rt_port.h"

namespace {

// Names already reported. Small and fixed: the point is one line per symbol,
// not a complete registry. Past the cap we stop naming and keep counting, which
// is the right trade — if 64 distinct symbols are missing, the first 64 have
// already told you what is wrong.
const int  kMaxNamed = 64;
const char* g_named[kMaxNamed];
int         g_namedCount = 0;
uint64_t    g_missCount  = 0;

bool AlreadyNamed(const char* mangled) {
    for (int i = 0; i < g_namedCount; ++i)
        if (std::strcmp(g_named[i], mangled) == 0) return true;
    return false;
}

}  // namespace

// ============================================================================
// EARLY-RESOLVE RECORDER (Wave 9 lane H, second half)
// ============================================================================
//
// The defect this instrument exists for is the one the retry contract above
// does NOT fix, and it is live TODAY rather than at cutover: a non-null answer
// is still frozen, so a site whose FIRST call happens before RuntimePortInit()
// has registered its port captures the DLL's address permanently and bypasses
// our own body for the life of the process.
//
// Nothing we have built can see it. The site is spelled correctly, it goes
// through the right (registry-aware) door, so tools/port_bypass_audit.py finds
// nothing; and the diff oracle reports the port clean for the same reason it
// always does when a port is bypassed -- our body never runs, so it never
// disagrees. ONLY THE ORDERING IS WRONG.
//
// So measure the ordering directly. A resolve is recorded here when it happened
// BEFORE RuntimePortInit() returned AND missed the port registry at the time.
// At summary, each recorded name is asked of the registry again:
//
//   * registered NOW  -> the port exists and this caller did not get it. If the
//                        call site caches, that is a LIVE PORT BYPASS.
//   * still absent    -> benign. Nothing has ever ported it, so reaching the
//                        DLL early is the same answer it would get later.
//
// The caller address is recorded so the site can be named with `nm` against
// standalone/build/reimpl-x64.exe (image base 0x140000000), exactly as
// BANISHED_RT_STRICT does for cutover residue.
//
// COVERAGE, stated so the number is not over-read: this hooks the three
// REGISTRY-AWARE doors (ResolveExport, ResolveOrdinal,
// ResolveAddr(Module::Runtime)). It deliberately does not hook
// ResolveExportRaw -- that door is supposed to reach the shipped body, so an
// early raw resolve is not a bypass. It also cannot tell whether the CALLER
// caches; that is what the recorded return address is for.
// ============================================================================

namespace {

struct EarlyRec { const char* name; uintptr_t addr; uint16_t ordinal; void* caller; };
const int kMaxEarly = 256;
EarlyRec g_early[kMaxEarly];
int      g_earlyCount = 0;
uint64_t g_earlyTotal = 0;
bool     g_portInitDone = false;

}  // namespace

void RtResolveMarkPortInitDone() { g_portInitDone = true; }
bool RtResolvePortInitDone()     { return g_portInitDone; }

void RtResolveNoteEarly(const char* mangled, uintptr_t ghidra_addr,
                        uint16_t ordinal, void* caller) {
    if (g_portInitDone) return;
    ++g_earlyTotal;
    for (int i = 0; i < g_earlyCount; ++i) {
        const EarlyRec& e = g_early[i];
        if (e.addr == ghidra_addr && e.ordinal == ordinal && e.caller == caller &&
            ((e.name == nullptr && mangled == nullptr) ||
             (e.name && mangled && std::strcmp(e.name, mangled) == 0)))
            return;                       // already recorded this exact site
    }
    if (g_earlyCount >= kMaxEarly) return;
    g_early[g_earlyCount++] = { mangled, ghidra_addr, ordinal, caller };
}

uint64_t RtResolveEarlyCount() { return g_earlyTotal; }

void RtResolveEarlySummary() {
    if (g_earlyCount == 0) {
        std::fprintf(stderr,
            "[RtResolveEarly] 0 registry-aware resolves happened before "
            "RuntimePortInit() finished. The early-snapshot port-bypass class is "
            "NOT LIVE on this path.\n");
        std::fflush(stderr);
        return;
    }
    int confirmed = 0;
    std::fprintf(stderr,
        "[RtResolveEarly] %llu early registry-aware resolve(s), %d distinct site(s). "
        "A site is a LIVE BYPASS only if the symbol is registered NOW and the site "
        "caches; the caller address names it (nm against reimpl-x64.exe, base "
        "0x140000000).\n",
        (unsigned long long)g_earlyTotal, g_earlyCount);
    for (int i = 0; i < g_earlyCount; ++i) {
        const EarlyRec& e = g_early[i];
        const bool now = e.name && RuntimeExportIsRegistered(e.name);
        if (now) ++confirmed;
        if (e.name)
            std::fprintf(stderr, "[RtResolveEarly]   %-9s caller=%p  \"%s\"\n",
                         now ? "BYPASS?" : "benign", e.caller, e.name);
        else if (e.ordinal)
            std::fprintf(stderr, "[RtResolveEarly]   benign    caller=%p  ordinal %u\n",
                         e.caller, (unsigned)e.ordinal);
        else
            std::fprintf(stderr, "[RtResolveEarly]   benign    caller=%p  addr 0x%llx\n",
                         e.caller, (unsigned long long)e.addr);
    }
    std::fprintf(stderr,
        "[RtResolveEarly] %d of %d are registered now -- those are the candidates. "
        "Do NOT fix them here; name them and hand them on.\n",
        confirmed, g_earlyCount);
    std::fflush(stderr);
}

void* RtResolveCached(const char* mangled, void** cache) {
    if (*cache) return *cache;
    void* p = ResolveExport(mangled);
    if (p) { *cache = p; return p; }

    // NAMED REFUSAL. The null is deliberately not stored.
    ++g_missCount;
    if (!AlreadyNamed(mangled)) {
        if (g_namedCount < kMaxNamed) g_named[g_namedCount++] = mangled;
        std::fprintf(stderr,
            "[RtResolve] MISS \"%s\" -> NULL. Nothing has registered it and the "
            "shipped DLL does not export it (or is absent). The null is NOT "
            "cached; the next call will retry. If this is cutover, this symbol "
            "still needs minting.\n", mangled);
        std::fflush(stderr);
    }
    return nullptr;
}

uint64_t RtResolveMissCount() { return g_missCount; }

void RtResolveMissSummary() {
    if (g_missCount == 0) {
        std::fprintf(stderr, "[RtResolve] 0 misses — every cached resolve in this "
                             "run found its symbol.\n");
    } else {
        std::fprintf(stderr, "[RtResolve] %llu MISSES across %d distinct symbol(s); "
                             "see the [RtResolve] MISS lines above.\n",
                     (unsigned long long)g_missCount, g_namedCount);
    }
    std::fflush(stderr);
}
