// code/Game/Work/work_act_clear.cpp
// Work clear-area / harvest-area action-chain constructor.
//
// Faithful 1:1 port of ShowTool_Action_Ctor4 @ 0x1400c1530 (Application-x64.exe),
// exposed here as WorkAct_HarvestAreaCtor_c1530. Despite the (mislabeled) Ghidra
// name, this is a LARGE work-action-queue builder (~0x948 bytes): it builds the
// citizen's action chain on the CitizenAIComponent task list and stores it on the
// entity (citizen+0x278 -> aiComp, list base aiComp+0x48; the SendComponentMessage
// action is stored at aiComp+0x58 after DeletePtr of the previous one).
//
// This is a sibling of WorkAct_PerformWorkCtor_c1eb0 in work_base_actionctor.cpp,
// but differs in real ways (verified against the actual disassembly):
//   * param_3 is a LOOP COUNT: it iterates param_3 times over the param_4[] array
//     (param_4 advances by 8 each iteration), and for each element builds an INLINE
//     RemoveNaturalResource action (vtable 0x140160050) + appends it + sets bit 0x14
//     (0x100000) on that target's +0x20 flags, detaching via 0x1400c3470 on change.
//   * the post-loop chain is: PerformWork_Ctor7 (0x1400c9720), MoveTo (0x1400c9170),
//     then an INLINE ShowTool action (vtable 0x1401589b0). There is NO inline-
//     PerformWork / AnimateNaturalResource block (that belongs to the c1eb0 sibling).
//   * the list "append" is INLINED in the disasm for every action except the first
//     two (CompleteTask + the first ShowTool), which use FUN_1400c54e0. We mirror
//     the inlined append faithfully via WA_AppendInline (same list-growth logic the
//     out-of-line FUN_1400c54e0 / FUN_1400032e0 perform on aiComp+0x48..+0x60).
//
// Instruction-faithful: do NOT "improve" behavior. Every offset / constant below
// cites its disasm address. Self-contained: resolves every sub-callee via
// ResolveAddr(Module::Exe, 0xADDR). No hooks.h, no RegisterExeAddr. In DLL mode
// addresses resolve to the loaded exe; in standalone they resolve to 0 unless the
// address is registered, in which case the guarded call is skipped (project norm).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// ============================================================
// Sub-callee / dispatch-slot typedefs (this file only; s_wac_* prefix)
// ============================================================

// --- exe-code sub-callees (0x1400xxxxx; ResolveAddr returns the fn ptr directly) ---

// Entity_impl_29a0 (0x1400c29a0): heading float toward path target → XMM0.
// Disasm 0x1400c1586: RCX=param_5 (float* worldPos), RDX=&local_res8 (entity inner).
typedef float (*s_wac_Entity29a0_t)(float* worldPos, void* entityInnerSlot);
// FUN_1400c5400 (TaskListCleanup): cancel pending work actions on the list.
typedef void  (*s_wac_TaskCleanup_t)(void* list);
// FUN_1400c54e0 (TaskListAppend): append an action IBP to the task list.
typedef void  (*s_wac_TaskAppend_t)(void* list, void* actionIBP);
// FUN_1400032e0 (TaskListGrow): grow the task list backing store to N.
typedef void  (*s_wac_TaskGrow_t)(void* list, int newCap);
// FUN_1400c3470 (WorkDetach): conditional work-target detach (tail).
typedef void  (*s_wac_WorkDetach_t)(void* target);
// Entity_FindComponent_Family (0x1400012d0): member-style RCX=entityInner, RDX=retIBP → RAX.
typedef void* (*s_wac_FindCompFamily_t)(uintptr_t entityInner, void* retIBP);
// SendComponentMessage_Action_CopyCtor (0x1400cb1d0).
typedef void* (*s_wac_SendCompMsg_t)(void* self, void* p2, void* p3, uint32_t p4, void* p5, uint32_t p6);
// CompleteTask_Action_CopyCtor (0x1400c5da0).
typedef void* (*s_wac_CompleteTask_t)(void* self, uint32_t workId, void* p3, void* p4, uint32_t p5);
// ShowTool_Action_Ctor (0x140042890).
typedef void* (*s_wac_ShowTool_t)(void* self, void* entityInnerSlot, int toolType);
// PerformWork_Action_Ctor7 (0x1400c9720). Win64 ABI (verified at 0x1400c1986..0x1400c1993):
//   RCX=self, RDX=&entityInner, XMM2=heading (3rd positional, float), R9D=actionType
//   (4th positional, int), [RSP+0x20]=time(int64), [RSP+0x28] byte=p6, [RSP+0x30] int=p7.
typedef void* (*s_wac_PerformWork7_t)(void* self, void* entityInnerSlot, float heading,
                                      int actionType, int64_t time, uint8_t p6, int p7);
// MoveTo_Action_Ctor (0x1400c9170). Disasm 0x1400c1b9b..0x1400c1ba4:
//   RCX=self, RDX=familyIBP, R8D=pathType, R9D=0, [RSP+0x20]=destination(float*),
//   [RSP+0x28]=0xe1f, [RSP+0x30]=0, [RSP+0x38]=speed(float).
typedef void* (*s_wac_MoveTo_t)(void* self, void* familyIBP, int pathType, int p4,
                                float* destination, int p6, int p7, float speed);

// --- dispatch slots (deref once to get runtime fn ptr) ---
typedef void  (*s_wac_CheckPtr_t)(void* ibp);                    // 0x140152ab0
typedef bool  (*s_wac_IsDerivedFrom_t)(void* obj, uint32_t tid); // 0x140152a98
typedef void* (*s_wac_MemAlloc_t)(uint64_t size, uint32_t align);// 0x140152b60
typedef void  (*s_wac_IbpCtor_t)(void* ibp, void* obj);          // 0x140152a90
typedef void  (*s_wac_DeletePtr_t)(void* ibp);                   // 0x140151ab8
typedef void* (*s_wac_ObjInternalCtor_t)(void* self);            // 0x140152768
typedef void  (*s_wac_MemFree_t)(void* ptr);                     // 0x140152b68

static s_wac_Entity29a0_t      s_wac_Entity29a0     = nullptr;
static s_wac_TaskCleanup_t     s_wac_TaskCleanup    = nullptr;
static s_wac_TaskAppend_t      s_wac_TaskAppend     = nullptr;
static s_wac_TaskGrow_t        s_wac_TaskGrow       = nullptr;
static s_wac_WorkDetach_t      s_wac_WorkDetach     = nullptr;
static s_wac_FindCompFamily_t  s_wac_FindCompFamily = nullptr;
static s_wac_SendCompMsg_t     s_wac_SendCompMsg    = nullptr;
static s_wac_CompleteTask_t    s_wac_CompleteTask   = nullptr;
static s_wac_ShowTool_t        s_wac_ShowTool       = nullptr;
static s_wac_PerformWork7_t    s_wac_PerformWork7   = nullptr;
static s_wac_MoveTo_t          s_wac_MoveTo         = nullptr;

static s_wac_CheckPtr_t        s_wac_CheckPtr       = nullptr;
static s_wac_IsDerivedFrom_t   s_wac_IsDerivedFrom  = nullptr;
static s_wac_MemAlloc_t        s_wac_MemAlloc       = nullptr;
static s_wac_IbpCtor_t         s_wac_IbpCtor        = nullptr;
static s_wac_DeletePtr_t       s_wac_DeletePtr      = nullptr;
static s_wac_ObjInternalCtor_t s_wac_ObjInternalCtor= nullptr;
static s_wac_MemFree_t         s_wac_MemFree        = nullptr;

// Inline-object vtables + globals (exe .rdata).
static void* s_wac_vtable_Action               = nullptr; // 0x14015faa0 (inline base)
static void* s_wac_vtable_RemoveNaturalResource= nullptr;  // 0x140160050 (loop inline action)
static void* s_wac_vtable_ShowTool             = nullptr;  // 0x1401589b0 (tail inline ShowTool)
static const uint32_t* s_wac_pTid_c7b68 = nullptr;         // 0x1401c7b68 (target derive type id)
static const uint32_t* s_wac_pMsgId_c70e8 = nullptr;       // 0x1401c70e8 (SendComponentMessage msg id)
static const float*    s_wac_pf_172280 = nullptr;          // 0x140172280 (MoveTo speed = -1.0f)

static bool s_wac_resolved = false;

static void s_wac_ResolveAll() {
    if (s_wac_resolved) return;
    s_wac_resolved = true;

    s_wac_Entity29a0     = reinterpret_cast<s_wac_Entity29a0_t>(ResolveAddr(Module::Exe, 0x1400c29a0));
    s_wac_TaskCleanup    = reinterpret_cast<s_wac_TaskCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_wac_TaskAppend     = reinterpret_cast<s_wac_TaskAppend_t>(ResolveAddr(Module::Exe, 0x1400c54e0));
    s_wac_TaskGrow       = reinterpret_cast<s_wac_TaskGrow_t>(ResolveAddr(Module::Exe, 0x1400032e0));
    s_wac_WorkDetach     = reinterpret_cast<s_wac_WorkDetach_t>(ResolveAddr(Module::Exe, 0x1400c3470));
    s_wac_FindCompFamily = reinterpret_cast<s_wac_FindCompFamily_t>(ResolveAddr(Module::Exe, 0x1400012d0));
    s_wac_SendCompMsg    = reinterpret_cast<s_wac_SendCompMsg_t>(ResolveAddr(Module::Exe, 0x1400cb1d0));
    s_wac_CompleteTask   = reinterpret_cast<s_wac_CompleteTask_t>(ResolveAddr(Module::Exe, 0x1400c5da0));
    s_wac_ShowTool       = reinterpret_cast<s_wac_ShowTool_t>(ResolveAddr(Module::Exe, 0x140042890));
    s_wac_PerformWork7   = reinterpret_cast<s_wac_PerformWork7_t>(ResolveAddr(Module::Exe, 0x1400c9720));
    s_wac_MoveTo         = reinterpret_cast<s_wac_MoveTo_t>(ResolveAddr(Module::Exe, 0x1400c9170));

    // Dispatch slots — deref once to reach the runtime fn ptr.
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152ab0)) s_wac_CheckPtr        = *reinterpret_cast<s_wac_CheckPtr_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a98)) s_wac_IsDerivedFrom   = *reinterpret_cast<s_wac_IsDerivedFrom_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b60)) s_wac_MemAlloc        = *reinterpret_cast<s_wac_MemAlloc_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a90)) s_wac_IbpCtor         = *reinterpret_cast<s_wac_IbpCtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140151ab8)) s_wac_DeletePtr       = *reinterpret_cast<s_wac_DeletePtr_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152768)) s_wac_ObjInternalCtor = *reinterpret_cast<s_wac_ObjInternalCtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152b68)) s_wac_MemFree         = *reinterpret_cast<s_wac_MemFree_t*>(s);

    // Engine-funcs fallbacks (same IAT targets) when the dispatch slot is unregistered.
    if (!s_wac_CheckPtr  && fn_InternalBasePtr_CheckPtr)  s_wac_CheckPtr  = reinterpret_cast<s_wac_CheckPtr_t>(fn_InternalBasePtr_CheckPtr);
    if (!s_wac_IbpCtor   && fn_InternalBasePtr_Ctor)      s_wac_IbpCtor   = reinterpret_cast<s_wac_IbpCtor_t>(fn_InternalBasePtr_Ctor);
    if (!s_wac_DeletePtr && fn_InternalBasePtr_DeletePtr) s_wac_DeletePtr = reinterpret_cast<s_wac_DeletePtr_t>(fn_InternalBasePtr_DeletePtr);
    if (!s_wac_ObjInternalCtor && fn_ObjectInternal_Ctor) s_wac_ObjInternalCtor = reinterpret_cast<s_wac_ObjInternalCtor_t>(fn_ObjectInternal_Ctor);
    if (!s_wac_IsDerivedFrom   && fn_IsDerivedFrom)       s_wac_IsDerivedFrom   = reinterpret_cast<s_wac_IsDerivedFrom_t>(fn_IsDerivedFrom);

    s_wac_vtable_Action                = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x14015faa0));
    s_wac_vtable_RemoveNaturalResource = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x140160050));
    s_wac_vtable_ShowTool              = reinterpret_cast<void*>(ResolveAddr(Module::Exe, 0x1401589b0));
    s_wac_pTid_c7b68   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b68));
    s_wac_pMsgId_c70e8 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c70e8));
    s_wac_pf_172280    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
}

// Small guarded wrappers.
static inline void* WAC_Alloc(uint64_t size, uint32_t align) {
    if (s_wac_MemAlloc) return s_wac_MemAlloc(size, align);
    if (fn_MemAlloc && g_pMemAllocSingleton) return fn_MemAlloc(g_pMemAllocSingleton, size, align);
    return nullptr;
}
static inline void WAC_CheckPtr(void* p)            { if (s_wac_CheckPtr) s_wac_CheckPtr(p); }
static inline void WAC_IbpCtor(void* ibp, void* o)  { if (s_wac_IbpCtor) s_wac_IbpCtor(ibp, o); }

// Inlined task-list append, mirroring the disasm's inlined FUN_1400c54e0 body
// (the list lives at aiComp+0x48; its header fields are at lVar3+0x50/+0x54/+0x60,
// where lVar3 = aiComp and plVar15 = &aiComp[0x48] is the backing-array pointer).
//   listArr = (void**)*plVar15 ; count@+0x50 ; cap@+0x54 ; index@+0x60.
// Faithful to disasm @0x1400c17ad..0x1400c18ec (RemoveNaturalResource append) and
// the identical blocks for PerformWork / inline ShowTool.
static void WAC_AppendInline(uintptr_t aiComp, void** plVar15, uintptr_t actionIbpVal) {
    int iVar16 = *reinterpret_cast<int*>(aiComp + 0x50);
    if (iVar16 == 0) {
        // First append: initialize to capacity 0x10, copy any prior contents.
        void* pvVar11 = *reinterpret_cast<void**>(plVar15);
        *reinterpret_cast<uint32_t*>(aiComp + 0x50) = 0x10;
        if (*reinterpret_cast<int*>(aiComp + 0x54) < 0x10) {
            *reinterpret_cast<uint32_t*>(aiComp + 0x54) = 0x10;
            void* nb = WAC_Alloc(static_cast<uint64_t>(*reinterpret_cast<int*>(aiComp + 0x54)) << 3, 8);
            *reinterpret_cast<void**>(plVar15) = nb;
        }
        // The original copies index 0 (iVar16==0 ⇒ start = 0), zeroing the rest.
        // Disasm: start = (RDI==*plVar15) ? old-count(EBX) : 0; here EBX(old count)=0.
        int64_t start = 0;
        if (pvVar11 == *reinterpret_cast<void**>(plVar15)) start = iVar16; // ==0 here
        for (int64_t i = start; i < 0x10; ++i) {
            uint64_t* slot = reinterpret_cast<uint64_t*>(*reinterpret_cast<uintptr_t*>(plVar15) + i * 8);
            if (i < static_cast<int64_t>(iVar16)) {
                if (slot) { *slot = 0; *slot = reinterpret_cast<uint64_t*>(pvVar11)[i]; }
            } else if (slot) {
                *slot = 0;
            }
        }
        if (pvVar11 != *reinterpret_cast<void**>(plVar15) && s_wac_MemFree) s_wac_MemFree(pvVar11);
    } else if (*reinterpret_cast<int*>(aiComp + 0x60) + 1 == iVar16) {
        if (s_wac_TaskGrow) s_wac_TaskGrow(plVar15, iVar16 * 2);
    }

    // Drain any pending nodes from the current index slot (cancel/detach chain).
    if (*reinterpret_cast<int*>(aiComp + 0x60) >= 0) {
        for (void* node = reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(plVar15))[*reinterpret_cast<int*>(aiComp + 0x60)];
             node != nullptr;
             node = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x10)) {
            WAC_CheckPtr(&node);
            if (*reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(node) + 0x18) != 0) {
                WAC_CheckPtr(&node);
                *reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(node) + 0x18) = 0;
                // virtual call vtable+0x38(node, plVar15)
                void** vt = *reinterpret_cast<void***>(node);
                typedef void (*Slot38_t)(void* self, void* list);
                reinterpret_cast<Slot38_t>(vt[0x38 / 8])(node, plVar15);
            }
            WAC_CheckPtr(&node);
        }
    }

    *reinterpret_cast<int*>(aiComp + 0x60) += 1;
    reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(plVar15))[*reinterpret_cast<int*>(aiComp + 0x60)] = actionIbpVal;
}

// ============================================================
// WorkAct_HarvestAreaCtor_c1530 (0x1400c1530)
// ============================================================
//   param_1 (RCX) = citizen InternalBasePtr*
//   param_2 (EDX) = workId (u32, CompleteTask param_2)
//   param_3 (R8D) = count of param_4[] entries to clear (loop count; signed)
//   param_4 (R9)  = InternalBasePtr* array (count entries; advances by 8/iter)
//   param_5 (stack, [RBP+0x60]) = float* worldPos (heading + MoveTo destination)
//   param_6 (stack, [RBP+0x68]) = u32 (CompleteTask param_5)
//   param_7 (stack, [RBP+0x70]) = dword (unused on entry; reloaded as SendComponentMessage msg id)
void WorkAct_HarvestAreaCtor_c1530(void* param_1, uint32_t param_2, int param_3,
                                   void** param_4, float* param_5, uint32_t param_6,
                                   uint32_t /*param_7*/) {
    s_wac_ResolveAll();

    // --- heading = Entity_impl_29a0(param_5, &{param_4[0] inner}) → XMM6 (0x1400c1563..0x1400c158b) ---
    WAC_CheckPtr(param_4);
    uintptr_t firstEntityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_4) + 0x10);
    float heading = 0.0f;
    if (s_wac_Entity29a0) heading = s_wac_Entity29a0(param_5, &firstEntityInner);

    // --- derive target component off param_4[0]+0x18; keep if IsDerivedFrom(c7b68) (0x1400c158e..0x1400c15b6) ---
    WAC_CheckPtr(param_4);
    void* targetComp = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(param_4) + 0x18);
    uintptr_t local_60 = 0; // derived target (the "pIVar8 = local_60" used post-loop)
    bool derived = false;
    if (s_wac_pTid_c7b68 && s_wac_IsDerivedFrom)
        derived = s_wac_IsDerivedFrom(targetComp, *s_wac_pTid_c7b68);
    if (derived) local_60 = reinterpret_cast<uintptr_t>(targetComp);

    // --- list base = (*(citizen)+0x278) + 0x48 ; cleanup (0x1400c15ba..0x1400c15e2) ---
    WAC_CheckPtr(param_1);
    uintptr_t lVar3 = *reinterpret_cast<uintptr_t*>(param_1);
    WAC_CheckPtr(reinterpret_cast<void*>(lVar3 + 0x278));
    uintptr_t aiComp = *reinterpret_cast<uintptr_t*>(lVar3 + 0x278);
    void** plVar15 = reinterpret_cast<void**>(aiComp + 0x48);
    if (s_wac_TaskCleanup) s_wac_TaskCleanup(plVar15);

    uintptr_t ibpA = 0; // local_68 / local_res20 scratch IBP
    uintptr_t ibpB = 0; // second scratch IBP (local_res20 alt)

    // --- SendComponentMessage action → stored at aiComp+0x58 (DeletePtr prior) (0x1400c15e7..0x1400c1677) ---
    {
        void* obj = WAC_Alloc(0x40, 8); // LEA EDX,[R12+8]; LEA ECX,[RDX+0x38] = 0x40
        void* sm = nullptr;
        if (obj) {
            uintptr_t innerIbp = 0;
            WAC_IbpCtor(&innerIbp, nullptr);
            uint32_t msgId = s_wac_pMsgId_c70e8 ? *s_wac_pMsgId_c70e8 : 0; // DWORD_1401c70e8
            WAC_CheckPtr(param_1);
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);
            if (s_wac_SendCompMsg) sm = s_wac_SendCompMsg(obj, &entityInner, &msgId, 0, &innerIbp, 0);
        }
        WAC_IbpCtor(&ibpA, sm);
        if (*reinterpret_cast<uintptr_t*>(aiComp + 0x58) != 0 && s_wac_DeletePtr)
            s_wac_DeletePtr(reinterpret_cast<void*>(aiComp + 0x58));
        *reinterpret_cast<uintptr_t*>(aiComp + 0x58) = ibpA;
    }

    // --- CompleteTask action; append via FUN_1400c54e0 (0x1400c1677..0x1400c16dc) ---
    {
        void* obj = WAC_Alloc(0x38, 8); // MOV EDX,8; LEA ECX,[RDX+0x30] = 0x38
        void* ct = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);
            // RDX=&entityInner, R8=&local_res8(innerIbp from prior IbpCtor), but disasm
            // reuses local_res8 (just IbpCtor'd to 0 at +0x1693). Mirror with fresh inner.
            uintptr_t innerIbp = 0;
            WAC_IbpCtor(&innerIbp, nullptr);
            if (s_wac_CompleteTask) ct = s_wac_CompleteTask(obj, param_2, &entityInner, &innerIbp, param_6);
        }
        WAC_IbpCtor(&ibpB, ct);
        if (s_wac_TaskAppend) s_wac_TaskAppend(plVar15, &ibpB);
    }

    // --- ShowTool (~Hoe = -1; disasm OR R8D,0xffffffff) action; append via FUN_1400c54e0 (0x1400c16dc..0x1400c1729) ---
    {
        void* obj = WAC_Alloc(0x30, 8); // MOV EDX,8; LEA ECX,[RDX+0x28] = 0x30
        void* st = nullptr;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);
            if (s_wac_ShowTool) st = s_wac_ShowTool(obj, &entityInner, ~0); // OR R8D,0xffffffff @0x1400c16fb
        }
        WAC_IbpCtor(&ibpA, st);
        if (s_wac_TaskAppend) s_wac_TaskAppend(plVar15, &ibpA);
    }

    // --- LOOP: for i in [0, param_3): inline RemoveNaturalResource action per param_4[i]
    //     (0x1400c1729..0x1400c1930). param_4 advances by 8 each iter; lVar18=param_3 counter. ---
    if (param_3 > 0) {
        int64_t loopCtr = static_cast<int64_t>(param_3); // R12=R14=param_3
        do {
            WAC_CheckPtr(param_4);
            uintptr_t innerR4 = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_4) + 0x10); // RDI

            void* obj = WAC_Alloc(0x38, 8); // MOV EDX,8; LEA ECX,[RDX+0x30] @0x1400c174a = 8+0x30 = 0x38
            uintptr_t actionObj = 0;        // R14 (the built action, or 0)
            if (obj) {
                uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1); // RBX = *(*param_1)
                if (s_wac_ObjInternalCtor) s_wac_ObjInternalCtor(obj);
                uintptr_t o = reinterpret_cast<uintptr_t>(obj);
                *reinterpret_cast<void**>(o)           = s_wac_vtable_Action;                // 0x14015faa0 @0x1400c176d
                *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
                *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
                *reinterpret_cast<void**>(o)           = s_wac_vtable_RemoveNaturalResource; // 0x140160050 @0x1400c1784
                *reinterpret_cast<uintptr_t*>(o + 0x20)= entityInner;                        // citizen inner
                *reinterpret_cast<uintptr_t*>(o + 0x28)= innerR4;                            // param_4[i] inner
                *reinterpret_cast<uint8_t*>(o + 0x30)  = 1;                                  // @0x1400c1796
                actionObj = o;
            }
            WAC_IbpCtor(&ibpA, reinterpret_cast<void*>(actionObj)); // &local_res8 → ibpA scratch

            // inline append of ibpA (0x1400c17ad..0x1400c18ec)
            WAC_AppendInline(aiComp, plVar15, ibpA);

            // set bit 0x14 (0x100000) on param_4[i] target's +0x20 flags; on change & +0x10 set,
            // detach via FUN_1400c3470 (0x1400c18f0..0x1400c1929)
            WAC_CheckPtr(param_4);
            uintptr_t targetObj = *reinterpret_cast<uintptr_t*>(param_4);
            uint32_t oldFlags = *reinterpret_cast<uint32_t*>(targetObj + 0x20);
            uint32_t newFlags = oldFlags | 0x100000u; // BTS EAX,0x14
            *reinterpret_cast<uint32_t*>(targetObj + 0x20) = newFlags;
            if (static_cast<uint16_t>(oldFlags ^ newFlags) != 0 &&            // TEST CX,CX
                *reinterpret_cast<uintptr_t*>(targetObj + 0x10) != 0) {
                WAC_CheckPtr(reinterpret_cast<void*>(targetObj + 0x10));
                void* detach = *reinterpret_cast<void**>(targetObj + 0x10);
                if (s_wac_WorkDetach) s_wac_WorkDetach(detach);
            }

            param_4 = param_4 + 1; // ADD R15,0x8 (param_4 is void** → +1 entry = +8 bytes)
            --loopCtr;             // DEC R12
        } while (loopCtr != 0);
    }

    // --- actionType: EBX=8 default; if dword[derivedTarget+0x80] <= 0xa: EBX = that+9 (0x1400c1936..0x1400c194b) ---
    int actionType = 8;
    if (derived && *reinterpret_cast<uint32_t*>(local_60 + 0x80) <= 0xa)
        actionType = static_cast<int>(*reinterpret_cast<uint32_t*>(local_60 + 0x80)) + 9;

    // --- PerformWork_Ctor7: heading(XMM2)=heading, actionType(R9)=actionType,
    //     time=*(derivedTarget+0x68), p6=1, p7=*(derivedTarget+0x80) (0x1400c194e..0x1400c1998) ---
    {
        void* obj = WAC_Alloc(0x48, 8); // MOV EDX,8; LEA ECX,[RDX+0x40] = 0x48
        uintptr_t pwIbp = 0;
        if (obj && derived) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);
            int p7  = static_cast<int>(*reinterpret_cast<uint32_t*>(local_60 + 0x80)); // [RSP+0x30]
            int64_t time = *reinterpret_cast<int64_t*>(local_60 + 0x68);               // [RSP+0x20]
            void* pw = nullptr;
            if (s_wac_PerformWork7) pw = s_wac_PerformWork7(obj, &entityInner, heading, actionType, time, 1, p7);
            WAC_IbpCtor(&ibpA, pw);
            pwIbp = ibpA;
        } else {
            WAC_IbpCtor(&ibpA, nullptr);
            pwIbp = ibpA;
        }
        WAC_AppendInline(aiComp, plVar15, pwIbp);
    }

    // --- MoveTo (path type from switch on dword[derivedTarget+0x80]) (0x1400c1aed..0x1400c1ba9) ---
    {
        void* obj = WAC_Alloc(0x480, 8); // MOV ECX,0x480
        uintptr_t mvIbp = 0;
        if (obj && derived) {
            // switch(dword[local_60+0x80]); ECX = that+1 (INC ECX); CMP ECX,8; JA → default Slow(1).
            // jump table @0x1400c1e7c (idx = resKind+1, 0-based):
            //   idx0→0x1b3f Slow(1); idx1,2,5,6,7,8→0x1b31 Fast(2); idx3,4→0x1b38 Fastest(3).
            int pathType;
            uint32_t resKind = *reinterpret_cast<uint32_t*>(local_60 + 0x80);
            uint32_t idx = resKind + 1; // INC ECX @0x1400c1b14
            if (idx > 8) {
                pathType = 1; // Slow (default; CMP ECX,8 / JA @0x1400c1b16)
            } else {
                switch (idx) {
                    case 0:                                  pathType = 1; break; // Slow
                    case 1: case 2: case 5: case 6:
                    case 7: case 8:                          pathType = 2; break; // Fast
                    case 3: case 4:                          pathType = 3; break; // Fastest
                    default:                                 pathType = 1; break;
                }
            }
            WAC_CheckPtr(param_1);
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(param_1) + 0x10);
            WAC_CheckPtr(&entityInner);
            uintptr_t famRet = 0;
            void* familyIBP = nullptr;
            if (s_wac_FindCompFamily) familyIBP = s_wac_FindCompFamily(entityInner, &famRet);
            float speed = s_wac_pf_172280 ? *s_wac_pf_172280 : 0.0f; // -1.0f
            void* mv = nullptr;
            if (s_wac_MoveTo) mv = s_wac_MoveTo(obj, familyIBP, pathType, 0, param_5, 0xe1f, 0, speed);
            WAC_IbpCtor(&ibpA, mv);
            mvIbp = ibpA;
        } else {
            WAC_IbpCtor(&ibpA, nullptr);
            mvIbp = ibpA;
        }
        WAC_AppendInline(aiComp, plVar15, mvIbp);
    }

    // --- inline ShowTool action (vtable 0x1401589b0); toolType = dword[derivedTarget+0x80]
    //     (0x1400c1cef..0x1400c1e58) ---
    {
        void* obj = WAC_Alloc(0x30, 8); // MOV EDX,8; LEA ECX,[RDX+0x28] = 0x30
        uintptr_t stIbp = 0;
        if (obj) {
            uintptr_t entityInner = *reinterpret_cast<uintptr_t*>(param_1);            // RBX
            uint32_t toolType = derived ? *reinterpret_cast<uint32_t*>(local_60 + 0x80) : 0; // EDI = [R15+0x80]
            if (s_wac_ObjInternalCtor) s_wac_ObjInternalCtor(obj);
            uintptr_t o = reinterpret_cast<uintptr_t>(obj);
            *reinterpret_cast<void**>(o)           = s_wac_vtable_Action;   // 0x14015faa0 @0x1400c1d1e
            *reinterpret_cast<uint64_t*>(o + 0x10) = 0;
            *reinterpret_cast<uint8_t*>(o + 0x18)  = 0;
            *reinterpret_cast<void**>(o)           = s_wac_vtable_ShowTool; // 0x1401589b0 @0x1400c1d31
            *reinterpret_cast<uintptr_t*>(o + 0x20)= entityInner;
            *reinterpret_cast<uint32_t*>(o + 0x28) = toolType;             // dword @0x1400c1d3f
            stIbp = o;
            WAC_IbpCtor(&ibpB, reinterpret_cast<void*>(o)); // &local_res20
            stIbp = ibpB;
        } else {
            WAC_IbpCtor(&ibpB, nullptr);
            stIbp = ibpB;
        }
        WAC_AppendInline(aiComp, plVar15, stIbp);
    }
}
