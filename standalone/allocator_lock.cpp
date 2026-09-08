// standalone/allocator_lock.cpp
// See allocator_lock.h for rationale.
//
// Detour mechanics (no prologue relocation needed — the first 13 bytes of
// Allocate are all position-independent register pushes + SUB RSP,0x20; the
// first RIP-relative instruction is at +13):
//
//   Allocate prologue (13 bytes, validated before patching):
//     40 53            PUSH RBX
//     57               PUSH RDI
//     41 54            PUSH R12
//     41 56            PUSH R14
//     41 57            PUSH R15
//     48 83 EC 20      SUB RSP,0x20
//     48 8B 05 ..      MOV RAX,[RIP+..]   <- +13, stays in place
//
//   We overwrite [0..12] with `MOV RAX, &AllocateLocked ; JMP RAX ; NOP`
//   (RAX is dead at entry — the first thing the body does is overwrite it).
//   A separate executable trampoline holds the original 13 bytes followed by an
//   absolute JMP back to Allocate+13, so calling the trampoline runs the real
//   allocator body and returns to us.


#include "allocator_lock.h"
#include "resolve.h"
#include "engine_funcs.h"   // g_pMemAllocSingleton

#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

extern "C" void RedzoneAlloc_TeardownOp(const char* what);   // redzone_alloc.cpp

// ---------------------------------------------------------------------------
// Observe-only shadow hooks (standalone/rt_alloc_swap.cpp, Wave 9 lane E).
// ---------------------------------------------------------------------------
// Null unless the shadow driver arms them. They are called from inside
// AllocateLocked/FreeLocked, i.e. WITH the allocator critical section held,
// which is the whole reason the shadow rides here instead of installing a
// second detour on the same two prologues: the replica must be mutated in the
// same serialized order the real arena was. See the header comment of
// rt_alloc_swap.cpp for why WAVE2_ALLOCATOR.md Â§3's literal reading is wrong.
extern "C" {
void (*g_allocShadowOnAlloc)(void* self, uint64_t size, uintptr_t align, void* result) = nullptr;
void (*g_allocShadowOnFree)(void* self, void* ptr) = nullptr;
}

// Saved state for AllocatorLockRetireForPort(). The port's atomic swap must
// RETIRE these patches, never coexist with them (WAVE2_ALLOCATOR.md Â§4).
namespace {
uintptr_t g_allocPatchAddr = 0;   uint8_t g_allocSaved[13];  bool g_allocPatched = false;
uintptr_t g_freePatchAddr  = 0;   uint8_t g_freeSaved[12];   bool g_freePatched  = false;
}

namespace {

typedef void* (*AllocFn)(void* self, uintptr_t size, uintptr_t align);

// Points into the executable trampoline; runs the original Allocate body.
AllocFn g_origAllocate = nullptr;

// ---------------------------------------------------------------------------
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

// Detour target. Win64 ABI: self=RCX, size=RDX, align=R8 — passed straight
// through to the trampoline (same registers), so the real body sees its args.
//
// The CS is the allocator's own at self+0x640 — the exact one Free/MemFree take
// (MemFree uses MemoryAllocator::_singleton+0x640, and for the singleton
// allocator that every real allocation runs on, self == _singleton). Taking it
// here makes Allocate and Free mutually exclusive. Read from `self` per call so
// there is no dependency on when the allocator singleton global is populated
// (it is set by the Engine ctor, after this module installs).
void* AllocateLocked(void* self, uintptr_t size, uintptr_t align) {
    LPCRITICAL_SECTION cs =
        reinterpret_cast<LPCRITICAL_SECTION>(reinterpret_cast<uint8_t*>(self) + 0x640);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-alloc");
    void* r = g_origAllocate(self, size, align);
    if (g_allocShadowOnAlloc) g_allocShadowOnAlloc(self, size, align, r);
    if (StatsOn()) {
        void* caller = __builtin_return_address(0);
        StatsRecordAlloc(r, size, caller);
        if (g_bigThreshold && size >= g_bigThreshold) {
            fprintf(stderr, "[ALLOCSTATS] BIG alloc %llu MB (%llu bytes) caller=%p -> %p\n",
                    (unsigned long long)(size >> 20), (unsigned long long)size, caller, r);
            fflush(stderr);
        }
        StatsMaybeReport();
    }
    LeaveCriticalSection(cs);
    return r;
}

bool LockDisabledByEnv() {
    const char* e = getenv("BANISHED_ALLOC_LOCK");
    return (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F'));
}

// --- Free lock -------------------------------------------------------------
// System::MemoryAllocator::Free (Runtime 0x1800c6c00) mutates the same
// boundary-tag free-list (Link/UnlinkFreeTag) but takes NO critical section —
// only MemFree (the static wrapper) does. Runtime-internal code calls the raw
// Free directly (bypassing MemFree), so a background thread freeing races the
// now-locked Allocate on the main thread → free-list corruption (the residual
// intermittent menu crash that the Allocate lock alone doesn't cover).
//
// We can't reuse the Allocate patch verbatim: Free's prologue is
//   48 85 D2            TEST RDX,RDX
//   0F 84 4F 02 00 00   JZ  Free+0x258      (rel32 — null-ptr early-out)
//   48 8B C4            MOV RAX,RSP          (frame-pointer setup)
// The rel32 JZ can't be relocated by a plain byte-copy. But our wrapper does
// the null check itself, so the trampoline never needs the JZ: it only
// reproduces `MOV RAX,RSP` and jumps to Free+12 (the rest of the body, at its
// original address, with intact .pdata unwind). The body's prologue/epilogue
// are RSP-relative and balanced, so running it one call-frame deeper is safe.
// CRITICAL_SECTION is recursive, so MemFree→Free re-entry just bumps the count.
typedef void (*FreeFn)(void* self, void* ptr);
FreeFn g_origFreeBody = nullptr;  // trampoline: MOV RAX,RSP; JMP Free+12

void FreeLocked(void* self, void* ptr) {
    if (!ptr) return;  // reproduce the original TEST RDX,RDX; JZ early-out
    LPCRITICAL_SECTION cs =
        reinterpret_cast<LPCRITICAL_SECTION>(reinterpret_cast<uint8_t*>(self) + 0x640);
    EnterCriticalSection(cs);
    RedzoneAlloc_TeardownOp("teardown-free");
    if (StatsOn()) StatsRecordFree(ptr);
    g_origFreeBody(self, ptr);
    if (g_allocShadowOnFree) g_allocShadowOnFree(self, ptr);
    LeaveCriticalSection(cs);
}

} // namespace

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

// Remove both prologue patches and hand the two entry points back to retail.
// Called by the atomic-swap installer before it detours the same addresses:
// the two must never coexist (WAVE2_ALLOCATOR.md section 4). Returns true if
// anything was actually retired.
//
// Safe here and nowhere else: this runs during single-threaded boot, before
// Engine::Startup creates the loader thread, so no thread can be inside the
// wrappers while their patches are removed.
bool AllocatorLockRetireForPort() {
    bool retired = false;
    DWORD oldProtect = 0;
    if (g_allocPatched && g_allocPatchAddr) {
        if (VirtualProtect(reinterpret_cast<void*>(g_allocPatchAddr), sizeof(g_allocSaved),
                           PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(reinterpret_cast<void*>(g_allocPatchAddr), g_allocSaved, sizeof(g_allocSaved));
            VirtualProtect(reinterpret_cast<void*>(g_allocPatchAddr), sizeof(g_allocSaved),
                           oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(),
                                  reinterpret_cast<void*>(g_allocPatchAddr), sizeof(g_allocSaved));
            g_allocPatched = false;
            retired = true;
        }
    }
    if (g_freePatched && g_freePatchAddr) {
        if (VirtualProtect(reinterpret_cast<void*>(g_freePatchAddr), sizeof(g_freeSaved),
                           PAGE_EXECUTE_READWRITE, &oldProtect)) {
            memcpy(reinterpret_cast<void*>(g_freePatchAddr), g_freeSaved, sizeof(g_freeSaved));
            VirtualProtect(reinterpret_cast<void*>(g_freePatchAddr), sizeof(g_freeSaved),
                           oldProtect, &oldProtect);
            FlushInstructionCache(GetCurrentProcess(),
                                  reinterpret_cast<void*>(g_freePatchAddr), sizeof(g_freeSaved));
            g_freePatched = false;
            retired = true;
        }
    }
    if (retired) {
        fprintf(stderr, "[AllocLock] patches RETIRED for the ported allocator "
                "(Allocate=%d Free=%d)\n", g_allocPatched ? 0 : 1, g_freePatched ? 0 : 1);
        fflush(stderr);
    }
    return retired;
}

bool InstallAllocatorLock() {
    if (LockDisabledByEnv()) {
        fprintf(stderr, "[AllocLock] disabled via BANISHED_ALLOC_LOCK=0 — allocator stays racy\n");
        fflush(stderr);
        return true;  // intentional skip
    }

    uintptr_t allocate = ResolveAddr(Module::Runtime, 0x1800c6e60);
    if (!allocate) {
        fprintf(stderr, "[AllocLock] could not resolve Allocate (0x1800c6e60)\n");
        fflush(stderr);
        return false;
    }

    // Validate the prologue before touching it (refuse on any mismatch).
    static const uint8_t expected[13] = {
        0x40,0x53,0x57,0x41,0x54,0x41,0x56,0x41,0x57,0x48,0x83,0xEC,0x20
    };
    if (memcmp(reinterpret_cast<void*>(allocate), expected, sizeof(expected)) != 0) {
        fprintf(stderr, "[AllocLock] Allocate prologue at %p does not match expected — refusing to patch\n",
                reinterpret_cast<void*>(allocate));
        fflush(stderr);
        return false;
    }

    // Build the trampoline: 13 original bytes + absolute JMP to Allocate+13.
    uint8_t* tramp = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) {
        fprintf(stderr, "[AllocLock] VirtualAlloc for trampoline failed (err=%lu)\n", GetLastError());
        fflush(stderr);
        return false;
    }
    memcpy(tramp, expected, 13);
    // FF 25 00000000 : JMP qword ptr [RIP+0]; followed by the 8-byte target.
    tramp[13] = 0xFF; tramp[14] = 0x25;
    tramp[15] = 0x00; tramp[16] = 0x00; tramp[17] = 0x00; tramp[18] = 0x00;
    uint64_t cont = static_cast<uint64_t>(allocate + 13);
    memcpy(tramp + 19, &cont, 8);
    FlushInstructionCache(GetCurrentProcess(), tramp, 27);
    g_origAllocate = reinterpret_cast<AllocFn>(tramp);
    memcpy(g_allocSaved, expected, sizeof(g_allocSaved));
    g_allocPatchAddr = allocate;

    // Patch Allocate[0..12]: MOV RAX, imm64; JMP RAX; NOP  (13 bytes).
    uint8_t patch[13];
    patch[0] = 0x48; patch[1] = 0xB8;
    uint64_t wrapper = reinterpret_cast<uint64_t>(&AllocateLocked);
    memcpy(patch + 2, &wrapper, 8);
    patch[10] = 0xFF; patch[11] = 0xE0;  // JMP RAX
    patch[12] = 0x90;                    // NOP padding to the 13-byte boundary

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(allocate), 13, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        fprintf(stderr, "[AllocLock] VirtualProtect failed (err=%lu)\n", GetLastError());
        fflush(stderr);
        return false;
    }
    memcpy(reinterpret_cast<void*>(allocate), patch, 13);
    VirtualProtect(reinterpret_cast<void*>(allocate), 13, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(allocate), 13);
    g_allocPatched = true;

    fprintf(stderr, "[AllocLock] MemoryAllocator::Allocate is now thread-safe "
            "(takes self+0x640, tramp=%p, wrapper=%p)\n",
            tramp, reinterpret_cast<void*>(&AllocateLocked));
    fflush(stderr);
    return true;
}

bool InstallFreeLock() {
    if (LockDisabledByEnv()) return true;  // same gate as the Allocate lock
    {
        const char* e = getenv("BANISHED_FREE_LOCK");
        if (e && (e[0] == '0' || e[0] == 'f' || e[0] == 'F')) {
            fprintf(stderr, "[AllocLock] Free lock disabled via BANISHED_FREE_LOCK=0\n");
            fflush(stderr);
            return true;
        }
    }

    uintptr_t freeAddr = ResolveAddr(Module::Runtime, 0x1800c6c00);
    if (!freeAddr) {
        fprintf(stderr, "[AllocLock] could not resolve Free (0x1800c6c00)\n");
        fflush(stderr);
        return false;
    }

    // Validate the 12-byte prologue we overwrite: TEST RDX,RDX; JZ rel32; MOV RAX,RSP.
    static const uint8_t expected[12] = {
        0x48,0x85,0xD2, 0x0F,0x84,0x4F,0x02,0x00,0x00, 0x48,0x8B,0xC4
    };
    if (memcmp(reinterpret_cast<void*>(freeAddr), expected, sizeof(expected)) != 0) {
        fprintf(stderr, "[AllocLock] Free prologue at %p does not match expected — refusing to patch\n",
                reinterpret_cast<void*>(freeAddr));
        fflush(stderr);
        return false;
    }

    // Trampoline: MOV RAX,RSP (reproduce the clobbered frame-pointer setup) then
    // JMP to Free+12 (continue the body at its original address). The wrapper
    // already handled the null case, so the JZ at +3 is intentionally dropped.
    uint8_t* tramp = reinterpret_cast<uint8_t*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) {
        fprintf(stderr, "[AllocLock] VirtualAlloc for Free trampoline failed (err=%lu)\n", GetLastError());
        fflush(stderr);
        return false;
    }
    tramp[0] = 0x48; tramp[1] = 0x8B; tramp[2] = 0xC4;        // MOV RAX,RSP
    tramp[3] = 0xFF; tramp[4] = 0x25;                          // JMP qword ptr [RIP+0]
    tramp[5] = 0x00; tramp[6] = 0x00; tramp[7] = 0x00; tramp[8] = 0x00;
    uint64_t cont = static_cast<uint64_t>(freeAddr + 12);
    memcpy(tramp + 9, &cont, 8);
    FlushInstructionCache(GetCurrentProcess(), tramp, 17);
    g_origFreeBody = reinterpret_cast<FreeFn>(tramp);
    memcpy(g_freeSaved, expected, sizeof(g_freeSaved));
    g_freePatchAddr = freeAddr;

    // Patch Free[0..11]: MOV RAX, &FreeLocked; JMP RAX  (12 bytes, RAX not an input).
    uint8_t patch[12];
    patch[0] = 0x48; patch[1] = 0xB8;
    uint64_t wrapper = reinterpret_cast<uint64_t>(&FreeLocked);
    memcpy(patch + 2, &wrapper, 8);
    patch[10] = 0xFF; patch[11] = 0xE0;  // JMP RAX

    DWORD oldProtect = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(freeAddr), 12, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        fprintf(stderr, "[AllocLock] VirtualProtect (Free) failed (err=%lu)\n", GetLastError());
        fflush(stderr);
        return false;
    }
    memcpy(reinterpret_cast<void*>(freeAddr), patch, 12);
    VirtualProtect(reinterpret_cast<void*>(freeAddr), 12, oldProtect, &oldProtect);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(freeAddr), 12);
    g_freePatched = true;

    fprintf(stderr, "[AllocLock] MemoryAllocator::Free is now thread-safe "
            "(takes self+0x640, tramp=%p, wrapper=%p)\n",
            tramp, reinterpret_cast<void*>(&FreeLocked));
    fflush(stderr);
    return true;
}

