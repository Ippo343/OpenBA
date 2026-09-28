// code/Game/Work/huntwork.cpp
// Clean C++ reimplementation of HuntWork -- all methods.
// Source: Game/Work/HuntWork.cpp (from profile build debug strings)
//
// HuntWork (0x58 bytes) -- Hunting work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HuntWork
// Vtable: 0x140161620
//
// Key difference from HerbalistWork: no herb search or growth check.
// HuntWork finds a valid hunting area cell, checks animal population,
// then creates a PerformWork action to hunt at that location.
// Does NOT serialize an extra int field (unlike HerbalistWork/ForesterWork).
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/HuntWork.h"
#include "Work/huntwork_funcs.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "math/Math.h"
#include "Work/exe_bodies.h"
#include "Action/exe_bodies.h"      // B3R: bodies called by name
#include "Component/exe_bodies.h"   // B3R: bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include "Component/HuntDescription.h"   // ENDGAME B.3.9 D5: HuntDescription::TypeId()

// ── Slot4 destination census (BANISHED_CROPFIELD_TRACE, the hunt trace flag). ──
// Retail's sub_bbc0 builds TWO different chains from this function's last argument:
//   param6=1 -> MoveTo(chosen cell, Slow, 0xe1f) + HuntAnimal chained behind it
//   param6=0 -> PerformWork(Time(5.0s)) IN PLACE  + HuntAnimal chained behind it

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x1400e3db0  WorkGrid_AutoPickup_e3db0
//   0x140003050  WorkBase_FindComponent_3050
//   0x140002f90  WorkBase_FindComponent_2f90
//   0x14012c3f0  CitizenComp_FUN_14012c3f0
//   0x14012e5c0  WorkComponent_CalcMaxWorkers
//   0x140006270  WorkBase_FindComponent_6270
//   0x14012cf10  CitizenComp_StorageComponent_QueryCount
//   0x14009f3e0  Reimpl_MapGetTerrainPos
//   0x1400bbbc0  WorkAct_HuntWorkCtor_bbbc0
//   0x140021a30  WorkBST_RegisterWork_1a30
//   0x140115e60  CitizenAI_State_LongTermPlan
//   0x14002a730  WorkLeaf_FindComp_a730
//   0x1400063d0  WorkLeaf_FindComp_63d0
//   0x140008830  WorkBase_CalcDistance_8830
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x140087fb0  Pf_EntityAnchorCell_7fb0
//   0x1400296b0  WorkBST_BST_Serialize_296b0
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x14001a6e0  WorkBST_BSTRemove_a6e0
//   0x140025520  WorkBST_BSTFindRemove_25520
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x140014c30  WorkBase_TreeNodeFree_14c30
//   0x140172114  kRdata_140172114 (.rdata)
//
// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: HuntWork_Ctor
// disassembles to `0x1400ef749: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.


// ============================================================
// HuntWork::Slot4 -- Hunt target assignment (0x1400EEFF0)
// ============================================================
//
// Algorithm:
// 1. Check auto-pickup assignments
// 2. If storage has items, replan citizen (LongTermPlan)
// 3. Find material description and herd/pasture component
// 4. Check if animal population allows hunting (resource tracker)
// 5. Set mesh visibility based on capacity
// 6. Check max workers vs current count
// 7. Random search for valid hunting cell within area radius
// 8. Validate cell: distance, pathability, terrain
// 9. Create PerformWork action at valid position
// 10. On failure: idle animation or replan
//
int HuntWork::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Save citizen ref
    uintptr_t citizenRef = *(uintptr_t*)entityIBP;
    void* citizenIBP = entityIBP;

    // -------------------------------------------------------
    // Step 1: Check auto-pickup assignments
    // -------------------------------------------------------
    int checkResult = WorkGrid_AutoPickup_e3db0(selfAddr, &citizenRef);
    if (checkResult == 2) {
        return 2;
    }

    // -------------------------------------------------------
    // Step 2: Check if citizen's storage has items -> replan
    // -------------------------------------------------------
    uintptr_t citizenEntity = *(uintptr_t*)entityIBP;

    uintptr_t storageIBP[2] = {0};
    WorkBase_FindComponent_6270(*(uintptr_t*)(citizenEntity + 0x10), storageIBP);

    int storageCount = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>((void*)*(uintptr_t*)storageIBP));
    if (storageCount >= 1) {
        CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*(void**)entityIBP));
        return 2;
    }

    // -------------------------------------------------------
    // Step 3: Find material description and herd component
    // -------------------------------------------------------
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*(uintptr_t*)(selfAddr + 0x10), &matDescIBP);

    uintptr_t herdIBP[2] = {0};
    WorkLeaf_FindComp_a730(*(uintptr_t*)(selfAddr + 0x10), herdIBP);

    // Get the herd object, check if it's derived from HerdComponent
    void* herdObj = *(void**)(*(uintptr_t*)herdIBP + 0x18);
    // DAT_1401c78c8 = HuntDescription (ENDGAME B.3.9 D5: by cell, not the legacy
    // variable name "HerdComponent").
    bool isHerd = Object::IsDerivedFromNullSafe(herdObj, HuntDescription::TypeId());
    void* herdComp = isHerd ? herdObj : nullptr;

    // Get area component from the workplace entity
    uintptr_t areaIBP[2] = {0};
    WorkLeaf_FindComp_63d0(*(uintptr_t*)(*(uintptr_t*)(selfAddr + 0x10) + 0x28), reinterpret_cast<void**>(areaIBP));

    // -------------------------------------------------------
    // Step 4: Check animal population via resource tracker
    // -------------------------------------------------------
    int materialIdx = *(int*)((uintptr_t)herdComp + 0x24);
    uintptr_t gameWorld = (uintptr_t)*g_pGameWorldSingleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // -------------------------------------------------------
    // Step 5: Set mesh visibility based on capacity (visGroup 2)
    // -------------------------------------------------------
    uintptr_t meshIBP[2] = {0};
    WorkBase_FindComponent_2f90(*(uintptr_t*)(selfAddr + 0x10), meshIBP);
    CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>((void*)(*(uintptr_t*)meshIBP)), 2, atCapacity ? 1 : 0);

    if (atCapacity) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 6: Check max workers vs current count
    // -------------------------------------------------------
    int huntCount = (int)selfQ[10]; // self->_field50

    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    if (huntCount >= *(int*)(matDescIBP + 0x30)) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 7: Random search for valid hunting cell
    // -------------------------------------------------------
    int* mapGrid = *(int**)(gameWorld + 0xf8);
    void* rng = *(void**)(gameWorld + 0xd8);

    int areaRadius = *(int*)((uintptr_t)areaIBP[0] + 0x28);
    int radiusSq = areaRadius * areaRadius;
    int mapSize = *mapGrid;

    // Workplace grid cell position
    uintptr_t workCell = selfQ[5]; // self->_field28
    int cellCol = (int)workCell % mapSize;
    int cellRow = (int)workCell / mapSize;

    // Generate random position within radius (rejection sampling)
    // Original only bounds-checks Y; we check both for safety but the
    // else branch below handles the case where the cell is invalid.
    int randX, randY;
    int testCellX, testCellY;

    do {
        do {
            randX = reinterpret_cast<System::Random*>(rng)->RandTol(areaRadius);   // B.3.4b bridge
            randY = reinterpret_cast<System::Random*>(rng)->RandTol(areaRadius);   // B.3.4b bridge
            testCellX = cellCol + randX;
            testCellY = cellRow + randY;
        } while (radiusSq < randY * randY + randX * randX);
    } while (testCellY < 0 || testCellY >= mapSize);
    // Note: X bounds not checked here matches original behavior.
    // Negative X will fail terrain/entity checks -> else branch handles it.

    uint32_t testCell = (uint32_t)(mapSize * testCellY + testCellX);

    // Get citizen's current position for pathfinding
    uintptr_t citEnt = *(uintptr_t*)citizenIBP;

    uintptr_t aiComp = *(uintptr_t*)(citEnt + 0x278);
    uintptr_t posComp = *(uintptr_t*)(aiComp + 0x68);

    int mapSz = *mapGrid;
    int citizenCellX = (int)(*(float*)(posComp + 0x158) + (float)(mapSz >> 1));
    int citizenCellY = (int)(*(float*)(posComp + 0x15c) + (float)(mapSz >> 1));

    // Clamp citizen cell to map bounds
    if (citizenCellX < 0) citizenCellX = 0;
    if (citizenCellX >= mapSz) citizenCellX = mapSz - 1;
    if (citizenCellY < 0) citizenCellY = 0;
    if (citizenCellY >= mapSz) citizenCellY = mapSz - 1;

    int citizenGridCell = mapSz * citizenCellY + citizenCellX;

    // Get workplace entity's grid cell for distance check
    uintptr_t entityIBPBuf[2] = { *(uintptr_t*)(selfAddr + 0x10), 0 };
    uint32_t entityGridCell = Pf_EntityAnchorCell_7fb0(entityIBPBuf, nullptr);

    // -------------------------------------------------------
    // Step 8: Validate cell — terrain, distance, entity, passability
    // Original checks all 4 conditions; if ANY fail, uses fallback action.
    // -------------------------------------------------------
    uintptr_t gridData = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);
    bool allChecksPass = false;

    // Only check if cell index is in bounds (negative X can produce bad index)
    if (testCellX < 0 || testCellX >= mapSize) {
    } else {
        // Check 1: terrain type — skip resource terrain (0x1e0 mask)
        uint8_t terrainByte = *(uint8_t*)(gridData + (uintptr_t)testCell * 0x30 + 0x29);
        uint32_t terrainBit = 1u << (terrainByte & 0x1f);
        bool terrainOk = (terrainBit & 0x1e0u) == 0;

        // Check 2: distance from workplace to random cell
        int dist = WorkBase_CalcDistance_8830(mapGrid, entityGridCell, testCell);
        bool distOk = dist <= areaRadius * 2;

        // Check 3: no entity at the cell
        uintptr_t cellEntity = *(uintptr_t*)(gridData + (uintptr_t)testCell * 0x30 + 8);
        bool emptyCell = (cellEntity == 0);

        // Attribute the failure to the FIRST failing check only, so the columns sum to
        // the fallback count instead of double-counting a cell that fails several.

        // Check 4: passability from random cell to citizen
        if (terrainOk && distOk && emptyCell) {
            bool canPath = (char)WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, testCell, citizenGridCell) != 0;
            allChecksPass = canPath;
        }
    }


    if (allChecksPass) {
        // -------------------------------------------------------
        // Valid hunt target — create hunt action at terrain position
        // -------------------------------------------------------
        float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
        Math::Vector3 huntPos(
            (float)((int)testCell % *mapGrid) - halfOffset,
            (float)((int)testCell / *mapGrid) - halfOffset,
            0.0f);

        // Get terrain height
        float pathPos[3];
        Reimpl_MapGetTerrainPos(mapGrid, pathPos, &huntPos.x, 0, nullptr);

        uintptr_t citizenEntRef = *(uintptr_t*)citizenIBP;

        uintptr_t huntCompIBP2 = 0;
        WorkLeaf_FindComp_a730(*(uintptr_t*)(selfAddr + 0x10), &huntCompIBP2);

        WorkAct_HuntWorkCtor_bbbc0(
            &citizenEntRef,        // citizen entity IBP
            (int)selfQ[4],         // priority
            reinterpret_cast<int64_t*>(&huntCompIBP2),         // hunt component IBP
            pathPos,               // terrain position
            0.0f,                  // float param
            1);                    // char param (specific location)
    } else {
        // -------------------------------------------------------
        // Cell checks failed — create fallback hunt action
        // Position (0,0,0), g_pAgeMultiplier float, char=0 (roaming)
        // -------------------------------------------------------
        uintptr_t citizenEntRef2 = *(uintptr_t*)citizenIBP;

        uintptr_t huntCompIBP3 = 0;
        uintptr_t workplaceEnt = *(uintptr_t*)(selfAddr + 0x10);
        WorkLeaf_FindComp_a730(workplaceEnt, &huntCompIBP3);

        float fallbackPos[3] = { 0.0f, 0.0f, 0.0f };

        WorkAct_HuntWorkCtor_bbbc0(
            &citizenEntRef2,       // citizen entity IBP
            (int)selfQ[4],         // priority
            reinterpret_cast<int64_t*>(&huntCompIBP3),         // hunt component IBP
            fallbackPos,           // fallback position (0,0,0)
            *g_pAgeMultiplier,     // float param (roaming radius)
            0);                    // char param (roaming mode)
    }

    // -------------------------------------------------------
    // Both paths: assign work and return 2
    // -------------------------------------------------------
    int priority = (int)selfQ[4];
    uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

    uintptr_t workEntIBP[2] = {0};
    uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
    uintptr_t workEnt = *wResult;

    *(uint32_t*)(citizenEnt + 0x54) |= 4;
    *(uint32_t*)(citizenEnt + 0xb0) = 0;
    *(int32_t*)(citizenEnt + 0x30) = priority;

    if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
    }

    WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), workEntIBP, citizenIBP);
    return 2;
}


// ============================================================
// HuntWork::Slot6 -- NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// Calls BSTFindRemove on the BST at self+0x48, returns false.
// ============================================================
int HuntWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return false;
}


// ============================================================
// HuntWork::Slot7 -- 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// ============================================================
int HuntWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return 0;
}


// ============================================================
// HuntWork::Slot8 / CancelAll (0x1400EF530)
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each leftmost node, cancels the work item via engine call.
// ============================================================
int HuntWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find leftmost node in BST (following +0x10 = left child)
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item: get citizen entity from node key
        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        // Navigate to citizen AI subsystem and cancel
        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// HuntWork::Slot9 / RemoveInvalid (0x1400EED90)
// Iterates BST, validates each entry via FUN_1400f7100,
// collects invalid ones, removes them from tree.
// Returns true if all valid (removeCount == 0), false otherwise.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP).
// ============================================================
int HuntWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;  // Empty tree -- nothing to validate
    }

    // Find leftmost node (minimum)
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        // Validate this entity
        bool stillValid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), nullptr, (void*)&entityIBP, nullptr);

        if (!stillValid) {
            // Add to removal list -- grow array if needed
            uint32_t newCount = removeCount + 1;
            if (newCount != removeCount) {
                uintptr_t* newArr = removeArr;
                if ((int)removeCapacity < (int)newCount) {
                    uint32_t newCap = newCount;
                    if (removeCount != 0) {
                        newCap = newCount * 3;
                    }
                    removeCapacity = newCap;
                    newArr = (uintptr_t*)System::MemoryAllocator::MemAlloc((int64_t)newCap * 8, 8);
                }

                // Copy existing entries if buffer changed
                uint32_t copyStart = 0;
                if (removeArr == newArr) {
                    copyStart = removeCount;
                }
                for (uint32_t i = copyStart; i < newCount; i++) {
                    uintptr_t val;
                    if (i < removeCount) {
                        val = removeArr[i];
                    } else {
                        val = entityIBP;
                    }
                    newArr[i] = val;
                }
                if (removeArr != newArr) {
                    System::MemoryAllocator::MemFree(removeArr);
                }
                removeArr = newArr;
                removeCount = newCount;
            }
        }

        // In-order successor: right child's leftmost descendant, or parent
        void* rightChild = *(void**)((uintptr_t)node + 0x18);
        if (rightChild != nullptr) {
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            void* parent = *(void**)((uintptr_t)node + 0x08);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x10) != node) {
                while (parent != nullptr && *(void**)((uintptr_t)parent + 0x18) == node) {
                    node = parent;
                    parent = *(void**)((uintptr_t)parent + 0x08);
                }
                node = parent;
            } else {
                node = parent;
            }
        }
    }

    // Remove all invalid entries from BST
    if (removeCount > 0) {
        for (uint32_t i = 0; i < removeCount; i++) {
            void* searchNode = *bstRootPtr;
            while (searchNode != nullptr) {
                if (removeArr[i] < *(uintptr_t*)searchNode) {
                    searchNode = *(void**)((uintptr_t)searchNode + 0x10);
                } else {
                    if (removeArr[i] <= *(uintptr_t*)searchNode) {
                        WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(bstRootPtr), searchNode);
                        break;
                    }
                    searchNode = *(void**)((uintptr_t)searchNode + 0x18);
                }
            }
        }
    }

    System::MemoryAllocator::MemFree(removeArr);
    return removeCount == 0;
}


// ============================================================
// HuntWork::Slot10 / GetWorkEntity (0x1400EF5A0)
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* HuntWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    uintptr_t subIBP[2] = {0};
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entityRef), subIBP);
    void* result = subIBP;


    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// HuntWork::GetPriority [11] 0x140029860 (shared by 13 tables) / HasWorkers.
// Returns (this->_field50 > 0). Slot 5 is Work's (0x1400f7540): the old dead
// HuntWork_Execute_Impl copy of it is gone.
// ============================================================
int HuntWork::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}


// ============================================================
// HuntWork::Serialize (0x1400EF600)
// Work::Serialize + Serializer(version=0) + BST serialize.
// Note: No extra int field (unlike HerbalistWork/ForesterWork).
// ============================================================
void HuntWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create first serializer (version block, version=0)
    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Create second serializer (version=0) for BST
    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at self+0x48
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializer2);

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// HuntWork::DtorBody (0x1400EF680)
// Sets vtables down chain, frees tree nodes at +0x48, then calls
// AutoPickupWork -> Work -> Internal dtor chain.
// ============================================================
// HuntWork dtor (retail 0x1400ef680, called by slot 0 0x14002a700). The stamps and the `~Internal` call are the compiler's chain.
HuntWork::~HuntWork() {
    uintptr_t selfAddr = (uintptr_t)this;

    // Free BST nodes at +0x48
    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        // Clean up left subtree
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        // Clean up right subtree
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        // Free root node
        {
            System::MemoryAllocator::MemFree(bstRoot);
        }
    }

    // Zero out BST root and count
    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}




// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's.
HuntWork::HuntWork() : AutoPickupWork(), _field48(nullptr), _field50(0) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_HuntWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over HuntWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.

// create 0x14002a690: MemAlloc(sizeof, 8), then the ctor.
void* HuntWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HuntWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HuntWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* HuntWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HuntWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(HuntWork, "AutoPickupWork")
