// code/Game/Game/region_seed.cpp
// Keystone A — build the pathfinder COARSE REGION-CONNECTIVITY MAP (Map+0x70 graph,
// per-cell regionId at cell+0x18, Map+0x78 region-pair cache).
//
// The real game builds this inside CreateNaturalResource_impl_sub_e8c0 (0x14009e8c0)
// the first time a natural resource is created during worldgen. e8c0 also spins up the
// async region-rebuild WORKER thread (Map+0x68) — but the INITIAL region build is fully
// SYNCHRONOUS and does NOT touch the worker: it allocates the region graph (8670),
// flushes the all-dirty chunk lists (8a90 -> per-8x8-block flood-fill -> stamps
// regionId into the Map cell array), then builds the region-pair cache. We replicate
// exactly that synchronous subset, skipping the worker (already seeded separately by
// pathfinder_seed.cpp when MOVETICK is on).
//
// Why it matters: Map_CalcDistance (0x140008830) -> Pf_IsDstReachable (f810). f810 reads
// each cell's regionId (cell+0x18); when unbuilt it spirals (9d0a0) and, finding all -1,
// returns -1, so Map_CalcDistance falls back to euclidean + gridW (= +256 on a 256 map),
// which ALWAYS exceeds work radii -> e.g. ForesterWork's "tree within radius*2" check
// fails for every cell. With the region map built, f810 returns 1 for same-region cells
// and runs the (already ported+registered) region A* (a8ba0/dcd0) for cross-region pairs,
// so distances are realistic and range checks pass.
//
// Dependencies (ALL already ported + registered): the chunk family 8a90/80a0/7f20/7ad0/
// 7950/7670/7750/8200/8350/8420/7480 (action_pf_region.cpp + reg_pf_region.cpp), the
// region A* dcd0/dc00/d320/d450/d550/3480/d400 + entry a8ba0 (action_pf_astar.cpp /
// action_pf_pools.cpp + their registrars). NEW here: 8670 (graph ctor) + e060 (coarse
// array) in region_graph_build.cpp, and the pair cache (cca0) in region_paircache.cpp.
//
// Gated behind BANISHED_LIVE_REGIONMAP (default OFF until A/B-verified); when off this is
// a no-op and Map+0x70/+0x78 stay 0 (zeroed by the Game ctor), preserving the euclidean
// fallback exactly as before.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// --- ported region-graph builders (region_graph_build.cpp / region_paircache.cpp) ---
extern int*  Pf_RegionGraphCtor_8670(int* param_1, int64_t param_2, int64_t param_3,
                                     int64_t param_4, uint32_t param_5, int param_6);  // 0x1400a8670
extern "C" void* Pf_RegionPairCache_Build();                                           // e8c0 pair-cache tail
// --- already-ported region chunk flush (action_pf_region.cpp) ---
extern void  Pf_FlushDirtyChunks_8a90(int64_t param_1);                                // 0x1400a8a90

namespace ComponentFactory { bool LiveEnabled(const char* envVar); }

extern "C" void Region_Seed(void* gameObjV, int dim) {
    if (!::ComponentFactory::LiveEnabled("BANISHED_LIVE_REGIONMAP")) return;
    if (!gameObjV || dim <= 0 || !fn_MemAlloc) return;

    uint8_t* map = reinterpret_cast<uint8_t*>(gameObjV);   // the Map/Game object (gameWorld+0xf8)
    if (*reinterpret_cast<void**>(map + 0x70) != nullptr) return;   // already built

    // coarse-grid dims = ceil(dim/8) via the exe's signed-rounding idiom (e8c0):
    //   (int)(dim + ((dim >> 0x1f) & 7)) >> 3
    int coarseDim = static_cast<int>((static_cast<int>(dim) + ((static_cast<int>(dim) >> 31) & 7)) >> 3);
    if (coarseDim <= 0) return;

    // 1) allocate + construct the region graph (e8c0: MemAlloc(0x108,4) -> 8670).
    void* graphV = fn_MemAlloc(g_pMemAllocSingleton, 0x108, 4);
    if (!graphV) return;
    Pf_RegionGraphCtor_8670(reinterpret_cast<int*>(graphV), reinterpret_cast<int64_t>(map),
                            /*worker*/0, /*ctx*/0, static_cast<uint32_t>(coarseDim), coarseDim);

    // 2) link the graph (Map+0x70) BEFORE flushing — 8a90 stamps regionId via graph+0x50=Map.
    *reinterpret_cast<void**>(map + 0x70) = graphV;

    // 3) flush all (8670 marked every coarse cell dirty in both lists) -> flood-fill each
    //    8x8 block, compute centroids/edges, and STAMP regionId into the Map cell array.
    Pf_FlushDirtyChunks_8a90(reinterpret_cast<int64_t>(graphV));

    // 4) region-pair length cache (Map+0x78) — needed by f810's cross-region path (a8ba0).
    void* pcV = Pf_RegionPairCache_Build();
    *reinterpret_cast<void**>(map + 0x78) = pcV;

    int regionCount = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(graphV) + 0x70);
    fprintf(stderr, "[RegionSeed] region map built: graph@%p coarseDim=%d regionNodes=%d pairCache=%p\n",
            graphV, coarseDim, regionCount, pcV);
    fflush(stderr);
}
