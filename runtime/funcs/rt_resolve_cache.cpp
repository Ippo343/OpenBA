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
