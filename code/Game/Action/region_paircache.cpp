// code/Game/Action/region_paircache.cpp
// Pathfinder region pair-cache: ring-buffer initializer + object builder.
//
// Faithful 1:1 port of FUN_14001cca0 @ 0x14001cca0 (the region-pair ring-buffer
// resize/init) plus a builder that replicates the pair-cache creation snippet from
// CreateNaturalResource_impl_sub_e8c0 @ 0x14009e8c0 (the region-system builder).
//
// The pair-cache caches region-to-region path lengths. It is a 0x38-byte object:
//   q[0]  (qword) seq/link
//   q[1]  (qword) Time-value staleness seed   <-- see DEVIATION below
//   q[2]  (ints)  ring head/cap
//   q[3]  (ptr)   ring buffer base            <-- the Vector header lives here (+0x18)
//   q[4]            (count/cap ints of the ring Vector)
//   q[5]  (ptr)   pair-BST root
//   *(int32*)(q+6)  trailing int = 0
// The ring buffer Vector header is the sub-object at byte offset +0x18 (= q+3), whose
// fields are: +0x00 base ptr (qword), +0x08 count (int), +0x0c capacity (int).
// FUN_14001cca0 resizes that ring to 0x20000 entries of stride 0x18 (~6 MB), which is
// faithful/correct.
//
// DEVIATION (one, documented): the original seeds q[1] via
//   System::Time::Time(&tmp, DAT_140172210)
// DAT_140172210 is a raw .rdata constant; ResolveAddr(Module::Exe, dataAddr) returns
// NULL for raw .rdata reads in standalone (see reference_standalone_rdata_null_fallback).
// q[1] is only a staleness timestamp used by cache eviction; for a freshly-built cache
// a 0 seed is safe (nothing in the cache yet to evict). We therefore store 0 and avoid
// the System::Time::Time dependency entirely.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// 0x14001ca60 - the shared intrusive-AVL delete (registered by reg_work_cellbst).
typedef void (*fn_BstRemove5d10_t)(uintptr_t* root, void* node);

// Diagnostic helper for Pf_RegionPairCacheTick_5d10 (BANISHED_DIAG_PAIRCACHE=1).
static void PairCacheDiag(int on, long calls, const int64_t* c, long evicted, long evictedAtEntry) {
    if (!on) return;
    if (calls != 1 && (calls % 2000) != 0 && evicted == evictedAtEntry) return;
    fprintf(stderr, "[PairCache] call=%ld clock=%lld window=%lld readTail=%d writeHead=%d cap=%d evicted=%ld(+%ld)\n",
            calls, (long long)c[0], (long long)c[1],
            *(const int*)((const char*)c + 0x14), *(const int*)((const char*)c + 0x10),
            *(const int*)((const char*)c + 0x20), evicted, evicted - evictedAtEntry);
    fflush(stderr);
}

// ---- FUN_14001cca0: resize the region-pair ring buffer to 0x20000 entries (stride 0x18).
// param_1 is the ring Vector header: [0]=base ptr, +0x08=count(int), +0x0c=capacity(int).
void Pf_RegionPairRingResize_cca0(int64_t* param_1) {
    int iVar1 = (int)param_1[1];
    if (iVar1 != 0x20000) {
        void* pvVar2 = (void*)*param_1;
        *(uint32_t*)(param_1 + 1) = 0x20000;
        if (*(int*)((int64_t)param_1 + 0xc) < 0x20000) {
            *(uint32_t*)((int64_t)param_1 + 0xc) = 0x20000;
            if (iVar1 == 0x1ffff) {
                *(uint32_t*)((int64_t)param_1 + 0xc) = 0x60000;
            }
            void* pvVar4 = fn_MemAlloc(g_pMemAllocSingleton,
                                       (int64_t)*(int*)((int64_t)param_1 + 0xc) * 0x18, 8);
            *param_1 = (int64_t)pvVar4;
        }
        int iVar3 = 0;
        if (pvVar2 == (void*)*param_1) {
            iVar3 = iVar1;
        }
        int64_t lVar7 = (int64_t)iVar3;
        if (lVar7 < 0x20000) {
            int64_t lVar5 = lVar7 * 0x18;
            do {
                if (lVar7 < iVar1) {
                    uint64_t* puVar6 = (uint64_t*)(*param_1 + lVar5);
                    if (puVar6 != nullptr) {
                        puVar6[0] = *(uint64_t*)(lVar5 + (int64_t)pvVar2);
                        puVar6[1] = *(uint64_t*)(lVar5 + 8 + (int64_t)pvVar2);
                        puVar6[2] = *(uint64_t*)(lVar5 + 0x10 + (int64_t)pvVar2);
                    }
                } else {
                    uint64_t* puVar6 = (uint64_t*)(*param_1 + lVar5);
                    if (puVar6 != nullptr) {
                        puVar6[0] = 0xffffffffffffffffULL;
                        *(uint32_t*)(puVar6 + 1) = 0xffffffff;
                        puVar6[2] = 0;
                    }
                }
                lVar5 = lVar5 + 0x18;
                lVar7 = lVar7 + 1;
            } while (lVar5 < 0x300000);
        }
        if (pvVar2 != (void*)*param_1) {
            fn_MemFreeStatic(pvVar2);
        }
    }
}

// ---- Builder: allocate + init the whole 0x38-byte pair-cache object.
// Replicates the snippet in CreateNaturalResource_impl_sub_e8c0 (0x14009e8c0).
// Returns null on allocation failure (caller then zeroes Map+0x78 fields).
extern "C" void* Pf_RegionPairCache_Build() {
    void* pc = fn_MemAlloc(g_pMemAllocSingleton, 0x38, 4);
    if (pc == nullptr) {
        return nullptr;
    }
    int64_t* q = (int64_t*)pc;
    q[0] = 0;       // [0] seq/link
    // [1] +0x08 = the cache RETENTION WINDOW, in Time units. Retail seeds it with
    // Time::Time(DAT_140172210) where DAT_140172210 = 120.0f, i.e. 120 seconds.
    // This was previously seeded 0 as a flagged deviation. That is NOT inert: the
    // evictor at 0x1400a5d10 drops every entry for which
    //   (now - entry.timestamp) > window
    // so a 0 window evicts each entry on the first tick after it is inserted and
    // defeats the cache entirely. Fixed 2026-08-30 alongside LANE1 row 41.
    q[1] = 0;
    if (fn_Time_Ctor_Float) {
        int64_t window = 0;
        fn_Time_Ctor_Float(&window, 120.0f);
        q[1] = window;
    }
    q[2] = 0;       // [2] ring head/cap ints
    q[3] = 0;       // [3] ring buffer base ptr (Vector header at +0x18)
    q[4] = 0;       // [4] ring Vector count/cap
    q[5] = 0;       // [5] pair-BST root
    *(int32_t*)(q + 6) = 0;  // [6] trailing int

    // Init the ring buffer at byte +0x18 (= q+3): resizes to 0x20000 entries of stride 0x18.
    Pf_RegionPairRingResize_cca0((int64_t*)((char*)pc + 0x18));

    return pc;
}

// ============================================================
// 0x1400a5d10 - Pf_RegionPairCacheTick_5d10(cache, time)
// ============================================================
// The pair-cache's per-sim-tick STALENESS SWEEP. Called from GameUpdateLoop::Update
// (retail 0x140096380 +0x7e) once per simulation iteration as
//   Pf_RegionPairCacheTick_5d10(*(Game* + 0x78), time)
// where Game is gameWorld+0xF8.
//
// Our source called this "Game::UpdateTimedEvents" / fn_Game_UpdateTimedEvents. That is a
// MISNOMER - there is no timed-event queue. param_1 is the region-pair distance cache built
// by Pf_RegionPairCache_Build above (retail: the tail of 0x14009e8c0), and this function
// only ages entries out of it.
//
// Two distinct jobs, and the FIRST one is easy to miss:
//
//  1. `cache[0] += *time` - cache[0] IS THE CACHE'S CLOCK. Nothing else in the binary
//     advances it. Pf_RegionPairCacheInsert_5b30 stamps every new entry's timestamp
//     (ring slot +0x10) with the then-current cache[0]. So while this function was
//     unported the clock sat at 0, every entry was stamped 0, and no entry could ever
//     be considered stale.
//  2. Evict from the ring tail while the oldest entry is older than the retention
//     window at cache+0x08 (120 game-seconds; see Pf_RegionPairCache_Build). For each
//     evicted slot: erase its (keyA,keyB) from the pair BST at cache+0x28 via the AVL
//     delete 0x14001ca60, reset the slot to {-1,-1,<pad>,0}, and advance the tail with
//     wraparound at the capacity in cache+0x20.
//
// Note the insert already evicts on ring-FULL (head==tail), so the ring cannot overflow
// without this. What this adds is AGE-based invalidation: without it a cached region-to-
// region distance survives until 0x20000 (131072) further inserts push it out, so distances
// stay stale across bridges/roads/terrain edits that re-split or re-merge regions.
//
// Faithfulness notes:
//  - Ring entry stride 0x18: +0x00 int keyA, +0x04 int keyB, +0x08 uint value,
//    +0x10 int64 insertion timestamp.
//  - BST node: +0x00 int keyA, +0x04 int keyB, +0x18 left child, +0x20 right child.
//    (Ghidra renders these as piVar[6] / piVar[8] on an int*.)
//  - Naming follows Pf_RegionPairCacheInsert_5b30 above: cache+0x10 is the WRITE HEAD
//    (advanced by the insert) and cache+0x14 the READ TAIL (advanced here); the sweep
//    stops when the read tail catches the write head.
//    the capacity the int at cache+0x20 - all ints inside a qword-typed array, exactly as
//    in Pf_RegionPairCacheInsert_5b30 above.
//  - Retail's slot reset stores 8 bytes at entry+0x08 whose low half is 0xFFFFFFFF and
//    whose high half is an uninitialized stack slot (the decompiler's uStack_1c). We
//    transcribe the pad as 0, matching how the insert port handles its uStack_3c.
//  - The outer test is `> window` (retail JLE skips the whole loop), and the loop repeats
//    on the same strict `>` after advancing - so an entry exactly AT the window survives.
void Pf_RegionPairCacheTick_5d10(int64_t* param_1, const int64_t* param_2) {
    static fn_BstRemove5d10_t s_remove = nullptr;
    static bool s_init = false;
    if (!s_init) {
        s_init = true;
        s_remove = reinterpret_cast<fn_BstRemove5d10_t>(ResolveAddr(Module::Exe, 0x14001ca60));
    }
    if (param_1 == nullptr || param_2 == nullptr) return;

    // 1) advance the cache clock.
    param_1[0] += *param_2;

    // BANISHED_DIAG_PAIRCACHE=1 -> periodic one-line report (stderr). Diagnostic only.
    static int s_diag = -1;
    static long s_calls = 0, s_evicted = 0;
    if (s_diag < 0) { const char* e = getenv("BANISHED_DIAG_PAIRCACHE"); s_diag = (e && *e && *e != '0') ? 1 : 0; }
    const long evictedAtEntry = s_evicted;

    const int64_t window = param_1[1];
    int64_t ringBase = param_1[3];
    if (ringBase == 0) return;

    int head = *(int*)((int64_t)param_1 + 0x14);
    if (param_1[0] - *(int64_t*)(ringBase + (int64_t)head * 0x18 + 0x10) <= window) {
        PairCacheDiag(s_diag, ++s_calls, param_1, s_evicted, evictedAtEntry);
        return;
    }

    do {
        // stop when the read tail catches the write head (+0x10)
        if (head == *(int*)((int64_t)param_1 + 0x10)) {
            PairCacheDiag(s_diag, ++s_calls, param_1, s_evicted, evictedAtEntry);
            return;
        }

        int64_t entry = ringBase + (int64_t)head * 0x18;

        // 2a) erase this pair from the BST at cache+0x28
        int* node = (int*)param_1[5];
        if (node != nullptr) {
            const int keyA = *(int*)entry;
            do {
                const int nodeA = *node;
                if (keyA < nodeA) {
                    node = *(int**)(node + 6);          // left
                } else {
                    if (keyA <= nodeA) {
                        const int keyB = *(int*)(entry + 4);
                        if (keyB < node[1]) {
                            node = *(int**)(node + 6);  // left
                            if (node == nullptr) break;
                            continue;
                        }
                        if (keyB <= node[1]) {
                            if (s_remove) s_remove((uintptr_t*)(param_1 + 5), node);
                            break;
                        }
                    }
                    node = *(int**)(node + 8);          // right
                }
            } while (node != nullptr);
        }

        // 2b) reset the slot
        uint64_t* slot = (uint64_t*)(ringBase + (int64_t)*(int*)((int64_t)param_1 + 0x14) * 0x18);
        const uint32_t pad = 0;   // retail's uninitialized uStack_1c high half
        slot[0] = 0xffffffffffffffffULL;
        slot[1] = ((uint64_t)pad << 32) | (uint64_t)0xffffffffu;
        slot[2] = 0;

        ++s_evicted;

        // 2c) advance the read cursor, wrapping at capacity (cache+0x20)
        int next = *(int*)((int64_t)param_1 + 0x14) + 1;
        if (*(int*)(param_1 + 4) <= next) next = 0;
        *(int*)((int64_t)param_1 + 0x14) = next;
        head = next;

        ringBase = param_1[3];
    } while (param_1[0] - *(int64_t*)(ringBase + (int64_t)head * 0x18 + 0x10) > window);

    PairCacheDiag(s_diag, ++s_calls, param_1, s_evicted, evictedAtEntry);
}
