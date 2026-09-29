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
void CitizenBehavior_Update(CitizenBehaviorRaw* self, int64_t* deltaTime) {
    int64_t* q = reinterpret_cast<int64_t*>(self->data);

    // Advance timers (Ghidra longlong* indexing)
    q[0x32] += *deltaTime;       // pathAccumTimer
    q[0x33] -= *deltaTime;       // statsTimer
    q[0x34] -= *deltaTime;       // notifyTimer

    // CountResources — Entity_helper_9ed0(self)
    CitizenBehavior_CountResources(self);

    // CheckThresholds — Entity_helper_c100(self, deltaTime)
    CitizenBehavior_CheckThresholds(self, deltaTime);

    // Path graph processing
    // DAT_140172234 is the path graph interval (float seconds)
    // System::Time::Time constructs a Time value from float
    int64_t pathInterval = 0;
    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&pathInterval), kRdata_140172234);   // 0x140172234


    if (pathInterval <= q[0x32]) {
        // Try to process pending paths
        bool pathsDone = false;
        PathGraph* pg = self->pathGraph();
        pathsDone = CitizenBehavior_ProcessPaths(pg);

        if (!pathsDone) {
            // Build new path graph
            CitizenBehavior_BuildPathGraph(pg);
        } else if (pg && pg->m_pendingIdx < 0 && pathInterval < q[0x32]) {
            // Clamp accumulated time: subtract intervals until within one interval.
            // Guard against pathInterval <= 0 (BANISHED_PATHGRAPH_FAST forces 0, or a failed
            // interval resolve) — `acc -= 0` would spin forever. This branch is reached only
            // when ProcessPaths returns true (pairs drained), which requires townMgr+0x100 to
            // be non-empty (e.g. a registered construction site) — so it stayed latent until
            // the natural builder flow first populated that BST.
            if (pathInterval > 0) {
                int64_t acc = q[0x32];
                do {
                    acc -= pathInterval;
                } while (pathInterval < acc);
                q[0x32] = acc;
            } else {
                q[0x32] = 0;   // no positive interval to modulo by -> reset the accumulator
            }
        }
    }

    // Stats collection (periodic)
    if (q[0x33] < 1) {
        CitizenBehavior_CollectStats(self);
        // Reset timer from DAT_1401721fc (float seconds)
        {
            int64_t newTime = 0;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&newTime), kRdata_1401721fc);   // 0x1401721fc
            q[0x33] = newTime;
        }
    }

    // Notification timer reset
    if (q[0x34] < 0) {
        *reinterpret_cast<uint32_t*>(&q[0x35]) = 0; // clear notify flags
        q[0x34] = 0;
    }

    // Tick-driven Builder pool (BANISHED_BUILDER_NATURAL, opt-in): keep registered
    // construction sites staffed by seating idle Laborers as Builders via the real
    // FindAvailableBuilding cascade, nearest-first, capped by site capacity. No-op unless
    // the natural flow is enabled. This is the headless stand-in for the player's town-hall
    // Builder count — the engine has no player-free first-builder seater (the periodic
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
bool CitizenBehavior_ProcessPaths(PathGraph* pg) {
    if (!pg) return false;
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
    int* mapGrid = *reinterpret_cast<int**>(
        reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton) + 0xf8);

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
void CitizenBehavior_BuildPathGraph(PathGraph* pg) {
    if (!pg) return;
    // Only rebuild when not currently processing
    if (pg->m_pendingIdx >= 0) {
        return;
    }

    int dbgEntities = 0, dbgEntRegionOk = 0, dbgPairsAdded = 0;

    // Get key pointers from the game world singleton
    uint8_t* world = reinterpret_cast<uint8_t*>(GameUpdateLoop::_singleton);
    int* mapGrid = *reinterpret_cast<int**>(world + 0xf8);
    uint8_t* citizenBehavior = *reinterpret_cast<uint8_t**>(world + 0xf0);
    uint8_t* buildingList = *reinterpret_cast<uint8_t**>(world + 0xb0);

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
    int profGroupCount = *reinterpret_cast<int*>(buildingList + 0x28);


    for (int gi = 0; gi < profGroupCount; gi++) {
        // Get profession key from building list
        uint64_t profKey = *reinterpret_cast<uint64_t*>(
            *reinterpret_cast<int64_t*>(buildingList + 0x20) + gi * 8);

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
                    *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pg) + 0x08) = newPairCount;
                    int pairCap = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pg) + 0x0C);
                    if (pairCap < newPairCount) {
                        int newCap = newPairCount;
                        if (curPairCount != 0) {
                            newCap = newPairCount * 3;
                        }
                        *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(pg) + 0x0C) = newCap;
                        void* newArr = System::MemoryAllocator::MemAlloc((int64_t)newCap * 8, 4);
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
void CitizenBehavior_CountResources(CitizenBehaviorRaw* self) {
    int64_t* q = reinterpret_cast<int64_t*>(self->data);

    // --- Phase 0: Shift currentCount -> previousCount and zero currentCount ---
    // The profResources array is 21 entries of 0x20 bytes each (total 0x2A0).
    // For each entry: previousCount (+0x04) = currentCount (+0x00), then currentCount = 0.
    uint8_t* profRes = reinterpret_cast<uint8_t*>(q[0x2e]); // param_1[0x2e] = profResources ptr
    if (profRes) {
        for (int64_t off = 0; off < 0x2A0; off += 0x20) {
            // previousCount = currentCount
            *reinterpret_cast<int32_t*>(profRes + off + 0x04) =
                *reinterpret_cast<int32_t*>(profRes + off + 0x00);
            // currentCount = 0
            *reinterpret_cast<int32_t*>(profRes + off + 0x00) = 0;
        }
    }

    // --- Zero notification summary at param_1[0x30] (offset 0x180) ---
    int32_t* notifySummary = reinterpret_cast<int32_t*>(q[0x30]);
    if (notifySummary) {
        notifySummary[1] = 0;  // +0x04: normal resource worker count
        notifySummary[0] = 0;  // +0x00: normal resource total
        notifySummary[5] = 0;  // +0x14: dead resource worker count
        notifySummary[4] = 0;  // +0x10: dead resource total
    }

    // Helper lambda: find a component on an entity by type ID.
    // Entity layout: +0x30 = component array (void**), +0x38 = component count (int).
    // Returns the found component pointer, or nullptr.
    auto findComponent = [](uintptr_t entity, uint32_t targetId) -> void* {
        if (!entity) return nullptr;
        void** compArray = *reinterpret_cast<void***>(entity + 0x30);
        int compCount = *reinterpret_cast<int*>(entity + 0x38);
        for (int i = 0; i < compCount; i++) {
            void* comp = compArray[i];
            if (!comp) continue;
            // virtual slot 2 (+0x10): Type()
            int compTypeId = static_cast<int>(reinterpret_cast<Object::Base*>(comp)->Type());  // B.3.8 bridge
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
    // profession entry in profResources. Returns the profession entry offset
    // within profRes (or -1 if no matching flag).
    auto addToProfession = [&](uint32_t flags, int32_t itemCount) {
        int offset = -1;
        if (flags & 0x20) {
            // bit 5 (Edible) -> entry 5, offset 0xa0
            offset = 0xa0;
        } else if (flags & 0x80) {
            // bit 7 (Wood) -> entry 3, offset 0x60
            offset = 0x60;
        } else if (flags & 0x100) {
            // bit 8 (Stone) -> entry 9, offset 0x120
            offset = 0x120;
        } else if (flags & 0x200) {
            // bit 9 (Iron) -> entry 4, offset 0x80
            offset = 0x80;
        } else if (flags & 0x400) {
            // bit 10 (Health) -> entry 0, offset 0x00
            offset = 0x00;
        } else if (flags & 0x800) {
            // bit 11 (Clothing) -> entry 1, offset 0x20
            offset = 0x20;
        } else if (flags & 0x1000) {
            // bit 12 (Textile) -> entry 2, offset 0x40
            offset = 0x40;
        } else if (flags & 0x2000) {
            // bit 13 (Alcohol) -> entry 6, offset 0xc0
            offset = 0xc0;
        } else if (flags & 0x4000) {
            // bit 14 (Custom0) -> entry 7, offset 0xe0
            offset = 0xe0;
        } else if (flags & 0x20000) {
            // bit 17 (Custom3) -> entry 8, offset 0x100
            offset = 0x100;
        } else if (flags & 0x10000) {
            // bit 16 (Custom2) -> entry 10, offset 0x140
            offset = 0x140;
        } else if (flags & 0x40000) {
            // bit 18 (Custom4) -> entry 11, offset 0x160
            offset = 0x160;
        } else if (flags & 0x80000) {
            // bit 19 (Custom5) -> entry 12, offset 0x180
            offset = 0x180;
        } else if (flags & 0x100000) {
            // bit 20 (Custom6) -> entry 13, offset 0x1a0
            offset = 0x1a0;
        } else if (flags & 0x200000) {
            // bit 21 (Custom7) -> entry 14, offset 0x1c0
            offset = 0x1c0;
        } else if (flags & 0x400000) {
            // bit 22 (Custom8) -> entry 15, offset 0x1e0
            offset = 0x1e0;
        } else if (flags & 0x800000) {
            // bit 23 (Custom9) -> entry 16, offset 0x200
            offset = 0x200;
        } else if (flags & 0x1000000) {
            // bit 24 -> entry 17, offset 0x220
            offset = 0x220;
        } else if (flags & 0x2000000) {
            // bit 25 -> entry 18, offset 0x240
            offset = 0x240;
        } else if (flags & 0x4000000) {
            // bit 26 -> entry 19, offset 0x260
            offset = 0x260;
        } else if (flags & 0x8000000) {
            // bit 27 -> entry 20, offset 0x280
            offset = 0x280;
        }

        if (offset >= 0 && profRes) {
            *reinterpret_cast<int32_t*>(profRes + offset) += itemCount;
        }
    };

    // --- Phase 1: Iterate first entity array (param_1[0]/param_1[1], offset +0x00/+0x08) ---
    // These entities have StorageComponent; their items are tallied for both
    // profession resource counts AND notification summaries.
    int64_t firstCount = static_cast<int64_t>(static_cast<int32_t>(q[1])); // param_1[1] = +0x08
    for (int64_t idx = 0; idx < firstCount; idx++) {
        // Read entity InternalBasePtr from array
        int64_t entityIBP = *reinterpret_cast<int64_t*>(q[0] + idx * 8);

        // Find StorageComponent on this entity
        void* storageComp = findComponent(static_cast<uintptr_t>(entityIBP),
                                          StorageComponent::TypeId());

        // Wrap in InternalBasePtr
        int64_t storageIBP = 0;
        {
            new (&storageIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(storageComp));   // B.3.3 bridge
        }

        // Read item count from storage: *(int*)(storage + 0x30)
        int32_t itemCount = *reinterpret_cast<int32_t*>(storageIBP + 0x30);
        if (itemCount <= 0) continue;

        bool hadNormalResources = false;
        bool hadDeadResources = false;

        // Iterate storage items: array at *(ptr*)(storage + 0x28), stride 0x10
        // Each item: [0x00] = item entity ptr, [0x08] = quantity (int cast from longlong)
        for (int32_t j = 0; j < itemCount; j++) {
            int64_t* itemEntry = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(storageIBP + 0x28) + j * 0x10);

            // Find RawMaterialDescription on the item entity
            // Item entity has components at +0x28/+0x30 (different from top-level entity +0x30/+0x38)
            int64_t itemEntity = itemEntry[0];
            void* rawMat = nullptr;
            if (itemEntity) {
                int subCompCount = *reinterpret_cast<int*>(itemEntity + 0x30);
                uint32_t rawMatTypeId = RawMaterialDescription::TypeId();
                for (int k = 0; k < subCompCount; k++) {
                    void* subComp = *reinterpret_cast<void**>(
                        *reinterpret_cast<int64_t*>(itemEntity + 0x28) + k * 8);
                    if (!subComp) continue;
                    int compTypeId = static_cast<int>(reinterpret_cast<Object::Base*>(subComp)->Type());  // B.3.8 bridge
                    if (static_cast<uint32_t>(compTypeId) == rawMatTypeId) {
                        if (::Object::IsDerivedFromNullSafe(subComp, rawMatTypeId)) {
                            rawMat = subComp;
                        }
                        break;
                    }
                }
            }
            if (!rawMat) continue;

            // Get weight per unit from rawMat+0x48, multiply by item quantity
            int32_t weightPerUnit = *reinterpret_cast<int32_t*>(
                reinterpret_cast<uintptr_t>(rawMat) + 0x48);
            int32_t quantity = static_cast<int32_t>(itemEntry[1]);
            int32_t totalAmount = weightPerUnit * quantity;

            // Get RawMaterialFlags from rawMat+0x5c
            uint32_t flags = *reinterpret_cast<uint32_t*>(
                reinterpret_cast<uintptr_t>(rawMat) + 0x5c);

            // Update notification summary: split by flag 0x1c40
            // 0x1c40 = Tool(6) | Health(10) | Clothing(11) | Textile(12)
            if (notifySummary) {
                if ((flags & 0x1c40) == 0) {
                    // Normal resources (food, fuel, etc.)
                    notifySummary[0] += totalAmount;
                    hadNormalResources = true;
                } else {
                    // Tools/health/clothing/textile
                    notifySummary[4] += totalAmount;
                    hadDeadResources = true;
                }
            }

            // Categorize by profession flag
            addToProfession(flags, static_cast<int32_t>(itemEntry[1]));
        }

        // Update notification worker counts from storage+0x38 (capacity/worker count)
        if (hadDeadResources && notifySummary) {
            notifySummary[5] += *reinterpret_cast<int32_t*>(storageIBP + 0x38);
        }
        if (hadNormalResources && notifySummary) {
            notifySummary[1] += *reinterpret_cast<int32_t*>(storageIBP + 0x38);
        }
    }

    // --- Phase 2: Iterate second entity array (param_1[2]/param_1[3], offset +0x10/+0x18) ---
    // Same logic as Phase 1 but WITHOUT notification summary updates.
    int64_t secondCount = static_cast<int64_t>(static_cast<int32_t>(q[3])); // param_1[3] = +0x18
    for (int64_t idx = 0; idx < secondCount; idx++) {
        // Read entity InternalBasePtr from array
        int64_t entityIBP = *reinterpret_cast<int64_t*>(q[2] + idx * 8);

        // Find StorageComponent on this entity
        uintptr_t entityAddr = static_cast<uintptr_t>(entityIBP);
        void* storageComp = findComponent(entityAddr, StorageComponent::TypeId());

        // Wrap in InternalBasePtr
        int64_t storageIBP = 0;
        {
            new (&storageIBP) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(storageComp));   // B.3.3 bridge
        }

        // Read item count from storage: *(int*)(storage + 0x30)
        int32_t itemCount = *reinterpret_cast<int32_t*>(storageIBP + 0x30);
        if (itemCount <= 0) continue;

        // Iterate storage items
        for (int32_t j = 0; j < itemCount; j++) {
            int64_t* itemEntry = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(storageIBP + 0x28) + j * 0x10);

            // Find RawMaterialDescription on the item entity
            int64_t itemEntity = itemEntry[0];
            void* rawMat = nullptr;
            if (itemEntity) {
                uint32_t rawMatTypeId = RawMaterialDescription::TypeId();
                int subCompCount = *reinterpret_cast<int*>(itemEntity + 0x30);
                for (int k = 0; k < subCompCount; k++) {
                    void* subComp = *reinterpret_cast<void**>(
                        *reinterpret_cast<int64_t*>(itemEntity + 0x28) + k * 8);
                    if (!subComp) continue;
                    int compTypeId = static_cast<int>(reinterpret_cast<Object::Base*>(subComp)->Type());  // B.3.8 bridge
                    if (static_cast<uint32_t>(compTypeId) == rawMatTypeId) {
                        if (::Object::IsDerivedFromNullSafe(subComp, rawMatTypeId)) {
                            rawMat = subComp;
                        }
                        break;
                    }
                }
            }
            if (!rawMat) continue;

            // Get RawMaterialFlags from rawMat+0x5c
            uint32_t flags = *reinterpret_cast<uint32_t*>(
                reinterpret_cast<uintptr_t>(rawMat) + 0x5c);

            // Categorize by profession flag (no notification update in Phase 2)
            addToProfession(flags, static_cast<int32_t>(itemEntry[1]));
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
void CitizenBehavior_CheckThresholds(CitizenBehaviorRaw* self, int64_t* deltaTime) {
    float notifyInterval = 120.0f; // DAT_140172210 = 120.0f seconds
    void* notifMgr = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0x138);

    // Part 1: Check profession resource thresholds (21 entries)
    for (int i = 0; i < 21; i++) {
        uintptr_t profRes = reinterpret_cast<uintptr_t>(self->profResources());
        int64_t* timer = reinterpret_cast<int64_t*>(profRes + i * 0x20 + 0x18);

        // Decrement timer
        *timer -= *deltaTime;

        if (*timer < 1 && static_cast<uint32_t>(i) < 6) {
            // Reset timer to zero (will be set to interval if notification fires)
            *timer = 0;

            int32_t maxCount = *reinterpret_cast<int32_t*>(profRes + i * 0x20 + 0x08);
            int32_t currentCount = *reinterpret_cast<int32_t*>(profRes + i * 0x20 + 0x00);
            int32_t previousCount = *reinterpret_cast<int32_t*>(profRes + i * 0x20 + 0x04);

            // Profession 3 (Fisherman): combine with profession 9 (Stonecutter)
            if (i == 3) {
                uintptr_t profRes2 = reinterpret_cast<uintptr_t>(self->profResources());
                maxCount += *reinterpret_cast<int32_t*>(profRes2 + 9 * 0x20 + 0x08);
                currentCount += *reinterpret_cast<int32_t*>(profRes2 + 9 * 0x20 + 0x00);
                previousCount += *reinterpret_cast<int32_t*>(profRes2 + 9 * 0x20 + 0x04);
            }

            int32_t threshold = maxCount / 20; // 5% threshold
            if (currentCount < threshold && threshold <= previousCount) {
                // Fire shortage notification
                {
                    uint64_t nullIBP = 0;
                    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                    EventLog_AddEvent(notifMgr, i + 0x11, 3, &nullIBP);   // 0x140095580
                }
                // Reset timer to notification cooldown (120 seconds)
                {
                    int64_t newTime = 0;
                    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&newTime), notifyInterval);
                    *timer = newTime;
                }
            }
        }
    }

    // Part 2: Check resource notify entries (2 entries, food/firewood storage).
    // [DEREF FIX] +0x180 is a POINTER to the heap-allocated 2-entry array
    // (fn_SummaryArray_Init(self->data + 0x180, 2) in the ctor; original:
    // `lVar3 = *(longlong*)(param_1 + 0x180)`). The previous code used the field
    // address itself as the array, so "current" read the pointer's low dword
    // (huge -> 95%-full always true) and the "timer" writes stomped +0x188 —
    // that was the type-24 event spam (thousands of storage-full posts/minute).
    for (int i = 0; i < 2; i++) {
        uintptr_t notifyBase = *reinterpret_cast<uintptr_t*>(self->data + 0x180);
        if (!notifyBase) break;
        int64_t* timer = reinterpret_cast<int64_t*>(notifyBase + i * 0x10 + 0x08);

        // Decrement timer
        *timer -= *deltaTime;

        if (*timer < 1) {
            *timer = 0;

            int32_t current = *reinterpret_cast<int32_t*>(notifyBase + i * 0x10 + 0x00);
            int32_t maxCapacity = *reinterpret_cast<int32_t*>(notifyBase + i * 0x10 + 0x04);

            // Check if storage is > 95% full: current > (max/20)*19
            if ((maxCapacity / 20) * 19 < current) {
                // Notification type: 0x17 for food (entry 0), 0x18 for firewood (entry 1)
                int notifType = 0x17 + (i != 0 ? 1 : 0);
                {
                    uint64_t nullIBP = 0;
                    new (&nullIBP) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                    EventLog_AddEvent(notifMgr, notifType, 3, &nullIBP);   // 0x140095580
                }
                {
                    int64_t newTime = 0;
                    RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&newTime), notifyInterval);
                    *timer = newTime;
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
void CitizenBehavior_CollectStats(CitizenBehaviorRaw* self) {
    // The per-citizen SA-guard below is VirtualQuery-backed (~80 us/syscall). All
    // citizens live in a couple of engine-heap arenas, so let the probe cache those
    // regions for the duration of this pass — nothing is unmapped while it runs.
    ForceWork::ResetReadableCache();

    // -- Phase 1: Iterate all citizens, accumulate counters --
    int citizenCount = self->allCitizensCount();
    uint64_t* citizenArray = reinterpret_cast<uint64_t*>(self->allCitizensPtr());

    float happinessSum = 0.0f;
    float healthSum = 0.0f;
    int adults = 0;    // alive, age >= adult age
    int children = 0;  // alive, age < adult age
    int dead = 0;      // death timestamp != 0
    int educated = 0;  // education >= threshold AND adult AND alive
    int working = 0;   // has job flag

    for (int i = 0; i < citizenCount; i++) {
        uint64_t citizenIBP = citizenArray[i];
        uintptr_t c = static_cast<uintptr_t>(citizenIBP);

        // SA-guard: skip an entry that is not a readable citizen. Retail's array holds
        // only live citizens, so this can only trip on a standalone list-integrity bug
        // — the town citizen list (self+0xA0/+0xA8) has no verified unregister when a
        // citizen entity is freed, and CheckPtr is an empty function in the shipping
        // Runtime, so it cannot null a stale weak ref. Observed live: an entry whose
        // +0x288 CitizenDescription read back as 0x760, so `descPtr + 0x44` faulted at
        // 0x7a4 (ACCESS_VIOLATION in CollectStats, 2026-07-21 and again 2026-07-24).
        // CollectStats only AVERAGES — dropping a dead entry changes a statistic, never
        // simulation state, which is much cheaper than aborting the process. The stale
        // entry itself is the real bug; the log names it so it can be root-caused.
        const bool guardFailed =
            !ForceWork::Readable(reinterpret_cast<void*>(c), 0x28C) ||
            !ForceWork::Readable(
                reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(c + 0x288)), 0x11C);
        if (guardFailed) {
            static int s_reported = 0;
            if (s_reported++ < 20) {
                std::fprintf(stderr, "[CollectStats] SKIP stale citizen[%d/%d]=%p desc=%p "
                             "— entry in the town citizen list is not live (list-integrity "
                             "bug: citizen freed without unregistering)\n",
                             i, citizenCount, reinterpret_cast<void*>(c),
                             ForceWork::Readable(reinterpret_cast<void*>(c), 0x28C)
                                 ? *reinterpret_cast<void**>(c + 0x288) : nullptr);
                std::fflush(stderr);
            }
            continue;
        }

        // Read citizen fields
        float age = *reinterpret_cast<float*>(c + 0x3C);
        uintptr_t descPtr = *reinterpret_cast<uintptr_t*>(c + 0x288);
        float adultAge = *reinterpret_cast<float*>(descPtr + 0x44);
        int64_t deathTime = *reinterpret_cast<int64_t*>(c + 0x208);

        // Classify: child (age < adult AND alive), dead, or adult
        if (age < adultAge || deathTime != 0) {
            if (deathTime == 0) {
                children++;
            } else {
                dead++;
            }
        } else {
            adults++;
        }

        // Accumulate happiness (+0x50) and health (+0x80)
        c = static_cast<uintptr_t>(citizenIBP);
        happinessSum += *reinterpret_cast<float*>(c + 0x50);

        c = static_cast<uintptr_t>(citizenIBP);
        healthSum += *reinterpret_cast<float*>(c + 0x80);

        // Check educated: education level >= threshold AND adult AND alive
        c = static_cast<uintptr_t>(citizenIBP);
        descPtr = *reinterpret_cast<uintptr_t*>(c + 0x288);
        int educThreshold = *reinterpret_cast<int*>(descPtr + 0x118);
        int educLevel = *reinterpret_cast<int*>(c + 0x210);
        if (educLevel >= educThreshold) {
            c = static_cast<uintptr_t>(citizenIBP);
            descPtr = *reinterpret_cast<uintptr_t*>(c + 0x288);
            float citizenAge = *reinterpret_cast<float*>(c + 0x3C);
            float citizenAdultAge = *reinterpret_cast<float*>(descPtr + 0x44);
            if (citizenAdultAge <= citizenAge && *reinterpret_cast<int64_t*>(c + 0x208) == 0) {
                educated++;
            }
        }

        // Check working: has job flag at +0x1E4
        c = static_cast<uintptr_t>(citizenIBP);
        if (*reinterpret_cast<int*>(c + 0x1E4) != 0) {
            working++;
        }
    }

    // -- Phase 1b: Compute percentage stats --
    // Uses first citizen's CitizenDescription for max happiness/health values.
    int happinessPct = 0;
    int healthPct = 0;
    int educatedPct = 0;
    int workingPct = 0;

    if (citizenCount > 0) {
        // Get CitizenDescription from first citizen's +0x18 via IsDerivedFrom.
        // Retail (0x14008a730): local_res8 = **(ulonglong**)(param_1+0xa0) —
        // allCitizensPtr() IS the array base, so ONE deref gives array[0] (the
        // citizen IBP value). A second deref would read the citizen's vtable
        // and +0x18 lands in .text (IsDerivedFrom faults on a code address).
        uint64_t firstIBP = *reinterpret_cast<uint64_t*>(self->allCitizensPtr());
        void* descBase = *reinterpret_cast<void**>(static_cast<uintptr_t>(firstIBP) + 0x18);
        void* citizenDesc = nullptr;
        if (::Object::IsDerivedFromNullSafe(descBase, CitizenDescription::TypeId())) {
            citizenDesc = descBase;
        }

        // Read maxHealth from CitizenDescription+0xA4
        float maxHealth = 0.0f;
        if (citizenDesc) {
            maxHealth = *reinterpret_cast<float*>(
                reinterpret_cast<uintptr_t>(citizenDesc) + 0xA4);
        }

        // Re-read first citizen for maxHappiness from CitizenDescription+0x70
        // (same single-deref as above).
        firstIBP = *reinterpret_cast<uint64_t*>(self->allCitizensPtr());
        descBase = *reinterpret_cast<void**>(static_cast<uintptr_t>(firstIBP) + 0x18);
        citizenDesc = nullptr;
        if (::Object::IsDerivedFromNullSafe(descBase, CitizenDescription::TypeId())) {
            citizenDesc = descBase;
        }

        float maxHappiness = 0.0f;
        if (citizenDesc) {
            maxHappiness = *reinterpret_cast<float*>(
                reinterpret_cast<uintptr_t>(citizenDesc) + 0x70);
        }

        float fCount = static_cast<float>(citizenCount);
        // DAT_140172208 = 100.0f — scale to percentage
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
    uintptr_t hist = reinterpret_cast<uintptr_t>(self->statsHistory());
    uintptr_t profRes = reinterpret_cast<uintptr_t>(self->profResources());

    for (int prof = 0; prof < 21; prof++) {
        uintptr_t histSlot = hist + 0x80 + prof * 0x30;
        uintptr_t profEntry = profRes + prof * 0x20;

        // Every push below is Collection::Array<T>::SetSize(n, fill) — B.3.2,
        // retail 0x140003600, stride 4 align 4, no init arm (every new slot takes
        // the fill). Elements are the int32 history samples.

        // Push currentCount (+0x00)
        int curArrayCount = *reinterpret_cast<int*>(histSlot + 0x08);
        GameColl::ArrayAt<GameColl::I32NoInit>(histSlot, 0x00).SetSize(
            curArrayCount + 1,
            *reinterpret_cast<const GameColl::I32NoInit*>(profEntry + 0x00));

        // Push field_0C (+0x0C)
        int f0CArrayCount = *reinterpret_cast<int*>(histSlot + 0x10 + 0x08);
        GameColl::ArrayAt<GameColl::I32NoInit>(histSlot, 0x10).SetSize(
            f0CArrayCount + 1,
            *reinterpret_cast<const GameColl::I32NoInit*>(profEntry + 0x0C));

        // Push field_10 (+0x10)
        int f10ArrayCount = *reinterpret_cast<int*>(histSlot + 0x20 + 0x08);
        GameColl::ArrayAt<GameColl::I32NoInit>(histSlot, 0x20).SetSize(
            f10ArrayCount + 1,
            *reinterpret_cast<const GameColl::I32NoInit*>(profEntry + 0x10));
    }

    // -- Phase 3: Push population stats --
    // 8 computed stats at hist + 0x00..0x70 (each 0x10-byte sub-array)
    // Same helper as Phase 2: Collection::Array<T>::SetSize(n, fill) — B.3.2,
    // retail 0x140003600, stride 4 align 4, no init arm (every new slot takes the fill).
    int32_t totalCount = citizenCount;
    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x00).SetSize(
        *reinterpret_cast<int*>(hist + 0x08) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&totalCount));

    int32_t adultsVal = adults;
    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x10).SetSize(
        *reinterpret_cast<int*>(hist + 0x18) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&adultsVal));

    int32_t deadVal = dead;
    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x20).SetSize(
        *reinterpret_cast<int*>(hist + 0x28) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&deadVal));

    int32_t childrenVal = children;
    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x30).SetSize(
        *reinterpret_cast<int*>(hist + 0x38) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&childrenVal));

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x40).SetSize(
        *reinterpret_cast<int*>(hist + 0x48) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&happinessPct));

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x60).SetSize(
        *reinterpret_cast<int*>(hist + 0x68) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&healthPct));

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x50).SetSize(
        *reinterpret_cast<int*>(hist + 0x58) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&educatedPct));

    GameColl::ArrayAt<GameColl::I32NoInit>(hist, 0x70).SetSize(
        *reinterpret_cast<int*>(hist + 0x78) + 1,
        *reinterpret_cast<const GameColl::I32NoInit*>(&workingPct));

    // -- Phase 4: Clear per-tick accumulators in profResources --
    // Clear +0x0C and +0x10 of each ProfessionResourceEntry
    for (int64_t off = 0x20; off < 0x2A0; off += 0x20) {
        *reinterpret_cast<int32_t*>(profRes + off - 0x14) = 0;  // +0x0C of entry
        *reinterpret_cast<int32_t*>(profRes + off - 0x10) = 0;  // +0x10 of entry
    }

    // Old delegate fallback (commented out):
    // if (fn_CitizenBehavior_CollectStats) {
    //     fn_CitizenBehavior_CollectStats(self);
    // }
}
