// standalone/redzone_alloc.cpp — see redzone_alloc.h for design rationale.


#include "redzone_alloc.h"
#include "engine_funcs.h"
#include "resolve.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

// The perf build strips every unqualified printf (log_strip.h) — which silenced ALL of
// this file's reports in the 800x10 teardown hunt (empty standalone_debug.log while the
// process died in the allocator). Diagnostics here must survive the strip: route them to
// stderr, which the harness captures per run.
#ifdef printf
#undef printf
#endif
#define printf(...) std::fprintf(stderr, __VA_ARGS__)

namespace {

// Tail-redzone padding (bytes) added to each allocation. DEFAULT 0 — padding
// every block shifts the whole engine heap layout, which HIDES layout-dependent
// free-list corruption (the OOB write lands on padding instead of a real tag).
// So by default we add NO padding (identical layout to an uninstrumented run →
// the bug still reproduces) and rely on the read-only HeapCheck walker to catch
// the corrupted boundary tag. Set BANISHED_REDZONE_PAD=<bytes> to re-enable the
// tail canary (useful only for contiguous overruns of OUR buffers).
size_t   g_pad      = 0;
bool     g_zeroOnAlloc = false;       // BANISHED_REDZONE_ZERO=1 diagnostic
constexpr uint8_t kCanary   = 0xAB;   // fill byte for the tail redzone
constexpr int    kBtFrames  = 8;      // captured alloc backtrace depth

struct Entry {
    size_t   reqSize;             // bytes the caller actually asked for
    uint32_t seq;                 // monotonic allocation id
    void*    bt[kBtFrames];       // alloc call stack (raw return addresses)
    uint16_t btCount;
};

// The shim's own bookkeeping lives on the CRT heap (std::unordered_map / mutex
// call malloc, NOT fn_MemAlloc), so there is no re-entrancy with the engine
// allocator we are wrapping.
std::unordered_map<void*, Entry>* g_live = nullptr;
std::mutex                         g_mutex;

fn_MemAlloc_t      g_realAlloc = nullptr;
fn_MemFreeStatic_t g_realFree  = nullptr;
bool               g_enabled   = false;
uint32_t           g_seq       = 0;

// Periodic auto-scan: validate every live canary once every g_scanEvery
// alloc/free operations, so an overflow is caught within g_scanEvery ops of the
// bad write (not only at the hand-placed RZ_CHECK checkpoints). 0 = disabled
// (checkpoints only). Set via BANISHED_REDZONE_SCAN=<N>. Heavy (O(live) per
// scan) but fine for diagnosing a one-shot menu-load corruption.
uint32_t g_scanEvery = 0;
uint32_t g_opCounter = 0;

// Watchdog thread: walks the heap in a tight loop CONCURRENTLY with everything
// else, so corruption introduced by Runtime-internal code (between our own
// alloc/free calls) is caught within ~g_watchIntervalMs of the bad write —
// before the fatal Allocate reuses the clobbered free block. Safe because the
// walk takes the allocator's own critical section. Enabled by
// BANISHED_REDZONE_WATCH=<intervalMs> (0 = tight spin with Sleep(0)).
int  g_watchIntervalMs = -1;   // -1 = disabled

// Module base for printing exe-relative backtrace addresses (so they map
// straight onto Ghidra's 0x140000000-based addresses).
uintptr_t g_exeBase = 0;

inline uint8_t* CanaryPtr(void* userPtr, size_t reqSize) {
    return reinterpret_cast<uint8_t*>(userPtr) + reqSize;
}

bool CanaryIntact(void* userPtr, size_t reqSize) {
    if (g_pad == 0) return true;  // no canary when padding disabled
    const uint8_t* c = CanaryPtr(userPtr, reqSize);
    for (size_t i = 0; i < g_pad; ++i) {
        if (c[i] != kCanary) return false;
    }
    return true;
}

// Forward decl — comprehensive boundary-tag walker (defined below). Caller
// holds g_mutex.
} // namespace
static int HeapCheckLocked(const char* tag, bool report = true);
namespace {

// Canary reports must go to stderr as well: stdout is fully buffered after the
// freopen to standalone_debug.log and an ExitProcess right after the report was
// observed to lose the whole thing (exit 0x122 with a 90-byte debug log).
void RZBoth(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    printf("%s", buf);
    fputs(buf, stderr);
    fflush(stdout);
    fflush(stderr);
}

void PrintEntry(const char* why, void* userPtr, const Entry& e) {
    // Find how far the corruption extends (first + last bad byte).
    const uint8_t* c = CanaryPtr(userPtr, e.reqSize);
    int firstBad = -1, lastBad = -1;
    for (int i = 0; i < (int)g_pad; ++i) {
        if (c[i] != kCanary) { if (firstBad < 0) firstBad = i; lastBad = i; }
    }
    RZBoth("[REDZONE] %s ptr=%p reqSize=%zu (0x%zx) seq=%u "
           "overflow bytes [%d..%d] of %zu\n",
           why, userPtr, e.reqSize, e.reqSize, e.seq,
           firstBad, lastBad, g_pad);
    // Dump the canary's first 16 bytes — the overflow payload identifies the writer.
    RZBoth("[REDZONE]   canary head: %02x %02x %02x %02x %02x %02x %02x %02x "
           "%02x %02x %02x %02x %02x %02x %02x %02x\n",
           c[0], c[1], c[2], c[3], c[4], c[5], c[6], c[7],
           c[8], c[9], c[10], c[11], c[12], c[13], c[14], c[15]);
    // First user qword = vtable for engine objects; names the corrupted block's type.
    if (!IsBadReadPtr(userPtr, 8))
        RZBoth("[REDZONE]   user+0 qword: 0x%016llx\n",
               (unsigned long long)*reinterpret_cast<uint64_t*>(userPtr));
    char bt[512]; int off = 0;
    off += snprintf(bt + off, sizeof(bt) - off, "[REDZONE]   alloc backtrace:");
    for (int i = 0; i < e.btCount && off < (int)sizeof(bt) - 32; ++i) {
        uintptr_t a = reinterpret_cast<uintptr_t>(e.bt[i]);
        if (g_exeBase && a >= g_exeBase && a < g_exeBase + 0x600000) {
            // exe-relative → Ghidra address (image base 0x140000000)
            off += snprintf(bt + off, sizeof(bt) - off, " 0x%llx",
                            (unsigned long long)(0x140000000ull + (a - g_exeBase)));
        } else {
            off += snprintf(bt + off, sizeof(bt) - off, " [%p]", e.bt[i]);
        }
    }
    RZBoth("%s\n", bt);
}

// Caller MUST hold g_mutex. Scans every live block's tail canary; logs each
// corrupted one and (unless BANISHED_REDZONE_ABORT=0) ExitProcess()es cleanly
// at the first corruption — before the bad block is ever freed, which is what
// otherwise deadlocks the allocator critical section into a hang.
// Print the CURRENT call stack (the alloc/free that triggered the scan). With
// SCAN=1 the overflow happened between the previous allocator op and this one,
// so this backtrace brackets the writer to a single code window.
void PrintHereBacktrace(const char* what) {
    void* bt[kBtFrames];
    USHORT n = CaptureStackBackTrace(2, kBtFrames, bt, nullptr);
    char buf[512]; int off = 0;
    off += snprintf(buf + off, sizeof(buf) - off, "[REDZONE]   %s backtrace:", what);
    for (USHORT i = 0; i < n && off < (int)sizeof(buf) - 32; ++i) {
        uintptr_t a = reinterpret_cast<uintptr_t>(bt[i]);
        if (g_exeBase && a >= g_exeBase && a < g_exeBase + 0x600000) {
            off += snprintf(buf + off, sizeof(buf) - off, " 0x%llx",
                            (unsigned long long)(0x140000000ull + (a - g_exeBase)));
        } else {
            off += snprintf(buf + off, sizeof(buf) - off, " [%p]", bt[i]);
        }
    }
    RZBoth("%s\n", buf);
}

int ValidateAllLocked(const char* tag) {
    int bad = 0;
    for (auto& kv : *g_live) {
        if (!CanaryIntact(kv.first, kv.second.reqSize)) {
            if (bad == 0) {
                RZBoth("[REDZONE] ==== corruption detected at checkpoint '%s' ====\n",
                       tag ? tag : "?");
            }
            PrintEntry("CORRUPT", kv.first, kv.second);
            ++bad;
        }
    }
    if (bad) {
        PrintHereBacktrace("trip-site");
        RZBoth("[REDZONE] checkpoint '%s': %d corrupted block(s), %zu live\n",
               tag ? tag : "?", bad, g_live->size());
        const char* noAbort = getenv("BANISHED_REDZONE_ABORT");
        if (!(noAbort && noAbort[0] == '0')) {
            RZBoth("[REDZONE] aborting cleanly at first corruption "
                   "(set BANISHED_REDZONE_ABORT=0 to continue)\n");
            ExitProcess(0x122);  // distinctive exit code for "redzone tripped"
        }
    }
    return bad;
}

void* ShimAlloc(void* singleton, uint64_t size, int alignment) {
    void* p = g_realAlloc(singleton, size + g_pad, alignment);
    if (!p) return p;  // alloc failed — nothing to stamp
    // Diagnostic (BANISHED_REDZONE_ZERO=1): zero-fill every allocation we hand
    // out. The engine allocator is a boundary-tag free-list over VirtualAlloc
    // and does NOT re-zero reused freed blocks, so a reused block returns with
    // stale bytes. If our reimpl reads an uninitialized slot as a pointer (e.g.
    // a not-yet-deserialized External*/Filename) it then writes through garbage
    // → heap corruption. Zeroing here (PAD=0, so NO layout change) tests that
    // "uninitialized/stale pointer" hypothesis: if the intermittent menu crash
    // vanishes with this on, the bug is a stale-read; if it persists, it's a
    // genuine OOB overrun or a Runtime-side allocation we don't intercept.
    if (g_zeroOnAlloc) memset(p, 0, (size_t)size);
    if (g_pad) memset(CanaryPtr(p, (size_t)size), kCanary, g_pad);
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        Entry e{};
        e.reqSize = (size_t)size;
        e.seq     = ++g_seq;
        e.btCount = (uint16_t)CaptureStackBackTrace(1, kBtFrames, e.bt, nullptr);
        (*g_live)[p] = e;
        if (g_scanEvery && (++g_opCounter % g_scanEvery) == 0) {
            ValidateAllLocked("auto-scan(alloc)");
            HeapCheckLocked("auto-scan(alloc)");
        }
    }
    return p;
}

// Static-signature wrappers for the System::MemoryAllocator::MemAlloc/MemFree
// dispatch slots (0x140152b60/b68). Several reimpls (tool_base_highlight*.cpp,
// work_act_*.cpp) allocate/free through those static exports, NOT through
// fn_MemAlloc/fn_MemFreeStatic. Un-shimmed, an alloc through one channel and a
// free through the other leaves a stale g_live entry; when the engine reuses
// that memory the canary scan reports a PHANTOM overflow (seen twice as a
// "corrupt decal vertex pool" at town load — alloc via f8d0's fn_MemAlloc,
// free via ace40's static MemFree). Rewiring the slots at install time makes
// both channels shimmed and the tracking coherent.
void ShimFree(void* ptr);
void* ShimAllocStatic(uint64_t size, int alignment) {
    return ShimAlloc(g_pMemAllocSingleton, size, alignment);
}
void ShimFreeStatic(void* ptr) {
    ShimFree(ptr);
}

void ShimFree(void* ptr) {
    if (ptr) {
        std::lock_guard<std::mutex> lk(g_mutex);
        auto it = g_live->find(ptr);
        if (it != g_live->end()) {
            if (!CanaryIntact(ptr, it->second.reqSize)) {
                PrintEntry("CORRUPT-ON-FREE", ptr, it->second);
            }
            g_live->erase(it);
        }
        // The crash always manifests inside the allocator's Free coalescing
        // (it follows a clobbered boundary tag), so frees participate in the
        // same throttled op counter as allocs. Scanning EVERY free was O(live)
        // per free and made loading take minutes once both allocator channels
        // were shimmed (~20k live blocks × constant frees).
        if (g_scanEvery && (++g_opCounter % g_scanEvery) == 0) {
            ValidateAllLocked("auto-scan(pre-free)");
            HeapCheckLocked("auto-scan(pre-free)");
        }
    }
    g_realFree(ptr);
}

} // namespace

static DWORD WINAPI RedzoneWatchdog(LPVOID);
static DWORD  g_mainTid    = 0;      // thread that ran Install (= the sim thread)
static HANDLE g_mainThread = nullptr;

void RedzoneAlloc_Install() {
    const char* env = getenv("BANISHED_REDZONE");
    if (!env || env[0] == '0' || env[0] == 'f' || env[0] == 'F') {
        return;  // disabled — leave the real allocator pointers in place
    }
    // BANISHED_REDZONE=watch — WATCH-ONLY mode: no alloc/free shim at all (the shim's
    // per-op bookkeeping changes timing enough to hide a cross-thread race; the 800x10
    // teardown corruption reproduces plain and vanished under the shim). Only the
    // heap-walking watchdog thread runs, so the corrupt tag is reported within
    // ~BANISHED_REDZONE_WATCH ms of the write together with EVERY thread's stack.
    if (env[0] == 'w' || env[0] == 'W') {
        g_live = new std::unordered_map<void*, Entry>();
        g_exeBase = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        g_mainTid = GetCurrentThreadId();
        g_mainThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                                  FALSE, g_mainTid);
        if (const char* w = getenv("BANISHED_REDZONE_WATCH")) g_watchIntervalMs = atoi(w);
        if (g_watchIntervalMs < 0) g_watchIntervalMs = 1;
        g_enabled = true;
        printf("[REDZONE] WATCH-ONLY mode: no shim, watchdog=%dms\n", g_watchIntervalMs);
        fflush(stdout);
        CreateThread(nullptr, 0, &RedzoneWatchdog, nullptr, 0, nullptr);
        return;
    }
    if (!fn_MemAlloc || !fn_MemFreeStatic) {
        printf("[REDZONE] cannot install — fn_MemAlloc=%p fn_MemFreeStatic=%p\n",
               (void*)fn_MemAlloc, (void*)fn_MemFreeStatic);
        fflush(stdout);
        return;
    }
    g_live = new std::unordered_map<void*, Entry>();
    g_live->reserve(8192);
    g_realAlloc = fn_MemAlloc;
    g_realFree  = fn_MemFreeStatic;
    g_exeBase   = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if (const char* scan = getenv("BANISHED_REDZONE_SCAN")) {
        int n = atoi(scan);
        if (n > 0) g_scanEvery = (uint32_t)n;
    }
    if (const char* pad = getenv("BANISHED_REDZONE_PAD")) {
        int n = atoi(pad);
        if (n > 0) g_pad = (size_t)((n + 7) & ~7);  // 8-byte multiple
    }
    if (const char* w = getenv("BANISHED_REDZONE_WATCH")) {
        g_watchIntervalMs = atoi(w);  // 0 = tight spin
    }
    if (const char* z = getenv("BANISHED_REDZONE_ZERO")) {
        g_zeroOnAlloc = (z[0] && z[0] != '0' && z[0] != 'f' && z[0] != 'F');
    }
    fn_MemAlloc      = &ShimAlloc;
    fn_MemFreeStatic = &ShimFree;
    // Rewire the static MemAlloc/MemFree dispatch slots too (see ShimAllocStatic
    // comment). Consumers snapshot *(fn**)slot lazily at first use, which happens
    // after this install, so overwriting the slot storage here covers them all.
    {
        uintptr_t sa = ResolveAddr(Module::Exe, 0x140152b60);
        uintptr_t sf = ResolveAddr(Module::Exe, 0x140152b68);
        if (sa) *reinterpret_cast<void**>(sa) = reinterpret_cast<void*>(&ShimAllocStatic);
        if (sf) *reinterpret_cast<void**>(sf) = reinterpret_cast<void*>(&ShimFreeStatic);
        printf("[REDZONE] static MemAlloc/MemFree slots rewired: b60=%p b68=%p\n",
               (void*)sa, (void*)sf);
    }
    g_mainTid = GetCurrentThreadId();
    g_mainThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                              FALSE, g_mainTid);
    g_enabled = true;
    printf("[REDZONE] installed: tailPad=%zu bytes (0=native layout), zeroOnAlloc=%d, "
           "autoScanEvery=%u, watchdog=%dms, real alloc=%p free=%p exeBase=%p\n",
           g_pad, (int)g_zeroOnAlloc, g_scanEvery, g_watchIntervalMs,
           (void*)g_realAlloc, (void*)g_realFree, (void*)g_exeBase);
    fflush(stdout);

    if (g_watchIntervalMs >= 0) {
        CreateThread(nullptr, 0, &RedzoneWatchdog, nullptr, 0, nullptr);
    }
}

bool RedzoneAlloc_Enabled() { return g_enabled; }

// Forward decl — RZOut is defined in a later anonymous-namespace block (anon
// namespaces merge across the TU, so this refers to the same function).
namespace { void RZOut(const char* fmt, ...); }

int RedzoneAlloc_ValidateAll(const char* tag) {
    if (!g_enabled) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    return ValidateAllLocked(tag);
}

// Free-list LINK validation, callable at a code checkpoint (e.g. between
// deserialize steps) to find the exact step that corrupts a freed block's
// forward link. Walks every size bucket's free chain and validates each
// signed-41-bit relative forward link points back into a committed range.
// On the first corrupt link it prints the checkpoint tag + the clobbered
// block body, then aborts (exit 0x124) so the tag names the culprit step.
// Quiescent-allocator assumption: only call from single-threaded reimpl code
// (no concurrent Allocate mutating the free list). Enabled only under
// BANISHED_REDZONE (so production runs are unaffected).
extern "C" int RedzoneAlloc_CheckLinks(const char* tag) {
    if (!g_enabled || !g_pMemAllocSingleton) return 0;
    uint8_t* alloc = reinterpret_cast<uint8_t*>(g_pMemAllocSingleton);
    if (IsBadReadPtr(alloc, 0x18)) return 0;
    uint8_t* ranges   = *reinterpret_cast<uint8_t**>(alloc + 0x00);
    int      rangeCnt = *reinterpret_cast<int*>(alloc + 0x08);
    if (!ranges || rangeCnt <= 0 || rangeCnt >= 4096) return 0;

    auto inAnyRange = [&](uint8_t* a) -> bool {
        for (int r = 0; r < rangeCnt; ++r) {
            uint8_t* rg = ranges + (size_t)r * 0x18;
            if (IsBadReadPtr(rg, 0x18)) continue;
            uint8_t* s = *reinterpret_cast<uint8_t**>(rg + 0x00);
            uint8_t* e = *reinterpret_cast<uint8_t**>(rg + 0x08);
            if (s && e && a >= s && a < e) return true;
        }
        return false;
    };

    for (int bucket = 0; bucket < 0xC0; ++bucket) {
        uint8_t* curFT = *reinterpret_cast<uint8_t**>(alloc + 0x28 + (size_t)bucket * 8);
        uint8_t* prevFT = nullptr;
        int guard = 0;
        while (curFT && ++guard < 2000000) {
            uint8_t* block = curFT - 8;
            if (IsBadReadPtr(curFT, 8) || !inAnyRange(block)) {
                RZOut("[REDZONE-LINK] CORRUPTION first seen at checkpoint '%s' "
                      "(bucket %d): free node @%p (block %p) out of range; "
                      "prev node @%p\n",
                      tag ? tag : "?", bucket, (void*)curFT, (void*)block, (void*)prevFT);
                fflush(stdout);
                ExitProcess(0x124);
            }
            uint64_t link = *reinterpret_cast<uint64_t*>(curFT);
            int64_t off = (int64_t)(link << 23) >> 23;
            if (off == 0) break;
            uint8_t* next = curFT + off;
            if (IsBadReadPtr(next, 8) || !inAnyRange(next - 8)) {
                RZOut("[REDZONE-LINK] CORRUPTION first seen at checkpoint '%s' "
                      "(bucket %d): free block @%p forward link qword=%016llx "
                      "off=%lld -> next=%p (out of range)\n",
                      tag ? tag : "?", bucket, (void*)block,
                      (unsigned long long)link, (long long)off, (void*)next);
                fflush(stdout);
                ExitProcess(0x124);
            }
            prevFT = curFT;
            curFT = next;
        }
    }
    return 0;
}

// ============================================================
// Heap-integrity walker over the engine allocator's boundary tags.
// ============================================================
// Catches corruption of ANY block (live OR free, ours OR Runtime's) — unlike
// the tail canaries which only cover our live blocks. Layout (from the Allocate
// decompile at Runtime 0x1800c6e60):
//   MemoryAllocator* this  = g_pMemAllocSingleton
//     +0x00  MemoryRange*  ranges            (array)
//     +0x08  int           rangeCount
//     +0x640 CRITICAL_SECTION                 (alloc/free lock)
//   MemoryRange (stride 0x18):
//     +0x00  uint8_t*      firstBlock
//     +0x08  uint8_t*      end (one past last block)
//     +0x10  uint64_t      committedBytes
//   Block: tag = *(uint64_t*)block;  blockBytes = (tag & 0x1ffffffffff) * 4;
//          next = block + blockBytes;  user = block + 8.
// A block is corrupt if blockBytes < 8 or block+blockBytes overruns the range.
namespace {
bool ReadablePtr(const void* p, size_t n) {
    return p && !IsBadReadPtr(p, n);
}

// Optional secondary output sink for the crash-context heap dump. The crash
// handler sets this to a write-through (FILE_FLAG_WRITE_THROUGH) logger so the
// dump's findings survive even if a re-entrant fault kills the process
// mid-walk. printf-to-stdout (debug.log) is unbuffered but was observed to
// produce NO output in crash context (the dump faults before its first line
// commits), so this sink is the reliable path. RZOut() emits to BOTH.
void (*g_rzSink)(const char*) = nullptr;

void RZOut(const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return;
    printf("%s", buf);
    if (g_rzSink) g_rzSink(buf);
}

// Resolve a code/vtable address to "module.dll+0xOFF" for identifying the
// owning class. Engine objects store their vtable pointer at +0x00, so dumping
// the preceding block's first qword and resolving it names the overflower.
void PrintModOff(const char* label, uint64_t addr) {
    HMODULE mod = nullptr;
    if (addr && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)addr, &mod) && mod) {
        char path[MAX_PATH] = {0};
        GetModuleFileNameA(mod, path, MAX_PATH);
        const char* nm = strrchr(path, '\\');
        RZOut("[REDZONE-HEAP]   %s = %s+0x%llX\n",
               label, nm ? nm + 1 : path,
               (unsigned long long)(addr - (uintptr_t)mod));
    } else {
        RZOut("[REDZONE-HEAP]   %s = 0x%016llx (not in a module)\n",
               label, (unsigned long long)addr);
    }
}
}

// Core walker. Caller MUST hold g_mutex (it reads g_live). Takes the
// allocator's own critical section internally. When report==false it only
// detects + records the first bad block in g_lastBadBlock (no print, no abort)
// — used by the watchdog to confirm a corruption persists across two walks
// before trusting it (Allocate runs WITHOUT the allocator lock, so a single
// walk can momentarily observe a mid-flight tag = false positive).
static uint8_t* g_lastBadBlock = nullptr;
extern "C" int AllocStats_Lookup(void*, uint64_t*, void**, void**);   // allocator_lock.cpp
static bool   g_reportedOnce = false;
extern "C" void StressHarness_Context(char* buf, int n);

// Print the MAIN thread's stack (from another thread: suspend, read context,
// unwind) so a watchdog detection names what the sim was executing when the
// corrupt tag was first seen. Same unwind walk as the crash handler.
static void DumpOneThreadStack(HANDLE th, DWORD tid, const char* label) {
    if (SuspendThread(th) == (DWORD)-1) {
        printf("[REDZONE-HEAP] SuspendThread(%s tid=%lu) failed err=%lu\n", label, tid, GetLastError());
        return;
    }
    CONTEXT ctx; memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_FULL;
    if (GetThreadContext(th, &ctx)) {
        printf("[REDZONE-HEAP] %s thread tid=%lu stack at detection:\n", label, tid);
        for (int frame = 0; frame < 40; frame++) {
            HMODULE mod = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)(uintptr_t)ctx.Rip, &mod) && mod) {
                char path[MAX_PATH] = {0};
                GetModuleFileNameA(mod, path, MAX_PATH);
                const char* nm = strrchr(path, '\\');
                printf("[REDZONE-HEAP]   #%-2d %-18s + 0x%-8llX\n", frame, nm ? nm + 1 : path,
                       (unsigned long long)((uintptr_t)ctx.Rip - (uintptr_t)mod));
            } else {
                printf("[REDZONE-HEAP]   #%-2d ??? @ %p\n", frame, (void*)ctx.Rip);
            }
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
            if (!fn) {
                uintptr_t* sp = reinterpret_cast<uintptr_t*>(ctx.Rsp);
                if (IsBadReadPtr(sp, sizeof(uintptr_t))) break;
                ctx.Rip = *sp; ctx.Rsp += 8;
                if (!ctx.Rip) break;
                continue;
            }
            PVOID handlerData = nullptr; ULONG64 est = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fn, &ctx, &handlerData, &est, nullptr);
            if (!ctx.Rip) break;
        }
    } else {
        printf("[REDZONE-HEAP] GetThreadContext(%s) failed err=%lu\n", label, GetLastError());
    }
    ResumeThread(th);
}

// Every thread of the process except the caller: the corrupter may be the pathfinder
// worker, not the sim thread, so one stack is not enough to name it.
static void DumpMainThreadStack() {
    const DWORD self = GetCurrentThreadId();
    if (g_mainThread && self != g_mainTid) DumpOneThreadStack(g_mainThread, g_mainTid, "MAIN");
    else {
        printf("[REDZONE-HEAP] detected on the main thread itself — caller backtrace:\n");
        void* bt[32];
        USHORT n = CaptureStackBackTrace(0, 32, bt, nullptr);
        for (USHORT i = 0; i < n; ++i) {
            HMODULE mod = nullptr;
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)bt[i], &mod) && mod) {
                char path[MAX_PATH] = {0};
                GetModuleFileNameA(mod, path, MAX_PATH);
                const char* nm = strrchr(path, '\\');
                printf("[REDZONE-HEAP]   #%-2d %-18s + 0x%llX\n", i, nm ? nm + 1 : path,
                       (unsigned long long)((uintptr_t)bt[i] - (uintptr_t)mod));
            } else printf("[REDZONE-HEAP]   #%-2d ??? @ %p\n", i, bt[i]);
        }
    }
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        THREADENTRY32 te; te.dwSize = sizeof(te);
        const DWORD pid = GetCurrentProcessId();
        if (Thread32First(snap, &te)) {
            do {
                if (te.th32OwnerProcessID != pid || te.th32ThreadID == self || te.th32ThreadID == g_mainTid)
                    continue;
                HANDLE th = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
                                       FALSE, te.th32ThreadID);
                if (!th) continue;
                DumpOneThreadStack(th, te.th32ThreadID, "OTHER");
                CloseHandle(th);
            } while (Thread32Next(snap, &te));
        }
        CloseHandle(snap);
    }
    char sim[256] = {0};
    StressHarness_Context(sim, sizeof(sim));
    printf("[REDZONE-HEAP] sim context: %s\n", sim);
    fflush(stdout);
}

static int HeapCheckLocked(const char* tag, bool report) {
    if (!g_enabled || !g_pMemAllocSingleton) return 0;
    // BANISHED_REDZONE_ABORT=0: after a report, keep quiet about THAT block (it stays
    // corrupt) but still report a different first-bad block (up to 8 reports).
    static int s_reports = 0;
    static uint8_t* s_lastReported = nullptr;
    if (g_reportedOnce && s_reports >= 8) return 0;
    uint8_t* alloc = reinterpret_cast<uint8_t*>(g_pMemAllocSingleton);

    // Serialize against concurrent alloc/free via the allocator's own lock.
    CRITICAL_SECTION* cs = reinterpret_cast<CRITICAL_SECTION*>(alloc + 0x640);
    EnterCriticalSection(cs);

    int bad = 0;
    uint8_t* badBlock = nullptr;   // first block whose tag is corrupt
    uint8_t* badPrev  = nullptr;   // physically-preceding block (overflow source)
    uint64_t badTag   = 0;
    size_t   badBytes = 0;
    int      badRange = -1;

    uint8_t* ranges   = *reinterpret_cast<uint8_t**>(alloc + 0x00);
    int      rangeCnt = *reinterpret_cast<int*>(alloc + 0x08);
    if (ranges && rangeCnt > 0 && rangeCnt < 4096) {
        for (int r = 0; r < rangeCnt && !bad; ++r) {
            uint8_t* rng   = ranges + (size_t)r * 0x18;
            uint8_t* start = *reinterpret_cast<uint8_t**>(rng + 0x00);
            uint8_t* end   = *reinterpret_cast<uint8_t**>(rng + 0x08);
            if (!start || !end || end <= start) continue;

            uint8_t* prev = nullptr;
            uint8_t* p = start;
            while (p < end) {
                if (!ReadablePtr(p, 8)) {
                    badBlock = p; badPrev = prev; badRange = r; ++bad; break;
                }
                uint64_t blkTag = *reinterpret_cast<uint64_t*>(p);
                size_t blkBytes = (size_t)(blkTag & 0x1ffffffffffULL) * 4;
                if (blkBytes < 8 || p + blkBytes > end) {
                    badBlock = p; badPrev = prev; badRange = r;
                    badTag = blkTag; badBytes = blkBytes; ++bad; break;
                }
                prev = p;
                p += blkBytes;
            }
        }
    } else {
        printf("[REDZONE-HEAP] suspicious allocator state ranges=%p cnt=%d — skipping\n",
               (void*)ranges, rangeCnt);
    }

    LeaveCriticalSection(cs);

    g_lastBadBlock = bad ? badBlock : nullptr;
    if (bad && !report) {
        return bad;  // detect-only (watchdog confirmation pass) — no print/abort
    }
    if (bad && g_reportedOnce && badBlock == s_lastReported) return bad;   // already reported

    if (bad) {
        ++s_reports; s_lastReported = badBlock;
        printf("[REDZONE-HEAP] ==== corruption at checkpoint '%s' (seq=%u) ====\n",
               tag ? tag : "?", g_seq);
        printf("[REDZONE-HEAP] range %d: bad block @%p tag=0x%016llx size=%zu (0x%zx)\n",
               badRange, (void*)badBlock, (unsigned long long)badTag, badBytes, badBytes);

        // (1) Physically-preceding block = likely overflow source. Dump its
        // raw header so its vtable pointer (user+0x00) names the overflowing
        // class. Also check our table (stale-flagged) for an alloc backtrace.
        if (badPrev && ReadablePtr(badPrev, 0x20)) {
            void* prevUser = badPrev + 8;
            const uint64_t* q = reinterpret_cast<const uint64_t*>(badPrev);
            printf("[REDZONE-HEAP] preceding block @%p raw: "
                   "tag=%016llx user[0]=%016llx user[1]=%016llx user[2]=%016llx\n",
                   (void*)badPrev,
                   (unsigned long long)q[0], (unsigned long long)q[1],
                   (unsigned long long)q[2], (unsigned long long)q[3]);
            // user+0x00 (=q[1]) is the vtable for an engine object — resolve it.
            PrintModOff("preceding user vtable[user+0]", q[1]);
            auto it = g_live->find(prevUser);
            if (it != g_live->end()) {
                size_t recBlock = ((it->second.reqSize + g_pad + 7) & ~size_t(7)) + 8;
                bool reaches = (badPrev + recBlock) >= badBlock;
                printf("[REDZONE-HEAP] preceding block IS OURS (user=%p) "
                       "recBlockBytes~%zu %s:\n", prevUser, recBlock,
                       reaches ? "" : "[STALE? footprint doesn't reach bad block]");
                PrintEntry("PRECEDING-OURS", prevUser, it->second);
            } else {
                printf("[REDZONE-HEAP] preceding block NOT in our table "
                       "(Runtime-allocated, or freed-stale)\n");
            }
        }

        // (1b) ALLOCSTATS owner lookup (BANISHED_ALLOC_STATS=1 records size + alloc
        // caller for every Allocate, including Runtime-internal ones): who owns the
        // preceding block and the bad block?
        {
            uint64_t sz = 0; void* caller = nullptr; void* actx = nullptr;
            if (badPrev && AllocStats_Lookup(badPrev + 8, &sz, &caller, &actx)) {
                printf("[REDZONE-HEAP] preceding block ALLOCSTATS: size=%llu ctx(array header)=%p caller=",
                       (unsigned long long)sz, actx);
                PrintModOff("alloc-caller", (uint64_t)(uintptr_t)caller);
                if (actx && ReadablePtr((uint8_t*)actx - 0x40, 0x60)) {
                    const uint64_t* q = reinterpret_cast<const uint64_t*>((uint8_t*)actx - 0x40);
                    printf("[REDZONE-HEAP]   owner struct around header (header at +0x40):\n");
                    for (int i = 0; i < 12; i += 2)
                        printf("[REDZONE-HEAP]     %p: %016llx %016llx\n", (void*)((uint8_t*)actx - 0x40 + i * 8),
                               (unsigned long long)q[i], (unsigned long long)q[i + 1]);
                    PrintModOff("owner candidate vtable (header-0x40)", q[0]);
                }
            }
            if (AllocStats_Lookup(badBlock + 8, &sz, &caller, &actx)) {
                printf("[REDZONE-HEAP] bad block ALLOCSTATS: size=%llu ctx=%p caller=", (unsigned long long)sz, actx);
                PrintModOff("alloc-caller", (uint64_t)(uintptr_t)caller);
            }
        }

        // (2) Any live block of OURS that physically CONTAINS the bad tag
        // address — the genuine victim whose tag was clobbered.
        for (auto& kv : *g_live) {
            uint8_t* u = reinterpret_cast<uint8_t*>(kv.first);
            uint8_t* blk = u - 8;
            size_t recBlock = ((kv.second.reqSize + g_pad + 7) & ~size_t(7)) + 8;
            if (badBlock >= blk && badBlock < blk + recBlock) {
                printf("[REDZONE-HEAP] bad tag falls INSIDE our block @%p (user=%p):\n",
                       (void*)blk, (void*)u);
                PrintEntry("CONTAINS-BADTAG", (void*)u, kv.second);
            }
        }

        // (3) Most-recent live allocations by seq — temporal suspects (immune
        // to the spatial walk derailing on the first bad tag). The bad write
        // usually happened shortly before detection.
        {
            const int kTop = 8;
            const std::pair<void* const, Entry>* top[kTop] = {nullptr};
            for (auto& kv : *g_live) {
                for (int i = 0; i < kTop; ++i) {
                    if (!top[i] || kv.second.seq > top[i]->second.seq) {
                        for (int j = kTop - 1; j > i; --j) top[j] = top[j-1];
                        top[i] = &kv; break;
                    }
                }
            }
            printf("[REDZONE-HEAP] most-recent %d live allocations (temporal suspects):\n", kTop);
            for (int i = 0; i < kTop && top[i]; ++i) {
                PrintEntry("RECENT", top[i]->first, top[i]->second);
            }
        }

        fflush(stdout);
        DumpMainThreadStack();
        // Dump the 64 bytes around the bad tag: the clobber VALUE identifies the writer
        // (a refcount bump, a state int, a pointer) better than any backtrace of the victim.
        if (ReadablePtr(badBlock - 0x20, 0x60)) {
            const uint64_t* q = reinterpret_cast<const uint64_t*>(badBlock - 0x20);
            for (int i = 0; i < 12; i += 2)
                printf("[REDZONE-HEAP]   %p: %016llx %016llx\n", (void*)(badBlock - 0x20 + i * 8),
                       (unsigned long long)q[i], (unsigned long long)q[i + 1]);
        }
        fflush(stdout);
        const char* noAbort = getenv("BANISHED_REDZONE_ABORT");
        if (!(noAbort && noAbort[0] == '0')) {
            printf("[REDZONE-HEAP] aborting cleanly at heap corruption\n");
            fflush(stdout);
            ExitProcess(0x123);  // distinctive code for "heap walker tripped"
        }
        g_reportedOnce = true;
    }
    return bad;
}

// BANISHED_TEARDOWN_HEAPCHECK=1 (with BANISHED_REDZONE=watch): once GameUpdateLoop_Shutdown
// begins, walk the whole heap on EVERY Allocate/Free (allocator_lock.cpp calls in). The
// 800x10 teardown corruption never shows up during the run (watchdog silent for 9000
// frames) but the process dies in EntityList_Shutdown, so the bad write happens between
// two teardown heap ops — this names the op right after it, with a backtrace.
int g_teardownHeapCheck = 0;
extern "C" void RedzoneAlloc_ArmTeardownCheck() {
    const char* e = getenv("BANISHED_TEARDOWN_HEAPCHECK");
    if (!g_enabled || !e || e[0] == '0') return;
    g_teardownHeapCheck = 1;
    printf("[REDZONE-HEAP] teardown per-op heap check ARMED\n");
    fflush(stdout);
    RedzoneAlloc_HeapCheck("teardown-arm");
}
extern "C" void RedzoneAlloc_TeardownOp(const char* what) {
    if (!g_teardownHeapCheck) return;
    static int depth = 0;
    if (depth) return;          // the walk itself must not re-enter
    ++depth;
    RedzoneAlloc_HeapCheck(what);
    --depth;
}

int RedzoneAlloc_HeapCheck(const char* tag) {
    if (!g_enabled) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    return HeapCheckLocked(tag);
}

// Shim-INDEPENDENT heap dump for the crash handler. Walks the engine
// allocator's boundary tags WITHOUT requiring the redzone shim to be installed
// (so it works on a fully-native run — no layout perturbation, natural crash
// rate). On the SIGSEGV from free-list poisoning, this finds the first corrupt
// block and dumps the physically-PRECEDING block's first qword (its vtable for
// an engine object) resolved to module+offset = the class whose buffer
// overflowed. Does NOT take the allocator critical section (crash context).
// Install a secondary, write-through output sink for the crash-context dump.
// The crash handler points this at its FILE_FLAG_WRITE_THROUGH logger so the
// dump survives even if a re-entrant fault terminates the process mid-walk.
extern "C" void RedzoneAlloc_SetDumpSink(void (*sink)(const char*)) {
    g_rzSink = sink;
}

extern "C" void RedzoneAlloc_HeapDump(const char* tag) {
    if (!g_pMemAllocSingleton) {
        RZOut("[REDZONE-DUMP] no allocator singleton\n"); fflush(stdout);
        return;
    }
    uint8_t* alloc = reinterpret_cast<uint8_t*>(g_pMemAllocSingleton);
    if (IsBadReadPtr(alloc, 0x18)) { RZOut("[REDZONE-DUMP] singleton unreadable\n"); return; }
    uint8_t* ranges   = *reinterpret_cast<uint8_t**>(alloc + 0x00);
    int      rangeCnt = *reinterpret_cast<int*>(alloc + 0x08);
    RZOut("[REDZONE-DUMP] '%s': allocator=%p ranges=%p cnt=%d\n",
           tag ? tag : "?", (void*)alloc, (void*)ranges, rangeCnt);
    if (!ranges || rangeCnt <= 0 || rangeCnt >= 4096) { fflush(stdout); return; }

    // Shared: dump a (suspected overflower) block — tag, full payload qwords,
    // user+0 vtable resolution, and our alloc backtrace if the table is tracking.
    auto dumpBlockInfo = [](uint8_t* block) {
        if (!block || IsBadReadPtr(block, 0x10)) return;
        const uint64_t* q = reinterpret_cast<const uint64_t*>(block);
        size_t pb = (size_t)(q[0] & 0x1ffffffffffULL) * 4;
        RZOut("[REDZONE-DUMP]   OVERFLOWER block @%p tag=%016llx size=%zu (payload %zu):\n",
               (void*)block, (unsigned long long)q[0], pb, pb >= 8 ? pb - 8 : 0);
        PrintModOff("overflower user+0 (vtable?)", q[1]);
        uint8_t* u = block + 8;
        size_t payQ = pb >= 8 ? (pb - 8) / 8 : 0;
        if (payQ > 32) payQ = 32;
        for (size_t i = 0; i < payQ; ++i) {
            if (IsBadReadPtr(u + i*8, 8)) break;
            uint64_t v = *reinterpret_cast<uint64_t*>(u + i*8);
            // Show ASCII for each byte too (catches embedded strings/paths).
            char asc[9]; for (int b = 0; b < 8; ++b) { char c = (char)((v >> (b*8)) & 0xff); asc[b] = (c >= 32 && c < 127) ? c : '.'; } asc[8] = 0;
            RZOut("[REDZONE-DUMP]     user+0x%02zx = %016llx  '%s'\n",
                   i*8, (unsigned long long)v, asc);
        }
        if (g_enabled && g_live) {
            auto it = g_live->find(u);
            if (it != g_live->end()) {
                RZOut("[REDZONE-DUMP]   overflower IS OURS reqSize=%zu seq=%u — alloc backtrace:\n",
                       it->second.reqSize, it->second.seq);
                for (int i = 0; i < it->second.btCount; ++i)
                    PrintModOff("    bt", (uint64_t)it->second.bt[i]);
            } else {
                RZOut("[REDZONE-DUMP]   overflower NOT in our table (Runtime-allocated or stale)\n");
            }
        }
    };

    int found = 0;
    for (int r = 0; r < rangeCnt; ++r) {
        uint8_t* rng   = ranges + (size_t)r * 0x18;
        if (IsBadReadPtr(rng, 0x18)) continue;
        uint8_t* start = *reinterpret_cast<uint8_t**>(rng + 0x00);
        uint8_t* end   = *reinterpret_cast<uint8_t**>(rng + 0x08);
        if (!start || !end || end <= start) continue;

        uint8_t* prev = nullptr;
        uint8_t* p = start;
        int guard = 0;
        while (p < end && ++guard < 4000000) {
            if (IsBadReadPtr(p, 8)) {
                RZOut("[REDZONE-DUMP] range %d: unreadable block @%p prev=%p\n",
                       r, (void*)p, (void*)prev);
                ++found; break;
            }
            uint64_t blkTag = *reinterpret_cast<uint64_t*>(p);
            size_t blkBytes = (size_t)(blkTag & 0x1ffffffffffULL) * 4;
            if (blkBytes < 8 || p + blkBytes > end) {
                RZOut("[REDZONE-DUMP] range %d [%p..%p): CORRUPT block @%p "
                       "tag=0x%016llx size=%zu\n",
                       r, (void*)start, (void*)end, (void*)p,
                       (unsigned long long)blkTag, blkBytes);
                if (prev) dumpBlockInfo(prev);
                ++found; break;
            }
            prev = p;
            p += blkBytes;
        }
        if (found) break;
    }
    if (found) { fflush(stdout); return; }

    // ---- Pass 2: free-list LINK validation ----
    // The crash is in Allocate's free-list traversal (GetFreeTagForSize), which
    // follows a per-block forward link stored at block+8 as a signed 41-bit
    // RELATIVE offset: next_FreeTag = (block+8) + signext41(qword & 0x1ffffffffff).
    // Bucket heads live at allocator + 0x28 + bucket*8 for buckets [0, 0xC0).
    // An OOB write into a freed block's body clobbers this link → Allocate
    // follows it to a wild address → crash. Tags stay intact, so pass 1 misses it.
    RZOut("[REDZONE-DUMP] no tag corruption — validating free-list LINKS...\n");
    auto inAnyRange = [&](uint8_t* a) -> bool {
        for (int r = 0; r < rangeCnt; ++r) {
            uint8_t* rg = ranges + (size_t)r * 0x18;
            if (IsBadReadPtr(rg, 0x18)) continue;
            uint8_t* s = *reinterpret_cast<uint8_t**>(rg + 0x00);
            uint8_t* e = *reinterpret_cast<uint8_t**>(rg + 0x08);
            if (s && e && a >= s && a < e) return true;
        }
        return false;
    };
    auto dumpOverflower = [&](uint8_t* badBlock) {
        for (int r = 0; r < rangeCnt; ++r) {
            uint8_t* rg = ranges + (size_t)r * 0x18;
            if (IsBadReadPtr(rg, 0x18)) continue;
            uint8_t* s = *reinterpret_cast<uint8_t**>(rg + 0x00);
            uint8_t* e = *reinterpret_cast<uint8_t**>(rg + 0x08);
            if (!(s && e && badBlock >= s && badBlock < e)) continue;
            uint8_t* pp = s; uint8_t* prev = nullptr; int g2 = 0;
            while (pp < badBlock && ++g2 < 4000000) {
                if (IsBadReadPtr(pp, 8)) { prev = nullptr; break; }
                uint64_t t = *reinterpret_cast<uint64_t*>(pp);
                size_t bb = (size_t)(t & 0x1ffffffffffULL) * 4;
                if (bb < 8) { prev = nullptr; break; }
                prev = pp; pp += bb;
            }
            if (pp == badBlock && prev) {
                dumpBlockInfo(prev);
            } else {
                RZOut("[REDZONE-DUMP]   (could not locate preceding block of %p)\n",
                       (void*)badBlock);
            }
            break;
        }
    };

    for (int bucket = 0; bucket < 0xC0 && !found; ++bucket) {
        uint8_t* head = *reinterpret_cast<uint8_t**>(alloc + 0x28 + (size_t)bucket * 8);
        if (!head) continue;
        uint8_t* curFT = head;  // FreeTag = block+8
        uint8_t* prevFT = nullptr;
        int guard = 0;
        while (curFT && ++guard < 2000000) {
            uint8_t* block = curFT - 8;
            if (IsBadReadPtr(curFT, 8) || !inAnyRange(block)) {
                RZOut("[REDZONE-DUMP] bucket %d: free node @%p (block %p) OUT OF RANGE "
                       "— prior node @%p has the corrupt link\n",
                       bucket, (void*)curFT, (void*)block, (void*)prevFT);
                if (prevFT) {
                    dumpOverflower(prevFT - 8);  // overflower precedes the clobbered block
                } else {
                    // Bucket HEAD itself was clobbered. The corrupt value usually
                    // differs from the real free node only in its HIGH bytes (e.g.
                    // +0x100000000 = a byte at +4 flipped). Reconstruct the in-range
                    // node by grafting each range's high 32 bits onto the corrupt
                    // node's low 32 bits, then dump the LIVE block physically before
                    // it = the overflow source (names the class via its vtable).
                    uint64_t corruptLow = reinterpret_cast<uint64_t>(curFT) & 0xffffffffULL;
                    for (int rr = 0; rr < rangeCnt; ++rr) {
                        uint8_t* rg2 = ranges + (size_t)rr * 0x18;
                        if (IsBadReadPtr(rg2, 0x18)) continue;
                        uint8_t* s2 = *reinterpret_cast<uint8_t**>(rg2 + 0x00);
                        uint8_t* e2 = *reinterpret_cast<uint8_t**>(rg2 + 0x08);
                        if (!s2 || !e2) continue;
                        uint64_t hi = reinterpret_cast<uint64_t>(s2) & ~0xffffffffULL;
                        uint8_t* cand = reinterpret_cast<uint8_t*>(hi | corruptLow);
                        if (cand >= s2 && cand < e2) {
                            RZOut("[REDZONE-DUMP]   reconstructed in-range node @%p "
                                  "(corrupt low32 grafted onto range hi) — dumping its predecessor:\n",
                                  (void*)cand);
                            dumpOverflower(cand - 8);
                            break;
                        }
                    }
                }
                ++found; break;
            }
            uint64_t link = *reinterpret_cast<uint64_t*>(curFT);
            int64_t off = (int64_t)(link << 23) >> 23;  // sign-extend low 41 bits
            if (off == 0) break;                          // end of chain
            uint8_t* next = curFT + off;
            if (IsBadReadPtr(next, 8) || !inAnyRange(next - 8)) {
                RZOut("[REDZONE-DUMP] bucket %d: free block @%p has CORRUPTED forward link "
                       "qword=%016llx off=%lld -> next=%p (out of range)\n",
                       bucket, (void*)block, (unsigned long long)link,
                       (long long)off, (void*)next);
                if (!IsBadReadPtr(block, 0x28)) {
                    const uint64_t* q = reinterpret_cast<const uint64_t*>(block);
                    RZOut("[REDZONE-DUMP]   clobbered free block body: "
                           "tag=%016llx +8=%016llx +10=%016llx +18=%016llx +20=%016llx\n",
                           (unsigned long long)q[0], (unsigned long long)q[1],
                           (unsigned long long)q[2], (unsigned long long)q[3],
                           (unsigned long long)q[4]);
                }
                dumpOverflower(block);   // the live block physically before it = overflow source
                ++found; break;
            }
            prevFT = curFT;
            curFT = next;
        }
    }
    if (!found) RZOut("[REDZONE-DUMP] free-list forward links also intact "
                       "(corruption may be in the high-bit prev links or a bucket head)\n");
    fflush(stdout);
}

static DWORD WINAPI RedzoneWatchdog(LPVOID) {
    int interval = g_watchIntervalMs > 0 ? g_watchIntervalMs : 2;  // default 2ms
    printf("[REDZONE-HEAP] watchdog thread started (interval=%dms)\n", interval);
    fflush(stdout);
    for (;;) {
        // Single-pass detect + report + abort. NO confirmation: the corrupted
        // free block is typically REUSED by the very Allocate that then crashes,
        // so a second confirmation pass would no longer see it (it raced away) —
        // confirmation drops real detections right before the crash. The walker
        // only flags egregious corruption (size<8 / overruns the range, e.g. a
        // zeroed tag), which is not a transient mid-Allocate state, so abort-on-
        // first-sight is safe enough.
        RedzoneAlloc_HeapCheck("watchdog");  // aborts on detection
        Sleep((DWORD)interval);
    }
    return 0;
}

