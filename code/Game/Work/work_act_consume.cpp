// code/Game/Work/work_act_consume.cpp
// Work action-queue builder for the Consume/Produce work task.
//
// 1:1 reverse-engineering port of FUN_1400bef70 @ 0x1400bef70 (Application-x64.exe)
// -> WorkAct_ConsumeProduceCtor_bef70. Referenced from consumeproduce.cpp
// (s_fn_SetupWorkChain). Transcribed literally from the Ghidra disassembly; no
#include "Work/exe_bodies.h"
// refactoring / "improvement" of behavior.
//
// Builds the citizen's action chain on the CitizenAIComponent task list
// (citizen+0x278 -> aiComp, list base aiComp+0x48). A SendComponentMessage action
// is stored at aiComp+0x58 (DeletePtr of the prior one first). The rest are appended
// to the list via TaskListAppend (0x1400c54e0) or the inlined array-append the
// original compiler emitted for the last two entries.
//
// Pattern follows work_base_actionctor.cpp: self-contained ResolveAll() resolving
// every sub-callee via ResolveAddr(Module::Exe, 0xADDR); WA-style helpers; vtable /
// float-const pointers. All statics prefixed s_wcp_* to avoid ODR clashes. No
// RegisterExeAddr.
//
// Every non-obvious offset/constant cites its disasm address.

#include <new>
#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "Action/Action.h"   // ENDGAME B.3.8 CB3
#include "Action/exe_bodies.h"      // B3R: the action ctor bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/Value.h"           // B3R: RtSystem_Time_Ctor
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include "Component/coll_bridge.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Component/ConsumeProduceDescription.h"   // ENDGAME B.3.9 D5

// ============================================================
// Sub-callees, called by name (B3R)
// ============================================================
// Every exe sub-callee is one of our own bodies (Action/, Work/, Component/ exe_bodies.h):
//   0x1400c5400 TaskListCleanup                       WorkBase_TaskListCleanup_c5400
//   0x1400c54e0 TaskListAppend                        Pf_ActStackPush_54e0
//   0x1400cb1d0 SendComponentMessage copy-ctor        Pf_SendComponentMessageCopyCtor_b1d0
//               (RCX=self, RDX=&entityInner, R8=&msgId, R9D=p4, [RSP+0x20]=&innerIbp, [RSP+0x28]=p6)
//   0x1400c5da0 CompleteTask copy-ctor                Pf_CompleteTaskCopyCtor_5da0
//   0x140042890 ShowTool ctor (R8D=toolType)          ActChild_ShowToolCtor_42890
//   0x1400c71a0 CreateRawMaterial copy-ctor           Pf_CreateRawMaterialCopyCtor_71a0
//               (disasm 0x1400bf12f..0x1400bf1a2: RCX=self, RDX=p2, XMM2=p3(float), R9=p4,
//                [RSP+0x20]=p5, [RSP+0x28]=p6, [RSP+0x30]=p7, [RSP+0x38]=p8, [RSP+0x40]=p9(u8),
//                [RSP+0x48]=p10, [RSP+0x50]=p11)
//   0x1400c9720 PerformWork_Action_Ctor7              Pf_PerformWorkCtor7_9720
//   0x1400c9170 MoveTo ctor                           Pf_MoveToCtor_9170
//   0x1400c6440 ConsumeRawMaterial_Action_Ctor3       ActChild_ConsumeRawCtor3_c6440
//   0x1400c64b0 consume-list append                   Entity_ConsumeListAppend_64b0
//   0x140006270 Entity_FindComponent_Storage          WorkBase_FindComponent_6270
//   0x1400012d0 Entity_FindComponent_Family           ActMv_FindComponentFamily_12d0
//   0x14000b6f0 Entity_FindStateMachine               Entity_FindConsumeProduce_b6f0
//   0x140119470 WeightedRandomIndex                   WeightedRandomIndex_119470
// 0x1400032e0 (task-list array grow) is Collection::Array<Ptr8>::SetSize, converted to
// GameColl::AsArray<GameColl::Ptr8> member calls in ENDGAME B.3.2 step 2.
// Dispatch slots: 0x140152b60 MemAlloc -> System::MemoryAllocator::MemAlloc;
// 0x140152b00 System::Time(float) -> RtSystem_Time_Ctor; 0x140152a98 IsDerivedFrom ->
// the member Object::Base::IsDerivedFrom.
// .rdata: 0x14017214c = 1.0f (CreateRawMaterial XMM2), 0x140172280 = -1.0f (MoveTo speed).

// ---- small WA-style helpers ----
static inline void* WCP_Alloc(uint64_t size) {
    // disasm: MOV EDX,8 (align); ECX=size; CALL [0x140152b60].
    return System::MemoryAllocator::MemAlloc(size, 8);
}
static inline void WCP_IbpCtor(void* ibp, void* o)  { new (ibp) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(o)); }   // B.3.3 bridge
static inline void WCP_Append(void* list, void* ibp){ Pf_ActStackPush_54e0(static_cast<int64_t*>(list), ibp); }

// Search a component list (base = listBase) for an element whose vtable GetTypeId
// (vtable+0x10) equals *typeId, then IsDerivedFrom; returns the matched component or
// null. Mirrors the two identical loops at 0x1400bf2a7.. and 0x1400bf3a4..
//   listBase: ptr to a struct with { +0x28 = element-ptr array base, +0x30 = count }.
static void* WCP_FindComponentDerived(uintptr_t listBase) {
    uint32_t typeId = ConsumeProduceDescription::TypeId();  // DWORD_1401c7388
    void* result = nullptr;
    int count = *reinterpret_cast<int*>(listBase + 0x30);        // [list+0x30]
    if (count > 0) {
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(listBase + 0x28);  // [list+0x28]
        int idx = 0;
        for (;;) {
            void* elem = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(idx) * 8);
            // slot 2 Type() (vtable +0x10) on a description-array element
            uint32_t tid = reinterpret_cast<Object::Base*>(elem)->Type();   // B.3.8 bridge
            if (tid == typeId) {
                void* comp = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(idx) * 8);
                if (reinterpret_cast<const Object::Base*>(comp)->IsDerivedFrom(typeId))   // B.3.x bridge (slot 0x140152a98)
                    result = comp;  // CMOVNZ RCX,RBX
                break;
            }
            ++idx;
            if (idx >= count) break;
        }
    }
    return result;
}

// Inlined task-list append the compiler emitted for the last two entries
// (0x1400bf5be.. and 0x1400bf69c..). Functionally identical to TaskListAppend.
//   list  = aiComp+0x48 ({ +0x00 array base, +0x08 capacity, +0x18 count }).
//   ibpVal = the InternalBasePtr value to store at array[count].
static void WCP_AppendInline(uintptr_t list, uintptr_t ibpVal) {
    // B.3.2 bridge: FUN_1400032e0 == Collection::Array<Ptr8>::SetSize
    // (ImmigrationComp_DynArrayResize in immigrationcomponent.cpp). It only touches
    // bytes [0x00,0x10) at `list` -- the array header embedded at the front of this
    // struct -- so this call is unaffected by whatever this struct keeps at +0x18.
    int cap = *reinterpret_cast<int*>(list + 0x8);        // [RSI+0x8]
    if (cap == 0) {
        cap = 0x10;
        GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(list)).SetSize(cap);
    } else {
        int n = *reinterpret_cast<int*>(list + 0x18) + 1; // [RSI+0x18]+1
        if (n == cap) {
            cap *= 2;
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(list)).SetSize(cap);
        }
    }

    // Cleanup walk over the chain at array[count] (clear flag +0x18, call vtable+0x38).
    int count = *reinterpret_cast<int*>(list + 0x18);     // [RSI+0x18] (signed)
    if (count >= 0) {
        uintptr_t base = *reinterpret_cast<uintptr_t*>(list);  // [RSI]
        uintptr_t node = *reinterpret_cast<uintptr_t*>(base + static_cast<uintptr_t>(count) * 8);
        while (node != 0) {
            if (*reinterpret_cast<uint8_t*>(node + 0x18) != 0) {
                *reinterpret_cast<uint8_t*>(node + 0x18) = 0;
                // slot 7 (vtable +0x38) Action::Exit(list) on an action-stack node
                reinterpret_cast<Action*>(node)->Exit(reinterpret_cast<void*>(list));   // B.3.8 bridge
            }
            node = *reinterpret_cast<uintptr_t*>(node + 0x10);
        }
    }

    // count++; array[count] = ibpVal.
    int newCount = *reinterpret_cast<int*>(list + 0x18) + 1;  // INC [RSI+0x18]
    *reinterpret_cast<int*>(list + 0x18) = newCount;
    uintptr_t base = *reinterpret_cast<uintptr_t*>(list);
    *reinterpret_cast<uintptr_t*>(base + static_cast<uintptr_t>(newCount) * 8) = ibpVal;
}

// ============================================================
// WorkAct_ConsumeProduceCtor_bef70 (FUN_1400bef70)
// ============================================================
// Parameters (Win64 + stack), matching consumeproduce.cpp's s_fn_SetupWorkChain_t:
//   param_1  RCX   citizen InternalBasePtr*
//   param_2  EDX   priority/workId (u32)
//   param_3  R8    entity InternalBasePtr* (the work target / raw-material source)
//   param_4  R9    float* destPos (MoveTo destination)
//   param_5  [+0x28] float  heading (PerformWork XMM2)
//   param_6  [+0x30] longlong  component-list base for the 2nd search loop
//   param_7  [+0x38] u64    productType (CreateRawMaterial p2)
//   param_8  [+0x40] u32    flags (low byte = "haul/produce" enable)
//   param_9  [+0x48] int*   -> if *param_9 != 0, emit optional SendComponentMessage #2
//   param_10 [+0x50] u32    SendComponentMessage #2 p4
//   param_11 [+0x58] u32    CreateRawMaterial p8
//   param_12 [+0x60] float  produceTime (Time::Time for PerformWork)
//   param_13 [+0x68] u32    CompleteTask p5
void WorkAct_ConsumeProduceCtor_bef70(void* param_1, uint32_t param_2, void* param_3,
                                      float* param_4, float param_5, int64_t param_6,
                                      uint64_t param_7, uint32_t param_8, int* param_9,
                                      uint32_t param_10, uint32_t param_11, float param_12,
                                      uint32_t param_13) {

    // list base = (*(citizen)+0x278) + 0x48  (0x1400befaf..0x1400befd1)
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(param_1);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    uintptr_t list = aiComp + 0x48;          // RSI / plVar15
    WorkBase_TaskListCleanup_c5400(reinterpret_cast<void*>(list));

    void* obj;

    // --- SendComponentMessage #1 -> stored at aiComp+0x58 (DeletePtr old first) (0x1400befd6..) ---
    obj = WCP_Alloc(0x40);                    // MOV EDX,8; LEA ECX,[RDX+0x38] => 0x40
    void* sm1 = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WCP_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);
        sm1 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerIbp, 0);
    }
    uintptr_t ibpSm1 = 0;
    WCP_IbpCtor(&ibpSm1, sm1);                // ctor(&local_60, sm1)
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)  // [lVar2+0x58]
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(aiComp + 0x58))->DeletePtr(
            );   // B.3.3 bridge
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpSm1;

    // --- CompleteTask action (0x1400bf06b..) ---
    obj = WCP_Alloc(0x38);                    // LEA ECX,[RDX+0x30] => 0x38
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WCP_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);     // *param_1
        ct = Pf_CompleteTaskCopyCtor_5da0(obj, param_2, &entityInner, &innerIbp, param_13);
    }
    uintptr_t ibpCt = 0;
    WCP_IbpCtor(&ibpCt, ct);
    WCP_Append(reinterpret_cast<void*>(list), &ibpCt);   // FUN_1400c54e0(plVar15, &local_70)

    // cVar4 = (char)param_8  (0x1400bf0d2)
    uint8_t cVar4 = static_cast<uint8_t>(param_8);

    if (cVar4 != 0) {
        // --- CreateRawMaterial action gated on the storage slot (0x1400bf0e0..) ---
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);  // *(*param_1+0x10)
        uintptr_t storRet = 0;
        void* storIbp = nullptr;
        storIbp = WorkBase_FindComponent_6270(static_cast<int64_t>(entityInner), &storRet);
        uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(storIbp) + 0x38);  // *(*pIVar11+0x38) -> EDI

        obj = WCP_Alloc(0x58);                // LEA ECX,[RDX+0x50] => 0x58
        void* crm = nullptr;
        if (obj) {
            uintptr_t local60 = 0;
            WCP_IbpCtor(&local60, nullptr);   // ctor(&local_60, 0)  (p10)
            uint64_t local70 = *reinterpret_cast<uintptr_t*>(param_1);                 // *param_1  -> p4
            uint64_t local68 = *reinterpret_cast<uintptr_t*>(local70 + 0x248);         // *(*param_1+0x248) -> p5
            float xmm2 = kRdata_14017214c;                    // [0x14017214c]==1.0f (g_fUVScale)
            // p6 = leftover 0 in [RSP+0x28]; p7=uVar1; p8=param_11; p9=0; p10=&local60; p11=4.
            crm = Pf_CreateRawMaterialCopyCtor_71a0(obj, param_7, xmm2, &local70, &local68,
                                         0, uVar1, param_11, 0, &local60, 4);
        }
        uintptr_t ibpCrm = 0;
        WCP_IbpCtor(&ibpCrm, crm);
        WCP_Append(reinterpret_cast<void*>(list), &ibpCrm);

        // --- optional SendComponentMessage #2 when *param_9 != 0 (0x1400bf1c5..) ---
        if (param_9 && static_cast<uint32_t>(*param_9) != 0) {  // CMP [RDI],0; JBE (unsigned)
            obj = WCP_Alloc(0x40);            // LEA ECX,[RDX+0x38] => 0x40
            void* sm2 = nullptr;
            if (obj) {
                uintptr_t innerIbp = 0;
                WCP_IbpCtor(&innerIbp, nullptr);
                uint32_t msgId = static_cast<uint32_t>(*param_9);                       // *param_9
                uintptr_t e2 = *reinterpret_cast<uintptr_t*>(param_3);                  // *param_3 (R13)
                sm2 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&e2), &msgId, param_10, &innerIbp, 0);
            }
            uintptr_t ibpSm2 = 0;
            WCP_IbpCtor(&ibpSm2, sm2);
            WCP_Append(reinterpret_cast<void*>(list), &ibpSm2);
        }
    }

    // --- ShowTool (~Hoe = -1) action (0x1400bf247..) ---
    obj = WCP_Alloc(0x30);                    // LEA ECX,[RDX+0x28] => 0x30
    void* st1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);  // *param_1
        // disasm: OR R8D,0xffffffff => toolType = -1 (~Hoe, Hoe==0).
        st1 = ActChild_ShowToolCtor_42890(obj, &entityInner, -1);
    }
    uintptr_t ibpSt1 = 0;
    WCP_IbpCtor(&ibpSt1, st1);
    WCP_Append(reinterpret_cast<void*>(list), &ibpSt1);

    // --- 1st component search over *(*param_3+0x28); derive toolType (0x1400bf293..) ---
    uintptr_t listBase1 = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_3) + 0x28);  // *(*param_3+0x28)
    void* foundComp1 = WCP_FindComponentDerived(listBase1);
    // toolType = *(foundComp+0x50); if null component, reads *(0+0x50) — but original uses
    // RCX=R14(0) when none found => *(0x50). Match faithfully via the found result (0 -> deref guard).
    uint32_t toolType = foundComp1 ? *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(foundComp1) + 0x50)
                                   : *reinterpret_cast<uint32_t*>(static_cast<uintptr_t>(0x50));
    // actionType = (toolType <= 0xa) ? toolType+9 : 8   (0x1400bf307..0x1400bf312)
    int actionType = 8;
    if (toolType <= 0xa) actionType = static_cast<int>(toolType) + 9;

    // --- PerformWork_Ctor7 (0x1400bf316..) ---
    obj = WCP_Alloc(0x48);                    // LEA ECX,[RDX+0x40] => 0x48
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);  // *param_1
        uint64_t timeRes[2] = {0, 0};
        RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(timeRes), param_12);           // Time::Time(&local_70, param_12)
        int64_t time = static_cast<int64_t>(timeRes[0]);                 // *puVar12
        // heading=param_5 (XMM2), actionType=EDI (R9D), p6=1, p7=-1.
        pw7 = Pf_PerformWorkCtor7_9720(obj, &entityInner, param_5, actionType, time, 1, -1);
    }
    uintptr_t ibpPw7 = 0;
    WCP_IbpCtor(&ibpPw7, pw7);
    WCP_Append(reinterpret_cast<void*>(list), &ibpPw7);

    // --- consume-list build (0x1400bf394..) gated on cVar4 (R12B) ---
    if (cVar4 != 0) {
        // 2nd component search over param_6.
        void* foundComp2 = WCP_FindComponentDerived(static_cast<uintptr_t>(param_6));

        // state machine + weighted random recipe index.
        uintptr_t smRet = 0;
        Entity_FindConsumeProduce_b6f0(*reinterpret_cast<uintptr_t*>(param_3), &smRet);  // *param_3
        uint64_t widx = WeightedRandomIndex_119470(static_cast<int64_t>(smRet));
        int64_t idx = static_cast<int64_t>(static_cast<int>(widx));
        int64_t r13 = idx + idx * 4;        // LEA R13,[RDX+RDX*4] => idx*5

        // recipe array base = *(foundComp2+0x28); count at [base + r13*8 + 8] (= base + idx*0x28 + 8)
        uintptr_t recBase = foundComp2 ? *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(foundComp2) + 0x28)
                                       : *reinterpret_cast<uintptr_t*>(static_cast<uintptr_t>(0x28));
        if (*reinterpret_cast<int*>(recBase + static_cast<uintptr_t>(r13) * 8 + 8) > 0) {
            // ConsumeRawMaterial_Action_Ctor3(self, &(*param_3)) (0x1400bf446..)
            obj = WCP_Alloc(0x38);            // LEA ECX,[RDX+0x30] => 0x38
            void* crmAct = nullptr;
            if (obj) {
                uint64_t inner = *reinterpret_cast<uintptr_t*>(param_3);   // *param_3
                crmAct = ActChild_ConsumeRawCtor3_c6440(obj, &inner);
            }
            uintptr_t consumeIbp = 0;         // stored at [RBP+0x57]
            WCP_IbpCtor(&consumeIbp, crmAct);
            uintptr_t appendCopy = consumeIbp;  // local_60 = local_res18
            WCP_Append(reinterpret_cast<void*>(list), &appendCopy);

            // inner loop: append each (resType,count) pair to the consume list (0x1400bf496..)
            int count = *reinterpret_cast<int*>(recBase + static_cast<uintptr_t>(r13) * 8 + 8);
            if (count > 0) {
                uintptr_t subArr = *reinterpret_cast<uintptr_t*>(recBase + static_cast<uintptr_t>(r13) * 8);  // *(recBase + idx*0x28)
                for (int j = 0; j < count; ++j) {
                    int64_t resType = *reinterpret_cast<int64_t*>(subArr + static_cast<uintptr_t>(j) * 0x10);      // [subArr + j*0x10]
                    int cnt = *reinterpret_cast<int*>(subArr + static_cast<uintptr_t>(j) * 0x10 + 8);             // [subArr + j*0x10 + 8]
                    Entity_ConsumeListAppend_64b0(static_cast<int64_t>(consumeIbp), resType, cnt);
                    // re-read count each iter (CMP [RSI+R13*8+8]); recBase==RSI here.
                    count = *reinterpret_cast<int*>(recBase + static_cast<uintptr_t>(r13) * 8 + 8);
                }
            }
        }
        // toolType reload (MOV R15D,[RBP+0x7f]) — value unchanged from above.
    }

    // --- MoveTo action with path-type switch on (toolType+1) (0x1400bf4ed..) ---
    obj = WCP_Alloc(0x480);                   // MOV ECX,0x480 => 0x480
    void* mv = nullptr;
    if (obj) {
        // Jump table @0x1400bf758 (read_memory verified), index = toolType+1,
        // default(>8 unsigned)=Slow(1). Each entry (base 0x140000000):
        //   idx0->0x1400bf53a(EBX=1,Slow) idx1->bf52c(2,Fast) idx2->bf52c(2)
        //   idx3->bf533(3,Fastest) idx4->bf533(3) idx5->bf52c(2) idx6->bf52c(2)
        //   idx7->bf52c(2) idx8->bf52c(2)
        int pathType;
        uint32_t sw = toolType + 1;
        if (sw > 8) {
            pathType = 1;                     // default Slow (CMP ECX,8; JA -> MOV EBX,1)
        } else {
            switch (sw) {
                case 1: case 2: case 5: case 6: case 7: case 8: pathType = 2; break;  // Fast
                case 3: case 4:                                 pathType = 3; break;  // Fastest
                default:                                        pathType = 1; break;  // case 0 -> Slow
            }
        }
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);  // *(*param_1+0x10)
        uintptr_t famRet = 0;
        void* familyIbp = nullptr;
        familyIbp = ActMv_FindComponentFamily_12d0(static_cast<int64_t>(entityInner), &famRet);
        float speed = kRdata_140172280;          // [0x140172280]==-1.0f
        mv = Pf_MoveToCtor_9170(obj, familyIbp, pathType, 0, param_4, 0xe1f, 0, speed);
    }
    uintptr_t ibpMv = 0;
    WCP_IbpCtor(&ibpMv, mv);                   // ctor(&local_58, mv)
    WCP_AppendInline(list, ibpMv);             // inlined append (0x1400bf5be..)

    // --- final ShowTool (toolType) action (0x1400bf65c..) ---
    obj = WCP_Alloc(0x30);                     // LEA ECX,[RDX+0x28] => 0x30
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);  // *param_1 (R12)
        st2 = ActChild_ShowToolCtor_42890(obj, &entityInner, static_cast<int>(toolType));
    }
    uintptr_t ibpSt2 = 0;
    WCP_IbpCtor(&ibpSt2, st2);                 // ctor(&local_res18, st2)
    WCP_AppendInline(list, ibpSt2);            // inlined append (0x1400bf69c..)
}
