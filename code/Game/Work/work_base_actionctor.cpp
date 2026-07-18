// code/Game/Work/work_base_actionctor.cpp
// Work action/animation constructors (Wave 2 — Work action-ctor sub-batch).
//
// These 4 exe helpers build the action sequence for a citizen work task: each
// allocates a chain of Action objects (SendComponentMessage / CompleteTask /
// ShowTool / PerformWork / MoveTo / *NaturalResource / inline PerformWork or
// PlayAnimation) and appends them to the CitizenAIComponent's task action list
// (entity+0x278, list base +0x48). They are resolved via ResolveAddr(Module::Exe,
// 0xADDR) from the Work .cpp call sites (cleararea / foresterwork / herbalistwork /
// pasturework / tendarea / plantorchard / fishingwork). They resolve to 0
// (silently skipped) unless registered. This file provides faithful 1:1 ports + a single registration
// point (RegisterWorkActionCtorBatch in standalone/reg_work_actionctor.cpp).
//
// Each function is an instruction-faithful port of the decompiled exe helper; do
// NOT "improve" behavior. Every exe sub-callee these ctors invoke is itself mirrored
// via a guarded ResolveAddr typedef'd call — if a sub-callee is unregistered the call
// is null-guarded and skipped, matching project semantics. Sub-callees are NOT
// registered here (they are separate addresses, owned elsewhere).
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Shared sub-callee / dispatch-slot resolution
// ============================================================
// Engine base helpers (InternalBasePtr ctor/checkptr/deleteptr, IsDerivedFrom) come
// from engine_funcs.h (fn_InternalBasePtr_Ctor / fn_InternalBasePtr_CheckPtr /
// fn_InternalBasePtr_DeletePtr / fn_IsDerivedFrom). Everything else is resolved here.

// --- exe code sub-callees (0x1400xxxxx → ResolveAddr returns fn ptr directly) ---

// Entity_impl_29a0 (0x1400c29a0): compute heading float toward citizen path target.
// Returns a float in XMM0 (the heading); typed accordingly.
typedef float (*WA_fn_Entity_impl_29a0_t)(float* worldPos, void* citizenIBP);

// FUN_1400c5400 (TaskListCleanup) — cancel pending work actions on the list.
typedef void (*WA_fn_TaskListCleanup_t)(void* list);
// FUN_1400c54e0 (TaskListAppend) — append an action IBP to the task list.
typedef void (*WA_fn_TaskListAppend_t)(void* list, void* actionIBP);
// FUN_1400c3470 — conditional Work-target detach helper (c1eb0 tail).
typedef void (*WA_fn_WorkDetach_3470_t)(void* target);

// Entity_FindComponent_Family (0x1400012d0) — member-style, RCX=entity, RDX=retIBP,
// returns retIBP (RAX). Used to wrap the family/move component for MoveTo.
typedef void* (*WA_fn_FindComponentFamily_t)(uintptr_t entity, void* retIBP);

// Action constructor / copy-constructor sub-callees. All return the constructed
// object pointer in RAX (or null on alloc failure), passed straight to InternalBasePtr_Ctor.
typedef void* (*WA_fn_SendComponentMessage_CopyCtor_t)(void* self, void* p2, void* p3, uint32_t p4, void* p5, uint32_t p6);
typedef void* (*WA_fn_CompleteTask_CopyCtor_t)(void* self, uint32_t workId, void* p3, void* p4, uint32_t p5);
typedef void* (*WA_fn_ShowTool_Ctor_t)(void* self, void* p2, int toolType);
typedef void* (*WA_fn_RemoveNaturalResource_CopyCtor_t)(void* self, void* p2, void* p3, uint8_t p4);
typedef void* (*WA_fn_AnimateNaturalResource_CopyCtor_t)(void* self, void* p2, uint32_t p3, uint8_t p4);
typedef void* (*WA_fn_CreateNaturalResource_CopyCtor_t)(void* self, void* p2, void* p3, uint32_t p4);
// PerformWork_Action_Ctor7 (0x1400c9720). Win64 ABI (verified via disassembly):
//   RCX=self, RDX=entity, XMM2=heading (3rd slot, float → self+0x28),
//   R9D=actionType (4th slot, int → self+0x2c), [stack]=time(self+0x30),
//   byte p6(self+0x40), int p7(self+0x44). The float MUST be the 3rd positional
//   parameter so it lands in XMM2; actionType MUST be the 4th so it lands in R9.
typedef void* (*WA_fn_PerformWork_Ctor7_t)(void* self, void* entity, float heading, int actionType, int64_t time, uint8_t p6, int p7);
typedef void* (*WA_fn_MoveTo_Ctor_t)(void* self, void* familyIBP, int pathType, int p4, float* destination, int p6, int p7, float speed);
// PickupRawMaterial_Action_CopyCtor (0x1400c9b70). Verified from disassembly:
//   RCX=self, RDX=p2, R8=p3 → self+0x20 = *p2 (citizen inner), self+0x28 = *p3 (target inner).
typedef void* (*WA_fn_PickupRawMaterial_CopyCtor_t)(void* self, void* p2, void* p3);
// Entity_FindComponent_Storage (0x140006270) — member-style, RCX=entity inner, RDX=retIBP.
typedef void* (*WA_fn_FindStorage_t)(int64_t entityInner, void* retIBP);
// StorageComponent::QueryCount (0x14012cf10) — current carried total.
typedef int   (*WA_fn_StorageQueryCount_t)(void* storageComp);

// --- dispatch slots (0x140152xxx / 0x14015xxxx → ResolveAddr returns slot storage,
//     must be dereferenced once to get the runtime fn ptr) ---

// 0x140152b60 — System::MemoryAllocator::MemAlloc(size, align) static wrapper.
typedef void* (*WA_fn_MemAllocStatic2_t)(uint64_t size, uint32_t align);
// 0x140152768 — Object::Internal::Internal(self) base ctor.
typedef void* (*WA_fn_ObjectInternal_Ctor_t)(void* self);
// 0x140152b00 — System::Time::Time(retval, float seconds) (used in PlayAnimation/grazing/plant ctors).
typedef void* (*WA_fn_Time_Ctor_t)(void* retval, float seconds);
// 0x140152a30 — Math::Vector3::Vector3(self, x, y, z).
typedef void* (*WA_fn_Vector3_Ctor_t)(void* self, float x, float y, float z);
// 0x1401512c0 — Math::Vector3::Normalize(self) -> Vector3*.
typedef void* (*WA_fn_Vector3_Normalize_t)(void* self);
// 0x1401512d8 — Math::Vector3::Heading(self) -> float (XMM0).
typedef float (*WA_fn_Vector3_Heading_t)(void* self);

static WA_fn_Entity_impl_29a0_t              s_fn_Entity_29a0          = nullptr;
static WA_fn_TaskListCleanup_t               s_fn_TaskListCleanup      = nullptr;
static WA_fn_TaskListAppend_t                s_fn_TaskListAppend       = nullptr;
static WA_fn_WorkDetach_3470_t               s_fn_WorkDetach_3470      = nullptr;
static WA_fn_FindComponentFamily_t           s_fn_FindComponentFamily  = nullptr;
static WA_fn_SendComponentMessage_CopyCtor_t s_fn_SendCompMsg          = nullptr;
static WA_fn_CompleteTask_CopyCtor_t         s_fn_CompleteTask         = nullptr;
static WA_fn_ShowTool_Ctor_t                 s_fn_ShowTool             = nullptr;
static WA_fn_RemoveNaturalResource_CopyCtor_t s_fn_RemoveNR            = nullptr;
static WA_fn_AnimateNaturalResource_CopyCtor_t s_fn_AnimateNR          = nullptr;
static WA_fn_CreateNaturalResource_CopyCtor_t s_fn_CreateNR            = nullptr;
static WA_fn_PerformWork_Ctor7_t             s_fn_PerformWork7         = nullptr;
static WA_fn_MoveTo_Ctor_t                   s_fn_MoveTo               = nullptr;
static WA_fn_PickupRawMaterial_CopyCtor_t    s_fn_PickupRawMaterial    = nullptr;
static WA_fn_FindStorage_t                   s_fn_FindStorage          = nullptr;
static WA_fn_StorageQueryCount_t             s_fn_StorageQueryCount    = nullptr;

static WA_fn_MemAllocStatic2_t      s_fn_MemAllocStatic = nullptr;
static WA_fn_ObjectInternal_Ctor_t  s_fn_ObjInternalCtor = nullptr;
static WA_fn_Time_Ctor_t            s_fn_TimeCtor        = nullptr;
static WA_fn_Vector3_Ctor_t         s_fn_Vec3Ctor        = nullptr;
static WA_fn_Vector3_Normalize_t    s_fn_Vec3Normalize   = nullptr;
static WA_fn_Vector3_Heading_t      s_fn_Vec3Heading     = nullptr;

// Inline-object vtables + globals (exe .rdata).
static void* s_vtable_Action        = nullptr;  // 0x14015faa0
static void* s_vtable_PerformWork   = nullptr;  // 0x14015fea0
static void* s_vtable_PlayAnimation = nullptr;  // 0x140158950
static void* s_vtable_PickupRawMaterial = nullptr; // 0x14015ff10 (LEA @0x1400c9bb0)
static const float* s_pf_172124 = nullptr;      // 0x140172124 (c0ad0 PerformWork time)
static const uint32_t* s_pTid_c7b68 = nullptr;  // 0x1401c7b68 (PerformWork target derive check)
static const uint32_t* s_pDword_c70e8 = nullptr;// 0x1401c70e8 (SendComponentMessage msg id)
static const float* s_pf_172280 = nullptr;      // 0x140172280 (MoveTo speed)
static const float* s_pf_172100 = nullptr;      // 0x140172100 (PlayAnimation time)
static const float* s_pf_1721b0 = nullptr;      // 0x1401721b0 (grazing PerformWork time)
static const float* s_pf_172178 = nullptr;      // 0x140172178 (plant PerformWork time)
static const float* s_pf_1722a8 = nullptr;      // 0x1401722a8 (plant PerformWork heading const)

static bool s_resolved = false;

static void ResolveAll() {
    if (s_resolved) return;
    s_resolved = true;

    s_fn_Entity_29a0         = reinterpret_cast<WA_fn_Entity_impl_29a0_t>(ResolveAddr(Module::Exe, 0x1400c29a0));
    s_fn_TaskListCleanup     = reinterpret_cast<WA_fn_TaskListCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_fn_TaskListAppend      = reinterpret_cast<WA_fn_TaskListAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_fn_WorkDetach_3470     = reinterpret_cast<WA_fn_WorkDetach_3470_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_fn_FindComponentFamily = reinterpret_cast<WA_fn_FindComponentFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_fn_SendCompMsg         = reinterpret_cast<WA_fn_SendComponentMessage_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_fn_CompleteTask        = reinterpret_cast<WA_fn_CompleteTask_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_fn_ShowTool            = reinterpret_cast<WA_fn_ShowTool_Ctor_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_fn_RemoveNR            = reinterpret_cast<WA_fn_RemoveNaturalResource_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400cadf0));
    s_fn_AnimateNR           = reinterpret_cast<WA_fn_AnimateNaturalResource_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c57d0));
    s_fn_CreateNR            = reinterpret_cast<WA_fn_CreateNaturalResource_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c6a20));
    s_fn_PerformWork7        = reinterpret_cast<WA_fn_PerformWork_Ctor7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_fn_MoveTo              = reinterpret_cast<WA_fn_MoveTo_Ctor_t>(ResolveAddr(Module::Exe, 0x1400c9170));
    s_fn_PickupRawMaterial   = reinterpret_cast<WA_fn_PickupRawMaterial_CopyCtor_t>(ResolveAddr(Module::Exe, 0x1400c9b70));
    s_fn_FindStorage         = reinterpret_cast<WA_fn_FindStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_fn_StorageQueryCount   = reinterpret_cast<WA_fn_StorageQueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));

    // Dispatch slots — deref once to get the runtime fn ptr.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_fn_MemAllocStatic  = *reinterpret_cast<WA_fn_MemAllocStatic2_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152768)) s_fn_ObjInternalCtor = *reinterpret_cast<WA_fn_ObjectInternal_Ctor_t*>(s);
    // Slot 0x140152768 isn't registered as a dispatch slot in standalone; fall back to
    // the engine_funcs Object::Internal::Internal helper (same IAT target).
    if (!s_fn_ObjInternalCtor) s_fn_ObjInternalCtor = reinterpret_cast<WA_fn_ObjectInternal_Ctor_t>(fn_ObjectInternal_Ctor);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b00)) s_fn_TimeCtor        = *reinterpret_cast<WA_fn_Time_Ctor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a30)) s_fn_Vec3Ctor        = *reinterpret_cast<WA_fn_Vector3_Ctor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x1401512c0)) s_fn_Vec3Normalize   = *reinterpret_cast<WA_fn_Vector3_Normalize_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x1401512d8)) s_fn_Vec3Heading     = *reinterpret_cast<WA_fn_Vector3_Heading_t*>(s);

    s_vtable_Action        = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015faa0));
    s_vtable_PerformWork   = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015fea0));
    s_vtable_PlayAnimation = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x140158950));
    s_vtable_PickupRawMaterial = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015ff10));
    s_pf_172124    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172124));
    s_pTid_c7b68   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b68));
    s_pDword_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_pf_172280    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    s_pf_172100    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172100));
    s_pf_1721b0    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401721b0));
    s_pf_172178    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172178));
    s_pf_1722a8    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401722a8));
}

// Small wrappers matching the original's MemAlloc(size,align) static call shape.
static inline void* WA_Alloc(uint64_t size, uint32_t align) {
    if (s_fn_MemAllocStatic) return s_fn_MemAllocStatic(size, align);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, align);
    return nullptr;
}
static inline void WA_IbpCtor(void* ibp, void* obj) { if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(ibp, obj); }
static inline void WA_CheckPtr(void* p)             { if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p); }
static inline void WA_Append(void* list, void* ibp) { if (s_fn_TaskListAppend) s_fn_TaskListAppend(list, ibp); }

// Real-value .rdata fallbacks (raw .rdata resolves NULL in standalone — see
// reference_standalone_rdata_null_fallback). Bit patterns read from Ghidra:
//   DAT_140172124 = 0x3f400000 =  0.75f  (PerformWork action duration in c0ad0)
//   DAT_140172280 = 0xbf800000 = -1.0f   (MoveTo speed sentinel)
static inline float WA_C_172124() { return s_pf_172124 ? *s_pf_172124 : 0.75f; }
// ⚠️ The three pre-existing ctors above fall back to 0.0f for DAT_140172280, which is
// WRONG (real value is -1.0f) but only bites in standalone. Deliberately NOT changed
// here — it would alter the behaviour of three already-shipping functions, which is out
// of scope for this port. c0ad0 below uses the correct value via WA_MoveSpeed().
static inline float WA_MoveSpeed() { return s_pf_172280 ? *s_pf_172280 : -1.0f; }

// c0ad0 repeats this exact idiom three times (@0x1400c0d06, @0x1400c0f04, @0x1400c0fdc):
//   CheckPtr(citizenIBP); e = *citizenIBP; CheckPtr(e+0x10);
//   Entity_FindComponent_Storage(*(e+0x10), &ibp); CheckPtr(&ibp);
//   StorageComponent_QueryCount(ibp)
// i.e. "how much is the citizen carrying right now". Re-evaluated each time in the
// original (not cached), so this mirrors that.
static int WA_CitizenCarryCount(void* citizenIBP) {
    if (!s_fn_FindStorage || !s_fn_StorageQueryCount) return 0;
    WA_CheckPtr(citizenIBP);
    uintptr_t e = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WA_CheckPtr(reinterpret_cast<void*>(e + 0x10));
    void* storageIBP = nullptr;
    s_fn_FindStorage(*reinterpret_cast<int64_t*>(e + 0x10), &storageIBP);
    WA_CheckPtr(&storageIBP);
    return s_fn_StorageQueryCount(storageIBP);
}

// ============================================================
// PerformWork_Action_Ctor3 (0x1400c1eb0)
// ============================================================
// Build the work-action chain for a resource-clearing / performwork task and store it
// on the citizen's CitizenAIComponent task list (citizen+0x278, list base +0x48).
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId (u32)
//   param_3 = target InternalBasePtr* (NaturalResource/work entity)
//   param_4 = float* worldPos
//   param_5 = int (consumed/recomputed internally)
//   param_6 = byte (RemoveNaturalResource flag)
void WorkAct_PerformWorkCtor_c1eb0(void* citizenIBP, uint32_t workId, void* targetIBP,
                                   float* worldPos, int /*zero*/, uint8_t zeroByte) {
    ResolveAll();

    // --- derive the NaturalResource component off the target (param_3) ---
    WA_CheckPtr(targetIBP);
    void* targetComp = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x18);
    void* derivedTarget = nullptr;
    if (s_pTid_c7b68 && fn_IsDerivedFrom && fn_IsDerivedFrom(targetComp, *s_pTid_c7b68))
        derivedTarget = targetComp;

    WA_CheckPtr(targetIBP);
    int64_t targetInner = *reinterpret_cast<int64_t*>(*reinterpret_cast<uintptr_t*>(targetIBP) + 0x10);

    // heading = Entity_impl_29a0(worldPos, &{target inner}) (XMM0 float)
    int64_t headingArg = targetInner;
    float heading = 0.0f;
    if (s_fn_Entity_29a0) heading = s_fn_Entity_29a0(worldPos, &headingArg);

    // list base = (*(citizen)+0x278) + 0x48
    WA_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WA_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_fn_TaskListCleanup) s_fn_TaskListCleanup(list);

    void* obj;
    uintptr_t ibpA = 0, ibpB = 0;

    // --- SendComponentMessage action → stored at aiComp+0x58 (replaces prior, DeletePtr) ---
    obj = WA_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_pDword_c70e8 ? *s_pDword_c70e8 : 0;
        WA_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_fn_SendCompMsg) sm = s_fn_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibpA, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA;

    // --- CompleteTask action ---
    obj = WA_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
        if (s_fn_CompleteTask) ct = s_fn_CompleteTask(obj, workId, &entityInner, &targetInnerSlot, 0);
    }
    WA_IbpCtor(&ibpB, ct);
    WA_Append(list, &ibpB);

    // --- ShowTool (~Hoe = -2) action ---
    obj = WA_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_ShowTool) st = s_fn_ShowTool(obj, &entityInner, ~0 /* ~Hoe = -1 (Hoe==0); disasm OR R8D,0xFFFFFFFF */);
    }
    WA_IbpCtor(&ibpA, st);
    WA_Append(list, &ibpA);

    // --- RemoveNaturalResource action ---
    // ABI (Ghidra-verified vs RemoveNaturalResource_Action_CopyCtor @0x1400cadf0 and its
    // caller c1eb0): the ctor stores *param_2 → self+0x28 (resourceEntity/TREE) and
    // *param_3 → self+0x20 (citizenEntity). c1eb0 calls it as
    //   RemoveNaturalResource_Action_CopyCtor(self, &tree(lVar2), &citizen(*param_1), flag)
    // i.e. param_2 = &TREE, param_3 = &CITIZEN. A prior port swapped these (passed citizen
    // as param_2 → +0x28), which made RemoveNaturalResource::Main FindNRComp(citizen) return
    // null (citizen has no NR component) and the organic produce got skipped. Pass tree first.
    obj = WA_Alloc(0x38, 8);
    void* rnr = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
        if (s_fn_RemoveNR) rnr = s_fn_RemoveNR(obj, &targetInnerSlot, &entityInner, zeroByte);
    }
    WA_IbpCtor(&ibpB, rnr);
    WA_Append(list, &ibpB);

    // --- compute performWork param_5 from derivedTarget+0x80 ---
    int pwParam = 8;
    if (derivedTarget && *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80) < 0xb)
        pwParam = static_cast<int>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80)) + 9;

    // --- if derivedTarget+0x64 (byte) set: inline PerformWork + two AnimateNaturalResource ---
    if (derivedTarget && *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x64) != 0) {
        void* inlineObj = WA_Alloc(0x48, 8);
        void* pw = nullptr;
        if (inlineObj) {
            uint32_t resKind = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80);
            uint64_t res70   = *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x70);
            if (s_fn_ObjInternalCtor) s_fn_ObjInternalCtor(inlineObj);
            uintptr_t o = reinterpret_cast<uintptr_t>(inlineObj);
            *reinterpret_cast<void**>(o)          = s_vtable_Action;
            *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
            *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
            *reinterpret_cast<void**>(o)          = s_vtable_PerformWork;
            *reinterpret_cast<uint64_t*>(o + 0x20) = 0;
            *reinterpret_cast<uintptr_t*>(o + 0x20) = *reinterpret_cast<uintptr_t*>(citizenIBP);  // entity inner
            *reinterpret_cast<float*>(o + 0x28)    = heading;
            *reinterpret_cast<int*>(o + 0x2c)      = pwParam;
            *reinterpret_cast<uint64_t*>(o + 0x30) = res70;
            *reinterpret_cast<uint64_t*>(o + 0x38) = 0;
            *reinterpret_cast<uint8_t*>(o + 0x40)  = 0;
            *reinterpret_cast<uint32_t*>(o + 0x44) = resKind;
            pw = inlineObj;
        }
        WA_IbpCtor(&ibpA, pw);
        WA_Append(list, &ibpA);

        // AnimateNaturalResource(targetInner, 1, 0)
        obj = WA_Alloc(0x30, 8);
        void* anr1 = nullptr;
        if (obj) {
            uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
            if (s_fn_AnimateNR) anr1 = s_fn_AnimateNR(obj, &targetInnerSlot, 1, 0);
        }
        WA_IbpCtor(&ibpA, anr1);
        WA_Append(list, &ibpA);

        // AnimateNaturalResource(targetInner, 0, 1)
        obj = WA_Alloc(0x30, 8);
        void* anr2 = nullptr;
        if (obj) {
            uintptr_t targetInnerSlot = static_cast<uintptr_t>(targetInner);
            if (s_fn_AnimateNR) anr2 = s_fn_AnimateNR(obj, &targetInnerSlot, 0, 1);
        }
        WA_IbpCtor(&ibpA, anr2);
        WA_Append(list, &ibpA);
    }

    // --- PerformWork_Ctor7: heading(XMM2)=heading, actionType(R9)=pwParam,
    //     time=derivedTarget+0x68, p6=1, p7=derivedTarget+0x80 (resKind) ---
    obj = WA_Alloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj && derivedTarget) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        int resKind  = static_cast<int>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80));
        int64_t time = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x68);
        if (s_fn_PerformWork7) pw7 = s_fn_PerformWork7(obj, &entityInner, heading, pwParam, time, 1, resKind);
    }
    WA_IbpCtor(&ibpA, pw7);
    WA_Append(list, &ibpA);

    // --- MoveTo (path type from derivedTarget+0x80 switch) ---
    obj = WA_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj && derivedTarget) {
        int pathType = 1; // Slow default (disasm: EBX=1 at 0x1400c2123)
        switch (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80)) {
            case 0: case 1: case 4: case 5: case 6: case 7: pathType = 2; break; // Fast
            case 2: case 3: pathType = 3; break;                                  // Fastest
            case 0xffffffff: pathType = 1; break;                                 // Slow
            default: pwParam = 1; break;                                          // resKind>=8: pathType stays 1
        }
        WA_CheckPtr(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        WA_CheckPtr(&entityInner);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_fn_FindComponentFamily) familyIBP = s_fn_FindComponentFamily(entityInner, &famRet);
        float speed = s_pf_172280 ? *s_pf_172280 : 0.0f;
        if (s_fn_MoveTo) mv = s_fn_MoveTo(obj, familyIBP, pathType, 0, worldPos, 0xe1f, 0, speed);
    }
    WA_IbpCtor(&ibpA, mv);
    WA_Append(list, &ibpA);

    // --- ShowTool (derivedTarget+0x80 tool type) action ---
    obj = WA_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj && derivedTarget) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        int toolType = static_cast<int>(*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(derivedTarget) + 0x80));
        if (s_fn_ShowTool) st2 = s_fn_ShowTool(obj, &entityInner, toolType);
    }
    WA_IbpCtor(&ibpA, st2);
    WA_Append(list, &ibpA);

    // --- tail: set bit 0x14 (0x100000) on target's +0x20 flags; on newly-set bit
    //     (low 16 changed) detach via FUN_1400c3470 if target+0x10 non-null ---
    WA_CheckPtr(targetIBP);
    uintptr_t targetObj = *reinterpret_cast<uintptr_t*>(targetIBP);
    uint32_t oldFlags = *reinterpret_cast<uint32_t*>(targetObj + 0x20);
    uint32_t newFlags = oldFlags | 0x100000u;
    *reinterpret_cast<uint32_t*>(targetObj + 0x20) = newFlags;
    if (static_cast<uint16_t>(oldFlags ^ newFlags) != 0 &&
        *reinterpret_cast<uintptr_t*>(targetObj + 0x10) != 0) {
        WA_CheckPtr(reinterpret_cast<void*>(targetObj + 0x10));
        void* detach = *reinterpret_cast<void**>(targetObj + 0x10);
        if (s_fn_WorkDetach_3470) s_fn_WorkDetach_3470(detach);
    }
}

// ============================================================
// PickupRawMaterial_Action_CopyCtor (0x1400c9b70)
// ============================================================
// Trivial field-copy ctor for the PickupRawMaterial action.
// Disassembly-verified: RCX=self, RDX=p2, R8=p3.
//   Object::Internal::Internal(self)
//   self+0x00 = vtable_Action (0x14015faa0)   [immediately overwritten]
//   self+0x10 = 0 ; self+0x18 = 0
//   self+0x00 = vtable_PickupRawMaterial (0x14015ff10)
//   self+0x20 = 0 then *p2   (citizen inner)
//   self+0x28 = 0 then *p3   (target/pile inner)
// Returns self (RAX).
void* WorkAct_PickupRawMaterialCopyCtor_c9b70(void* self, void* p2, void* p3) {
    ResolveAll();
    if (!self) return nullptr;

    if (s_fn_ObjInternalCtor) s_fn_ObjInternalCtor(self);

    uintptr_t o = reinterpret_cast<uintptr_t>(self);
    *reinterpret_cast<void**>(o)           = s_vtable_Action;
    *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
    *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
    *reinterpret_cast<void**>(o)           = s_vtable_PickupRawMaterial;
    *reinterpret_cast<uint64_t*>(o + 0x20) = 0;
    *reinterpret_cast<uintptr_t*>(o + 0x20) = p2 ? *reinterpret_cast<uintptr_t*>(p2) : 0;
    *reinterpret_cast<uint64_t*>(o + 0x28) = 0;
    *reinterpret_cast<uintptr_t*>(o + 0x28) = p3 ? *reinterpret_cast<uintptr_t*>(p3) : 0;
    return self;
}

// ============================================================
// PerformWork_Action_Ctor9 (0x1400c0ad0) — THE pile-haul action builder
// ============================================================
// Called from Pickup::Execute (0x1400f2f00) once per accepted candidate. Builds the
// citizen's LIFO action stack, so the pushed order below EXECUTES IN REVERSE:
//   pushed:   ShowTool(~Hoe) → [SendComponentMessage → aiComp+0x58, not pushed]
//             → PerformWork(kind 7) → [ShowTool(~Axe) if carrying nothing]
//             → PickupRawMaterial → CompleteTask → PerformWork(kind 6) → MoveTo
//             → [ShowTool(~Axe) if carrying something]
//   executes: [ShowTool] → MoveTo(walk to pile) → PerformWork(6) → CompleteTask
//             → PickupRawMaterial(pick it up) → [ShowTool] → PerformWork(7) → ShowTool(~Hoe)
//
//   param_1 = citizen InternalBasePtr*
//   param_2 = workId / resource type id (u32) → CompleteTask
//   param_3 = target (pile) InternalBasePtr*
//   param_4 = float* worldPos (MoveTo destination + heading source)
//   param_5 = int cell index (pile cell; indexes the map grid for the path-mask byte)
//   param_6 = byte — when non-zero, cancel any pending actions first (first candidate only)
//
// Ghidra-verified constants: ~Hoe = -1 (OR R8D,0xffffffff @0x1400c0b68),
// ~Axe = -2 (MOV R8D,0xfffffffe @0x1400c0d5b/@0x1400c1024),
// pathType = QueryCount>0 ? 4 : 1 (MOV EAX,4 / MOV EBX,1 / CMOVNZ @0x1400c0f61).
void WorkAct_PerformWorkCtor9_c0ad0(void* citizenIBP, uint32_t workId, void* targetIBP,
                                    float* worldPos, int cellIdx, uint8_t cancelFirst) {
    ResolveAll();

    // list base = (*(citizen)+0x278) + 0x48
    WA_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WA_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (cancelFirst != 0 && s_fn_TaskListCleanup) s_fn_TaskListCleanup(list);

    // heading = Entity_impl_29a0(worldPos, &target inner) — returned in XMM0.
    uintptr_t targetInner = targetIBP ? *reinterpret_cast<uintptr_t*>(targetIBP) : 0;
    float heading = 0.0f;
    {
        uintptr_t headingArg = targetInner;
        if (s_fn_Entity_29a0) heading = s_fn_Entity_29a0(worldPos, &headingArg);
    }

    void* obj;
    uintptr_t ibpA = 0, ibpB = 0;

    // --- ShowTool (~Hoe = -1) ---
    obj = WA_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_ShowTool) st = s_fn_ShowTool(obj, &entityInner, -1 /* ~Hoe */);
    }
    WA_IbpCtor(&ibpA, st);
    WA_Append(list, &ibpA);

    // --- SendComponentMessage → aiComp+0x58 (replaces prior, DeletePtr; NOT pushed) ---
    obj = WA_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_pDword_c70e8 ? *s_pDword_c70e8 : 0;
        WA_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_fn_SendCompMsg) sm = s_fn_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    uintptr_t ibpSm = 0;
    WA_IbpCtor(&ibpSm, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpSm;

    // --- path-mask byte from the pile's map cell: cells(+0x80) + cellIdx*0x30, byte +0x29 ---
    uint8_t cellFlag = 0;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton) {
        uintptr_t gw   = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
        uintptr_t grid = *reinterpret_cast<uintptr_t*>(gw + 0xf8);
        uintptr_t cells = *reinterpret_cast<uintptr_t*>(grid + 0x80);
        cellFlag = *reinterpret_cast<uint8_t*>(cells + 0x29 +
                       static_cast<uintptr_t>(static_cast<int64_t>(cellIdx)) * 0x30);
    }

    // --- inline PerformWork action, kind 7 ---
    obj = WA_Alloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_fn_TimeCtor) s_fn_TimeCtor(&timeVal, WA_C_172124());
        if (s_fn_ObjInternalCtor) s_fn_ObjInternalCtor(obj);
        uintptr_t o = reinterpret_cast<uintptr_t>(obj);
        *reinterpret_cast<void**>(o)           = s_vtable_Action;
        *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
        *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
        *reinterpret_cast<void**>(o)           = s_vtable_PerformWork;
        *reinterpret_cast<uint64_t*>(o + 0x20) = 0;
        *reinterpret_cast<uintptr_t*>(o + 0x20) = entityInner;
        *reinterpret_cast<float*>(o + 0x28)    = heading;
        *reinterpret_cast<int*>(o + 0x2c)      = 7;
        *reinterpret_cast<uint64_t*>(o + 0x30) = timeVal;
        *reinterpret_cast<uint64_t*>(o + 0x38) = 0;
        *reinterpret_cast<uint8_t*>(o + 0x40)  = 0;
        *reinterpret_cast<uint32_t*>(o + 0x44) = 0xffffffffu;
        pw7 = obj;
    }
    WA_IbpCtor(&ibpA, pw7);
    WA_Append(list, &ibpA);

    // --- ShowTool (~Axe = -2) when the citizen is currently carrying nothing ---
    if (WA_CitizenCarryCount(citizenIBP) < 1) {
        obj = WA_Alloc(0x30, 8);
        void* stAxe = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
            if (s_fn_ShowTool) stAxe = s_fn_ShowTool(obj, &entityInner, -2 /* ~Axe */);
        }
        WA_IbpCtor(&ibpA, stAxe);
        WA_Append(list, &ibpA);
    }

    // --- PickupRawMaterial action (self+0x20 = citizen, self+0x28 = pile) ---
    obj = WA_Alloc(0x30, 8);
    void* prm = nullptr;
    if (obj) {
        uintptr_t targetSlot  = targetInner;
        uintptr_t citizenSlot = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_PickupRawMaterial) prm = s_fn_PickupRawMaterial(obj, &citizenSlot, &targetSlot);
    }
    WA_IbpCtor(&ibpB, prm);
    WA_Append(list, &ibpB);

    // --- CompleteTask action ---
    obj = WA_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t targetSlot  = targetInner;
        uintptr_t citizenSlot = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_CompleteTask) ct = s_fn_CompleteTask(obj, workId, &citizenSlot, &targetSlot, 0);
    }
    WA_IbpCtor(&ibpB, ct);
    WA_Append(list, &ibpB);

    // --- inline PerformWork action, kind 6 ---
    obj = WA_Alloc(0x48, 8);
    void* pw6 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_fn_TimeCtor) s_fn_TimeCtor(&timeVal, WA_C_172124());
        if (s_fn_ObjInternalCtor) s_fn_ObjInternalCtor(obj);
        uintptr_t o = reinterpret_cast<uintptr_t>(obj);
        *reinterpret_cast<void**>(o)           = s_vtable_Action;
        *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
        *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
        *reinterpret_cast<void**>(o)           = s_vtable_PerformWork;
        *reinterpret_cast<uint64_t*>(o + 0x20) = 0;
        *reinterpret_cast<uintptr_t*>(o + 0x20) = entityInner;
        *reinterpret_cast<float*>(o + 0x28)    = heading;
        *reinterpret_cast<int*>(o + 0x2c)      = 6;
        *reinterpret_cast<uint64_t*>(o + 0x30) = timeVal;
        *reinterpret_cast<uint64_t*>(o + 0x38) = 0;
        *reinterpret_cast<uint8_t*>(o + 0x40)  = 0;
        *reinterpret_cast<uint32_t*>(o + 0x44) = 0xffffffffu;
        pw6 = obj;
    }
    WA_IbpCtor(&ibpA, pw6);
    WA_Append(list, &ibpA);

    // --- MoveTo: walk to the pile. pathType = carrying>0 ? 4 : 1; mask = (1<<(cellFlag&0x1f)) | 0xe1f ---
    obj = WA_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        int carried = WA_CitizenCarryCount(citizenIBP);
        WA_CheckPtr(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        WA_CheckPtr(&entityInner);
        int pathType = (carried > 0) ? 4 : 1;
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_fn_FindComponentFamily) familyIBP = s_fn_FindComponentFamily(entityInner, &famRet);
        int mask = static_cast<int>((1u << (cellFlag & 0x1f)) | 0xe1fu);
        if (s_fn_MoveTo) mv = s_fn_MoveTo(obj, familyIBP, pathType, 0, worldPos, mask, 0, WA_MoveSpeed());
    }
    WA_IbpCtor(&ibpB, mv);
    WA_Append(list, &ibpB);

    // --- ShowTool (~Axe = -2) when the citizen IS carrying something ---
    if (WA_CitizenCarryCount(citizenIBP) > 0) {
        obj = WA_Alloc(0x30, 8);
        void* stAxe2 = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
            if (s_fn_ShowTool) stAxe2 = s_fn_ShowTool(obj, &entityInner, -2 /* ~Axe */);
        }
        WA_IbpCtor(&ibpA, stAxe2);
        WA_Append(list, &ibpA);
    }
}

// ============================================================
// PlayAnimation_Action_Ctor (0x1400c2490)
// ============================================================
//   param_1 = citizen InternalBasePtr*
//   param_2 = priority/workId (u32)
//   param_3 = citizenIndex (u32, forwarded to CompleteTask param_5)
// Builds SendComponentMessage (→ aiComp+0x58), CompleteTask, and an inline
// PlayAnimation action, appending the latter two to the task list.
void WorkAct_PlayAnimationCtor_c2490(void* citizenIBP, uint32_t priority, uint32_t citizenIndex) {
    ResolveAll();

    WA_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WA_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_fn_TaskListCleanup) s_fn_TaskListCleanup(list);

    uintptr_t ibp30 = 0, ibpRes20 = 0;

    // --- SendComponentMessage → aiComp+0x58 ---
    void* obj = WA_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_pDword_c70e8 ? *s_pDword_c70e8 : 0;
        WA_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_fn_SendCompMsg) sm = s_fn_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp30, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibp30;

    // --- CompleteTask action (param_3 forwarded as param_5) ---
    obj = WA_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_CompleteTask) ct = s_fn_CompleteTask(obj, priority, &entityInner, &innerIbp, citizenIndex);
    }
    WA_IbpCtor(&ibp30, ct);
    WA_Append(list, &ibp30);

    // --- inline PlayAnimation action ---
    void* inlineObj = WA_Alloc(0x40, 8);
    void* pa = nullptr;
    if (inlineObj) {
        // System::Time::Time(&t, DAT_140172100)
        uint64_t timeVal = 0;
        if (s_fn_TimeCtor) s_fn_TimeCtor(&timeVal, s_pf_172100 ? *s_pf_172100 : 0.0f);
        WA_CheckPtr(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_fn_ObjInternalCtor) s_fn_ObjInternalCtor(inlineObj);
        uintptr_t o = reinterpret_cast<uintptr_t>(inlineObj);
        *reinterpret_cast<void**>(o)          = s_vtable_Action;
        *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
        *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
        *reinterpret_cast<void**>(o)          = s_vtable_PlayAnimation;
        *reinterpret_cast<uintptr_t*>(o + 0x20) = entityInner;
        *reinterpret_cast<uint32_t*>(o + 0x28) = 0;
        *reinterpret_cast<uint64_t*>(o + 0x30) = timeVal;
        *reinterpret_cast<uint64_t*>(o + 0x38) = 0;
        pa = inlineObj;
    }
    WA_IbpCtor(&ibpRes20, pa);
    WA_Append(list, &ibpRes20);
}

// ============================================================
// FUN_1400bb0a0 — grazing/tend work-action ctor (pasturework / tendarea)
// ============================================================
//   param_1 = citizen InternalBasePtr*
//   param_2 = priority (u32)
//   param_3 = workEntity InternalBasePtr* (u64* — *param_3 = inner)
//   param_4 = unused (u32, used as SendComponentMessage msg id slot init)
//   param_5 = float* targetPos (MoveTo destination)
//   param_6 = float* srcPos (used to compute heading; the original calls Vector3 sub
//             + Normalize + Heading but discards the result)
//   param_7 = typeId (u32, second SendComponentMessage msg id)
//   param_8 = u32 (SendComponentMessage param_4)
//   param_9 = u32 (SendComponentMessage param_6)
void WorkAct_GrazingWorkCtor_bb0a0(void* citizenIBP, uint32_t priority, void* workEntityIBP,
                                   uint32_t /*unused4*/, float* targetPos, float* srcPos,
                                   uint32_t typeId, uint32_t flag8, uint32_t cellIdx9) {
    ResolveAll();

    WA_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WA_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_fn_TaskListCleanup) s_fn_TaskListCleanup(list);

    uintptr_t ibp30 = 0, ibpResA = 0, ibpResB = 0;

    // --- SendComponentMessage #1 → aiComp+0x58 ---
    void* obj = WA_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_pDword_c70e8 ? *s_pDword_c70e8 : 0;
        WA_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_fn_SendCompMsg) sm = s_fn_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp30, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibp30;

    // --- CompleteTask action ---
    obj = WA_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_CompleteTask) ct = s_fn_CompleteTask(obj, priority, &entityInner, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp30, ct);
    WA_Append(list, &ibp30);

    // --- heading: Vector3(srcPos-targetPos) → Normalize → Heading (XMM0 → used by Ctor7) ---
    // Disasm: dst=[param_6=srcPos], src=[param_5=targetPos]; v = srcPos - targetPos.
    float heading = 0.0f;
    if (s_fn_Vec3Ctor && targetPos && srcPos) {
        float v3[3];
        s_fn_Vec3Ctor(v3, srcPos[0] - targetPos[0], srcPos[1] - targetPos[1], srcPos[2] - targetPos[2]);
        if (s_fn_Vec3Normalize) {
            void* n = s_fn_Vec3Normalize(v3);
            if (s_fn_Vec3Heading && n) heading = s_fn_Vec3Heading(n);
        }
    }

    // --- ShowTool (~Hoe = -2) ---
    obj = WA_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_ShowTool) st = s_fn_ShowTool(obj, &entityInner, ~0 /* ~Hoe = -1 (Hoe==0); disasm OR R8D,0xFFFFFFFF */);
    }
    WA_IbpCtor(&ibpResA, st);
    WA_Append(list, &ibpResA);

    // --- SendComponentMessage #2 (msg id = param_7, params 8/9) ---
    obj = WA_Alloc(0x40, 8);
    void* sm2 = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = typeId;
        void* entityInner = reinterpret_cast<void*>(*reinterpret_cast<uintptr_t*>(workEntityIBP));
        if (s_fn_SendCompMsg) sm2 = s_fn_SendCompMsg(obj, &entityInner, &msgId, flag8, &innerIbp, cellIdx9);
    }
    WA_IbpCtor(&ibpResB, sm2);
    WA_Append(list, &ibpResB);

    // --- PerformWork_Ctor7: heading(XMM2)=computed heading, actionType(R9)=0xC,
    //     time=DAT_1401721b0, p6=1, p7=3 (disasm: R9D=0xc, not the decompile's Cancel|Normal) ---
    obj = WA_Alloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_fn_TimeCtor) s_fn_TimeCtor(&timeVal, s_pf_1721b0 ? *s_pf_1721b0 : 0.0f);
        if (s_fn_PerformWork7) pw7 = s_fn_PerformWork7(obj, &entityInner, heading, 0xC, static_cast<int64_t>(timeVal), 1, 3);
    }
    WA_IbpCtor(&ibpResA, pw7);
    WA_Append(list, &ibpResA);

    // --- MoveTo (Fastest=3, destination=targetPos) ---
    obj = WA_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        WA_CheckPtr(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        WA_CheckPtr(&entityInner);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_fn_FindComponentFamily) familyIBP = s_fn_FindComponentFamily(entityInner, &famRet);
        float speed = s_pf_172280 ? *s_pf_172280 : 0.0f;
        if (s_fn_MoveTo) mv = s_fn_MoveTo(obj, familyIBP, 3 /*Fastest*/, 0, targetPos, 0xe1f, 0, speed);
    }
    WA_IbpCtor(&ibpResA, mv);
    WA_Append(list, &ibpResA);

    // --- ShowTool (Shovel) ---
    obj = WA_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_ShowTool) st2 = s_fn_ShowTool(obj, &entityInner, 3 /*Shovel; disasm MOV R8D,3*/);
    }
    WA_IbpCtor(&ibpResA, st2);
    WA_Append(list, &ibpResA);
}

// ============================================================
// FUN_1400be6e0 — plant/orchard work-action ctor (foresterwork / plantorchard)
// ============================================================
//   param_1  = citizen InternalBasePtr*
//   param_2  = priority (u32)
//   param_3  = workEntity inner (u64; CompleteTask/CreateNaturalResource param_2)
//   param_4  = unused (u32, SendComponentMessage msg id slot init)
//   param_5  = float* position (MoveTo destination)
//   param_6  = u32 (CreateNaturalResource param_4)
//   param_7  = u32 (CompleteTask param_5)
//   param_8  = longlong* — if *param_8 != 0, emit optional SendComponentMessage #2
//   param_9  = u32* (msg id for optional SendComponentMessage #2 = *param_9)
//   param_10 = u64 unused (SendComponentMessage #2 param_4 = 0 in disasm)
//   param_11 = u32 (SendComponentMessage #2 param_6)
void WorkAct_PlantWorkCtor_be6e0(void* citizenIBP, uint32_t priority, void* workEntityInner,
                                 uint32_t /*unused4*/, float* position, uint32_t createParam6,
                                 uint32_t completeParam7, int64_t* condPtr8, uint32_t* msgIdPtr9,
                                 uint64_t /*unused10*/, uint32_t smParam11) {
    ResolveAll();

    WA_CheckPtr(citizenIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(citizenIBP);
    WA_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    void* list = reinterpret_cast<void*>(aiComp + 0x48);
    if (s_fn_TaskListCleanup) s_fn_TaskListCleanup(list);

    uintptr_t ibp40 = 0, ibpRes8 = 0;

    // --- SendComponentMessage #1 → aiComp+0x58 ---
    void* obj = WA_Alloc(0x40, 8);
    void* sm = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uint32_t msgId = s_pDword_c70e8 ? *s_pDword_c70e8 : 0;
        WA_CheckPtr(citizenIBP);
        void* entityInner = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        if (s_fn_SendCompMsg) sm = s_fn_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
    }
    WA_IbpCtor(&ibp40, sm);
    if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && fn_InternalBasePtr_DeletePtr)
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
    *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibp40;

    // --- CompleteTask action (param_5 = param_7) ---
    obj = WA_Alloc(0x38, 8);
    void* ct = nullptr;
    if (obj) {
        uintptr_t innerIbp = 0;
        WA_IbpCtor(&innerIbp, nullptr);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_CompleteTask) ct = s_fn_CompleteTask(obj, priority, &entityInner, &innerIbp, completeParam7);
    }
    WA_IbpCtor(&ibp40, ct);
    WA_Append(list, &ibp40);

    // --- optional SendComponentMessage #2 when *param_8 != 0 ---
    if (condPtr8 && *condPtr8 != 0) {
        obj = WA_Alloc(0x40, 8);
        void* sm2 = nullptr;
        if (obj) {
            uintptr_t innerIbp = 0;
            WA_IbpCtor(&innerIbp, nullptr);
            uint32_t msgId = msgIdPtr9 ? *msgIdPtr9 : 0;
            uintptr_t entityInner = static_cast<uintptr_t>(*condPtr8);
            if (s_fn_SendCompMsg) sm2 = s_fn_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, smParam11);
        }
        WA_IbpCtor(&ibp40, sm2);
        WA_Append(list, &ibp40);
    }

    // --- ShowTool (~Hoe = -2) ---
    obj = WA_Alloc(0x30, 8);
    void* st = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_ShowTool) st = s_fn_ShowTool(obj, &entityInner, ~0 /* ~Hoe = -1 (Hoe==0); disasm OR R8D,0xFFFFFFFF */);
    }
    WA_IbpCtor(&ibpRes8, st);
    WA_Append(list, &ibpRes8);

    // --- CreateNaturalResource action (param_2 = workEntity inner = *param_8, param_4 = param_6) ---
    obj = WA_Alloc(0x38, 8);
    void* cnr = nullptr;
    if (obj) {
        uintptr_t srcInner = condPtr8 ? static_cast<uintptr_t>(*condPtr8) : 0;
        if (s_fn_CreateNR) cnr = s_fn_CreateNR(obj, workEntityInner, &srcInner, createParam6);
    }
    WA_IbpCtor(&ibpRes8, cnr);
    WA_Append(list, &ibpRes8);

    // --- PerformWork_Ctor7: heading(XMM2)=const DAT_1401722a8, actionType(R9)=0xC,
    //     time=DAT_140172178, p6=1, p7=-1 (disasm: R9D=0xc, XMM2=[0x1401722a8]) ---
    obj = WA_Alloc(0x48, 8);
    void* pw7 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        uint64_t timeVal = 0;
        if (s_fn_TimeCtor) s_fn_TimeCtor(&timeVal, s_pf_172178 ? *s_pf_172178 : 0.0f);
        float headingC = s_pf_1722a8 ? *s_pf_1722a8 : 0.0f;
        if (s_fn_PerformWork7) pw7 = s_fn_PerformWork7(obj, &entityInner, headingC, 0xC, static_cast<int64_t>(timeVal), 1, -1);
    }
    WA_IbpCtor(&ibpRes8, pw7);
    WA_Append(list, &ibpRes8);

    // --- MoveTo (Fastest=3, destination=position) ---
    obj = WA_Alloc(0x480, 8);
    void* mv = nullptr;
    if (obj) {
        WA_CheckPtr(citizenIBP);
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(citizenIBP) + 0x10);
        WA_CheckPtr(&entityInner);
        void* familyIBP = nullptr;
        uintptr_t famRet = 0;
        if (s_fn_FindComponentFamily) familyIBP = s_fn_FindComponentFamily(entityInner, &famRet);
        float speed = s_pf_172280 ? *s_pf_172280 : 0.0f;
        if (s_fn_MoveTo) mv = s_fn_MoveTo(obj, familyIBP, 3 /*Fastest*/, 0, position, 0xe1f, 0, speed);
    }
    WA_IbpCtor(&ibpRes8, mv);
    WA_Append(list, &ibpRes8);

    // --- ShowTool (Shovel) ---
    obj = WA_Alloc(0x30, 8);
    void* st2 = nullptr;
    if (obj) {
        uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(citizenIBP);
        if (s_fn_ShowTool) st2 = s_fn_ShowTool(obj, &entityInner, 3 /*Shovel; disasm MOV R8D,3*/);
    }
    WA_IbpCtor(&ibpRes8, st2);
    WA_Append(list, &ibpRes8);
}
