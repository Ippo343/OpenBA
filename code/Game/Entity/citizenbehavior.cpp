// code/Game/Entity/citizenbehavior.cpp
// Clean reimplementation of CitizenBehavior (population manager).
//
// CitizenBehavior (0x1C0 bytes) — manages all citizens, worker assignment,
// profession statistics, resource shortage notifications, and path distance graphs.
// Lives at GameUpdateLoop+0xF0 (m_citizenList).
//
// Profile source path: Entity\CitizenBehavior.cpp
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Entity/citizenbehavior_funcs.h"
#include "Component/coll_bridge.h"
#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Game/exe_constants.h"   // kRdata_*
#include "Game/force_work.h"   // ForceWork::Readable — CollectStats stale-entry guard
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers: System::Random
#include "Component/StorageComponent.h"
#include "Component/RawMaterialDescription.h"
#include "Component/ResidenceComponent.h"
#include "Component/CitizenDescription.h"
#include "Component/CitizenComponent.h"
#include "Component/Component.h"
#include "Entity/ComponentEntity.h"
#include "Game/ProfessionList.h"
#include "Entity/exe_bodies.h"
#include "Action/exe_bodies.h"      // Pf_IsDstReachable_f810, Pf_EntityAnchorCell_7fb0, PathBST_Insert_25a10, PathPairs_Clear_25b30
#include "Component/exe_bodies.h"   // CitizenEmploy_Strat_c4b0, PathGraph_ReassignWorkers, CitizenAssignCitizens, CitizenComp_AssignHousing, StorageComponent_DynArrayResize
#include "Game/exe_bodies.h"        // EventLog_AddEvent, ResizeStride0x10 / 0x20
#include "Work/exe_bodies.h"        // WorkBase_TreeNodeFree_e320, WorkBase_CalcDistance_8830

// Tick-driven Builder pool (builder_pool_driver.cpp). No-op unless BANISHED_BUILDER_NATURAL
// is set. Called at the end of CitizenBehavior_Update.
extern "C" void BuilderPool_Tick();


// ============================================================
// Internal helpers, called by name (B3R)
// ============================================================
//   ??0Time@System@@QEAA@M@Z  System::Time::Time(float) -> RtSystem_Time_Ctor
//   (system/Value.h). [MANGLING FIX 2026-07-25] this file once asked for
//   "?Time@Time@System@@QEAA@M@Z", which does not exist; the lookup fell through
//   to an ordinal-798 fallback ("??2GroupDescription@UI@@SAPEAX_KPEAX@Z", the
//   PLACEMENT operator new, which writes nothing), so every Time read back 0:
//   the 300 s path-graph branch and CollectStats ran EVERY tick.
//   .rdata 0x140172234 (300.0f, the path-graph interval) and 0x1401721fc (75.0f,
//   the CollectStats interval): exe_constants.h.

// ============================================================
// CitizenBehavior::Update (0x14008c2b0)
// ============================================================
// Main per-frame tick. Called from GameUpdateLoop::Update.
//
// Logic:
//   1. Advance timers: pathAccumTimer += delta, statsTimer -= delta, notifyTimer -= delta
//   2. Call CountResources (tallies resources per profession)
//   3. Call CheckThresholds (fires shortage notifications)
//   4. If pathAccumTimer >= pathGraphInterval:
//      a. If ProcessPaths returns false: call BuildPathGraph
//      b. Else if pending index < 0 and timer > interval: clamp timer
//   5. If statsTimer <= 0: call CollectStats, reset timer
//   6. If notifyTimer < 0: clear notifyFlags, reset timer to 0
//
// Original: Entity_impl_c2b0
void CitizenBehavior::Update(int64_t* deltaTime) {
    // Advance timers
    field_0x190 += *deltaTime;       // pathAccumTimer
    field_0x198 -= *deltaTime;       // statsTimer
    field_0x1A0 -= *deltaTime;       // notifyTimer

    // CountResources â€” Entity_helper_9ed0(self)
    CountResources();

    // CheckThresholds â€” Entity_helper_c100(self, deltaTime)
    CheckThresholds(deltaTime);

    // Path graph processing
    // DAT_140172234 is the path graph interval (float seconds)
    // System::Time::Time constructs a Time value from float
    System::Time pathIntervalTime;
    pathIntervalTime._ticks = 0;
    RtSystem_Time_Ctor(&pathIntervalTime, kRdata_140172234);   // 0x140172234
    const int64_t pathInterval = pathIntervalTime._ticks;


    if (pathInterval <= field_0x190) {
        // Try to process pending paths. The null tests on the path graph are the
        // tree's (the ctor always allocates it; retail has none): listed, kept.
        bool pathsDone = false;
        PathGraph* pg = pathGraph;
        pathsDone = pg != nullptr && pg->ProcessPaths();

        if (!pathsDone) {
            // Build new path graph
            if (pg) pg->BuildPathGraph();
        } else if (pg && pg->m_pendingIdx < 0 && pathInterval < field_0x190) {
            // Clamp accumulated time: subtract intervals until within one interval.
            // Guard against pathInterval <= 0 (BANISHED_PATHGRAPH_FAST forces 0, or a failed
            // interval resolve) â€” `acc -= 0` would spin forever. This branch is reached only
            // when ProcessPaths returns true (pairs drained), which requires townMgr+0x100 to
            // be non-empty (e.g. a registered construction site) â€” so it stayed latent until
            // the natural builder flow first populated that BST.
            if (pathInterval > 0) {
                int64_t acc = field_0x190;
                do {
                    acc -= pathInterval;
                } while (pathInterval < acc);
                field_0x190 = acc;
            } else {
                field_0x190 = 0;   // no positive interval to modulo by -> reset the accumulator
            }
        }
    }

    // Stats collection (periodic)
    if (field_0x198 < 1) {
        CollectStats();
        // Reset timer from DAT_1401721fc (float seconds)
        {
            System::Time newTime;
            newTime._ticks = 0;
            RtSystem_Time_Ctor(&newTime, kRdata_1401721fc);   // 0x1401721fc
            field_0x198 = newTime._ticks;
        }
    }

    // Notification timer reset: retail stores a dword 0 into +0x1A8 (the float's bits)
    if (field_0x1A0 < 0) {
        field_0x1A8 = 0.0f;
        field_0x1A0 = 0;
    }

    // Tick-driven Builder pool (BANISHED_BUILDER_NATURAL, opt-in): keep registered
    // construction sites staffed by seating idle Laborers as Builders via the real
    // FindAvailableBuilding cascade, nearest-first, capped by site capacity. No-op unless
    // the natural flow is enabled. This is the headless stand-in for the player's town-hall
    // Builder count â€” the engine has no player-free first-builder seater (the periodic
    // employment allocator only rebalances already-staffed sites). See builder_pool_driver.cpp.
    BuilderPool_Tick();
}


// ============================================================
// CitizenBehavior::ProcessPaths (0x1400d6f00)
// ============================================================
// Processes up to 7 pending path distance calculations per call.
// For each pair (cellA, cellB), computes path cost via fn_Map_CalcPathCost.
// If cost < 2 (unreachable), falls back to Euclidean grid distance.
// Stores result in a BST keyed by (max(regionA, regionB), min(regionA, regionB)).
// When all pairs are processed (pendingIdx < 0), triggers worker reassignment
// via fn_PathGraph_ReassignWorkers and fn_PathGraph_AssignCitizens.
//
// Original: FUN_1400d6f00
bool PathGraph::ProcessPaths() {
    PathGraph* pg = this;
    int pendingIdx = pg->m_pendingIdx;

    // Nothing to process
    if (pendingIdx < 0) {
        return false;
    }

    // Process at most 7 pairs per call (indices 0..6)
    int remaining = pendingIdx;
    if (remaining > 6) {
        remaining = 6;
    }

    // Get map grid pointer: *(int**)(g_pGameWorldSingleton + 0xf8)
    // (GameUpdateLoop::m_game is still void*: LEFT)
    int* mapGrid = reinterpret_cast<int*>(GameUpdateLoop::_singleton->m_game);

    // Pairs array: each entry is {int32 cellA, int32 cellB} = 8 bytes
    int32_t* pairs = reinterpret_cast<int32_t*>(pg->m_pairs);

    for (int i = remaining; i >= 0; i--) {
        int idx = pg->m_pendingIdx;
        int32_t cellA = pairs[idx * 2];
        int32_t cellB = pairs[idx * 2 + 1];

        // Compute path cost between cells
        uint32_t cost = 0;
        cost = (uint32_t)Pf_IsDstReachable_f810(mapGrid, (uint32_t)cellA,
                                                (uint32_t)cellB, 0);   // 0x14009f810

        // If cost < 2 (unreachable or adjacent), fall back to Euclidean grid distance
        if (cost < 2) {
            int mapWidth = *mapGrid;
            int dx = cellA / mapWidth - cellB / mapWidth;
            int dy = cellA % mapWidth - cellB % mapWidth;
            float dist = sqrtf((float)(dx * dx + dy * dy));
            cost = (uint32_t)dist;
        }

        // Get region IDs from nav data: *(int64*)(mapGrid+0x20) = navData
        // Region at navData + 0x18 + cell * 0x30
        int64_t navData = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uint8_t*>(mapGrid) + 0x80);  // piVar3+0x20 in qword = +0x80 bytes
        int regionA = *reinterpret_cast<int*>(navData + 0x18 + (int64_t)cellA * 0x30);
        int regionB = *reinterpret_cast<int*>(navData + 0x18 + (int64_t)cellB * 0x30);

        // BST key: (max, min) of region IDs
        int keyHigh = regionA;
        int keyLow = regionB;
        if (regionA < regionB) {
            keyHigh = regionB;
            keyLow = regionA;
        }

        // Search BST for existing entry
        uint32_t* foundNode = nullptr;
        uint32_t* bstNode = reinterpret_cast<uint32_t*>(pg->m_bstRoot);
        while (bstNode != nullptr) {
            int nodeHigh = (int)bstNode[0];
            if (keyHigh < nodeHigh) {
                // Go left: bstNode[6] = left child (offset +0x18)
                bstNode = *reinterpret_cast<uint32_t**>(&bstNode[6]);
            } else if (keyHigh > nodeHigh) {
                // Check low key
                if (keyHigh == nodeHigh) {
                    // Same high, check low
                    if (keyLow < (int)bstNode[1]) {
                        bstNode = *reinterpret_cast<uint32_t**>(&bstNode[6]);
                    } else {
                        foundNode = bstNode;
                        break;
                    }
                } else {
                    // Go right: bstNode[8] = right child (offset +0x20)
                    bstNode = *reinterpret_cast<uint32_t**>(&bstNode[8]);
                }
            } else {
                // keyHigh == nodeHigh
                if (keyLow < (int)bstNode[1]) {
                    bstNode = *reinterpret_cast<uint32_t**>(&bstNode[6]);
                } else if (keyLow > (int)bstNode[1]) {
                    bstNode = *reinterpret_cast<uint32_t**>(&bstNode[8]);
                } else {
                    foundNode = bstNode;
                    break;
                }
            }
        }

        // If not found, insert new BST node
        if (foundNode == nullptr) {
            struct { int high; int low; int value; } key;
            key.high = keyHigh;
            key.low = keyLow;
            key.value = 0;

            void* outNode = nullptr;
            PathBST_Insert_25a10(   // 0x140025a10
                reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(pg) + 0x18),  // &pg->m_bstRoot
                reinterpret_cast<uint64_t*>(&outNode),
                reinterpret_cast<uint32_t*>(&key));
            foundNode = reinterpret_cast<uint32_t*>(outNode);
        }

        // Store the computed distance
        if (foundNode) {
            foundNode[2] = cost;
        }

        // Decrement pending index
        pg->m_pendingIdx = pg->m_pendingIdx - 1;
    }

    // If all pairs processed, run post-processing
    if (pg->m_pendingIdx < 0) {
        // Worker reassignment based on computed distances
        PathGraph_ReassignWorkers(pg);   // 0x1400d62a0
        // Citizen assignment sweep
        CitizenAssignCitizens(pg);   // 0x1400d43b0
        // Clear the pairs array
        PathPairs_Clear_25b30(reinterpret_cast<int64_t*>(pg));   // 0x140025b30
    }

    return true;
}

// ============================================================
// CitizenBehavior::BuildPathGraph (0x1400d70c0)
// ============================================================
// Rebuilds the path distance graph when no computation is pending.
// 1. Collects grid cell IDs for all buildings
// 2. Clears old BST distance cache
// 3. Clears pairs array
// 4. For each profession group, for each entity, generates (entity_cell, building_cell)
//    pairs with all buildings, skipping pairs that already have a BST entry
// 5. Sets pendingIdx = pairCount - 1 to begin processing
//
// Original: Entity_impl_70c0
void PathGraph::BuildPathGraph() {
    PathGraph* pg = this;
    // Only rebuild when not currently processing
    if (pg->m_pendingIdx >= 0) {
        return;
    }

    int dbgEntities = 0, dbgEntRegionOk = 0, dbgPairsAdded = 0;

    // Get key pointers from the game world singleton
    // (GameUpdateLoop::m_game and m_citizenList are still void*: LEFT)
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    int* mapGrid = reinterpret_cast<int*>(world->m_game);
    uint8_t* citizenBehavior = reinterpret_cast<uint8_t*>(world->m_citizenList);
    ProfessionList* buildingList = world->m_professionList;

    // ---- Step 1: Collect grid cell IDs for all buildings ----
    // Retail (0x1400d70c0): lVar3 = *(gw+0xf0), loop over *(int*)(lVar3+0x28)
    // entries of *(longlong*)(lVar3+0x20) — the CitizenBehavior/town RESIDENCE
    // list, NOT gw+0xb0 (that is the ProfessionList, used only in Step 4 keys).
    int buildingCount = *reinterpret_cast<int*>(citizenBehavior + 0x28);
    int64_t* buildingArray = *reinterpret_cast<int64_t**>(citizenBehavior + 0x20);

    // Dynamic array for building grid IDs
    int* buildingCells = nullptr;
    int cellCount = 0;
    int cellCapacity = 0;

    for (int i = 0; i < buildingCount; i++) {
        // Get InternalBasePtr for this building
        int64_t entityIBP = *reinterpret_cast<int64_t*>(
            reinterpret_cast<uint8_t*>(buildingArray) + i * 8);

        // Get grid cell ID for the building
        int gridId = -1;
        gridId = Pf_EntityAnchorCell_7fb0(&entityIBP, nullptr);   // 0x140087fb0

        // Grow array if needed (same pattern as decompiled code)
        int newCount = cellCount + 1;
        if (newCount != cellCapacity) {
            int newCap = cellCapacity;
            if (cellCount + 1 > cellCapacity) {
                newCap = newCount;
                if (cellCapacity != 0 && newCount == cellCapacity + 1) {
                    newCap = newCount * 3;
                }
                int* newArr = reinterpret_cast<int*>(
                    System::MemoryAllocator::MemAlloc((int64_t)newCap * 4, 4));

                // Copy existing + new entry
                int copyStart = (buildingCells == newArr) ? cellCount : 0;
                for (int j = copyStart; j < newCount; j++) {
                    if (newArr) {
                        if (j < cellCount && buildingCells) {
                            newArr[j] = buildingCells[j];
                        } else {
                            newArr[j] = gridId;
                        }
                    }
                }

                if (buildingCells != newArr && buildingCells) {
                    System::MemoryAllocator::MemFree(buildingCells);
                }
                buildingCells = newArr;
                cellCapacity = newCap;
            } else {
                // Capacity is sufficient, just copy
                int copyStart = 0;
                for (int j = copyStart; j < newCount; j++) {
                    if (buildingCells) {
                        if (j < cellCount) {
                            // Already in place
                        } else {
                            buildingCells[j] = gridId;
                        }
                    }
                }
            }
            cellCount = newCount;
        }
    }

    // ---- Step 2: Clear old BST ----
    void* oldBst = pg->m_bstRoot;
    if (oldBst != nullptr) {
        // Recursively free left and right subtrees
        WorkBase_TreeNodeFree_e320(   // 0x14000e320
            reinterpret_cast<uint8_t*>(pg) + 0x18,  // &pg->m_bstRoot
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(oldBst) + 0x18));
        WorkBase_TreeNodeFree_e320(
            reinterpret_cast<uint8_t*>(pg) + 0x18,
            *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(oldBst) + 0x20));
        System::MemoryAllocator::MemFree(oldBst);
    }
    pg->m_bstRoot = nullptr;
    pg->m_bstCount = 0;

    // ---- Step 3: Clear pairs array ----
    PathPairs_Clear_25b30(reinterpret_cast<int64_t*>(pg));   // 0x140025b30

    // ---- Step 4: Generate pairs from profession entities x buildings ----
    // Nav data for region lookups
    int64_t navData = *reinterpret_cast<int64_t*>(
        reinterpret_cast<uint8_t*>(mapGrid) + 0x80);

    // Building list at citizenBehavior+0xb0 (via game world +0xb0)
    int profGroupCount = buildingList->professions.GetSize();


    for (int gi = 0; gi < profGroupCount; gi++) {
        // Get profession key from building list
        // (ProfessionList keeps its Array as raw storage, H5: LEFT)
        uint64_t profKey = reinterpret_cast<uint64_t*>(buildingList->professions.GetData())[gi];

        // Look up profession BST at citizenBehavior+0x100
        uint64_t* profNode = *reinterpret_cast<uint64_t**>(citizenBehavior + 0x100);
        int entityCount = 0;
        while (profNode != nullptr) {
            if (profKey < profNode[0]) {
                profNode = reinterpret_cast<uint64_t*>(profNode[4]);
            } else if (profKey > profNode[0]) {
                profNode = reinterpret_cast<uint64_t*>(profNode[5]);
            } else {
                entityCount = (int)profNode[2];
                break;
            }
        }

        // For each entity in this profession group
        for (int64_t ei = 0; ei < entityCount; ei++) {
            // Re-lookup profession BST (same pattern as decompiled code)
            uint64_t* lookupNode = *reinterpret_cast<uint64_t**>(citizenBehavior + 0x100);
            uint64_t* foundNode = nullptr;
            while (lookupNode != nullptr) {
                if (profKey < lookupNode[0]) {
                    lookupNode = reinterpret_cast<uint64_t*>(lookupNode[4]);
                } else if (profKey > lookupNode[0]) {
                    lookupNode = reinterpret_cast<uint64_t*>(lookupNode[5]);
                } else {
                    foundNode = lookupNode;
                    break;
                }
            }
            if (!foundNode) break;

            // Get entity IBP from the profession node's entity array
            int64_t entityIBP = *reinterpret_cast<int64_t*>(foundNode[1] + ei * 8);

            // Get grid cell ID for this entity
            int entityCell = -1;
            entityCell = Pf_EntityAnchorCell_7fb0(&entityIBP, nullptr);   // 0x140087fb0

            // Get entity's region ID
            int entityRegion = *reinterpret_cast<int*>(
                navData + 0x18 + (int64_t)(uint32_t)entityCell * 0x30);
            ++dbgEntities;
            if (entityRegion >= 0) ++dbgEntRegionOk;

            // Pair with each building cell
            if (entityRegion >= 0 && buildingCells != nullptr) {
                for (int bi = 0; bi < cellCount; bi++) {
                    int buildingCell = buildingCells[bi];
                    int buildingRegion = *reinterpret_cast<int*>(
                        navData + 0x18 + (int64_t)buildingCell * 0x30);

                    if (buildingRegion < 0) continue;

                    // BST key: (max, min) of region IDs
                    int keyHigh = entityRegion;
                    int keyLow = buildingRegion;
                    if (entityRegion < buildingRegion) {
                        keyHigh = buildingRegion;
                        keyLow = entityRegion;
                    }

                    // Check if this pair already exists in BST
                    int* bstNode = reinterpret_cast<int*>(pg->m_bstRoot);
                    bool found = false;
                    while (bstNode != nullptr) {
                        int nodeHigh = bstNode[0];
                        if (keyHigh < nodeHigh) {
                            bstNode = *reinterpret_cast<int**>(&bstNode[6]);
                        } else if (keyHigh > nodeHigh) {
                            bstNode = *reinterpret_cast<int**>(&bstNode[8]);
                        } else {
                            // keyHigh == nodeHigh
                            if (keyLow < bstNode[1]) {
                                bstNode = *reinterpret_cast<int**>(&bstNode[6]);
                            } else if (keyLow > bstNode[1]) {
                                bstNode = *reinterpret_cast<int**>(&bstNode[8]);
                            } else {
                                found = true;
                                break;
                            }
                        }
                    }

                    if (found) continue;

                    // Add pair to pairs array
                    // Each pair is {int entityCell, int buildingCell} = 8 bytes
                    int32_t pairData[2] = { entityCell, buildingCell };
                    int curPairCount = (int)pg->m_pairCount;
                    int newPairCount = curPairCount + 1;

                    // Grow pairs array (same dynamic array pattern)
                    void* oldPairs = pg->m_pairs;
                    pg->m_pairCount = newPairCount;
                    int pairCap = pg->m_pairCapacity;
                    if (pairCap < newPairCount) {
                        int newCap = newPairCount;
                        if (curPairCount != 0) {
                            newCap = newPairCount * 3;
                        }
                        pg->m_pairCapacity = newCap;
                        PathCellPair* newArr = static_cast<PathCellPair*>(
                            System::MemoryAllocator::MemAlloc((int64_t)newCap * 8, 4));
                        pg->m_pairs = newArr;
                    }

                    // Copy existing entries + new
                    int copyStart = (oldPairs == pg->m_pairs) ? curPairCount : 0;
                    for (int k = copyStart; k < newPairCount; k++) {
                        int64_t* dest = reinterpret_cast<int64_t*>(pg->m_pairs);
                        if (dest) {
                            if (k < curPairCount && oldPairs) {
                                dest[k] = reinterpret_cast<int64_t*>(oldPairs)[k];
                            } else {
                                dest[k] = *reinterpret_cast<int64_t*>(pairData);
                            }
                        }
                    }

                    if (oldPairs != pg->m_pairs && oldPairs) {
                        System::MemoryAllocator::MemFree(oldPairs);
                    }
                    ++dbgPairsAdded;

                    // Insert placeholder into BST to prevent duplicate pairs
                    struct { int high; int low; int value; } bstKey;
                    bstKey.high = entityRegion;
                    bstKey.low = buildingRegion;
                    if (entityRegion < buildingRegion) {
                        bstKey.high = buildingRegion;
                        bstKey.low = entityRegion;
                    }
                    bstKey.value = 0;

                    {
                        void* dummy = nullptr;
                        PathBST_Insert_25a10(   // 0x140025a10
                            reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(pg) + 0x18),
                            reinterpret_cast<uint64_t*>(&dummy),
                            reinterpret_cast<uint32_t*>(&bstKey));
                    }
                }
            }
        }
    }

    // ---- Step 5: Set pending index ----
    pg->m_pendingIdx = (int)pg->m_pairCount - 1;


    // Free temporary building cells array
    if (buildingCells) {
        System::MemoryAllocator::MemFree(buildingCells);
    }
}
// ============================================================
// CitizenBehavior::CountResources (0x140089ed0)
// ============================================================
// Iterates two entity arrays (workplaces at +0x00/+0x08 and buildings
// at +0x10/+0x18), finds StorageComponent on each entity, then iterates
// stored items to find RawMaterialDescription and tally resource counts
// by profession type (RawMaterialFlags at +0x5c of the description).
//
// Results are accumulated into the ProfessionResourceEntry[21] array
// at +0x170 (profResources). Before counting, previousCount is set to
// currentCount and currentCount is zeroed for each entry.
//
// Also tracks food vs durable-goods totals (flags & 0x1c40 split) in the
// notification summary at +0x180 (param_1[0x30] in Ghidra longlong* indexing).
//
// The RawMaterialFlags -> entry mapping (the bit->offset chain below is
// DISASM-FAITHFUL — verified against 0x140089ed0 on 2026-07-19. NOTE: the
// flag/profession NAMES were derived from the old wrong toolkit-order flag
// table; the real bit 5 = EDIBLE (not Fuel), bit 6 = Fuel — and bit 6 is not
// tallied here at all. See engine/types/RawMaterialDescription.h.):
//   bit 5  (Edible)    -> entry 5                  offset 0xa0
//   bit 7  (Wood)      -> entry 3  (Fisherman)    offset 0x60
//   bit 8  (Stone)     -> entry 9  (Stonecutter)  offset 0x120
//   bit 9  (Iron)      -> entry 4  (Hunter)       offset 0x80
//   bit 10 (Health)    -> entry 0  (Laborer)       offset 0x00
//   bit 11 (Clothing)  -> entry 1  (Builder)       offset 0x20
//   bit 12 (Textile)   -> entry 2  (Farmer)        offset 0x40
//   bit 13 (Alcohol)   -> entry 6  (Forester)      offset 0xc0
//   bit 14 (Custom0)   -> entry 7  (Woodcutter)    offset 0xe0
//   bit 17 (Custom3)   -> entry 8  (Blacksmith)    offset 0x100
//   bit 16 (Custom2)   -> entry 10 (Tailor)        offset 0x140
//   bit 18 (Custom4)   -> entry 11 (Doctor)        offset 0x160
//   bit 19 (Custom5)   -> entry 12 (Teacher)       offset 0x180
//   bit 20 (Custom6)   -> entry 13 (Cleric)        offset 0x1a0
//   bit 21 (Custom7)   -> entry 14 (Innkeeper)     offset 0x1c0
//   bit 22 (Custom8)   -> entry 15 (Vendor)        offset 0x1e0
//   bit 23 (Custom9)   -> entry 16 (Miner)         offset 0x200
//   bit 24             -> entry 17 (Pastor)        offset 0x220
//   bit 25             -> entry 18 (Trader)        offset 0x240
//   bit 26             -> entry 19 (Herdsman)      offset 0x260
//   bit 27             -> entry 20 (Farmer2)       offset 0x280
//
// Original: Entity_helper_9ed0
void CitizenBehavior::CountResources() {
    // --- Phase 0: Shift currentCount -> previousCount and zero currentCount ---
    // The profResources array is 21 entries of 0x20 bytes each (total 0x2A0).
    ProfessionResourceEntry* profRes = professionResources;
    if (profRes) {
        for (int e = 0; e < 21; e++) {
            profRes[e].previousCount = profRes[e].currentCount;
            profRes[e].currentCount = 0;
        }
    }

    // --- Zero notification summary at +0x180 (the two ResourceNotifyEntry records:
    // [0] = normal resources, [1] = tools / health / clothing / textile) ---
    ResourceNotifyEntry* notifySummary = notifyEntries;
    if (notifySummary) {
        notifySummary[0].maxCapacity = 0;  // +0x04: normal resource worker count
        notifySummary[0].current = 0;      // +0x00: normal resource total
        notifySummary[1].maxCapacity = 0;  // +0x14: dead resource worker count
        notifySummary[1].current = 0;      // +0x10: dead resource total
    }

    // Helper lambda: find a component on an entity by type ID.
    // Returns the found component, or nullptr. The null tests are the tree's.
    auto findComponent = [](ComponentEntity* entity, uint32_t targetId) -> Component* {
        if (!entity) return nullptr;
        int compCount = entity->_componentCount;
        for (int i = 0; i < compCount; i++) {
            Component* comp = entity->_components[i].Get();
            if (!comp) continue;
            // virtual slot 2 (+0x10): Type()
            int compTypeId = static_cast<int>(comp->Type());
            if (static_cast<uint32_t>(compTypeId) == targetId) {
                if (::Object::IsDerivedFromNullSafe(comp, targetId)) {
                    return comp;
                }
                break;
            }
        }
        return nullptr;
    };

    // Helper lambda: given RawMaterialFlags, add itemCount to the correct
    // profession entry in profResources.
    auto addToProfession = [&](uint32_t flags, int32_t itemCount) {
        int entry = -1;
        if (flags & 0x20) {
            // bit 5 (Edible) -> entry 5, offset 0xa0
            entry = 5;
        } else if (flags & 0x80) {
            // bit 7 (Wood) -> entry 3, offset 0x60
            entry = 3;
        } else if (flags & 0x100) {
            // bit 8 (Stone) -> entry 9, offset 0x120
            entry = 9;
        } else if (flags & 0x200) {
            // bit 9 (Iron) -> entry 4, offset 0x80
            entry = 4;
        } else if (flags & 0x400) {
            // bit 10 (Health) -> entry 0, offset 0x00
            entry = 0;
        } else if (flags & 0x800) {
            // bit 11 (Clothing) -> entry 1, offset 0x20
            entry = 1;
        } else if (flags & 0x1000) {
            // bit 12 (Textile) -> entry 2, offset 0x40
            entry = 2;
        } else if (flags & 0x2000) {
            // bit 13 (Alcohol) -> entry 6, offset 0xc0
            entry = 6;
        } else if (flags & 0x4000) {
            // bit 14 (Custom0) -> entry 7, offset 0xe0
            entry = 7;
        } else if (flags & 0x20000) {
            // bit 17 (Custom3) -> entry 8, offset 0x100
            entry = 8;
        } else if (flags & 0x10000) {
            // bit 16 (Custom2) -> entry 10, offset 0x140
            entry = 10;
        } else if (flags & 0x40000) {
            // bit 18 (Custom4) -> entry 11, offset 0x160
            entry = 11;
        } else if (flags & 0x80000) {
            // bit 19 (Custom5) -> entry 12, offset 0x180
            entry = 12;
        } else if (flags & 0x100000) {
            // bit 20 (Custom6) -> entry 13, offset 0x1a0
            entry = 13;
        } else if (flags & 0x200000) {
            // bit 21 (Custom7) -> entry 14, offset 0x1c0
            entry = 14;
        } else if (flags & 0x400000) {
            // bit 22 (Custom8) -> entry 15, offset 0x1e0
            entry = 15;
        } else if (flags & 0x800000) {
            // bit 23 (Custom9) -> entry 16, offset 0x200
            entry = 16;
        } else if (flags & 0x1000000) {
            // bit 24 -> entry 17, offset 0x220
            entry = 17;
        } else if (flags & 0x2000000) {
            // bit 25 -> entry 18, offset 0x240
            entry = 18;
        } else if (flags & 0x4000000) {
            // bit 26 -> entry 19, offset 0x260
            entry = 19;
        } else if (flags & 0x8000000) {
            // bit 27 -> entry 20, offset 0x280
            entry = 20;
        }

        if (entry >= 0 && profRes) {
            profRes[entry].currentCount += itemCount;
        }
    };

    // --- Phase 1: Iterate first entity array (+0x00/+0x08) ---
    // These entities have StorageComponent; their items are tallied for both
    // profession resource counts AND notification summaries.
    int64_t firstCount = static_cast<int64_t>(field_0x08);
    for (int64_t idx = 0; idx < firstCount; idx++) {
        // Find StorageComponent on this entity
        Component* storageComp = findComponent(field_0x00[idx], StorageComponent::TypeId());
        // (found by the type id and IsDerivedFrom above: B10)
        StorageComponent* storage = static_cast<StorageComponent*>(storageComp);

        // Read item count from storage (+0x30)
        int32_t itemCount = storage->slotCount;
        if (itemCount <= 0) continue;

        bool hadNormalResources = false;
        bool hadDeadResources = false;

        // Iterate storage items: array at +0x28, stride 0x10
        // Each item: [0x00] = item description ptr, [0x08] = quantity (int cast from longlong)
        // StorageComponent.h keeps the item array as raw storage (H5): LEFT.
        for (int32_t j = 0; j < itemCount; j++) {
            int64_t* itemEntry = reinterpret_cast<int64_t*>(
                reinterpret_cast<int64_t>(storage->storageSlots) + j * 0x10);

            // Find RawMaterialDescription on the item entity
            // Item entity has components at +0x28/+0x30 (different from top-level entity +0x30/+0x38)
            // The item's first qword is a description whose child list is at
            // +0x28 / count +0x30 (its class is not settled here: LEFT).
            int64_t itemEntity = itemEntry[0];
            RawMaterialDescription* rawMat = nullptr;
            if (itemEntity) {
                int subCompCount = *reinterpret_cast<int*>(itemEntity + 0x30);
                uint32_t rawMatTypeId = RawMaterialDescription::TypeId();
                for (int k = 0; k < subCompCount; k++) {
                    Object::Base* subComp = *reinterpret_cast<Object::Base**>(
                        *reinterpret_cast<int64_t*>(itemEntity + 0x28) + k * 8);
                    if (!subComp) continue;
                    int compTypeId = static_cast<int>(subComp->Type());
                    if (static_cast<uint32_t>(compTypeId) == rawMatTypeId) {
                        if (::Object::IsDerivedFromNullSafe(subComp, rawMatTypeId)) {
                            rawMat = static_cast<RawMaterialDescription*>(subComp);
                        }
                        break;
                    }
                }
            }
            if (!rawMat) continue;

            // Weight per unit (+0x48) times the item quantity
            int32_t weightPerUnit = rawMat->_weight;
            int32_t quantity = static_cast<int32_t>(itemEntry[1]);
            int32_t totalAmount = weightPerUnit * quantity;

            // RawMaterialFlags (+0x5c)
            uint32_t flags = rawMat->_flags;

            // Update notification summary: split by flag 0x1c40
            // 0x1c40 = Tool(6) | Health(10) | Clothing(11) | Textile(12)
            if (notifySummary) {
                if ((flags & 0x1c40) == 0) {
                    // Normal resources (food, fuel, etc.)
                    notifySummary[0].current += totalAmount;
                    hadNormalResources = true;
                } else {
                    // Tools/health/clothing/textile
                    notifySummary[1].current += totalAmount;
                    hadDeadResources = true;
                }
            }

            // Categorize by profession flag
            addToProfession(flags, static_cast<int32_t>(itemEntry[1]));
        }

        // Update notification worker counts from storage+0x38 (capacity/worker count)
        if (hadDeadResources && notifySummary) {
            notifySummary[1].maxCapacity += storage->capacity;
        }
        if (hadNormalResources && notifySummary) {
            notifySummary[0].maxCapacity += storage->capacity;
        }
    }

    // --- Phase 2: Iterate second entity array (+0x10/+0x18) ---
    // Same logic as Phase 1 but WITHOUT notification summary updates.
    int64_t secondCount = static_cast<int64_t>(field_0x18);
    for (int64_t idx = 0; idx < secondCount; idx++) {
        // Find StorageComponent on this entity
        Component* storageComp = findComponent(field_0x10[idx], StorageComponent::TypeId());
        StorageComponent* storage = static_cast<StorageComponent*>(storageComp);

        // Read item count from storage (+0x30)
        int32_t itemCount = storage->slotCount;
        if (itemCount <= 0) continue;

        // Iterate storage items
        for (int32_t j = 0; j < itemCount; j++) {
            int64_t* itemEntry = reinterpret_cast<int64_t*>(
                reinterpret_cast<int64_t>(storage->storageSlots) + j * 0x10);

            // Find RawMaterialDescription on the item entity
            int64_t itemEntity = itemEntry[0];
            RawMaterialDescription* rawMat = nullptr;
            if (itemEntity) {
                uint32_t rawMatTypeId = RawMaterialDescription::TypeId();
                int subCompCount = *reinterpret_cast<int*>(itemEntity + 0x30);
                for (int k = 0; k < subCompCount; k++) {
                    Object::Base* subComp = *reinterpret_cast<Object::Base**>(
                        *reinterpret_cast<int64_t*>(itemEntity + 0x28) + k * 8);
                    if (!subComp) continue;
                    int compTypeId = static_cast<int>(subComp->Type());
                    if (static_cast<uint32_t>(compTypeId) == rawMatTypeId) {
                        if (::Object::IsDerivedFromNullSafe(subComp, rawMatTypeId)) {
                            rawMat = static_cast<RawMaterialDescription*>(subComp);
                        }
                        break;
                    }
                }
            }
            if (!rawMat) continue;

            // RawMaterialFlags (+0x5c); categorize by profession flag (no notification
            // update in Phase 2)
            addToProfession(rawMat->_flags, static_cast<int32_t>(itemEntry[1]));
        }
    }
}

// ============================================================
// CitizenBehavior::CheckThresholds (0x14008c100)
// ============================================================
// Checks resource shortage thresholds for each profession and fires
// notification events when resources drop critically low.
//
// Logic:
//   Part 1 — Profession resources (21 entries at +0x170, 0x20 bytes each):
//     For each entry where timer <= 0 AND index < 6:
//       - threshold = maxCount / 20 (5% of max)
//       - For index 3 (Fisherman): combine with profession 9 (Stonecutter) values
//       - If currentCount < threshold AND threshold <= previousCount:
//           fire notification type (index + 0x11), severity 3
//           reset timer to 120 seconds
//
//   Part 2 — Resource notify entries (2 at +0x180, 0x10 bytes each):
//     For each entry where timer <= 0:
//       - If current > (maxCapacity / 20) * 19 (95% full):
//           fire notification type 0x17 (entry 0) or 0x18 (entry 1), severity 3
//           reset timer to 120 seconds
//
// Original: Entity_helper_c100
void CitizenBehavior::CheckThresholds(int64_t* deltaTime) {
    float notifyInterval = 120.0f; // DAT_140172210 = 120.0f seconds
    // GameUpdateLoop +0x138 is the event notifier (still void* in GameUpdateLoop.h)
    void* notifMgr = GameUpdateLoop::_singleton->m_eventNotifier;

    // Part 1: Check profession resource thresholds (21 entries)
    for (int i = 0; i < 21; i++) {
        int64_t* timer = &professionResources[i].timer;

        // Decrement timer
        *timer -= *deltaTime;

        if (*timer < 1 && static_cast<uint32_t>(i) < 6) {
            // Reset timer to zero (will be set to interval if notification fires)
            *timer = 0;

            int32_t maxCount = professionResources[i].maxCount;
            int32_t currentCount = professionResources[i].currentCount;
            int32_t previousCount = professionResources[i].previousCount;

            // Profession 3 (Fisherman): combine with profession 9 (Stonecutter)
            if (i == 3) {
                maxCount += professionResources[9].maxCount;
                currentCount += professionResources[9].currentCount;
                previousCount += professionResources[9].previousCount;
            }

            int32_t threshold = maxCount / 20; // 5% threshold
            if (currentCount < threshold && threshold <= previousCount) {
                // Fire shortage notification (with a null entity InternalBasePtr)
                {
                    void* nullIBP = nullptr;
                    EventLog_AddEvent(notifMgr, i + 0x11, 3, &nullIBP);   // 0x140095580
                }
                // Reset timer to notification cooldown (120 seconds)
                {
                    System::Time newTime;
                    newTime._ticks = 0;
                    RtSystem_Time_Ctor(&newTime, notifyInterval);
                    *timer = newTime._ticks;
                }
            }
        }
    }

    // Part 2: Check resource notify entries (2 entries, food/firewood storage).
    // [DEREF FIX] +0x180 is a POINTER to the heap-allocated 2-entry array
    // (fn_SummaryArray_Init(self->data + 0x180, 2) in the ctor; original:
    // `lVar3 = *(longlong*)(param_1 + 0x180)`). The previous code used the field
    // address itself as the array, so "current" read the pointer's low dword
    // (huge -> 95%-full always true) and the "timer" writes stomped +0x188 â€”
    // that was the type-24 event spam (thousands of storage-full posts/minute).
    for (int i = 0; i < 2; i++) {
        ResourceNotifyEntry* notifyBase = notifyEntries;
        if (!notifyBase) break;
        int64_t* timer = &notifyBase[i].timer;

        // Decrement timer
        *timer -= *deltaTime;

        if (*timer < 1) {
            *timer = 0;

            int32_t current = notifyBase[i].current;
            int32_t maxCapacity = notifyBase[i].maxCapacity;

            // Check if storage is > 95% full: current > (max/20)*19
            if ((maxCapacity / 20) * 19 < current) {
                // Notification type: 0x17 for food (entry 0), 0x18 for firewood (entry 1)
                int notifType = 0x17 + (i != 0 ? 1 : 0);
                {
                    void* nullIBP = nullptr;
                    EventLog_AddEvent(notifMgr, notifType, 3, &nullIBP);   // 0x140095580
                }
                {
                    System::Time newTime;
                    newTime._ticks = 0;
                    RtSystem_Time_Ctor(&newTime, notifyInterval);
                    *timer = newTime._ticks;
                }
            }
        }
    }
}


// ============================================================
// CitizenBehavior::CollectStats (0x14008a730)
// ============================================================
// Iterates all citizens, collects population statistics (alive, dead,
// educated, working, idle, happiness, health averages), then pushes
// ~30+ stat values to the stats history arrays at +0x150.
//
// Stats history layout (at *(param_1+0x150)):
//   +0x00..+0x70: 8 population stats (each is a 0x10-byte sub-array struct)
//     +0x00: total citizen count
//     +0x10: adults (alive, age >= adult)
//     +0x20: dead citizens
//     +0x30: children (alive, age < adult)
//     +0x40: happiness percentage (0-100)
//     +0x50: educated percentage (0-100)
//     +0x60: health percentage (0-100)
//     +0x70: working percentage (0-100)
//   +0x80..+0x490: 21 profession entries, each 0x30 bytes with 3 sub-arrays:
//     +0x00: currentCount (profRes entry +0x00)
//     +0x10: field_0C (profRes entry +0x0C) — accumulated per-tick resource delta
//     +0x20: field_10 (profRes entry +0x10) — accumulated resource count
//
// After pushing all stats, clears profRes +0x0C and +0x10 for each of 21 entries.
//
// Original: FUN_14008a730
void CitizenBehavior::CollectStats() {
    // The per-citizen SA-guard below is VirtualQuery-backed (~80 us/syscall). All
    // citizens live in a couple of engine-heap arenas, so let the probe cache those
    // regions for the duration of this pass â€” nothing is unmapped while it runs.
    ForceWork::ResetReadableCache();

    // -- Phase 1: Iterate all citizens, accumulate counters --
    // Field names are CitizenComponent.h's (W0-H1). The tree's local names are kept
    // where the header names the field differently: `happinessSum` sums +0x50
    // (`health`), `healthSum` sums +0x80 (`happiness`), `dead` counts a non-null
    // +0x208 (`school`), `working` a non-zero +0x1E4 (`clothingLevel`).
    int citizenCount = this->citizenCount;
    CitizenComponent** citizenArray = citizens;

    float happinessSum = 0.0f;
    float healthSum = 0.0f;
    int adults = 0;    // alive, age >= adult age
    int children = 0;  // alive, age < adult age
    int dead = 0;      // +0x208 != 0
    int educated = 0;  // education >= threshold AND adult AND alive
    int working = 0;   // +0x1E4 != 0

    for (int i = 0; i < citizenCount; i++) {
        CitizenComponent* c = citizenArray[i];

        // SA-guard: skip an entry that is not a readable citizen. Retail's array holds
        // only live citizens, so this can only trip on a standalone list-integrity bug
        // â€” the town citizen list (self+0xA0/+0xA8) has no verified unregister when a
        // citizen entity is freed, and CheckPtr is an empty function in the shipping
        // Runtime, so it cannot null a stale weak ref. Observed live: an entry whose
        // +0x288 CitizenDescription read back as 0x760, so `descPtr + 0x44` faulted at
        // 0x7a4 (ACCESS_VIOLATION in CollectStats, 2026-07-21 and again 2026-07-24).
        // CollectStats only AVERAGES â€” dropping a dead entry changes a statistic, never
        // simulation state, which is much cheaper than aborting the process. The stale
        // entry itself is the real bug; the log names it so it can be root-caused.
        const bool guardFailed =
            !ForceWork::Readable(c, 0x28C) ||
            !ForceWork::Readable(c->description, 0x11C);
        if (guardFailed) {
            static int s_reported = 0;
            if (s_reported++ < 20) {
                std::fprintf(stderr, "[CollectStats] SKIP stale citizen[%d/%d]=%p desc=%p "
                             "â€” entry in the town citizen list is not live (list-integrity "
                             "bug: citizen freed without unregistering)\n",
                             i, citizenCount, static_cast<void*>(c),
                             ForceWork::Readable(c, 0x28C)
                                 ? static_cast<void*>(c->description) : nullptr);
                std::fflush(stderr);
            }
            continue;
        }

        // Read citizen fields
        float age = c->age;
        float adultAge = c->description->adultAge;
        const bool atSchool = c->school != nullptr;

        // Classify: child (age < adult AND no +0x208), +0x208 set, or adult
        if (age < adultAge || atSchool) {
            if (!atSchool) {
                children++;
            } else {
                dead++;
            }
        } else {
            adults++;
        }

        // Accumulate +0x50 and +0x80
        happinessSum += c->health;
        healthSum += c->happiness;

        // Check educated: education level >= threshold AND adult AND +0x208 null
        int educThreshold = c->description->maxEducation;
        int educLevel = c->education;
        if (educLevel >= educThreshold) {
            float citizenAge = c->age;
            float citizenAdultAge = c->description->adultAge;
            if (citizenAdultAge <= citizenAge && c->school == nullptr) {
                educated++;
            }
        }

        // Check working: +0x1E4 != 0
        if (c->clothingLevel != 0) {
            working++;
        }
    }

    // -- Phase 1b: Compute percentage stats --
    // Uses first citizen's CitizenDescription for the two maxima.
    int happinessPct = 0;
    int healthPct = 0;
    int educatedPct = 0;
    int workingPct = 0;

    if (citizenCount > 0) {
        // Get CitizenDescription from first citizen's +0x18 via IsDerivedFrom.
        // Retail (0x14008a730): local_res8 = **(ulonglong**)(param_1+0xa0) â€”
        // the array base, ONE deref gives array[0] (the citizen), then its
        // Component +0x18 description.
        void* descBase = citizens[0]->_description;
        CitizenDescription* citizenDesc = nullptr;
        if (::Object::IsDerivedFromNullSafe(descBase, CitizenDescription::TypeId())) {
            citizenDesc = reinterpret_cast<CitizenDescription*>(descBase);   // Component::_description is void*: LEFT
        }

        // Read CitizenDescription+0xA4
        float maxHealth = 0.0f;
        if (citizenDesc) {
            maxHealth = citizenDesc->maxHappiness;
        }

        // Re-read first citizen for CitizenDescription+0x70 (same single-deref as above).
        descBase = citizens[0]->_description;
        citizenDesc = nullptr;
        if (::Object::IsDerivedFromNullSafe(descBase, CitizenDescription::TypeId())) {
            citizenDesc = reinterpret_cast<CitizenDescription*>(descBase);   // Component::_description is void*: LEFT
        }

        float maxHappiness = 0.0f;
        if (citizenDesc) {
            maxHappiness = citizenDesc->maxHealth;
        }

        float fCount = static_cast<float>(citizenCount);
        // DAT_140172208 = 100.0f â€” scale to percentage
        if (maxHappiness != 0.0f) {
            happinessPct = static_cast<int>(
                ((happinessSum / fCount) / maxHappiness) * 100.0f);
        }
        if (maxHealth != 0.0f) {
            healthPct = static_cast<int>(
                ((healthSum / fCount) / maxHealth) * 100.0f);
        }
        if (adults > 0) {
            educatedPct = static_cast<int>(
                (static_cast<float>(educated) / static_cast<float>(adults)) * 100.0f);
        }
        workingPct = static_cast<int>(
            (static_cast<float>(working) / fCount) * 100.0f);
    }

    // -- Phase 2: Push profession resource stats to history --
    // Stats history at +0x150: each sub-array is a 0x10-byte struct
    //   { void* data, int count, int capacity }
    // 21 professions, 3 values each, at hist + 0x80 + profIdx*0x30 + {0, 0x10, 0x20}
    // (CitizenBehavior.h keeps +0x150 as a void* data pointer: LEFT)
    uintptr_t hist = reinterpret_cast<uintptr_t>(field_0x150);

    for (int prof = 0; prof < 21; prof++) {
        uintptr_t histSlot = hist + 0x80 + prof * 0x30;
        const ProfessionResourceEntry& profEntry = professionResources[prof];

        // Every push below is Collection::Array<T>::SetSize(n, fill) — B.3.2,
        // retail 0x140003600, stride 4 align 4, no init arm (every new slot takes
        // the fill). Elements are the int32 history samples.

        // Push currentCount (+0x00)
        int curArrayCount = *reinterpret_cast<int*>(histSlot + 0x08);
        GameColl::ArrayAt<GameColl::I32NoInit>(histSlot, 0x00).SetSize(
            curArrayCount + 1, GameColl::I32NoInit{profEntry.currentCount});

        // Push field_0x0C (+0x0C)
        int f0CArrayCount = *reinterpret_cast<int*>(histSlot + 0x10 + 0x08);
        GameColl::ArrayAt<GameColl::I32NoInit>(histSlot, 0x10).SetSize(
            f0CArrayCount + 1, GameColl::I32NoInit{profEntry.field_0x0C});

        // Push _field10 (+0x10)
        int f10ArrayCount = *reinterpret_cast<int*>(histSlot + 0x20 + 0x08);
        GameColl::ArrayAt<GameColl::I32NoInit>(histSlot, 0x20).SetSize(
            f10ArrayCount + 1, GameColl::I32NoInit{profEntry._field10});
    }

    // -- Phase 3: Push population stats --
    // 8 computed stats at hist + 0x00..0x70 (each 0x10-byte sub-array)
    // Same helper as Phase 2: Collection::Array<T>::SetSize(n, fill) — B.3.2,
    // retail 0x140003600, stride 4 align 4, no init arm (every new slot takes the fill).
    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x00).SetSize(
        *reinterpret_cast<int*>(hist + 0x08) + 1, GameColl::I32NoInit{citizenCount});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x10).SetSize(
        *reinterpret_cast<int*>(hist + 0x18) + 1, GameColl::I32NoInit{adults});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x20).SetSize(
        *reinterpret_cast<int*>(hist + 0x28) + 1, GameColl::I32NoInit{dead});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x30).SetSize(
        *reinterpret_cast<int*>(hist + 0x38) + 1, GameColl::I32NoInit{children});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x40).SetSize(
        *reinterpret_cast<int*>(hist + 0x48) + 1, GameColl::I32NoInit{happinessPct});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x60).SetSize(
        *reinterpret_cast<int*>(hist + 0x68) + 1, GameColl::I32NoInit{healthPct});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x50).SetSize(
        *reinterpret_cast<int*>(hist + 0x58) + 1, GameColl::I32NoInit{educatedPct});

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x70).SetSize(
        *reinterpret_cast<int*>(hist + 0x78) + 1, GameColl::I32NoInit{workingPct});

    // -- Phase 4: Clear per-tick accumulators in profResources --
    // Clear +0x0C and +0x10 of each ProfessionResourceEntry
    for (int e = 0; e < 21; e++) {
        professionResources[e].field_0x0C = 0;  // +0x0C of entry
        professionResources[e]._field10 = 0;    // +0x10 of entry
    }
}


// ============================================================
// B4 FORWARD: 1 outside caller (code/Game/Game/gameupdateloop.cpp, which still
// hands a CitizenBehaviorRaw*). The closing wave rewires it and deletes this.
// ============================================================
void CitizenBehavior_Update(void* self, int64_t* deltaTime) {
    reinterpret_cast<CitizenBehavior*>(self)->Update(deltaTime);
}
