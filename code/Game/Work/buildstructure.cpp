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

#include "Work/buildstructure_funcs.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

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
    // Retail zeroes +0x58 as a qword — also clears the array capacity at +0x5c.
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0x5c) = 0;
    self->field_60 = nullptr;
    self->field_68 = 0;

    return self;
}

// ============================================================
// Full Ctor (0x1400e59c0) — 3-arg: (self, &entityIBP, altAnim)
// ============================================================
// The REAL retail ctor used by BuildComponent State8Enter (sub_7ae0). Unlike the
// 1-arg ctor (0x1400e5a90) it (a) routes base init through Work_Ctor25 so field_10
// (building entity), +0x18 (WorkComponent) and +0x28 (target cell) are populated, and
// (b) ALLOCATES the build-position byte array field_50/field_58 sized to the site's
// PositionComponent footprint (FUN_140123070 position count -> FUN_14000a590 resize),
// zeroing every position byte. Without this array Execute's unbuilt-position scan finds
// nothing (field_58 == 0 -> unbuiltCount 0) and returns 1 (never assigns labor) — the
// 2c-B blocker. Faithful offset-for-offset port of the Ghidra decompilation.
typedef void* (*fn_WorkCtor25_t)(void* self, void* pEntityIBP);   // 0x1400f72e0

void* BuildStructure_CtorFull(BuildStructure* self, void* entityIBP, uint8_t altAnim) {
    EnsureVtables();

    // local_res10 = *entityIBP; Work_Ctor25(self, &local_res10) — base init + field_10.
    void* entity = entityIBP ? *reinterpret_cast<void**>(entityIBP) : nullptr;
    fn_WorkCtor25_t workCtor25 =
        reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    if (workCtor25) {
        void* tmp = entity;
        workCtor25(self, &tmp);
    }

    self->vtable = s_vtable_BuildStructure;
    self->field_48 = altAnim;
    self->field_4c = 0;
    self->field_50 = nullptr;
    self->field_58 = 0;
    // +0x5c is the array's capacity (read by FUN_14000a590). Retail zeroes +0x58 as a
    // full qword; field_58 is int, so zero the capacity slot explicitly or the resize
    // reads garbage capacity out of the fresh MemAlloc block and takes a bad realloc path.
    *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(self) + 0x5c) = 0;
    self->field_60 = nullptr;
    self->field_68 = 0;

    // Allocate the build-position byte array from the site's PositionComponent count.
    if (entityIBP) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(entityIBP);
        uintptr_t posCompIBP = 0;
        fn_Entity_FindComponent_Position(*reinterpret_cast<uintptr_t*>(entityIBP), &posCompIBP);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posCompIBP);
        int count = fn_PositionComponent_Count ? fn_PositionComponent_Count(posCompIBP, 1) : 0;
        // FUN_14000a590(&field_50, count): resizes the {data,count,cap} array in place,
        // writing field_58 = count. New bytes are uninitialised, so zero-fill below.
        if (fn_DynArray_Resize) fn_DynArray_Resize(&self->field_50, count);
        int n = self->field_58;
        uint8_t* arr = reinterpret_cast<uint8_t*>(self->field_50);
        for (int i = 0; i < n && arr; i++) arr[i] = 0;
    }

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
int BuildStructure_OnComplete(BuildStructure* self, void* citizenIBP,
                                void* unused, int slotIndex) {
    fn_TreeRemoveByKey((void*)&self->field_60, citizenIBP);
    uint8_t* slots = (uint8_t*)self->field_50;
    slots[slotIndex] = 0;
    return 0;
}

// ============================================================
// OnCancel (0x1400e4f40)
// ============================================================
int BuildStructure_OnCancel(BuildStructure* self, void* citizenIBP,
                              void* unused, int slotIndex) {
    fn_TreeRemoveByKey((void*)&self->field_60, citizenIBP);
    uint8_t* slots = (uint8_t*)self->field_50;
    slots[slotIndex] = 0;
    return 0;
}

// ============================================================
// Cleanup (0x1400e57a0)
// ============================================================
int BuildStructure_Cleanup(BuildStructure* self) {
    while (true) {
        void* root = self->field_60;
        if (root == nullptr) return 0;

        void* node = root;
        void* leftChild = *(void**)((uintptr_t)node + 0x10);
        while (leftChild != nullptr) {
            node = leftChild;
            leftChild = *(void**)((uintptr_t)node + 0x10);
        }

        if (node == nullptr) return 0;

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

    // Traverse: buildMgmt -> +0x10 child -> find the WorkPlaceComponent (DWORD_1401c80e0).
    // (Was documented as "build description"; corrected 2026-08-09 — the +0x50 read below
    // is the workplace accepting flag, not a description field. Behaviour unchanged.)
    fn_InternalBasePtr_CheckPtr(&buildMgmtIBP);
    uintptr_t buildMgmt = buildMgmtIBP;
    fn_InternalBasePtr_CheckPtr((void*)(buildMgmt + 0x10));
    uintptr_t buildDescIBP = 0;
    fn_Entity_FindComponent_WorkPlace(*(uintptr_t*)(buildMgmt + 0x10), &buildDescIBP);
    fn_InternalBasePtr_CheckPtr(&buildDescIBP);

    // Check if building is enabled (byte at +0x50 of description)
    if (*(uint8_t*)(buildDescIBP + 0x50) == 0) {
        return 1;
    }

    // --- Phase 2: Check if we have positions left to build ---
    fn_InternalBasePtr_CheckPtr(&posCompIBP);
    int totalPositions = fn_PositionComponent_Count(posCompIBP, 1);
    if (self->field_68 >= totalPositions) {
        return 1;
    }

    // --- Phase 3: Check builder count vs max, verify citizen authorization ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x10));
    uintptr_t bcIBP = 0;
    fn_Entity_FindComponent_WorkPlace(*(uintptr_t*)(s + 0x10), &bcIBP);
    fn_InternalBasePtr_CheckPtr(&bcIBP);
    int currentBuilders = *(int32_t*)(bcIBP + 0x60);

    fn_InternalBasePtr_CheckPtr(&bcIBP);
    uintptr_t bc2 = bcIBP;
    fn_InternalBasePtr_CheckPtr((void*)(bc2 + 0x10));
    uintptr_t matDescIBP = 0;
    fn_Entity_FindComponent_MatDesc(*(uintptr_t*)(bc2 + 0x10), &matDescIBP);
    fn_InternalBasePtr_CheckPtr(&matDescIBP);
    uintptr_t matDesc = matDescIBP;

    // Calculate max builders if not yet done
    if (*(int32_t*)(matDesc + 0x30) == 0) {
        *(int32_t*)(matDesc + 0x30) = fn_CalcMaxBuilders(matDesc);
    }

    // If at max builders, verify this citizen is on the builder list
    if (currentBuilders == *(int32_t*)(matDesc + 0x30)) {
        bool found = false;
        fn_InternalBasePtr_CheckPtr(&bcIBP);
        if (*(int32_t*)(bcIBP + 0x60) < 1) {
            return 1;
        }

        int i = 0;
        uintptr_t byteOff = 0;
        do {
            fn_InternalBasePtr_CheckPtr(&bcIBP);
            uintptr_t bcVal = bcIBP;
            uintptr_t builderId = *(uintptr_t*)(byteOff + *(uintptr_t*)(bcVal + 0x58));

            fn_InternalBasePtr_CheckPtr(citizenPtr);
            if (builderId == *(uintptr_t*)citizenPtr) {
                found = true;
            }
            byteOff += 8;
            i++;

            fn_InternalBasePtr_CheckPtr(&bcIBP);
        } while (i < *(int32_t*)(bcIBP + 0x60));

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

    if (unbuiltCount <= 0) {
        return 1;
    }

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
            posCompIBP, candidatePos, 1, chosenIndex, nullptr);

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
                    posCompIBP, bestPos, 1, chosenIndex, nullptr);

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
                        posCompIBP, testPos, 1, posIdx, nullptr);

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

// ============================================================
// GetTarget (vtable +0x50, 0x1400E5830)
// ============================================================
// Returns the world's current build target entity via InternalBasePtr.
// Reads: gameWorld+0xB0 -> +0x38 (build target entity IBP)
// Uses hidden return pointer ABI: RCX=self, RDX=retBuf
void* BuildStructure_GetTarget(BuildStructure* /*self*/, void* retBuf) {
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t buildMgr = *(uintptr_t*)(gameWorld + 0xB0);
    *(uint64_t*)retBuf = *(uint64_t*)(buildMgr + 0x38);
    return retBuf;
}

// ============================================================
// Helper: FUN_1400f7100 — Check if citizen has a CompleteTask
// matching this Work's priority.
// ============================================================
// Walks the citizen's state machine action stack, finds CompleteTask
// actions, and checks if any match the Work's priority at +0x20.
// Returns 1 if found (and stores task IBP in *outTask), 0 otherwise.

// TypeInfo_CompleteTask = DWORD_1401c7158
static uint32_t* s_TypeId_CompleteTask = nullptr;

static bool CheckCitizenHasTask(BuildStructure* self, void* citizenIBP, uintptr_t* outTask) {
    if (!s_TypeId_CompleteTask) {
        s_TypeId_CompleteTask = (uint32_t*)ResolveAddr(Module::Exe, 0x1401c7158);
    }

    uint32_t taskTypeId = s_TypeId_CompleteTask ? *s_TypeId_CompleteTask : 0;

    // [CITID A/B] Report BOTH arms. With 0x1401c7158 unregistered this returns false for
    // every citizen, so Validate evicts the whole build BST; with the slot filled the
    // IsDerivedFrom loop below actually runs. Print the first few outcomes of each arm so
    // the two runs are comparable rather than both looking silently "fine".
    static int s_dormant = 0, s_kept = 0, s_dropped = 0;
    struct Rep { static void go(uint32_t tid, int d, int k, int dr) {
        fprintf(stderr, "[CITID/BuildValidate] tid=%u dormantSkips=%d kept=%d dropped=%d\n",
                tid, d, k, dr);
        fflush(stderr);
    } };

    if (taskTypeId == 0) {
        if (++s_dormant <= 3 || s_dormant % 500 == 0) Rep::go(0, s_dormant, s_kept, s_dropped);
        return false;
    }

    // Check IBP is valid
    if (*(uintptr_t*)citizenIBP == 0) return false;

    fn_InternalBasePtr_CheckPtr(citizenIBP);
    bool isEntity = fn_IsA((void*)*(uintptr_t*)citizenIBP, *g_TypeId_ComponentEntity);
    if (!isEntity) return false;

    fn_InternalBasePtr_CheckPtr(citizenIBP);
    uintptr_t entity = *(uintptr_t*)citizenIBP;

    // Get state machine component at entity+0x278
    fn_InternalBasePtr_CheckPtr((void*)(entity + 0x278));
    uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

    // Iterate action stack: smComp+0x48 = array ptr, smComp+0x60 = count
    int actionCount = *(int*)(smComp + 0x60) + 1;  // original: 0x60 field + 1
    if (actionCount <= 0) return false;

    uintptr_t actionArray = *(uintptr_t*)(smComp + 0x48);
    for (int i = 0; i < actionCount; i++) {
        uintptr_t actionIBP = *(uintptr_t*)(actionArray + i * 8);
        fn_InternalBasePtr_CheckPtr(&actionIBP);

        // Check if this action is a CompleteTask
        bool isCompleteTask = fn_IsDerivedFrom((void*)actionIBP, taskTypeId);
        if (!isCompleteTask) continue;

        // Found a CompleteTask — wrap in IBP and check priority match
        uint8_t taskBuf[8];
        new (taskBuf) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(isCompleteTask ? (void*)actionIBP : nullptr));   // B.3.3 bridge
        uintptr_t taskPtr = *(uintptr_t*)taskBuf;

        if (taskPtr != 0) {
            fn_InternalBasePtr_CheckPtr(taskBuf);
            taskPtr = *(uintptr_t*)taskBuf;
            // Check if task's priority (+0x30) matches work's priority (+0x20)
            if (*(int*)(taskPtr + 0x30) == self->field_20) {
                if (outTask) *outTask = taskPtr;
                if (++s_kept <= 3 || s_kept % 500 == 0)
                    Rep::go(taskTypeId, s_dormant, s_kept, s_dropped);
                return true;
            }
        }
    }

    if (++s_dropped <= 3 || s_dropped % 500 == 0)
        Rep::go(taskTypeId, s_dormant, s_kept, s_dropped);
    return false;
}

// ============================================================
// Validate (vtable +0x48, 0x1400E4BE0)
// ============================================================
// Walks the BST at +0x60 (citizens assigned to build slots).
// For each citizen, checks if they still have a matching CompleteTask.
// Citizens without a task are collected for removal.
// After traversal, removes invalid citizens from the BST and
// updates the slot array (+0x50) accordingly.
//
// Returns: 0 = some work remains (citizens validated/removed)
//          1 = BST is empty (no assigned citizens)

int BuildStructure_Validate(BuildStructure* self, void* /*citizenIBP*/) {
    uintptr_t s = (uintptr_t)self;

    // Dynamic arrays for citizens to remove and their slot indices
    void* removeList = nullptr;     // array of IBP keys (8 bytes each)
    int removeCount = 0;
    int removeCap = 0;

    void* slotList = nullptr;       // array of int slot indices (4 bytes each)
    int slotCount = 0;
    int slotCap = 0;

    uintptr_t* treeRootPtr = (uintptr_t*)(s + 0x60);

    // BST in-order traversal: find leftmost node first
    uintptr_t root = *treeRootPtr;
    if (root == 0) goto done_empty;

    {
        // Find leftmost (minimum) node
        uintptr_t node = root;
        while (*(uintptr_t*)(node + 0x10) != 0) {  // left child at +0x10
            node = *(uintptr_t*)(node + 0x10);
        }

        // In-order traversal
        while (node != 0) {
            uintptr_t citizenKey = *(uintptr_t*)node;
            uintptr_t taskResult = 0;

            bool hasTask = CheckCitizenHasTask(self, &citizenKey, &taskResult);

            if (!hasTask) {
                // Citizen has no matching task — add to remove list
                int newCount = removeCount + 1;
                if (newCount != removeCount) {
                    if (removeCap < newCount) {
                        removeCap = (removeCount == 0) ? newCount : newCount * 3;
                        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton,
                                                    (uint64_t)removeCap * 8, 8);
                        if (removeList != newBuf) {
                            // Copy existing entries
                            for (int k = 0; k < removeCount; k++) {
                                ((uint64_t*)newBuf)[k] = ((uint64_t*)removeList)[k];
                            }
                        }
                        // Set the new entry
                        ((uint64_t*)newBuf)[removeCount] = citizenKey;
                        if (removeList != newBuf) {
                            fn_MemFreeStatic(removeList);
                        }
                        removeList = newBuf;
                    } else {
                        ((uint64_t*)removeList)[removeCount] = citizenKey;
                    }
                    removeCount = newCount;
                }
            } else {
                // Citizen has matching task — record the slot index from task+0x34
                fn_InternalBasePtr_CheckPtr(&taskResult);
                int slotIdx = *(int*)(taskResult + 0x34);

                int newSlotCount = slotCount + 1;
                if (newSlotCount != slotCount) {
                    if (slotCap < newSlotCount) {
                        slotCap = (slotCount == 0) ? newSlotCount : newSlotCount * 3;
                        void* newBuf = fn_MemAlloc(g_pMemAllocSingleton,
                                                    (uint64_t)slotCap * 4, 4);
                        if (slotList != newBuf) {
                            for (int k = 0; k < slotCount; k++) {
                                ((int*)newBuf)[k] = ((int*)slotList)[k];
                            }
                        }
                        ((int*)newBuf)[slotCount] = slotIdx;
                        if (slotList != newBuf) {
                            fn_MemFreeStatic(slotList);
                        }
                        slotList = newBuf;
                    } else {
                        ((int*)slotList)[slotCount] = slotIdx;
                    }
                    slotCount = newSlotCount;
                }
            }

            // Move to next node (in-order successor)
            uintptr_t right = *(uintptr_t*)(node + 0x18);  // right child
            if (right != 0) {
                // Go right, then find leftmost
                node = right;
                while (*(uintptr_t*)(node + 0x10) != 0) {
                    node = *(uintptr_t*)(node + 0x10);
                }
            } else {
                // Go up until we come from a left child
                uintptr_t parent = *(uintptr_t*)(node + 0x08);  // parent at +0x08
                if (parent != 0 && *(uintptr_t*)(parent + 0x10) == node) {
                    // We are the left child — parent is next
                    node = parent;
                } else {
                    // We are the right child — go up until we find a left-child relationship
                    uintptr_t prev = node;
                    while (parent != 0 && *(uintptr_t*)(parent + 0x18) == prev) {
                        prev = parent;
                        parent = *(uintptr_t*)(parent + 0x08);
                    }
                    node = parent;
                }
            }
        }

        // Process remove list: remove each citizen from the BST
        if (removeCount > 0) {
            uint64_t* rList = (uint64_t*)removeList;
            for (int i = 0; i < removeCount; i++) {
                // BST lookup to find the node
                uintptr_t bstRoot = *treeRootPtr;
                uintptr_t* searchNode = (uintptr_t*)bstRoot;
                while (searchNode != nullptr) {
                    fn_InternalBasePtr_CheckPtr(searchNode);
                    if (rList[i] < *searchNode) {
                        searchNode = (uintptr_t*)searchNode[2];  // left at +0x10
                    } else {
                        fn_InternalBasePtr_CheckPtr(&rList[i]);
                        if (rList[i] <= *searchNode) {
                            // Found — remove from BST
                            fn_TreeRemoveByKey(treeRootPtr, searchNode);
                            break;
                        }
                        searchNode = (uintptr_t*)searchNode[3];  // right at +0x18
                    }
                }
            }

            // Update slot array: mark only validated slots as claimed
            int totalSlots = self->field_58;
            for (int si = 0; si < totalSlots; si++) {
                // Check if this slot index appears in the slotList
                bool found = false;
                for (int j = 0; j < slotCount; j++) {
                    if (((int*)slotList)[j] == si) {
                        found = true;
                        break;
                    }
                }
                ((uint8_t*)self->field_50)[si] = found ? 1 : 0;
            }

            fn_MemFreeStatic(slotList);
            fn_MemFreeStatic(removeList);
            return 0;
        }
    }

    fn_MemFreeStatic(slotList);
    fn_MemFreeStatic(removeList);
    return 0;

done_empty:
    fn_MemFreeStatic(slotList);
    fn_MemFreeStatic(removeList);
    return 1;
}

// ============================================================
// Standalone factory + vtable — Work family (factory cascade).
// Vtable @ 0x1401610a0 (13 slots), size 0x70, parent "Work".
// Shared slot helpers: work_base_standalone.{h,cpp}.
//
// NOTE: this file's ctor is named BuildStructure_Ctor (not _Ctor_Impl) and
// self-resolves via internal EnsureHelpers()/EnsureVtables(); there is no
// separate BuildStructure_ResolveFuncs(), so the factory does not pre-resolve.
// ============================================================
#include "work_base_standalone.h"
#include "Object/factory_registration.h"
#include "address_registry.h"

static uint32_t BuildStructure_GetTypeId(void*) {
    static uint32_t c = 0;
    if (!c) c = ::Object::GetTypeIdByName("BuildStructure");
    return c;
}

static void* g_BuildStructure_Vtable[13] = {
    (void*)&BuildStructure_ScalarDtor,  // +0x00 ScalarDtor
    (void*)&Work_Slot1,                 // +0x08 slot1 (null)
    (void*)&BuildStructure_GetTypeId,   // +0x10 GetTypeId
    (void*)&BuildStructure_Serialize,   // +0x18 Serialize
    (void*)&BuildStructure_Execute,     // +0x20 Execute
    (void*)&Work_Slot5,                 // +0x28 slot5 (returns 0)
    (void*)&BuildStructure_OnComplete,  // +0x30 OnComplete
    (void*)&BuildStructure_OnCancel,    // +0x38 OnCancel
    (void*)&BuildStructure_Cleanup,     // +0x40 Cleanup
    (void*)&BuildStructure_Validate,    // +0x48 Validate
    (void*)&BuildStructure_GetTarget,   // +0x50 GetTarget
    (void*)&BuildStructure_GetPriority, // +0x58 GetPriority
    (void*)&Work_Slot12,                // +0x60 slot12 (void nop)
};

extern "C" void RegisterBuildStructureVtable() {
    RegisterExeAddr(0x1401610a0, (uintptr_t)g_BuildStructure_Vtable);
}
extern "C" void* BuildStructure_CreateInPlace(void* mem) {
    return BuildStructure_Ctor((BuildStructure*)mem);
}
// Full 3-arg ctor (0x1400e59c0) — sets field_10 + build-position array. Used by the
// State8Enter staged work-spawn so BuildStructure::Execute can assign build positions.
extern "C" void* BuildStructure_CreateInPlaceFull(void* mem, void* entityIBP, uint8_t altAnim) {
    return BuildStructure_CtorFull((BuildStructure*)mem, entityIBP, altAnim);
}
extern "C" void* BuildStructure_Factory(void) {
    void* p = Work_Alloc(0x70);
    return p ? BuildStructure_Ctor((BuildStructure*)p) : nullptr;
}
REGISTER_TYPE(BuildStructure, "Work", &BuildStructure_Factory, &BuildStructure_CreateInPlace);

// ---- Construction force-harness wrappers (construction_force_harness.cpp) ----
// Mirror the GatherRawMaterials ConstructForSite/ForceDrive/DepsReady trio so the generic
// ForceWork harness can drive the state-8 BUILD LABOR half exactly like it drives the
// state-1 gather half. BuildStructure::Execute assigns the bound citizen a build position
// and enqueues the timed-labor action chain (MoveTo/ShowTool/PerformWork/CompleteTask) onto
// its CharacterComponent action stack; MoveTick's executor runs it. The gather driver reached
// state 8 crash-free; this drives the site 8 -> 2.

// Build a BuildStructure bound to a construction SITE entity using the FAITHFUL full ctor
// (0x1400e59c0): it routes base init through Work_Ctor25 (field_10=site entity, +0x18
// WorkComponent, +0x28 target cell) AND allocates the build-position byte array (field_50/
// field_58) from the site's PositionComponent footprint — without which Execute's unbuilt-
// position scan finds nothing (field_58==0) and returns 1 (never assigns labor). buildEntityIBP
// is an InternalBasePtr* whose *value is the site entity pointer.
extern "C" void* BuildStructure_ConstructForSite(void* buildEntityIBP) {
    EnsureHelpers();
    EnsureVtables();
    void* mem = Work_Alloc(0x70);
    if (!mem) return nullptr;
    return BuildStructure_CtorFull((BuildStructure*)mem, buildEntityIBP, 0);
}

// Drive Execute with the WORK-DRIVER ABI (self, citizenIBP). Execute reads *citizenIBP as the
// citizen entity, so a plain InternalBasePtr* is correct (same shape as the gather driver).
extern "C" int BuildStructure_ForceDrive(void* self, void* citizenIBP) {
    return BuildStructure_Execute((BuildStructure*)self, citizenIBP);
}

// Execute-readiness gate for the force harness. BuildStructure's Execute/ctor helpers are the
// SHARED engine_funcs globals (resolved once at startup by the engine-funcs init), not local
// s_fn_* — gate on the Execute+ctor-critical subset (mirrors GatherRawMaterials_DepsReady).
// Serialize helpers + the dtor-only parent vtable are excluded (Execute never touches them).
extern "C" bool BuildStructure_DepsReady() {
    EnsureHelpers();
    EnsureVtables();
    return fn_InternalBasePtr_CheckPtr && fn_InternalBasePtr_Ctor &&
           fn_Entity_FindComponent_Position && fn_Entity_FindComponent_Build &&
           fn_Entity_FindComponent_WorkPlace && fn_Entity_FindComponent_MatDesc &&
           fn_PositionComponent_Count && fn_CalcMaxBuilders && fn_DynArray_Resize &&
           fn_Entity_GetPositionForIndex && fn_Map_Invalidate && fn_Map_CalcPathCost &&
           fn_Map_NeighborLookup && fn_Map_CalcPathDist && fn_Map_CachePathDist &&
           fn_SetupBuildActions && fn_BST_Insert && fn_IsA && fn_IsDerivedFrom &&
           s_vtable_BuildStructure && g_pGameWorldSingleton && *g_pGameWorldSingleton;
}
