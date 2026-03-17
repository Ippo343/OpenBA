// code/Game/Work/buildstructure.cpp
// Clean reimplementation of BuildStructure Work class.
//
// BuildStructure (0x70 bytes) — structure construction work action.
// Extends Work (0x48 base) which extends Object::Internal.
// Vtable: 0x1401610a0
//
// Fields:
//   +0x48: bool   field_48      — use alternate animation flag (serialized)
//   +0x4C: int    field_4c      — rotating scan index into bool array (serialized)
//   +0x50: void*  field_50      — byte array (one per build position: 0=free, 1=claimed)
//   +0x58: int    field_58      — count of build positions
//   +0x60: void*  field_60      — BST root (tracks which entity claimed which position)
//   +0x68: int    field_68      — build progress counter
//
// Work base fields used:
//   +0x10: void*  field_10      — InternalBasePtr to building entity
//   +0x20: int    field_20      — priority value
//
// Decompiled from Application-x64.exe using Ghidra.
// This file must NOT depend on hooks.h — it is portable for standalone exe.

#include "Work/buildstructure_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstddef>
#include <cstring>

// ============================================================
// Helper: Object::Internal ctor/dtor (via dispatch slots)
// ============================================================
typedef void* (*fn_ObjectInternal_Ctor_fn)(void* self);
static fn_ObjectInternal_Ctor_fn s_fn_ObjectInternal_Ctor = nullptr;

static void EnsureHelpers() {
    if (!s_fn_ObjectInternal_Ctor) {
        s_fn_ObjectInternal_Ctor = reinterpret_cast<fn_ObjectInternal_Ctor_fn>(
            *reinterpret_cast<uintptr_t*>(ResolveAddr(Module::Exe, 0x140152768)));
    }
}

// ============================================================
// Vtable pointer cache
// ============================================================
static void* s_vtable_Work = nullptr;
static void* s_vtable_BuildStructure = nullptr;

static void EnsureVtables() {
    if (!s_vtable_Work) {
        s_vtable_Work = (void*)ResolveAddr(Module::Exe, 0x140161a80);
    }
    if (!s_vtable_BuildStructure) {
        s_vtable_BuildStructure = (void*)ResolveAddr(Module::Exe, 0x1401610a0);
    }
}

// ============================================================
// Ctor (0x1400e5a90)
// ============================================================
void* BuildStructure_Ctor(BuildStructure* self) {
    EnsureHelpers();
    EnsureVtables();

    s_fn_ObjectInternal_Ctor(self);

    self->vtable = s_vtable_Work;
    self->field_10 = nullptr;
    self->field_18 = nullptr;
    self->field_20 = -1;
    self->field_24 = 0;
    self->field_28 = -1;
    self->field_2c = 0;
    self->field_30 = nullptr;
    self->field_38 = nullptr;
    self->field_40 = 0;

    self->vtable = s_vtable_BuildStructure;

    self->field_50 = nullptr;
    self->field_58 = 0;
    self->field_60 = nullptr;
    self->field_68 = 0;

    return self;
}

// ============================================================
// ScalarDtor (0x140029680)
// ============================================================
void* BuildStructure_ScalarDtor(BuildStructure* self, uint32_t flags) {
    BuildStructure_BaseDtor(self);
    if (flags & 1) {
        fn_MemFreeStatic(self);
    }
    return self;
}

// ============================================================
// BaseDtor (0x1400e5920)
// ============================================================
void BuildStructure_BaseDtor(BuildStructure* self) {
    EnsureVtables();

    self->vtable = s_vtable_BuildStructure;

    void* treeRoot = self->field_60;
    if (treeRoot != nullptr) {
        fn_TreeRecursiveFree(&self->field_60, *(void**)((uintptr_t)treeRoot + 0x10));
        fn_TreeRecursiveFree(&self->field_60, *(void**)((uintptr_t)treeRoot + 0x18));
        fn_MemFreeStatic(treeRoot);
    }
    self->field_60 = nullptr;
    self->field_68 = 0;

    fn_MemFreeStatic(self->field_50);
    self->field_50 = nullptr;
    self->field_58 = 0;

    self->vtable = s_vtable_Work;
    fn_ObjectInternal_Dtor(self);
}

// ============================================================
// OnComplete (0x1400e4f80)
// ============================================================
void BuildStructure_OnComplete(BuildStructure* self, void* citizenIBP,
                                void* unused, int slotIndex) {
    fn_TreeRemoveByKey((void*)&self->field_60, citizenIBP);
    uint8_t* slots = (uint8_t*)self->field_50;
    slots[slotIndex] = 0;
}

// ============================================================
// OnCancel (0x1400e4f40)
// ============================================================
void BuildStructure_OnCancel(BuildStructure* self, void* citizenIBP,
                              void* unused, int slotIndex) {
    fn_TreeRemoveByKey((void*)&self->field_60, citizenIBP);
    uint8_t* slots = (uint8_t*)self->field_50;
    slots[slotIndex] = 0;
}

// ============================================================
// Cleanup (0x1400e57a0)
// ============================================================
void BuildStructure_Cleanup(BuildStructure* self) {
    while (true) {
        void* root = self->field_60;
        if (root == nullptr) return;

        void* node = root;
        void* leftChild = *(void**)((uintptr_t)node + 0x10);
        while (leftChild != nullptr) {
            node = leftChild;
            leftChild = *(void**)((uintptr_t)node + 0x10);
        }

        if (node == nullptr) return;

        void* node2 = root;
        if (node2 != nullptr) {
            void* left2 = *(void**)((uintptr_t)node2 + 0x10);
            while (left2 != nullptr) {
                node2 = left2;
                left2 = *(void**)((uintptr_t)node2 + 0x10);
            }
        }

        fn_InternalBasePtr_CheckPtr(node2);
        uintptr_t entityPtr = *(uintptr_t*)node2;

        fn_InternalBasePtr_CheckPtr((void*)(entityPtr + 0x278));
        uintptr_t smComp = *(uintptr_t*)(entityPtr + 0x278);

        fn_StateMachine_ReleaseTasks((void*)(smComp + 0x48));
    }
}

// ============================================================
// GetPriority (0x140029670) — not hookable, available as clean code
// ============================================================
bool BuildStructure_GetPriority(BuildStructure* self) {
    return self->field_68 > 0;
}

// ============================================================
// BuildStructure::Serialize (0x1400E5850)
// ============================================================
void BuildStructure_Serialize(BuildStructure* self, void* stream) {
    // Base class Work::Serialize
    fn_Work_SerializeBase(self, stream);

    // IO::Serializer block (version 0)
    uint8_t serBuf[24];
    g_fn_Serializer_Ctor(serBuf, stream, 0);

    // Serialize bool at +0x48
    g_fn_Serialize_Bool(serBuf, &self->field_48, nullptr);

    // Serialize BST at +0x60 (InternalBasePtr tree)
    fn_BST_SerializeHelper(&self->field_60, serBuf);

    // Serialize count (+0x58), resize array on load
    int count = self->field_58;
    g_fn_Serialize_Int(serBuf, &count, nullptr);

    // Check if loading: first qword of serializer -> *(int*)(ptr + 8) == 0 means loading
    int64_t serFirst = *(int64_t*)serBuf;
    if (*(int32_t*)(serFirst + 8) == 0) {
        fn_DynArray_Resize(&self->field_50, count);
    }

    // Bulk serialize the byte array (one byte per build position)
    fn_Serializer_SerializeBulk(serBuf, count, self->field_50, nullptr);

    // Serialize rotating scan index at +0x4C
    g_fn_Serialize_Int(serBuf, &self->field_4c, nullptr);

    g_fn_Serializer_Dtor(serBuf);
}

// ============================================================
// Helper: check terrain connectivity between two grid cells.
// Each cell is 0x30 bytes in the grid data array at mapGrid[0x20].
//   +0x26: int16 zoneId
//   +0x29: byte terrainType (bit index)
// Returns true if cells are walkably connected.
// ============================================================
static bool CheckCellConnectivity(uintptr_t gridData, int64_t cellA, int64_t cellB) {
    uintptr_t entryA = gridData + cellA * 0x30;
    uintptr_t entryB = gridData + cellB * 0x30;

    uint32_t bitA = 1u << (*(uint8_t*)(entryA + 0x29) & 0x1f);
    uint32_t bitB = 1u << (*(uint8_t*)(entryB + 0x29) & 0x1f);
    uint32_t combined = bitA | bitB;
    uint32_t mask = combined | 0xe1fu;

    if (!(mask & bitA) || !(mask & bitB)) return false;
    if (combined & 0x1e0) return true;

    int16_t zA = *(int16_t*)(entryA + 0x26);
    if (zA == 0) return false;
    int16_t zB = *(int16_t*)(entryB + 0x26);
    if (zB == 0) return false;
    return zA == zB;
}

// ============================================================
// Helper: convert world coords to clamped grid cell index.
// ============================================================
static int WorldToCell(int gridSize, float x, float z) {
    int half = gridSize >> 1;
    int cx = (int)(x + (float)half);
    int cz = (int)(z + (float)half);
    if (cx < 0) cx = 0;
    if (cx >= gridSize) cx = gridSize - 1;
    if (cz < 0) cz = 0;
    if (cz >= gridSize) cz = gridSize - 1;
    return gridSize * cz + cx;
}

// ============================================================
// BuildStructure::Execute (0x1400E4FC0)
// ============================================================
//
// Finds an unbuilt construction point on the building, checks walkability
// between the building center and candidate points on the map grid,
// optionally computes shortest path cost, then sets up the citizen's
// action queue (move, show tool, perform work, complete task).
//
// Returns 1 (not ready / skip) or 2 (work assigned).
//
int BuildStructure_Execute(BuildStructure* self, void* citizenPtr) {
    uintptr_t s = (uintptr_t)self;

    // --- Phase 1: Validate building is enabled ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t entity = *(uintptr_t*)(s + 0x10);

    // Find PositionComponent on the building entity
    uintptr_t posCompIBP = 0;
    fn_Entity_FindComponent_Position(entity, &posCompIBP);

    // Find the build-management component (type DWORD_1401c7008)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    entity = *(uintptr_t*)(s + 0x10);
    uintptr_t buildMgmtIBP = 0;
    fn_Entity_FindComponent_Build(entity, &buildMgmtIBP);

    // Traverse: buildMgmt -> +0x10 child -> find build description (type DWORD_1401c80e0)
    fn_InternalBasePtr_CheckPtr(&buildMgmtIBP);
    uintptr_t buildMgmt = *(uintptr_t*)buildMgmtIBP;
    fn_InternalBasePtr_CheckPtr((void*)(buildMgmt + 0x10));
    uintptr_t buildDescIBP = 0;
    fn_Entity_FindComponent_BuildDesc(*(uintptr_t*)(buildMgmt + 0x10), &buildDescIBP);
    fn_InternalBasePtr_CheckPtr(&buildDescIBP);

    // Check if building is enabled (byte at +0x50 of description)
    if (*(uint8_t*)(*(uintptr_t*)buildDescIBP + 0x50) == 0) {
        return 1;
    }

    // --- Phase 2: Check if we have positions left to build ---
    fn_InternalBasePtr_CheckPtr(&posCompIBP);
    int totalPositions = fn_PositionComponent_Count(*(uintptr_t*)posCompIBP, 1);
    if (self->field_68 >= totalPositions) {
        return 1;
    }

    // --- Phase 3: Check builder count vs max, verify citizen authorization ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t bcIBP = 0;
    fn_Entity_FindComponent_BuildDesc(*(uintptr_t*)(s + 0x10), &bcIBP);
    fn_InternalBasePtr_CheckPtr(&bcIBP);
    int currentBuilders = *(int32_t*)(*(uintptr_t*)bcIBP + 0x60);

    fn_InternalBasePtr_CheckPtr(&bcIBP);
    uintptr_t bc2 = *(uintptr_t*)bcIBP;
    fn_InternalBasePtr_CheckPtr((void*)(bc2 + 0x10));
    uintptr_t matDescIBP = 0;
    fn_Entity_FindComponent_MatDesc(*(uintptr_t*)(bc2 + 0x10), &matDescIBP);
    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    uintptr_t matDesc = *(uintptr_t*)matDescIBP;

    // Calculate max builders if not yet done
    if (*(int32_t*)(matDesc + 0x30) == 0) {
        *(int32_t*)(matDesc + 0x30) = fn_CalcMaxBuilders(matDesc);
    }

    // If at max builders, verify this citizen is on the builder list
    if (currentBuilders == *(int32_t*)(matDesc + 0x30)) {
        bool found = false;
        fn_InternalBasePtr_CheckPtr(&bcIBP);
        if (*(int32_t*)(*(uintptr_t*)bcIBP + 0x60) < 1) {
            return 1;
        }

        int i = 0;
        uintptr_t byteOff = 0;
        do {
            fn_InternalBasePtr_CheckPtr(&bcIBP);
            uintptr_t bcVal = *(uintptr_t*)bcIBP;
            uintptr_t builderId = *(uintptr_t*)(byteOff + *(uintptr_t*)(bcVal + 0x58));

            fn_InternalBasePtr_CheckPtr(citizenPtr);
            if (builderId == *(uintptr_t*)citizenPtr) {
                found = true;
            }
            byteOff += 8;
            i++;

            fn_InternalBasePtr_CheckPtr(&bcIBP);
        } while (i < *(int32_t*)(*(uintptr_t*)bcIBP + 0x60));

        if (!found) {
            return 1;
        }
    }

    // --- Phase 4: Count unbuilt positions ---
    int unbuiltCount = 0;
    int posCount = self->field_58;
    if (posCount > 0) {
        uint8_t* arr = (uint8_t*)self->field_50;
        for (int i = 0; i < posCount; i++) {
            if (arr[i] == 0) unbuiltCount++;
        }
    }

    // --- Phase 5: Get map grid and building center cell ---
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    fn_InternalBasePtr_CheckPtr(citizenPtr);
    uintptr_t citizenEntity = *(uintptr_t*)citizenPtr;

    fn_InternalBasePtr_CheckPtr((void*)(citizenEntity + 0x278));
    uintptr_t comp278 = *(uintptr_t*)(citizenEntity + 0x278);
    fn_InternalBasePtr_CheckPtr((void*)(comp278 + 0x68));
    uintptr_t posData = *(uintptr_t*)(comp278 + 0x68);

    (void)*(float*)(posData + 0x160);  // centerY: available for future use
    int gridSize = *mapGrid;
    int buildingCell = WorldToCell(gridSize,
        *(float*)(posData + 0x158), *(float*)(posData + 0x15c));

    int chosenIndex = -1;
    int attemptCount = 0;

    if (unbuiltCount <= 0) return 1;

    // --- Phase 6: Search for accessible unbuilt position ---
    while (attemptCount < unbuiltCount) {
        // Scan for next unbuilt position using rotating index
        int scanTotal = self->field_58;
        if (scanTotal > 0) {
            for (int scan = 0; scan < scanTotal; scan++) {
                int idx = self->field_4c;
                if (((uint8_t*)self->field_50)[idx] == 0) {
                    chosenIndex = idx;
                }
                int next = idx + 1;
                if (next >= scanTotal) next = 0;
                self->field_4c = next;
                if (chosenIndex >= 0) break;
            }
        }

        // Get world position for this build point
        fn_InternalBasePtr_CheckPtr(&posCompIBP);
        float candidatePos[3];
        fn_Entity_GetPositionForIndex(
            *(uintptr_t*)posCompIBP, candidatePos, 1, chosenIndex, nullptr);

        gridSize = *mapGrid;
        int candidateCell = WorldToCell(gridSize, candidatePos[0], candidatePos[1]);

        if (candidateCell != 0) {
            fn_Map_Invalidate(mapGrid);

            uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20);
            if (CheckCellConnectivity(gridData, buildingCell, candidateCell)) {
                // Found accessible point -- get exact position
                fn_InternalBasePtr_CheckPtr(&posCompIBP);
                float bestPos[3];
                fn_Entity_GetPositionForIndex(
                    *(uintptr_t*)posCompIBP, bestPos, 1, chosenIndex, nullptr);

                gridSize = *mapGrid;
                int exactCell = WorldToCell(gridSize, bestPos[0], bestPos[1]);

                // Calculate initial path cost
                int bestPathCost = fn_Map_CalcPathCost(
                    mapGrid, (uint32_t)buildingCell, exactCell, 1);

                // Iterate ALL positions to find a shorter path
                int total = self->field_58;
                int64_t centerCellOff = (int64_t)buildingCell * 0x30;
                if (total < 1) goto doSetupActions;

                for (int posIdx = 0; posIdx < total; posIdx++) {
                    if (posIdx == chosenIndex) continue;

                    fn_InternalBasePtr_CheckPtr(&posCompIBP);
                    float testPos[3];
                    fn_Entity_GetPositionForIndex(
                        *(uintptr_t*)posCompIBP, testPos, 1, posIdx, nullptr);

                    gridSize = *mapGrid;
                    int testCell = WorldToCell(gridSize, testPos[0], testPos[1]);

                    // Check connectivity (first pass)
                    fn_Map_Invalidate(mapGrid);
                    gridData = *(uintptr_t*)(mapGrid + 0x20);
                    if (!CheckCellConnectivity(gridData, buildingCell, testCell))
                        continue;

                    // Check connectivity (second pass after re-invalidation)
                    fn_Map_Invalidate(mapGrid);
                    gridData = *(uintptr_t*)(mapGrid + 0x20);
                    if (!CheckCellConnectivity(gridData, buildingCell, testCell))
                        continue;

                    // Get path region IDs from grid entries
                    uintptr_t cEntry = gridData + centerCellOff;
                    uintptr_t tEntry = gridData + (int64_t)testCell * 0x30;
                    int32_t regionCenter = *(int32_t*)(cEntry + 0x18);
                    int32_t regionTest = *(int32_t*)(tEntry + 0x18);

                    if (regionCenter < 0) {
                        regionCenter = fn_Map_NeighborLookup(mapGrid, buildingCell);
                    }
                    if (regionTest < 0) {
                        regionTest = fn_Map_NeighborLookup(mapGrid, testCell);
                    }
                    if (regionCenter < 0 || regionTest < 0) continue;

                    int dist;
                    if ((uint32_t)regionCenter == (uint32_t)regionTest) {
                        dist = 1;
                    } else {
                        // Normalize: hi >= lo
                        uint32_t hi = (uint32_t)regionCenter;
                        uint32_t lo = (uint32_t)regionTest;
                        if ((int32_t)hi < (int32_t)lo) {
                            hi = (uint32_t)regionTest;
                            lo = (uint32_t)regionCenter;
                        }

                        // Search path cache BST at mapGrid offset 0x78
                        uintptr_t pathCache = *(uintptr_t*)((uintptr_t)mapGrid + 0x78);
                        int* node = *(int**)(pathCache + 0x28);
                        dist = -1;
                        bool cached = false;

                        while (node != nullptr) {
                            int nHi = node[0];
                            if ((int)hi < nHi) {
                                node = *(int**)(node + 6);
                            } else if ((int)hi > nHi) {
                                node = *(int**)(node + 8);
                            } else {
                                int nLo = node[1];
                                if ((int)lo < nLo) {
                                    node = *(int**)(node + 6);
                                } else if ((int)lo <= nLo) {
                                    // Exact match
                                    int eIdx = node[2];
                                    uintptr_t entries = *(uintptr_t*)(pathCache + 0x18);
                                    dist = *(int32_t*)(entries + 8 + (int64_t)eIdx * 0x18);
                                    if (dist >= 0) cached = true;
                                    break;
                                } else {
                                    node = *(int**)(node + 8);
                                }
                            }
                        }

                        if (!cached) {
                            dist = fn_Map_CalcPathDist(
                                *(int**)((uintptr_t)mapGrid + 0x70),
                                regionCenter, regionTest);
                            fn_Map_CachePathDist(
                                *(void**)((uintptr_t)mapGrid + 0x78),
                                regionCenter, regionTest, dist);
                        }

                        if (dist < 0) continue;
                    }

                    // If path is shorter than half current best, adopt this position
                    if (dist < (bestPathCost >> 1)) {
                        bestPos[0] = testPos[0];
                        bestPos[1] = testPos[1];
                        bestPos[2] = testPos[2];
                    }
                }

                goto doSetupActions;

            doSetupActions:
                // --- Phase 7: Setup action queue ---
                {
                    // Prepare IBP values for action setup
                    uintptr_t citizenEntityVal = *(uintptr_t*)citizenPtr;

                    // FUN_1400c0690: sets up MoveTo, ShowTool, PerformWork, CompleteTask actions
                    // Args: citizenEntityIBP, priority, buildingEntityIBPptr, position, index, altAnim
                    fn_SetupBuildActions(
                        &citizenEntityVal,
                        self->field_20,
                        (void*)(s + 0x10),
                        bestPos,
                        chosenIndex,
                        (uint8_t)self->field_48
                    );

                    // Update building entity state
                    int priority = self->field_20;
                    fn_InternalBasePtr_CheckPtr(citizenPtr);
                    uintptr_t bldgEntity = *(uintptr_t*)citizenPtr;

                    // Get target via vtable slot 10 (GetTarget, +0x50)
                    typedef void* (*fn_GetTarget_t)(void*, void*);
                    void** vtbl = *(void***)self;
                    uintptr_t targetIBP = 0;
                    void* targetResult = ((fn_GetTarget_t)vtbl[10])(self, &targetIBP);
                    uintptr_t targetVal = *(uintptr_t*)targetResult;

                    *(uint32_t*)(bldgEntity + 0x54) |= 4;
                    *(int32_t*)(bldgEntity + 0x30) = priority;
                    *(int32_t*)(bldgEntity + 0xb0) = 0;

                    if (targetVal == *(uintptr_t*)(bldgEntity + 0x1f0) && priority >= 0) {
                        *(uint32_t*)(bldgEntity + 0x88) |= 0x20000;
                    }

                    // Insert claimed position into BST at +0x60
                    fn_BST_Insert((void*)(s + 0x60), &targetIBP, citizenPtr);

                    // Mark build position as claimed
                    ((uint8_t*)self->field_50)[chosenIndex] = 1;

                    return 2;
                }
            }
        }

        attemptCount++;
    }

    return 1;
}
