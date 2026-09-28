// code/Game/Action/action_pf_roots_drop.cpp
// Wave 3 (Action) — DropOff / RemoveAnimal-ctor ROOT callers.
//
// Faithful 1:1 ports of the 4 root-caller exe helpers in the raw-material drop-off /
// RemoveAnimal-action closure. Each is transcribed literally from the Ghidra decompile,
// cross-checked against disassembly for the call-site arg ABI. Do NOT "improve" — none
// of this is exercised on the autostart path, so a wrong port is undetectable latent
// corruption.
#include "Action/exe_bodies.h"
//
// Registration: RegisterPfRootsDropBatch() in standalone/reg_pf_roots_drop.cpp.
//
// Sub-callees (called by name, B3R — bodies NOT duplicated):
//   0x14009bd40 CreateRawMaterial_FindPlacementCell — ported by another agent THIS wave
//   0x1400c3720 ComponentEntity_CtorFromDescription (CopyCtor)
//   0x1400223c0 FindComponent (rawmat batch)
//   0x140129760 PositionEntity / helper_9760 (rawmat batch)
//   0x1400c48a0 EntityList_RegisterEntity
//   0x140006270 Entity_FindComponent_Storage (work_base)
//   0x1400012d0 Entity_FindComponent_Family (action_move)
//   0x14012d150 StorageComponent::SubtractItem (rawmat)
//   0x1401167c0 storage count-delta (action_pf_ctors)
//   0x140116760 storage drop-off (action_pf_ctors)
//   0x14012cd80 StorageComponent::CanAdd (Pf_StorageCanAccept_cd80)
//   0x14012d000 StorageComponent::AddItem (rawmat)
//   0x14008d590 FindStorageForResource (Pf_PickBestDropTarget_8d590)
//   0x1400c9170 MoveTo_Action_Ctor (action_pf_ctors)
//   0x1400c9720 PerformWork_Action_Ctor7 (action_pf_ctors)
//   0x1400c5da0 CompleteTask_Action_CopyCtor (action_pf_ctors)
//   0x1400c71a0 CreateRawMaterial_Action_CopyCtor (action_pf_ctors)
//   0x1400cb1d0 SendComponentMessage_Action_CopyCtor (action_pf_ctors)
//   0x1400c54e0 action-stack push (action_pf_ctors) — bbea0/c7620 inline this; call helper
//   0x1400c5400 TaskListCleanup (work_base)
//   0x140107260 set-flag/refresh/Update == RemoveAnimal_helper_7260 (action_pf_ctors)
// Engine members: Object::InternalBasePtr, System::MemoryAllocator::MemAlloc,
//   RtSystem_Time_Ctor, System::Random::RandTol.
//
// bbea0 inlines the RemoveAnimal ctor; ENDGAME B.3.8 ACT: `new (p) RemoveAnimal(animal)`.

#include "Action/RemoveAnimal.h"
#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Work/exe_bodies.h"              // WorkBase_FindComponent_6270, WorkBase_TaskListCleanup_c5400
#include "Entity/componententity_funcs.h" // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Game/exe_constants.h"           // kRdata_*
#include "system/MemoryAllocator.h"       // System::MemoryAllocator::MemAlloc
#include <cstring>
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include <cstdint>
#include <new>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers

namespace {

// .rdata constants (Game/exe_constants.h): 0x140172204 (c78c0 RandTol(2) scale),
// 0x140172280 (MoveTo speed, c7620 / bbea0), 0x140172194 (PerformWork Time seconds, bbea0),
// 0x1401722a8 (PerformWork animIndex float, bbea0; a bit pattern), 0x14017214c (g_fUVScale,
// the CreateRawMaterial CopyCtor float arg).
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

// 0x1400032e0 (array-grow) was resolved here but never called anywhere in this file
// -- dead resolved-pointer machinery, removed rather than converted (B.3.2 step 2).

// MemAlloc shim matching the original `System::MemoryAllocator::MemAlloc(size, align)`.
static void* MemAlloc(int64_t size, int64_t align) {
    return System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(size), static_cast<int>(align));
}
} // namespace

// ============================================================
// FUN_1400c78c0 — spawn a raw-material entity on a found map cell, then subtract the
// dropped amount from the source storage.
// param_1 = action self (entity IBP at +0x30); param_2 = source ComponentEntity ptr;
// param_3 = amount. If amount != 0: pick a placement cell near the entity's map position
// via 9bd40; on success copy-construct an entity, wire its component (223c0), position it
// (9760) with a small random yaw, set +0x28=amount/+0x30=0, register it in the world
// entity list (c48a0). Finally subtract `amount` from the source storage (12d150).
// ============================================================
void Pf_SpawnRawMaterialOnCell_78c0(int64_t param_1, int64_t param_2, int param_3) {
    void* gworld = GameUpdateLoop::_singleton;
    if (param_3 == 0) return;

    int64_t local_res20 = 0;
    int64_t local_48 = 0;
    void* local_40 = nullptr;
    uint32_t local_38 = 0;

    int* piVar2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gworld) + 0xf8);
    new (&local_res20) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    int64_t lVar3 = *reinterpret_cast<int64_t*>(param_1 + 0x30);
    lVar3 = *reinterpret_cast<int64_t*>(lVar3 + 0x278);
    lVar3 = *reinterpret_cast<int64_t*>(lVar3 + 0x68);
    local_38 = *reinterpret_cast<uint32_t*>(lVar3 + 0x160);
    int iVar1 = *piVar2;
    int iVar6 = static_cast<int>(*reinterpret_cast<float*>(lVar3 + 0x158) + static_cast<float>(iVar1 >> 1));
    int iVar5 = static_cast<int>(*reinterpret_cast<float*>(lVar3 + 0x15c) + static_cast<float>(iVar1 >> 1));
    void* ptr = nullptr;
    if (iVar6 < 0) iVar6 = 0;
    if (iVar1 <= iVar6) iVar6 = iVar1 - 1;
    if (iVar5 < 0) iVar5 = 0;
    if (iVar1 <= iVar5) iVar5 = iVar1 - 1;
    int cell = Pf_FindPlacementCell_bd40(piVar2, iVar1 * iVar5 + iVar6, '\0', &local_res20, 0);   // 0x14009bd40
    if (-1 < cell) {
        local_40 = MemAlloc(0x48, 8);
        if (local_40 != nullptr) {
            ptr = ComponentEntity_CtorFromDescription(static_cast<ComponentEntity*>(local_40), reinterpret_cast<void*>(param_2));   // 0x1400c3720
        }
        new (&local_48) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ptr));   // B.3.3 bridge
        ActRm_FindComponent_223c0(static_cast<uintptr_t>(local_48), &local_res20);   // 0x1400223c0
        void* random = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gworld) + 0xd8);
        int64_t entAdj = local_res20;
        int randv = random ? reinterpret_cast<System::Random*>(random)->RandTol(2) : 0;   // B.3.4b bridge
        ActRm_PositionEntity_9760(entAdj, cell,   // 0x140129760
                                  static_cast<float>(randv) * kRdata_140172204);
        *reinterpret_cast<int*>(local_res20 + 0x28) = param_3;
        *reinterpret_cast<uint64_t*>(local_res20 + 0x30) = 0;
        local_40 = reinterpret_cast<void*>(local_48);
        EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gworld) + 0xe8),
                           &local_40, *reinterpret_cast<int*>(param_2 + 0x20));
    }
    lVar3 = *reinterpret_cast<int64_t*>(param_1 + 0x30);
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(lVar3 + 0x10), &local_res20);   // 0x140006270
    ActRm_StorageSubtract_d150(static_cast<uintptr_t>(local_res20), param_2, param_3);   // 0x14012d150
    (void)local_38;
}

// ============================================================
// FUN_1400c7570 — iterate the source storage's resource stacks; for each stack with a
// positive count, spill it onto a placed entity via 78c0. Re-reads the storage count each
// iteration because 78c0 mutates it (12d150 compacts the array). param_1 = action self.
// ============================================================
void Pf_SpillAllStorage_7570(int64_t param_1) {
    int64_t local_res8 = 0;
    int64_t local_res10 = 0;
    local_res10 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + 0x10);
    WorkBase_FindComponent_6270(static_cast<uintptr_t>(local_res10), &local_res8);   // 0x140006270
    int iVar1 = *reinterpret_cast<int*>(local_res8 + 0x30);
    while (0 < iVar1) {
        int64_t* arr = *reinterpret_cast<int64_t**>(local_res8 + 0x28);
        iVar1 = static_cast<int>(arr[1]);  // first stack's count
        if (0 < iVar1) {
            Pf_SpawnRawMaterialOnCell_78c0(param_1, *arr, iVar1);  // *arr = first stack's resType
        }
        iVar1 = *reinterpret_cast<int*>(local_res8 + 0x30);
    }
}

// ============================================================
// FUN_1400c7620 — DropOffRawMaterial action executor (deposit-or-move branch).
// param_1 = action self; param_2 = action-stack list to push onto; param_3 = dest storage
// IBP; param_4 = resource type. Computes a deposit amount = min(self.+0x24 cap, available
// from 1167c0); if it fits (cd80 CanAdd) deposit it (d000 AddItem + 116760 source subtract)
// and return 1. Otherwise, if self.+0x20 flag set, search for a storage destination cell
// (8d590); if none, spill via 78c0 and return 2; else stash the found cell at self.+0x38,
// build a MoveTo action toward it and push it (c54e0). Returns 1.
// ============================================================
uint64_t Pf_DropOffExec_7620(int64_t param_1, int64_t* param_2, void* param_3, uint64_t param_4) {
    int iVar5 = Pf_StorageCountDelta_1167c0(*reinterpret_cast<int64_t*>(param_1 + 0x30), param_4);   // 0x1401167c0
    int iVar8 = *reinterpret_cast<int*>(param_1 + 0x24);
    int iVar9 = iVar5;
    if ((-1 < iVar8) && (iVar9 = iVar8, iVar5 < iVar8)) {
        iVar9 = iVar5;
    }
    if (0 < iVar9) {
        uint32_t uVar6 = Pf_StorageCanAccept_cd80(*reinterpret_cast<int64_t*>(param_3), static_cast<int64_t>(param_4), iVar9);   // 0x14012cd80
        if (static_cast<char>(uVar6) != '\0') {
            ActRm_StorageAdd_d000(*reinterpret_cast<uintptr_t*>(param_3), static_cast<int64_t>(param_4), iVar9);   // 0x14012d000
            Pf_StorageDropOff_116760(*reinterpret_cast<int64_t*>(param_1 + 0x30), static_cast<int64_t>(param_4), iVar9);   // 0x140116760
            return 1;
        }
    }
    if (*reinterpret_cast<char*>(param_1 + 0x20) != '\0') {
        void* gworld = GameUpdateLoop::_singleton;
        int* piVar2 = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(gworld) + 0xf8);
        void** pplVar3 = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(gworld) + 0xf0);
        float local_50[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        void* ptr = nullptr;
        int64_t local_res8 = 0;
        int64_t local_70[2] = {0, 0};
        uint8_t local_60[8] = {0,0,0,0,0,0,0,0};  // InternalBasePtr local_60 (Family find out)
        uint32_t local_58 = 0;

        int64_t lVar4 = *reinterpret_cast<int64_t*>(param_1 + 0x30);
        lVar4 = *reinterpret_cast<int64_t*>(lVar4 + 0x278);
        lVar4 = *reinterpret_cast<int64_t*>(lVar4 + 0x68);
        local_58 = *reinterpret_cast<uint32_t*>(lVar4 + 0x160);
        int iVar1 = *piVar2;
        iVar8 = static_cast<int>(*reinterpret_cast<float*>(lVar4 + 0x158) + static_cast<float>(iVar1 >> 1));
        iVar5 = static_cast<int>(*reinterpret_cast<float*>(lVar4 + 0x15c) + static_cast<float>(iVar1 >> 1));
        if (iVar8 < 0) iVar8 = 0;
        if (iVar1 <= iVar8) iVar8 = iVar1 - 1;
        if (iVar5 < 0) iVar5 = 0;
        if (iVar1 <= iVar5) iVar5 = iVar1 - 1;
        Pf_PickBestDropTarget_8d590(reinterpret_cast<int64_t**>(pplVar3), local_70,   // 0x14008d590
                                    static_cast<uint32_t>(iVar1 * iVar5 + iVar8), param_4, iVar9, '\x01',
                                    reinterpret_cast<uint32_t*>(local_50), &local_res8);
        if (local_70[0] == 0) {
            Pf_SpawnRawMaterialOnCell_78c0(param_1, param_4, iVar9);
            return 2;
        }
        *reinterpret_cast<int64_t*>(param_1 + 0x38) = local_70[0];
        void* self = MemAlloc(0x480, 8);
        if (self != nullptr) {
            local_res8 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x30) + 0x10);
            void* pIVar7 = ActMv_FindComponentFamily_12d0(static_cast<uintptr_t>(local_res8), local_60);   // 0x1400012d0
            ptr = Pf_MoveToCtor_9170(self, pIVar7, /*Slow*/1, 0, local_50, 0xe1f, 0,   // 0x1400c9170
                                     kRdata_140172280);
        }
        new (local_70) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ptr));   // B.3.3 bridge
        Pf_ActStackPush_54e0(param_2, local_70);
        (void)local_58;
    }
    return 1;
}

// ============================================================
// FUN_1400bbea0 — RemoveAnimal_Action Ctor. Builds a child-action sub-tree on the action
// list at (entity.+0x278).+0x48. param_1 = entity IBP; param_2 = a CompleteTask arg;
// param_3 = MoveTo destination vec3; param_4 = CreateRawMaterial flag arg; param_5 = a
// second-entity IBP; param_6 = a description-list (count +0x50, array +0x48); param_7 = a
// gate flag (low byte). If gated, pushes a SendComponentMessage + CompleteTask pair. Then
// for each entry in param_6 pushes a CreateRawMaterial action; appends a RemoveAnimal
// (raw 0x28-byte action), a PerformWork, and a MoveTo; finally calls 107260 on param_5.
// The original inlined the action-stack push (c54e0) at each append site — we call the
// registered push helper (byte-identical behaviour).
// ============================================================
void Pf_RemoveAnimalCtor_bbea0(void* param_1, uint32_t param_2, uint32_t* param_3, uint32_t param_4,
                               void* param_5, int64_t param_6, uint32_t param_7) {

    int64_t local_78 = 0;
    uint64_t local_70 = 0;
    void* local_68 = nullptr;
    int64_t local_res8_v = 0;       // longlong* local_res8 (IBP out / scratch)
    uint8_t local_58[32] = {0};     // Internal* local_58[4] / Time scratch

    int64_t lVar3 = *reinterpret_cast<int64_t*>(param_1);
    lVar3 = *reinterpret_cast<int64_t*>(lVar3 + 0x278);
    int64_t* plVar18 = reinterpret_cast<int64_t*>(lVar3 + 0x48);  // action-stack list

    if (static_cast<char>(param_7) != '\0') {
        WorkBase_TaskListCleanup_c5400(plVar18);   // 0x1400c5400
        // --- SendComponentMessage child action ---
        void* pIVar7 = MemAlloc(0x40, 8);
        void* pIVar8 = nullptr;
        local_68 = pIVar7;
        if (pIVar7 != nullptr) {
            new (&local_res8_v) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
            local_78 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            pIVar8 = Pf_SendComponentMessageCopyCtor_b1d0(pIVar7, reinterpret_cast<uint64_t*>(&local_78),   // 0x1400cb1d0
                                                          &msgId, 0, reinterpret_cast<uint64_t*>(&local_res8_v), 0);
        }
        new (&local_70) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(pIVar8));   // B.3.3 bridge
        if (*reinterpret_cast<int64_t*>(lVar3 + 0x58) != 0) {
            reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(lVar3 + 0x58))->DeletePtr(
                );   // B.3.3 bridge
        }
        *reinterpret_cast<uint64_t*>(lVar3 + 0x58) = local_70;
        // --- CompleteTask child action ---
        void* pIVar8b = MemAlloc(0x38, 8);
        void* pIVar9 = nullptr;
        local_68 = pIVar8b;
        if (pIVar8b != nullptr) {
            local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_5) + 0x10);
            local_70 = *reinterpret_cast<uint64_t*>(param_1);
            pIVar9 = Pf_CompleteTaskCopyCtor_5da0(pIVar8b, param_2,   // 0x1400c5da0
                                                  &local_70, reinterpret_cast<uint64_t*>(&local_res8_v), 0);
        }
        new (&local_78) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(pIVar9));   // B.3.3 bridge
        Pf_ActStackPush_54e0(plVar18, &local_78);
    }

    // --- find Storage on entity (+0x10) for the CreateRawMaterial loop ---
    local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
    void* pStorIBP = WorkBase_FindComponent_6270(static_cast<uintptr_t>(local_res8_v), &local_68);   // 0x140006270
    float fVar6 = kRdata_14017214c;  // g_fUVScale, 0x14017214c
    // StorageComponent+0x38 is NOT a dead load: retail keeps it in R12D (1400bc048) and
    // passes it as arg7 of EVERY CreateRawMaterial_Action_CopyCtor below (stored to
    // RSP+0x30 at 1400bc0d8). R12D is only zeroed at 1400bc294, AFTER the loop — which is
    // where the old "arg7 = 0 (R12D)" comment came from. Read it here, before local_68 is
    // recycled as the child-action push IBP.
    uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(pStorIBP) + 0x38);

    // --- CreateRawMaterial action per description entry ---
    if (0 < *reinterpret_cast<int*>(param_6 + 0x50)) {
        int iVar20 = 0;
        do {
            void* pIVar8 = MemAlloc(0x58, 8);
            void* pIVar9 = nullptr;
            if (pIVar8 != nullptr) {
                // Disasm-verified arg setup (Ghidra's bbea0 decompile aliased the integer
                // args; the disassembly is ground truth):
                //   arg2 = *(*(param_6+0x48)+iVar20*8)   (description entry)
                //   arg3 = fVar6 (XMM2/g_fUVScale float, stored at ctor +0x4c)
                //   arg4 = &local_78  (local_78 = *param_1)
                //   arg5 = &local_70  (local_70 = (*param_1)+0x248)
                //   arg6 = <unwritten by caller — uninitialized, ctor never reads it>
                //   arg7 = uVar1      (R12D = StorageComponent+0x38, loaded at 1400bc048)
                //   arg8 = param_4    (R13D = bbea0's 4th arg)
                //   arg9 = 1
                //   arg10 = &local_res8_v  (local_res8_v = (*param_1)+0x248, a 2nd copy)
                //   arg11 = 0xc
                local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x248);
                local_78 = *reinterpret_cast<int64_t*>(param_1);
                local_70 = *reinterpret_cast<uint64_t*>(local_78 + 0x248);
                pIVar9 = Pf_CreateRawMaterialCopyCtor_71a0(   // 0x1400c71a0
                    pIVar8,
                    *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_6 + 0x48) + static_cast<int64_t>(iVar20) * 8),
                    fVar6,   // float arg3 -> XMM2 (ctor MOVSS to +0x4c); pass the value, not its bits
                    reinterpret_cast<uint64_t*>(&local_78),
                    &local_70,
                    0 /*arg6 uninitialized — unused by ctor*/,
                    uVar1 /*arg7 = R12D = StorageComponent+0x38*/, param_4 /*arg8 = R13D*/, 1,
                    reinterpret_cast<uint64_t*>(&local_res8_v), 0xc);
            }
            new (&local_68) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(pIVar9));   // B.3.3 bridge
            Pf_ActStackPush_54e0(plVar18, &local_68);
            iVar20 = iVar20 + 1;
        } while (iVar20 < *reinterpret_cast<int*>(param_6 + 0x50));
    }

    // --- RemoveAnimal action (raw 0x28-byte, no dedicated ctor) ---
    {
        void* pIVar9 = MemAlloc(0x28, 8);
        if (pIVar9 != nullptr) {
            uint64_t uVar4 = *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(param_5) + 0x10);
            // ENDGAME B.3.8 ACT: the real ctor (RemoveAnimal.h) — retail inlines it here.
            new (pIVar9) RemoveAnimal(uVar4);
        }
        new (&local_res8_v) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(pIVar9));   // B.3.3 bridge
        Pf_ActStackPush_54e0(plVar18, &local_res8_v);
    }

    // --- PerformWork action ---
    {
        void* pIVar13 = MemAlloc(0x48, 8);
        void* pvVar11 = nullptr;
        if (pIVar13 != nullptr) {
            local_res8_v = *reinterpret_cast<int64_t*>(param_1);
            uint64_t timeStore = 0;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(local_58), kRdata_140172194);   // 0x140172194
            timeStore = *reinterpret_cast<uint64_t*>(local_58);
            pvVar11 = Pf_PerformWorkCtor7_9720(pIVar13, &local_res8_v,   // 0x1400c9720
                                               RdataF(kRdata_1401722a8),   // 0x1401722a8
                                               0xb /*Fast|ShowGroup*/, static_cast<int64_t>(timeStore),
                                               1, 2);
        }
        new (&local_68) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(pvVar11));   // B.3.3 bridge
        Pf_ActStackPush_54e0(plVar18, &local_68);
    }

    // --- MoveTo action toward param_3 ---
    {
        void* pIVar13 = MemAlloc(0x480, 8);
        void* pvVar11 = nullptr;
        if (pIVar13 != nullptr) {
            local_res8_v = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1) + 0x10);
            void* pIVar15 = ActMv_FindComponentFamily_12d0(static_cast<uintptr_t>(local_res8_v), local_58);   // 0x1400012d0
            pvVar11 = Pf_MoveToCtor_9170(pIVar13, pIVar15, /*Slow*/1, 0,   // 0x1400c9170
                                         reinterpret_cast<float*>(param_3), 0xe1f, 0,
                                         kRdata_140172280);
        }
        new (&local_68) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(pvVar11));   // B.3.3 bridge
        Pf_ActStackPush_54e0(plVar18, &local_68);
    }

    // --- finalise on param_5 (the animal's NaturalResourceComponent) ---
    // MUST be param_5, NOT pStorIBP. WorkBase_FindComponent_6270 returns its OUT-IBP argument
    // (work_base_reimpl.cpp:57 `return retIBP`), so pStorIBP == &local_68 — and local_68
    // is recycled as the push-IBP for every child action above (lines 482 / 520 / 537).
    // By this point it holds the MoveTo action, whose +0x10 the Action base ctor zeroes,
    // so 0x140107260 handed a NULL entity to Entity_FindComponent_Family and faulted on
    // `entity + 0x38` — the first time a pasture ever culled an animal. Worse than the
    // crash: we had already OR'd the 0x100000 removal flag into the MoveTo action's +0x20,
    // corrupting a queued action.
    // Retail loads param_5's home slot into R14 at 1400bc267, BEFORE that stack slot is
    // recycled as a loop cursor at 1400bc71f, and calls 0x140107260 with *R14 at
    // 1400bc755. Ghidra's decompile prints the storage-find result here because it
    // reassigns its alias to param_5 mid-function — the same arg-aliasing trap this
    // function's own comment already warns about for the CreateRawMaterial args.
    // Semantic proof it is the animal: 0x140107260 sets bit 0x100000 at param_1+0x20, and
    // pasturework.cpp:517 tests exactly that bit on the NatRes component to skip animals
    // already flagged for removal. A null guard here would be WRONG — this is the only
    // writer of that flag, so guarding would leave the culled animal unflagged and the
    // pasture would re-select it every tick: a silent livelock instead of a crash.
    Pf_SetFlagRefreshUpdate_107260(*reinterpret_cast<int64_t**>(param_5));   // 0x140107260
    (void)plVar18; (void)param_3;
}
