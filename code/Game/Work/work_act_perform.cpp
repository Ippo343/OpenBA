// code/Game/Work/work_act_perform.cpp
// PerformWork work-action-queue builders (Work action-ctor sub-batch).
//
// Faithful 1:1 ports of two exe helpers that build a citizen's work-action chain
// and append it to the CitizenAIComponent task list (citizen+0x278 -> aiComp,
// list base aiComp+0x48; the SendComponentMessage action is stored at aiComp+0x58
// after DeletePtr-ing the prior one). Each appended sub-action is allocated via the
// MemAlloc static slot (0x140152b60) and wrapped in an InternalBasePtr before being
// appended (FUN_1400c54e0) — exactly as the disassembly does.
//
//   1. PerformWork_Action_CopyCtor (0x1400ba9f0) -> WorkAct_PerformWorkCopyCtor_ba9f0
//   2. PerformWork_Action_Ctor     (0x1400bffd0) -> WorkAct_PerformWorkCtor_bffd0
//
// This is a REVERSE-ENGINEERING transcription of the disassembly; do NOT "improve"
// behavior. Every offset/size/immediate below is annotated with its disasm address.
// Sub-callees are resolved via ResolveAddr(Module::Exe, 0xADDR) and null-guarded
// (skipped if unregistered, matching project semantics). They are NOT registered here.
//

#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Sub-callee / dispatch-slot resolution (self-contained, s_wap_* prefix)
// ============================================================
// Engine base helpers (InternalBasePtr ctor/checkptr/deleteptr) come from engine_funcs.h.
// NOTE: these two functions reach their base helpers via the .rdata dispatch slots that
// the disassembly actually uses (CheckPtr=0x140152ab0, InternalBasePtr_Ctor=0x140152a90,
// DeletePtr=0x140151ab8) — different slots than some sibling ports — so resolve those
// slots directly and deref once, with engine_funcs fallbacks.

// --- exe code sub-callees (0x1400xxxxx → ResolveAddr returns fn ptr directly) ---

// Entity_impl_29a0 (0x1400c29a0): compute heading float toward path target (XMM0).
typedef float  (*WAP_fn_Entity29a0_t)(float* worldPos, void* innerIbp);
// FUN_1400c5400 (TaskListCleanup) — cancel pending work actions on the list.
typedef void   (*WAP_fn_TaskListCleanup_t)(void* list);
// FUN_1400c54e0 (TaskListAppend) — append an action IBP to the task list.
typedef void   (*WAP_fn_TaskListAppend_t)(void* list, void* actionIBP);
// Entity_FindComponent_Family (0x1400012d0) — member-style: RCX=entity, RDX=retIBP, ret RAX.
typedef void*  (*WAP_fn_FindComponentFamily_t)(uintptr_t entity, void* retIBP);
// Entity_FindComponent_Storage (0x140006270) — member-style: RCX=entity, RDX=retIBP.
typedef void*  (*WAP_fn_FindComponentStorage_t)(uintptr_t entity, void* retIBP);
// StorageComponent_QueryCount (0x14012cf10) — RCX=storageComp -> int (EAX).
typedef int    (*WAP_fn_StorageQueryCount_t)(void* storageComp);

// Action constructor / copy-constructor sub-callees. All return the constructed
// object pointer in RAX (or null), passed straight to InternalBasePtr_Ctor.
//   SendComponentMessage_Action_CopyCtor (0x1400cb1d0)
typedef void*  (*WAP_fn_SendCompMsg_t)(void* self, void* entityInnerPtr, uint32_t* msgIdPtr, uint32_t p4, void* innerIbp, uint32_t p6);
//   CompleteTask_Action_CopyCtor (0x1400c5da0)
typedef void*  (*WAP_fn_CompleteTask_t)(void* self, uint32_t workId, void* entityInnerPtr, void* innerIbp, uint32_t p5);
//   ShowTool_Action_Ctor (0x140042890): RCX=self, RDX=&entityInner, R8D=toolType.
typedef void*  (*WAP_fn_ShowTool_t)(void* self, void* entityInnerPtr, int toolType);
//   CompleteRoad_Action_CopyCtor (0x1400c59d0): RCX=self, EDX=p2(int), R8D=p3(int), R9B=p4(byte).
typedef void*  (*WAP_fn_CompleteRoad_t)(void* self, int p2, int p3, uint8_t p4);
//   ConsumeRawMaterial_Action_CopyCtor (0x1400c63b0): RCX=self, RDX=&entityInner, R8=p3(int64), R9B(byte).
typedef void*  (*WAP_fn_ConsumeRawMaterial_t)(void* self, void* entityInnerPtr, int64_t p3, uint8_t p4);
//   PickupStoredRawMaterial_Action_CopyCtor (0x1400ca520):
//     RCX=self, RDX=&entityInner, R8=&srcInner, R9=p4(int64), [stack]=p5(int)=-1, [stack]=p6(byte)=1.
typedef void*  (*WAP_fn_PickupStored_t)(void* self, void* entityInnerPtr, void* srcInnerPtr, int64_t p4, int p5, uint8_t p6);
// PerformWork_Action_Ctor7 (0x1400c9720). Win64 ABI (verified via disassembly):
//   RCX=self, RDX=&entityInner, XMM2=heading (3rd slot, float), R9D=actionType (4th slot, int),
//   [stack]=time(int64), [stack]=byte, [stack]=int.
typedef void*  (*WAP_fn_PerformWork7_t)(void* self, void* entityInnerPtr, float heading, int actionType, int64_t time, uint8_t p6, int p7);
// MoveTo_Action_Ctor (0x1400c9170): RCX=self, RDX=familyIBP, R8D=pathType, R9D=p4,
//   [stack]=float* dest, [stack]=int(0xe1f), [stack]=int(0), XMM?=speed (last positional float).
typedef void*  (*WAP_fn_MoveTo_t)(void* self, void* familyIBP, int pathType, int p4, float* dest, int p6, int p7, float speed);

// --- dispatch slots (deref once to get the runtime fn ptr) ---
typedef void   (*WAP_fn_IbpCtor_t)(void* ibp, void* obj);     // 0x140152a90
typedef void   (*WAP_fn_DeletePtr_t)(void* ibp);              // 0x140151ab8
typedef void*  (*WAP_fn_MemAlloc_t)(uint64_t size, uint32_t align); // 0x140152b60
typedef void*  (*WAP_fn_ObjInternalCtor_t)(void* self);       // 0x140152768
typedef void*  (*WAP_fn_TimeCtor_t)(void* retval, float seconds);   // 0x140152b00
// 0x1401512c0 — ?Normalize@Vector3@Math@@QEBA?AV12@XZ. Returns a NEW Vector3 BY VALUE,
// so the Microsoft x64 ABI passes a hidden return-slot pointer in RDX (>=12 bytes) and
// the retail call site loads it: `lea -0x41(%rbp),%rdx` at 0x1400bab2f, right before
// `lea -0x29(%rbp),%rcx` and the call.
//
// ⚠ FIXED 2026-08-22. This typedef used to take ONE argument, so Normalize wrote its
// three floats through whatever RDX happened to hold. work_base_actionctor.cpp carries
// the identical function with the correct two-arg form AND a comment describing this
// exact failure ("the organic herder's first-claim crash") — the fix simply never
// reached this second copy, because this call site was unreachable: FireWork returned
// early on found=0 for as long as no water source was ever registered. The moment the
// well registration landed, the very first bucket run faulted inside Runtime's
// Vector3::Normalize writing to a Runtime CODE address. See
// reference_one_retail_fn_ported_twice_diverges and reference_byvalue_return_hidden_rdx.
// B.3.1 step 2: the three Vector3 slots above (0x140152a30 ctor, 0x1401512c0
// Normalize, 0x1401512d8 Heading) are now direct calls on Math::Vector3; the
// typedefs and the hidden-RDX shape they encoded are gone with the DLL.

static WAP_fn_Entity29a0_t          s_wap_Entity29a0      = nullptr;
static WAP_fn_TaskListCleanup_t     s_wap_TaskCleanup     = nullptr;
static WAP_fn_TaskListAppend_t      s_wap_TaskAppend      = nullptr;
static WAP_fn_FindComponentFamily_t s_wap_FindFamily      = nullptr;
static WAP_fn_FindComponentStorage_t s_wap_FindStorage    = nullptr;
static WAP_fn_StorageQueryCount_t   s_wap_QueryCount      = nullptr;
static WAP_fn_SendCompMsg_t         s_wap_SendCompMsg     = nullptr;
static WAP_fn_CompleteTask_t        s_wap_CompleteTask    = nullptr;
static WAP_fn_ShowTool_t            s_wap_ShowTool        = nullptr;
static WAP_fn_CompleteRoad_t        s_wap_CompleteRoad    = nullptr;

// Road-wave counter (standalone/road_probe.cpp): CompleteRoad actions actually built
// and appended to a citizen task list.
static WAP_fn_ConsumeRawMaterial_t  s_wap_ConsumeRaw      = nullptr;
static WAP_fn_PickupStored_t        s_wap_PickupStored    = nullptr;
static WAP_fn_PerformWork7_t        s_wap_PerformWork7    = nullptr;
static WAP_fn_MoveTo_t              s_wap_MoveTo          = nullptr;

static WAP_fn_IbpCtor_t             s_wap_IbpCtor         = nullptr;
static WAP_fn_DeletePtr_t           s_wap_DeletePtr       = nullptr;
static WAP_fn_MemAlloc_t            s_wap_MemAlloc        = nullptr;
static WAP_fn_ObjInternalCtor_t     s_wap_ObjInternalCtor = nullptr;
static WAP_fn_TimeCtor_t            s_wap_TimeCtor        = nullptr;

// Inline-object vtables (exe .rdata) + .rdata float/dword constants.
static void* s_wap_vtable_Action      = nullptr;  // 0x14015faa0
static void* s_wap_vtable_PerformWork = nullptr;  // 0x14015fea0
static const uint32_t* s_wap_pDword_c70e8 = nullptr; // 0x1401c70e8 (SendComponentMessage #1 target type-id = CitizenAIComponent)
static const uint32_t* s_wap_pDword_c41e0 = nullptr; // 0x1401c41e0 (ba9f0 SendComponentMessage #2 target type-id = FireComponent)
static const float*    s_wap_pf_172124    = nullptr; // 0x140172124 = 0.75f  (PerformWork Time #1)
static const float*    s_wap_pf_172188    = nullptr; // 0x140172188 = 3.5f   (ba9f0 MoveTo speed)
static const float*    s_wap_pf_172194    = nullptr; // 0x140172194 = 4.0f   (bffd0 PerformWork_Ctor7 Time)
static const float*    s_wap_pf_172280    = nullptr; // 0x140172280 = -1.0f  (bffd0 MoveTo speed)
static const float*    s_wap_pf_1722a8    = nullptr; // 0x1401722a8 = -1000.0f (bffd0 PerformWork_Ctor7 heading)

static bool s_wap_resolved = false;

static void ResolveAll() {
    if (s_wap_resolved) return;
    s_wap_resolved = true;

    s_wap_Entity29a0   = reinterpret_cast<WAP_fn_Entity29a0_t>(ResolveAddr(Module::Exe, 0x1400c29a0));
    s_wap_TaskCleanup  = reinterpret_cast<WAP_fn_TaskListCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_wap_TaskAppend   = reinterpret_cast<WAP_fn_TaskListAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_wap_FindFamily   = reinterpret_cast<WAP_fn_FindComponentFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_wap_FindStorage  = reinterpret_cast<WAP_fn_FindComponentStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_wap_QueryCount   = reinterpret_cast<WAP_fn_StorageQueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_wap_SendCompMsg  = reinterpret_cast<WAP_fn_SendCompMsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_wap_CompleteTask = reinterpret_cast<WAP_fn_CompleteTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_wap_ShowTool     = reinterpret_cast<WAP_fn_ShowTool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_wap_CompleteRoad = reinterpret_cast<WAP_fn_CompleteRoad_t>(ResolveAddr(Module::Exe, 0x1400c59d0));
    s_wap_ConsumeRaw   = reinterpret_cast<WAP_fn_ConsumeRawMaterial_t>(ResolveAddr(Module::Exe, 0x1400c63b0));
    s_wap_PickupStored = reinterpret_cast<WAP_fn_PickupStored_t>(ResolveAddr(Module::Exe, 0x1400ca520));
    s_wap_PerformWork7 = reinterpret_cast<WAP_fn_PerformWork7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_wap_MoveTo       = reinterpret_cast<WAP_fn_MoveTo_t>(ResolveAddr(Module::Exe, 0x1400c9170));

    // Dispatch slots — deref once to get the runtime fn ptr (with engine_funcs fallbacks).
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a90)) s_wap_IbpCtor         = *reinterpret_cast<WAP_fn_IbpCtor_t*>(s);
    if (!s_wap_IbpCtor) s_wap_IbpCtor = reinterpret_cast<WAP_fn_IbpCtor_t>(fn_InternalBasePtr_Ctor);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140151ab8)) s_wap_DeletePtr       = *reinterpret_cast<WAP_fn_DeletePtr_t*>(s);
    if (!s_wap_DeletePtr) s_wap_DeletePtr = reinterpret_cast<WAP_fn_DeletePtr_t>(fn_InternalBasePtr_DeletePtr);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_wap_MemAlloc        = *reinterpret_cast<WAP_fn_MemAlloc_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152768)) s_wap_ObjInternalCtor = *reinterpret_cast<WAP_fn_ObjInternalCtor_t*>(s);
    if (!s_wap_ObjInternalCtor) s_wap_ObjInternalCtor = reinterpret_cast<WAP_fn_ObjInternalCtor_t>(fn_ObjectInternal_Ctor);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b00)) s_wap_TimeCtor        = *reinterpret_cast<WAP_fn_TimeCtor_t*>(s);

    s_wap_vtable_Action      = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015faa0));
    s_wap_vtable_PerformWork = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015fea0));
    s_wap_pDword_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_wap_pDword_c41e0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c41e0));
    s_wap_pf_172124    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172124));
    s_wap_pf_172188    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172188));
    s_wap_pf_172194    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172194));
    s_wap_pf_172280    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    s_wap_pf_1722a8    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401722a8));
}

// Small idiom wrappers matching the disasm's call shapes.
static inline void* WAP_Alloc(uint64_t size, uint32_t align) {
    if (s_wap_MemAlloc) return s_wap_MemAlloc(size, align);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, align);
    return nullptr;
}
static inline void  WAP_IbpCtor(void* ibp, void* obj) { if (s_wap_IbpCtor) s_wap_IbpCtor(ibp, obj); }
static inline void  WAP_Append(void* list, void* ibp) { if (s_wap_TaskAppend) s_wap_TaskAppend(list, ibp); }

// Build an inline PerformWork action object into 'o' (0x48 alloc, already non-null).
// Mirrors the repeated inline block at 0x1400babb1.. / 0x1400c0408.. etc.
//   entityInner  -> [o+0x20]    (citizen inner)
//   heading      -> [o+0x28]    (float)
//   actionType   -> [o+0x2c]    (int, 0x6 or 0x7)
//   timeVal      -> [o+0x30]    (int64 from System::Time::Time)
//   [o+0x44]     = 0xffffffff
static void WAP_BuildInlinePerformWork(void* o, uintptr_t entityInner, float heading,
                                       int actionType, int64_t timeVal) {
    if (s_wap_ObjInternalCtor) s_wap_ObjInternalCtor(o);          // 0x140152768
    uintptr_t p = reinterpret_cast<uintptr_t>(o);
    *reinterpret_cast<void**>(p)           = s_wap_vtable_Action;      // 0x14015faa0
    *reinterpret_cast<uint64_t*>(p + 0x10) = 0;
    *reinterpret_cast<uint8_t*>(p + 0x18)  = 0;
    *reinterpret_cast<void**>(p)           = s_wap_vtable_PerformWork; // 0x14015fea0
    *reinterpret_cast<uint64_t*>(p + 0x20) = 0;
    *reinterpret_cast<uintptr_t*>(p + 0x20) = entityInner;
    *reinterpret_cast<float*>(p + 0x28)    = heading;
    *reinterpret_cast<int*>(p + 0x2c)      = actionType;
    *reinterpret_cast<int64_t*>(p + 0x30)  = timeVal;
    *reinterpret_cast<uint64_t*>(p + 0x38) = 0;
    *reinterpret_cast<uint8_t*>(p + 0x40)  = 0;
    *reinterpret_cast<uint32_t*>(p + 0x44) = 0xffffffffu;
}

// ============================================================
// PerformWork_Action_CopyCtor (0x1400ba9f0) -> WorkAct_PerformWorkCopyCtor_ba9f0
// ============================================================
// Builds the forester/clear-area work-action chain for a citizen and stores it on the
// CitizenAIComponent task list (citizen+0x278, list base +0x48).
//   citizenIBP = InternalBasePtr*           (RCX)
//   workId     = u32                         (EDX -> CompleteTask workId)
//   pos3       = float*                      (R8, param_3: Vector3 minuend src + 2nd MoveTo dest)
//   pos4       = float*                      (R9, param_4: Vector3 subtrahend)
//   pos5       = float*                      (stack, param_5: Entity_impl_29a0 worldPos + 1st MoveTo dest)
//   srcEntity6 = longlong*                   (stack, param_6: 2nd SendComponentMessage entity inner)
void WorkAct_PerformWorkCopyCtor_ba9f0(void* citizenIBP, uint32_t workId,
                                       float* pos3, float* pos4, float* pos5,
                                       int64_t* srcEntity6) {
    ResolveAll();

    // list base = (*(citizen)+0x278) + 0x48 ; cleanup (0x1400baa2f..0x1400baa53)
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_wap_TaskCleanup) s_wap_TaskCleanup(list);

    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0;

    // --- SendComponentMessage #1 → aiComp+0x58 (DeletePtr old) (0x1400baa58..0x1400baae5) ---
    void* obj = WAP_Alloc(0x40, 8);  // 0x1400baa5d: ECX=0x38+8=0x40
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAP_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_wap_pDword_c70e8 ? *s_wap_pDword_c70e8 : 0; // 0x1401c70e8
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_wap_SendCompMsg) sm = s_wap_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0); // 0x1400cb1d0
    }
    WAP_IbpCtor(&ibpA, sm);
    // 0x1400baad0: CMP [R15+0x10] — R15 = aiComp+0x48, so this is (aiComp+0x48)+0x10 = aiComp+0x58.
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && s_wap_DeletePtr)
        s_wap_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA; // 0x1400baae5: MOV [R15+0x10],RAX

    // --- heading via Entity_impl_29a0(pos5, &{*param_6}) (0x1400baae9..0x1400bab02; XMM6) ---
    int64_t srcInner6 = srcEntity6 ? *srcEntity6 : 0; // 0x1400baaed: MOV RAX,[R13]; [RBP+0x7f]=RAX
    float headingA = 0.0f; // XMM6
    if (s_wap_Entity29a0) headingA = s_wap_Entity29a0(pos5, &srcInner6);

    // --- headingB via Vector3(pos4-pos3) → Normalize → Heading (0x1400bab05..0x1400bab46; XMM8) ---
    float headingB = 0.0f; // XMM8
    if (pos3 && pos4) {
        // 0x1400bab05: XMM = [pos4+k] - [pos3+k]  (k = 8,4,0 → z,y,x)
        Math::Vector3 v3(pos4[0] - pos3[0], pos4[1] - pos3[1], pos4[2] - pos3[2]);
        Math::Vector3 nv = v3.Normalize();   // was the hidden RDX return slot
        headingB = nv.Heading();
    }

    float timeC = s_wap_pf_172124 ? *s_wap_pf_172124 : 0.0f; // XMM7 = DAT_140172124 = 0.75f

    // --- inline PerformWork #1: heading=headingA(XMM6), actionType=7, time=Time(0.75) (0x1400bab4a..) ---
    obj = WAP_Alloc(0x48, 8); // 0x1400bab4f: ECX=0x40+8=0x48
    void* pw1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // 0x1400bab6c: [R14]
        uint64_t timeVal = 0;
        if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeC); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
        WAP_BuildInlinePerformWork(obj, entityInner, headingA, 0x7, static_cast<int64_t>(timeVal)); // 0x2c=7
        pw1 = obj;
    }
    WAP_IbpCtor(&ibpA, pw1);
    WAP_Append(list, &ibpA);

    // --- ShowTool (~Hoe = -1) (0x1400babf2..0x1400bac39) ---
    obj = WAP_Alloc(0x30, 8); // 0x1400babf7: ECX=0x28+8=0x30
    void* st1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_wap_ShowTool) st1 = s_wap_ShowTool(obj, &entityInner, ~0 /* 0x1400bac10: OR R8D,0xffffffff = -1 */);
    }
    WAP_IbpCtor(&ibpA, st1);
    WAP_Append(list, &ibpA);

    // --- CompleteTask (workId) (0x1400bac3e..0x1400bac9b) ---
    obj = WAP_Alloc(0x38, 8); // 0x1400bac43: ECX=0x30+8=0x38
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAP_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // 0x1400bac64: [R14]
        if (s_wap_CompleteTask) ct = s_wap_CompleteTask(obj, workId, &entityInner, &innerIbp, 0); // EDX=R12D=workId, [RSP+0x20]=0
    }
    WAP_IbpCtor(&ibpB, ct);
    WAP_Append(list, &ibpB);

    // --- SendComponentMessage #2 (msgId=DWORD_1401c41e0, entity=*param_6) (0x1400baca0..0x1400bad10) ---
    obj = WAP_Alloc(0x40, 8); // 0x1400baca5: ECX=0x38+8=0x40
    void* sm2 = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAP_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_wap_pDword_c41e0 ? *s_wap_pDword_c41e0 : 0; // 0x1401c41e0
        uintptr_t entityInner = srcEntity6 ? static_cast<uintptr_t>(*srcEntity6) : 0; // 0x1400baccf: MOV RCX,[R13]
        if (s_wap_SendCompMsg) sm2 = s_wap_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WAP_IbpCtor(&ibpC, sm2);
    WAP_Append(list, &ibpC);

    // --- inline PerformWork #2: heading=headingA(XMM6), actionType=6, time=Time(0.75) (0x1400bad15..) ---
    obj = WAP_Alloc(0x48, 8); // 0x1400bad1a: ECX=0x40+8=0x48
    void* pw2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeC); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
        WAP_BuildInlinePerformWork(obj, entityInner, headingA, 0x6, static_cast<int64_t>(timeVal)); // 0x1400bad7d: 0x2c=6
        pw2 = obj;
    }
    WAP_IbpCtor(&ibpA, pw2);
    WAP_Append(list, &ibpA);

    float speed = s_wap_pf_172188 ? *s_wap_pf_172188 : 0.0f; // XMM6 = DAT_140172188 = 3.5f

    // --- MoveTo #1 (Faster=4, dest=pos5) (0x1400badc3..0x1400bae5c) ---
    obj = WAP_Alloc(0x480, 8); // 0x1400badc8: ECX=0x480
    void* mv1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_wap_FindFamily) familyIBP = s_wap_FindFamily(entityInner, &famRet); // 0x1400012d0
        // 0x1400bae37: LEA R8D,[R9+4] (R9=0) → pathType=4 (Faster); [RSP+0x28]=0xe1f; [RSP+0x30]=0; XMM=speed
        if (s_wap_MoveTo) mv1 = s_wap_MoveTo(obj, familyIBP, 4, 0, pos5, 0xe1f, 0, speed);
    }
    WAP_IbpCtor(&ibpA, mv1);
    WAP_Append(list, &ibpA);

    // --- inline PerformWork #3: heading=headingB(XMM8), actionType=7, time=Time(0.75) (0x1400bae61..) ---
    obj = WAP_Alloc(0x48, 8); // 0x1400bae66: ECX=0x40+8=0x48
    void* pw3 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeC); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
        WAP_BuildInlinePerformWork(obj, entityInner, headingB, 0x7, static_cast<int64_t>(timeVal)); // 0x1400baebc: 0x2c=7, XMM8
        pw3 = obj;
    }
    WAP_IbpCtor(&ibpA, pw3);
    WAP_Append(list, &ibpA);

    // --- ShowTool (Barrow = 8) (0x1400baef4..0x1400baf3d) ---
    obj = WAP_Alloc(0x30, 8); // 0x1400baef9: ECX=0x28+8=0x30
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_wap_ShowTool) st2 = s_wap_ShowTool(obj, &entityInner, 8 /* 0x1400baf12: MOV R8D,8 (Barrow) */);
    }
    WAP_IbpCtor(&ibpA, st2);
    WAP_Append(list, &ibpA);

    // --- inline PerformWork #4: heading=headingB(XMM8), actionType=6, time=Time(0.75) (0x1400baf42..) ---
    obj = WAP_Alloc(0x48, 8); // 0x1400baf47: ECX=0x40+8=0x48
    void* pw4 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeC); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
        WAP_BuildInlinePerformWork(obj, entityInner, headingB, 0x6, static_cast<int64_t>(timeVal)); // 0x1400baf9d: 0x2c=6, XMM8
        pw4 = obj;
    }
    WAP_IbpCtor(&ibpA, pw4);
    WAP_Append(list, &ibpA);

    // --- MoveTo #2 (Slow=1, dest=pos3) (0x1400bafd5..0x1400bb064) ---
    obj = WAP_Alloc(0x480, 8); // 0x1400bafda: ECX=0x480
    void* mv2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_wap_FindFamily) familyIBP = s_wap_FindFamily(entityInner, &famRet);
        // 0x1400bb041: LEA R8D,[R9+1] (R9=0) → pathType=1 (Slow); dest=pos3 ([RBP+0x67]); XMM6=speed
        if (s_wap_MoveTo) mv2 = s_wap_MoveTo(obj, familyIBP, 1, 0, pos3, 0xe1f, 0, speed);
    }
    WAP_IbpCtor(&ibpA, mv2);
    WAP_Append(list, &ibpA);

    (void)ibpB; (void)ibpC; // appended above
}

// ============================================================
// PerformWork_Action_Ctor (0x1400bffd0) -> WorkAct_PerformWorkCtor_bffd0
// ============================================================
// Builds the road/raw-material work-action chain for a citizen and stores it on the
// CitizenAIComponent task list (citizen+0x278, list base +0x48).
//   citizenIBP = InternalBasePtr*  (RCX)            param_1
//   workId     = u32   (EDX = R15D)                 param_2  -> CompleteTask workId
//   roadP3     = u32   (R8D... actually R8=p3 ptr)  param_3  (int; CompleteRoad p2 + MoveTo#1 dest cast)
//   roadP4     = int64*/int (R9D = R12D)            param_4  -> CompleteRoad p3 / ConsumeRawMaterial p4 byte chain
//   completeRoadP5 = u32                            param_5  -> CompleteTask p5
//   roadByte6  = byte                               param_6  -> CompleteRoad p4 byte
//   consumeFlag8 = int64                            param_8  -> ConsumeRawMaterial cond + arg
//   loopCount9 = int                                param_9  -> optional pickup-chain count gate
//   pos10      = float*                             param_10 -> Entity_impl_29a0 worldPos + MoveTo#2 dest
//   srcEntity11 = longlong*                         param_11 -> pickup chain entity inner
//
// NOTE: param_3 is the home [RBP+0x57] (DWORD read as msg/road id) and ALSO reused as the
// MoveTo#1 destination float* (passed via the RCX home pointer). param_7 = completeRoadP5
// is the CompleteTask p5; in the disasm CompleteTask reads its p5 from [RBP+0x67] (param_7).
void WorkAct_PerformWorkCtor_bffd0(void* citizenIBP, uint32_t workId, uint32_t* roadP3,
                                   int roadP4, uint32_t completeRoadP5, uint8_t roadByte6,
                                   uint32_t completeTaskP7, int64_t consumeArg8, int loopCount9,
                                   float* pos10, int64_t* srcEntity11) {
    ResolveAll();

    // list base = (*(citizen)+0x278) + 0x48 ; cleanup (0x1400c0010..0x1400c0034)
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);  // RSI = aiComp+0x48
    if (s_wap_TaskCleanup) s_wap_TaskCleanup(list);

    uintptr_t ibpA = 0, ibpB = 0, ibpC = 0;

    // --- SendComponentMessage #1 → aiComp+0x58 (DeletePtr old) (0x1400c0039..0x1400c00c6) ---
    void* obj = WAP_Alloc(0x40, 8); // 0x1400c003e: ECX=0x38+8=0x40
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAP_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_wap_pDword_c70e8 ? *s_wap_pDword_c70e8 : 0; // 0x1401c70e8
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_wap_SendCompMsg) sm = s_wap_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0); // 0x1400cb1d0
    }
    WAP_IbpCtor(&ibpA, sm);
    // 0x1400c00b1: CMP [RSI+0x10] — RSI = aiComp+0x48 → aiComp+0x58.
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && s_wap_DeletePtr)
        s_wap_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA; // 0x1400c00c6: MOV [RSI+0x10],RAX

    // --- CompleteTask (workId, p5=completeTaskP7) (0x1400c00ca..0x1400c012a) ---
    obj = WAP_Alloc(0x38, 8); // 0x1400c00cf: ECX=0x30+8=0x38
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WAP_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // 0x1400c00f0: [R14]
        // 0x1400c00f7: ECX=[RBP+0x67] (param_7) → [RSP+0x20]=p5 ; EDX=R15D=workId
        if (s_wap_CompleteTask) ct = s_wap_CompleteTask(obj, workId, &entityInner, &innerIbp, completeTaskP7);
    }
    WAP_IbpCtor(&ibpB, ct);
    WAP_Append(list, &ibpB);

    // --- CompleteRoad (p2=param_5, p3=param_4(int), p4=param_6 byte) (0x1400c012f..0x1400c0172) ---
    obj = WAP_Alloc(0x30, 8); // 0x1400c0134: ECX=0x28+8=0x30
    void* cr = nullptr;
    if (obj) {
        // 0x1400c0146: R9D=byte[RBP+0x5f]=param_6 ; R8D=R12D=param_4 ; EDX=[RBP+0x57]=param_5
        if (s_wap_CompleteRoad) cr = s_wap_CompleteRoad(obj, static_cast<int>(completeRoadP5), roadP4, roadByte6); // 0x1400c59d0
    }
    WAP_IbpCtor(&ibpA, cr);
    WAP_Append(list, &ibpA);

    // --- ShowTool (~Hoe = -1) (0x1400c0177..0x1400c01be) ---
    obj = WAP_Alloc(0x30, 8); // 0x1400c017c: ECX=0x28+8=0x30
    void* st1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // 0x1400c018e: [R14]
        if (s_wap_ShowTool) st1 = s_wap_ShowTool(obj, &entityInner, ~0 /* 0x1400c0195: OR R8D,0xffffffff = -1 */);
    }
    WAP_IbpCtor(&ibpC, st1);
    WAP_Append(list, &ibpC);

    // --- optional ConsumeRawMaterial when param_8 != 0 (0x1400c01c3..0x1400c0226) ---
    if (consumeArg8 != 0) {
        obj = WAP_Alloc(0x40, 8); // 0x1400c01cf: MOV EDX,8; LEA ECX,[RDX+0x38] = 0x40
                                  // (was mis-transcribed 0x38 — same LEA-operand misread as
                                  // the Ctor7 0x40/0x48 bug. The ConsumeRawMaterial ctor
                                  // (0x1400c63b0) writes byte +0x38, so 0x38 overflowed by
                                  // 1 byte. Only stone roads construct this child (material
                                  // != null), hence dirt roads never crashed. Redzone
                                  // fingerprint: canary head 0x01, reqSize 0x38, alloc site
                                  // bffd0+0x3ff, 2026-07-26.)
        void* crm = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10); // 0x1400c01ed
            // 0x1400c01f8: byte[RSP+0x20]=0 (p4) ; R8=[RBP+0x6f]=param_8 (p3) ; RDX=&entityInner
            if (s_wap_ConsumeRaw) crm = s_wap_ConsumeRaw(obj, &entityInner, consumeArg8, 0); // 0x1400c63b0
        }
        WAP_IbpCtor(&ibpA, crm);
        WAP_Append(list, &ibpA);
    }

    // --- PerformWork_Ctor7: heading(XMM2)=-1000(DAT_1722a8), actionType(R9)=0xA,
    //     time=Time(4.0=DAT_172194), p6=1, p7=-1 (0x1400c022b..0x1400c02a6) ---
    obj = WAP_Alloc(0x48, 8); // 0x1400c0230: MOV EDX,8; LEA ECX,[RDX+0x40] = 0x48
                              // (was mis-transcribed as 0x40; Ctor7 writes byte +0x40 and
                              // int +0x44, so 0x40 overflowed the block by 8 bytes — the
                              // road-build heap corruption, caught by redzone PAD+SCAN)
    void* pw = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // 0x1400c0245: [R14]
        float timeF = s_wap_pf_172194 ? *s_wap_pf_172194 : 0.0f; // 0x1400c024c: XMM1=DAT_140172194=4.0f
        uint64_t timeVal = 0;
        if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeF); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
        float headingC = s_wap_pf_1722a8 ? *s_wap_pf_1722a8 : 0.0f; // 0x1400c0279: XMM2=DAT_1401722a8=-1000.0f
        // 0x1400c0273: R9D=0xa ; [RSP+0x30]=0xffffffff (p7=-1) ; byte[RSP+0x28]=1 (p6) ; [RSP+0x20]=time
        if (s_wap_PerformWork7) pw = s_wap_PerformWork7(obj, &entityInner, headingC, 0xA, static_cast<int64_t>(timeVal), 1, -1);
    }
    WAP_IbpCtor(&ibpA, pw);
    WAP_Append(list, &ibpA);

    float speed = s_wap_pf_172280 ? *s_wap_pf_172280 : 0.0f; // 0x1400c02c2: XMM8=DAT_140172280=-1.0f

    // --- MoveTo #1 (Fast=2, dest=param_3) (0x1400c02ab..0x1400c0342) ---
    obj = WAP_Alloc(0x480, 8); // 0x1400c02b0: ECX=0x480
    void* mv1 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_wap_FindFamily) familyIBP = s_wap_FindFamily(entityInner, &famRet);
        // 0x1400c031d: LEA R8D,[R9+2] (R9=0) → pathType=2 (Fast); dest=param_3 (R13 at entry); XMM8=speed
        if (s_wap_MoveTo) mv1 = s_wap_MoveTo(obj, familyIBP, 2, 0, reinterpret_cast<float*>(roadP3), 0xe1f, 0, speed);
    }
    WAP_IbpCtor(&ibpA, mv1);
    WAP_Append(list, &ibpA);

    // --- ShowTool (Axe = 1) (0x1400c0347..0x1400c0393) ---
    obj = WAP_Alloc(0x30, 8); // 0x1400c034c: ECX=0x28+8=0x30
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        // 0x1400c0359: R13D=1 ; 0x1400c036b: R8D=R13D=1 (Axe)
        if (s_wap_ShowTool) st2 = s_wap_ShowTool(obj, &entityInner, 1 /* Axe */);
    }
    WAP_IbpCtor(&ibpA, st2);
    WAP_Append(list, &ibpA);

    // --- optional pickup chain when param_9 > 0 (0x1400c0398..0x1400c0653) ---
    if (loopCount9 > 0) {
        // heading via Entity_impl_29a0(param_10, &{*param_11}) (0x1400c03a2..0x1400c03be; XMM6)
        int64_t srcInner11 = srcEntity11 ? *srcEntity11 : 0; // 0x1400c03a9: MOV RAX,[R12]
        float headingP = 0.0f;
        if (s_wap_Entity29a0) headingP = s_wap_Entity29a0(pos10, &srcInner11);

        float timeC = s_wap_pf_172124 ? *s_wap_pf_172124 : 0.0f; // 0x1400c03e2: XMM1=DAT_140172124=0.75f

        // inline PerformWork A: heading=headingP(XMM6), actionType=7, time=Time(0.75) (0x1400c03c1..)
        obj = WAP_Alloc(0x48, 8); // 0x1400c03c6: ECX=0x40+8=0x48
        void* pwA = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
            uint64_t timeVal = 0;
            if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeC); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
            WAP_BuildInlinePerformWork(obj, entityInner, headingP, 0x7, static_cast<int64_t>(timeVal)); // 0x1400c0430: 0x2c=7
            pwA = obj;
        }
        WAP_IbpCtor(&ibpA, pwA);
        WAP_Append(list, &ibpA);

        // PickupStoredRawMaterial (entity, *param_11, param_8, -1, 1) (0x1400c046b..0x1400c04cb)
        obj = WAP_Alloc(0x48, 8); // 0x1400c0470: ECX=0x40+8=0x48
        void* pickup = nullptr;
        if (obj) {
            uintptr_t srcInner = srcEntity11 ? static_cast<uintptr_t>(*srcEntity11) : 0; // 0x1400c0482: MOV RCX,[R12]
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP); // 0x1400c048a: [R14]
            // 0x1400c0491: byte[RSP+0x28]=1 (p6) ; [RSP+0x20]=0xffffffff (p5=-1) ; R9=[RBP+0x6f]=param_8 (p4) ;
            //   R8=&srcInner ; RDX=&entityInner.
            if (s_wap_PickupStored) pickup = s_wap_PickupStored(obj, &entityInner, &srcInner, consumeArg8, -1, 1); // 0x1400ca520
        }
        WAP_IbpCtor(&ibpB, pickup);
        WAP_Append(list, &ibpB);

        // inline PerformWork B: heading=headingP(XMM6), actionType=6, time=Time(0.75) (0x1400c04d0..)
        obj = WAP_Alloc(0x48, 8); // 0x1400c04d5: ECX=0x40+8=0x48
        void* pwB = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
            uint64_t timeVal = 0;
            if (s_wap_TimeCtor) { void* t = s_wap_TimeCtor(&timeVal, timeC); if (t) timeVal = *reinterpret_cast<uint64_t*>(t); }
            WAP_BuildInlinePerformWork(obj, entityInner, headingP, 0x6, static_cast<int64_t>(timeVal)); // 0x1400c053f: 0x2c=6
            pwB = obj;
        }
        WAP_IbpCtor(&ibpA, pwB);
        WAP_Append(list, &ibpA);

        // MoveTo #2: pathType = (StorageComponent::QueryCount > 0) ? Faster(4) : Slow(1); dest=param_10 (0x1400c057a..0x1400c0653)
        obj = WAP_Alloc(0x480, 8); // 0x1400c057f: ECX=0x480
        void* mv2 = nullptr;
        if (obj) {
            // QueryCount(entity storage) → BL=(count>0) (0x1400c059a..0x1400c05d2)
            uintptr_t entObj = *reinterpret_cast<uintptr_t*>(citizenIBP);    // 0x1400c05a3: [R14]
            uintptr_t storRet = 0;
            void* storIbp = reinterpret_cast<void*>(&storRet);
            if (s_wap_FindStorage) s_wap_FindStorage(*reinterpret_cast<uintptr_t*>(entObj + 0x10), &storRet); // 0x140006270 (local_78)
            int count = 0;
            if (s_wap_QueryCount) count = s_wap_QueryCount(reinterpret_cast<void*>(storRet)); // 0x14012cf10
            bool countPositive = (count > 0); // 0x1400c05d0: TEST EAX,EAX; SETG BL

            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
            int pathType = 1; // R13D = Slow (default); 0x1400c05fc: CMOVNZ R13D, 4 if BL
            if (countPositive) pathType = 4; // Faster
            void* familyIBP = nullptr;
            uintptr_t famRet = 0;
            if (s_wap_FindFamily) familyIBP = s_wap_FindFamily(entityInner, &famRet);
            // 0x1400c0631: R8D=R13D=pathType ; dest=[RBP+0x7f]=param_10 ; XMM8=speed ; [RSP+0x28]=0xe1f
            if (s_wap_MoveTo) mv2 = s_wap_MoveTo(obj, familyIBP, pathType, 0, pos10, 0xe1f, 0, speed);
            (void)storIbp;
        }
        WAP_IbpCtor(&ibpA, mv2);
        WAP_Append(list, &ibpA);
    }

    (void)ibpB; (void)ibpC; // appended above
}
