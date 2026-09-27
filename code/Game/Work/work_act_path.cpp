// code/Game/Work/work_act_path.cpp
// WorkAct_GatherRawMatCtor_bf790 — 1:1 port of exe Pathfinding_FindPath @ 0x1400bf790.
//
// Despite the Ghidra name this is NOT pathfinding: it is a citizen work-action-queue
// builder (~0x835 bytes) that constructs a pickup/dropoff/move action chain and stores
// it on the citizen's CitizenAIComponent task list (entity+0x278, list base +0x48). It
// is the "gather raw material" task builder: walk to a source, optional pickup, drop off
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

#include "engine_funcs.h"
#include "Action/PerformWork.h"   // ENDGAME B.3.8 CB3
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// ============================================================
// Sub-callee / dispatch-slot typedefs
// ============================================================

// --- exe code sub-callees (0x1400xxxxx → ResolveAddr returns fn ptr directly) ---

// FUN_1400c5400 (TaskListCleanup) — cancel pending work actions on the list. (0xbf7f3)
typedef void  (*WAP2_TaskListCleanup_t)(void* list);
// FUN_1400c54e0 (TaskListAppend) — append an action IBP to the task list. (0xbf9f5 etc.)
typedef void  (*WAP2_TaskListAppend_t)(void* list, void* actionIBP);

// Entity_impl_29a0 (0x1400c29a0): heading float toward a world target. RCX=worldPos*,
// RDX=&innerIBP. Returns float in XMM0. (0xbf929 / 0xbf944)
typedef float (*WAP2_Entity29a0_t)(float* worldPos, void* innerIBP);

// Entity_FindComponent_Family (0x1400012d0): member-style, RCX=entityInner, RDX=retIBP,
// returns retIBP (RAX). Wraps the move/family component for MoveTo. (0xbfbf5 / 0xbfefc)
typedef void* (*WAP2_FindComponentFamily_t)(uintptr_t entityInner, void* retIBP);

// SendComponentMessage_Action_CopyCtor (0x1400cb1d0): RCX=self, RDX=&entityInner,
// R8=&msgId(u32), R9D=p4, [RSP+0x20]=&innerIbp, [RSP+0x28]=p6. (0xbf858 / 0xbf8f6)
typedef void* (*WAP2_SendCompMsg_t)(void* self, void* entityInner, uint32_t* msgId,
                                    uint32_t p4, void* innerIbp, uint32_t p6);

// CompleteTask_Action_CopyCtor (0x1400c5da0): RCX=self, EDX=workId(=pathType slot),
// R8=&entityInner, R9=&innerIbp, [RSP+0x20]=completionCode. (0xbfa94 / 0xbfe0c)
typedef void* (*WAP2_CompleteTask_t)(void* self, uint32_t workId, void* entityInner,
                                     void* innerIbp, uint32_t completionCode);

// ShowTool_Action_Ctor (0x140042890): RCX=self, RDX=&entityInner, R8D=toolType. (0xbfa23 etc.)
typedef void* (*WAP2_ShowTool_t)(void* self, void* entityInner, int toolType);

// DropOffRawMaterial_Action_CopyCtor (0x1400c7cd0):
//   RCX=self, RDX=&p2, R8=&p3, R9=p4, [RSP+0x20]=p5(int), [RSP+0x28]=p6(byte).
//   field map per notes: +0x20=p6byte, +0x24=p5, +0x28=p4, +0x30=*p2, +0x38=*p3. (0xbfafe)
typedef void* (*WAP2_DropOff_t)(void* self, uint64_t* p2, uint64_t* p3, uint64_t p4,
                                uint32_t p5, uint8_t p6);

// PickupStoredRawMaterial_Action_CopyCtor (0x1400ca520):
//   RCX=self, RDX=&p2, R8=&p3, R9=p4, [RSP+0x20]=p5(int), [RSP+0x28]=p6(byte). (0xbfd94)
typedef void* (*WAP2_Pickup_t)(void* self, uint64_t* p2, uint64_t* p3, int64_t p4,
                               int p5, uint8_t p6);

// PerformWork_Action_Ctor7 (0x1400c9720): RCX=self, RDX=&entityInner, XMM2=heading(float),
//   R9D=actionType(int), [RSP+0x20]=time(qword), [RSP+0x28]=p6(byte), [RSP+0x30]=p7(int).
//   (0xbfb7c / 0xbfe83)
typedef void* (*WAP2_PerformWork7_t)(void* self, void* entityInner, float heading,
                                     int actionType, int64_t time, uint8_t p6, int p7);

// MoveTo_Action_Ctor (0x1400c9170): RCX=self, RDX=familyIBP, R8D=pathType, R9D=p4(0),
//   [RSP+0x20]=destination(float*), [RSP+0x28]=p6(0xe1f), [RSP+0x30]=p7(0), XMM=[RSP+0x38]=speed.
//   (0xbfc22 / 0xbff28)
typedef void* (*WAP2_MoveTo_t)(void* self, void* familyIBP, int pathType, int p4,
                               float* destination, int p6, int p7, float speed);

// --- dispatch slots (deref once for the runtime fn ptr) ---

// 0x140152b60 — System::MemoryAllocator::MemAlloc(size, align) static. (0xbf800 etc.)
typedef void* (*WAP2_MemAlloc_t)(uint64_t size, uint32_t align);
// 0x140152a90 — Object::InternalBasePtr::InternalBasePtr(self, obj). (0xbf86a etc.)
typedef void* (*WAP2_IbpCtor_t)(void* self, void* obj);
// 0x140151ab8 — Object::InternalBasePtr::DeletePtr(self). (0xbf87b)
typedef void  (*WAP2_IbpDelete_t)(void* self);
// 0x140152768 — Object::Internal::Internal(self) base ctor. (0xbf989 / 0xbfc79)
// 0x140152b00 — System::Time::Time(retval, float seconds) → returns Time*. (0xbf97d etc.)
typedef void* (*WAP2_TimeCtor_t)(void* retval, float seconds);

static WAP2_TaskListCleanup_t     s_wap2_TaskListCleanup   = nullptr;
static WAP2_TaskListAppend_t      s_wap2_TaskListAppend    = nullptr;
static WAP2_Entity29a0_t          s_wap2_Entity29a0        = nullptr;
static WAP2_FindComponentFamily_t s_wap2_FindCompFamily    = nullptr;
static WAP2_SendCompMsg_t         s_wap2_SendCompMsg       = nullptr;
static WAP2_CompleteTask_t        s_wap2_CompleteTask      = nullptr;
static WAP2_ShowTool_t            s_wap2_ShowTool          = nullptr;
static WAP2_DropOff_t             s_wap2_DropOff           = nullptr;
static WAP2_Pickup_t              s_wap2_Pickup            = nullptr;
static WAP2_PerformWork7_t        s_wap2_PerformWork7      = nullptr;
static WAP2_MoveTo_t              s_wap2_MoveTo            = nullptr;

static WAP2_MemAlloc_t            s_wap2_MemAlloc          = nullptr;
static WAP2_IbpCtor_t             s_wap2_IbpCtor           = nullptr;
static WAP2_IbpDelete_t           s_wap2_IbpDelete         = nullptr;
static WAP2_TimeCtor_t            s_wap2_TimeCtor          = nullptr;

// Inline-object vtables (exe .rdata).

// Runtime CRC type-id / message-id globals — read LIVE (these are 0 at static-init, the
// engine fills them with real CRC ids after Object::Factory::Startup).
static const uint32_t* s_wap2_pMsgId_c70e8 = nullptr; // DWORD_1401c70e8 (0xbf81e)
static const uint32_t* s_wap2_pMsgId_c7158 = nullptr; // DWORD_1401c7158 (0xbf8b8)

// Float .rdata constants (bit-exact).
static const float* s_wap2_pf_172124 = nullptr; // DAT_140172124 = 0.75f (0xbf962)
static const float* s_wap2_pf_172280 = nullptr; // DAT_140172280 = -1.0f MoveTo speed (0xbfbb6)

static bool s_wap2_resolved = false;

static void ResolveAll() {
    if (s_wap2_resolved) return;
    s_wap2_resolved = true;

    s_wap2_TaskListCleanup = reinterpret_cast<WAP2_TaskListCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_wap2_TaskListAppend  = reinterpret_cast<WAP2_TaskListAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_wap2_Entity29a0      = reinterpret_cast<WAP2_Entity29a0_t>(ResolveAddr(Module::Exe, 0x1400c29a0));
    s_wap2_FindCompFamily  = reinterpret_cast<WAP2_FindComponentFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_wap2_SendCompMsg     = reinterpret_cast<WAP2_SendCompMsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_wap2_CompleteTask    = reinterpret_cast<WAP2_CompleteTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_wap2_ShowTool        = reinterpret_cast<WAP2_ShowTool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_wap2_DropOff         = reinterpret_cast<WAP2_DropOff_t>(ResolveAddr(Module::Exe, 0x1400c7cd0));
    s_wap2_Pickup          = reinterpret_cast<WAP2_Pickup_t>(ResolveAddr(Module::Exe, 0x1400ca520));
    s_wap2_PerformWork7    = reinterpret_cast<WAP2_PerformWork7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_wap2_MoveTo          = reinterpret_cast<WAP2_MoveTo_t>(ResolveAddr(Module::Exe, 0x1400c9170));

    // Dispatch slots — deref once for the runtime fn ptr.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_wap2_MemAlloc        = *reinterpret_cast<WAP2_MemAlloc_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a90)) s_wap2_IbpCtor         = *reinterpret_cast<WAP2_IbpCtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140151ab8)) s_wap2_IbpDelete       = *reinterpret_cast<WAP2_IbpDelete_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b00)) s_wap2_TimeCtor        = *reinterpret_cast<WAP2_TimeCtor_t*>(s);

    // Fallbacks to engine_funcs helpers (same IAT targets) when a slot is unregistered.
    if (!s_wap2_IbpCtor)         s_wap2_IbpCtor         = reinterpret_cast<WAP2_IbpCtor_t>(fn_InternalBasePtr_Ctor);
    if (!s_wap2_IbpDelete)       s_wap2_IbpDelete       = reinterpret_cast<WAP2_IbpDelete_t>(fn_InternalBasePtr_DeletePtr);

    s_wap2_pMsgId_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_wap2_pMsgId_c7158 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7158));
    s_wap2_pf_172124    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172124));
    s_wap2_pf_172280    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
}

// Small guarded wrappers matching the original call shapes.
static inline void* WAP2_Alloc(uint64_t size, uint32_t align) {
    if (s_wap2_MemAlloc) return s_wap2_MemAlloc(size, align);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, align);
    return nullptr;
}
static inline void  WAP2_IbpCtor(void* ibp, void* obj) { if (s_wap2_IbpCtor) s_wap2_IbpCtor(ibp, obj); }
static inline void  WAP2_Append(void* list, void* ibp) { if (s_wap2_TaskListAppend) s_wap2_TaskListAppend(list, ibp); }

// Read a runtime Time qword via System::Time::Time(&buf, seconds); the original reads
// the first qword of the returned Time and forwards it as the action's "time" field.
static inline int64_t WAP2_MakeTime(float seconds) {
    uint64_t buf = 0;
    if (s_wap2_TimeCtor) {
        void* r = s_wap2_TimeCtor(&buf, seconds);
        if (r) return static_cast<int64_t>(*reinterpret_cast<uint64_t*>(r));
    }
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
    ResolveAll();

    // --- list base = (*(entity)+0x278) + 0x48 ; cleanup pending actions (0xbf7cf-0xbf7f3) ---
    uintptr_t ent = *reinterpret_cast<uintptr_t*>(entity);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ent + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_wap2_TaskListCleanup) s_wap2_TaskListCleanup(list);

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
        uint32_t msgId = s_wap2_pMsgId_c70e8 ? *s_wap2_pMsgId_c70e8 : 0; // DWORD_1401c70e8 (0xbf81e)
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(entity) + 0x10);
        if (s_wap2_SendCompMsg) sm = s_wap2_SendCompMsg(obj, &entityInner, &msgId, 0, &ibpInner, 0);
    }
    WAP2_IbpCtor(&ibpA, sm);                                            // (0xbf86a)
    // SendComponentMessage handle slot = R14+0x10, where R14 = list = aiComp+0x48,
    // i.e. aiComp+0x58. DeletePtr the prior handle, then store. (0xbf870-0xbf885)
    uintptr_t smSlot = reinterpret_cast<uintptr_t>(list) + 0x10;        // == aiComp+0x58
    if (*reinterpret_cast<uintptr_t*>(smSlot) != 0 && s_wap2_IbpDelete)
        s_wap2_IbpDelete(reinterpret_cast<void*>(smSlot));              // (0xbf877-0xbf87b)
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
            uint32_t msgId = s_wap2_pMsgId_c7158 ? *s_wap2_pMsgId_c7158 : 0; // DWORD_1401c7158 (0xbf8b8)
            void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(entity) + 0x10);
            if (s_wap2_SendCompMsg) sm2 = s_wap2_SendCompMsg(obj, &entityInner, &msgId, 0, &innerSlot, 0);
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
    if (s_wap2_Entity29a0) headingSrc = s_wap2_Entity29a0(srcPos, &headSlot); // XMM6 (0xbf929)
    float headingDst = 0.0f;
    headSlot = (dstLoc) ? reinterpret_cast<uint64_t>(*dstLoc) : 0;     // local_res8 = *dstLoc (0xbf931)
    if (s_wap2_Entity29a0) headingDst = s_wap2_Entity29a0(dstPos, &headSlot); // XMM8 (0xbf944)

    const float kTime075 = s_wap2_pf_172124 ? *s_wap2_pf_172124 : 0.0f; // DAT_140172124 = 0.75f (0xbf962)

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
        if (s_wap2_ShowTool) st1 = s_wap2_ShowTool(obj, &entityInner, -1); // R8D = -1 (0xbfa18)
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
            if (s_wap2_CompleteTask)
                ct = s_wap2_CompleteTask(obj, pathType, &entityInner, &ibpInner,
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
        if (s_wap2_DropOff)
            drop = s_wap2_DropOff(obj, &entInner, &srcInner,
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
        if (s_wap2_PerformWork7)
            pw7a = s_wap2_PerformWork7(obj, &entityInner, headingSrc, 6 /*R9D=6*/,
                                       timeVal, param_14 /*R12B*/, -1 /*[RSP+0x30]=-1*/); // (0xbfb7c)
    }
    WAP2_IbpCtor(&ibpA, pw7a);
    WAP2_Append(list, &ibpA);                                          // (0xbfb9a)

    // ============================================================
    // MoveTo #1 (destination=srcPos, pathType = (param_14!=0)?5:4) (0xbfb9f-0xbfc40)
    // ============================================================
    const float kSpeed = s_wap2_pf_172280 ? *s_wap2_pf_172280 : 0.0f;   // DAT_140172280 = -1.0f (0xbfbb6)
    obj = WAP2_Alloc(0x480, 8);
    void* mv1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(
            *reinterpret_cast<uintptr_t*>(entity) + 0x10);             // *((*entity)+0x10) (0xbfbcc-0xbfbd7)
        int pathT = 4 + (param_14 != 0 ? 1 : 0);                       // LEA R8D,[RBX+4], RBX=SETNZ (0xbfbe7-0xbfc1b)
        void* familyIBP = nullptr;
        ibpInner = 0;
        if (s_wap2_FindCompFamily) familyIBP = s_wap2_FindCompFamily(entityInner, &ibpInner); // (0xbfbf5)
        if (s_wap2_MoveTo)
            mv1 = s_wap2_MoveTo(obj, familyIBP, pathT, 0, srcPos, 0xe1f, 0, kSpeed); // (0xbfc22)
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
        if (s_wap2_ShowTool) st2 = s_wap2_ShowTool(obj, &entityInner, toolType);
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
        if (s_wap2_Pickup)
            pick = s_wap2_Pickup(obj, &entInner, &dstInner,
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
            if (s_wap2_CompleteTask)
                ct = s_wap2_CompleteTask(obj, pathType, &entityInner, &ibpInner,
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
        if (s_wap2_PerformWork7)
            pw7b = s_wap2_PerformWork7(obj, &entityInner, headingDst, 6 /*R9D=6*/,
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
        if (s_wap2_FindCompFamily) familyIBP = s_wap2_FindCompFamily(entityInner, &ibpInner); // (0xbfefc)
        if (s_wap2_MoveTo)
            mv2 = s_wap2_MoveTo(obj, familyIBP, pathT, 0, dstPos, 0xe1f, 0, kSpeed); // (0xbff28)
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
            if (s_wap2_ShowTool) st3 = s_wap2_ShowTool(obj, &entityInner, 9); // R8D=9 (0xbff6e)
        }
        WAP2_IbpCtor(&ibpB, st3);
        WAP2_Append(list, &ibpB);                                      // (0xbff97)
    }

    return retval;                                                     // RAX = R13 = retval (0xbff9c)
}
