// code/Game/Work/pasturework.cpp
// Clean C++ reimplementation of PastureWork -- all methods.
// Source: Game/Work/PastureWork.cpp (from profile build debug strings)
//
// PastureWork (0x58 bytes) -- Pasture animal work action.
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> PastureWork
// Vtable: 0x1401617a0
//
// PastureWork is the most complex Work subclass. Slot4 handles:
// - Checking animal count vs max workers
// - Resource capacity checks
// - Removing animals from pasture (via RemoveAnimal action)
#include "Work/exe_bodies.h"
// - Finding empty pasture positions for animals
// - Creating grazing work actions
//
// Slot7 is unique: tree insertion + clearing "flagged for removal" entity bits.
// Slot8/Slot9/Slot10/Serialize/Dtor are identical patterns to HuntWork.
//
// Decompiled from Application-x64.exe using Ghidra.

#include "Work/PastureWork.h"
#include "Work/pasturework_funcs.h"
#include "object/Internal.h"
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
#include <cstdlib>
#include <cstring>
#include <new>
#include "Component/PastureDescription.h"   // ENDGAME B.3.9 D5
#include "Component/AnimalDescription.h"    // ENDGAME B.3.9 D5
#include "Component/PastureComponent.h"     // ENDGAME B.3.9 D5


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
//   0x140021a30  WorkBST_RegisterWork_1a30
//   0x140115e60  CitizenAI_State_LongTermPlan
//   0x140004fe0  WorkLeaf_FindComp_4fe0
//   0x140004680  FloorComponent_FindResData
//   0x140008830  WorkBase_CalcDistance_8830
//   0x14009ccf0  WorkGrid_FindCell_9ccf0
//   0x1400a00f0  WorkBase_ValidateCell_a00f0
//   0x140005500  ActHunt_FindComponent_5500
//   0x140002ed0  WorkBase_FindComponent_2ed0
//   0x1400223c0  ActRm_FindComponent_223c0
//   0x1400012d0  ActMv_FindComponentFamily_12d0
//   0x1400296b0  WorkBST_BST_Serialize_296b0
//   0x1400f7100  WorkBase_ValidateTask_f7100
//   0x14001a6e0  WorkBST_BSTRemove_a6e0
//   0x140025520  WorkBST_BSTFindRemove_25520
//   0x1400c5400  WorkBase_TaskListCleanup_c5400
//   0x140002e10  AttendanceComponent_GetJobHostComponent
//   0x140014c30  WorkBase_TreeNodeFree_14c30
//   0x140128350  WorkLeaf_Pasture_8350
//   0x1400bb0a0  WorkAct_GrazingWorkCtor_bb0a0
//   0x1400bbea0  Pf_RemoveAnimalCtor_bbea0
//   0x140122e20  Pf_EntityResolvePos_2e20
//   0x140123070  Pf_RecursiveChildSum_123070
//   0x1400f8620  WorkGrid_TryAssignWorkplace_f8620
//   0x1400c3470  ActHunt_NotifyEntityChanged_c3470
//   0x140172114  kRdata_140172114 (.rdata)
//
// Vtable pointers for dtor chain
//
// The Work BASE vtable is 0x140161a80, byte-verified in retail: PastureWork_Ctor
// disassembles to `0x1400f2da9: LEA RAX,[0x140161a80]` followed by MOV [RBX],RAX.
// The AutoPickupWork vtable it then overwrites with is 0x140160fa0 (the next LEA
// in the same ctor); only after that comes this class's own vtable.
// These were previously 0x1401612e0 / 0x140161340, neither of which is a vtable:
// both are INTERIOR addresses of the FireWork (0x1401612a0) and FishingWork
// (0x140161320) vtables, and Ghidra reports ZERO xrefs to either.


// ============================================================
// PastureWork::Slot4 -- Pasture work assignment (0x1400F2370)
// ============================================================
//
// This is the most complex Slot4 in the Work hierarchy (509 instructions, 279 lines).
// Three main branches:
//   A) If pasture resource NOT at capacity -> find empty animal slot, create RemoveAnimal action
//   B) If at capacity -> iterate position component for overflow storage work
//   C) Final fallback -> random position via Area component, create grazing work action
//
// Decompiled from Ghidra at 0x1400F2370, all control flow matched to original.
//
int PastureWork::Execute(void* entityIBP, void* /*context*/) {
    uintptr_t selfAddr = (uintptr_t)this;
    uintptr_t* selfQ = (uintptr_t*)this;
    void* citizenIBP = entityIBP;

    // local_res20 = *(longlong*)param_2;  local_res10 = param_2
    uintptr_t local_res20 = *(uintptr_t*)entityIBP;
    void* local_res10 = entityIBP;

    // -------------------------------------------------------
    // Step 1: Check auto-pickup assignments (FUN_1400e3db0)
    // -------------------------------------------------------
    int checkResult = WorkGrid_AutoPickup_e3db0(selfAddr, &local_res20);
    if (checkResult == 2) {
        return 2;
    }

    // plVar17 = param_1 + 2 => selfAddr + 0x10
    uintptr_t* plVar17 = (uintptr_t*)(selfAddr + 0x10);

    // -------------------------------------------------------
    // Step 2: Find StorageLocation and Pasture components
    // -------------------------------------------------------
    uintptr_t local_58 = 0;  // matDescIBP (StorageLocation component)
    WorkBase_FindComponent_3050(*plVar17, &local_58);

    uintptr_t local_a8 = 0;  // pastureIBP
    WorkLeaf_FindComp_4fe0(*plVar17, &local_a8);

    // -------------------------------------------------------
    // Step 3: Get pasture description and worker count
    // -------------------------------------------------------
    void* pvVar20 = *(void**)(local_a8 + 0x18);  // pasture component description
    bool bVar8 = Object::IsDerivedFromNullSafe(pvVar20, PastureDescription::TypeId());  // DAT_1401c7bd8
    uintptr_t lVar18 = selfQ[10];  // worker count at self+0x50
    void* pvVar19 = nullptr;
    if (bVar8) {
        pvVar19 = pvVar20;
    }

    // -------------------------------------------------------
    // Step 4: Check max workers
    // -------------------------------------------------------
    if (*(int*)(local_58 + 0x30) == 0) {
        int iVar9 = WorkComponent_CalcMaxWorkers(local_58);
        *(int*)(local_58 + 0x30) = iVar9;
    }

    // Original: if ((int)lVar18 <= *(int*)(local_58 + 0x30)) { ... whole body ... }
    // Inverted: if workerCount > maxWorkers, skip to return 1 at end
    if ((int)lVar18 > *(int*)(local_58 + 0x30)) {
        return 1;
    }

    // -------------------------------------------------------
    // Step 5: Check resource capacity via game world resource table
    //   lVar18 = *(int*)(pvVar19 + 0x20) * 0x20
    //   table = *(*(g_pGameWorldSingleton + 0xf0) + 0x170)
    //   bVar8 = table[lVar18 + 8] <= table[lVar18]
    // -------------------------------------------------------
    {
        uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
        int descIndex = *(int*)((uintptr_t)pvVar19 + 0x20);
        uintptr_t offset = (uintptr_t)descIndex * 0x20;
        uintptr_t tableBase = *(uintptr_t*)(*(uintptr_t*)(gameWorld + 0xf0) + 0x170);
        int currentCount = *(int*)(tableBase + offset);
        int maxCount = *(int*)(tableBase + offset + 8);
        bVar8 = (maxCount <= currentCount);  // at capacity?

        // Set mesh visibility based on capacity
        uintptr_t meshInstIBP = 0;
        WorkBase_FindComponent_2f90(*plVar17, &meshInstIBP);
        CitizenComp_FUN_14012c3f0(reinterpret_cast<int64_t*>(*(void**)&meshInstIBP), 2, bVar8 ? 1 : 0);
    }

    // -------------------------------------------------------
    // Branch A: NOT at capacity (bVar8 == false) -> find empty slot, remove animal
    // -------------------------------------------------------
    if (!bVar8) {
        uintptr_t gameWorld = (uintptr_t)GameUpdateLoop::_singleton;
        int* piVar4 = *(int**)(gameWorld + 0xf8);  // mapGrid

        // Get citizen position for pathfinding
        uintptr_t citEnt = *(uintptr_t*)citizenIBP;
        uintptr_t aiComp = *(uintptr_t*)(citEnt + 0x278);
        uintptr_t charComp = *(uintptr_t*)(aiComp + 0x68);

        int iVar21 = *piVar4;  // map size
        (void)*(uint32_t*)(charComp + 0x160);  // local_48: read but unused (matches original)
        int iVar9 = (int)(*(float*)(charComp + 0x158) + (float)(iVar21 >> 1));
        int iVar10 = (int)(*(float*)(charComp + 0x15c) + (float)(iVar21 >> 1));
        if (iVar9 < 0) iVar9 = 0;
        if (iVar21 <= iVar9) iVar9 = iVar21 - 1;
        if (iVar10 < 0) iVar10 = 0;
        if (iVar21 <= iVar10) iVar10 = iVar21 - 1;
        uint32_t uVar23 = iVar21 * iVar10 + iVar9;  // citizen cell index

        // Check if pasture has room for more animals (field+0x40 < field+0x38)
        int pastureCount = *(int*)(local_a8 + 0x38);
        if (*(int*)(local_a8 + 0x40) < pastureCount) {
            // Find empty animal slot via helper
            int slotIdx = WorkLeaf_Pasture_8350(local_a8);
            if (slotIdx >= 0) {
                // Get entity from pasture slot array
                local_res20 = *(uintptr_t*)(*(uintptr_t*)(local_a8 + 0x30) + (uintptr_t)slotIdx * 8);

                // Navigate entity chain to get NatRes component
                uintptr_t local_a0_0 = *(uintptr_t*)(local_res20 + 0x10);
                uintptr_t natResIBP = 0;
                ActHunt_FindComponent_5500(local_a0_0, &natResIBP);

                if (natResIBP != 0) {
                    // Check if not flagged for removal (bit 20)
                    if (((*(uint32_t*)(natResIBP + 0x20) >> 0x14) & 1) == 0) {
                        // Get entity from natRes to find Family component
                        local_a0_0 = *(uintptr_t*)(natResIBP + 0x10);
                        uintptr_t familyIBP[2] = {0};
                        ActMv_FindComponentFamily_12d0(local_a0_0, familyIBP);  // Entity_FindComponent_Family @ 0x1400012d0
                        uintptr_t familyComp = *(uintptr_t*)familyIBP;
                        uintptr_t familyCharComp = *(uintptr_t*)(familyComp + 0x68);

                        // Get animal position as cell index
                        iVar21 = *piVar4;
                        (void)*(uint32_t*)(familyCharComp + 0x160);  // local_48: read but unused
                        iVar9 = (int)(*(float*)(familyCharComp + 0x158) + (float)(iVar21 >> 1));
                        iVar10 = (int)(*(float*)(familyCharComp + 0x15c) + (float)(iVar21 >> 1));
                        if (iVar9 < 0) iVar9 = 0;
                        if (iVar21 <= iVar9) iVar9 = iVar21 - 1;
                        if (iVar10 < 0) iVar10 = 0;
                        if (iVar21 <= iVar10) iVar10 = iVar21 - 1;
                        int animalCell = iVar21 * iVar10 + iVar9;

                        // Pathfinding check
                        uint32_t uVar11 = (uint32_t)WorkGrid_FindCell_9ccf0(piVar4, animalCell, uVar23, 0xe1f);
                        uint64_t uVar16 = (uint64_t)WorkBase_ValidateCell_a00f0(piVar4, 0xe1f, (uint32_t)uVar11, uVar23);

                        if ((char)uVar16 != '\0') {
                            // Get description from natRes for RemoveAnimal action
                            void* descPtr = *(void**)(natResIBP + 0x18);
                            bool isDerived = Object::IsDerivedFromNullSafe(descPtr, AnimalDescription::TypeId());
                            pvVar20 = nullptr;
                            if (isDerived) {
                                pvVar20 = descPtr;
                            }

                            // Compute world position from cell index
                            float halfOff = (float)(*piVar4 >> 1) - kRdata_140172114;
                            Math::Vector3 local_80(
                                (float)((int)uVar11 % *piVar4) - halfOff,
                                (float)((int)uVar11 / *piVar4) - halfOff,
                                0.0f);
                            float local_70[4];  // Vector3 + padding
                            Reimpl_MapGetTerrainPos(piVar4, local_70, &local_80.x, '\0', nullptr);

                            // Build RemoveAnimal action
                            uintptr_t local_90 = *(uintptr_t*)citizenIBP;
                            uintptr_t local_a0_ibp = natResIBP;
                            Pf_RemoveAnimalCtor_bbea0(
                                &local_90,              // citizenIBP
                                (int)selfQ[4],          // priority
                                reinterpret_cast<uint32_t*>(local_70),               // position
                                animalCell,             // cell index
                                &local_a0_ibp,          // natRes IBP
                                (uintptr_t)pvVar20,     // description ptr
                                1);                     // flag (CONCAT31 result)

                            // Assign work to citizen
                            int priority = (int)selfQ[4];
                            uintptr_t citizenEnt = *(uintptr_t*)citizenIBP;

                            // Call vtable slot +0x50 (GetWorkEntity)
                            uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(&local_90));
                            uintptr_t workEnt = *wResult;

                            *(uint32_t*)(citizenEnt + 0x54) |= 4;
                            *(int32_t*)(citizenEnt + 0x30) = priority;
                            *(uint32_t*)(citizenEnt + 0xb0) = 0;

                            if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                                *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
                            }
                            return 2;
                        }
                    }
                }
            }
            // Slot search failed
            return 1;
        }

        // -------------------------------------------------------
        // Branch B: At animal capacity -> try position-based overflow storage work
        // -------------------------------------------------------
        uintptr_t posCompIBP = 0;
        WorkBase_FindComponent_2ed0(*plVar17, &posCompIBP);

        int posCount = Pf_RecursiveChildSum_123070(posCompIBP, 2);

        if (posCount > 0) {
            int iPvVar20 = 0;
            do {
                float local_80[3];
                Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posCompIBP), local_80, 2, iPvVar20, nullptr);

                int iVar10map = *piVar4;
                int iVar21pos = (int)(local_80[0] + (float)(iVar10map >> 1));
                int iVar12 = (int)(local_80[1] + (float)(iVar10map >> 1));
                if (iVar21pos < 0) iVar21pos = 0;
                if (iVar10map <= iVar21pos) iVar21pos = iVar10map - 1;
                if (iVar12 < 0) iVar12 = 0;
                if (iVar10map <= iVar12) iVar12 = iVar10map - 1;

                // Read entity from map grid cell
                uintptr_t mapDataBase = *(uintptr_t*)((uintptr_t)piVar4 + 0x80);  // piVar4 + 0x20 as int* => byte offset 0x80
                uintptr_t cellOffset = ((uintptr_t)iVar12 * (uintptr_t)iVar10map + (uintptr_t)iVar21pos) * 0x30;
                local_res20 = *(uintptr_t*)(mapDataBase + 8 + cellOffset);

                if (local_res20 != 0) {
                    uintptr_t familyIBP2[2] = {0};
                    ActRm_FindComponent_223c0(local_res20, familyIBP2);

                    if (*(uintptr_t*)familyIBP2 != 0) {
                        uintptr_t storageIBP2[4] = {0};  // local_70
                        WorkBase_FindComponent_3050(local_res20, storageIBP2);

                        uintptr_t local_90_val = *(uintptr_t*)citizenIBP;
                        uintptr_t storageWorkMgr = *(uintptr_t*)(gameWorld + 0x118);
                        uint64_t overflowResult = WorkGrid_TryAssignWorkplace_f8620(
                            storageWorkMgr, &local_90_val,
                            *(int*)(*(uintptr_t*)storageIBP2 + 0x28), '\x01');

                        if ((char)overflowResult != '\0') {
                            return 2;
                        }
                    }
                }

                iPvVar20++;
            } while (iPvVar20 < posCount);
        }

        // -------------------------------------------------------
        // Branch C: Final fallback -> random area position, create grazing work
        // -------------------------------------------------------
        uintptr_t areaIBP = 0;
        FloorComponent_FindResData(reinterpret_cast<void*>(*plVar17), &areaIBP);

        void* rng = *(void**)(gameWorld + 0xd8);  // Random*

        short sVar1 = *(short*)(areaIBP + 0x28);
        short sVar2 = *(short*)(areaIBP + 0x2a);
        short sVar3 = *(short*)(areaIBP + 0x2c);

        uint32_t uVar11 = (uint32_t)reinterpret_cast<System::Random*>(rng)->RandTol((int)*(short*)(areaIBP + 0x2e));   // B.3.4b bridge
        uint32_t uVar13 = (uint32_t)reinterpret_cast<System::Random*>(rng)->RandTol((int)sVar3);   // B.3.4b bridge
        int randCell = ((int)sVar2 + uVar11) * *piVar4 + uVar13 + (int)sVar1;

        // Update local_res20 with randCell (low 32 bits)
        local_res20 = (local_res20 & 0xFFFFFFFF00000000ULL) | (uint32_t)randCell;

        int nearCell = WorkGrid_FindCell_9ccf0(piVar4, randCell, uVar23, 0xe1f);
        if (nearCell >= 0) {
            uint64_t canPath = (uint64_t)WorkBase_ValidateCell_a00f0(piVar4, 0xe1f, uVar23, nearCell);
            uint32_t dVar7 = PastureComponent::TypeId();  // DWORD_1401c7ba0

            if ((char)canPath != '\0') {
                float halfOff = (float)(*piVar4 >> 1) - kRdata_140172114;
                Math::Vector3 local_80(
                    (float)((int)local_res20 % *piVar4) - halfOff,
                    (float)((int)local_res20 / *piVar4) - halfOff,
                    0.0f);
                float local_a0_pos[4];  // position buffer
                Reimpl_MapGetTerrainPos(piVar4, local_a0_pos, &local_80.x, '\0', nullptr);

                halfOff = (float)(*piVar4 >> 1) - kRdata_140172114;
                Math::Vector3 local_70_pos(
                    (float)(nearCell % *piVar4) - halfOff,
                    (float)(nearCell / *piVar4) - halfOff,
                    0.0f);
                // Retail 0x1400F2370: this local is a Vector3 (12 bytes) that GetTerrainPos
                // fills; the port had it as float[2], 4 bytes short -- a stack overwrite.
                Math::Vector3 local_50;  // dest position
                Reimpl_MapGetTerrainPos(piVar4, &local_50.x, &local_70_pos.x, '\0', nullptr);

                // Create grazing work action (FUN_1400bb0a0)
                local_res20 = *plVar17;
                uintptr_t local_90_val = *(uintptr_t*)local_res10;
                WorkAct_GrazingWorkCtor_bb0a0(
                    &local_90_val,          // citizenIBP value
                    (int)selfQ[4],          // priority
                    &local_res20,           // work entity IBP
                    0,                      // param4
                    &local_50.x,            // dest position
                    local_a0_pos,           // source position
                    dVar7,                  // type ID (DWORD_1401c7ba0)
                    0,                      // param8
                    0);                     // param9

                // Assign work to citizen
                int priority = (int)selfQ[4];
                uintptr_t citizenEnt = *(uintptr_t*)local_res10;

                // ⚠ Retail saves the citizen IBP pointer (pIVar15) BEFORE the
                // vtable+0x50 call: slot 10 writes its result (the workplace's
                // profession key, wpc+0x48) THROUGH the out param — i.e. it
                // OVERWRITES local_res10. The register call below must use the
                // SAVED pointer; passing the clobbered local made the BST key
                // *(Profession*) = g_Prof_Vtable, and the next Slot9 validate
                // crashed IsA on it (organic herder, first completed cycle).
                void* savedCitizenIBP = local_res10;

                // Call vtable slot +0x50 (GetWorkEntity — returns the profession key)
                uintptr_t* wResult = static_cast<uintptr_t*>(GetTarget(&local_res10));
                uintptr_t workEnt = *wResult;

                *(uint32_t*)(citizenEnt + 0x54) |= 4;
                *(int32_t*)(citizenEnt + 0x30) = priority;
                *(uint32_t*)(citizenEnt + 0xb0) = 0;

                // Profession match: slot 10's result vs citizen+0x1F0 (Profession*).
                if (workEnt == *(uintptr_t*)(citizenEnt + 0x1f0) && priority >= 0) {
                    *(uint32_t*)(citizenEnt + 0x88) |= 0x20000;
                }

                // Register work with CitizenAI (CitizenAI_impl_1a30):
                // (tree root = self+0x48, outResult = &local_res10, key = citizen IBP)
                WorkBST_RegisterWork_1a30(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), &local_res10, savedCitizenIBP);
                return 2;
            }
        }
        return 0;
    }

    // Worker count at or below max but resource at capacity -> return 1
    return 1;
}


// ============================================================
// PastureWork::Slot6 -- NOT HOOKABLE (RIP-relative CALL)
// Thunk: sub rsp,0x28; add rcx,0x48; call CitizenAI_impl_5520; xor al,al; add rsp,0x28; ret
// ============================================================
int PastureWork::OnComplete(void* param2, void* /*arg2*/, int /*arg3*/) {
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);
    return false;
}


// ============================================================
// PastureWork::Slot7 (0x1400F22C0) -- Tree insertion with validation
// Unlike other Work classes, this does more than BSTFindRemove:
// 1. Insert citizen into BST at self+0x48
// 2. If param3 entity is non-null:
//    a. Check if it's derived from ComponentEntity
//    b. Find NatRes component
//    c. Clear "flagged for removal" bit (0x100000) in entity flags at +0x20
//    d. If flag changed and entity has parent (+0x10), call UpdateParentEntity
// ============================================================
int PastureWork::OnCancel(void* param2, void* param3, int /*arg3*/) {
    // Insert citizen into BST
    WorkBST_BSTFindRemove_25520(reinterpret_cast<uintptr_t*>((void*)((uintptr_t)this + 0x48)), param2);

    // Check if param3 entity is valid
    if (*(uintptr_t*)param3 == 0) {
        return 0;
    }

    uintptr_t entity = *(uintptr_t*)param3;

    // Check if derived from ComponentEntity
    if (!Object::IsDerivedFromNullSafe((void*)entity, AnimalDescription::TypeId())) {
        return 0;
    }

    // Find NatRes component
    uintptr_t natResIBP[2] = {0};
    ActHunt_FindComponent_5500(entity, natResIBP);

    if (*(uintptr_t*)natResIBP == 0) {
        return 0;
    }

    // Clear "flagged for removal" bit (0x100000) in entity flags at +0x20
    uintptr_t natResObj = *(uintptr_t*)natResIBP;
    uint32_t oldFlags = *(uint32_t*)(natResObj + 0x20);
    uint32_t newFlags = oldFlags & ~0x100000u;

    if (newFlags != oldFlags) {
        *(uint32_t*)(natResObj + 0x20) = newFlags;

        // If entity has parent, update it
        uintptr_t parentEntity = *(uintptr_t*)(natResObj + 0x10);
        if (parentEntity != 0) {
            ActHunt_NotifyEntityChanged_c3470(reinterpret_cast<void*>(parentEntity));
        }
    }
    return 0;
}


// ============================================================
// PastureWork::Slot8 / CancelAll (0x1400F2B90)
// Identical to HuntWork::Slot8 -- loops tree, cancels each.
// ============================================================
int PastureWork::Cleanup() {
    uintptr_t selfAddr = (uintptr_t)this;
    int count = *(int*)(selfAddr + 0x50);

    while (count > 0) {
        void* node = *(void**)(selfAddr + 0x48);
        void* leftmost = node;
        while (node != nullptr) {
            leftmost = node;
            node = *(void**)((uintptr_t)node + 0x10);
        }

        uintptr_t citizenEntity = *(uintptr_t*)leftmost;

        WorkBase_TaskListCleanup_c5400((void*)(*(uintptr_t*)(citizenEntity + 0x278) + 0x48));

        count = *(int*)(selfAddr + 0x50);
    }
    return 0;
}


// ============================================================
// PastureWork::Slot9 / RemoveInvalid (0x1400F2090)
// Identical to HuntWork::Slot9 -- BST traversal, validates, removes.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP).
// ============================================================
int PastureWork::Validate() {
    uintptr_t selfAddr = (uintptr_t)this;
    void** bstRootPtr = (void**)((uintptr_t)this + 0x48);

    void* root = *bstRootPtr;
    uintptr_t* removeArr = nullptr;
    uint32_t removeCount = 0;
    uint32_t removeCapacity = 0;

    if (root == nullptr) {
        return true;
    }

    // Find leftmost node
    void* node = root;
    while (*(void**)((uintptr_t)node + 0x10) != nullptr) {
        node = *(void**)((uintptr_t)node + 0x10);
    }

    // In-order traversal
    while (node != nullptr) {
        uintptr_t entityIBP = *(uintptr_t*)node;

        bool stillValid = WorkBase_ValidateTask_f7100(reinterpret_cast<void*>(selfAddr), nullptr, (void*)&entityIBP, nullptr);

        if (!stillValid) {
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
// PastureWork::Slot10 / GetWorkEntity (0x1400F2C00)
// Identical to HuntWork::Slot10.
// ============================================================
void* PastureWork::GetTarget(void* outPtr) {
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
// PastureWork::GetPriority [11] 0x140029860 (shared by 13 tables) / HasWorkers.
// Returns (this->_field50 > 0).
// ============================================================
int PastureWork::GetPriority() {
    // Retail 0x140029860 is `CMP [RCX+0x50],EAX; SETG` — a SIGNED compare.
    return static_cast<int32_t>(_field50) > 0 ? 1 : 0;
}




// ============================================================
// PastureWork::Serialize (0x1400F2C60)
// Work::Serialize + Serializer(version=0) + BST serialize.
// Identical to HuntWork::Serialize (no extra int field).
// ============================================================
void PastureWork::Persist(void* stream) {
    uintptr_t selfAddr = (uintptr_t)this;

    Work::Persist(stream);   // 0x1400f7400, a direct call in retail — QUALIFIED here

    alignas(8) uint8_t serializer1[24] = {0};
    ::new (static_cast<void*>(serializer1)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(serializer1)->~Serializer();   // B.3.4 bridge

    alignas(8) uint8_t serializer2[24] = {0};
    ::new (static_cast<void*>(serializer2)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 0);   // B.3.4 bridge

    WorkBST_BST_Serialize_296b0(reinterpret_cast<uintptr_t*>((void*)(selfAddr + 0x48)), serializer2);

    reinterpret_cast<IO::Serializer*>(serializer2)->~Serializer();   // B.3.4 bridge
}


// ============================================================
// PastureWork::DtorBody (0x1400F2CE0)
// Identical pattern to HuntWork, uses vtable_PastureWork.
// ============================================================
// PastureWork dtor (retail 0x1400f2ce0, called by slot 0 0x14002bf70). The stamps and the `~Internal` call are the compiler's chain.
PastureWork::~PastureWork() {
    uintptr_t selfAddr = (uintptr_t)this;

    void* bstRoot = *(void**)(selfAddr + 0x48);
    if (bstRoot != nullptr) {
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x10));
        WorkBase_TreeNodeFree_14c30((void*)(selfAddr + 0x48), *(void**)((uintptr_t)bstRoot + 0x18));
        System::MemoryAllocator::MemFree(bstRoot);
    }

    *(void**)(selfAddr + 0x48) = nullptr;
    *(uint32_t*)(selfAddr + 0x50) = 0;

}




// Ctor — the bytes the tree's factory built (Work_GenericCtor): the Work base init,
// the leaf fields zeroed. The three stamps are the compiler's.
PastureWork::PastureWork() : AutoPickupWork(), _field48(nullptr), _field50(0) {
}

// ============================================================
// PastureWork::PastureWork (0x1400f2d80) — the retail ctor Slot14 cmd-2 calls
// when it turns the herder on. Base = Work_Ctor25 (0x1400f7250 pattern):
//   +0x10 = *entityIBP (raw 8-byte copy of the pasture entity ref)
//   +0x18 = WorkComponent ref (finder 0x140003050 on the same entity)
//   +0x20 = -1, +0x28 = -1 OR the Area component's center cell:
//           ((h>>1)+y)*mapDim + (w>>1)+x  from area +0x28/+0x2a/+0x2c/+0x2e
//   +0x24/+0x2c/+0x30/+0x38/+0x40 = 0
// PastureWork tail: vtable, +0x48 BST root = 0, +0x50 count = 0.
// The tree's port starts from the default shape (Work(): the zero fields and the two
// -1s) and adds the entity-dependent fields on top; that is kept.
// ============================================================
PastureWork::PastureWork(void* entityIBP) : AutoPickupWork(), _field48(nullptr), _field50(0) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    *reinterpret_cast<uint64_t*>(s + 0x10) = *reinterpret_cast<uint64_t*>(entityIBP);
    if (*reinterpret_cast<uint64_t*>(s + 0x10) != 0 &&
        GameUpdateLoop::_singleton != nullptr) {
        uintptr_t areaIBP[2] = {0};
        FloorComponent_FindResData(reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(s + 0x10)), areaIBP);
        if (areaIBP[0] != 0) {
            uintptr_t a = areaIBP[0];
            uintptr_t gw = (uintptr_t)GameUpdateLoop::_singleton;
            int mapDim = **reinterpret_cast<int**>(gw + 0xf8);
            *reinterpret_cast<int32_t*>(s + 0x28) =
                (((int)*reinterpret_cast<int16_t*>(a + 0x2e) >> 1) +
                  (int)*reinterpret_cast<int16_t*>(a + 0x2a)) * mapDim +
                ((int)*reinterpret_cast<int16_t*>(a + 0x2c) >> 1) +
                 (int)*reinterpret_cast<int16_t*>(a + 0x28);
        }
        uintptr_t wcIBP[2] = {0};
        WorkBase_FindComponent_3050(*reinterpret_cast<uintptr_t*>(entityIBP), wcIBP);  // 0x140003050 = WorkComponent finder
        *reinterpret_cast<uint64_t*>(s + 0x18) = wcIBP[0];
    }
}

// Convenience for PastureComponent Slot14 (pasturecomponent_states.cpp, block MI-B):
// MemAlloc(0x58,8) + the faithful ctor. `entityRefSlot` points at an 8-byte entity ref
// (the pasture's owner entity).
extern "C" void* PastureWork_CreateForEntity(void* entityRefSlot) {
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureWork), 8);
    return p ? static_cast<Object::Base*>(new (p) PastureWork(entityRefSlot)) : nullptr;
}


// ============================================================
// Reflection, factory and the seam (ENDGAME B.3.8, B38_RULES D4/D5)
// ============================================================
// Before: a hand-built 13-slot `g_PastureWork_Vtable` (slot 1 null, slot 2 a lazily
// cached id) and REGISTER_TYPE over PastureWork_Factory / _CreateInPlace (both
// Work_GenericCtor), plus three vtable-pointer statics for the dtor's stamps.
// Nothing outside this block reads 0x1401617a0 back, so that row goes with the table.

// create 0x14002bf00: MemAlloc(sizeof, 8), then the ctor.
void* PastureWork::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureWork), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PastureWork());
}
// Retail's createInPlace is the imported Internal::CreateInPlaceObject (returns null);
// the tree's in-place entry built the object, and that is kept (B8-13).
void* PastureWork::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PastureWork());
}


#include "Object/factory_registration.h"
#include "address_registry.h"

DEFINE_EXE_TYPE(PastureWork, "AutoPickupWork")
