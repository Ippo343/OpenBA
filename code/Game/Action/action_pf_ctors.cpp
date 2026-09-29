// code/Game/Action/action_pf_ctors.cpp
// Action-stack and entity/storage leaf sub-helpers (pathfinding/action closure batch).
//
// Faithful 1:1 ports of small exe helpers referenced from the Action closure.
// Each is transcribed literally from the Ghidra decompile (cross-checked against disassembly
// for intricate index/field math). Do NOT "improve" — none of this is exercised on the
// autostart path, so a wrong port is undetectable latent corruption.
//
// B.4 R0-EXE: the five action ctors that lived here and their by-address doors moved,
// text unchanged, to the classes' files: CompleteTask (c5da0) completetask.cpp,
// CreateRawMaterial (c71a0) createrawmaterial.cpp, MoveTo (c9170, with the file-local
// RdataF it alone used) moveto.cpp, PerformWork (c9720) performwork.cpp,
// SendComponentMessage (cb1d0) sendcomponentmessage.cpp. What stays has no settled
// class (B.4 inventory A: unattributed, or low confidence from the retail caller's class).
//
// Registration: RegisterPfCtorsBatch() in standalone/reg_pf_ctors.cpp.
//
// Sub-callees (called by name, B3R — body NOT duplicated):
//   0x1400032e0  array-grow (component_people)               — c54e0
//   0x1400c4f10  action-stack walk/clear (THIS file)         — c54e0
//   0x1400c3470  NotifyEntityChanged (action_hunt)           — 107260
//   0x1400012d0  Entity_FindComponent_Family (action_move)   — 107260
#include "Action/exe_bodies.h"
//   0x1400c5400  TaskListCleanup (work_base)                 — 107260
//   0x140006270  Entity_FindComponent_Storage (work_base)    — 116760 / 1167c0
//   0x14012d150  StorageComponent::SubtractItem (rawmat)     — 116760
//
// Engine helpers: Object::IsDerivedFromNullSafe, RtSystem_Time_Ctor
//   (??0Time@System@@QEAA@M@Z).

#include "Action/Action.h"
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Component/coll_bridge.h"
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Work/exe_bodies.h"          // WorkBase_TaskListCleanup_c5400, WorkBase_FindComponent_6270
#include "Game/exe_constants.h"       // kRdata_*
#include <cstring>
#include <cstdint>
#include <new>
#include <cstdio>
#include <cstdlib>
#include "Component/CharacterDescription.h"   // ENDGAME B.3.8 CB3: CharacterDescription::TypeId()
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CB3: CitizenComponent::TypeId()
#include "Component/InteractDescription.h"
#include "Component/RawMaterialDescription.h"
// For the BANISHED_LIVE_STORAGE eat-message typeid heal in the
// SendComponentMessage copy-ctor (see Pf_SendComponentMessageCopyCtor_b1d0).
#include "Object/factory_registration.h"           // ::Object::GetTypeIdByName

// ============================================================
// FUN_1400c4f10 — action-stack: walk the linked entry list, clear an in-use flag
// and call vtable+0x38 (Exit?) on each entry whose +0x18 byte is set.
// param_1 = action-stack/context passed through to the vcall; param_2 = InternalBasePtr
// holding the current entry. CheckPtr before every field access; advance via [obj+0x10].
// ============================================================
void Pf_ActStackWalkClear_4f10(void* param_1, void* param_2) {
    int64_t lVar1 = *reinterpret_cast<int64_t*>(param_2);
    while (lVar1 != 0) {
        int64_t obj = *reinterpret_cast<int64_t*>(param_2);
        if (*reinterpret_cast<char*>(obj + 0x18) != '\0') {
            int64_t* plVar2 = *reinterpret_cast<int64_t**>(param_2);
            *reinterpret_cast<uint8_t*>(plVar2 + 3) = 0;  // byte at obj+0x18
            reinterpret_cast<Action*>(plVar2)->Exit(param_1);   // B.3.x bridge (slot 7)
        }
        lVar1 = *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_2) + 0x10);
        *reinterpret_cast<int64_t*>(param_2) = lVar1;
    }
}

// ============================================================
// FUN_1400c54e0 — action-stack push: grow backing array if full, clear the slot
// being overwritten (via 4f10), then store the new entry pointer and bump the count.
// param_1 = stack object ([0]=array, [1]=capacity, [3]=count); param_2 = src entry ptr.
// ============================================================
void Pf_ActStackPush_54e0(int64_t* param_1, void* param_2_) {
    uint64_t* param_2 = reinterpret_cast<uint64_t*>(param_2_);
    int iVar1 = static_cast<int>(param_1[1]);
    if (iVar1 == 0) {
        iVar1 = 0x10;
    } else {
        if (static_cast<int>(param_1[3]) + 1 != iVar1) goto LAB_550e;
        iVar1 = iVar1 * 2;
    }
    // B.3.2 bridge: FUN_1400032e0 == Collection::Array<Ptr8>::SetSize (stride 8,
    // zero tail; ImmigrationComp_DynArrayResize in immigrationcomponent.cpp). It only
    // touches bytes [0x00,0x10) at the pointer it is given -- the array header
    // embedded at the front of this action-stack struct -- so this call is
    // unaffected by whatever the surrounding struct keeps at param_1[3] (+0x18).
    GameColl::AsArray<GameColl::Ptr8>(param_1).SetSize(iVar1);
LAB_550e:
    if (-1 < static_cast<int>(param_1[3])) {
        uint64_t local_res8 = *reinterpret_cast<uint64_t*>(
            param_1[0] + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8);
        Pf_ActStackWalkClear_4f10(param_1, &local_res8);
    }
    *reinterpret_cast<int*>(param_1 + 3) = static_cast<int>(param_1[3]) + 1;
    *reinterpret_cast<uint64_t*>(
        param_1[0] + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8) = *param_2;
}

// ============================================================
// FUN_140107260 — set flag bit 0x100000 at +0x20, and if the masked-low-16 bits changed
// notify the entity (c3470) on the IBP at +0x10. Then find the family component on that
// entity; if its +0x60 count+1 > 0, clean its task list (+0x48) and vcall Update(+0x48)
// with Time(0).
// param_1: object whose +0x10 is the entity IBP, +0x20 the flag dword.
// ============================================================
void Pf_SetFlagRefreshUpdate_107260(int64_t* param_1) {
    uint32_t uVar1 = *reinterpret_cast<uint32_t*>(param_1 + 4);  // +0x20
    uint32_t uVar2 = uVar1 | 0x100000;
    *reinterpret_cast<uint32_t*>(param_1 + 4) = uVar2;
    if ((((uVar2 ^ uVar1) & 0xffff) != 0) && (param_1[2] != 0)) {
        ActHunt_NotifyEntityChanged_c3470(reinterpret_cast<void*>(param_1[2]));   // 0x1400c3470
    }
    int64_t local_res8 = 0;
    ActMv_FindComponentFamily_12d0(static_cast<uintptr_t>(param_1[2]), &local_res8);   // 0x1400012d0
    if (0 < *reinterpret_cast<int*>(local_res8 + 0x60) + 1) {
        WorkBase_TaskListCleanup_c5400(reinterpret_cast<void*>(local_res8 + 0x48));   // 0x1400c5400
        uint64_t timeStore = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeStore), 0.0f);
        reinterpret_cast<Component*>(param_1)->Update(&timeStore);   // B.3.x bridge (slot 9)
    }
}

// ============================================================
// FUN_140123070 — recursive sum of a per-entity counter over the entity's child tree.
// obj = [param_1+0x18]; if IsDerivedFrom(obj, DAT_1401c7938) base=obj else 0. Seed value
// = *(int*)(base + (param_2+3)*0x10). Then for each child IBP in [param_1+0x28] (count
// [param_1+0x30], stride 8) recurse on *child and add the result.
// ============================================================
int Pf_RecursiveChildSum_123070(int64_t param_1, int param_2) {
    void* obj = *reinterpret_cast<void**>(param_1 + 0x18);
    bool derived = ::Object::IsDerivedFromNullSafe(obj, InteractDescription::TypeId());
    void* base = derived ? obj : nullptr;
    // [STANDALONE GUARD] base is null when obj is not an InteractDescription OR the
    // type-id global (0x1401c7938) is unfilled/unregistered in standalone. The real
    // game always resolves it; guard so the seed read can't deref null+0x30 (the
    // counter simply contributes 0). Surfaced via the BANISHED_FORCE_WORK harness
    // (Leisure/Work anchor a citizen on an inert workplace building's position comp).
    int iVar3 = base ? *reinterpret_cast<int*>(
        reinterpret_cast<uintptr_t>(base) + (static_cast<int64_t>(param_2) + 3) * 0x10) : 0;
    int iVar5 = 0;
    if (0 < *reinterpret_cast<int*>(param_1 + 0x30)) {
        int64_t lVar4 = 0;
        do {
            int64_t* self = reinterpret_cast<int64_t*>(
                *reinterpret_cast<int64_t*>(param_1 + 0x28) + lVar4);
            int iVar2 = Pf_RecursiveChildSum_123070(*self, param_2);
            iVar5 = iVar5 + 1;
            lVar4 = lVar4 + 8;
            iVar3 = iVar3 + iVar2;
        } while (iVar5 < *reinterpret_cast<int*>(param_1 + 0x30));
    }
    return iVar3;
}

// ============================================================
// FUN_14012c560 — ResourceLimit flag-mask test.
// Walk param_2's component array (ptr [param_2+0x28], count [param_2+0x30], stride 8);
// find the child whose GetTypeId (vtable+0x10) == DWORD_1401c7cb8, IsDerivedFrom-confirm
// it, then return (*(uint*)(param_1+0x3c) & *(uint*)(found+0x5c)) != 0. If no match,
// `found` stays null and +0x5c reads from address 0 — faithful to the original.
// ============================================================
int Pf_ResourceLimitFlagTest_c560(int64_t param_1, int64_t param_2) {
    void* found = nullptr;
    int iVar = 0;
    if (0 < *reinterpret_cast<int*>(param_2 + 0x30)) {
        int64_t byteOff = 0;  // R14 = iVar*8
        for (;;) {
            int64_t arr = *reinterpret_cast<int64_t*>(param_2 + 0x28);
            uint32_t wantTid = RawMaterialDescription::TypeId();
            void* child = *reinterpret_cast<void**>(arr + byteOff);
            uint32_t tid = reinterpret_cast<Object::Base*>(child)->Type();   // B.3.x bridge (slot 2)
            if (tid == wantTid) {
                int64_t arr2 = *reinterpret_cast<int64_t*>(param_2 + 0x28);
                void* match = *reinterpret_cast<void**>(arr2 + static_cast<int64_t>(iVar) * 8);
                bool derived = ::Object::IsDerivedFromNullSafe(match, RawMaterialDescription::TypeId());
                if (derived) found = match;
                break;
            }
            iVar = iVar + 1;
            byteOff = byteOff + 8;
            if (iVar >= *reinterpret_cast<int*>(param_2 + 0x30)) break;
        }
    }
    uint32_t foundFlags = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(found) + 0x5c);
    return ((*reinterpret_cast<uint32_t*>(param_1 + 0x3c) & foundFlags) != 0) ? 1 : 0;
}

// ============================================================
// FUN_140116760 — storage drop-off: find the Storage component on the entity IBP at
// (param_1+0x10) and subtract `param_3` of resource `param_2` from it (via 12d150).
// ============================================================
void Pf_StorageDropOff_116760(int64_t param_1, int64_t param_2, int param_3) {
    int64_t local_res8 = 0;
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), &local_res8);   // 0x140006270
    ActRm_StorageSubtract_d150(static_cast<uintptr_t>(local_res8), param_2, param_3);   // 0x14012d150
}

// ============================================================
// FUN_1401167c0 — storage count delta for a resource id: find the Storage component on
// the entity IBP at (param_1+0x10), walk its inventory (ptr [stor+0x28], count
// [stor+0x30], stride 0x10: [+0]=resType u64, [+8]=count int, [+0xc]=reserved int).
// For the matching resType return count - reserved (i.e. plVar1[idx*2+1] minus the dword
// at +0xc). Returns 0 if not found.
// ============================================================
int Pf_StorageCountDelta_1167c0(int64_t param_1, int64_t param_2) {
    int64_t local_res8 = 0;
    WorkBase_FindComponent_6270(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), &local_res8);   // 0x140006270
    int iVar4 = 0;
    int64_t lVar3 = 0;
    if (0 < static_cast<int64_t>(*reinterpret_cast<int*>(local_res8 + 0x30))) {
        int64_t* plVar1 = *reinterpret_cast<int64_t**>(local_res8 + 0x28);
        int64_t* plVar2 = plVar1;
        do {
            if (*plVar2 == param_2) {
                if (iVar4 < 0) return 0;
                return static_cast<int>(plVar1[static_cast<int64_t>(iVar4) * 2 + 1]) -
                       *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(plVar1) +
                                               static_cast<int64_t>(iVar4) * 0x10 + 0xc);
            }
            lVar3 = lVar3 + 1;
            iVar4 = iVar4 + 1;
            plVar2 = plVar2 + 2;
        } while (lVar3 < *reinterpret_cast<int*>(local_res8 + 0x30));
    }
    return 0;
}
