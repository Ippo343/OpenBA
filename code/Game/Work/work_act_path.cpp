// code/Game/Work/work_act_path.cpp
// WorkAct_GatherRawMatCtor_bf790 — 1:1 port of exe Pathfinding_FindPath @ 0x1400bf790.
//
// Despite the Ghidra name this is NOT pathfinding: it is a citizen work-action-queue
// builder (~0x835 bytes) that constructs a pickup/dropoff/move action chain and stores
// it on the citizen's CitizenAIComponent task list (entity+0x278, list base +0x48). It
// is the "gather raw material" task builder: walk to a source, optional pickup, drop off
#include "Work/exe_bodies.h"
// raw material at a destination, do work, move on. SendComponentMessage is stored at
// aiComp+0x58 (the prior one DeletePtr'd first).
//
// Instruction-faithful transcription of the decompiled exe helper; do NOT "improve"
// behavior. Every exe sub-callee is resolved via ResolveAddr(Module::Exe, 0xADDR) and
// null-guarded (skipped if unregistered, matching project semantics). Self-contained:
// own ResolveAll(), no RegisterExeAddr. Statics prefixed s_wap2_ to avoid
// ODR clashes with sibling action-ctor ports.
//
// All disasm citations are exe addresses (base 0x140000000).

#include <new>
#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include "Action/PerformWork.h"   // ENDGAME B.3.8 CB3
#include "Action/exe_bodies.h"      // B3R: the action ctor bodies called by name
#include "Game/exe_constants.h"     // B3R: kRdata_*
#include "system/Value.h"           // B3R: RtSystem_Time_Ctor
#include "object/Internal.h"
#include "Component/CitizenAIComponent.h"   // ENDGAME B.3.9 CB: kCitizenAIMessageTargetTypeId
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Component/CitizenComponent.h"   // ENDGAME B.3.9 D5

// ============================================================
// Sub-callees, called by name (B3R)
// ============================================================
// Every exe sub-callee is one of our own bodies (Action/, Work/ exe_bodies.h):
//   0x1400c5400 TaskListCleanup                     WorkBase_TaskListCleanup_c5400 (0xbf7f3)
//   0x1400c54e0 TaskListAppend                      Pf_ActStackPush_54e0 (0xbf9f5 etc.)
//   0x1400c29a0 Entity_impl_29a0 (heading, XMM0)    ActChild_Entity29a0_c29a0
//   0x1400012d0 Entity_FindComponent_Family         ActMv_FindComponentFamily_12d0
//   0x1400cb1d0 SendComponentMessage copy-ctor      Pf_SendComponentMessageCopyCtor_b1d0
//   0x1400c5da0 CompleteTask copy-ctor              Pf_CompleteTaskCopyCtor_5da0
//   0x140042890 ShowTool ctor (R8D=toolType)        ActChild_ShowToolCtor_42890
//   0x1400c7cd0 DropOffRawMaterial copy-ctor        ActChild_DropOffRawCopyCtor_c7cd0
//   0x1400ca520 PickupStoredRawMaterial copy-ctor   ActChild_PickupStoredCopyCtor_ca520
//   0x1400c9720 PerformWork_Action_Ctor7            Pf_PerformWorkCtor7_9720
//   0x1400c9170 MoveTo ctor                         Pf_MoveToCtor_9170
// Dispatch slots:
//   0x140152b60 MemAlloc             -> System::MemoryAllocator::MemAlloc
//   0x140152a90 InternalBasePtr ctor -> placement-new Object::InternalBasePtr
//   0x140152b00 System::Time(float)  -> RtSystem_Time_Ctor
//   PORT GAP 0x140151ab8 InternalBasePtr::DeletePtr: the import slot is not filled; the
//   fallback the reader took (the member Object::InternalBasePtr::DeletePtr) is the code.
// .rdata: 0x140172124 = 0.75f (0xbf962), 0x140172280 = -1.0f MoveTo speed (0xbfbb6).

static inline void* WAP2_Alloc(uint64_t size, uint32_t align) {
    return System::MemoryAllocator::MemAlloc(size, static_cast<int>(align));
}
static inline void  WAP2_IbpCtor(void* ibp, void* obj) {
    new (ibp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(obj));   // B.3.3 bridge
}
static inline void  WAP2_Append(void* list, void* ibp) { Pf_ActStackPush_54e0(static_cast<int64_t*>(list), ibp); }

// Read a runtime Time qword via System::Time::Time(&buf, seconds); the original reads
// the first qword of the returned Time and forwards it as the action's "time" field.
static inline int64_t WAP2_MakeTime(float seconds) {
    uint64_t buf = 0;
    void* r = RtSystem_Time_Ctor(reinterpret_cast<System::Time*>(&buf), seconds);
    if (r) return static_cast<int64_t>(*reinterpret_cast<uint64_t*>(r));
    return 0;
}

// ============================================================
// WorkAct_GatherRawMatCtor_bf790  (exe Pathfinding_FindPath @ 0x1400bf790)
// ============================================================
//   retval         = InternalBasePtr* hidden return (RCX) — written with the Pickup IBP.
//   entity         = citizen InternalBasePtr* (RDX)
//   pathType       = u32 (R8D, homed; reused as CompleteTask workId)
//   srcLoc         = u64* (R9 — *srcLoc = source inner)
//   srcPos         = float* (stack; DropOff/MoveTo source position / first heading)
//   dstLoc         = void** (stack; *dstLoc = dest inner — Pickup target)
//   dstPos         = float* (stack; second heading / second MoveTo destination)
//   materialDesc   = int64_t (stack; DropOff/Pickup p4)
//   matCount       = int (stack; DropOff/Pickup p5)
//   completionCode = int (stack; CompleteTask completionCode)
//   useAnimal      = byte (stack; Pickup p6)
//   twoWay         = char (stack; gates pre-CompleteTask + post pickup CompleteTask)
//   flags          = int (stack; low byte gates the optional srcLoc SendComponentMessage)
//   param_14       = byte (stack; PerformWork p6 + selects path/tool variants)
extern "C" void* WorkAct_GatherRawMatCtor_bf790(
        void* retval, void* entity, uint32_t pathType, uint64_t* srcLoc,
        float* srcPos, void** dstLoc, float* dstPos, int64_t materialDesc,
        int matCount, int completionCode, uint8_t useAnimal, char twoWay,
        int flags, uint8_t param_14) {

    // --- list base = (*(entity)+0x278) + 0x48 ; cleanup pending actions (0xbf7cf-0xbf7f3) ---
    uintptr_t ent = *reinterpret_cast<uintptr_t*>(entity);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ent + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    WorkBase_TaskListCleanup_c5400(list);

    // Temporaries (mirror the original's stack IBP scratch slots).
    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0, ibpInner = 0;
    void* obj = nullptr;

    // ============================================================
    // SendComponentMessage #1  → stored at aiComp+0x58 (0xbf7f8-0xbf885)
    // ============================================================
    obj = WAP2_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        ibpInner = 0;
        WAP2_IbpCtor(&ibpInner, nullptr);
        uint32_t msgId = kCitizenAIMessageTargetTypeId;  // PORT GAP: CitizenAIComponent::OnMessage (see CitizenAIComponent.h)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(entity) + 0x10);
        sm = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &ibpInner, 0);
    }
    WAP2_IbpCtor(&ibpA, sm);                                            // (0xbf86a)
    // SendComponentMessage handle slot = R14+0x10, where R14 = list = aiComp+0x48,
    // i.e. aiComp+0x58. DeletePtr the prior handle, then store. (0xbf870-0xbf885)
    uintptr_t smSlot = reinterpret_cast<uintptr_t>(list) + 0x10;        // == aiComp+0x58
    if (*reinterpret_cast<uintptr_t*>(smSlot) != 0)   // PORT GAP 0x140151ab8: the fallback member DeletePtr is the code
        reinterpret_cast< ::Object::InternalBasePtr*>(smSlot)->DeletePtr();   // B.3.3 bridge              // (0xbf877-0xbf87b)
    *reinterpret_cast<uintptr_t*>(smSlot) = ibpA;                       // (0xbf885)

    // ============================================================
    // optional SendComponentMessage #2 when (char)flags != 0  (0xbf889-0xbf914)
    //   msgId = DWORD_1401c7158 ; innerIbp slot = *srcLoc
    // ============================================================
    if (static_cast<char>(flags) != 0) {
        obj = WAP2_Alloc(0x40, 8);
        void* sm2 = nullptr;
        if (obj) {
            uint64_t innerSlot = srcLoc ? *srcLoc : 0;                  // local_res8 = *srcLoc (0xbf8b0)
            uint32_t msgId = CitizenComponent::TypeId(); // DWORD_1401c7158 (0xbf8b8)
            void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(entity) + 0x10);
            sm2 = Pf_SendComponentMessageCopyCtor_b1d0(obj, reinterpret_cast<uint64_t*>(&entityInner), &msgId, 0, &innerSlot, 0);
        }
        WAP2_IbpCtor(&ibpC, sm2);                                       // (0xbf907)
        WAP2_Append(list, &ibpC);                                      // (0xbf914)
    }

    // ============================================================
    // headings: Entity_impl_29a0(srcPos, &*srcLoc) and (dstPos, &*dstLoc) (0xbf919-0xbf94d)
    // ============================================================
    uint64_t headSlot;
    float headingSrc = 0.0f;
    headSlot = srcLoc ? *srcLoc : 0;                                   // local_res8 = *srcLoc
    headingSrc = ActChild_Entity29a0_c29a0(srcPos, &headSlot); // XMM6 (0xbf929)
    float headingDst = 0.0f;
    headSlot = (dstLoc) ? reinterpret_cast<uint64_t>(*dstLoc) : 0;     // local_res8 = *dstLoc (0xbf931)
    headingDst = ActChild_Entity29a0_c29a0(dstPos, &headSlot); // XMM8 (0xbf944)

    const float kTime075 = kRdata_140172124; // DAT_140172124 = 0.75f (0xbf962)

    // ============================================================
    // inline PerformWork #1 (heading=headingSrc, actionType=7, time=0.75) (0xbf94d-0xbf9f5)
    // ============================================================
    obj = WAP2_Alloc(0x48, 8);
    void* pw1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);  // *(entity) (0xbf96f)
        int64_t timeVal = WAP2_MakeTime(kTime075);                      // System::Time::Time(_,0.75) (0xbf97d)
        // (0xbf989..0xbf9d4) the inlined PerformWork ctor 0x1400c9720 with useTool = 0,
        // +0x44 = -1 — the real ctor.
        new (obj) PerformWork(&entityInner, headingSrc, 7, timeVal, 0, -1);
        pw1 = obj;
    }
    WAP2_IbpCtor(&ibpB, pw1);
    WAP2_Append(list, &ibpB);                                          // (0xbf9f5)

    // ============================================================
    // ShowTool #1 (~Hoe = -1; OR R8D,0xffffffff) (0xbf9fa-0xbfa41)
    // ============================================================
    obj = WAP2_Alloc(0x30, 8);                                         // size 0x30 (0xbf9ff LEA ECX,[RDX+0x28]; ECX=8+0x28)
    void* st1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);
        st1 = ActChild_ShowToolCtor_42890(obj, &entityInner, -1); // R8D = -1 (0xbfa18)
    }
    WAP2_IbpCtor(&ibpB, st1);
    WAP2_Append(list, &ibpB);                                          // (0xbfa41)

    // ============================================================
    // pre-CompleteTask when twoWay == 0 (0xbfa46-0xbfab7)
    //   workId = pathType (homed slot), completionCode = completionCode
    // ============================================================
    if (twoWay == 0) {
        obj = WAP2_Alloc(0x38, 8);
        void* ct = nullptr;
        if (obj) {
            ibpInner = 0;
            WAP2_IbpCtor(&ibpInner, nullptr);                          // (0xbfa6f)
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity); // *(entity) (0xbfa75)
            ct = Pf_CompleteTaskCopyCtor_5da0(obj, pathType, &entityInner, &ibpInner,
                                         static_cast<uint32_t>(completionCode)); // (0xbfa94)
        }
        WAP2_IbpCtor(&ibpC, ct);
        WAP2_Append(list, &ibpC);                                      // (0xbfab2)
    }

    // ============================================================
    // DropOffRawMaterial (0xbfab7-0xbfb1c)
    //   p2 = &*entity (citizen, 0xbfad6→RDX@0xbfaf7), p3 = &*srcLoc (dropoff
    //   target, 0xbface→R8@0xbfaf3), p4 = materialDesc, p5 = matCount, p6 = 1
    //   (disasm-verified 2026-07-20; an earlier comment here had p2/p3 swapped)
    // ============================================================
    obj = WAP2_Alloc(0x40, 8);                                         // size 0x40 (0xbfabc LEA ECX,[RDX+0x38])
    void* drop = nullptr;
    if (obj) {
        uint64_t srcInner = srcLoc ? *srcLoc : 0;                      // local_res8 = *srcLoc (0xbface)
        uint64_t entInner = *reinterpret_cast<uint64_t*>(entity);      // local_res10 = *entity (0xbfad6)
        drop = ActChild_DropOffRawCopyCtor_c7cd0(obj, &entInner, &srcInner,
                                  static_cast<uint64_t>(materialDesc),
                                  static_cast<uint32_t>(matCount), 1);  // p6=1 (0xbfadd)
    }
    WAP2_IbpCtor(&ibpC, drop);
    WAP2_Append(list, &ibpC);                                          // (0xbfb1c)

    // ============================================================
    // PerformWork_Ctor7 #1 (heading=headingSrc, actionType=6, time=0.75,
    //   p6=param_14, p7=-1) (0xbfb21-0xbfb9a)
    // ============================================================
    obj = WAP2_Alloc(0x48, 8);                                         // size 0x48 (0xbfb26 LEA ECX,[RDX+0x40])
    void* pw7a = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);
        int64_t timeVal = WAP2_MakeTime(kTime075);                      // (0xbfb51)
        pw7a = Pf_PerformWorkCtor7_9720(obj, &entityInner, headingSrc, 6 /*R9D=6*/,
                                       timeVal, param_14 /*R12B*/, -1 /*[RSP+0x30]=-1*/); // (0xbfb7c)
    }
    WAP2_IbpCtor(&ibpA, pw7a);
    WAP2_Append(list, &ibpA);                                          // (0xbfb9a)

    // ============================================================
    // MoveTo #1 (destination=srcPos, pathType = (param_14!=0)?5:4) (0xbfb9f-0xbfc40)
    // ============================================================
    const float kSpeed = kRdata_140172280;   // DAT_140172280 = -1.0f (0xbfbb6)
    obj = WAP2_Alloc(0x480, 8);
    void* mv1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(
            *reinterpret_cast<uintptr_t*>(entity) + 0x10);             // *((*entity)+0x10) (0xbfbcc-0xbfbd7)
        int pathT = 4 + (param_14 != 0 ? 1 : 0);                       // LEA R8D,[RBX+4], RBX=SETNZ (0xbfbe7-0xbfc1b)
        void* familyIBP = nullptr;
        ibpInner = 0;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &ibpInner); // (0xbfbf5)
        mv1 = Pf_MoveToCtor_9170(obj, familyIBP, pathT, 0, srcPos, 0xe1f, 0, kSpeed); // (0xbfc22)
    }
    WAP2_IbpCtor(&ibpB, mv1);
    WAP2_Append(list, &ibpB);                                          // (0xbfc40)

    // ============================================================
    // inline PerformWork #2 (heading=headingDst, actionType=7, time=0.75) (0xbfc45-0xbfce5)
    // ============================================================
    obj = WAP2_Alloc(0x48, 8);                                         // size 0x48 (0xbfc4a LEA ECX,[RDX+0x40])
    void* pw2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);  // *(entity) (0xbfc5f)
        int64_t timeVal = WAP2_MakeTime(kTime075);                      // (0xbfc6d)
        // (0xbfc79..0xbfcc4) the inlined PerformWork ctor 0x1400c9720 with useTool = 0,
        // +0x44 = -1 — the real ctor.
        new (obj) PerformWork(&entityInner, headingDst, 7, timeVal, 0, -1);
        pw2 = obj;
    }
    WAP2_IbpCtor(&ibpB, pw2);
    WAP2_Append(list, &ibpB);                                          // (0xbfce5)

    // ============================================================
    // ShowTool #2 (param_14!=0 ? 0xa : -2) (0xbfcea-0xbfd3f)
    // ============================================================
    obj = WAP2_Alloc(0x30, 8);                                         // size 0x30 (0xbfcef LEA ECX,[RDX+0x28])
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);
        int toolType = (param_14 != 0) ? 0xa : -2;                     // MOV R8D,-2; CMOVNZ R8D,0xa (0xbfd0d-0xbfd16)
        st2 = ActChild_ShowToolCtor_42890(obj, &entityInner, toolType);
    }
    WAP2_IbpCtor(&ibpB, st2);
    WAP2_Append(list, &ibpB);                                          // (0xbfd3f)

    // ============================================================
    // PickupStoredRawMaterial → written to retval (0xbfd44-0xbfdb9)
    //   p2 = &*entity, p3 = &*dstLoc, p4 = materialDesc, p5 = matCount, p6 = useAnimal
    // ============================================================
    obj = WAP2_Alloc(0x48, 8);                                         // size 0x48 (0xbfd44 LEA ECX,[RDX+0x40])
    void* pick = nullptr;
    if (obj) {
        uint64_t dstInner = (dstLoc) ? reinterpret_cast<uint64_t>(*dstLoc) : 0; // *dstLoc (0xbfd5b)
        uint64_t entInner = *reinterpret_cast<uint64_t*>(entity);              // *(entity) (0xbfd66)
        pick = ActChild_PickupStoredCopyCtor_ca520(obj, &entInner, &dstInner,
                                 static_cast<int64_t>(materialDesc),
                                 matCount, useAnimal);                  // p6=useAnimal (0xbfd94)
    }
    WAP2_IbpCtor(retval, pick);                                        // ctor into hidden retval (0xbfda4)
    uintptr_t retInner = *reinterpret_cast<uintptr_t*>(retval);        // dstLoc = *retval (0xbfdaa)
    WAP2_Append(list, &retInner);                                     // (0xbfdb9)

    // ============================================================
    // post-Pickup CompleteTask when twoWay != 0 (0xbfdbe-0xbfe2a)
    // ============================================================
    if (twoWay != 0) {
        obj = WAP2_Alloc(0x38, 8);                                     // size 0x38 (0xbfdcc LEA ECX,[RDX+0x30])
        void* ct = nullptr;
        if (obj) {
            ibpInner = 0;
            WAP2_IbpCtor(&ibpInner, nullptr);                          // (0xbfde7)
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);
            ct = Pf_CompleteTaskCopyCtor_5da0(obj, pathType, &entityInner, &ibpInner,
                                         static_cast<uint32_t>(completionCode)); // R8=&entityInner@0xbfe02, R9=&ibpInner@0xbfdfe
        }
        WAP2_IbpCtor(&ibpB, ct);
        WAP2_Append(list, &ibpB);                                      // (0xbfe2a)
    }

    // ============================================================
    // PerformWork_Ctor7 #2 (heading=headingDst, actionType=6, time=0.75,
    //   p6=param_14, p7=-1) (0xbfe2f-0xbfea1)
    // ============================================================
    obj = WAP2_Alloc(0x48, 8);                                         // size 0x48 (0xbfe34 LEA ECX,[RDX+0x40])
    void* pw7b = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);  // *(entity) (0xbfe49)
        int64_t timeVal = WAP2_MakeTime(kTime075);                      // (0xbfe57)
        pw7b = Pf_PerformWorkCtor7_9720(obj, &entityInner, headingDst, 6 /*R9D=6*/,
                                       timeVal, param_14 /*R12B*/, -1); // (0xbfe83)
    }
    WAP2_IbpCtor(&ibpA, pw7b);
    WAP2_Append(list, &ibpA);                                          // (0xbfea1)

    // ============================================================
    // MoveTo #2 (destination=dstPos, pathType = (param_14!=0)?5:1) (0xbfea6-0xbff46)
    // ============================================================
    obj = WAP2_Alloc(0x480, 8);
    void* mv2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(
            *reinterpret_cast<uintptr_t*>(entity) + 0x10);             // *((*entity)+0x10) (0xbfecb-0xbfed6)
        int pathT = (param_14 != 0) ? 5 : 1;                           // MOV EBX,1; MOV EAX,5; CMOVNZ (0xbfee4-0xbfef1)
        void* familyIBP = nullptr;
        ibpInner = 0;
        familyIBP = ActMv_FindComponentFamily_12d0(entityInner, &ibpInner); // (0xbfefc)
        mv2 = Pf_MoveToCtor_9170(obj, familyIBP, pathT, 0, dstPos, 0xe1f, 0, kSpeed); // (0xbff28)
    }
    WAP2_IbpCtor(&ibpB, mv2);
    WAP2_Append(list, &ibpB);                                          // (0xbff46)

    // ============================================================
    // optional ShowTool #3 when param_14 != 0 (toolType=9) (0xbff4b-0xbff97)
    // ============================================================
    if (param_14 != 0) {
        obj = WAP2_Alloc(0x30, 8);                                     // size 0x30 (0xbff55 LEA ECX,[RDX+0x28])
        void* st3 = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(entity);
            st3 = ActChild_ShowToolCtor_42890(obj, &entityInner, 9); // R8D=9 (0xbff6e)
        }
        WAP2_IbpCtor(&ibpB, st3);
        WAP2_Append(list, &ibpB);                                      // (0xbff97)
    }

    return retval;                                                     // RAX = R13 = retval (0xbff9c)
}
