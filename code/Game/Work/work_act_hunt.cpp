// code/Game/Work/work_act_hunt.cpp
// Hunt / ShowTool work-action-queue builders (Work action-ctor tail batch).
//
// Two instruction-faithful 1:1 ports of exe work-action constructors. Each builds
// a chain of Action objects (SendComponentMessage / CompleteTask / ShowTool /
#include "Work/exe_bodies.h"
// PerformWork / MoveTo / CreateNaturalResource / HuntAnimal) and appends their
// InternalBasePtr wrappers to the citizen's CitizenAIComponent task action list
// (citizen+0x278 -> aiComp, list base aiComp+0x48). The SendComponentMessage action
// is stored at aiComp+0x58 (list+0x10), DeletePtr'ing the previous one first.
//
//   WorkAct_HuntWorkCtor_bbbc0   <- HuntWork_Serialize_sub_bbc0 @ 0x1400bbbc0
//   WorkAct_ShowToolCtor3_bdd20  <- ShowTool_Action_Ctor3        @ 0x1400bdd20
//
// This file mirrors the verified resolution-block idiom of work_base_actionctor.cpp.
// Statics are prefixed s_wah_* to avoid ODR clashes with sibling files. Every exe
// sub-callee is resolved via ResolveAddr(Module::Exe, 0xADDR); they resolve to
// registered ports (or 0 = silently skipped).
//
// Disasm references cite Application-x64.exe addresses (base 0x140000000).

#include "engine_funcs.h"
#include "system/MemoryAllocator.h"
#include "Action/CreateNaturalResource.h"   // ENDGAME B.3.8 CB3
#include "Action/ShowTool.h"   // ENDGAME B.3.8 CB3
#include "Action/exe_bodies.h"      // B3R: the action ctor bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/Value.h"           // B3R: RtSystem_Time_Ctor
#include <cstring>
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
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

static inline float WAH_RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

static inline void* WAH_Alloc(uint64_t size, uint32_t align) {
    return System::MemoryAllocator::MemAlloc(size, static_cast<int>(align));
}
static inline void  WAH_IbpCtor(void* ibp, void* obj) { new (ibp) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge
static inline void  WAH_Append(void* list, void* ibp) { Pf_ActStackPush_54e0(static_cast<int64_t*>(list), ibp); }

// ============================================================
// WorkAct_HuntWorkCtor_bbbc0 (0x1400bbbc0)
// ============================================================
// Build the hunt work-action chain on the citizen's CitizenAIComponent task list.
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId (u32)               [disasm R15D := EDX]
//   param_3 = longlong* (target inner;   [disasm R13 := R8] *param_3 = inner value)
//   param_4 = float* worldPos            [disasm R12 := R9] (MoveTo destination)
//   param_5 = float (PerformWork time)   [stack RBP+0x50]
//   param_6 = char (MoveTo-vs-PerformWork selector) [stack RBP+0x58]
//
// Chain:
//   [aiComp+0x58] = SendComponentMessage(entityInner, msgId=DAT_1401c70e8, 0, innerIbp(0), 0)
//   append CompleteTask(workId, entityInner, innerIbp(0), 0)
//   if param_6==0 : append PerformWork_Ctor7(entityInner, heading=DAT_1401722a8(-1000),
//                          actionType=8, time=Time(param_5), p6=1, p7=-1)
//   else          : append MoveTo(family, Slow(1), 0, param_4, 0xe1f, 0, speed=DAT_140172280(-1.0))
//   HuntAnimal(entityInner=*param_1, innerVal=*param_3) -> stored directly into the list slot
//                          at index *(int*)(list+0x18), at *(list[idx])+0x10 (NOT appended)
void WorkAct_HuntWorkCtor_bbbc0(void* citizenIBP, uint32_t workId, int64_t* targetInnerPtr,
                                float* worldPos, float performWorkTime, char selectMoveTo) {

    // --- list base = (*(citizen)+0x278) + 0x48 ---  (disasm 0x1400bbbf3..0x1400bbc17)
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);  // RDI
    WorkBase_TaskListCleanup_c5400(list);

    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0;

    // --- SendComponentMessage action -> stored at list+0x10 (aiComp+0x58), DeletePtr old first ---
    // disasm 0x1400bbc1c..0x1400bbca9 (alloc 0x40,8)
    void* obj = WAH_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpA, sm);
    if (*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(list) + 0x10) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(list) + 0x10))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(list) + 0x10) = ibpA;  // [RDI+0x10] = aiComp+0x58

    // --- CompleteTask action --- disasm 0x1400bbcad..0x1400bbd0f (alloc 0x38,8)
    obj = WAH_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, workId, &entityInner, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpB, ct);
    WAH_Append(list, &ibpB);

    // --- selector: param_6 != 0 -> MoveTo (0x480) ; else PerformWork_Ctor7 (0x48) ---
    // disasm 0x1400bbd14 (CMP byte [RBP+0x58],0)
    void* mainAction = nullptr;
    if (selectMoveTo != 0) {
        // disasm 0x1400bbd1a..0x1400bbd91 (alloc 0x480,8)
        obj = WAH_Alloc(0x480, 8);
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
            uintptr_t famRet = 0;
            void* familyIBP = nullptr;
            familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
            float speed = kRdata_140172280;  // -1.0f
            // MoveTo(self, family, Slow=1, 0, param_4, 0xe1f, 0, speed)  (disasm R8D=1, [RSP+0x28]=0xe1f)
            mainAction = Pf_MoveToCtor_9170(obj, familyIBP, 1 /*Slow; disasm LEA R8D,[R9+1]*/, 0, worldPos, 0xe1f, 0, speed);
        }
    } else {
        // disasm 0x1400bbd98..0x1400bbdf4 (alloc 0x48,8)
        obj = WAH_Alloc(0x48, 8);
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
            uint64_t timeVal = 0;
            RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), performWorkTime);  // System::Time::Time(&t, param_5/XMM1)
            float heading = WAH_RdataF(kRdata_1401722a8);       // XMM2 = [0x1401722a8] = -1000.0f
            // PerformWork_Ctor7(self, &entityInner, heading, actionType=8, time, p6=1, p7=-1)
            // disasm: R9D=0x8, XMM2=[0x1401722a8], [RSP+0x28]=1, [RSP+0x30]=0xffffffff, time=*Time()
            mainAction = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, 8, static_cast<int64_t>(timeVal), 1, -1);
        }
    }
    WAH_IbpCtor(&ibpA, mainAction);
    WAH_Append(list, &ibpA);

    // --- HuntAnimal action -> stored directly into list[*(int*)(list+0x18)] at +0x10 ---
    // disasm 0x1400bbe12..0x1400bbe73 (alloc 0x40,8). NOTE: NOT a c54e0 append; the
    // original writes [ *( list[idx] ) + 0x10 ] where idx = *(int*)(list+0x18) (= aiComp+0x60).
    obj = WAH_Alloc(0x40, 8);
    void* hunt = nullptr;
    if (obj) {
        uint64_t innerVal    = static_cast<uint64_t>(*targetInnerPtr);          // *param_3 -> local_res8
        uint64_t entityInner = *reinterpret_cast<uint64_t*>(citizenIBP);        // *param_1 -> local_48
        hunt = ActChild_HuntAnimalCopyCtor_c8400(obj, &entityInner, &innerVal);
    }
    WAH_IbpCtor(&ibpC, hunt);
    uintptr_t storedIbp = ibpC;  // local_50 captured before slot write
    // plVar8 = *(list) + *(int*)(list+0x18) * 8 ; CheckPtr(plVar8) ; *(*plVar8 + 0x10) = storedIbp
    int32_t idx = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(list) + 0x18);   // [RDI+0x18]
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(list);                                // [RDI]
    void* slot = reinterpret_cast<void*>(arrBase + static_cast<intptr_t>(idx) * 8);         // RDI = &arr[idx]
    *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(slot) + 0x10) = storedIbp;  // *(*plVar8+0x10)
    (void)ibpB;
}

// ============================================================
// WorkAct_ShowToolCtor3_bdd20 (0x1400bdd20)
// ============================================================
// Large work-queue builder: SendComponentMessage -> aiComp+0x58, CompleteTask, ShowTool(~Hoe),
// a per-element loop of (optional SendComponentMessage + CreateNaturalResource), then a final
// PerformWork_Ctor7, MoveTo, and ShowTool(Shovel=3). param_6/param_7/param_12 drive the loop.
//
//   param_1  = citizen InternalBasePtr*          [RCX -> R15, home RBP+0x40]
//   param_2  = workId (u32)                       [EDX -> EDI]
//   param_3  = dword* (CreateNaturalResource +0x20) [R8, home RBP+0x50]
//   param_4  = dword (unused; spilled local_res20) [R9D, home RBP+0x58]
//   param_5  = float* destination (MoveTo)         [RBP+0x60]
//   param_6  = int loop count                      [RBP+0x68]
//   param_7  = dword* loop base ptr (per-iter +4)  [RBP+0x70]
//   param_8  = dword CompleteTask param_5          [RBP+0x78]
//   param_9  = longlong* (per-iter *param_9 gate; SendComponentMessage entity; CreateNaturalResource +0x28) [RBP+0x80]
//   param_10 = dword* (SendComponentMessage msg id = *param_10)  [RBP+0x88]
//   param_11 = (unused)                            [RBP+0x90]
//   param_12 = longlong (loop tail: lVar15 = param_12 - param_7; per-iter [lVar15 + param_7]) [RBP+0x98]
void WorkAct_ShowToolCtor3_bdd20(void* citizenIBP, uint32_t workId, uint32_t* createParam3,
                                 uint32_t /*unused4*/, float* destination, int loopCount,
                                 uint32_t* loopBase, uint32_t completeParam8, int64_t* gatePtr9,
                                 uint32_t* msgIdPtr10, uint64_t /*unused11*/, int64_t param12) {

    // --- list base = (*(citizen)+0x278) + 0x48 ---  (disasm 0x1400bdd55..0x1400bdd79)
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);  // lVar2
    void* list = reinterpret_cast<void*>(aiComp + 0x48);           // RSI / plVar14
    WorkBase_TaskListCleanup_c5400(list);

    uintptr_t ibpA = 0, ibpB = 0;

    // --- SendComponentMessage #1 -> list+0x10 (aiComp+0x58), DeletePtr old first ---
    // disasm 0x1400bdd7e..0x1400bde0f (alloc 0x40,8)
    void* obj = WAH_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpA, sm);
    if (*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(list) + 0x10) != 0)
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(list) + 0x10))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(list) + 0x10) = ibpA;

    // --- CompleteTask action (param_5 = param_8) --- disasm 0x1400bde13..0x1400bde72 (alloc 0x38,8)
    obj = WAH_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, workId, &entityInner, &innerIbp, completeParam8);
    }
    WAH_IbpCtor(&ibpB, ct);
    WAH_Append(list, &ibpB);

    // --- ShowTool (~Hoe = -1) --- disasm 0x1400bde77..0x1400bdec3 (alloc 0x30,8; OR R8D,0xffffffff)
    obj = WAH_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
        st = ActChild_ShowToolCtor_42890(obj, &entityInner, -1 /* ~Hoe; disasm OR R8D,0xFFFFFFFF */);
    }
    WAH_IbpCtor(&ibpA, st);
    WAH_Append(list, &ibpA);

    // --- per-element loop: param_6 iterations over param_7 (+4 each) ---
    // disasm 0x1400bdec3..0x1400be1cc. lVar15 = param_12 - param_7(initial).
    if (loopCount > 0) {
        int64_t lVar15 = param12 - reinterpret_cast<int64_t>(loopBase);  // disasm SUB RDI,R12
        uint32_t* curBase = loopBase;                                    // R12
        int64_t remaining = loopCount;                                   // local_70
        do {
            // --- optional SendComponentMessage #2 when *param_9 != 0 --- (alloc 0x40,8)
            if (*gatePtr9 != 0) {
                obj = WAH_Alloc(0x40, 8);
                void* sm2 = nullptr;
                if (obj) {
                    uintptr_t innerIbp = 0;
                    WAH_IbpCtor(&innerIbp, nullptr);
                    uint32_t msgId = msgIdPtr10 ? *msgIdPtr10 : 0;       // *param_10
                    void* entityInner = reinterpret_cast<void*>(*gatePtr9);  // *param_9 -> local_78
                    // p6 = *(int*)(lVar15 + curBase)   (disasm MOV ECX,[RDI + R12])
                    uint32_t p6 = *reinterpret_cast<uint32_t*>(lVar15 + reinterpret_cast<int64_t>(curBase));
                    sm2 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, p6);
                }
                WAH_IbpCtor(&ibpB, sm2);
                WAH_Append(list, &ibpB);  // inlined grow-and-append in disasm (== FUN_1400c54e0)
            }

            // --- inline CreateNaturalResource action --- (alloc 0x38,8) disasm 0x1400be01c..0x1400be075
            obj = WAH_Alloc(0x38, 8);
            void* cnr = nullptr;
            if (obj) {
                uint64_t gateVal = static_cast<uint64_t>(*gatePtr9);   // lVar11 = *param_9
                uint32_t baseVal = *curBase;         // uVar1 = *param_7 (current)
                // the inlined CreateNaturalResource ctor (+0x20 param_3 raw, +0x28 *param_9,
                // +0x30 *param_7) — the real ctor.
                new (obj) CreateNaturalResource(reinterpret_cast<uint64_t>(createParam3), &gateVal, baseVal);
                cnr = obj;
            }
            WAH_IbpCtor(&ibpA, cnr);
            WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm (== FUN_1400c54e0)

            curBase = curBase + 1;     // param_7 += 1 (dword, i.e. +4 bytes)
            remaining = remaining - 1;
        } while (remaining != 0);
    }

    // --- final PerformWork_Ctor7 --- disasm 0x1400be1dd..0x1400be23f (alloc 0x48,8)
    obj = WAH_Alloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
        uint64_t timeVal = 0;
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&timeVal), kRdata_140172178);  // XMM1=[0x140172178]=2.1
        float heading = WAH_RdataF(kRdata_1401722a8);  // XMM2=[0x1401722a8]=-1000.0
        // PerformWork_Ctor7(self, &entityInner, heading, actionType=0xC, time, p6=1, p7=-1)
        // disasm: R9D=0xc, XMM2=[0x1401722a8], [RSP+0x28]=1, [RSP+0x30]=0xffffffff
        pw7 = Pf_PerformWorkCtor7_9720(obj, &entityInner, heading, 0xC, static_cast<int64_t>(timeVal), 1, -1);
    }
    WAH_IbpCtor(&ibpA, pw7);
    WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm

    // --- MoveTo (Fastest=3, destination=param_5) --- disasm 0x1400be38d..0x1400be40e (alloc 0x480,8)
    obj = WAH_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        uintptr_t famRet = 0;
        void* familyIBP = nullptr;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &famRet);
        float speed = kRdata_140172280;  // XMM0=[0x140172280]=-1.0
        // MoveTo(self, family, Fastest=3, 0, param_5, 0xe1f, 0, speed)  (disasm R8D=3, [RSP+0x28]=0xe1f)
        mv = Pf_MoveToCtor_9170(obj, familyIBP, 3 /*Fastest; disasm LEA R8D,[R9+3]*/, 0, destination, 0xe1f, 0, speed);
    }
    WAH_IbpCtor(&ibpA, mv);
    WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm

    // --- inline ShowTool (Shovel = 3) action --- disasm 0x1400be55d..0x1400be5a4
    // alloc 0x30,8 (disasm 0x1400be55d MOV EDX,0x8 / LEA ECX,[RDX+0x28] => 0x30)
    obj = WAH_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1 -> RBX
        // the inlined ShowTool ctor (+0x20 *param_1, +0x28 Shovel = 3) — the real ctor.
        new (obj) ShowTool(&entityInner, 3);
        st2 = obj;
    }
    WAH_IbpCtor(&ibpA, st2);
    WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm
    (void)ibpB;
}
