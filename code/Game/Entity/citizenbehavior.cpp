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
// This file must NOT depend on hooks.h — portable for standalone exe.

#include "Entity/citizenbehavior_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cmath>
#include <cstdint>
#include <cstring>

// ============================================================
// Internal helper addresses (resolved lazily)
// ============================================================

// System::Time::Time constructor — creates Time from float seconds
typedef void* (*fn_Time_Ctor_t)(void* out, float seconds);
static fn_Time_Ctor_t s_fn_Time_Ctor = nullptr;

// System::Random::Rand(Random*, uint max) -> uint [0, max)
typedef uint32_t (*fn_Random_Rand_t)(void* random, uint32_t max);
static fn_Random_Rand_t s_fn_Random_Rand = nullptr;

static bool s_resolved = false;

static void EnsureResolved() {
    if (s_resolved) return;

    // System::Time::Time(Time*, float) — Runtime export
    s_fn_Time_Ctor = reinterpret_cast<fn_Time_Ctor_t>(
        ResolveExport("?Time@Time@System@@QEAA@M@Z"));
    if (!s_fn_Time_Ctor) {
        // Fallback: resolve from ordinal or known address
        s_fn_Time_Ctor = reinterpret_cast<fn_Time_Ctor_t>(
            ResolveOrdinal(0x031e));
    }

    // System::Random::Rand — Runtime export
    s_fn_Random_Rand = reinterpret_cast<fn_Random_Rand_t>(
        ResolveExport("?Rand@Random@System@@QEAAI@Z"));

    s_resolved = true;
}

// ============================================================
// CitizenBehavior::Ctor (found via GameUpdateLoop::Load)
// ============================================================
// Zeroes all 0x1C0 bytes, initializes profession resource array (21 entries
// with default max counts), stats sub-arrays, and allocates the PathGraph.
//
// Original: TradeComponent_impl_0a30
CitizenBehaviorRaw* CitizenBehavior_Ctor(CitizenBehaviorRaw* self) {
    EnsureResolved();

    // Zero all fields
    memset(self->data, 0, 0x1C0);

    // The decompiled code zeroes param_1[0]..param_1[0x2d] (qwords),
    // then specifically zeros *(uint32_t*)(param_1 + 0x21) and
    // *(uint32_t*)(param_1 + 0x2d), plus param_1[0x2e]..param_1[0x34]
    // and *(uint64_t*)((char*)param_1 + 0x1ac).
    // memset(0, 0x1C0) covers all of this.

    // Initialize profession resource array at +0x170 (ptr to 0x2A0 byte array).
    // fn_ProfResArray_Init allocates 21 entries (0x20 bytes each)
    // and sets default maxCount values.
    int64_t* profResPtr = reinterpret_cast<int64_t*>(self->data + 0x170);
    if (fn_ProfResArray_Init) {
        fn_ProfResArray_Init(profResPtr, 0x15); // 21 professions
    }

    // Set default max counts for each profession
    // These are game balance constants from the constructor:
    void* profArray = self->profResources();
    if (profArray) {
        uint8_t* arr = reinterpret_cast<uint8_t*>(profArray);
        // Profession indices and their default maxCount values:
        //  0: 200   (Laborer)
        //  1: 200   (Builder)
        //  2: 200   (Farmer)
        //  3: 200   (Fisherman)
        //  4:  50   (Hunter)
        //  5: 5000  (Gatherer)
        //  6: 100   (Forester)
        //  7:  50   (Woodcutter)
        //  8: 500   (Blacksmith)
        //  9: 200   (Stonecutter)
        // 10: 200   (Tailor)
        // 11: 1000  (Doctor/Herbalist)
        // 12: 1000  (Teacher)
        // 13: 1000  (Cleric)
        // 14: 1000  (Innkeeper/Brewer)
        // 15: 1000  (Vendor)
        // 16: 1000  (Miner)
        // 17: 1000  (Pastor)
        // 18: 1000  (Trader)
        // 19: 1000  (Herdsman)
        // 20: 1000  (Farmer2/Crop)
        static const int32_t defaultMax[21] = {
            200, 200, 200, 200, 50, 5000, 100, 50, 500,
            200, 200, 1000, 1000, 1000, 1000, 1000, 1000,
            1000, 1000, 1000, 1000
        };
        for (int i = 0; i < 21; i++) {
            *reinterpret_cast<int32_t*>(arr + i * 0x20 + 0x08) = defaultMax[i];
        }
    }

    // Initialize stats tracking arrays
    // FUN_14000f090(param_1 + 0x30, 2) — at offset 0x180 (0x30*8)
    // Actually: param_1 + 0x30 in qword indexing = byte offset 0x180
    // Wait — the decompiled code uses `undefined8 *param_1`, not `longlong *param_1`
    // for the constructor. Let me recheck:
    // param_1 + 0x30 = &param_1[0x30] = base + 0x30*8 = base + 0x180
    // param_1 + 0x2e = &param_1[0x2e] = base + 0x2e*8 = base + 0x170 (profResources)
    // param_1 + 0x2a = &param_1[0x2a] = base + 0x2a*8 = base + 0x150 (statsHistory)

    // FUN_14000f090(base + 0x180, 2) — init notification/summary at +0x180
    if (fn_SummaryArray_Init) {
        fn_SummaryArray_Init(self->data + 0x180, 2);
    }

    // TradeComponent_impl_f760_sub_7640(base + 0x150, 0x47) — init stats tracking
    if (fn_StatsArray_Init) {
        fn_StatsArray_Init(self->data + 0x150, 0x47);
    }

    // *(uint32_t*)(param_1 + 0x35) = 0 — clear notify flags at +0x1A8
    self->notifyFlags() = 0;

    // Allocate PathGraph sub-object (0x28 bytes)
    PathGraph* pg = nullptr;
    if (fn_MemAlloc && g_pMemAllocSingleton) {
        pg = reinterpret_cast<PathGraph*>(
            fn_MemAlloc(g_pMemAllocSingleton, 0x28, 4));
    }
    if (pg) {
        pg->m_pairs = nullptr;
        pg->m_pairCount = 0;
        pg->m_pendingIdx = -1;
        pg->_pad14 = 0;
        pg->m_bstRoot = nullptr;
        pg->m_bstCount = 0;
        pg->_pad24 = 0;
    }
    self->pathGraph() = pg;

    return self;
}

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
    EnsureResolved();

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
    if (s_fn_Time_Ctor) {
        float* intervalFloat = reinterpret_cast<float*>(
            ResolveAddr(Module::Exe, 0x140172234));
        if (intervalFloat) {
            s_fn_Time_Ctor(&pathInterval, *intervalFloat);
        }
    }

    if (pathInterval <= q[0x32]) {
        // Try to process pending paths
        bool pathsDone = false;
        PathGraph* pg = self->pathGraph();
        pathsDone = CitizenBehavior_ProcessPaths(pg);

        if (!pathsDone) {
            // Build new path graph
            CitizenBehavior_BuildPathGraph(pg);
        } else if (pg && pg->m_pendingIdx < 0 && pathInterval < q[0x32]) {
            // Clamp accumulated time: subtract intervals until within one interval
            int64_t acc = q[0x32];
            do {
                acc -= pathInterval;
            } while (pathInterval < acc);
            q[0x32] = acc;
        }
    }

    // Stats collection (periodic)
    if (q[0x33] < 1) {
        CitizenBehavior_CollectStats(self);
        // Reset timer from DAT_1401721fc (float seconds)
        if (s_fn_Time_Ctor) {
            float* statsInterval = reinterpret_cast<float*>(
                ResolveAddr(Module::Exe, 0x1401721fc));
            if (statsInterval) {
                int64_t newTime = 0;
                s_fn_Time_Ctor(&newTime, *statsInterval);
                q[0x33] = newTime;
            }
        }
    }

    // Notification timer reset
    if (q[0x34] < 0) {
        *reinterpret_cast<uint32_t*>(&q[0x35]) = 0; // clear notify flags
        q[0x34] = 0;
    }
}

// ============================================================
// CitizenBehavior::ShuffleSchools (0x1400896a0)
// ============================================================
// Fisher-Yates shuffle on the schools array at +0x20/+0x28.
// Uses the global System::Random at GameUpdateLoop+0xD8.
//
// Original: FUN_1400896a0
void CitizenBehavior_ShuffleSchools(CitizenBehaviorRaw* self) {
    EnsureResolved();

    uint32_t count = *reinterpret_cast<uint32_t*>(self->data + 0x28);
    if (count < 2) return;

    // Get global Random from GameUpdateLoop
    void* random = *reinterpret_cast<void**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xD8);
    if (!random || !s_fn_Random_Rand) return;

    uint64_t* schoolArray = *reinterpret_cast<uint64_t**>(self->data + 0x20);
    if (!schoolArray) return;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t j = s_fn_Random_Rand(random, count);
        if (i != j) {
            uint64_t tmp = schoolArray[i];
            schoolArray[i] = schoolArray[j];
            schoolArray[j] = tmp;
        }
    }
}

// ============================================================
// CitizenBehavior::ShuffleEntities (0x140089470)
// ============================================================
// Looks up a profession in the BST at +0x100 by profId key,
// then Fisher-Yates shuffles the entity array in that BST node.
//
// BST node layout:
//   +0x00: uint64 key (profession ID)
//   +0x08: void*  entityArray (InternalBasePtr[])
//   +0x10: int32  entityCount
//   +0x20: void*  leftChild
//   +0x28: void*  rightChild
//
// Original: FUN_140089470
void CitizenBehavior_ShuffleEntities(CitizenBehaviorRaw* self, uint64_t profId) {
    EnsureResolved();

    // Walk BST at +0x100
    uint64_t* node = *reinterpret_cast<uint64_t**>(self->data + 0x100);
    if (!node) return;

    while (node) {
        uint64_t key = node[0];
        if (profId < key) {
            node = reinterpret_cast<uint64_t*>(node[4]); // left child at +0x20
        } else if (profId > key) {
            node = reinterpret_cast<uint64_t*>(node[5]); // right child at +0x28
        } else {
            // Found the node
            uint32_t count = static_cast<uint32_t>(node[2]); // entity count at +0x10
            if (count < 2) return;

            void* random = *reinterpret_cast<void**>(
                reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xD8);
            if (!random || !s_fn_Random_Rand) return;

            uint64_t* entities = reinterpret_cast<uint64_t*>(node[1]); // array at +0x08
            for (uint32_t i = 0; i < count; i++) {
                uint32_t j = s_fn_Random_Rand(random, count);
                if (i != j) {
                    uint64_t tmp = entities[i];
                    entities[i] = entities[j];
                    entities[j] = tmp;
                }
            }
            return;
        }
    }
}

// ============================================================
// CitizenBehavior::FindAny (0x14008c4b0)
// ============================================================
// Iterates allCitizensPtr (at +0xA0/+0xA8), finds the first citizen
// that is alive (age >= adult threshold), not dead (deathTimestamp == 0),
// and matches the expected profession. Assigns housing via
// CitizenComponent_AssignHousing if the citizen's current workplace/home
// doesn't match. Returns InternalBasePtr to the found citizen.
//
// Citizen checks (from decompiled code):
//   - *(float*)(citizen+0x288->+0x44) <= *(float*)(citizen+0x3c)  [age >= adult]
//   - *(int64*)(citizen+0x208) == 0                                [not dead]
//   - *(int64*)(citizen+0x1f0) == profListEntry+0x30               [profession matches]
//
// Original: CitizenComponent_impl_c4b0
void* CitizenBehavior_FindAny(CitizenBehaviorRaw* self, void* retval,
                               void* profSlot, uint64_t profId) {
    // This function is complex with many CheckPtr calls and component lookups.
    // For now, delegate to the original via the already-resolved function pointer.
    if (fn_CitizenList_FindAny) {
        fn_CitizenList_FindAny(self, retval, profSlot, profId);
        return retval;
    }

    // Fallback: return null InternalBasePtr
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(retval, nullptr);
    }
    return retval;
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
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf8);

    // Pairs array: each entry is {int32 cellA, int32 cellB} = 8 bytes
    int32_t* pairs = reinterpret_cast<int32_t*>(pg->m_pairs);

    for (int i = remaining; i >= 0; i--) {
        int idx = pg->m_pendingIdx;
        int32_t cellA = pairs[idx * 2];
        int32_t cellB = pairs[idx * 2 + 1];

        // Compute path cost between cells
        uint32_t cost = 0;
        if (fn_Map_CalcPathCost) {
            cost = (uint32_t)fn_Map_CalcPathCost(mapGrid, (uint32_t)cellA, cellB, 0);
        }

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
            if (fn_PathBST_Insert) {
                fn_PathBST_Insert(
                    reinterpret_cast<uint8_t*>(pg) + 0x18,  // &pg->m_bstRoot
                    &outNode,
                    &key);
            }
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
        if (fn_PathGraph_ReassignWorkers) {
            fn_PathGraph_ReassignWorkers(pg);
        }
        // Citizen assignment sweep
        if (fn_PathGraph_AssignCitizens) {
            fn_PathGraph_AssignCitizens(pg);
        }
        // Clear the pairs array
        if (fn_PathPairs_Clear) {
            fn_PathPairs_Clear(pg);
        }
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

    // Get key pointers from the game world singleton
    uint8_t* world = reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton);
    int* mapGrid = *reinterpret_cast<int**>(world + 0xf8);
    uint8_t* citizenBehavior = *reinterpret_cast<uint8_t**>(world + 0xf0);
    uint8_t* buildingList = *reinterpret_cast<uint8_t**>(world + 0xb0);

    // ---- Step 1: Collect grid cell IDs for all buildings ----
    int buildingCount = *reinterpret_cast<int*>(buildingList + 0x28);
    int64_t* buildingArray = *reinterpret_cast<int64_t**>(buildingList + 0x20);

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
        if (fn_Entity_GetGridId) {
            gridId = fn_Entity_GetGridId(&entityIBP, nullptr);
        }

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
                    fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, (int64_t)newCap * 4, 4) : nullptr);

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
                    if (fn_MemFreeStatic) fn_MemFreeStatic(buildingCells);
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
        if (fn_PathBST_FreeRecursive) {
            fn_PathBST_FreeRecursive(
                reinterpret_cast<uint8_t*>(pg) + 0x18,  // &pg->m_bstRoot
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(oldBst) + 0x18));
            fn_PathBST_FreeRecursive(
                reinterpret_cast<uint8_t*>(pg) + 0x18,
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(oldBst) + 0x20));
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(oldBst);
    }
    pg->m_bstRoot = nullptr;
    pg->m_bstCount = 0;

    // ---- Step 3: Clear pairs array ----
    if (fn_PathPairs_Clear) {
        fn_PathPairs_Clear(pg);
    }

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
            if (fn_Entity_GetGridId) {
                entityCell = fn_Entity_GetGridId(&entityIBP, nullptr);
            }

            // Get entity's region ID
            int entityRegion = *reinterpret_cast<int*>(
                navData + 0x18 + (int64_t)(uint32_t)entityCell * 0x30);

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
                        void* newArr = fn_MemAlloc ?
                            fn_MemAlloc(g_pMemAllocSingleton, (int64_t)newCap * 8, 4) : nullptr;
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
                        if (fn_MemFreeStatic) fn_MemFreeStatic(oldPairs);
                    }

                    // Insert placeholder into BST to prevent duplicate pairs
                    struct { int high; int low; int value; } bstKey;
                    bstKey.high = entityRegion;
                    bstKey.low = buildingRegion;
                    if (entityRegion < buildingRegion) {
                        bstKey.high = buildingRegion;
                        bstKey.low = entityRegion;
                    }
                    bstKey.value = 0;

                    if (fn_PathBST_Insert) {
                        void* dummy = nullptr;
                        fn_PathBST_Insert(
                            reinterpret_cast<uint8_t*>(pg) + 0x18,
                            &dummy,
                            &bstKey);
                    }
                }
            }
        }
    }

    // ---- Step 5: Set pending index ----
    pg->m_pendingIdx = (int)pg->m_pairCount - 1;

    // Free temporary building cells array
    if (buildingCells && fn_MemFreeStatic) {
        fn_MemFreeStatic(buildingCells);
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
// Also tracks food/fuel vs tools/health/clothing/textile totals in the
// notification summary at +0x180 (param_1[0x30] in Ghidra longlong* indexing).
//
// The RawMaterialFlags -> profession entry mapping:
//   bit 5  (Fuel)      -> entry 5  (Gatherer)     offset 0xa0
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
    auto findComponent = [](uintptr_t entity, uint32_t* typeIdPtr) -> void* {
        if (!entity || !typeIdPtr) return nullptr;
        uint32_t targetId = *typeIdPtr;
        void** compArray = *reinterpret_cast<void***>(entity + 0x30);
        int compCount = *reinterpret_cast<int*>(entity + 0x38);
        for (int i = 0; i < compCount; i++) {
            void* comp = compArray[i];
            if (!comp) continue;
            // vtable slot +0x10 (index 2) returns type ID
            void** vtable = *reinterpret_cast<void***>(comp);
            typedef int (*fn_GetTypeId_t)(void*);
            int compTypeId = reinterpret_cast<fn_GetTypeId_t>(vtable[2])(comp);
            if (static_cast<uint32_t>(compTypeId) == targetId) {
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(comp, targetId)) {
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
            // bit 5 (Fuel) -> entry 5, offset 0xa0
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
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(&entityIBP);
        }

        // Find StorageComponent on this entity
        void* storageComp = findComponent(static_cast<uintptr_t>(entityIBP),
                                          g_TypeId_StorageComponent_FC);

        // Wrap in InternalBasePtr
        int64_t storageIBP = 0;
        if (fn_InternalBasePtr_Ctor) {
            fn_InternalBasePtr_Ctor(&storageIBP, storageComp);
        }

        // Read item count from storage: *(int*)(storage + 0x30)
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(&storageIBP);
        }
        int32_t itemCount = *reinterpret_cast<int32_t*>(storageIBP + 0x30);
        if (itemCount <= 0) continue;

        bool hadNormalResources = false;
        bool hadDeadResources = false;

        // Iterate storage items: array at *(ptr*)(storage + 0x28), stride 0x10
        // Each item: [0x00] = item entity ptr, [0x08] = quantity (int cast from longlong)
        for (int32_t j = 0; j < itemCount; j++) {
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(&storageIBP);
            }
            int64_t* itemEntry = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(storageIBP + 0x28) + j * 0x10);

            // Find RawMaterialDescription on the item entity
            // Item entity has components at +0x28/+0x30 (different from top-level entity +0x30/+0x38)
            int64_t itemEntity = itemEntry[0];
            void* rawMat = nullptr;
            if (itemEntity) {
                void** subCompArray = *reinterpret_cast<void***>(itemEntity + 0x28);
                int subCompCount = *reinterpret_cast<int*>(itemEntity + 0x30);
                uint32_t rawMatTypeId = g_TypeId_RawMaterialDescription ?
                    *g_TypeId_RawMaterialDescription : 0;
                for (int k = 0; k < subCompCount; k++) {
                    void* subComp = *reinterpret_cast<void**>(
                        *reinterpret_cast<int64_t*>(itemEntity + 0x28) + k * 8);
                    if (!subComp) continue;
                    void** vtable = *reinterpret_cast<void***>(subComp);
                    typedef int (*fn_GetTypeId_t)(void*);
                    int compTypeId = reinterpret_cast<fn_GetTypeId_t>(vtable[2])(subComp);
                    if (static_cast<uint32_t>(compTypeId) == rawMatTypeId) {
                        if (fn_IsDerivedFrom && fn_IsDerivedFrom(subComp, rawMatTypeId)) {
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
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(&storageIBP);
            }
            notifySummary[5] += *reinterpret_cast<int32_t*>(storageIBP + 0x38);
        }
        if (hadNormalResources && notifySummary) {
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(&storageIBP);
            }
            notifySummary[1] += *reinterpret_cast<int32_t*>(storageIBP + 0x38);
        }
    }

    // --- Phase 2: Iterate second entity array (param_1[2]/param_1[3], offset +0x10/+0x18) ---
    // Same logic as Phase 1 but WITHOUT notification summary updates.
    int64_t secondCount = static_cast<int64_t>(static_cast<int32_t>(q[3])); // param_1[3] = +0x18
    for (int64_t idx = 0; idx < secondCount; idx++) {
        // Read entity InternalBasePtr from array
        int64_t entityIBP = *reinterpret_cast<int64_t*>(q[2] + idx * 8);
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(&entityIBP));
        }

        // Find StorageComponent on this entity
        uintptr_t entityAddr = static_cast<uintptr_t>(entityIBP);
        void* storageComp = findComponent(entityAddr, g_TypeId_StorageComponent_FC);

        // Wrap in InternalBasePtr
        int64_t storageIBP = 0;
        if (fn_InternalBasePtr_Ctor) {
            fn_InternalBasePtr_Ctor(&storageIBP, storageComp);
        }

        // Read item count from storage: *(int*)(storage + 0x30)
        if (fn_InternalBasePtr_CheckPtr) {
            fn_InternalBasePtr_CheckPtr(&storageIBP);
        }
        int32_t itemCount = *reinterpret_cast<int32_t*>(storageIBP + 0x30);
        if (itemCount <= 0) continue;

        // Iterate storage items
        for (int32_t j = 0; j < itemCount; j++) {
            if (fn_InternalBasePtr_CheckPtr) {
                fn_InternalBasePtr_CheckPtr(&storageIBP);
            }
            int64_t* itemEntry = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(storageIBP + 0x28) + j * 0x10);

            // Find RawMaterialDescription on the item entity
            int64_t itemEntity = itemEntry[0];
            void* rawMat = nullptr;
            if (itemEntity) {
                uint32_t rawMatTypeId = g_TypeId_RawMaterialDescription ?
                    *g_TypeId_RawMaterialDescription : 0;
                int subCompCount = *reinterpret_cast<int*>(itemEntity + 0x30);
                for (int k = 0; k < subCompCount; k++) {
                    void* subComp = *reinterpret_cast<void**>(
                        *reinterpret_cast<int64_t*>(itemEntity + 0x28) + k * 8);
                    if (!subComp) continue;
                    void** vtable = *reinterpret_cast<void***>(subComp);
                    typedef int (*fn_GetTypeId_t)(void*);
                    int compTypeId = reinterpret_cast<fn_GetTypeId_t>(vtable[2])(subComp);
                    if (static_cast<uint32_t>(compTypeId) == rawMatTypeId) {
                        if (fn_IsDerivedFrom && fn_IsDerivedFrom(subComp, rawMatTypeId)) {
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
    EnsureResolved();

    float notifyInterval = 120.0f; // DAT_140172210 = 120.0f seconds
    void* notifMgr = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x138);

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
                if (fn_NotificationEvent && fn_InternalBasePtr_Ctor) {
                    uint64_t nullIBP = 0;
                    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
                    fn_NotificationEvent(notifMgr, i + 0x11, 3, &nullIBP);
                }
                // Reset timer to notification cooldown (120 seconds)
                if (s_fn_Time_Ctor) {
                    int64_t newTime = 0;
                    s_fn_Time_Ctor(&newTime, notifyInterval);
                    *timer = newTime;
                }
            }
        }
    }

    // Part 2: Check resource notify entries (2 entries at +0x180, food/firewood storage)
    for (int i = 0; i < 2; i++) {
        uintptr_t notifyBase = reinterpret_cast<uintptr_t>(self->data + 0x180);
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
                if (fn_NotificationEvent && fn_InternalBasePtr_Ctor) {
                    uint64_t nullIBP = 0;
                    fn_InternalBasePtr_Ctor(&nullIBP, nullptr);
                    fn_NotificationEvent(notifMgr, notifType, 3, &nullIBP);
                }
                if (s_fn_Time_Ctor) {
                    int64_t newTime = 0;
                    s_fn_Time_Ctor(&newTime, notifyInterval);
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
// This function is extremely large (~600 lines decompiled) with 30+
// "push to history array" operations covering every profession resource
// and population metric. Delegated to original for now.
//
// Original: FUN_14008a730
void CitizenBehavior_CollectStats(CitizenBehaviorRaw* self) {
    if (fn_CitizenBehavior_CollectStats) {
        fn_CitizenBehavior_CollectStats(self);
    }
}

// ============================================================
// CitizenBehavior::FindIdle (0x14008cbf0)
// ============================================================
// Find an idle citizen near a building for worker assignment.
// Iterates all citizens, checks age >= adult, not dead, profession matches
// the "idle" profession (from g_pGameWorldSingleton+0xB0+0x30). For each
// candidate, looks up their current residence, iterates rooms, and finds
// idle occupants. Uses path distance to prefer closer buildings.
//
// Non-trivial return ABI: RCX=this, RDX=hidden_retval, R8+=args.
//
// Original: CitizenComponent_impl_sub_cbf0
void* CitizenBehavior_FindIdle(CitizenBehaviorRaw* self, void* retval,
                                void* profSlot, uint64_t profId) {
    if (fn_CitizenList_FindIdle) {
        fn_CitizenList_FindIdle(self, retval, profSlot, profId);
        return retval;
    }

    // Fallback: return null InternalBasePtr
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(retval, nullptr);
    }
    return retval;
}

// ============================================================
// CitizenBehavior::FindWorking (0x14008ca20)
// ============================================================
// Find a working citizen nearest to a building. Iterates all citizens,
// checks age >= adult, not dead, profession == idle profession. For each
// candidate with a valid workplace (+0x238), computes path distance to
// the target building. Returns the closest match.
//
// Non-trivial return ABI: RCX=this, RDX=hidden_retval, R8+=args.
//
// Original: CitizenComponent_impl_sub_ca20
void* CitizenBehavior_FindWorking(CitizenBehaviorRaw* self, void* retval,
                                   void* profSlot, uint64_t profId) {
    if (fn_CitizenList_FindWorking) {
        fn_CitizenList_FindWorking(self, retval, profSlot, profId);
        return retval;
    }

    // Fallback: return null InternalBasePtr
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(retval, nullptr);
    }
    return retval;
}

// ============================================================
// CitizenBehavior::FindStudent (0x14008c5f0)
// ============================================================
// Find a student citizen from the school with the most students.
// First shuffles schools array, then iterates all schools. For each
// school, finds its ResidenceComponent and iterates rooms/occupants
// to count students (citizens with idle profession). Picks the school
// with the most students, then returns the first eligible student from
// that school.
//
// Non-trivial return ABI: RCX=this, RDX=hidden_retval, R8+=args.
//
// Original: CitizenComponent_impl_c5f0
void* CitizenBehavior_FindStudent(CitizenBehaviorRaw* self, void* retval,
                                   void* profSlot, uint64_t profId) {
    if (fn_CitizenList_FindStudent) {
        fn_CitizenList_FindStudent(self, retval, profSlot, profId);
        return retval;
    }

    // Fallback: return null InternalBasePtr
    if (fn_InternalBasePtr_Ctor) {
        fn_InternalBasePtr_Ctor(retval, nullptr);
    }
    return retval;
}
