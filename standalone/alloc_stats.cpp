// standalone/alloc_stats.cpp
// [ALLOCSTATS] engine-heap accounting (BANISHED_ALLOC_STATS=1, BANISHED_ALLOC_BIG=<bytes>).
// Fed by the ported allocator's locked Allocate/Free entry points
// (standalone/alloc_cutover_arm.cpp). Formerly the tail of allocator_lock.cpp,
// whose DLL byte-patch half died with the DLL (ENDGAME A.3).

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace {

// [ALLOCSTATS] env BANISHED_ALLOC_STATS — engine-heap accounting (diagnostic).
// ---------------------------------------------------------------------------
// The engine allocator is a fixed boundary-tag arena; when it fills, Allocate
// raises the "failed to allocate a memory range" fatal box. This attributes the
// consumption: running live-bytes, alloc/free counts, and per-caller totals for
// the biggest consumers. All state is in plain globals / VirtualAlloc'd memory —
// it must never call back into the engine allocator.
//
//   BANISHED_ALLOC_STATS=1        report every ~2s
//   BANISHED_ALLOC_BIG=<bytes>    also log each single allocation >= this
//
// Not thread-safe by design beyond the allocator CS we already hold: every
// update below happens inside EnterCriticalSection(self+0x640), which both
// Allocate and Free take, so the counters are consistent without extra locking.
int      g_statsOn  = -1;         // -1 unresolved, 0 off, 1 on
uint64_t g_bigThreshold = 0;      // 0 = don't log individual big allocations

uint64_t g_liveBytes = 0, g_totalAllocBytes = 0;
uint64_t g_allocCount = 0, g_freeCount = 0, g_freeUnknown = 0;
uint64_t g_peakLiveBytes = 0;
uint64_t g_lastReportMs = 0;

// Open-addressing table: ptr -> (size, caller). Sized for ~1M live blocks and
// carved out of its own VirtualAlloc reservation, independent of both the engine
// heap and the CRT heap.
struct BlockRec { void* ptr; uint64_t size; void* caller; void* ctx; };
}  // namespace
// Set by callers (GenericArrayResize) right before fn_MemAlloc so the block table can
// remember WHICH array header owns a buffer; cleared after the call.
extern "C" { thread_local void* g_allocStatsCtx = nullptr; }
namespace {
const size_t kBlkSlots = 1u << 21;           // 2M slots, ~48 MB
BlockRec* g_blocks = nullptr;
uint64_t  g_blkInserts = 0, g_blkEvicted = 0;

// Per-caller live totals (small fixed table; callers are few).
struct CallerRec { void* caller; uint64_t liveBytes; uint64_t liveCount; uint64_t totalCount; };
const size_t kCallerSlots = 4096;
CallerRec* g_callers = nullptr;

inline size_t HashPtr(void* p) {
    uint64_t x = reinterpret_cast<uint64_t>(p) >> 4;
    x *= 0x9E3779B97F4A7C15ull; x ^= x >> 29;
    return static_cast<size_t>(x);
}

CallerRec* FindCaller(void* caller, bool create) {
    if (!g_callers) return nullptr;
    size_t i = HashPtr(caller) & (kCallerSlots - 1);
    for (size_t probe = 0; probe < 64; probe++) {
        CallerRec& r = g_callers[(i + probe) & (kCallerSlots - 1)];
        if (r.caller == caller) return &r;
        if (!r.caller) {
            if (!create) return nullptr;
            r.caller = caller; return &r;
        }
    }
    return nullptr;
}

void StatsRecordAlloc(void* p, uint64_t size, void* caller) {
    if (!p) return;
    g_allocCount++;
    g_liveBytes += size;
    g_totalAllocBytes += size;
    if (g_liveBytes > g_peakLiveBytes) g_peakLiveBytes = g_liveBytes;

    if (CallerRec* cr = FindCaller(caller, true)) {
        cr->liveBytes += size; cr->liveCount++; cr->totalCount++;
    }
    if (!g_blocks) return;
    size_t i = HashPtr(p) & (kBlkSlots - 1);
    for (size_t probe = 0; probe < 32; probe++) {
        BlockRec& b = g_blocks[(i + probe) & (kBlkSlots - 1)];
        if (!b.ptr) { b.ptr = p; b.size = size; b.caller = caller; b.ctx = g_allocStatsCtx; g_blkInserts++; return; }
    }
    g_blkEvicted++;   // table full at this bucket — this block won't be matched on free
}

void StatsRecordFree(void* p) {
    if (!p || !g_blocks) return;
    g_freeCount++;
    size_t i = HashPtr(p) & (kBlkSlots - 1);
    for (size_t probe = 0; probe < 32; probe++) {
        BlockRec& b = g_blocks[(i + probe) & (kBlkSlots - 1)];
        if (b.ptr == p) {
            g_liveBytes -= (b.size <= g_liveBytes ? b.size : g_liveBytes);
            if (CallerRec* cr = FindCaller(b.caller, false)) {
                cr->liveBytes -= (b.size <= cr->liveBytes ? b.size : cr->liveBytes);
                if (cr->liveCount) cr->liveCount--;
            }
            b.ptr = nullptr; b.size = 0; b.caller = nullptr; b.ctx = nullptr;
            return;
        }
        if (!b.ptr) break;      // not tracked (allocated before stats armed)
    }
    g_freeUnknown++;
}

// Emit a report at most every ~2s. Called with the allocator CS held.
void StatsMaybeReport() {
    uint64_t now = GetTickCount64();
    if (now - g_lastReportMs < 2000) return;
    g_lastReportMs = now;

    fprintf(stderr,
        "[ALLOCSTATS] live=%llu MB (peak %llu MB) totalAllocated=%llu MB  "
        "allocs=%llu frees=%llu untracked_frees=%llu tableFull=%llu\n",
        (unsigned long long)(g_liveBytes >> 20), (unsigned long long)(g_peakLiveBytes >> 20),
        (unsigned long long)(g_totalAllocBytes >> 20),
        (unsigned long long)g_allocCount, (unsigned long long)g_freeCount,
        (unsigned long long)g_freeUnknown, (unsigned long long)g_blkEvicted);

    // Top 8 callers by live bytes.
    for (int rank = 0; rank < 8; rank++) {
        CallerRec* best = nullptr;
        static void* shown[8];
        for (size_t i = 0; i < kCallerSlots; i++) {
            CallerRec& r = g_callers[i];
            if (!r.caller || !r.liveBytes) continue;
            bool already = false;
            for (int k = 0; k < rank; k++) if (shown[k] == r.caller) { already = true; break; }
            if (already) continue;
            if (!best || r.liveBytes > best->liveBytes) best = &r;
        }
        if (!best) break;
        shown[rank] = best->caller;
        fprintf(stderr, "[ALLOCSTATS]   #%d caller=%p  live=%llu MB  liveBlocks=%llu  totalAllocs=%llu\n",
                rank + 1, best->caller, (unsigned long long)(best->liveBytes >> 20),
                (unsigned long long)best->liveCount, (unsigned long long)best->totalCount);
    }
    fflush(stderr);
}

}  // namespace (reopened below)

// Look a user pointer up in the ALLOCSTATS block table (BANISHED_ALLOC_STATS=1): size +
// allocation caller. For the redzone heap report — names the owner of a corrupt or
// overflowing block when the redzone shim is not installed (watch-only mode).
extern "C" int AllocStats_Lookup(void* userPtr, uint64_t* outSize, void** outCaller, void** outCtx) {
    if (!g_blocks || !userPtr) return 0;
    size_t i = HashPtr(userPtr) & (kBlkSlots - 1);
    for (size_t probe = 0; probe < 32; probe++) {
        BlockRec& b = g_blocks[(i + probe) & (kBlkSlots - 1)];
        if (b.ptr == userPtr) { if (outSize) *outSize = b.size; if (outCaller) *outCaller = b.caller;
                                if (outCtx) *outCtx = b.ctx; return 1; }
        if (!b.ptr) break;
    }
    return 0;
}

// Record one allocation / one free in the ALLOCSTATS tables. Exported so the
// ported allocator's composed swap stub (standalone/rt_alloc_swap.cpp) can keep
// this accounting alive after the lock patches are retired, instead of
// reimplementing it. No-ops unless BANISHED_ALLOC_STATS is set.
extern "C" void AllocStats_RecordAlloc(void* p, uint64_t size, void* caller);
extern "C" void AllocStats_RecordFree(void* p);

namespace {

bool StatsOn() {
    if (g_statsOn < 0) {
        const char* e = getenv("BANISHED_ALLOC_STATS");
        g_statsOn = (e && e[0] && e[0] != '0') ? 1 : 0;
        if (g_statsOn) {
            const char* b = getenv("BANISHED_ALLOC_BIG");
            g_bigThreshold = b ? strtoull(b, nullptr, 0) : 0;
            g_blocks = reinterpret_cast<BlockRec*>(VirtualAlloc(
                nullptr, kBlkSlots * sizeof(BlockRec), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            g_callers = reinterpret_cast<CallerRec*>(VirtualAlloc(
                nullptr, kCallerSlots * sizeof(CallerRec), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            g_lastReportMs = GetTickCount64();
            fprintf(stderr, "[ALLOCSTATS] armed (blocks=%p callers=%p bigThreshold=%llu)\n",
                    (void*)g_blocks, (void*)g_callers, (unsigned long long)g_bigThreshold);
            fflush(stderr);
        }
    }
    return g_statsOn > 0;
}

}  // namespace

// See the declarations above AllocStats_Lookup.
extern "C" void AllocStats_RecordAlloc(void* p, uint64_t size, void* caller) {
    if (!StatsOn()) return;
    StatsRecordAlloc(p, size, caller);
    if (g_bigThreshold && size >= g_bigThreshold) {
        fprintf(stderr, "[ALLOCSTATS] BIG alloc %llu MB (%llu bytes) caller=%p -> %p\n",
                (unsigned long long)(size >> 20), (unsigned long long)size, caller, p);
        fflush(stderr);
    }
    StatsMaybeReport();
}

extern "C" void AllocStats_RecordFree(void* p) {
    if (!StatsOn()) return;
    StatsRecordFree(p);
}
