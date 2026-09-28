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
//   * WAVE 15 LANE ORDER CHANGED THIS. It used to read: "it does NOT fix the
//     other half of the frozen-static problem -- a NON-null answer is still
//     frozen, so a site whose first call happens before RuntimePortInit()
//     captures the DLL's address permanently and bypasses a port registered
//     afterwards." That is now fixed here, in §6 below: a hit obtained before
//     RtResolvePortInitDone() is RETURNED but NOT STORED, so the site
//     re-resolves and picks up the final answer. See §6 for the reason this
//     helper is the right place for the fix and what it still does not cover.
//
// The benign race on `*cache` is intentional: two threads may both resolve and
// both store, and they store the same value. A lock here would put a mutex in
// front of every singleton accessor in the engine for no benefit.
// ============================================================================

#include <cstdio>
#include <cstdlib>
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
        RtResolveSlotAudit();     // Wave 15 lane ORDER, §6
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
    RtResolveSlotAudit();         // Wave 15 lane ORDER, §6
}

// ============================================================================
// §6 — DO NOT FREEZE AN ANSWER OBTAINED BEFORE RuntimePortInit() FINISHED
//      (Wave 15 lane ORDER)
// ============================================================================
//
// §5 above, and WAVE9_LANE_H.md §5, describe the half of the frozen-static
// defect the retry contract does not fix: a NON-null answer is frozen too, so a
// site first called before the registry is complete keeps the DLL's address for
// the life of the process while every other site moves to our storage. On a
// LIVE MUTABLE POINTER — which is what every `?_singleton@` row is — that is a
// SPLIT BRAIN: two copies of one pointer, our writer publishing into one and
// somebody's reader loading the other.
//
// TWO FACTS FROM LANE ORDER'S ENUMERATION MADE THIS THE PLACE TO FIX IT.
//
//  1. The hazard is NOT confined to hand-written `static T* p = Resolve(...)`
//     accessors. `RT_DATA_SLOT` and `RT_EXPORT_ONCE` both expand to a
//     function-local static plus THIS function, and this function froze a hit
//     unconditionally — so all 83 RT_DATA_SLOT singleton sites carried exactly
//     the same hazard as the 10 hand-written frozen statics. The distinction
//     the Wave 15 kickoff drew ("still a plain frozen static rather than
//     RtResolveCached") is not a distinction for THIS defect: converting a
//     frozen static to RtResolveCached fixed the null half and changed nothing
//     about the non-null half. Fixing it here fixes both macros and every
//     caller of them at once.
//
//  2. `?_singleton@SaveLoad@Save@@` is PROMOTED to our own storage on every
//     default run (rt_data_mint.cpp:924 calls RtMintRegisterPromotedRows()
//     unless BANISHED_RT_MINT=1), so ResolveExport's answer for that name
//     genuinely CHANGES across RtDataMintInit(). The hazard therefore has a
//     live subject in the tree today, not a hypothetical one.
//
// (History: B3R W0-SM M3 deleted the mint, RtDataMintInit() and RT_DATA_SLOT;
// every exported DATA cell our tree touches is now its class's own static, so
// no by-name answer changes across init any more. RT_EXPORT_ONCE remains.)
//
// THE RULE. A hit obtained while `!RtResolvePortInitDone()` is returned to the
// caller but NOT stored. The caller gets a correct answer for right now, and
// re-resolves next time, so the first call made after the registry is complete
// is the one that freezes. Cost: pre-init calls pay a GetProcAddress each
// (ResolveExportRaw IS GetProcAddress —
// reference_rtoriginal_is_getprocaddress.md), which is why the count is
// reported rather than hidden. It is bounded by the startup path; steady state
// is unchanged, one plain load.
//
// WHAT THIS STILL DOES NOT COVER, stated so the number is not over-read:
//
//   * The ADDRESS route. `ResolveAddr(Module::Runtime, 0x1801fc6f0)` does not
//     come through here at all, and 13 live source sites reach a Wave 15 row
//     that way (`reference_resolveaddr_consults_port_registry_first.md` — that
//     door consults the port registry FIRST, so it has the same ordering
//     dependence). A promotion must therefore register the ADDRESS as well as
//     the NAME, which is what reg_mint_promoted.cpp:669-670 does for SaveLoad.
//   * Hand-written frozen statics that call `ResolveExport` directly. Lane
//     ORDER converted the ten it found in `runtime/`; a new one can always be
//     written, and `tools/` has no checker for the shape.
//   * An in-DLL RIP-relative reference. We intercept only at doors
//     (reference_a_ported_row_can_be_permanently_unpromotable.md).
// ============================================================================

namespace {

// One record per NAME (not per site): the audit's question is "did this
// symbol's answer change across init?", which is a property of the name.
struct EarlySlotRec { const char* name; void* first; bool frozen; };
const int    kMaxEarlySlot = 128;
EarlySlotRec g_slot[kMaxEarlySlot];
int          g_slotCount     = 0;
uint64_t     g_preInitHits   = 0;   // returned WITHOUT freezing (the new rule)
uint64_t     g_preInitFrozen = 0;   // frozen anyway (negative control armed)

void NoteEarlySlot(const char* mangled, void* first, bool frozen) {
    for (int i = 0; i < g_slotCount; ++i)
        if (std::strcmp(g_slot[i].name, mangled) == 0) return;
    if (g_slotCount >= kMaxEarlySlot) return;
    g_slot[g_slotCount++] = { mangled, first, frozen };
}

}  // namespace

uint64_t RtResolvePreInitHitCount()   { return g_preInitHits; }
uint64_t RtResolvePreInitFrozenCount(){ return g_preInitFrozen; }

int RtResolveSlotAudit() {
    if (g_slotCount == 0) {
        std::fprintf(stderr,
            "[RtSlotAudit] 0 names were resolved through the CACHING door "
            "(RtResolveCached / RT_EXPORT_ONCE) before "
            "RuntimePortInit() finished. Nothing could have frozen an early "
            "answer on this path.\n");
        std::fflush(stderr);
        return 0;
    }
    int changed = 0, split = 0;
    std::fprintf(stderr,
        "[RtSlotAudit] %d distinct name(s) first resolved through the caching "
        "door BEFORE init finished; %llu hit(s) returned unfrozen, %llu frozen "
        "early (control %s). Each name is re-resolved now and compared.\n",
        g_slotCount, (unsigned long long)g_preInitHits,
        (unsigned long long)g_preInitFrozen,
        "off");
    for (int i = 0; i < g_slotCount; ++i) {
        const EarlySlotRec& r = g_slot[i];
        void* now = ResolveExport(r.name);
        if (now == r.first) {
            std::fprintf(stderr,
                "[RtSlotAudit]   same      %p  \"%s\"\n", now, r.name);
            continue;
        }
        ++changed;
        // The answer moved across init. Whether that is a defect depends
        // entirely on whether the site kept the old one.
        if (r.frozen) {
            ++split;
            std::fprintf(stderr,
                "[RtSlotAudit]   SPLIT     early=%p final=%p  \"%s\"  -- FROZEN "
                "on the pre-init answer: this site reads the old storage for the "
                "life of the process while everyone else reads the new one.\n",
                r.first, now, r.name);
        } else {
            std::fprintf(stderr,
                "[RtSlotAudit]   REPAIRED  early=%p final=%p  \"%s\"  -- not "
                "frozen (§6), so the site re-resolved and now reads the final "
                "storage.\n", r.first, now, r.name);
        }
    }
    std::fprintf(stderr,
        "[RtSlotAudit] %d of %d name(s) CHANGED answer across init (that is the "
        "EFFECTIVENESS number -- a run where it is 0 says the rule had nothing "
        "to bite on, not that the rule works); %d of those are LIVE SPLIT "
        "BRAINS on this run.\n", changed, g_slotCount, split);
    std::fflush(stderr);
    return split;
}

void* RtResolveCached(const char* mangled, void** cache) {
    if (*cache) return *cache;
    void* p = ResolveExport(mangled);
    if (p) {
        // §6. An answer obtained before the registry is complete is handed back
        // but not kept, so a later promotion of this name is picked up.
        const bool early  = !RtResolvePortInitDone();
        const bool freeze = !early;
        if (early) {
            NoteEarlySlot(mangled, p, freeze);
            if (freeze) ++g_preInitFrozen; else ++g_preInitHits;
        }
        if (freeze) *cache = p;
        return p;
    }

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
