// standalone/road_probe.cpp
//
// BANISHED_ROAD_CENSUS=<n frames> — the ROAD PAVING census.
//
// Highwaymen wants 2,000 paved road tiles. The stat it reads (StatValueDispatch
// case 0x17, code/Game/Goal/statvaluedispatch.cpp Helper_86a0) is NOT a walk of
// the map: it reads one int out of the road manager's per-road-type counter
// array at gameWorld+0x110 +0x220, indexed by the goal's road-type index.
//
// That array is written in exactly one place — the road mesh rebuild
// (Pf_RoadMeshRebuild_f370, 0x1400af370). Per fine terrain cell it does:
//
//     batchId = cell+0x2a;  blendId = cell+0x2b;
//     if (batchId != 0xff && blendId == 0xff) counter[batchId]++;
//
// So a tile counts only when it carries a road type AND its blend byte is 0xff.
// The two writers of those bytes are:
//
//   * RoadTool_PlaceRoadSegment_9c960 (commit) — sets +0x2a = roadType and
//     +0x2b = 0x80, the "queued for construction" marker. A freshly drawn road
//     is therefore NOT counted, by design.
//   * Pf_FinalizeRoadTile_9c640 (the CompleteRoad action, run by a citizen who
//     worked the BuildRoad work item) — overwrites +0x2b with the finished
//     blend value.
//
// Hence: 3,000 tiles drawn with stat 23 still 0 means the tiles are stuck at
// 0x80 and no citizen ever completed a BuildRoad work item. This census proves
// which of the two it is instead of inferring it, by printing the counter array
// AND a histogram of the live (+0x2a, +0x2b) pairs on the map.

#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Road-path counters, incremented from the ported functions themselves so the census
// can say WHERE the chain stops: a builder that takes the work but never runs
// BuildRoad::Execute, an Execute that always bails on its passability guard, or a
// CompleteRoad action that runs and still leaves the tile queued.
extern "C" {
long long g_brExecCalls = 0;
long long g_brExecBusy = 0;        // early-out: this work already has an assignee
long long g_brExecNotPassable = 0; // the citizen cannot reach the tile
long long g_brExecPushedAction = 0;
long long g_crCompleteRoadRuns = 0;
long long g_roadFinalizeRuns = 0;
long long g_wapCompleteRoadBuilt = 0;
long long g_actCompleteTaskRuns = 0;
long long g_actShowToolRuns = 0;
}
extern "C" {
extern long long g_brExecStructOnCell;
extern long long g_brExecRawMatWait;
extern long long g_brExecNatResWait;
extern long long g_brExecClearPath;
}

extern "C" {
extern long long g_idleSearchCalls;
extern long long g_idleSearchByBuilder;
extern long long g_idleBuilderBucketHit;
extern long long g_idleBuilderTookWork;
}

namespace {

int  s_interval = -2;      // -2 = env unread, -1 = disabled

void Init() {
    if (s_interval != -2) return;
    s_interval = -1;
    if (const char* e = std::getenv("BANISHED_ROAD_CENSUS")) {
        int v = std::atoi(e);
        if (v > 0) s_interval = v;
    }
}

// Count the nodes of the pending-road BST at mapGrid+0xe8 (node: key int @+0x00,
// left @+0x10, right @+0x18) — i.e. how many cells still have a road QUEUED.
int CountBst(void* node, int depth) {
    if (!node || depth > 64) return 0;
    uint8_t* n = static_cast<uint8_t*>(node);
    return 1 + CountBst(*reinterpret_cast<void**>(n + 0x10), depth + 1)
             + CountBst(*reinterpret_cast<void**>(n + 0x18), depth + 1);
}


// Walk the work scheduler's per-PROFESSION bucket tree at sched+0x120 (0x38 nodes:
// key u64 @+0x00, inner id-tree @+0x08, inner count @+0x10, left @+0x20, right @+0x28)
// and report each bucket's key and how many work items sit in it.
//
// This is the half of the road path that nothing downstream can see going wrong: a
// citizen only ever sees the items in ITS OWN profession bucket, and the bucket a work
// item lands in is chosen by the work's vtable slot +0x50 ("who can do this"). BuildRoad's
// slot 10 (0x1400e4960) returns *(*(gw+0xb0)+0x38) — the BUILDER key — while the generic
// laborer works (Pickup, GatherRawMaterials) return *(*(gw+0xb0)+0x30). So road work is
// invisible to laborers by design, and a town with no builder never paves a tile.
void DumpBuckets(uint8_t* node, int depth, uint64_t laborerKey, uint64_t builderKey) {
    if (!node || depth > 48) return;
    DumpBuckets(*reinterpret_cast<uint8_t**>(node + 0x20), depth + 1, laborerKey, builderKey);
    const uint64_t key   = *reinterpret_cast<uint64_t*>(node + 0x00);
    const int      count = *reinterpret_cast<int*>(node + 0x10);
    const char* which = (key == laborerKey) ? " (LABORER)"
                      : (key == builderKey) ? " (BUILDER)" : "";
    std::fprintf(stderr, "[RoadCensus]     bucket key=%016llx%s items=%d\n",
                 (unsigned long long)key, which, count);
    DumpBuckets(*reinterpret_cast<uint8_t**>(node + 0x28), depth + 1, laborerKey, builderKey);
}

}  // namespace

extern "C" void RoadProbe_Tick(int frameNum) {
    Init();
    if (s_interval <= 0) return;
    if (frameNum == 0 || (frameNum % s_interval) != 0) return;

    void* gw = (g_pGameWorldSingleton && *g_pGameWorldSingleton) ? *g_pGameWorldSingleton : nullptr;
    if (!gw) { std::fprintf(stderr, "[RoadCensus] f=%d NO GAMEWORLD\n", frameNum); return; }

    uint8_t* mgr = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(gw) + 0x110);
    int*     mapGrid = *reinterpret_cast<int**>(static_cast<uint8_t*>(gw) + 0xf8);
    if (!mgr || !mapGrid) {
        std::fprintf(stderr, "[RoadCensus] f=%d mgr(gw+0x110)=%p mapGrid(gw+0xf8)=%p\n",
                     frameNum, (void*)mgr, (void*)mapGrid);
        return;
    }

    // --- the array stat 23 actually reads ---
    const int   batchCount = *reinterpret_cast<int*>(mgr + 0x218);
    const int   intCount   = *reinterpret_cast<int*>(mgr + 0x228);
    int* const  counters   = *reinterpret_cast<int**>(mgr + 0x220);
    std::fprintf(stderr, "[RoadCensus] ===== f=%d mgr=%p batches(+0x218)=%d counters(+0x228)=%d ptr=%p =====\n",
                 frameNum, (void*)mgr, batchCount, intCount, (void*)counters);
    if (counters) {
        for (int i = 0; i < intCount && i < 16; ++i)
            std::fprintf(stderr, "[RoadCensus]   counter[%d] (== stat 23 for road-type index %d) = %d\n",
                         i, i, counters[i]);
    }

    // --- the live map state those counters are derived from ---
    const int gridDim = *mapGrid;
    uint8_t* terrain = *reinterpret_cast<uint8_t**>(reinterpret_cast<uintptr_t>(mapGrid) + 0x80);
    if (terrain && gridDim > 0) {
        // Per road type: how many cells sit at each blend byte. 0x80 = queued for
        // construction (never counted), 0xff = the value the counter looks for.
        long long total = 0, queued = 0, counted = 0, other = 0;
        int perType[8] = {0};
        int perTypeQueued[8] = {0};
        int perTypeCounted[8] = {0};
        const long long cells = (long long)gridDim * gridDim;
        for (long long i = 0; i < cells; ++i) {
            uint8_t batch = terrain[i * 0x30 + 0x2a];
            if (batch == 0xff) continue;
            uint8_t blend = terrain[i * 0x30 + 0x2b];
            ++total;
            if (batch < 8) ++perType[batch];
            if (blend == 0x80)      { ++queued;  if (batch < 8) ++perTypeQueued[batch]; }
            else if (blend == 0xff) { ++counted; if (batch < 8) ++perTypeCounted[batch]; }
            else                    ++other;
        }
        std::fprintf(stderr, "[RoadCensus]   map %dx%d: %lld road cells — queued(+0x2b=0x80)=%lld "
                             "countable(0xff)=%lld other=%lld\n",
                     gridDim, gridDim, total, queued, counted, other);
        for (int t = 0; t < 8; ++t)
            if (perType[t])
                std::fprintf(stderr, "[RoadCensus]     roadType %d: %d cells (queued=%d countable=%d)\n",
                             t, perType[t], perTypeQueued[t], perTypeCounted[t]);
    } else {
        std::fprintf(stderr, "[RoadCensus]   no terrain array (gridDim=%d terrain=%p)\n",
                     gridDim, (void*)terrain);
    }

    // --- how many cells still have a BuildRoad work item outstanding ---
    void* bstRoot = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(mapGrid) + 0xe8);
    std::fprintf(stderr, "[RoadCensus]   pending-road BST (mapGrid+0xe8) nodes = %d\n",
                 CountBst(bstRoot, 0));
    std::fflush(stderr);

    // --- which profession bucket the queued road work is sitting in ---
    // The work scheduler files every posted item into a per-PROFESSION bucket; a citizen
    // only ever sees its own bucket. BuildRoad's slot 10 returns the BUILDER key, the
    // generic laborer works return the LABORER key — so an empty builder bucket and a
    // town with no builder are the same symptom.
    uint8_t* sched   = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(gw) + 0x118);
    uint8_t* profMgr = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(gw) + 0xb0);
    uint64_t laborerKey = profMgr ? *reinterpret_cast<uint64_t*>(profMgr + 0x30) : 0;
    uint64_t builderKey = profMgr ? *reinterpret_cast<uint64_t*>(profMgr + 0x38) : 0;
    std::fprintf(stderr, "[RoadCensus]   workScheduler(gw+0x118)=%p idMapCount(+0x38)=%d "
                         "profMgr(gw+0xb0)=%p laborerKey=%016llx builderKey=%016llx\n",
                 (void*)sched, sched ? *reinterpret_cast<int*>(sched + 0x38) : -1,
                 (void*)profMgr, (unsigned long long)laborerKey,
                 (unsigned long long)builderKey);
    std::fprintf(stderr, "[RoadCensus]   idle work searches: total=%lld byBuilder=%lld "
                         "builderBucketHit=%lld builderTookWork=%lld\n",
                 g_idleSearchCalls, g_idleSearchByBuilder,
                 g_idleBuilderBucketHit, g_idleBuilderTookWork);
    std::fprintf(stderr, "[RoadCensus]   BuildRoad::Execute calls=%lld (busy=%lld unreachable=%lld actionPushed=%lld) CompleteRoad runs=%lld finalizeRoadTile=%lld\n",
                 g_brExecCalls, g_brExecBusy, g_brExecNotPassable, g_brExecPushedAction,
                 g_crCompleteRoadRuns, g_roadFinalizeRuns);
    std::fprintf(stderr, "[RoadCensus]   CompleteRoad actions built=%lld\n", g_wapCompleteRoadBuilt);
    std::fprintf(stderr, "[RoadCensus]   action runs: CompleteTask=%lld ShowTool=%lld\n",
                 g_actCompleteTaskRuns, g_actShowToolRuns);
    std::fprintf(stderr, "[RoadCensus]   Execute branches: structOnCell=%lld rawMatWait=%lld natResWait=%lld clearPath=%lld\n",
                 g_brExecStructOnCell, g_brExecRawMatWait, g_brExecNatResWait, g_brExecClearPath);
    if (sched) DumpBuckets(*reinterpret_cast<uint8_t**>(sched + 0x120), 0, laborerKey, builderKey);
    std::fflush(stderr);
}
