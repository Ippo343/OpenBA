// code/Game/Work/work_act_hunt.cpp
// Hunt / ShowTool work-action-queue builders (Work action-ctor tail batch).
//
// Two instruction-faithful 1:1 ports of exe work-action constructors. Each builds
// a chain of Action objects (SendComponentMessage / CompleteTask / ShowTool /
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
#include "resolve.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B

// ============================================================
// Sub-callee / dispatch-slot resolution
// ============================================================
// InternalBasePtr ctor/checkptr/deleteptr come from engine_funcs.h
// (fn_InternalBasePtr_Ctor / _CheckPtr / _DeletePtr). The disasm uses dispatch slots
//   0x140152a90 = InternalBasePtr::InternalBasePtr(self, obj)   (IbpCtor)
//   0x140152ab0 = InternalBasePtr::CheckPtr(self)               (CheckPtr)
//   0x140151ab8 = Object::InternalBasePtr::DeletePtr(self)      (DeletePtr)
// We route those through the engine_funcs helpers (same IAT targets), matching the
// sibling work_base_actionctor.cpp.

// --- exe code sub-callees (0x1400xxxxx) ---

// FUN_1400c5400 (TaskListCleanup) — cancel pending work actions on the list.
typedef void (*WAH_fn_TaskListCleanup_t)(void* list);
// FUN_1400c54e0 (TaskListAppend) — grow-and-append an action IBP to the task list.
typedef void (*WAH_fn_TaskListAppend_t)(void* list, void* actionIBP);
// 0x1400032e0 (array grow, e.g. disasm 0x1400bdf94 CALL 0x1400032e0) was resolved
// here but never called anywhere in this file -- dead resolved-pointer machinery,
// removed rather than converted (B.3.2 step 2).

// Entity_FindComponent_Family (0x1400012d0) — member-style: RCX=entityInner,
// RDX=retIBP, returns retIBP (RAX). Wraps the family/move component for MoveTo.
typedef void* (*WAH_fn_FindComponentFamily_t)(uintptr_t entityInner, void* retIBP);

// Action constructor / copy-constructor sub-callees. Each returns the constructed
// object pointer in RAX (or null on alloc failure), fed straight to InternalBasePtr_Ctor.
// SendComponentMessage_Action_CopyCtor (0x1400cb1d0):
//   RCX=self, RDX=&entityInner, R8=&msgId, R9D=p4, [stack]=&innerIbp, [stack]=p6.
typedef void* (*WAH_fn_SendCompMsg_t)(void* self, void* entityInner, uint32_t* msgId, uint32_t p4, void* innerIbp, uint32_t p6);
// CompleteTask_Action_CopyCtor (0x1400c5da0):
//   RCX=self, EDX=workId, R8=&entityInner, R9=&innerIbp, [stack]=p5.
typedef void* (*WAH_fn_CompleteTask_t)(void* self, uint32_t workId, void* entityInner, void* innerIbp, uint32_t p5);
// ShowTool_Action_Ctor (0x140042890): RCX=self, RDX=&entityInner, R8D=toolType.
typedef void* (*WAH_fn_ShowTool_t)(void* self, void* entityInner, int toolType);
// HuntAnimal_Action_CopyCtor (0x1400c8400):
//   RCX=self, RDX=&entityInner (->self+0x28), R8=&innerVal (->self+0x20); self+0x30=0, self+0x38=0(byte).
typedef void* (*WAH_fn_HuntAnimal_CopyCtor_t)(void* self, uint64_t* p2, uint64_t* p3);
// PerformWork_Action_Ctor7 (0x1400c9720). Win64 ABI (verified via disassembly):
//   RCX=self, RDX=&entityInner, XMM2=heading (3rd slot, float), R9D=actionType (4th slot),
//   [stack]=time, byte p6, int p7. heading MUST be 3rd positional (lands in XMM2);
//   actionType MUST be 4th positional (lands in R9D).
typedef void* (*WAH_fn_PerformWork7_t)(void* self, void* entityInner, float heading, int actionType, int64_t time, uint8_t p6, int p7);
// MoveTo_Action_Ctor (0x1400c9170):
//   RCX=self, RDX=family, R8D=pathType, R9D=p4, [stack]=destination, p6, p7, XMM=speed.
typedef void* (*WAH_fn_MoveTo_t)(void* self, void* familyIBP, int pathType, int p4, float* destination, int p6, int p7, float speed);

// --- dispatch slots (deref once for the runtime fn ptr) ---
// 0x140152b60 — System::MemoryAllocator::MemAlloc(size, align) static wrapper.
typedef void* (*WAH_fn_MemAlloc_t)(uint64_t size, uint32_t align);
// 0x140152768 — Object::Internal::Internal(self) base ctor.
typedef void* (*WAH_fn_ObjInternalCtor_t)(void* self);
// 0x140152b00 — System::Time::Time(retval, float seconds).
typedef void* (*WAH_fn_TimeCtor_t)(void* retval, float seconds);

static WAH_fn_TaskListCleanup_t      s_wah_TaskListCleanup = nullptr;
static WAH_fn_TaskListAppend_t       s_wah_TaskListAppend  = nullptr;
static WAH_fn_FindComponentFamily_t  s_wah_FindCompFamily  = nullptr;
static WAH_fn_SendCompMsg_t          s_wah_SendCompMsg     = nullptr;
static WAH_fn_CompleteTask_t         s_wah_CompleteTask    = nullptr;
static WAH_fn_ShowTool_t             s_wah_ShowTool        = nullptr;
static WAH_fn_HuntAnimal_CopyCtor_t  s_wah_HuntAnimal      = nullptr;
static WAH_fn_PerformWork7_t         s_wah_PerformWork7    = nullptr;
static WAH_fn_MoveTo_t               s_wah_MoveTo          = nullptr;

static WAH_fn_MemAlloc_t             s_wah_MemAlloc        = nullptr;
static WAH_fn_ObjInternalCtor_t      s_wah_ObjInternalCtor = nullptr;
static WAH_fn_TimeCtor_t             s_wah_TimeCtor        = nullptr;

// Inline-object vtables + globals (exe .rdata).
static void* s_wah_vtable_Action               = nullptr;  // 0x14015faa0
static void* s_wah_vtable_CreateNaturalResource = nullptr; // 0x14015fca0
static void* s_wah_vtable_ShowTool             = nullptr;   // 0x1401589b0
static const uint32_t* s_wah_pDword_c70e8 = nullptr;        // 0x1401c70e8 (SendComponentMessage target type-id = CitizenAIComponent)
static const float* s_wah_pf_172280  = nullptr;            // 0x140172280 = -1.0f  (MoveTo speed)
static const float* s_wah_pf_172178  = nullptr;            // 0x140172178 =  2.1f  (PerformWork time, bdd20)
static const float* s_wah_pf_1722a8  = nullptr;            // 0x1401722a8 = -1000.0f (PerformWork heading const)

static bool s_wah_resolved = false;

static void ResolveAll() {
    if (s_wah_resolved) return;
    s_wah_resolved = true;

    s_wah_TaskListCleanup = reinterpret_cast<WAH_fn_TaskListCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_wah_TaskListAppend  = reinterpret_cast<WAH_fn_TaskListAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_wah_FindCompFamily  = reinterpret_cast<WAH_fn_FindComponentFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_wah_SendCompMsg     = reinterpret_cast<WAH_fn_SendCompMsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_wah_CompleteTask    = reinterpret_cast<WAH_fn_CompleteTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_wah_ShowTool        = reinterpret_cast<WAH_fn_ShowTool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_wah_HuntAnimal      = reinterpret_cast<WAH_fn_HuntAnimal_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c8400));
    s_wah_PerformWork7    = reinterpret_cast<WAH_fn_PerformWork7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_wah_MoveTo          = reinterpret_cast<WAH_fn_MoveTo_t>(ResolveAddr(Module::Exe, 0x1400c9170));

    // Dispatch slots — deref once to get the runtime fn ptr.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_wah_MemAlloc        = *reinterpret_cast<WAH_fn_MemAlloc_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152768)) s_wah_ObjInternalCtor = *reinterpret_cast<WAH_fn_ObjInternalCtor_t*>(s);
    if (!s_wah_ObjInternalCtor) s_wah_ObjInternalCtor = reinterpret_cast<WAH_fn_ObjInternalCtor_t>(fn_ObjectInternal_Ctor);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b00)) s_wah_TimeCtor        = *reinterpret_cast<WAH_fn_TimeCtor_t*>(s);

    s_wah_vtable_Action                = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015faa0));
    s_wah_vtable_CreateNaturalResource = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015fca0));
    s_wah_vtable_ShowTool              = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x1401589b0));
    s_wah_pDword_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_wah_pf_172280    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    s_wah_pf_172178    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172178));
    s_wah_pf_1722a8    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401722a8));
}

// Small wrappers matching the original MemAlloc(size,align) static call shape.
static inline void* WAH_Alloc(uint64_t size, uint32_t align) {
    if (s_wah_MemAlloc) return s_wah_MemAlloc(size, align);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, align);
    return nullptr;
}
static inline void  WAH_IbpCtor(void* ibp, void* obj) { if (fn_InternalBasePtr_Ctor) new (ibp) ::Object::InternalBasePtr(
    reinterpret_cast<RtObject::Internal*>(obj)); }   // B.3.3 bridge
static inline void  WAH_CheckPtr(void* p)             { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline void  WAH_Append(void* list, void* ibp) { if (s_wah_TaskListAppend) s_wah_TaskListAppend(list, ibp); }

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
    ResolveAll();

    // --- list base = (*(citizen)+0x278) + 0x48 ---  (disasm 0x1400bbbf3..0x1400bbc17)
    WAH_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WAH_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);  // RDI
    if (s_wah_TaskListCleanup) s_wah_TaskListCleanup(list);

    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0;

    // --- SendComponentMessage action -> stored at list+0x10 (aiComp+0x58), DeletePtr old first ---
    // disasm 0x1400bbc1c..0x1400bbca9 (alloc 0x40,8)
    void* obj = WAH_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_wah_pDword_c70e8 ? *s_wah_pDword_c70e8 : 0;  // [0x1401c70e8]
        WAH_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_wah_SendCompMsg) sm = s_wah_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpA, sm);
    if (*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(list) + 0x10) != 0 && fn_InternalBasePtr_DeletePtr)
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
        if (s_wah_CompleteTask) ct = s_wah_CompleteTask(obj, workId, &entityInner, &innerIbp, 0);
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
            WAH_CheckPtr(citizenIBP);
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
            WAH_CheckPtr(&entityInner);
            uintptr_t famRet = 0;
            void* familyIBP = nullptr;
            if (s_wah_FindCompFamily) familyIBP = s_wah_FindCompFamily(entityInner, &famRet);
            float speed = s_wah_pf_172280 ? *s_wah_pf_172280 : 0.0f;  // -1.0f
            // MoveTo(self, family, Slow=1, 0, param_4, 0xe1f, 0, speed)  (disasm R8D=1, [RSP+0x28]=0xe1f)
            if (s_wah_MoveTo) mainAction = s_wah_MoveTo(obj, familyIBP, 1 /*Slow; disasm LEA R8D,[R9+1]*/, 0, worldPos, 0xe1f, 0, speed);
        }
    } else {
        // disasm 0x1400bbd98..0x1400bbdf4 (alloc 0x48,8)
        obj = WAH_Alloc(0x48, 8);
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
            uint64_t timeVal = 0;
            if (s_wah_TimeCtor) s_wah_TimeCtor(&timeVal, performWorkTime);  // System::Time::Time(&t, param_5/XMM1)
            float heading = s_wah_pf_1722a8 ? *s_wah_pf_1722a8 : 0.0f;       // XMM2 = [0x1401722a8] = -1000.0f
            // PerformWork_Ctor7(self, &entityInner, heading, actionType=8, time, p6=1, p7=-1)
            // disasm: R9D=0x8, XMM2=[0x1401722a8], [RSP+0x28]=1, [RSP+0x30]=0xffffffff, time=*Time()
            if (s_wah_PerformWork7) mainAction = s_wah_PerformWork7(obj, &entityInner, heading, 8, static_cast<int64_t>(timeVal), 1, -1);
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
        if (s_wah_HuntAnimal) hunt = s_wah_HuntAnimal(obj, &entityInner, &innerVal);
    }
    WAH_IbpCtor(&ibpC, hunt);
    uintptr_t storedIbp = ibpC;  // local_50 captured before slot write
    // plVar8 = *(list) + *(int*)(list+0x18) * 8 ; CheckPtr(plVar8) ; *(*plVar8 + 0x10) = storedIbp
    int32_t idx = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(list) + 0x18);   // [RDI+0x18]
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(list);                                // [RDI]
    void* slot = reinterpret_cast<void*>(arrBase + static_cast<intptr_t>(idx) * 8);         // RDI = &arr[idx]
    WAH_CheckPtr(slot);   // disasm 0x1400bbe67 MOV RCX,RDI ; CheckPtr(slot address itself)
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
    ResolveAll();

    // --- list base = (*(citizen)+0x278) + 0x48 ---  (disasm 0x1400bdd55..0x1400bdd79)
    WAH_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WAH_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);  // lVar2
    void* list = reinterpret_cast<void*>(aiComp + 0x48);           // RSI / plVar14
    if (s_wah_TaskListCleanup) s_wah_TaskListCleanup(list);

    uintptr_t ibpA = 0, ibpB = 0;

    // --- SendComponentMessage #1 -> list+0x10 (aiComp+0x58), DeletePtr old first ---
    // disasm 0x1400bdd7e..0x1400bde0f (alloc 0x40,8)
    void* obj = WAH_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAH_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_wah_pDword_c70e8 ? *s_wah_pDword_c70e8 : 0;  // [0x1401c70e8]
        WAH_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_wah_SendCompMsg) sm = s_wah_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WAH_IbpCtor(&ibpA, sm);
    if (*reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(list) + 0x10) != 0 && fn_InternalBasePtr_DeletePtr)
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
        if (s_wah_CompleteTask) ct = s_wah_CompleteTask(obj, workId, &entityInner, &innerIbp, completeParam8);
    }
    WAH_IbpCtor(&ibpB, ct);
    WAH_Append(list, &ibpB);

    // --- ShowTool (~Hoe = -1) --- disasm 0x1400bde77..0x1400bdec3 (alloc 0x30,8; OR R8D,0xffffffff)
    obj = WAH_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1
        if (s_wah_ShowTool) st = s_wah_ShowTool(obj, &entityInner, -1 /* ~Hoe; disasm OR R8D,0xFFFFFFFF */);
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
                    if (s_wah_SendCompMsg) sm2 = s_wah_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, p6);
                }
                WAH_IbpCtor(&ibpB, sm2);
                WAH_Append(list, &ibpB);  // inlined grow-and-append in disasm (== FUN_1400c54e0)
            }

            // --- inline CreateNaturalResource action --- (alloc 0x38,8) disasm 0x1400be01c..0x1400be075
            obj = WAH_Alloc(0x38, 8);
            void* cnr = nullptr;
            if (obj) {
                int64_t  gateVal = *gatePtr9;        // lVar11 = *param_9
                uint32_t baseVal = *curBase;         // uVar1 = *param_7 (current)
                if (s_wah_ObjInternalCtor) s_wah_ObjInternalCtor(obj);
                uintptr_t o = reinterpret_cast<uintptr_t>(obj);
                *reinterpret_cast<void**>(o)           = s_wah_vtable_Action;
                *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
                *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
                *reinterpret_cast<void**>(o)           = s_wah_vtable_CreateNaturalResource;  // 0x14015fca0
                *reinterpret_cast<void**>(o + 0x20)    = createParam3;     // param_3 (dword*) -> +0x20
                *reinterpret_cast<int64_t*>(o + 0x28)  = gateVal;          // *param_9      -> +0x28
                *reinterpret_cast<uint32_t*>(o + 0x30) = baseVal;          // *param_7      -> +0x30
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
        if (s_wah_TimeCtor) s_wah_TimeCtor(&timeVal, s_wah_pf_172178 ? *s_wah_pf_172178 : 0.0f);  // XMM1=[0x140172178]=2.1
        float heading = s_wah_pf_1722a8 ? *s_wah_pf_1722a8 : 0.0f;  // XMM2=[0x1401722a8]=-1000.0
        // PerformWork_Ctor7(self, &entityInner, heading, actionType=0xC, time, p6=1, p7=-1)
        // disasm: R9D=0xc, XMM2=[0x1401722a8], [RSP+0x28]=1, [RSP+0x30]=0xffffffff
        if (s_wah_PerformWork7) pw7 = s_wah_PerformWork7(obj, &entityInner, heading, 0xC, static_cast<int64_t>(timeVal), 1, -1);
    }
    WAH_IbpCtor(&ibpA, pw7);
    WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm

    // --- MoveTo (Fastest=3, destination=param_5) --- disasm 0x1400be38d..0x1400be40e (alloc 0x480,8)
    obj = WAH_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        WAH_CheckPtr(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        WAH_CheckPtr(&entityInner);
        uintptr_t famRet = 0;
        void* familyIBP = nullptr;
        if (s_wah_FindCompFamily) familyIBP = s_wah_FindCompFamily(entityInner, &famRet);
        float speed = s_wah_pf_172280 ? *s_wah_pf_172280 : 0.0f;  // XMM0=[0x140172280]=-1.0
        // MoveTo(self, family, Fastest=3, 0, param_5, 0xe1f, 0, speed)  (disasm R8D=3, [RSP+0x28]=0xe1f)
        if (s_wah_MoveTo) mv = s_wah_MoveTo(obj, familyIBP, 3 /*Fastest; disasm LEA R8D,[R9+3]*/, 0, destination, 0xe1f, 0, speed);
    }
    WAH_IbpCtor(&ibpA, mv);
    WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm

    // --- inline ShowTool (Shovel = 3) action --- disasm 0x1400be55d..0x1400be5a4
    // alloc 0x30,8 (disasm 0x1400be55d MOV EDX,0x8 / LEA ECX,[RDX+0x28] => 0x30)
    obj = WAH_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // *param_1 -> RBX
        if (s_wah_ObjInternalCtor) s_wah_ObjInternalCtor(obj);
        uintptr_t o = reinterpret_cast<uintptr_t>(obj);
        *reinterpret_cast<void**>(o)           = s_wah_vtable_Action;
        *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
        *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
        *reinterpret_cast<void**>(o)           = s_wah_vtable_ShowTool;   // 0x1401589b0
        *reinterpret_cast<uintptr_t*>(o + 0x20) = entityInner;           // *param_1 -> +0x20
        *reinterpret_cast<uint32_t*>(o + 0x28) = 3;                      // Shovel   -> +0x28
        st2 = obj;
    }
    WAH_IbpCtor(&ibpA, st2);
    WAH_Append(list, &ibpA);  // inlined grow-and-append in disasm
    (void)ibpB;
}
