// code/Game/Work/work_act_consume.cpp
// Work action-queue builder for the Consume/Produce work task.
//
// 1:1 reverse-engineering port of FUN_1400bef70 @ 0x1400bef70 (Application-x64.exe)
// -> WorkAct_ConsumeProduceCtor_bef70. Referenced from consumeproduce.cpp
// (s_fn_SetupWorkChain). Transcribed literally from the Ghidra disassembly; no
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
// float-const pointers. All statics prefixed s_wcp_* to avoid ODR clashes. No hooks.h,
// no RegisterExeAddr.
//
// Every non-obvious offset/constant cites its disasm address.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Sub-callee typedefs (all exe code, resolved via ResolveAddr)
// ============================================================

// FUN_1400c5400 (TaskListCleanup) — cancel pending work actions on the list.
typedef void  (*WCP_fn_TaskListCleanup_t)(void* list);
// FUN_1400c54e0 (TaskListAppend) — append an action IBP to the task list.
typedef void  (*WCP_fn_TaskListAppend_t)(void* list, void* actionIBP);
// FUN_1400032e0 — task-list array grow (capacity arg).
typedef void  (*WCP_fn_ArrayGrow_t)(void* list, int newCapacity);

// FUN_1400cb1d0 SendComponentMessage_Action_CopyCtor
//   disasm: RCX=self, RDX=&entityInner, R8=&msgId(u32*), R9D=p4, [RSP+0x20]=&innerIbp, [RSP+0x28]=p6
typedef void* (*WCP_fn_SendCompMsg_t)(void* self, void* p2, uint32_t* p3, uint32_t p4, void* p5, uint32_t p6);
// FUN_1400c5da0 CompleteTask_Action_CopyCtor
//   disasm: RCX=self, EDX=workId, R8=&entityInner, R9=&innerIbp, [RSP+0x20]=p5
typedef void* (*WCP_fn_CompleteTask_t)(void* self, uint32_t workId, void* p3, void* p4, uint32_t p5);
// FUN_140042890 ShowTool_Action_Ctor
//   disasm: RCX=self, RDX=&entityInner, R8D=toolType
typedef void* (*WCP_fn_ShowTool_t)(void* self, void* p2, int toolType);
// FUN_1400c71a0 CreateRawMaterial_Action_CopyCtor
//   disasm (0x1400bf12f..0x1400bf1a2): RCX=self, RDX=p2, XMM2=p3(float), R9=p4,
//   [RSP+0x20]=p5, [RSP+0x28]=p6, [RSP+0x30]=p7(u32), [RSP+0x38]=p8(u32),
//   [RSP+0x40]=p9(u8), [RSP+0x48]=p10, [RSP+0x50]=p11(u32).
typedef void* (*WCP_fn_CreateRawMaterial_t)(void* self, uint64_t p2, float p3, uint64_t* p4,
                                            uint64_t* p5, uint64_t p6, uint32_t p7, uint32_t p8,
                                            uint8_t p9, uint64_t* p10, uint32_t p11);
// FUN_1400c9720 PerformWork_Action_Ctor7
//   disasm: RCX=self, RDX=&entityInner, XMM2=heading(float), R9D=actionType,
//   [RSP+0x20]=time, [RSP+0x28]=p6(u8), [RSP+0x30]=p7(int).
typedef void* (*WCP_fn_PerformWork7_t)(void* self, void* entity, float heading, int actionType,
                                       int64_t time, uint8_t p6, int p7);
// FUN_1400c9170 MoveTo_Action_Ctor
//   disasm: RCX=self, RDX=familyIBP, R8D=pathType, R9D=p4, [RSP+0x20]=dest,
//   [RSP+0x28]=p6(0xe1f), [RSP+0x30]=p7(0), [RSP+0x38]=speed(float).
typedef void* (*WCP_fn_MoveTo_t)(void* self, void* familyIBP, int pathType, int p4,
                                 float* dest, int p6, int p7, float speed);
// FUN_1400c6440 ConsumeRawMaterial_Action_Ctor3 — RCX=self, RDX=&inner(u64*).
typedef void* (*WCP_fn_ConsumeRawMat3_t)(void* self, uint64_t* p2);
// FUN_1400c64b0 Entity_impl_64b0 (consume-list append) — RCX=self, RDX=resType, R8D=count.
typedef void  (*WCP_fn_ConsumeListAppend_t)(int64_t self, int64_t resType, int count);
// FUN_140006270 Entity_FindComponent_Storage — RCX=entityInner, RDX=&retIBP -> retIBP.
typedef void* (*WCP_fn_FindComponentStorage_t)(int64_t entityInner, void* retIBP);
// FUN_1400012d0 Entity_FindComponent_Family — RCX=entityInner, RDX=&retIBP -> retIBP.
typedef void* (*WCP_fn_FindComponentFamily_t)(int64_t entityInner, void* retIBP);
// FUN_14000b6f0 Entity_FindStateMachine — RCX=entity, RDX=&retIBP.
typedef void* (*WCP_fn_FindStateMachine_t)(int64_t entity, void* retIBP);
// FUN_140119470 WeightedRandomIndex — RCX=param -> uint64 index.
typedef uint64_t (*WCP_fn_WeightedRandomIndex_t)(int64_t param_1);

// --- dispatch slots (deref once for the runtime fn ptr) ---
// 0x140152b60 System::MemoryAllocator::MemAlloc(size, align).
typedef void* (*WCP_fn_MemAlloc_t)(uint64_t size, uint32_t align);
// 0x140152b00 System::Time::Time(retval, float seconds).
typedef void* (*WCP_fn_TimeCtor_t)(void* retval, float seconds);
// 0x140152a98 Object::IsDerivedFrom(obj, typeId) -> bool.
typedef bool  (*WCP_fn_IsDerivedFrom_t)(void* obj, uint32_t typeId);

// ============================================================
// Resolved statics
// ============================================================
static WCP_fn_TaskListCleanup_t       s_wcp_TaskListCleanup   = nullptr;
static WCP_fn_TaskListAppend_t        s_wcp_TaskListAppend    = nullptr;
static WCP_fn_ArrayGrow_t             s_wcp_ArrayGrow         = nullptr;
static WCP_fn_SendCompMsg_t           s_wcp_SendCompMsg       = nullptr;
static WCP_fn_CompleteTask_t          s_wcp_CompleteTask      = nullptr;
static WCP_fn_ShowTool_t              s_wcp_ShowTool          = nullptr;
static WCP_fn_CreateRawMaterial_t     s_wcp_CreateRawMat      = nullptr;
static WCP_fn_PerformWork7_t          s_wcp_PerformWork7      = nullptr;
static WCP_fn_MoveTo_t                s_wcp_MoveTo            = nullptr;
static WCP_fn_ConsumeRawMat3_t        s_wcp_ConsumeRawMat3    = nullptr;
static WCP_fn_ConsumeListAppend_t     s_wcp_ConsumeListAppend = nullptr;
static WCP_fn_FindComponentStorage_t  s_wcp_FindCompStorage   = nullptr;
static WCP_fn_FindComponentFamily_t   s_wcp_FindCompFamily    = nullptr;
static WCP_fn_FindStateMachine_t      s_wcp_FindStateMachine  = nullptr;
static WCP_fn_WeightedRandomIndex_t   s_wcp_WeightedRandIdx   = nullptr;

static WCP_fn_MemAlloc_t      s_wcp_MemAlloc       = nullptr;
static WCP_fn_TimeCtor_t      s_wcp_TimeCtor       = nullptr;
static WCP_fn_IsDerivedFrom_t s_wcp_IsDerivedFrom  = nullptr;

// Runtime-filled globals (read live via pointer; read 0 at static-link time).
static const uint32_t* s_wcp_pMsgId_c70e8 = nullptr;  // 0x1401c70e8 (SendComponentMessage #1 msg id)
static const uint32_t* s_wcp_pTid_c7388   = nullptr;  // 0x1401c7388 (component type id for the two search loops)
// Float .rdata constants.
static const float* s_wcp_pf_17214c = nullptr;        // 0x14017214c == 1.0f (CreateRawMaterial XMM2)
static const float* s_wcp_pf_172280 = nullptr;        // 0x140172280 == -1.0f (MoveTo speed)

static bool s_wcp_resolved = false;

static void WCP_ResolveAll() {
    if (s_wcp_resolved) return;
    s_wcp_resolved = true;

    s_wcp_TaskListCleanup   = reinterpret_cast<WCP_fn_TaskListCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_wcp_TaskListAppend    = reinterpret_cast<WCP_fn_TaskListAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_wcp_ArrayGrow         = reinterpret_cast<WCP_fn_ArrayGrow_t>(ResolveAddr(Module::Exe, 0x1400032e0));
    s_wcp_SendCompMsg       = reinterpret_cast<WCP_fn_SendCompMsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_wcp_CompleteTask      = reinterpret_cast<WCP_fn_CompleteTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_wcp_ShowTool          = reinterpret_cast<WCP_fn_ShowTool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_wcp_CreateRawMat      = reinterpret_cast<WCP_fn_CreateRawMaterial_t>(ResolveAddr(Module::Exe, 0x1400c71a0));
    s_wcp_PerformWork7      = reinterpret_cast<WCP_fn_PerformWork7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_wcp_MoveTo            = reinterpret_cast<WCP_fn_MoveTo_t>(ResolveAddr(Module::Exe, 0x1400c9170));
    s_wcp_ConsumeRawMat3    = reinterpret_cast<WCP_fn_ConsumeRawMat3_t>(ResolveAddr(Module::Exe, 0x1400c6440));
    s_wcp_ConsumeListAppend = reinterpret_cast<WCP_fn_ConsumeListAppend_t>(ResolveAddr(Module::Exe, 0x1400c64b0));
    s_wcp_FindCompStorage   = reinterpret_cast<WCP_fn_FindComponentStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_wcp_FindCompFamily    = reinterpret_cast<WCP_fn_FindComponentFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_wcp_FindStateMachine  = reinterpret_cast<WCP_fn_FindStateMachine_t>(ResolveAddr(Module::Exe, 0x14000b6f0));
    s_wcp_WeightedRandIdx   = reinterpret_cast<WCP_fn_WeightedRandomIndex_t>(ResolveAddr(Module::Exe, 0x140119470));

    // Dispatch slots — deref once.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_wcp_MemAlloc      = *reinterpret_cast<WCP_fn_MemAlloc_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b00)) s_wcp_TimeCtor      = *reinterpret_cast<WCP_fn_TimeCtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a98)) s_wcp_IsDerivedFrom = *reinterpret_cast<WCP_fn_IsDerivedFrom_t*>(s);
    // Fallback: engine_funcs IsDerivedFrom (same Runtime target) if the slot isn't a dispatch slot in standalone.
    if (!s_wcp_IsDerivedFrom) s_wcp_IsDerivedFrom = reinterpret_cast<WCP_fn_IsDerivedFrom_t>(fn_IsDerivedFrom);

    s_wcp_pMsgId_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_wcp_pTid_c7388   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7388));
    s_wcp_pf_17214c    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_wcp_pf_172280    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
}

// ---- small WA-style helpers ----
static inline void* WCP_Alloc(uint64_t size) {
    // disasm: MOV EDX,8 (align); ECX=size; CALL [0x140152b60].
    if (s_wcp_MemAlloc) return s_wcp_MemAlloc(size, 8);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, 8);
    return nullptr;
}
static inline void WCP_CheckPtr(void* p)            { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline void WCP_IbpCtor(void* ibp, void* o)  { if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibp, o); }
static inline void WCP_Append(void* list, void* ibp){ if (s_wcp_TaskListAppend) s_wcp_TaskListAppend(list, ibp); }

// Search a component list (base = listBase) for an element whose vtable GetTypeId
// (vtable+0x10) equals *typeId, then IsDerivedFrom; returns the matched component or
// null. Mirrors the two identical loops at 0x1400bf2a7.. and 0x1400bf3a4..
//   listBase: ptr to a struct with { +0x28 = element-ptr array base, +0x30 = count }.
static void* WCP_FindComponentDerived(uintptr_t listBase) {
    uint32_t typeId = s_wcp_pTid_c7388 ? *s_wcp_pTid_c7388 : 0;  // DWORD_1401c7388 (runtime-filled)
    void* result = nullptr;
    int count = *reinterpret_cast<int*>(listBase + 0x30);        // [list+0x30]
    if (count > 0) {
        uintptr_t arr = *reinterpret_cast<uintptr_t*>(listBase + 0x28);  // [list+0x28]
        int idx = 0;
        for (;;) {
            void* elem = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(idx) * 8);
            // vcall GetTypeId at vtable+0x10
            uintptr_t vt = *reinterpret_cast<uintptr_t*>(elem);
            typedef uint32_t (*GetTypeId_t)(void*);
            uint32_t tid = (*reinterpret_cast<GetTypeId_t*>(vt + 0x10))(elem);
            if (tid == typeId) {
                void* comp = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(idx) * 8);
                if (s_wcp_IsDerivedFrom && s_wcp_IsDerivedFrom(comp, typeId))
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
    int cap = *reinterpret_cast<int*>(list + 0x8);        // [RSI+0x8]
    if (cap == 0) {
        cap = 0x10;
        if (s_wcp_ArrayGrow) s_wcp_ArrayGrow(reinterpret_cast<void*>(list), cap);
    } else {
        int n = *reinterpret_cast<int*>(list + 0x18) + 1; // [RSI+0x18]+1
        if (n == cap) {
            cap *= 2;
            if (s_wcp_ArrayGrow) s_wcp_ArrayGrow(reinterpret_cast<void*>(list), cap);
        }
    }

    // Cleanup walk over the chain at array[count] (clear flag +0x18, call vtable+0x38).
    int count = *reinterpret_cast<int*>(list + 0x18);     // [RSI+0x18] (signed)
    if (count >= 0) {
        uintptr_t base = *reinterpret_cast<uintptr_t*>(list);  // [RSI]
        uintptr_t node = *reinterpret_cast<uintptr_t*>(base + static_cast<uintptr_t>(count) * 8);
        while (node != 0) {
            WCP_CheckPtr(reinterpret_cast<void*>(&node));
            if (*reinterpret_cast<uint8_t*>(node + 0x18) != 0) {
                WCP_CheckPtr(reinterpret_cast<void*>(&node));
                *reinterpret_cast<uint8_t*>(node + 0x18) = 0;
                uintptr_t vt = *reinterpret_cast<uintptr_t*>(node);
                typedef void (*Detach_t)(uintptr_t, void*);
                (*reinterpret_cast<Detach_t*>(vt + 0x38))(node, reinterpret_cast<void*>(list));
            }
            WCP_CheckPtr(reinterpret_cast<void*>(&node));
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
    WCP_ResolveAll();

    // list base = (*(citizen)+0x278) + 0x48  (0x1400befaf..0x1400befd1)
    WCP_CheckPtr(param_1);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(param_1);
    WCP_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    uintptr_t list = aiComp + 0x48;          // RSI / plVar15
    if (s_wcp_TaskListCleanup) s_wcp_TaskListCleanup(reinterpret_cast<void*>(list));

    void* obj;

    // --- SendComponentMessage #1 -> stored at aiComp+0x58 (DeletePtr old first) (0x1400befd6..) ---
    obj = WCP_Alloc(0x40);                    // MOV EDX,8; LEA ECX,[RDX+0x38] => 0x40
    void* sm1 = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WCP_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_wcp_pMsgId_c70e8 ? *s_wcp_pMsgId_c70e8 : 0;  // DWORD_1401c70e8
        WCP_CheckPtr(param_1);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);
        if (s_wcp_SendCompMsg) sm1 = s_wcp_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    uintptr_t ibpSm1 = 0;
    WCP_IbpCtor(&ibpSm1, sm1);                // ctor(&local_60, sm1)
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)  // [lVar2+0x58]
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpSm1;

    // --- CompleteTask action (0x1400bf06b..) ---
    obj = WCP_Alloc(0x38);                    // LEA ECX,[RDX+0x30] => 0x38
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WCP_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);     // *param_1
        if (s_wcp_CompleteTask) ct = s_wcp_CompleteTask(obj, param_2, &entityInner, &innerIbp, param_13);
    }
    uintptr_t ibpCt = 0;
    WCP_IbpCtor(&ibpCt, ct);
    WCP_Append(reinterpret_cast<void*>(list), &ibpCt);   // FUN_1400c54e0(plVar15, &local_70)

    // cVar4 = (char)param_8  (0x1400bf0d2)
    uint8_t cVar4 = static_cast<uint8_t>(param_8);

    if (cVar4 != 0) {
        // --- CreateRawMaterial action gated on the storage slot (0x1400bf0e0..) ---
        WCP_CheckPtr(param_1);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);  // *(*param_1+0x10)
        WCP_CheckPtr(&entityInner);
        uintptr_t storRet = 0;
        void* storIbp = nullptr;
        if (s_wcp_FindCompStorage) storIbp = s_wcp_FindCompStorage(static_cast<int64_t>(entityInner), &storRet);
        WCP_CheckPtr(storIbp);
        uint32_t uVar1 = *reinterpret_cast<uint32_t*>(*reinterpret_cast<uintptr_t*>(storIbp) + 0x38);  // *(*pIVar11+0x38) -> EDI

        obj = WCP_Alloc(0x58);                // LEA ECX,[RDX+0x50] => 0x58
        void* crm = nullptr;
        if (obj) {
            uintptr_t local60 = 0;
            WCP_IbpCtor(&local60, nullptr);   // ctor(&local_60, 0)  (p10)
            WCP_CheckPtr(param_1);
            uint64_t local70 = *reinterpret_cast<uintptr_t*>(param_1);                 // *param_1  -> p4
            uint64_t local68 = *reinterpret_cast<uintptr_t*>(local70 + 0x248);         // *(*param_1+0x248) -> p5
            float xmm2 = s_wcp_pf_17214c ? *s_wcp_pf_17214c : 1.0f;                    // [0x14017214c]==1.0f (g_fUVScale)
            // p6 = leftover 0 in [RSP+0x28]; p7=uVar1; p8=param_11; p9=0; p10=&local60; p11=4.
            if (s_wcp_CreateRawMat)
                crm = s_wcp_CreateRawMat(obj, param_7, xmm2, &local70, &local68,
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
                if (s_wcp_SendCompMsg) sm2 = s_wcp_SendCompMsg(obj, &e2, &msgId, param_10, &innerIbp, 0);
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
        if (s_wcp_ShowTool) st1 = s_wcp_ShowTool(obj, &entityInner, -1);
    }
    uintptr_t ibpSt1 = 0;
    WCP_IbpCtor(&ibpSt1, st1);
    WCP_Append(reinterpret_cast<void*>(list), &ibpSt1);

    // --- 1st component search over *(*param_3+0x28); derive toolType (0x1400bf293..) ---
    WCP_CheckPtr(param_3);
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
        if (s_wcp_TimeCtor) s_wcp_TimeCtor(timeRes, param_12);           // Time::Time(&local_70, param_12)
        int64_t time = static_cast<int64_t>(timeRes[0]);                 // *puVar12
        // heading=param_5 (XMM2), actionType=EDI (R9D), p6=1, p7=-1.
        if (s_wcp_PerformWork7) pw7 = s_wcp_PerformWork7(obj, &entityInner, param_5, actionType, time, 1, -1);
    }
    uintptr_t ibpPw7 = 0;
    WCP_IbpCtor(&ibpPw7, pw7);
    WCP_Append(reinterpret_cast<void*>(list), &ibpPw7);

    // --- consume-list build (0x1400bf394..) gated on cVar4 (R12B) ---
    if (cVar4 != 0) {
        // 2nd component search over param_6.
        void* foundComp2 = WCP_FindComponentDerived(static_cast<uintptr_t>(param_6));

        // state machine + weighted random recipe index.
        WCP_CheckPtr(param_3);
        uintptr_t smRet = 0;
        if (s_wcp_FindStateMachine) s_wcp_FindStateMachine(*reinterpret_cast<uintptr_t*>(param_3), &smRet);  // *param_3
        WCP_CheckPtr(&smRet);
        uint64_t widx = s_wcp_WeightedRandIdx ? s_wcp_WeightedRandIdx(static_cast<int64_t>(smRet)) : 0;
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
                if (s_wcp_ConsumeRawMat3) crmAct = s_wcp_ConsumeRawMat3(obj, &inner);
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
                    WCP_CheckPtr(reinterpret_cast<void*>(&consumeIbp));
                    int64_t resType = *reinterpret_cast<int64_t*>(subArr + static_cast<uintptr_t>(j) * 0x10);      // [subArr + j*0x10]
                    int cnt = *reinterpret_cast<int*>(subArr + static_cast<uintptr_t>(j) * 0x10 + 8);             // [subArr + j*0x10 + 8]
                    if (s_wcp_ConsumeListAppend)
                        s_wcp_ConsumeListAppend(static_cast<int64_t>(consumeIbp), resType, cnt);
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
        WCP_CheckPtr(param_1);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);  // *(*param_1+0x10)
        WCP_CheckPtr(&entityInner);
        uintptr_t famRet = 0;
        void* familyIbp = nullptr;
        if (s_wcp_FindCompFamily) familyIbp = s_wcp_FindCompFamily(static_cast<int64_t>(entityInner), &famRet);
        float speed = s_wcp_pf_172280 ? *s_wcp_pf_172280 : -1.0f;          // [0x140172280]==-1.0f
        if (s_wcp_MoveTo) mv = s_wcp_MoveTo(obj, familyIbp, pathType, 0, param_4, 0xe1f, 0, speed);
    }
    uintptr_t ibpMv = 0;
    WCP_IbpCtor(&ibpMv, mv);                   // ctor(&local_58, mv)
    WCP_AppendInline(list, ibpMv);             // inlined append (0x1400bf5be..)

    // --- final ShowTool (toolType) action (0x1400bf65c..) ---
    obj = WCP_Alloc(0x30);                     // LEA ECX,[RDX+0x28] => 0x30
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);  // *param_1 (R12)
        if (s_wcp_ShowTool) st2 = s_wcp_ShowTool(obj, &entityInner, static_cast<int>(toolType));
    }
    uintptr_t ibpSt2 = 0;
    WCP_IbpCtor(&ibpSt2, st2);                 // ctor(&local_res18, st2)
    WCP_AppendInline(list, ibpSt2);            // inlined append (0x1400bf69c..)
}
