// code/Game/Work/work_act_hunt.cpp
// Hunt / ShowTool work-action-queue builders (Work action-ctor tail batch).
//
// Two instruction-faithful 1:1 ports of exe work-action constructors. Each builds
// a chain of Action objects (SendComponentMessage / CompleteTask / ShowTool /
// PerformWork / MoveTo / CreateNaturalResource / HuntAnimal) and appends their
// InternalBasePtr wrappers to the citizen's action stack (CitizenComponent
// +0x278 = its CharacterComponent, whose +0x48 is the stack). The
// SendComponentMessage action is stored at the character's +0x58
// (CharacterComponent::field_0x58), DeletePtr'ing the previous one first.
#include "Work/exe_bodies.h"
//
//   WorkAct_HuntWorkCtor_bbbc0   <- HuntWork_Serialize_sub_bbc0 @ 0x1400bbbc0
//   WorkAct_ShowToolCtor3_bdd20  <- ShowTool_Action_Ctor3        @ 0x1400bdd20
//
// Disasm references cite Application-x64.exe addresses (base 0x140000000).

#include <new>
#include "system/MemoryAllocator.h"
#include "Action/Action.h"
#include "Action/CreateNaturalResource.h"   // ENDGAME B.3.8 CB3
#include "Action/ShowTool.h"   // ENDGAME B.3.8 CB3
#include "Action/exe_bodies.h"      // B3R: the action ctor bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/Value.h"           // B3R: RtSystem_Time_Ctor
#include <cstring>
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include "Component/CitizenComponent.h"
#include "Component/CharacterComponent.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Sub-callees, called by name (B3R)
// ============================================================
// The disasm's InternalBasePtr dispatch slots (0x140152a90 ctor, 0x140152ab0 CheckPtr,
// 0x140151ab8 DeletePtr) are the real Object::InternalBasePtr members. Every exe
// sub-callee is one of our own bodies (Action/, Work/ exe_bodies.h):
//   0x1400c5400 TaskListCleanup                     WorkBase_TaskListCleanup_c5400
//   0x1400c54e0 TaskListAppend                      Pf_ActStackPush_54e0
//   0x1400012d0 Entity_FindComponent_Family         ActMv_FindComponentFamily_12d0
//   0x1400cb1d0 SendComponentMessage copy-ctor      Pf_SendComponentMessageCopyCtor_b1d0
//   0x1400c5da0 CompleteTask copy-ctor              Pf_CompleteTaskCopyCtor_5da0
//   0x140042890 ShowTool ctor (R8D=toolType)        ActChild_ShowToolCtor_42890
//   0x1400c8400 HuntAnimal copy-ctor                ActChild_HuntAnimalCopyCtor_c8400
//   0x1400c9720 PerformWork_Action_Ctor7            Pf_PerformWorkCtor7_9720
//   0x1400c9170 MoveTo ctor                         Pf_MoveToCtor_9170
// Dispatch slots: 0x140152b60 MemAlloc -> System::MemoryAllocator::MemAlloc;
// 0x140152b00 System::Time(float) -> RtSystem_Time_Ctor.
// .rdata: 0x140172280 = -1.0f (MoveTo speed), 0x140172178 = 2.1f (PerformWork time,
// bdd20), 0x1401722a8 = -1000.0f (PerformWork heading const; a bit pattern).
//
// LEFT (both bodies): the action ctors above are ACT's C bodies taking their
// entity references as uint64_t* / uintptr_t and returning void*; each call keeps
// the one conversion its signature asks for.

static inline float WAH_RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

static inline void* WAH_Alloc(uint64_t size, uint32_t align) {
    return System::MemoryAllocator::MemAlloc(size, static_cast<int>(align));
}
static inline void  WAH_IbpCtor(void* ibp, void* obj) { new (ibp) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge
static inline void  WAH_Append(void* list, void* ibp) { Pf_ActStackPush_54e0(static_cast<int64_t*>(list), ibp); }

// The SendComponentMessage action goes into the character's +0x58 slot, the
// previous one DeletePtr'd first. LEFT: CharacterComponent declares the InternalPtr
// member as Action* (COMP-a), so DeletePtr is reached through the base view.

// ============================================================
// WorkAct_HuntWorkCtor_bbbc0 (0x1400bbbc0)
// ============================================================
// Build the hunt work-action chain on the citizen's action stack.
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId (u32)               [disasm R15D := EDX]
//   param_3 = longlong* (target inner;   [disasm R13 := R8] *param_3 = inner value)
//   param_4 = float* worldPos            [disasm R12 := R9] (MoveTo destination)
//   param_5 = float (PerformWork time)   [stack RBP+0x50]
//   param_6 = char (MoveTo-vs-PerformWork selector) [stack RBP+0x58]
//
// Chain:
//   [character+0x58] = SendComponentMessage(entityInner, msgId=DAT_1401c70e8, 0, innerIbp(0), 0)
//   append CompleteTask(workId, entityInner, innerIbp(0), 0)
//   if param_6==0 : append PerformWork_Ctor7(entityInner, heading=DAT_1401722a8(-1000),
//                          actionType=8, time=Time(param_5), p6=1, p7=-1)
//   else          : append MoveTo(family, Slow(1), 0, param_4, 0xe1f, 0, speed=DAT_140172280(-1.0))
//   HuntAnimal(entityInner=*param_1, innerVal=*param_3) -> stored as the chain link
//                          (+0x10) of the stack's running action (NOT appended)
void WorkAct_HuntWorkCtor_bbbc0(void* citizenIBP, uint32_t workId, int64_t* targetInnerPtr,
                                float* worldPos, float performWorkTime, char selectMoveTo) {

    // LEFT: the outside callers (WORK-b) hand the citizen reference untyped.
    CitizenComponent* citizen = *reinterpret_cast<CitizenComponent**>(citizenIBP);
    // --- the action stack = character + 0x48 ---  (disasm 0x1400bbbf3..0x1400bbc17)
    CharacterComponent* character = citizen->character;
    WorkBase_TaskListCleanup_c5400(&character->actions);

    Action* ibpA = nullptr;
    Action* ibpB = nullptr;
    Action* ibpC = nullptr;

    // --- SendComponentMessage action -> stored at character+0x58, DeletePtr old first ---
    // disasm 0x1400bbc1c..0x1400bbca9 (alloc 0x40,8)
    void* obj = WAH_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uint64_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        ComponentEntity* entityInner = citizen->_entity;
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpA, sm);
    if (character->field_0x58 != nullptr)
        reinterpret_cast< ::Object::InternalBasePtr*>(&character->field_0x58)->DeletePtr();
    character->field_0x58 = ibpA;       // [RDI+0x10] = character+0x58

    // --- CompleteTask action --- disasm 0x1400bbcad..0x1400bbd0f (alloc 0x38,8)
    obj = WAH_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uint64_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        CitizenComponent* entityInner = citizen;  // *param_1
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, workId, reinterpret_cast<uint64_t*>(&entityInner), &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpB, ct);
    WAH_Append(&character->actions, &ibpB);

    // --- selector: param_6 != 0 -> MoveTo (0x480) ; else PerformWork_Ctor7 (0x48) ---
    // disasm 0x1400bbd14 (CMP byte [RBP+0x58],0)
    void* mainAction = nullptr;
    if (selectMoveTo != 0) {
        // disasm 0x1400bbd1a..0x1400bbd91 (alloc 0x480,8)
        obj = WAH_Alloc(0x480, 8);
        if (obj) {
            ComponentEntity* entityInner = citizen->_entity;
            uint64_t famRet = 0;
            void* familyIBP = nullptr;
            familyIBP = ActMv_FindComponentFamily_12d0(reinterpret_cast<uintptr_t>(entityInner), &famRet);
            float speed = kRdata_140172280;  // -1.0f
            // MoveTo(self, family, Slow=1, 0, param_4, 0xe1f, 0, speed)  (disasm R8D=1, [RSP+0x28]=0xe1f)
            mainAction = Pf_MoveToCtor_9170(obj, familyIBP, 1 /*Slow; disasm LEA R8D,[R9+1]*/, 0, worldPos, 0xe1f, 0, speed);
        }
    } else {
        // disasm 0x1400bbd98..0x1400bbdf4 (alloc 0x48,8)
        obj = WAH_Alloc(0x48, 8);
        if (obj) {
            CitizenComponent* entityInner = citizen;  // *param_1
            System::Time timeVal = {0};
            RtSystem_Time_Ctor(&timeVal, performWorkTime);  // System::Time::Time(&t, param_5/XMM1)
            float heading = WAH_RdataF(kRdata_1401722a8);       // XMM2 = [0x1401722a8] = -1000.0f
            // PerformWork_Ctor7(self, &entityInner, heading, actionType=8, time, p6=1, p7=-1)
            // disasm: R9D=0x8, XMM2=[0x1401722a8], [RSP+0x28]=1, [RSP+0x30]=0xffffffff, time=*Time()
            mainAction = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, 8, timeVal._ticks, 1, -1);
        }
    }
    WAH_IbpCtor(&ibpA, mainAction);
    WAH_Append(&character->actions, &ibpA);

    // --- HuntAnimal action -> stored as the running action's chain link ---
    // disasm 0x1400bbe12..0x1400bbe73 (alloc 0x40,8). NOTE: NOT a c54e0 append; the
    // original writes [ actions[currentAction] + 0x10 ] where currentAction = character+0x60.
    obj = WAH_Alloc(0x40, 8);
    void* hunt = nullptr;
    if (obj) {
        uint64_t innerVal = static_cast<uint64_t>(*targetInnerPtr);          // *param_3 -> local_res8
        CitizenComponent* entityInner = citizen;                            // *param_1 -> local_48
        hunt = ActChild_HuntAnimalCopyCtor_c8400(obj, reinterpret_cast<uint64_t*>(&entityInner), &innerVal);
    }
    WAH_IbpCtor(&ibpC, hunt);
    // plVar8 = &actions[currentAction] ; CheckPtr(plVar8) ; (*plVar8)->+0x10 = ibpC
    Action* running = character->actions[character->currentAction];
    WAH_IbpCtor(&running->_next, ibpC);   // Action::_next (+0x10)
    (void)ibpB;
}

// ============================================================
// WorkAct_ShowToolCtor3_bdd20 (0x1400bdd20)
// ============================================================
// Large work-queue builder: SendComponentMessage -> character+0x58, CompleteTask,
// ShowTool(~Hoe), a per-element loop of (optional SendComponentMessage +
// CreateNaturalResource), then a final PerformWork_Ctor7, MoveTo, and ShowTool(Shovel=3).
// Its one caller is PlantArea::Execute; the parameter types are what the body does
// with them.
//
//   citizen       = the citizen reference            [RCX -> R15, home RBP+0x40]
//   workId        = (u32)                            [EDX -> EDI]
//   plantType     = the crop (CreateNaturalResource +0x20)  [R8, home RBP+0x50]
//   unused4       = dword (spilled local_res20)      [R9D, home RBP+0x58]
//   destination   = float* (MoveTo)                  [RBP+0x60]
//   loopCount     = int                              [RBP+0x68]
//   cells         = the cells, one per iteration (CreateNaturalResource +0x30)  [RBP+0x70]
//   completeParam8 = CompleteTask param_5            [RBP+0x78]
//   messageTarget = the entity (per-iter gate; SendComponentMessage entity;
//                   CreateNaturalResource +0x28)     [RBP+0x80]
//   msgIdPtr10    = dword* (SendComponentMessage msg id)  [RBP+0x88]
//   unused11                                         [RBP+0x90]
//   messageArgs   = the per-iteration message argument array: retail walks it in
//                   step with `cells` (lVar15 = param_12 - param_7; [lVar15 + param_7])  [RBP+0x98]
void WorkAct_ShowToolCtor3_bdd20(CitizenComponent** citizenRef, uint32_t workId, ComponentDescription* plantType,
                                 uint32_t /*unused4*/, float* destination, int loopCount,
                                 const int32_t* cells, uint32_t completeParam8, ComponentEntity** messageTarget,
                                 uint32_t* msgIdPtr10, uint64_t /*unused11*/, const uint32_t* messageArgs) {

    CitizenComponent* citizen = *citizenRef;
    // --- the action stack = character + 0x48 ---  (disasm 0x1400bdd55..0x1400bdd79)
    CharacterComponent* character = citizen->character;
    WorkBase_TaskListCleanup_c5400(&character->actions);

    Action* ibpA = nullptr;
    Action* ibpB = nullptr;

    // --- SendComponentMessage #1 -> character+0x58, DeletePtr old first ---
    // disasm 0x1400bdd7e..0x1400bde0f (alloc 0x40,8)
    void* obj = WAH_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uint64_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        ComponentEntity* entityInner = citizen->_entity;
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpA, sm);
    if (character->field_0x58 != nullptr)
        reinterpret_cast< ::Object::InternalBasePtr*>(&character->field_0x58)->DeletePtr();
    character->field_0x58 = ibpA;

    // --- CompleteTask action (param_5 = param_8) --- disasm 0x1400bde13..0x1400bde72 (alloc 0x38,8)
    obj = WAH_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uint64_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        CitizenComponent* entityInner = citizen;  // *param_1
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, workId, reinterpret_cast<uint64_t*>(&entityInner), &innerIbp, completeParam8);
    }
    WAH_IbpCtor(&ibpB, ct);
    WAH_Append(&character->actions, &ibpB);

    // --- ShowTool (~Hoe = -1) --- disasm 0x1400bde77..0x1400bdec3 (alloc 0x30,8; OR R8D,0xffffffff)
    obj = WAH_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        CitizenComponent* entityInner = citizen;  // *param_1
        st = ActChild_ShowToolCtor_42890(obj, &entityInner, -1 /* ~Hoe; disasm OR R8D,0xFFFFFFFF */);
    }
    WAH_IbpCtor(&ibpA, st);
    WAH_Append(&character->actions, &ibpA);

    // --- per-element loop: loopCount iterations over `cells` (and `messageArgs`) ---
    // disasm 0x1400bdec3..0x1400be1cc.
    if (loopCount > 0) {
        int64_t remaining = loopCount;                                   // local_70
        int iteration = 0;
        do {
            // --- optional SendComponentMessage #2 when *param_9 != 0 --- (alloc 0x40,8)
            if (*messageTarget != nullptr) {
                obj = WAH_Alloc(0x40, 8);
                void* sm2 = nullptr;
                if (obj) {
                    uint64_t innerIbp = 0;
                    WAH_IbpCtor(&innerIbp, nullptr);
                    uint32_t msgId = msgIdPtr10 ? *msgIdPtr10 : 0;       // *param_10 (the null arm is the tree's)
                    ComponentEntity* entityInner = *messageTarget;       // *param_9 -> local_78
                    // p6 = the iteration's message argument   (disasm MOV ECX,[RDI + R12])
                    uint32_t p6 = messageArgs[iteration];
                    sm2 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, p6);
                }
                WAH_IbpCtor(&ibpB, sm2);
                WAH_Append(&character->actions, &ibpB);  // inlined grow-and-append in disasm (== FUN_1400c54e0)
            }

            // --- inline CreateNaturalResource action --- (alloc 0x38,8) disasm 0x1400be01c..0x1400be075
            obj = WAH_Alloc(0x38, 8);
            void* cnr = nullptr;
            if (obj) {
                ComponentEntity* gateVal = *messageTarget;           // lVar11 = *param_9
                uint32_t baseVal = static_cast<uint32_t>(cells[iteration]);   // uVar1 = *param_7 (current)
                // the inlined CreateNaturalResource ctor (+0x20 param_3 raw, +0x28 *param_9,
                // +0x30 *param_7) — the real ctor (typed by B.4 ACT-b).
                new (obj) CreateNaturalResource(plantType, &gateVal, baseVal);
                cnr = obj;
            }
            WAH_IbpCtor(&ibpA, cnr);
            WAH_Append(&character->actions, &ibpA);  // inlined grow-and-append in disasm (== FUN_1400c54e0)

            iteration = iteration + 1;     // param_7 += 1 (dword, i.e. +4 bytes)
            remaining = remaining - 1;
        } while (remaining != 0);
    }

    // --- final PerformWork_Ctor7 --- disasm 0x1400be1dd..0x1400be23f (alloc 0x48,8)
    obj = WAH_Alloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        CitizenComponent* entityInner = citizen;  // *param_1
        System::Time timeVal = {0};
        RtSystem_Time_Ctor(&timeVal, kRdata_140172178);  // XMM1=[0x140172178]=2.1
        float heading = WAH_RdataF(kRdata_1401722a8);  // XMM2=[0x1401722a8]=-1000.0
        // PerformWork_Ctor7(self, &entityInner, heading, actionType=0xC, time, p6=1, p7=-1)
        // disasm: R9D=0xc, XMM2=[0x1401722a8], [RSP+0x28]=1, [RSP+0x30]=0xffffffff
        pw7 = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, 0xC, timeVal._ticks, 1, -1);
    }
    WAH_IbpCtor(&ibpA, pw7);
    WAH_Append(&character->actions, &ibpA);  // inlined grow-and-append in disasm

    // --- MoveTo (Fastest=3, destination=param_5) --- disasm 0x1400be38d..0x1400be40e (alloc 0x480,8)
    obj = WAH_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        ComponentEntity* entityInner = citizen->_entity;
        uint64_t famRet = 0;
        void* familyIBP = nullptr;
        familyIBP = ActMv_FindComponentFamily_12d0(reinterpret_cast<uintptr_t>(entityInner), &famRet);
        float speed = kRdata_140172280;  // XMM0=[0x140172280]=-1.0
        // MoveTo(self, family, Fastest=3, 0, param_5, 0xe1f, 0, speed)  (disasm R8D=3, [RSP+0x28]=0xe1f)
        mv = Pf_MoveToCtor_9170(obj, familyIBP, 3 /*Fastest; disasm LEA R8D,[R9+3]*/, 0, destination, 0xe1f, 0, speed);
    }
    WAH_IbpCtor(&ibpA, mv);
    WAH_Append(&character->actions, &ibpA);  // inlined grow-and-append in disasm

    // --- inline ShowTool (Shovel = 3) action --- disasm 0x1400be55d..0x1400be5a4
    // alloc 0x30,8 (disasm 0x1400be55d MOV EDX,0x8 / LEA ECX,[RDX+0x28] => 0x30)
    obj = WAH_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        CitizenComponent* entityInner = citizen;  // *param_1 -> RBX
        // the inlined ShowTool ctor (+0x20 *param_1, +0x28 Shovel = 3) — the real ctor.
        new (obj) ShowTool(&entityInner, 3);
        st2 = obj;
    }
    WAH_IbpCtor(&ibpA, st2);
    WAH_Append(&character->actions, &ibpA);  // inlined grow-and-append in disasm
    (void)ibpB;
}
