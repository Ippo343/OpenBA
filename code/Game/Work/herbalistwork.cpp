// code/Game/Work/herbalistwork.cpp
// Clean C++ reimplementation of HerbalistWork — all methods.
// Source: Game/Work/HerbalistWork.cpp (from profile build debug strings)
//
// HerbalistWork (0x60 bytes) — Herbalist gathering work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HerbalistWork
// Vtable: 0x1401615a0
// Same layout as ForesterWork (different vtable, different work logic).
//
// Key difference from ForesterWork: no plant/cut branching. Herbalist only
// searches for existing natural resources (herbs) that match the description
// list, checks growth threshold, then creates a PerformWork action to harvest.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/HerbalistWork.h"
#include "Work/herbalistwork_funcs.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Tutorial/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
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
#include <cmath>
#include <cstdlib>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/HerbalistComponent.h"    // ENDGAME B.3.9 D5
#include "Component/HerbalistDescription.h"  // ENDGAME B.3.9 D5
#include "Component/RadiusDescription.h"     // ENDGAME B.3.9 D5

// ============================================================
// Sub-functions, called by name (B3R; the module exe_bodies.h headers)
// ============================================================
//   0x1400e3db0  WorkGrid_AutoPickup_e3db0
//   0x140003050  WorkBase_FindComponent_3050
//   0x140002f90  WorkBase_FindComponent_2f90
//   0x14012c3f0  CitizenComp_FUN_14012c3f0
//   0x14012e5c0  WorkComponent_CalcMaxWorkers
//   0x140132010  WorkImpl_Sqrtf_132010
//   0x140087fb0  Pf_EntityAnchorCell_7fb0
//   0x140008830  WorkBase_CalcDistance_8830
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x1400052c0  WorkBase_FindComponent_52c0
//   0x14009f3e0  Reimpl_MapGetTerrainPos
//   0x1400c1eb0  WorkAct_PerformWorkCtor_c1eb0
//   0x1400c2490  WorkAct_PlayAnimationCtor_c2490
//   0x140021a30  WorkBST_RegisterWork_1a30
//   0x140115e60  CitizenAI_State_LongTermPlan
//   0x140006270  WorkBase_FindComponent_6270
//   0x14012cf10  CitizenComp_StorageComponent_QueryCount
//   0x1400296b0  WorkBST_BST_Serialize_296b0
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x14001a6e0  WorkBST_BSTRemove_a6e0
//   0x140025520  WorkBST_BSTFindRemove_25520
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x140014c30  WorkBase_TreeNodeFree_14c30
//   0x140172184  kRdata_140172184 (.rdata)
//   0x140172114  kRdata_140172114 (.rdata)
//   0x14017214c  kRdata_14017214c (.rdata)
//
// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: HerbalistWork_Ctor
// disassembles to `0x14004b1a9: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.


// ============================================================
// Helper: Find component by type ID in entity's component list
// Iterates array at (entity+0x30), count at (entity+0x38).
// Each entry is an InternalBasePtr; vtable+0x10 = GetTypeId().
// Returns the component pointer, or nullptr if not found.
// ============================================================
static void* FindComponentInList(uintptr_t entity, uint32_t typeId) {
    int count = *(int*)(entity + 0x38);
    uintptr_t arr = *(uintptr_t*)(entity + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t* ibpPtr = (uintptr_t*)(arr + (uintptr_t)i * 8);
        uintptr_t obj = *ibpPtr;
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
            ibpPtr = (uintptr_t*)(arr + (uintptr_t)i * 8);
            void* comp = (void*)*ibpPtr;
            if (Object::IsDerivedFromNullSafe(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}

// ============================================================
// Helper: Find component by type ID in sub-object's component list
// Uses the +0x28/+0x30 layout (different from entity main list).
// ============================================================
static void* FindSubComponentInList(uintptr_t subObj, uint32_t typeId) {
    uintptr_t listBase = *(uintptr_t*)(subObj + 0x28);
    int count = *(int*)(subObj + 0x30);
    for (int i = 0; i < count; i++) {
        uintptr_t obj = *(uintptr_t*)(listBase + (uintptr_t)i * 8);
        // slot 2 Type() (the old no-argument typedef never passed `this`)
        if (reinterpret_cast<Object::Base*>(obj)->Type() == typeId) {   // B.3.8 bridge
            void* comp = *(void**)(listBase + (uintptr_t)i * 8);
            if (Object::IsDerivedFromNullSafe(comp, typeId)) {
                return comp;
            }
            return nullptr;
        }
    }
    return nullptr;
}


// ============================================================
// Helper: Assign work and register with AI
// ============================================================
static void AssignWorkToEntity(HerbalistWork* self, void* citizenIBP, int priority) {
    uintptr_t citizen = *(uintptr_t*)citizenIBP;

    // Call vtable slot +0x50 (GetWorkEntity) on self
    uintptr_t workEntityIBP[2] = {0};
    uintptr_t* result = static_cast<uintptr_t*>(self->GetTarget(workEntityIBP));
    uintptr_t workEntity = *result;

    // Set entity flags
    *(uint32_t*)(citizen + 0x54) |= 4;
    *(uint32_t*)(citizen + 0xb0) = 0;
    *(int32_t*)(citizen + 0x30) = priority;

    // If work entity matches citizen's home entity (+0x1f0) and priority >= 0,
    // set the "owned work" flag
    if (workEntity == *(uintptr_t*)(citizen + 0x1f0) && priority >= 0) {
        *(uint32_t*)(citizen + 0x88) |= 0x20000;
    }

    // Register work with citizen AI subsystem at self+0x48
    uintptr_t citizenAISub = (uintptr_t)self + 0x48;
    WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)citizenAISub), workEntityIBP, citizenIBP);
}


// ============================================================
// HerbalistWork::Slot4 — Main work-assignment logic
// ============================================================
//
// Algorithm (differs from ForesterWork — no plant/cut branching):
// 1. Check if citizen already has auto-pickup assignments
// 2. Find WorkplaceComponent, RadiusComponent, MaterialComponent
// 3. Check if material count has reached capacity (return 1 = done if so)
// 4. Calculate search radius from workplace radius
// 5. Random iteration over grid cells in radius
// 6. For each valid cell:
//    a. Skip if terrain is forest/resource terrain (0x1e0 mask)
//    b. Check distance within 2x radius
//    c. If cell has a resource entity, find NatRes component
//    d. For each herb type in the description list:
//       - Check growth >= threshold
//       - Check resource description matches
//       - Check not flagged for removal
//       - Find nearest passable cell, check pathfinding
//       - Create PerformWork action to harvest
// 7. If no work found, handle failure (idle animation or replan)
//
int HerbalistWork::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;

    // Save citizen ref for later use
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
    // Step 2: Find components on the workplace entity
    // -------------------------------------------------------

    // Get material description from workplace entity (self+0x10)
    uintptr_t* plVar14 = (uintptr_t*)(selfAddr + 0x10);
    uintptr_t matDescIBP = 0;
    WorkBase_FindComponent_3050(*plVar14, &matDescIBP);

    // Find WorkplaceComponent on workplace entity
    uintptr_t entity = *plVar14;
    // DAT_1401c43a0 = HerbalistComponent (ENDGAME B.3.9 D5: by cell, not the legacy
    // variable name "WorkplaceComponent").
    void* workplaceComp = FindComponentInList(entity, HerbalistComponent::TypeId());

    // Wrap in InternalBasePtr
    uintptr_t workplaceIBP = 0;
    new (&workplaceIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(workplaceComp));   // B.3.3 bridge

    // Get RadiusComponent from workplace description (+0x18)
    void* workplaceDesc = *(void**)(workplaceIBP + 0x18);
    void* radiusComp = nullptr;
    // DAT_1401c43d8 = HerbalistDescription (ENDGAME B.3.9 D5: by cell, not the
    // legacy variable name "RadiusComponent").
    if (Object::IsDerivedFromNullSafe(workplaceDesc, HerbalistDescription::TypeId())) {
        radiusComp = workplaceDesc;
    }

    // Find MaterialComponent on workplace's sub-object (+0x28)
    uintptr_t entitySub = *(uintptr_t*)(*plVar14 + 0x28);
    // DAT_1401c4870 = RadiusDescription (ENDGAME B.3.9 D5: by cell, not the legacy
    // variable name "MaterialComponent").
    void* materialComp = FindSubComponentInList(entitySub, RadiusDescription::TypeId());

    // -------------------------------------------------------
    // Step 3: Check capacity — is material count at limit?
    // -------------------------------------------------------
    int materialIdx = *(int*)((uintptr_t)radiusComp + 0x30);
    uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
    uintptr_t resourceTracker = *(uintptr_t*)(gameWorld + 0xf0);
    uintptr_t resourceTable = *(uintptr_t*)(resourceTracker + 0x170);
    uintptr_t resEntry = resourceTable + (uintptr_t)materialIdx * 0x20;
    bool atCapacity = *(int*)(resEntry + 8) <= *(int*)(resEntry);

    // Set mesh visibility based on capacity (visGroup 2)
    uintptr_t meshIBP[2] = {0};
    WorkBase_FindComponent_2f90(*plVar14, meshIBP);
    CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>((void*)(*(uintptr_t*)meshIBP)), 2, atCapacity ? 1 : 0);

    // Get herb count (self->_field50 via qword index 10)
    int herbCount = (int)selfQ[10]; // self->_field50

    // If material description count is 0, calculate it
    if (*(int*)(matDescIBP + 0x30) == 0) {
        int maxWorkers = WorkComponent_CalcMaxWorkers(matDescIBP);
        *(int*)(matDescIBP + 0x30) = maxWorkers;
    }

    // If herbCount exceeds max workers OR at capacity, we're done (return 1)
    if (*(int*)(matDescIBP + 0x30) < herbCount || atCapacity) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 4: Calculate search parameters
    // -------------------------------------------------------
    void* rng = *(void**)(gameWorld + 0xd8);
    int* mapGrid = *(int**)(gameWorld + 0xf8);

    int workplaceRadius = *(int*)((uintptr_t)materialComp + 0x28);
    int radiusSq = workplaceRadius * workplaceRadius;
    float searchArea = WorkImpl_Sqrtf_132010((float)radiusSq * (kRdata_140172184));
    int mapSize = *mapGrid;
    int maxAttempts = (int)searchArea;

    // Workplace grid cell position
    int workplaceCell = (int)selfQ[5]; // self->_field28
    int cellRow = workplaceCell / mapSize;
    int cellCol = workplaceCell % mapSize;

    // Get citizen's current position
    uintptr_t citizenEntity = *(uintptr_t*)citizenIBP;

    uintptr_t aiComp = *(uintptr_t*)(citizenEntity + 0x278);
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

    // Get entity's grid cell for distance calculations
    uintptr_t entityIBPBuf[2] = { *plVar14, 0 };
    uint32_t entityGridCell = Pf_EntityAnchorCell_7fb0(entityIBPBuf, nullptr);

    float fGrowthVal = kRdata_14017214c;



    // -------------------------------------------------------
    // Step 5: Random search for herb spots
    // -------------------------------------------------------
    if (maxAttempts > 0) {
        for (int attempt = 0; attempt < maxAttempts; attempt++) {
            int randX, randY;
            int testCellX, testCellY;

            // Generate random position within radius (rejection sampling)
            do {
                do {
                    randX = reinterpret_cast<System::Random*>(rng)->RandTol(workplaceRadius);   // B.3.4b bridge
                    randY = reinterpret_cast<System::Random*>(rng)->RandTol(workplaceRadius);   // B.3.4b bridge
                    testCellX = cellCol + randX;
                    testCellY = cellRow + randY;
                } while (randY * randY + randX * randX > radiusSq);
            } while (testCellX < 0 || testCellX >= mapSize ||
                     testCellY < 0 || testCellY >= mapSize);

            uint32_t testCell = (uint32_t)(*mapGrid) * testCellY + testCellX;
            uintptr_t gridData = *(uintptr_t*)((uintptr_t)mapGrid + 0x80);

            // Check terrain type bitmask — skip "forest/resource" terrain (0x1e0 mask)
            uint8_t terrainByte = *(uint8_t*)(gridData + (uintptr_t)testCell * 0x30 + 0x29);
            uint32_t terrainBit = 1u << (terrainByte & 0x1f);
            if ((terrainBit & 0x1e0u) != 0) {
                continue;
            }

            // Check distance is within 2x workplace radius
            int dist = WorkBase_CalcDistance_8830(mapGrid, entityGridCell, testCell);
            if (dist > workplaceRadius * 2) {
                continue;
            }

            // Get resource entity at this cell (+8 in cell data)
            uintptr_t resourceEntity = *(uintptr_t*)(gridData + (uintptr_t)testCell * 0x30 + 8);
            if (resourceEntity == 0) {
                continue;
            }

            // -------------------------------------------------------
            // Found a resource entity — check if it's a valid herb
            // -------------------------------------------------------
            uintptr_t natResCompIBP[2] = {0};
            WorkBase_FindComponent_52c0(resourceEntity, natResCompIBP);

            if (*(uintptr_t*)natResCompIBP == 0) {
                continue;
            }

            // Check NatRes component against the description list
            uintptr_t natResCompIBP2[2] = {0};
            WorkBase_FindComponent_52c0(resourceEntity, natResCompIBP2);

            // Iterate through herb types in the description list
            int herbTypeCount = *(int*)((uintptr_t)radiusComp + 0x28);
            uintptr_t herbTypeList = *(uintptr_t*)((uintptr_t)radiusComp + 0x20);

            for (int herbIdx = 0; herbIdx < herbTypeCount; herbIdx++) {
                // Check growth >= threshold
                float growth = *(float*)(*(uintptr_t*)natResCompIBP2 + 0x28);
                if (growth < fGrowthVal) {
                    continue;
                }

                // Check resource description matches this herb type
                uintptr_t resDescRef = *(uintptr_t*)(resourceEntity + 0x28);
                uintptr_t herbTypeRef = *(uintptr_t*)(herbTypeList + (uintptr_t)herbIdx * 8);
                if (resDescRef != herbTypeRef) {
                    continue;
                }

                // Check not flagged for removal (bit 20 of +0x20)
                uint32_t flags20 = *(uint32_t*)(*(uintptr_t*)natResCompIBP2 + 0x20);
                if ((flags20 >> 20) & 1) {
                    continue;
                }

                // Find nearest passable cell for pathfinding
                int nearestCell = WorkGrid_FindCell_9ccf0(
                    mapGrid, testCell, citizenGridCell, 0xe07);
                if (nearestCell < 0) {
                    continue;
                }

                // Check path to the nearest passable cell
                bool canPath = static_cast<uint8_t>(WorkBase_ValidateCell_a00f0(
                    mapGrid, 0xe1f, nearestCell, citizenGridCell)) != 0;   // the bool door read AL
                if (!canPath) {
                    continue;
                }

                // -------------------------------------------------------
                // Valid herb found — create PerformWork action
                // -------------------------------------------------------
                float halfOffset = (float)(*mapGrid >> 1) - kRdata_140172114;
                Math::Vector3 cutPos(
                    (float)(nearestCell % *mapGrid) - halfOffset,
                    (float)(nearestCell / *mapGrid) - halfOffset,
                    0.0f);

                float pathPos[3];
                Reimpl_MapGetTerrainPos(mapGrid, pathPos, &cutPos.x, 0, nullptr);

                uintptr_t targetRef = *(uintptr_t*)natResCompIBP2;
                uintptr_t harvestEntityRef = *(uintptr_t*)citizenIBP;
                uintptr_t harvestBuf[2] = { harvestEntityRef, 0 };
                uintptr_t harvestTargetBuf[2] = { targetRef, 0 };
                WorkAct_PerformWorkCtor_c1eb0(
                    harvestBuf,
                    (int)selfQ[4],
                    harvestTargetBuf,
                    pathPos,
                    0,
                    1);

                *(uint32_t*)(selfAddr + 0x58) = 0;
                AssignWorkToEntity(this, citizenIBP, (int)selfQ[4]);
                return 2;
            }
        }
    }

    // -------------------------------------------------------
    // Step 7: No work found — handle failure
    // -------------------------------------------------------
    int failCount = (int)selfQ[0xb]; // self->_field58

    if (failCount > maxAttempts) {
        // Too many failures — check if citizen's storage has items, then replan
        uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

        uintptr_t storageIBP[2] = {0};
        WorkBase_FindComponent_6270(*(uintptr_t*)(citizenEnt + 0x10), storageIBP);

        int storageCount = CitizenComp_StorageComponent_QueryCount(reinterpret_cast<int64_t>((void*)*(uintptr_t*)storageIBP));
        if (storageCount < 1) {
            return 0;
        }

        // Citizen has items in storage — replan long term
        CitizenAI_State_LongTermPlan(reinterpret_cast<int64_t>(*(void**)citizenIBP));
    }
    else {
        // Play idle animation and increment fail counter
        uintptr_t idleEntityRef = *(uintptr_t*)citizenIBP;
        uintptr_t idleBuf[2] = { idleEntityRef, 0 };
        WorkAct_PlayAnimationCtor_c2490(idleBuf, (int)selfQ[4], 0);

        // Assign work (idle animation) and register with AI
        int priority = (int)selfQ[4];
        uintptr_t citizenEnt2 = *(uintptr_t*)citizenIBP;

        // Call vtable slot +0x50 (GetWorkEntity)
        uintptr_t workEntIBP[2] = {0};
        uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(workEntIBP));
        uintptr_t workEnt = *wResult;

        *(uint32_t*)(citizenEnt2 + 0x54) |= 4;
        *(int32_t*)(citizenEnt2 + 0x30) = priority;
        *(uint32_t*)(citizenEnt2 + 0xb0) = 0;

        if (workEnt == *(uintptr_t*)(citizenEnt2 + 0x1f0) && priority >= 0) {
            *(uint32_t*)(citizenEnt2 + 0x88) |= 0x20000;
        }

        uintptr_t citizenAISub = (uintptr_t)this + 0x48;
        WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)citizenAISub), workEntIBP, citizenIBP);

        // Increment fail counter
        *(int32_t*)(selfAddr + 0x58) = failCount + 1;
    }

    return 2;
}


// ============================================================
// HerbalistWork::Slot6 — NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// Calls BSTFindRemove on the BST at self+0x48, returns false.
// ============================================================
int HerbalistWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return false;
}


// ============================================================
// HerbalistWork::Slot7 — 9-byte thunk, too small to hook.
// Thunk: add rcx,0x48; jmp CitizenAI_impl_5520
// Calls BSTFindRemove on the BST at self+0x48.
// ============================================================
int HerbalistWork::OnCancel(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return 0;
}


// ============================================================
// HerbalistWork::Slot8 / CancelAll
// Loops through tree at +0x48 while count (+0x50) > 0.
// For each rightmost node, cancels the work item via engine call.
// ============================================================
int HerbalistWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        // Find rightmost node in BST
        void* node = *(void**)(selfAddr + 0x48);
        void* rightmost = node;
        while (node != nullptr) {
            rightmost = node;
            // Follow right child at +0x10 (descend to rightmost leaf)
            node = *(void**)((uintptr_t)node + 0x10);
        }

        // Cancel the work item: get citizen entity from node key
        uintptr_t citizenEntity = *(uintptr_t*)rightmost;

        // Navigate to citizen AI subsystem and cancel
        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// HerbalistWork::Slot9 / RemoveInvalid (164 instructions)
// Iterates BST, validates each entry via FUN_1400f7100,
// collects invalid ones, removes them from tree.
// Returns true if tree becomes empty, false otherwise.
// ============================================================
int HerbalistWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;  // Empty tree — nothing to validate
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
            // Add to removal list — grow array if needed
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
            // Go right, then all the way left
            node = rightChild;
            while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
                node = *(void**)((uintptr_t)node + 0x10);
            }
        } else {
            // Go up until we come from a left child
            void* parent = *(void**)((uintptr_t)node + 0x08);
            if (parent != nullptr && *(void**)((uintptr_t)parent + 0x10) != node) {
                // We came from the right — keep going up
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
            // Find and remove the node with this key from BST
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
// HerbalistWork::Slot10 / GetWorkEntity
// Returns entity from this->_field10's sub-object at +0x48.
// ============================================================
void* HerbalistWork::GetTarget(void* outPtr) {
    uintptr_t selfAddr = (uintptr_t)this;

    // CheckPtr on self+0x10
    uintptr_t entityRef = *(uintptr_t*)(selfAddr + 0x10);

    // Find sub-entity via FUN_140002e10
    uintptr_t subIBP[2] = {0};
    // 0x140002e10 returns its out IBP (RDX) in RAX; the ported body is declared void,
    // so the returned pointer is the out buffer itself.
    AttendanceComponent_GetJobHostComponent(reinterpret_cast<void*>(entityRef), subIBP);
    void* result = subIBP;

    // CheckPtr on the result

    // Read +0x48 from the resolved entity and store in output
    uintptr_t resolvedEntity = *(uintptr_t*)result;
    *(uintptr_t*)outPtr = *(uintptr_t*)(resolvedEntity + 0x48);

    return outPtr;
}


// ============================================================
// HerbalistWork::Serialize
// Work::Serialize + Serializer(version=0) + BST serialize + int field
// ============================================================
void HerbalistWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    // Work::Persist (0x1400f7400, a direct call in retail — QUALIFIED here)
    Work::Persist(stream);

    // Create first serializer (version block, version=0)
    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    // Create second serializer (version=0) for BST + int
    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    // Serialize BST at self+0x48
    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializer2);

    // Serialize int at self+0x58
    reinterpret_cast<IO::Serializer*>(serializer2)->Serialize(*reinterpret_cast<int*>((selfAddr + 0x58)), nullptr);   // B.3.4 bridge

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// HerbalistWork::DtorBody (0x1400EEC40)
// Sets vtables down chain, frees tree nodes at +0x48, then calls
// AutoPickupWork -> Work -> Internal dtor chain.
// ============================================================
// HerbalistWork dtor (retail 0x1400eec40, called by slot 0 0x14002a640). The stamps and the `~Internal` call are the compiler's chain.
HerbalistWork::~HerbalistWork() {
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




// [11] 0x140029860 (shared by 13 tables): `XOR EAX,EAX; CMP [RCX+0x50],EAX; SETG AL`
// — "is anyone assigned". The old table pointed at Work_Slot11_HasActiveTasks.
int HerbalistWork::GetPriority() {
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}

// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's.
HerbalistWork::HerbalistWork() : AutoPickupWork(), _field48(nullptr), _field50(0), _field58(0) {
}

// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_HerbalistWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over HerbalistWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.

// create 0x14002a5e0: MemAlloc(sizeof, 8), then the ctor.
void* HerbalistWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(HerbalistWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) HerbalistWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* HerbalistWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) HerbalistWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(HerbalistWork, "AutoPickupWork")
