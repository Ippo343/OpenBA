// standalone/redzone_alloc.cpp — see redzone_alloc.h for design rationale.

#ifdef STANDALONE_EXE

#include "redzone_alloc.h"
#include "engine_funcs.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <unordered_map>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

// Tail-redzone padding (bytes) added to each allocation. DEFAULT 0 — padding
// every block shifts the whole engine heap layout, which HIDES layout-dependent
// free-list corruption (the OOB write lands on padding instead of a real tag).
// So by default we add NO padding (identical layout to an uninstrumented run →
// the bug still reproduces) and rely on the read-only HeapCheck walker to catch
// the corrupted boundary tag. Set BANISHED_REDZONE_PAD=<bytes> to re-enable the
// tail canary (useful only for contiguous overruns of OUR buffers).
size_t   g_pad      = 0;
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

void PrintEntry(const char* why, void* userPtr, const Entry& e) {
    // Find how far the corruption extends (first + last bad byte).
    const uint8_t* c = CanaryPtr(userPtr, e.reqSize);
    int firstBad = -1, lastBad = -1;
    for (int i = 0; i < (int)g_pad; ++i) {
        if (c[i] != kCanary) { if (firstBad < 0) firstBad = i; lastBad = i; }
    }
    printf("[REDZONE] %s ptr=%p reqSize=%zu (0x%zx) seq=%u "
           "overflow bytes [%d..%d] of %zu\n",
           why, userPtr, e.reqSize, e.reqSize, e.seq,
           firstBad, lastBad, g_pad);
    printf("[REDZONE]   alloc backtrace:");
    for (int i = 0; i < e.btCount; ++i) {
        uintptr_t a = reinterpret_cast<uintptr_t>(e.bt[i]);
        if (g_exeBase && a >= g_exeBase && a < g_exeBase + 0x600000) {
            // exe-relative → Ghidra address (image base 0x140000000)
            printf(" 0x%llx", (unsigned long long)(0x140000000ull + (a - g_exeBase)));
        } else {
            printf(" [%p]", e.bt[i]);
        }
    }
    printf("\n");
    fflush(stdout);
}

// Caller MUST hold g_mutex. Scans every live block's tail canary; logs each
// corrupted one and (unless BANISHED_REDZONE_ABORT=0) ExitProcess()es cleanly
// at the first corruption — before the bad block is ever freed, which is what
// otherwise deadlocks the allocator critical section into a hang.
int ValidateAllLocked(const char* tag) {
    int bad = 0;
    for (auto& kv : *g_live) {
        if (!CanaryIntact(kv.first, kv.second.reqSize)) {
            if (bad == 0) {
                printf("[REDZONE] ==== corruption detected at checkpoint '%s' ====\n",
                       tag ? tag : "?");
            }
            PrintEntry("CORRUPT", kv.first, kv.second);
            ++bad;
        }
    }
    if (bad) {
        printf("[REDZONE] checkpoint '%s': %d corrupted block(s), %zu live\n",
               tag ? tag : "?", bad, g_live->size());
        fflush(stdout);
        const char* noAbort = getenv("BANISHED_REDZONE_ABORT");
        if (!(noAbort && noAbort[0] == '0')) {
            printf("[REDZONE] aborting cleanly at first corruption "
                   "(set BANISHED_REDZONE_ABORT=0 to continue)\n");
            fflush(stdout);
            ExitProcess(0x122);  // distinctive exit code for "redzone tripped"
        }
    }
    return bad;
}

void* ShimAlloc(void* singleton, uint64_t size, int alignment) {
    void* p = g_realAlloc(singleton, size + g_pad, alignment);
    if (!p) return p;  // alloc failed — nothing to stamp
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
        // (it follows a clobbered boundary tag). So when scanning is enabled we
        // validate EVERY free, right before handing the pointer to the real
        // Free — the fatal free is then preceded by a full canary scan that
        // catches the overflow and aborts cleanly first. Frees are rarer than
        // allocs, so this O(live) per free is acceptable for a diagnostic.
        if (g_scanEvery) {
            ValidateAllLocked("auto-scan(pre-free)");
            HeapCheckLocked("auto-scan(pre-free)");
        }
    }
    g_realFree(ptr);
}

} // namespace

static DWORD WINAPI RedzoneWatchdog(LPVOID);

void RedzoneAlloc_Install() {
    const char* env = getenv("BANISHED_REDZONE");
    if (!env || env[0] == '0' || env[0] == 'f' || env[0] == 'F') {
        return;  // disabled — leave the real allocator pointers in place
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
    fn_MemAlloc      = &ShimAlloc;
    fn_MemFreeStatic = &ShimFree;
    g_enabled = true;
    printf("[REDZONE] installed: tailPad=%zu bytes (0=native layout), "
           "autoScanEvery=%u, watchdog=%dms, real alloc=%p free=%p exeBase=%p\n",
           g_pad, g_scanEvery, g_watchIntervalMs,
           (void*)g_realAlloc, (void*)g_realFree, (void*)g_exeBase);
    fflush(stdout);

    if (g_watchIntervalMs >= 0) {
        CreateThread(nullptr, 0, &RedzoneWatchdog, nullptr, 0, nullptr);
    }
}

bool RedzoneAlloc_Enabled() { return g_enabled; }

int RedzoneAlloc_ValidateAll(const char* tag) {
    if (!g_enabled) return 0;
    std::lock_guard<std::mutex> lk(g_mutex);
    return ValidateAllLocked(tag);
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
        printf("[REDZONE-HEAP]   %s = %s+0x%llX\n",
               label, nm ? nm + 1 : path,
               (unsigned long long)(addr - (uintptr_t)mod));
    } else {
        printf("[REDZONE-HEAP]   %s = 0x%016llx (not in a module)\n",
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
static int HeapCheckLocked(const char* tag, bool report) {
    if (!g_enabled || !g_pMemAllocSingleton) return 0;
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

    if (bad) {
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
        const char* noAbort = getenv("BANISHED_REDZONE_ABORT");
        if (!(noAbort && noAbort[0] == '0')) {
            printf("[REDZONE-HEAP] aborting cleanly at heap corruption\n");
            fflush(stdout);
            ExitProcess(0x123);  // distinctive code for "heap walker tripped"
        }
    }
    return bad;
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
extern "C" void RedzoneAlloc_HeapDump(const char* tag) {
    if (!g_pMemAllocSingleton) {
        printf("[REDZONE-DUMP] no allocator singleton\n"); fflush(stdout);
        return;
    }
    uint8_t* alloc = reinterpret_cast<uint8_t*>(g_pMemAllocSingleton);
    if (IsBadReadPtr(alloc, 0x18)) { printf("[REDZONE-DUMP] singleton unreadable\n"); return; }
    uint8_t* ranges   = *reinterpret_cast<uint8_t**>(alloc + 0x00);
    int      rangeCnt = *reinterpret_cast<int*>(alloc + 0x08);
    printf("[REDZONE-DUMP] '%s': allocator=%p ranges=%p cnt=%d\n",
           tag ? tag : "?", (void*)alloc, (void*)ranges, rangeCnt);
    if (!ranges || rangeCnt <= 0 || rangeCnt >= 4096) { fflush(stdout); return; }

    // Shared: dump a (suspected overflower) block — tag, full payload qwords,
    // user+0 vtable resolution, and our alloc backtrace if the table is tracking.
    auto dumpBlockInfo = [](uint8_t* block) {
        if (!block || IsBadReadPtr(block, 0x10)) return;
        const uint64_t* q = reinterpret_cast<const uint64_t*>(block);
        size_t pb = (size_t)(q[0] & 0x1ffffffffffULL) * 4;
        printf("[REDZONE-DUMP]   OVERFLOWER block @%p tag=%016llx size=%zu (payload %zu):\n",
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
            printf("[REDZONE-DUMP]     user+0x%02zx = %016llx  '%s'\n",
                   i*8, (unsigned long long)v, asc);
        }
        if (g_enabled && g_live) {
            auto it = g_live->find(u);
            if (it != g_live->end()) {
                printf("[REDZONE-DUMP]   overflower IS OURS reqSize=%zu seq=%u — alloc backtrace:\n",
                       it->second.reqSize, it->second.seq);
                for (int i = 0; i < it->second.btCount; ++i)
                    PrintModOff("    bt", (uint64_t)it->second.bt[i]);
            } else {
                printf("[REDZONE-DUMP]   overflower NOT in our table (Runtime-allocated or stale)\n");
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
                printf("[REDZONE-DUMP] range %d: unreadable block @%p prev=%p\n",
                       r, (void*)p, (void*)prev);
                ++found; break;
            }
            uint64_t blkTag = *reinterpret_cast<uint64_t*>(p);
            size_t blkBytes = (size_t)(blkTag & 0x1ffffffffffULL) * 4;
            if (blkBytes < 8 || p + blkBytes > end) {
                printf("[REDZONE-DUMP] range %d [%p..%p): CORRUPT block @%p "
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
    printf("[REDZONE-DUMP] no tag corruption — validating free-list LINKS...\n");
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
                printf("[REDZONE-DUMP]   (could not locate preceding block of %p)\n",
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
                printf("[REDZONE-DUMP] bucket %d: free node @%p (block %p) OUT OF RANGE "
                       "— prior node @%p has the corrupt link\n",
                       bucket, (void*)curFT, (void*)block, (void*)prevFT);
                if (prevFT) dumpOverflower(prevFT - 8);  // overflower precedes the clobbered block
                ++found; break;
            }
            uint64_t link = *reinterpret_cast<uint64_t*>(curFT);
            int64_t off = (int64_t)(link << 23) >> 23;  // sign-extend low 41 bits
            if (off == 0) break;                          // end of chain
            uint8_t* next = curFT + off;
            if (IsBadReadPtr(next, 8) || !inAnyRange(next - 8)) {
                printf("[REDZONE-DUMP] bucket %d: free block @%p has CORRUPTED forward link "
                       "qword=%016llx off=%lld -> next=%p (out of range)\n",
                       bucket, (void*)block, (unsigned long long)link,
                       (long long)off, (void*)next);
                if (!IsBadReadPtr(block, 0x28)) {
                    const uint64_t* q = reinterpret_cast<const uint64_t*>(block);
                    printf("[REDZONE-DUMP]   clobbered free block body: "
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
    if (!found) printf("[REDZONE-DUMP] free-list forward links also intact "
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

#endif // STANDALONE_EXE
