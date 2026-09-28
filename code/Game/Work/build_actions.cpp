// code/Game/Work/build_actions.cpp
// Faithful 1:1 port of FUN_1400c0690 — BuildStructure::SetupBuildActions: builds the
// citizen build-labor action chain and pushes it onto the CharacterComponent action
// stack. Called from BuildStructure::Execute (buildstructure.cpp, via the engine_funcs
// global fn_SetupBuildActions == ResolveAddr(Exe, 0x1400c0690)). Unported/NULL in
// standalone was the EXEC-of-0 crash blocking organic build labor.
//
#include "Work/exe_bodies.h"
// Signature (matches buildstructure.cpp's call = fn_SetupBuildActions(&citizenEntityVal,
// self->field_20, (void*)(bs+0x10), bestPos, chosenIndex, (uint8_t)self->field_48)):
//   param_1 = citizen entity InternalBasePtr* (the builder)
//   param_2 = priority / task index (CompleteTask +0x30)
//   param_3 = building entity InternalBasePtr* (the construction site = BuildStructure+0x10)
//   param_4 = target world position (float[3]) — MoveTo destination + heading source
//   param_5 = task param (CompleteTask +0x34)
//   param_6 = altAnim byte — selects the state-8 SendComponentMessage id + tool/anim variant
//
// The chain (order of PUSH via TaskListAppend 0x1400c54e0; the stack is LIFO so it runs
// bottom-to-top): a LEAD SendComponentMessage (target type-id 70e8 = CitizenAIComponent) is stored at CharacterComponent
// +0x58 (NOT pushed — it fires when the chain completes), then pushed in order:
// CompleteTask -> SendComponentMessage(c7008 idle / c75f0 alt) -> ShowTool(-1) ->
// PerformWork(3.0s, heading anim) -> MoveTo(Fast) -> ShowTool(Knife 7 / Saw 4). When the
// timed PerformWork fires, the lead message posts back to the BuildComponent and credits
// build progress (BuildComponent+0x48).
//
// Offset-exact against the 0x1400c0690 disassembly (Ghidra): float consts
// DAT_140172180=3.0f (PerformWork time), DAT_140172280=-1.0f (MoveTo speed sentinel =
// "randomize default"); MoveTo pathType=2 (Fast); ShowTool#1 tool=-1; ShowTool#2 tool=
// uVar2?4(Saw):7(Knife); PerformWork actionType=uVar2?0xd:0x10, p6=1, p7=-1, animIndex=
// Entity_impl_29a0(pos,&buildingEntity) heading; uVar2=Random::Rand(*(gw+0xd8),2). The
// msg-id .rdata globals (c70e8/c7008/c75f0) are 0 in the static image; in standalone the
// BuildComponent type-id (c7008) is healed via GetTypeIdByName so the state-8 message
// targets the BuildComponent (the others carry 0, matching citizenai_actionctors).
//
// fn_SetupBuildActions corresponds to the exe helper at c0690.

#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Action/exe_bodies.h"   // B3R: the action ctor bodies called by name
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/BuildComponent.h"   // ENDGAME B.3.8 CB3: BuildComponent::TypeId()
#include "Component/DestroyComponent.h"   // ENDGAME B.3.8 CB3: DestroyComponent::TypeId()

namespace {

// Already-ported action ctors + stack ops, called by name (B3R; Action/ and Work/
// exe_bodies.h): 0x1400c5400 WorkBase_TaskListCleanup_c5400, 0x1400c54e0
// Pf_ActStackPush_54e0, 0x1400c29a0 ActChild_Entity29a0_c29a0, 0x1400012d0
// ActMv_FindComponentFamily_12d0, 0x1400cb1d0 Pf_SendComponentMessageCopyCtor_b1d0,
// 0x1400c5da0 Pf_CompleteTaskCopyCtor_5da0, 0x140042890 ActChild_ShowToolCtor_42890,
// 0x1400c9720 Pf_PerformWorkCtor7_9720, 0x1400c9170 Pf_MoveToCtor_9170.
uint32_t s_msg7008 = 0, s_msg75f0 = 0;
bool s_init = false;
// ENDGAME B.3.9 (B39_RULES.md D5): 0x1401c7008 and 0x1401c75f0 are registered type-id
// cells (BuildComponent, DestroyComponent) that always hold that class's id once
// Fill* runs, so reading them via the cell is behaviour-neutral with a direct
// Class::TypeId() call. 0x1401c70e8 (CitizenAIComponent) is unregistered; its lead
// message keeps the PORT GAP constant (ENDGAME B.3.9 CB, see CitizenAIComponent.h).

void EnsureInit() {
    if (s_init) return;
    // c7008 = BuildComponent type-id (the state-8 message target).
    s_msg7008 = BuildComponent::TypeId();
    // c75f0 = DestroyComponent type-id -- the altAnim/demolition variant of the same
    // state-8 message.
    s_msg75f0 = DestroyComponent::TypeId();
    s_init = true;
}


inline void  Ctor(void* self, void* obj) { new (self) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge
inline void* Alloc(uint64_t sz)          { return System::MemoryAllocator::MemAlloc(sz, 8); }

} // namespace

extern "C" void BuildStructure_SetupBuildActions(void* citizenIBPptr, uint32_t priority,
                    int64_t* buildingIBPptr, float* position, uint32_t taskParam, uint32_t altAnim) {
    EnsureInit();
    if (!citizenIBPptr) return;

    uintptr_t citizenEntity = *reinterpret_cast<uintptr_t*>(citizenIBPptr);
    if (!citizenEntity) return;
    uintptr_t charComp = *reinterpret_cast<uintptr_t*>(citizenEntity + 0x278);
    if (!charComp) return;
    void* stack = reinterpret_cast<void*>(charComp + 0x48);   // action stack (base +0x48, top +0x60)

    // heading = Entity_impl_29a0(position, &buildingEntityValue) — used as PerformWork anim.
    int64_t buildingEntity = buildingIBPptr ? *buildingIBPptr : 0;
    float heading = 0.0f;
    heading = ActChild_Entity29a0_c29a0(position, &buildingEntity);

    // WIPE the action stack (start the new chain fresh).
    WorkBase_TaskListCleanup_c5400(stack);

    // --- LEAD SendComponentMessage (target type-id 70e8 = CitizenAIComponent) -> stored at CharacterComponent+0x58 (NOT pushed) ---
    {
        void* mem = Alloc(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t outIBP = 0; Ctor(&outIBP, nullptr);
            uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
            int64_t targetEntity = *reinterpret_cast<int64_t*>(citizenEntity + 0x10);  // *(citizenEntity+0x10)
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, reinterpret_cast<uint64_t*>(&targetEntity), &msgId, 0, reinterpret_cast<uint64_t*>(&outIBP), 0);
        }
        int64_t leadIBP = 0; Ctor(&leadIBP, act);
        if (*reinterpret_cast<int64_t*>(charComp + 0x58) != 0)
            reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(charComp + 0x58))->DeletePtr(
                );   // B.3.3 bridge
        *reinterpret_cast<int64_t*>(charComp + 0x58) = leadIBP;
    }

    // uVar2 = Random::Rand(*(gw+0xd8), 2) — tool/anim variant (0 or 1).
    uint32_t uVar2 = 0;
    {
        uintptr_t gw = GameUpdateLoop::_singleton ? reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) : 0;
        void* rng = gw ? *reinterpret_cast<void**>(gw + 0xd8) : nullptr;
        if (rng) uVar2 = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(2));   // B.3.4b bridge
    }

    // --- CompleteTask (0x38) -> PUSH ---
    {
        void* mem = Alloc(0x38);
        void* act = nullptr;
        if (mem) {
            int64_t outIBP = 0; Ctor(&outIBP, nullptr);
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);   // citizen entity
            act = Pf_CompleteTaskCopyCtor_5da0(mem, priority, reinterpret_cast<uint64_t*>(&ent),
                             reinterpret_cast<uint64_t*>(&outIBP), taskParam);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(stack), &ibp);
    }

    // --- SendComponentMessage #2 (msg c7008 idle / c75f0 alt, target=building) -> PUSH ---
    {
        void* mem = Alloc(0x40);
        void* act = nullptr;
        if (mem) {
            int64_t outIBP = 0; Ctor(&outIBP, nullptr);
            uint32_t msgId = (altAnim & 0xff) ? s_msg75f0 : s_msg7008;
            int64_t bldg = buildingIBPptr ? *buildingIBPptr : 0;   // building entity
            act = Pf_SendComponentMessageCopyCtor_b1d0(mem, reinterpret_cast<uint64_t*>(&bldg), &msgId, 0, reinterpret_cast<uint64_t*>(&outIBP), 0);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(stack), &ibp);
    }

    // --- ShowTool #1 (tool = -1) -> PUSH ---
    {
        void* mem = Alloc(0x30);
        void* act = nullptr;
        if (mem) {
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);
            act = ActChild_ShowToolCtor_42890(mem, &ent, -1);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(stack), &ibp);
    }

    // --- PerformWork (0x48, timed 3.0s, heading anim) -> PUSH ---
    {
        void* mem = Alloc(0x48);
        void* act = nullptr;
        if (mem) {
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);
            int64_t timeVal[2] = { 0, 0 };
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeVal), 3.0f);   // DAT_140172180
            uint32_t actionType = (uVar2 != 0) ? 0xd : 0x10;
            act = Pf_PerformWorkCtor7_9720(mem, &ent, heading, actionType, timeVal[0], 1, -1);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(stack), &ibp);
    }

    // --- MoveTo (0x480, Fast, to family-component pos) -> PUSH ---
    {
        void* mem = Alloc(0x480);
        void* act = nullptr;
        if (mem) {
            int64_t entInner = *reinterpret_cast<int64_t*>(citizenEntity + 0x10);
            int64_t famIBP = 0;
            void* fam = ActMv_FindComponentFamily_12d0(static_cast<uintptr_t>(entInner), &famIBP);
            act = Pf_MoveToCtor_9170(mem, fam, 2 /*pathType Fast*/, 0, position, 0xe1f, 0, -1.0f /*DAT_140172280*/);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(stack), &ibp);
    }

    // --- ShowTool #2 (tool = uVar2 ? 4(Saw) : 7(Knife)) -> PUSH ---
    {
        void* mem = Alloc(0x30);
        void* act = nullptr;
        if (mem) {
            int64_t ent = *reinterpret_cast<int64_t*>(citizenIBPptr);
            int toolType = (uVar2 != 0) ? 4 : 7;
            act = ActChild_ShowToolCtor_42890(mem, &ent, toolType);
        }
        int64_t ibp = 0; Ctor(&ibp, act);
        Pf_ActStackPush_54e0(reinterpret_cast<int64_t*>(stack), &ibp);
    }

}
