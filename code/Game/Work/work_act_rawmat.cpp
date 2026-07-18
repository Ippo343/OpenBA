// code/Game/Work/work_act_rawmat.cpp
// Raw-material work-action constructors (Work action-ctor sub-batch).
//
// Two exe helpers that build the action sequence for a citizen "gather raw
// material" / "fishing" work task. Each allocates a chain of Action objects
// (SendComponentMessage / ShowTool / CompleteTask / CreateRawMaterial /
// PerformWork / MoveTo) and appends InternalBasePtr wrappers to the
// CitizenAIComponent task action list (citizen+0x278 -> aiComp, list base
// aiComp+0x48). The SendComponentMessage action is stored at aiComp+0x58
// (DeletePtr the prior one first). 1:1 instruction-faithful ports of
//   FUN_1400bb7b0 (WorkAct_GatherWorkCtor_bb7b0)
//   FUN_1400beae0 (WorkAct_FishWorkCtor_beae0)
//
// Resolved via ResolveAddr(Module::Exe, 0xADDR) — 0 (silently skipped) unless
// registered. Self-contained
// (own static resolvers, s_war_* prefix to avoid ODR clashes with sibling
// work_base_actionctor.cpp).
//
// Do NOT "improve" — every offset/constant cites its disasm address.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Sub-callee / dispatch-slot resolution (self-contained, s_war_* prefix)
// ============================================================
// Engine base IBP helpers (Ctor/CheckPtr/DeletePtr) come from engine_funcs.h.

// --- exe code sub-callees ---
// FUN_1400c5400 TaskListCleanup — cancel pending work actions on the list.
typedef void  (*WAR_fn_TaskListCleanup_t)(void* list);
// FUN_1400c54e0 TaskListAppend — append an action IBP to the task list.
typedef void  (*WAR_fn_TaskListAppend_t)(void* list, void* actionIBP);
// Entity_FindComponent_Family (0x1400012d0): RCX=entityInner, RDX=retIBP -> retIBP.
typedef void* (*WAR_fn_FindComponentFamily_t)(uintptr_t entityInner, void* retIBP);
// Entity_FindComponent_Storage (0x140006270): RCX=entityInner, RDX=retIBP -> retIBP (storage IBP).
typedef void* (*WAR_fn_FindComponentStorage_t)(int64_t entityInner, void* retIBP);
// SendComponentMessage_Action_CopyCtor (0x1400cb1d0).
typedef void* (*WAR_fn_SendCompMsg_t)(void* self, void* p2, uint32_t* p3, uint32_t p4, void* p5, uint32_t p6);
// CompleteTask_Action_CopyCtor (0x1400c5da0).
typedef void* (*WAR_fn_CompleteTask_t)(void* self, uint32_t workId, void* p3, void* p4, uint32_t p5);
// ShowTool_Action_Ctor (0x140042890): RCX=self, RDX=&entityInner, R8D=toolType.
typedef void* (*WAR_fn_ShowTool_t)(void* self, void* p2, int toolType);
// CreateRawMaterial_Action_CopyCtor (0x1400c71a0). Win64 ABI (verified disasm 0x1400bba55/0x1400bedbd):
//   RCX=self, RDX=p2(u64), XMM2=p3 float(3rd slot), R9=p4(ptr),
//   [RSP+0x20]=p5(ptr), [RSP+0x28]=p6(u64), [RSP+0x30]=p7(u32), [RSP+0x38]=p8(u32),
//   [RSP+0x40]=p9(byte), [RSP+0x48]=p10(ptr), [RSP+0x50]=p11(u32).
typedef void* (*WAR_fn_CreateRawMaterial_t)(void* self, uint64_t p2, float p3, uint64_t* p4, uint64_t* p5,
                                            uint64_t p6, uint32_t p7, uint32_t p8, uint8_t p9,
                                            uint64_t* p10, uint32_t p11);
// PerformWork_Action_Ctor7 (0x1400c9720). Win64 ABI (verified disasm 0x1400bbad5/0x1400bee3a):
//   RCX=self, RDX=&entityInner, XMM2=heading float(3rd slot), R9D=actionType(int),
//   [RSP+0x20]=time(i64), [RSP+0x28]=byte p6, [RSP+0x30]=int p7.
typedef void* (*WAR_fn_PerformWork7_t)(void* self, void* entity, float heading, int actionType, int64_t time, uint8_t p6, int p7);
// MoveTo_Action_Ctor (0x1400c9170): RCX=self, RDX=familyIBP, R8D=pathType, R9D=p4,
//   [RSP+0x20]=destination(ptr), [RSP+0x28]=u32(0xe1f), [RSP+0x30]=u32, [RSP+0x38]=speed(float).
typedef void* (*WAR_fn_MoveTo_t)(void* self, void* familyIBP, int pathType, int p4, float* destination, int p6, int p7, float speed);

// --- dispatch slots (deref once to get runtime fn ptr) ---
// 0x140152b60 — System::MemoryAllocator::MemAlloc(size, align) static wrapper.
typedef void* (*WAR_fn_MemAllocStatic2_t)(uint64_t size, uint32_t align);
// 0x140152a90 — Object::InternalBasePtr::InternalBasePtr(self, obj) ctor.
typedef void* (*WAR_fn_IbpCtorSlot_t)(void* self, void* obj);
// 0x140151ab8 — Object::InternalBasePtr::DeletePtr(self).
typedef void  (*WAR_fn_IbpDeleteSlot_t)(void* self);
// 0x140152ab0 — Object::InternalBasePtr::CheckPtr(self).
typedef void  (*WAR_fn_IbpCheckSlot_t)(void* self);
// 0x140152b00 — System::Time::Time(retval, float seconds).
typedef void* (*WAR_fn_Time_Ctor_t)(void* retval, float seconds);

static WAR_fn_TaskListCleanup_t     s_war_TaskListCleanup  = nullptr;
static WAR_fn_TaskListAppend_t      s_war_TaskListAppend   = nullptr;
static WAR_fn_FindComponentFamily_t s_war_FindCompFamily   = nullptr;
static WAR_fn_FindComponentStorage_t s_war_FindCompStorage = nullptr;
static WAR_fn_SendCompMsg_t         s_war_SendCompMsg      = nullptr;
static WAR_fn_CompleteTask_t        s_war_CompleteTask     = nullptr;
static WAR_fn_ShowTool_t            s_war_ShowTool         = nullptr;
static WAR_fn_CreateRawMaterial_t   s_war_CreateRawMat     = nullptr;
static WAR_fn_PerformWork7_t        s_war_PerformWork7     = nullptr;
static WAR_fn_MoveTo_t              s_war_MoveTo           = nullptr;

static WAR_fn_MemAllocStatic2_t     s_war_MemAllocStatic   = nullptr;
static WAR_fn_IbpCtorSlot_t         s_war_IbpCtorSlot      = nullptr;
static WAR_fn_IbpDeleteSlot_t       s_war_IbpDeleteSlot    = nullptr;
static WAR_fn_IbpCheckSlot_t        s_war_IbpCheckSlot     = nullptr;
static WAR_fn_Time_Ctor_t           s_war_TimeCtor         = nullptr;

// Runtime type-id dwords (CRC values filled after Engine::Startup). Read via pointer
// at call time, never inlined (they are 0 in the static image).
static const uint32_t* s_war_pMsgId1_c70e8 = nullptr; // 0x1401c70e8 (SendComponentMessage #1 msg id, both fns)
static const uint32_t* s_war_pMsgId2_c46b0 = nullptr; // 0x1401c46b0 (bb7b0 SendComponentMessage #2 msg id)
static const uint32_t* s_war_pMsgId2_c7660 = nullptr; // 0x1401c7660 (beae0 SendComponentMessage #2 msg id)
// Float .rdata constants (bit-exact from read_memory).
static const float*    s_war_pf_172280 = nullptr;     // 0x140172280 = -1.0f (MoveTo speed, both fns)

static bool s_war_resolved = false;

static void ResolveAll() {
    if (s_war_resolved) return;
    s_war_resolved = true;

    s_war_TaskListCleanup = reinterpret_cast<WAR_fn_TaskListCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_war_TaskListAppend  = reinterpret_cast<WAR_fn_TaskListAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_war_FindCompFamily  = reinterpret_cast<WAR_fn_FindComponentFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_war_FindCompStorage = reinterpret_cast<WAR_fn_FindComponentStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_war_SendCompMsg     = reinterpret_cast<WAR_fn_SendCompMsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_war_CompleteTask    = reinterpret_cast<WAR_fn_CompleteTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_war_ShowTool        = reinterpret_cast<WAR_fn_ShowTool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_war_CreateRawMat    = reinterpret_cast<WAR_fn_CreateRawMaterial_t>(ResolveAddr(Module::Exe, 0x1400c71a0));
    s_war_PerformWork7    = reinterpret_cast<WAR_fn_PerformWork7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_war_MoveTo          = reinterpret_cast<WAR_fn_MoveTo_t>(ResolveAddr(Module::Exe, 0x1400c9170));

    // Dispatch slots — deref once to get the runtime fn ptr.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_war_MemAllocStatic = *reinterpret_cast<WAR_fn_MemAllocStatic2_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a90)) s_war_IbpCtorSlot    = *reinterpret_cast<WAR_fn_IbpCtorSlot_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140151ab8)) s_war_IbpDeleteSlot  = *reinterpret_cast<WAR_fn_IbpDeleteSlot_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ab0)) s_war_IbpCheckSlot   = *reinterpret_cast<WAR_fn_IbpCheckSlot_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b00)) s_war_TimeCtor       = *reinterpret_cast<WAR_fn_Time_Ctor_t*>(s);

    s_war_pMsgId1_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_war_pMsgId2_c46b0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c46b0));
    s_war_pMsgId2_c7660 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7660));
    s_war_pf_172280     = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
}

// Helpers mirroring the original idioms. Prefer the .rdata dispatch slots (what the
// disasm actually calls); fall back to engine_funcs.h when a slot is unregistered.
static inline void* WAR_Alloc(uint64_t size, uint32_t align) {
    if (s_war_MemAllocStatic) return s_war_MemAllocStatic(size, align);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, align);
    return nullptr;
}
static inline void WAR_IbpCtor(void* ibp, void* obj) {
    if (s_war_IbpCtorSlot) s_war_IbpCtorSlot(ibp, obj);
    else if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibp, obj);
}
static inline void WAR_CheckPtr(void* p) {
    if (s_war_IbpCheckSlot) s_war_IbpCheckSlot(p);
    else if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
}
static inline void WAR_DeletePtr(void* p) {
    if (s_war_IbpDeleteSlot) s_war_IbpDeleteSlot(p);
    else if (fn_InternalBasePtr_DeletePtr) fn_InternalBasePtr_DeletePtr(p);
}
static inline void WAR_Append(void* list, void* ibp) { if (s_war_TaskListAppend) s_war_TaskListAppend(list, ibp); }

// Bit-exact float constants (read_memory). Used where the disasm loads an immediate
// from .rdata that holds a real value in the static image (Time-ctor args).
static const uint32_t kWar_f_1p0_14017214c   = 0x3f800000u; // [0x14017214c] = 1.0f  (bb7b0 PerformWork Time arg)
static const uint32_t kWar_f_n1000_1401722a8 = 0xc47a0000u; // [0x1401722a8] = -1000.0f (bb7b0 PerformWork heading)
static const uint32_t kWar_f_30p0_1401721cc  = 0x41f00000u; // [0x1401721cc] = 30.0f  (beae0 PerformWork Time arg)
static inline float WAR_FloatBits(uint32_t bits) { float f; __builtin_memcpy(&f, &bits, 4); return f; }

// ============================================================
// WorkAct_GatherWorkCtor_bb7b0 (FUN_1400bb7b0)
// ============================================================
// Build the gather-raw-material work-action chain (forester/gatherer-style) and
// store it on the citizen's CitizenAIComponent task list (citizen+0x278, +0x48).
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId (u32)               -> CompleteTask param_2
//   param_3 = longlong* (workEntity)     -> *param_3 used as SendComponentMessage #2 entity inner
//   param_4 = float* worldPos            -> MoveTo destination
//   param_5 = u64 (raw-material p2)      -> CreateRawMaterial param_2 (RDX, [RBP+0x67])
//   param_6 = char flag                  -> gates the CreateRawMaterial block ([RBP+0x6f])
//   param_7 = float (raw-material p3)    -> CreateRawMaterial XMM2 ([RBP+0x77])
//   param_8 = u32 (raw-material p8)      -> CreateRawMaterial p8 ([RBP+0x7f])
//   param_9 = u32                        -> CompleteTask p5 / SendComponentMessage #2 p6 ([RBP+0x87])
void WorkAct_GatherWorkCtor_bb7b0(void* citizenIBP, uint32_t workId, int64_t* workEntity,
                                  float* worldPos, uint64_t rawMatP2, char flag6,
                                  float rawMatP3, uint32_t rawMatP8, uint32_t param9) {
    ResolveAll();

    // list base = (*(citizen)+0x278) + 0x48 (disasm 0x1400bb7eb..0x1400bb802)
    WAR_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WAR_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_war_TaskListCleanup) s_war_TaskListCleanup(list);

    void* obj;
    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0;

    // --- SendComponentMessage #1 -> aiComp+0x58 (replaces prior, DeletePtr) (0x1400bb80e..0x1400bb89b) ---
    obj = WAR_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;            // [RBP-0x29]
        WAR_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_war_pMsgId1_c70e8 ? *s_war_pMsgId1_c70e8 : 0; // [0x1401c70e8] -> [RBP+0x47]
        WAR_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // [RBP-0x21]
        if (s_war_SendCompMsg) sm = s_war_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WAR_IbpCtor(&ibpA, sm);              // [RBP-0x19]
    // disasm 0x1400bb886: CMP [RSI+0x10],0 ; RSI = aiComp+0x48 -> +0x10 = aiComp+0x58.
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        WAR_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA; // disasm 0x1400bb89b: MOV [RSI+0x10],RAX

    // --- CompleteTask action (0x1400bb89f..0x1400bb904) ---
    obj = WAR_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);   // [RBP-0x19]
        uintptr_t innerIbp = 0;                                              // [RBP+0x47] re-init via IbpCtor? — disasm reuses [RBP+0x47] as 0-init IBP
        WAR_IbpCtor(&innerIbp, nullptr);
        if (s_war_CompleteTask) ct = s_war_CompleteTask(obj, workId, &entityInner, &innerIbp, param9);
    }
    WAR_IbpCtor(&ibpB, ct);              // [RBP-0x21]
    WAR_Append(list, &ibpB);

    // --- SendComponentMessage #2 (entity = *param_3, msgId = [0x1401c46b0], p4=1, p6=param_9) (0x1400bb909..0x1400bb97d) ---
    obj = WAR_Alloc(0x40, 8);
    void* sm2 = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;          // [RBP-0x19]
        WAR_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_war_pMsgId2_c46b0 ? *s_war_pMsgId2_c46b0 : 0;     // [0x1401c46b0] -> [RBP+0x47]
        uintptr_t entityInner = static_cast<uintptr_t>(*workEntity);        // *param_3 -> [RBP-0x21]
        if (s_war_SendCompMsg) sm2 = s_war_SendCompMsg(obj, &entityInner, &msgId, 1, &innerIbp, param9);
    }
    WAR_IbpCtor(&ibpC, sm2);             // [RBP-0x29]
    WAR_Append(list, &ibpC);

    // --- CreateRawMaterial action, gated on param_6 (0x1400bb982..0x1400bba73) ---
    if (flag6 != '\0') {
        // storage = Entity_FindComponent_Storage(citizen entity inner)
        WAR_CheckPtr(citizenIBP);
        uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // [RBP+0x47]
        WAR_CheckPtr(&entInner);
        uintptr_t storageRet = 0;        // [RBP-0x9]
        void* storageIBP = nullptr;
        if (s_war_FindCompStorage) storageIBP = s_war_FindCompStorage(static_cast<int64_t>(entInner), &storageRet);
        WAR_CheckPtr(storageIBP);
        uint32_t p7 = 0;                 // R15D = *(*(storageIBP)+0x38)
        if (storageIBP) p7 = *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(storageIBP) + 0x38);

        obj = WAR_Alloc(0x58, 8);
        void* crm = nullptr;
        if (obj) {
            WAR_CheckPtr(citizenIBP);
            uintptr_t entField248 = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x248); // [RBP+0x47] (p10 target)
            WAR_CheckPtr(citizenIBP);
            uintptr_t entObj = *reinterpret_cast<uintptr_t*>(citizenIBP); // RCX -> [RBP-0x21] (p4)
            uintptr_t ent248Slot = *reinterpret_cast<uintptr_t*>(entObj + 0x248);                                   // [RBP-0x19] (p5)
            // p2=param_5 (RDX), p3=param_7 (XMM2 float), p4=&entObj, p5=&ent248Slot, p6=0 (leftover stack),
            // p7=*(*(storage)+0x38), p8=param_8, p9=1, p10=&entField248, p11=0xc (disasm 0x1400bba1a..0x1400bba55)
            if (s_war_CreateRawMat)
                crm = s_war_CreateRawMat(obj, rawMatP2, rawMatP3, &entObj, &ent248Slot, 0,
                                         p7, rawMatP8, 0x1, &entField248, 0xc);
        }
        WAR_IbpCtor(&ibpC, crm);         // [RBP-0x29]
        WAR_Append(list, &ibpC);
    }

    // --- PerformWork action (0x1400bba78..0x1400bbaf3) ---
    // disasm: XMM2 = [0x1401722a8] (-1000.0f heading), R9D = 0x8 (actionType),
    //         time = System::Time::Time(&t, 1.0f [0x14017214c]); p6 = 1; p7 = -1.
    obj = WAR_Alloc(0x48, 8);
    void* pw = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);  // [RBP+0x47]
        uint64_t timeVal = 0;                                              // [RBP-0x21]
        if (s_war_TimeCtor) s_war_TimeCtor(&timeVal, WAR_FloatBits(kWar_f_1p0_14017214c));
        float heading = WAR_FloatBits(kWar_f_n1000_1401722a8);
        if (s_war_PerformWork7)
            pw = s_war_PerformWork7(obj, &entityInner, heading, 0x8, static_cast<int64_t>(timeVal), 1, -1);
    }
    WAR_IbpCtor(&ibpB, pw);              // [RBP-0x19]
    WAR_Append(list, &ibpB);

    // --- MoveTo action (Slow=1, destination=param_4) (0x1400bbaf8..0x1400bbb8f) ---
    obj = WAR_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        WAR_CheckPtr(citizenIBP);
        uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // [RBP+0x47]
        WAR_CheckPtr(&entInner);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;            // [RBP-0x21]
        if (s_war_FindCompFamily) familyIBP = s_war_FindCompFamily(entInner, &famRet);
        float speed = s_war_pf_172280 ? *s_war_pf_172280 : 0.0f;          // [0x140172280] = -1.0f
        // disasm: R8D=1 (Slow), R9D=0, [RSP+0x20]=param_4, [RSP+0x28]=0xe1f, [RSP+0x30]=0, [RSP+0x38]=speed
        if (s_war_MoveTo) mv = s_war_MoveTo(obj, familyIBP, 1 /*Slow; disasm LEA R8D,[R9+1]*/, 0, worldPos, 0xe1f, 0, speed);
    }
    WAR_IbpCtor(&ibpB, mv);              // [RBP-0x19]
    WAR_Append(list, &ibpB);
}

// ============================================================
// WorkAct_FishWorkCtor_beae0 (FUN_1400beae0)
// ============================================================
// Build the fishing work-action chain and store it on the citizen's
// CitizenAIComponent task list (citizen+0x278, +0x48).
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId (u32)               -> CompleteTask param_2
//   param_3 = longlong* (workEntity)     -> *param_3 gates + feeds SendComponentMessage #2
//   param_4 = float* position            -> MoveTo destination
//   param_5 = (unused in body; param-area only)
//   param_6 = u64 (raw-material p2)      -> CreateRawMaterial param_2 (RDX, [RBP+0x68])
//   param_7 = float (raw-material p3)    -> CreateRawMaterial XMM2 ([RBP+0x70])
//   param_8 = u32                        -> SendComponentMessage #2 p6 / CreateRawMaterial p8 ([RBP+0x78])
//   param_9 = u32                        -> CompleteTask p5 ([RBP+0x80])
void WorkAct_FishWorkCtor_beae0(void* citizenIBP, uint32_t workId, int64_t* workEntity,
                                float* position, float param5_heading, uint64_t rawMatP2,
                                float rawMatP3, uint32_t param8, uint32_t param9) {
    ResolveAll();

    // list base = (*(citizen)+0x278) + 0x48 (disasm 0x1400beb18..0x1400beb2f)
    WAR_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WAR_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_war_TaskListCleanup) s_war_TaskListCleanup(list);

    void* obj;
    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0;

    // --- SendComponentMessage #1 -> aiComp+0x58 (0x1400beb3b..0x1400bebc8) ---
    obj = WAR_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;          // [RBP+0x50]
        WAR_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_war_pMsgId1_c70e8 ? *s_war_pMsgId1_c70e8 : 0;     // [0x1401c70e8] -> [RBP+0x40]
        WAR_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // [RBP-0x20]
        if (s_war_SendCompMsg) sm = s_war_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WAR_IbpCtor(&ibpA, sm);              // [RBP-0x18]
    // disasm 0x1400bebb3: CMP [RSI+0x10],0 ; RSI = aiComp+0x48 -> +0x10 = aiComp+0x58.
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0)
        WAR_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA; // disasm 0x1400bebc8: MOV [RSI+0x10],RAX

    // --- ShowTool (~Hoe = -1) action (0x1400bebcc..0x1400bec13) ---
    obj = WAR_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // [RBP+0x40]
        // disasm: OR R8D,0xFFFFFFFF -> toolType = -1 (~Hoe, Hoe==0)
        if (s_war_ShowTool) st = s_war_ShowTool(obj, &entityInner, -1);
    }
    WAR_IbpCtor(&ibpB, st);              // [RBP+0x50]
    WAR_Append(list, &ibpB);

    // --- CompleteTask action (param_5 = param_9) (0x1400bec18..0x1400bec7b) ---
    obj = WAR_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;          // [RBP+0x40]
        WAR_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // [RBP+0x50]
        if (s_war_CompleteTask) ct = s_war_CompleteTask(obj, workId, &entityInner, &innerIbp, param9);
    }
    WAR_IbpCtor(&ibpA, ct);              // [RBP-0x18]
    WAR_Append(list, &ibpA);

    // --- optional SendComponentMessage #2 when *param_3 != 0 (0x1400bec80..0x1400becfc) ---
    if (*workEntity != 0) {
        obj = WAR_Alloc(0x40, 8);
        void* sm2 = nullptr;
        if (obj) {
            uintptr_t innerIbp = 0;      // [RBP+0x50]
            WAR_IbpCtor(&innerIbp, nullptr);
            uint32_t msgId = s_war_pMsgId2_c7660 ? *s_war_pMsgId2_c7660 : 0; // [0x1401c7660] -> [RBP+0x40]
            uintptr_t entityInner = static_cast<uintptr_t>(*workEntity);    // *param_3 -> [RBP-0x18]
            // disasm: R9D=0 (p4), [RSP+0x28]=R15D = param_8 ([RBP+0x78]) -> p6
            if (s_war_SendCompMsg) sm2 = s_war_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, param8);
        }
        WAR_IbpCtor(&ibpC, sm2);         // [RBP-0x20]
        WAR_Append(list, &ibpC);
    }

    // --- CreateRawMaterial action (always; 0x1400bed01..0x1400beddb) ---
    {
        // storage = Entity_FindComponent_Storage(citizen entity inner)
        WAR_CheckPtr(citizenIBP);
        uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // [RBP+0x40]
        WAR_CheckPtr(&entInner);
        uintptr_t storageRet = 0;        // [RBP+0x50]
        void* storageIBP = nullptr;
        if (s_war_FindCompStorage) storageIBP = s_war_FindCompStorage(static_cast<int64_t>(entInner), &storageRet);
        WAR_CheckPtr(storageIBP);
        uint32_t p7 = 0;                 // R12D = *(*(storageIBP)+0x38)
        if (storageIBP) p7 = *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(storageIBP) + 0x38);

        obj = WAR_Alloc(0x58, 8);
        void* crm = nullptr;
        if (obj) {
            uintptr_t innerIbp = 0;      // [RBP+0x40] (IbpCtor(&,0))
            WAR_IbpCtor(&innerIbp, nullptr);
            WAR_CheckPtr(citizenIBP);
            uintptr_t entObj = *reinterpret_cast<uintptr_t*>(citizenIBP); // RDX -> [RBP-0x18] (p4); RCX side
            uintptr_t ent248Slot = *reinterpret_cast<uintptr_t*>(entObj + 0x248);                                // [RBP+0x50] (p5)
            // p2=param_6 (RDX), p3=param_7 (XMM2), p4=&ent248Slot([RBP+0x50]) , p5=... — order per disasm:
            //   R9 = &[RBP-0x18] (= entObj, p4); [RSP+0x20] = &[RBP+0x50] (= ent248Slot, p5);
            //   [RSP+0x28]=0 (p6); [RSP+0x30]=R12D=p7; [RSP+0x38]=R15D=param_8 (p8);
            //   [RSP+0x40]=0 (p9 byte); [RSP+0x48]=&[RBP+0x40] (p10); [RSP+0x50]=4 (p11)
            if (s_war_CreateRawMat)
                crm = s_war_CreateRawMat(obj, rawMatP2, rawMatP3, &entObj, &ent248Slot, 0,
                                         p7, param8, 0x0, &innerIbp, 0x4);
        }
        WAR_IbpCtor(&ibpC, crm);         // [RBP-0x20]
        WAR_Append(list, &ibpC);
    }

    // --- PerformWork action (0x1400bede0..0x1400bee58) ---
    // disasm: R9D = 0xe (actionType), XMM2 = [RBP+0x60] (param area float — the heading
    //         arg; carried from caller), time = System::Time::Time(&t, 30.0f [0x1401721cc]),
    //         p6 = 1, p7 = -1.
    obj = WAR_Alloc(0x48, 8);
    void* pw = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // [RBP+0x40]
        uint64_t timeVal = 0;                                             // [RBP-0x18]
        if (s_war_TimeCtor) s_war_TimeCtor(&timeVal, WAR_FloatBits(kWar_f_30p0_1401721cc));
        // XMM2 = [RBP+0x60] = param_5 (5th incoming arg, the caller-supplied heading float;
        // [RBP+0x60] is the 5th-arg home slot, one below param_6@[RBP+0x68]).
        // (disasm 0x1400bee2e MOVSS XMM2,[RBP+0x60])
        float heading = param5_heading;
        if (s_war_PerformWork7)
            pw = s_war_PerformWork7(obj, &entityInner, heading, 0xe, static_cast<int64_t>(timeVal), 1, -1);
    }
    WAR_IbpCtor(&ibpC, pw);              // [RBP+0x50]
    WAR_Append(list, &ibpC);

    // --- MoveTo action (Fast=2, destination=param_4) (0x1400bee5d..0x1400beef2) ---
    obj = WAR_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        WAR_CheckPtr(citizenIBP);
        uintptr_t entInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // [RBP+0x40]
        WAR_CheckPtr(&entInner);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;            // [RBP-0x18]
        if (s_war_FindCompFamily) familyIBP = s_war_FindCompFamily(entInner, &famRet);
        float speed = s_war_pf_172280 ? *s_war_pf_172280 : 0.0f;          // [0x140172280] = -1.0f
        // disasm: R8D = 2 (Fast; LEA R8D,[R9+2]), R9D=0, [RSP+0x20]=R13(=param_4),
        //         [RSP+0x28]=0xe1f, [RSP+0x30]=0, [RSP+0x38]=speed
        if (s_war_MoveTo) mv = s_war_MoveTo(obj, familyIBP, 2 /*Fast*/, 0, position, 0xe1f, 0, speed);
    }
    WAR_IbpCtor(&ibpC, mv);              // [RBP+0x50]
    WAR_Append(list, &ibpC);

    // --- ShowTool (Hammer = 5) action (0x1400beef7..0x1400bef3e) ---
    obj = WAR_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // [RBP+0x40]
        // disasm: MOV R8D,5 -> toolType = 5 (Hammer)
        if (s_war_ShowTool) st2 = s_war_ShowTool(obj, &entityInner, 5);
    }
    WAR_IbpCtor(&ibpC, st2);             // [RBP+0x50]
    WAR_Append(list, &ibpC);
}
