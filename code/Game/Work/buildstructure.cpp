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
#include "Work/exe_bodies.h"
//   +0x58: int    field_58      — count of build positions
//   +0x60: void*  field_60      — BST root (tracks which entity claimed which position)
//   +0x68: int    field_68      — build progress counter
//
// Work base fields used:
//   +0x10: void*  field_10      — InternalBasePtr to building entity
//   +0x20: int    field_20      — priority value
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/BuildStructure.h"
#include "Work/buildstructure_funcs.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "Action/exe_bodies.h"          // B3R: the Pf_* bodies called by name
#include "Component/exe_bodies.h"       // B3R: AttendanceComponent_GetJobHostComponent
#include "Dialog/dialog_exe_helpers.h"  // B3R: Dialog_FindComponent_d6c0
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5: CitizenComponent::TypeId()
#include "Entity/ComponentEntity.h"       // ENDGAME B.3.9 D5: ComponentEntity::TypeId()

// ============================================================
// Ctor (0x1400e5a90)
// ============================================================
// Retail 0x1400e5a90: `??0Internal`, the Work base init, the two stamps (the
// compiler's), then +0x50/+0x58 (a qword: count AND capacity), +0x60, +0x68.
// +0x48/+0x4c are left to Persist / the caller, as in retail.
BuildStructure::BuildStructure()
    : Work(),
      field_50(nullptr), field_58(0), field_5c(0), field_60(nullptr), field_68(0) {}

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
BuildStructure::BuildStructure(void* entityIBP, uint8_t altAnim)
    : Work(entityIBP),   // local_res10 = *entityIBP; Work_Ctor25(this, &local_res10)
      field_48(altAnim), field_4c(0),
      field_50(nullptr), field_58(0),
      // +0x5c is the array's capacity (read by FUN_14000a590). Retail zeroes +0x58 as a
      // full qword, so the resize below never reads a garbage capacity.
      field_5c(0),
      field_60(nullptr), field_68(0) {
    // Allocate the build-position byte array from the site's PositionComponent count.
    if (entityIBP) {
        uintptr_t posCompIBP = 0;
        WorkBase_FindComponent_2ed0(*reinterpret_cast<uintptr_t*>(entityIBP), &posCompIBP);
        int count = Pf_RecursiveChildSum_123070(posCompIBP, 1);
        // FUN_14000a590(&field_50, count): resizes the {data,count,cap} array in place,
        // writing field_58 = count. New bytes are uninitialised, so zero-fill below.
        Reimpl_Array_Resize_Byte(&field_50, count);
        int n = field_58;
        uint8_t* arr = reinterpret_cast<uint8_t*>(field_50);
        for (int i = 0; i < n && arr; i++) arr[i] = 0;
    }
}

// ============================================================
// BaseDtor (0x1400e5920)
// ============================================================
// The stamps (BuildStructure, then Work) and `~Internal` are the compiler's.
BuildStructure::~BuildStructure() {
    void* treeRoot = field_60;
    if (treeRoot != nullptr) {
        WorkBase_TreeNodeFree_14c30(&field_60, *(void**)((uintptr_t)treeRoot + 0x10));
        WorkBase_TreeNodeFree_14c30(&field_60, *(void**)((uintptr_t)treeRoot + 0x18));
        System::MemoryAllocator::MemFree(treeRoot);
    }
    field_60 = nullptr;
    field_68 = 0;

    System::MemoryAllocator::MemFree(field_50);
    field_50 = nullptr;
    field_58 = 0;
}

// ============================================================
// OnComplete (0x1400e4f80)
// ============================================================
int BuildStructure::OnComplete(void* citizenIBP, void* /*unused*/, int slotIndex) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)&field_60), citizenIBP);
    uint8_t* slots = (uint8_t*)field_50;
    slots[slotIndex] = 0;
    return 0;
}

// ============================================================
// OnCancel (0x1400e4f40)
// ============================================================
int BuildStructure::OnCancel(void* citizenIBP, void* /*unused*/, int slotIndex) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)&field_60), citizenIBP);
    uint8_t* slots = (uint8_t*)field_50;
    slots[slotIndex] = 0;
    return 0;
}

// ============================================================
// Cleanup (0x1400e57a0)
// ============================================================
int BuildStructure::Cleanup() {
    while (true) {
        void* root = field_60;
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

        uintptr_t entityPtr = *(uintptr_t*)node2;

        uintptr_t smComp = *(uintptr_t*)(entityPtr + 0x278);

        WorkBase_TaskListCleanup_c5400((void*)(smComp + 0x48));
    }
}

// ============================================================
// GetPriority (0x140029670) — not hookable, available as clean code
// ============================================================
int BuildStructure::GetPriority() {
    return field_68 > 0;
}

// ============================================================
// BuildStructure::Serialize (0x1400E5850)
// ============================================================
void BuildStructure::Persist(void* stream) {
    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // IO::Serializer block (version 0)
    alignas(8) uint8_t serBuf[24];
    ::new (static_cast<void*>(serBuf)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize bool at +0x48
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(*reinterpret_cast<bool*>(&field_48), nullptr);   // B.3.4 bridge

    // Serialize BST at +0x60 (InternalBasePtr tree)
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>(&field_60), serBuf);

    // Serialize count (+0x58), resize array on load
    int count = field_58;
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(*reinterpret_cast<int*>(&count), nullptr);   // B.3.4 bridge

    // Check if loading: first qword of serializer -> *(int*)(ptr + 8) == 0 means loading
    if (static_cast<int>(reinterpret_cast<IO::Serializer*>(serBuf)->GetDirection()) == 0) {   // B.3.4 bridge  (IO::Serializer::GetDirection, retail 0x180006210)
        Reimpl_Array_Resize_Byte(&field_50, count);
    }

    // Bulk serialize the byte array (one byte per build position)
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(count, reinterpret_cast<unsigned char*>(field_50), nullptr);   // B.3.4 bridge

    // Serialize rotating scan index at +0x4C
    reinterpret_cast<IO::Serializer*>(serBuf)->Serialize(*reinterpret_cast<int*>(&field_4c), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serBuf)->~Serializer();   // B.3.4 bridge
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


int BuildStructure::Execute(void* citizenPtr, void* /*context*/) {
    uintptr_t s = (uintptr_t)this;

    // --- Phase 1: Validate building is enabled ---
    uintptr_t entity = *(uintptr_t*)(s + 0x10);

    // Find PositionComponent on the building entity
    uintptr_t posCompIBP = 0;
    WorkBase_FindComponent_2ed0(entity, &posCompIBP);

    // Find the build-management component (type DWORD_1401c7008)
    entity = *(uintptr_t*)(s + 0x10);
    uintptr_t buildMgmtIBP = 0;
    Dialog_FindComponent_d6c0(reinterpret_cast<void*>(entity), &buildMgmtIBP);

    // Traverse: buildMgmt -> +0x10 child -> find the WorkPlaceComponent (DWORD_1401c80e0).
    // (Was documented as "build description"; corrected 2026-08-09 — the +0x50 read below
    // is the workplace accepting flag, not a description field. Behaviour unchanged.)
    uintptr_t buildMgmt = buildMgmtIBP;
    uintptr_t buildDescIBP = 0;
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(*(uintptr_t*)(buildMgmt + 0x10)), &buildDescIBP);

    // Check if building is enabled (byte at +0x50 of description)
    if (*(uint8_t*)(buildDescIBP + 0x50) == 0) {
        return 1;
    }

    // --- Phase 2: Check if we have positions left to build ---
    int totalPositions = Pf_RecursiveChildSum_123070(posCompIBP, 1);
    if (field_68 >= totalPositions) {
        return 1;
    }

    // --- Phase 3: Check builder count vs max, verify citizen authorization ---
    uintptr_t bcIBP = 0;
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(*(uintptr_t*)(s + 0x10)), &bcIBP);
    int currentBuilders = *(int32_t*)(bcIBP + 0x60);

    uintptr_t bc2 = bcIBP;
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*(uintptr_t*)(bc2 + 0x10), &matDescIBP);
    uintptr_t matDesc = matDescIBP;

    // Calculate max builders if not yet done
    if (*(int32_t*)(matDesc + 0x30) == 0) {
        *(int32_t*)(matDesc + 0x30) = WorkComponent_CalcMaxWorkers(matDesc);
    }

    // If at max builders, verify this citizen is on the builder list
    if (currentBuilders == *(int32_t*)(matDesc + 0x30)) {
        bool found = false;
        if (*(int32_t*)(bcIBP + 0x60) < 1) {
            return 1;
        }

        int i = 0;
        uintptr_t byteOff = 0;
        do {
            uintptr_t bcVal = bcIBP;
            uintptr_t builderId = *(uintptr_t*)(byteOff + *(uintptr_t*)(bcVal + 0x58));

            if (builderId == *(uintptr_t*)citizenPtr) {
                found = true;
            }
            byteOff += 8;
            i++;

        } while (i < *(int32_t*)(bcIBP + 0x60));

        if (!found) {
            return 1;
        }
    }

    // --- Phase 4: Count unbuilt positions ---
    int unbuiltCount = 0;
    int posCount = field_58;
    if (posCount > 0) {
        uint8_t* arr = (uint8_t*)field_50;
        for (int i = 0; i < posCount; i++) {
            if (arr[i] == 0) unbuiltCount++;
        }
    }

    // --- Phase 5: Get map grid and building center cell ---
    int* mapGrid = *(int**)((uintptr_t)*g_pGameWorldSingleton + 0xf8);

    uintptr_t citizenEntity = *(uintptr_t*)citizenPtr;

    uintptr_t comp278 = *(uintptr_t*)(citizenEntity + 0x278);
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
        int scanTotal = field_58;
        if (scanTotal > 0) {
            for (int scan = 0; scan < scanTotal; scan++) {
                int idx = field_4c;
                if (((uint8_t*)field_50)[idx] == 0) {
                    chosenIndex = idx;
                }
                int next = idx + 1;
                if (next >= scanTotal) next = 0;
                field_4c = next;
                if (chosenIndex >= 0) break;
            }
        }

        // Get world position for this build point
        float candidatePos[3];
        Pf_EntityResolvePos_2e20(
            reinterpret_cast<void*>(posCompIBP), candidatePos, 1, chosenIndex, nullptr);

        gridSize = *mapGrid;
        int candidateCell = WorldToCell(gridSize, candidatePos[0], candidatePos[1]);

        if (candidateCell != 0) {
            Pf_ZoneRefresh_9c480(mapGrid);   // retail sets RCX only (0x1400e532a / 54ea / 556c)

            uintptr_t gridData = *(uintptr_t*)(mapGrid + 0x20);
            if (CheckCellConnectivity(gridData, buildingCell, candidateCell)) {
                // Found accessible point -- get exact position
                float bestPos[3];
                Pf_EntityResolvePos_2e20(
                    reinterpret_cast<void*>(posCompIBP), bestPos, 1, chosenIndex, nullptr);

                gridSize = *mapGrid;
                int exactCell = WorldToCell(gridSize, bestPos[0], bestPos[1]);

                // Calculate initial path cost
                int bestPathCost = Pf_IsDstReachable_f810(
                    mapGrid, (uint32_t)buildingCell, exactCell, 1);

                // Iterate ALL positions to find a shorter path
                int total = field_58;
                int64_t centerCellOff = (int64_t)buildingCell * 0x30;
                if (total < 1) goto doSetupActions;

                for (int posIdx = 0; posIdx < total; posIdx++) {
                    if (posIdx == chosenIndex) continue;

                    float testPos[3];
                    Pf_EntityResolvePos_2e20(
                        reinterpret_cast<void*>(posCompIBP), testPos, 1, posIdx, nullptr);

                    gridSize = *mapGrid;
                    int testCell = WorldToCell(gridSize, testPos[0], testPos[1]);

                    // Check connectivity (first pass)
                    Pf_ZoneRefresh_9c480(mapGrid);   // retail sets RCX only (0x1400e532a / 54ea / 556c)
                    gridData = *(uintptr_t*)(mapGrid + 0x20);
                    if (!CheckCellConnectivity(gridData, buildingCell, testCell))
                        continue;

                    // Check connectivity (second pass after re-invalidation)
                    Pf_ZoneRefresh_9c480(mapGrid);   // retail sets RCX only (0x1400e532a / 54ea / 556c)
                    gridData = *(uintptr_t*)(mapGrid + 0x20);
                    if (!CheckCellConnectivity(gridData, buildingCell, testCell))
                        continue;

                    // Get path region IDs from grid entries
                    uintptr_t cEntry = gridData + centerCellOff;
                    uintptr_t tEntry = gridData + (int64_t)testCell * 0x30;
                    int32_t regionCenter = *(int32_t*)(cEntry + 0x18);
                    int32_t regionTest = *(int32_t*)(tEntry + 0x18);

                    if (regionCenter < 0) {
                        regionCenter = Pf_RegionSpiral_9d0a0(mapGrid, buildingCell);
                    }
                    if (regionTest < 0) {
                        regionTest = Pf_RegionSpiral_9d0a0(mapGrid, testCell);
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
                            // Retail 0x1400e5630..0x1400e56a1: R9D = the smaller region id (`lo`), left there by
                            // the cache-key normalisation; 0x1400a8ba0 forwards R9 to 0x14001dc00 untouched.
                            dist = Pf_RegionPathLength_8ba0(
                                *(int**)((uintptr_t)mapGrid + 0x70),
                                regionCenter, static_cast<uint32_t>(regionTest), static_cast<uint64_t>(lo));
                            Pf_RegionPairCacheInsert_5b30(
                                reinterpret_cast<uint64_t*>(*(void**)((uintptr_t)mapGrid + 0x78)),
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
                    BuildStructure_SetupBuildActions(
                        &citizenEntityVal,
                        field_20,
                        reinterpret_cast<int64_t*>((void*)(s + 0x10)),
                        bestPos,
                        chosenIndex,
                        (uint8_t)field_48
                    );

                    // Update building entity state
                    int priority = field_20;
                    uintptr_t bldgEntity = *(uintptr_t*)citizenPtr;

                    // Get target via vtable slot 10 (GetTarget, +0x50)
                    uintptr_t targetIBP = 0;
                    void* targetResult = GetTarget(&targetIBP);
                    uintptr_t targetVal = *(uintptr_t*)targetResult;

                    *(uint32_t*)(bldgEntity + 0x54) |= 4;
                    *(int32_t*)(bldgEntity + 0x30) = priority;
                    *(int32_t*)(bldgEntity + 0xb0) = 0;

                    if (targetVal == *(uintptr_t*)(bldgEntity + 0x1f0) && priority >= 0) {
                        *(uint32_t*)(bldgEntity + 0x88) |= 0x20000;
                    }

                    // Insert claimed position into BST at +0x60
                    WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)(s + 0x60)), &targetIBP, citizenPtr);

                    // Mark build position as claimed
                    ((uint8_t*)field_50)[chosenIndex] = 1;

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
void* BuildStructure::GetTarget(void* retBuf) {
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

// DWORD_1401c7158 = CitizenComponent (ENDGAME B.3.9 D5: by cell, not by the legacy
// comment name "TypeInfo_CompleteTask" -- CompleteTask has its own cell, 0x1401c5e50).
static bool CheckCitizenHasTask(BuildStructure* self, void* citizenIBP, uintptr_t* outTask) {
    uint32_t taskTypeId = CitizenComponent::TypeId();

    static int s_dormant = 0, s_kept = 0, s_dropped = 0;
    struct Rep { static void go(uint32_t tid, int d, int k, int dr) {
        fprintf(stderr, "[CITID/BuildValidate] tid=%u dormantSkips=%d kept=%d dropped=%d\n",
                tid, d, k, dr);
        fflush(stderr);
    } };

    // Check IBP is valid
    if (*(uintptr_t*)citizenIBP == 0) return false;

    // DAT_1401c5d70 = ComponentEntity (ENDGAME B.3.9 D5: by cell).
    bool isEntity = reinterpret_cast<const Object::Base*>(*(uintptr_t*)citizenIBP)->IsA(ComponentEntity::TypeId());   // B.3.x bridge (fn_IsA)
    if (!isEntity) return false;

    uintptr_t entity = *(uintptr_t*)citizenIBP;

    // Get state machine component at entity+0x278
    uintptr_t smComp = *(uintptr_t*)(entity + 0x278);

    // Iterate action stack: smComp+0x48 = array ptr, smComp+0x60 = count
    int actionCount = *(int*)(smComp + 0x60) + 1;  // original: 0x60 field + 1
    if (actionCount <= 0) return false;

    uintptr_t actionArray = *(uintptr_t*)(smComp + 0x48);
    for (int i = 0; i < actionCount; i++) {
        uintptr_t actionIBP = *(uintptr_t*)(actionArray + i * 8);

        // Check if this action is a CompleteTask
        bool isCompleteTask = Object::IsDerivedFromNullSafe((void*)actionIBP, taskTypeId);
        if (!isCompleteTask) continue;

        // Found a CompleteTask — wrap in IBP and check priority match
        uint8_t taskBuf[8];
        new (taskBuf) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(isCompleteTask ? (void*)actionIBP : nullptr));   // B.3.3 bridge
        uintptr_t taskPtr = *(uintptr_t*)taskBuf;

        if (taskPtr != 0) {
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

int BuildStructure::Validate() {
    uintptr_t s = (uintptr_t)this;

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

            bool hasTask = CheckCitizenHasTask(this, &citizenKey, &taskResult);

            if (!hasTask) {
                // Citizen has no matching task — add to remove list
                int newCount = removeCount + 1;
                if (newCount != removeCount) {
                    if (removeCap < newCount) {
                        removeCap = (removeCount == 0) ? newCount : newCount * 3;
                        void* newBuf = System::MemoryAllocator::MemAlloc((uint64_t)removeCap * 8, 8);
                        if (removeList != newBuf) {
                            // Copy existing entries
                            for (int k = 0; k < removeCount; k++) {
                                ((uint64_t*)newBuf)[k] = ((uint64_t*)removeList)[k];
                            }
                        }
                        // Set the new entry
                        ((uint64_t*)newBuf)[removeCount] = citizenKey;
                        if (removeList != newBuf) {
                            System::MemoryAllocator::MemFree(removeList);
                        }
                        removeList = newBuf;
                    } else {
                        ((uint64_t*)removeList)[removeCount] = citizenKey;
                    }
                    removeCount = newCount;
                }
            } else {
                // Citizen has matching task — record the slot index from task+0x34
                int slotIdx = *(int*)(taskResult + 0x34);

                int newSlotCount = slotCount + 1;
                if (newSlotCount != slotCount) {
                    if (slotCap < newSlotCount) {
                        slotCap = (slotCount == 0) ? newSlotCount : newSlotCount * 3;
                        void* newBuf = System::MemoryAllocator::MemAlloc((uint64_t)slotCap * 4, 4);
                        if (slotList != newBuf) {
                            for (int k = 0; k < slotCount; k++) {
                                ((int*)newBuf)[k] = ((int*)slotList)[k];
                            }
                        }
                        ((int*)newBuf)[slotCount] = slotIdx;
                        if (slotList != newBuf) {
                            System::MemoryAllocator::MemFree(slotList);
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
                    if (rList[i] < *searchNode) {
                        searchNode = (uintptr_t*)searchNode[2];  // left at +0x10
                    } else {
                        if (rList[i] <= *searchNode) {
                            // Found — remove from BST
                            WorkBST_BSTFindRemove_25520(treeRootPtr, searchNode);
                            break;
                        }
                        searchNode = (uintptr_t*)searchNode[3];  // right at +0x18
                    }
                }
            }

            // Update slot array: mark only validated slots as claimed
            int totalSlots = field_58;
            for (int si = 0; si < totalSlots; si++) {
                // Check if this slot index appears in the slotList
                bool found = false;
                for (int j = 0; j < slotCount; j++) {
                    if (((int*)slotList)[j] == si) {
                        found = true;
                        break;
                    }
                }
                ((uint8_t*)field_50)[si] = found ? 1 : 0;
            }

            System::MemoryAllocator::MemFree(slotList);
            System::MemoryAllocator::MemFree(removeList);
            return 0;
        }
    }

    System::MemoryAllocator::MemFree(slotList);
    System::MemoryAllocator::MemFree(removeList);
    return 0;

done_empty:
    System::MemoryAllocator::MemFree(slotList);
    System::MemoryAllocator::MemFree(removeList);
    return 1;
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_BuildStructure_Vtable` (slot 1 answered null, slot 2
// a lazily cached id) registered at 0x1401610a0, REGISTER_TYPE over
// BuildStructure_Factory / BuildStructure_CreateInPlace, and a lazy vtable-pointer
// cache (EnsureHelpers / EnsureVtables). Nothing outside this file reads 0x1401610a0
// back (sweep by hex and by BuildStructureAddr::Vtable), so the table's
// registration goes with it.

// create 0x140029630: MemAlloc(sizeof, 8), then the ctor.
void* BuildStructure::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(BuildStructure), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) BuildStructure());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* BuildStructure::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) BuildStructure());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

// The one entry point other blocks name: the full ctor on caller memory
// (buildcomponent_state8.cpp / destroycomponent_states.cpp, BuildComponent's state-8
// enter). B8-5: the real ctor, the same bytes.
extern "C" void* BuildStructure_CreateInPlaceFull(void* mem, void* entityIBP, uint8_t altAnim) {
    return static_cast<Object::Base*>(new (mem) BuildStructure(entityIBP, altAnim));
}
// The default ctor on caller memory (buildcomponent_state8.cpp names it).
extern "C" void* BuildStructure_CreateInPlace(void* mem) {
    return BuildStructure::CreateInPlaceObject(mem);
}

DEFINE_EXE_TYPE(BuildStructure, "Work")

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
    void* mem = Object::detail::InternalMemAlloc(sizeof(BuildStructure), 8);
    if (!mem) return nullptr;
    return static_cast<Object::Base*>(new (mem) BuildStructure(buildEntityIBP, 0));
}

// Drive Execute with the WORK-DRIVER ABI (self, citizenIBP). Execute reads *citizenIBP as the
// citizen entity, so a plain InternalBasePtr* is correct (same shape as the gather driver).
extern "C" int BuildStructure_ForceDrive(void* self, void* citizenIBP) {
    return reinterpret_cast<BuildStructure*>(self)->BuildStructure::Execute(citizenIBP, nullptr);   // B.3.8 bridge
}

// Execute-readiness gate for the force harness (no retail address). B3R: BuildStructure's
// helpers are called by name, so the only thing left to test is the game world (STORAGE).
extern "C" bool BuildStructure_DepsReady() {
    return g_pGameWorldSingleton && *g_pGameWorldSingleton;
}
