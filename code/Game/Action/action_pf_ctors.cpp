// code/Game/Action/action_pf_ctors.cpp
// Action-ctor + entity/storage leaf sub-helpers (pathfinding/action closure batch).
//
// Faithful 1:1 ports of 12 small exe helpers referenced from the Action closure.
// Each is transcribed literally from the Ghidra decompile (cross-checked against disassembly
// for intricate index/field math). Do NOT "improve" — none of this is exercised on the
// autostart path, so a wrong port is undetectable latent corruption.
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
//   (??0Time@System@@QEAA@M@Z). Constants: kRdata_1401720e4 / kRdata_14017214c.
// Runtime export: System::Random::RandFloatTol (MoveTo speed default).
//
// ENDGAME B.3.8 ACT: the five action ctors (c5da0 CompleteTask, c71a0
// CreateRawMaterial, c9170 MoveTo, c9720 PerformWork, cb1d0 SendComponentMessage)
// are the classes' REAL constructors now, defined below next to their by-address
// doors; the compiler stamps the vtables (no ResolveAddr'd table remains here).

#include "Action/CompleteTask.h"
#include "Action/CreateRawMaterial.h"
#include "Action/MoveTo.h"
#include "Action/PerformWork.h"
#include "Action/SendComponentMessage.h"
#include "engine_funcs.h"
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

namespace {
// The .rdata float constants (Game/exe_constants.h) that are stored as bit patterns.
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }
// 0x1400032e0 (Array<Ptr8>::SetSize) is the exe-side Collection::Array<T>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::Ptr8> member call, so its resolved-pointer stand-in
// is gone.
} // namespace

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
// FUN_1400c5da0 — CompleteTask_Action CopyCtor
// Base Internal ctor, then Action vtable -> CompleteTask vtable; copies two IBP fields
// (+0x20 from param_3, +0x28 from param_4) and two int fields (+0x30=param_2, +0x34=param_5).
// ============================================================
void* Pf_CompleteTaskCopyCtor_5da0(void* param_1, uint32_t param_2, uint64_t* param_3,
                                   uint64_t* param_4, uint32_t param_5) {
    // ENDGAME B.3.8 ACT: the real ctor (CompleteTask.h). A by-address door.
    return static_cast<Object::Base*>(new (param_1) CompleteTask(param_2, param_3, param_4, param_5));
}

// 0x1400c5da0: +0x20 = *param_3 ; +0x28 = *param_4 ; +0x30 = param_2 ; +0x34 = param_5.
CompleteTask::CompleteTask(uint32_t taskIndexArg, uint64_t* workEntityArg, uint64_t* targetEntityArg,
                           uint32_t taskParamArg)
    : workEntity(reinterpret_cast<void*>(*workEntityArg)),
      targetEntity(reinterpret_cast<void*>(*targetEntityArg)),
      taskIndex(static_cast<int32_t>(taskIndexArg)),
      taskParam(static_cast<int32_t>(taskParamArg)) {}

// ============================================================
// FUN_1400c71a0 — CreateRawMaterial_Action CopyCtor
// Base Internal ctor, Action -> CreateRawMaterial vtable; copies +0x20=param_2 (raw u64),
// three IBP fields (+0x28 from param_4, +0x30 from param_5, +0x38 from param_10), three
// int fields (+0x40=param_7, +0x44=param_8, +0x48=param_11), one FLOAT (+0x4c=param_3, the
// XMM2 arg stored via MOVSS), and two bytes (+0x50=param_9, +0x51=1).
// ============================================================
void* Pf_CreateRawMaterialCopyCtor_71a0(void* param_1, uint64_t param_2, float param_3,
                                        uint64_t* param_4, uint64_t* param_5, uint64_t param_6,
                                        uint32_t param_7, uint32_t param_8, uint8_t param_9,
                                        uint64_t* param_10, uint32_t param_11) {
    // ENDGAME B.3.8 ACT: the real ctor (CreateRawMaterial.h). A by-address door.
    return static_cast<Object::Base*>(new (param_1) CreateRawMaterial(
        param_2, param_3, param_4, param_5, param_6, param_7, param_8, param_9, param_10, param_11));
}

// 0x1400c71a0: +0x20 = param_2 (raw) ; +0x28/+0x30/+0x38 = *param_4 / *param_5 /
// *param_10 ; +0x40/+0x44/+0x48 = param_7/8/11 ; +0x4c = param_3 (MOVSS, the XMM2
// float) ; +0x50 = param_9 ; +0x51 = 1. param_6 is not read.
CreateRawMaterial::CreateRawMaterial(uint64_t desc, float yield, uint64_t* source, uint64_t* secondary,
                                     uint64_t /*unused6*/, uint32_t quantity, uint32_t cellIdx,
                                     uint8_t naturalRes, uint64_t* tertiary, uint32_t placement)
    : descEntity(reinterpret_cast<void*>(desc)),
      sourceBuilding(reinterpret_cast<void*>(*source)),
      secondaryEntity(reinterpret_cast<void*>(*secondary)),
      tertiaryEntity(reinterpret_cast<void*>(*tertiary)),
      totalQuantity(static_cast<int32_t>(quantity)),
      materialCellIdx(static_cast<int32_t>(cellIdx)),
      placementFlags(placement),
      yieldMultiplier(yield),
      naturalResFlag(naturalRes),
      placeNearFlag(1) {}

// ============================================================
// FUN_1400c9170 — MoveTo_Action Ctor
// Base Internal ctor, Action -> MoveTo vtable; inits fields, copies destination vec3
// (+0x38..+0x40), the entity IBP (+0x28 from param_2), resolves a derived component at
// +0x30 (IsDerivedFrom obj, DAT_1401c70b0). Speed: if >=0 use it, else a randomised
// default = RandFloatTol(world.random) * DAT_1401720e4 + DAT_14017214c.
// Note: hidden float arg `speed` arrives in XMM (XMM2 per the prologue), param order
// matches Ghidra (pathType, param_4, destination, param_6, param_7, speed).
// ============================================================
void* Pf_MoveToCtor_9170(void* self, void* param_2, uint32_t pathType, int param_4,
                         float* destination, int param_6, int param_7, float speed) {
    // ENDGAME B.3.8 ACT: the real ctor (MoveTo.h). A by-address door.
    return static_cast<Object::Base*>(
        new (self) MoveTo(param_2, pathType, param_4, destination, param_6, param_7, speed));
}

// 0x1400c9170 — the ctor body, moved from the door above; after Action() the
// retail stores are kept verbatim at their raw offsets (the destination and the
// path block have no named members yet).
MoveTo::MoveTo(void* param_2, uint32_t pathType, int param_4, float* destination, int param_6,
               int param_7, float speed) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    *reinterpret_cast<uint32_t*>(s + 0x20) = 0xffffffff;
    *reinterpret_cast<uint32_t*>(s + 0x24) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x28) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x28) = *reinterpret_cast<uint64_t*>(param_2);
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<float*>(s + 0x38) = destination[0];
    *reinterpret_cast<float*>(s + 0x3c) = destination[1];
    *reinterpret_cast<float*>(s + 0x40) = destination[2];
    *reinterpret_cast<uint64_t*>(s + 0x44) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x4c) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x54) = 0;
    *reinterpret_cast<int*>(s + 0x5c) = param_6;
    *reinterpret_cast<uint32_t*>(s + 0x60) = 0xffffffff;
    *reinterpret_cast<uint32_t*>(s + 0x64) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x468) = pathType;
    *reinterpret_cast<int*>(s + 0x46c) = param_4;
    *reinterpret_cast<int*>(s + 0x470) = param_7;
    s[0x474] = 0;
    void* obj = *reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(s + 0x28) + 0x18);
    // DAT_1401c70b0 is CharacterDescription's type-id cell (Table 1).
    uint32_t charDescTid = CharacterDescription::TypeId();
    bool derived = charDescTid && ::Object::IsDerivedFromNullSafe(obj, charDescTid);
    *reinterpret_cast<void**>(s + 0x30) = derived ? obj : nullptr;
    if (0.0f <= speed) {
        *reinterpret_cast<float*>(s + 0x478) = speed;
    } else {
        void* random = *reinterpret_cast<void**>(
            reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) + 0xd8);
        float r = random ? reinterpret_cast<System::Random*>(random)->RandFloatTol() : 0.0f;   // B.3.4b bridge
        *reinterpret_cast<float*>(s + 0x478) =
            r * RdataF(kRdata_1401720e4) + kRdata_14017214c;   // 0x1401720e4 / 0x14017214c
    }
}

// ============================================================
// FUN_1400c9720 — PerformWork_Action Ctor (7-field variant)
// Base Internal ctor, Action -> PerformWork vtable; copies entity IBP (+0x20), anim
// index (+0x28, float arg XMM2), action type (+0x2c), Time (+0x30), flag (+0x40), int
// (+0x44). If the flag is set and the entity's +0xac < 1, scales the Time by the
// component's +0x114 seconds (Time::Time(comp.f114); self.time = AsSeconds * AsSeconds).
// animIndex arrives in XMM2 (a float); keep it as a 32-bit store at +0x28.
// ============================================================
void* Pf_PerformWorkCtor7_9720(void* self, void* entity, float animIndex, uint32_t actionType,
                               int64_t time, uint8_t param_6, int param_7) {
    // ENDGAME B.3.8 ACT: the real ctor (PerformWork.h). A by-address door.
    return static_cast<Object::Base*>(
        new (self) PerformWork(entity, animIndex, actionType, time, param_6, param_7));
}

// 0x1400c9720 — the ctor body, moved from the door above; after Action() the
// retail stores are kept verbatim at their raw offsets.
PerformWork::PerformWork(void* entity, float animIndex, uint32_t actionType, int64_t time,
                         uint8_t param_6, int param_7) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    *reinterpret_cast<uint64_t*>(s + 0x20) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x20) = *reinterpret_cast<uint64_t*>(entity);
    *reinterpret_cast<float*>(s + 0x28) = animIndex;
    *reinterpret_cast<uint32_t*>(s + 0x2c) = actionType;
    *reinterpret_cast<int64_t*>(s + 0x30) = time;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    s[0x40] = param_6;
    *reinterpret_cast<int*>(s + 0x44) = param_7;
    if (param_6 != 0) {
        if (*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(entity) + 0xac) < 1) {
            void* obj = *reinterpret_cast<void**>(*reinterpret_cast<int64_t*>(s + 0x20) + 0x18);
            // IsDerivedFromOrTrust: the original gates `comp = IsDerivedFrom(obj,c7190) ? obj
            // : null` then UNCONDITIONALLY reads comp+0x114 — i.e. it ASSUMES obj is the type
            // (it would crash too if not). In standalone IsDerivedFrom returns false on the
            // partial factory tree, so we TRUST obj (it IS the linked +0x18 desc) and only
            // guard the genuinely-null case (skip the work-duration scaling -> base time).
            void* comp = obj;
            if (comp) {
                // Time scaleTime(comp.f114); self.time = AsSeconds(self.time) * AsSeconds(scaleTime)
                float compSecs = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(comp) + 0x114);
                uint64_t scaleTimeStore = 0;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&scaleTimeStore), compSecs);
                float selfSecs  = *reinterpret_cast<float*>(s + 0x30);
                float scaleSecs = *reinterpret_cast<float*>(&scaleTimeStore);
                uint64_t prod = 0;
                RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&prod), selfSecs * scaleSecs);
                *reinterpret_cast<uint64_t*>(s + 0x30) = prod;
            }
        }
    }
}

// ============================================================
// FUN_1400cb1d0 — SendComponentMessage_Action CopyCtor
// Base Internal ctor, Action -> SendComponentMessage vtable; copies IBP +0x20 (param_2),
// IBP +0x28 (param_5), int +0x30 (*param_3), +0x34 (param_4), +0x38 (param_6), +0x3c=0.
// ============================================================
void* Pf_SendComponentMessageCopyCtor_b1d0(void* param_1, uint64_t* param_2, uint32_t* param_3,
                                           uint32_t param_4, uint64_t* param_5, uint32_t param_6) {
    // ENDGAME B.3.8 ACT: the real ctor (SendComponentMessage.h). A by-address door.
    return static_cast<Object::Base*>(
        new (param_1) SendComponentMessage(param_2, param_3, param_4, param_5, param_6));
}

// 0x1400cb1d0 — the ctor body, moved from the door above: +0x20 = *param_2,
// +0x28 = *param_5, then the retail stores (and the tree's typeid heal) verbatim.
SendComponentMessage::SendComponentMessage(uint64_t* param_2, uint32_t* param_3, uint32_t param_4,
                                           uint64_t* param_5, uint32_t param_6)
    : _field20(reinterpret_cast<void*>(*param_2)),
      _field28(reinterpret_cast<void*>(*param_5)) {
    uint8_t* s = reinterpret_cast<uint8_t*>(this);
    uint32_t compTypeId = *param_3;
    // [ECONOMY M-D — BANISHED_LIVE_STORAGE typeid heal]
    // The eat push (PerformWork_Action_Ctor6, citizenai_actionctors.cpp) reads the
    // component-typeid from .rdata DWORD_1401c7158 (CitizenComponent) — deliberately
    // NOT registered in standalone (see reg_work_tail_trade.cpp), so it arrives here
    // as 0 and SendComponentMessage::AbstractImpl can never match a component
    // (returns 2, message silently dropped). Under BANISHED_LIVE_STORAGE, heal the
    // msg==2 (EAT) case only — msg 2 with a zero typeid is uniquely the ctor6
    // eat-delivery message — substituting the live GetTypeIdByName CRC that our
    // CitizenComponent live vtable's GetTypeId (slot 2) returns. msg 0 stays
    // unhealed: a zero-typeid msg 0 is ambiguous (CitizenAIComponent 0x1401c70e8
    // completion lead vs CitizenComponent warm-house), deferred.
    if (compTypeId == 0 && param_4 == 2) {
        compTypeId = CitizenComponent::TypeId();
    }
    *reinterpret_cast<uint32_t*>(s + 0x30) = compTypeId;
    *reinterpret_cast<uint32_t*>(s + 0x34) = param_4;
    *reinterpret_cast<uint32_t*>(s + 0x38) = param_6;
    *reinterpret_cast<uint32_t*>(s + 0x3c) = 0;
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
