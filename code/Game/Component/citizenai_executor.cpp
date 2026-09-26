// code/Game/Component/citizenai_executor.cpp
//
// Brick 4b — the ACTION-STACK EXECUTOR that runs the Action/Work objects the
// CitizenAI handlers push onto the citizen action stack (CharacterComponent+0x48),
// plus the CharacterComponent per-tick "move-tick" that drives it.
//
// Faithful 1:1 ports of three exe functions (lead-verified addresses this session):
//   0x1400c4c50  CitizenAI_ActionExecutor_c4c50   — the action-stack executor
//   0x1400c4bb0  CitizenAI_ActionFlush_c4bb0      — the executor's flush/cleanup tail
//   0x1401094c0  CharacterComponent_MoveTick_1094c0 — runs the executor IF the stack is
//                non-empty, then (only when char-index>=0) updates the ModelComponent
//                transform/animation. Ghidra MISLABELS this "CharacterComponent_OnAttach".
//
// CRITICAL ABI (transcribed from disassembly at 0x1400c4c50):
//   * Stack object layout (param_1 = CharacterComponent+0x48):
//       [param_1+0x00] = backing array base (slots of InternalBasePtr, 8 bytes each)
//       [param_1+0x18] = top index (int)   (== CharacterComponent+0x60)
//       [param_1+0x10] = the "flush" list head (used by c4bb0)
//   * Each stack entry is an InternalBasePtr; the wrapped node:
//       node+0x18 = 1-byte "entered/in-use" flag
//       node+0x10 = next-node link (the do/while advances via this, NOT node+0x18)
//       node->vtable+0x20 = precheck slot, +0x28 = main slot, +0x38 = exit slot
//   * vcall ABI (bug #6 — explicit forwarding):
//       precheck:  CALL [vt+0x20] with RCX=node, RDX=stack            (asm 0x1400c4ced)
//       main:      CALL [vt+0x28] with RCX=node, RDX=stack, R8=time   (asm 0x1400c4d57)
//       exit:      CALL [vt+0x38] with RCX=node, RDX=stack            (asm 0x1400c4ddc)
//   * RETURN WIDTH (bug #7): each slot return is read as INT (EAX), compared ==1 / ==0 / ==2.
//     Keep `int` returns through the function pointer typedefs — never bool.
//
// Crash-safety for the move-tick (0x1401094c0): in standalone the citizen's
// ModelComponent (CharacterComponent+0x68), its MeshInstance(+0x28)/Geometry/Picking,
// and the char render-index (self+0x40) are INERT (zeroed/negative). The disasm shape is:
//     RCX = self+0x48
//     if (*(int*)(RCX+0x18) /* self+0x60 top index */ >= 0) FUN_1400c4c50(RCX, time);
//     if (*(int*)(self+0x40) < 0) return;        // <-- early-return SKIPS the whole tail
//     ...big transform/animation/rendering block...
// Because self+0x40 < 0 in standalone, the early-return is the intended crash-safety: the
// transform tail (which touches the inert ModelComponent/MeshInstance/Geometry/Picking
// rendering layer) is NEVER reached. It is ported faithfully below but all its engine
// dispatch-slots / exe sub-callees are null-guarded; it stays dormant until ModelComponent
// is brought live. See the report for the rendering-coupled sub-callee table.
//


// graphics/*.h FIRST: they pull runtime/types/volume/Volume.h, whose global-scope
// `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once <windows.h> has
// been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
#include "graphics/Mesh.h"
#include <new>
#include "engine_funcs.h"
#include "options.h"
#include "perf_terrain.h"
#include "resolve.h"
#include "perf_experimental.h"
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName
#include "component_factory_base.h"        // ::ComponentFactory::the live gate
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)

namespace {

// ---------------------------------------------------------------------------
// Move-tick movement diagnostic (BANISHED_MOVETICK_TRACE). Default-off; proves
// the move-tick TAIL runs (char-index>=0) and that the per-frame transform write
// actually changes the citizen's world position at ModelComponent+0x158.
// Tracks the first few citizens whose tail executes and logs position deltas.
// ---------------------------------------------------------------------------
static int  s_mtTrace = -1;            // -1 = unread, 0 = off, 1 = on
static inline bool MoveTickTraceOn() {
    if (s_mtTrace < 0) {
        const char* e = Opt().movetickTrace;
        s_mtTrace = (e && e[0] && e[0] != '0') ? 1 : 0;
    }
    return s_mtTrace == 1;
}

// ---------------------------------------------------------------------------
// Action vtable slot signatures (return INT — bug #7).
//   precheck (+0x20): (node, stack)
//   main     (+0x28): (node, stack, time)
//   exit     (+0x38): (node, stack)
// ---------------------------------------------------------------------------
typedef int (*ActSlotPre_t)(void* node, void* stack);
typedef int (*ActSlotMain_t)(void* node, void* stack, void* time);
typedef int (*ActSlotExit_t)(void* node, void* stack);

static inline int CallPre(int64_t* node, void* stack) {
    void* vt = *reinterpret_cast<void**>(node);
    return reinterpret_cast<ActSlotPre_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt) + 0x20))(node, stack);
}
static inline int CallMain(int64_t* node, void* stack, void* time) {
    void* vt = *reinterpret_cast<void**>(node);
    return reinterpret_cast<ActSlotMain_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt) + 0x28))(node, stack, time);
}
static inline void CallExit(int64_t* node, void* stack) {
    void* vt = *reinterpret_cast<void**>(node);
    reinterpret_cast<ActSlotExit_t>(*reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt) + 0x38))(node, stack);
}


// FUN_1400c5400 — TaskListCleanup, called when a main-slot returns 2 (push-more / requeue).
typedef void (*fn_taskCleanup_t)(void* stack);   // 0x1400c5400 (registered: WorkBase_TaskListCleanup_c5400)
static fn_taskCleanup_t s_fn_taskCleanup = nullptr;
static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_fn_taskCleanup = reinterpret_cast<fn_taskCleanup_t>(ResolveAddr(Module::Exe, 0x1400c5400));
    s_init = true;
}

} // namespace

// ===========================================================================
// FUN_1400c4bb0 — flush/cleanup tail. Walk the flush list at (stack+0x10):
//   for each node: CheckPtr, set node+0x18 = 1, call precheck(+0x20)(node) — break if
//   ret != 1; CheckPtr; advance via node+0x10. After the walk: DeletePtr(stack+0x10) +
//   re-Ctor the slot (InternalBasePtr default ctor, null internal).
// param_1 = stack object (== CharacterComponent+0x48).
// NOTE: in the disasm the precheck here is CALL [vt+0x20] with only RCX=node set (no
// stack/time forwarded), so this precheck takes a single arg. Match exactly.
// ===========================================================================
extern "C" void CitizenAI_ActionFlush_c4bb0(int64_t param_1) {
    EnsureInit();
    int64_t* node = *reinterpret_cast<int64_t**>(param_1 + 0x10);
    if (node != nullptr) {
        do {
            *reinterpret_cast<uint8_t*>(node + 3) = 1;          // node+0x18 = 1
            // precheck +0x20 with RCX=node only (single-arg form, per disasm)
            void* vt = *reinterpret_cast<void**>(node);
            int iVar1 = reinterpret_cast<int(*)(void*)>(
                *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(vt) + 0x20))(node);
            if (iVar1 != 1) break;
            node = reinterpret_cast<int64_t*>(node[2]);          // advance via node+0x10
        } while (node != nullptr);
        if (fn_InternalBasePtr_DeletePtr)
            reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(param_1 + 0x10))->DeletePtr(
                );   // B.3.3 bridge
        // re-Ctor a default InternalBasePtr (null internal) and store into [stack+0x10]
        node = nullptr;
        if (fn_InternalBasePtr_Ctor) new (&node) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<int64_t*>(param_1 + 0x10) = reinterpret_cast<int64_t>(node);
    }
}

// ===========================================================================
// FUN_1400c4c50 — the action-stack executor. do/while over the stack top index.
// param_1 = stack object (== CharacterComponent+0x48); param_2 = Time* (forwarded to main).
// ===========================================================================
extern "C" void CitizenAI_ActionExecutor_c4c50(int64_t* param_1, void* param_2) {
    EnsureInit();
    bool bVar2 = false;            // R14: "stay" sticky flag → break the outer loop
    do {
        int iVar1 = static_cast<int>(param_1[3]);                       // top index (stack+0x18)
        int64_t* self = reinterpret_cast<int64_t*>(*param_1 + static_cast<int64_t>(iVar1) * 8);
        if (*reinterpret_cast<int64_t*>(*param_1 + static_cast<int64_t>(iVar1) * 8) == 0) {
            *reinterpret_cast<int*>(param_1 + 3) = iVar1 - 1;           // empty slot → pop
        } else {
            int iVar3 = 1;
            if (-1 < iVar1) {
                int64_t* local_res8;
                if (*reinterpret_cast<char*>(*self + 0x18) == '\0') {
                    // PRECHECK phase: walk the node list, set +0x18=1, call precheck(node,stack).
                    for (local_res8 = *reinterpret_cast<int64_t**>(
                             *param_1 + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8);
                         local_res8 != nullptr;
                         local_res8 = reinterpret_cast<int64_t*>(local_res8[2])) {
                        *reinterpret_cast<uint8_t*>(local_res8 + 3) = 1;   // node+0x18 = 1
                        iVar3 = CallPre(local_res8, param_1);
                        if (iVar3 != 1) goto LAB_d17;
                    }
                    iVar3 = 1;
                }
LAB_d17:
                if (-1 < static_cast<int>(param_1[3])) {
                    if (iVar3 == 1) {
                        // MAIN phase: walk the node list, call main(node,stack,time).
                        for (local_res8 = *reinterpret_cast<int64_t**>(
                                 *param_1 + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8);
                             local_res8 != nullptr;
                             local_res8 = reinterpret_cast<int64_t*>(local_res8[2])) {
                            iVar3 = CallMain(local_res8, param_1, param_2);
                            if (iVar3 != 1) goto LAB_d85;
                        }
                        iVar3 = 1;
                        bVar2 = true;
                    }
LAB_d85:
                    if ((-1 < static_cast<int>(param_1[3])) && (iVar3 == 0)) {
                        // POP phase: walk the node list, for entered nodes clear +0x18 and
                        // call exit(node,stack); then DeletePtr the slot, re-Ctor, dec top.
                        for (local_res8 = *reinterpret_cast<int64_t**>(
                                 *param_1 + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8);
                             local_res8 != nullptr;
                             local_res8 = reinterpret_cast<int64_t*>(local_res8[2])) {
                            if (*reinterpret_cast<char*>(local_res8 + 3) != '\0') {  // node+0x18
                                *reinterpret_cast<uint8_t*>(local_res8 + 3) = 0;
                                CallExit(local_res8, param_1);
                            }
                        }
                        if (fn_InternalBasePtr_DeletePtr)
                            reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>( *param_1 + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8))->DeletePtr(
                                );   // B.3.3 bridge
                        int64_t local_res18 = 0;
                        if (fn_InternalBasePtr_Ctor) new (&local_res18) ::Object::InternalBasePtr(
                            nullptr);   // B.3.3 bridge
                        *reinterpret_cast<int64_t*>(
                            *param_1 + static_cast<int64_t>(static_cast<int>(param_1[3])) * 8) = local_res18;
                        *reinterpret_cast<int*>(param_1 + 3) = static_cast<int>(param_1[3]) - 1;
                        goto LAB_e38;
                    }
                }
                if (iVar3 == 2) {
                    if (s_fn_taskCleanup) s_fn_taskCleanup(param_1);
                }
            }
LAB_e38:
            if (bVar2) break;
        }
    } while (-1 < static_cast<int>(param_1[3]));
    if (static_cast<int>(param_1[3]) < 0) {
        CitizenAI_ActionFlush_c4bb0(reinterpret_cast<int64_t>(param_1));
    }
}

// ===========================================================================
// 0x1401094c0  CharacterComponent move-tick (Ghidra: "CharacterComponent_OnAttach").
// HEAD (exercised in standalone): run the executor if the stack is non-empty, then
// early-return when the char render-index (self+0x40) is < 0 — the crash-safety gate.
// TAIL (DORMANT in standalone, char-index < 0): the ModelComponent transform/animation
// update. Ported faithfully but every engine dispatch-slot / exe sub-callee is resolved
// lazily + null-guarded. The tail touches the inert rendering layer and must NOT run
// until ModelComponent is brought live. See report for the full sub-callee table.
// ===========================================================================
namespace {

// --- exe .rdata read by the tail ---
static const uint8_t* s_pRootOnlyUpdates = nullptr;   // _rootOnlyUpdates_exref @ 0x1401bc2e8 (single byte)
static const float* s_pDAT_172220 = nullptr;
static const float* s_pDAT_172238 = nullptr;
static const float* s_pDAT_1720e4 = nullptr;
static const float* s_pDAT_1720cc = nullptr;
static const float* s_pDAT_172114 = nullptr;
static const float* s_pDAT_17214c = nullptr;
static const float* s_pDAT_1722a0 = nullptr;
static const uint64_t* s_pDAT_1722b0 = nullptr;   // double abs mask
static const uint32_t* s_pDAT_1722c0 = nullptr;   // float sign-flip mask (XOR)
static const float* s_pUVScale = nullptr;         // g_fUVScale (0x140172148 region — read via 0x14017214c sibling)

// --- exe sub-callees used by the tail (some registered, some NOT) ---
typedef float  (*fn_f1_t)(float);                          // 131090 / 131c90 / 132010
typedef float* (*fn_gridToWorld_t)(void* game, float* out3, float* inXY2, char flag, void* outTri); // 9f3e0 (REG)
typedef void   (*fn_sub8660_t)(float* arr, int idx);       // 018660
static fn_f1_t          s_fn_131090 = nullptr;   // CharacterComponent_helper_1090 (NOT REG)
static fn_f1_t          s_fn_131c90 = nullptr;   // FUN_140131c90 sin-ish           (NOT REG)
static fn_f1_t          s_fn_132010 = nullptr;   // WorkImpl_Sqrtf_132010           (REG)
static fn_gridToWorld_t s_fn_9f3e0  = nullptr;   // GridToWorld / impl_f3e0         (REG)
static fn_sub8660_t     s_fn_18660  = nullptr;   // CharacterComponent_sub_8660     (NOT REG)

// --- engine dispatch slots used by the tail (all rendering-layer) ---
// Value-return ABIs (CLAUDE.md gotcha): MeshInstance::RootAnimationOffset returns
// Vector3 by value (RCX=this, RDX=hidden retbuf); Vector3::Dot returns Scalar by value
// (RCX=this, RDX=retbuf, R8=other); Scalar::Max is a const member returning Scalar by
// value (RCX=this, RDX=retbuf, R8=other). Verified vs disasm at 0x14010967b/0x14010968b/
// 0x140109802.
typedef void  (*fn_vec3Dot_t)(void* thisVec, void* retScalar, void* other); // [0x140151298]
typedef void* (*fn_vec3Ctor_t)(void* self, float x, float y, float z); // [0x140152a30]
typedef void  (*fn_transformAssign_t)(void* dst, void* src);          // [0x140151280]
typedef void  (*fn_transformCtor_t)(void* self, void* src);          // [0x140151418]
typedef void* (*fn_scalarMax_t)(void* thisScalar, void* retScalar, void* other); // [0x140151e78]

// PERF (2026-09-04): bit-identical inline copies of the trivial Runtime math exports
// the hot paths called through dispatch slots (a DLL call per 3-float store). Bodies
// transcribed from the Runtime decompiles — note Dot's operand ORDER (y, x, z), kept
// so the float result is the same to the bit. Non-trivial helpers (Normalize, Heading,
// RootAnimationOffset) still go through the Runtime.
//   ??0Vector3@Math@@QEAA@MMM@Z (0x1800152d0)          -> InlV3(out,x,y,z)
//   ??0Vector3@Math@@QEAA@AEBVScalar@1@@Z (0x180015360) -> InlV3Splat(out,&s)
//   ?Dot@Vector3@Math@@ (0x18001dff0)                   -> InlV3Dot(a,&ret,b)
//   ?Dot@Vector2@Math@@ (0x18006c840)                   -> InlV2Dot(a,&ret,b)
//   ?Intersect@Box@Volume@@ (0x180012160)               -> InlBoxIsect(a,b)
//   ?Max@Scalar@Math@@ (0x18005ff60)                    -> InlScalarMax(&a,&ret,&b)
static inline void InlV3(void* out, float x, float y, float z) {
    float* o = static_cast<float*>(out); o[0] = x; o[1] = y; o[2] = z;
}
static inline void InlV3Splat(void* out, const float* s) {
    float* o = static_cast<float*>(out); o[0] = *s; o[1] = *s; o[2] = *s;
}
static inline void* InlV3Dot(const void* a, void* ret, const void* b) {
    const float* p = static_cast<const float*>(a); const float* q = static_cast<const float*>(b);
    *static_cast<float*>(ret) = q[1] * p[1] + q[0] * p[0] + q[2] * p[2];
    return ret;
}
static inline void* InlV2Dot(const void* a, void* ret, const void* b) {
    const float* p = static_cast<const float*>(a); const float* q = static_cast<const float*>(b);
    *static_cast<float*>(ret) = q[1] * p[1] + q[0] * p[0];
    return ret;
}
static inline bool InlBoxIsect(const void* a, const void* b) {
    const float* p = static_cast<const float*>(a); const float* q = static_cast<const float*>(b);
    return p[0] <= q[3] && q[0] <= p[3] && p[1] <= q[4] && q[1] <= p[4] && p[2] <= q[5] && q[2] <= p[5];
}
static inline void* InlScalarMax(const void* a, void* ret, const void* b) {
    const float x = *static_cast<const float*>(a), y = *static_cast<const float*>(b);
    *static_cast<float*>(ret) = (y < x) ? x : y;
    return ret;
}

static fn_vec3Dot_t         s_slot_dot      = nullptr;
static fn_vec3Ctor_t        s_slot_vec3     = nullptr;
static fn_transformAssign_t s_slot_xfAssign = nullptr;
static fn_transformCtor_t   s_slot_xfCtor   = nullptr;
// PerfX's deferred pick push (perf-only, PICK_CULL) replays through this: the
// member Picking::Scene::SetTransform, which the exe slot 0x1401529a8 used to
// be resolved for (the slot's registered target is the same member's door).
static void PickSetXfMember(void* scene, uint32_t id, void* xform) {
    reinterpret_cast<Picking::Scene*>(scene)->SetTransform(id, *static_cast<const Math::Transform*>(xform));   // B.3.7 bridge
}
static fn_scalarMax_t       s_slot_max      = nullptr;

// Picking::Scene::AddMesh (Runtime 0x1801252e0) registers a pick mesh and returns its
// id (stored at PickingComponent+0x28); ABI from Entity_impl_8f50 (exe 0x140128f50):
// (scene, meshHandle, &transform, ownerIBP, 0, 0, 0). ENDGAME B.3.7: called as a
// member (RegisterCitizenPickMesh); its file-local cache went with the last reader.
typedef bool     (*fn_isDerived_t)(void* obj, uint32_t typeId);   // [0x140152a98]
static fn_isDerived_t   s_slot_isDerived = nullptr;
static uint32_t         s_tidPickingDesc = 0;

static void* ResolveSlot(uintptr_t slotAddr) {
    uintptr_t s = ResolveAddr(Module::Exe, slotAddr);
    return s ? *reinterpret_cast<void**>(s) : nullptr;
}

static bool s_tailInit = false;
static void EnsureTailInit() {
    if (s_tailInit) return;
    // Retail reads the dispatch slot 0x140151f30 (-> Runtime MeshInstance::_rootOnlyUpdates);
    // 0x1401bc2e8 was a mis-read data address with no xrefs, so this was always null (=0).
    {
        const uint8_t** slot = reinterpret_cast<const uint8_t**>(ResolveAddr(Module::Exe, 0x140151f30));
        s_pRootOnlyUpdates = slot ? *slot : nullptr;
        if (!s_pRootOnlyUpdates)
            s_pRootOnlyUpdates = reinterpret_cast<const uint8_t*>(
                ResolveExport("?_rootOnlyUpdates@MeshInstance@Graphics@@0_NA"));
    }
    s_pDAT_172220 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172220));
    s_pDAT_172238 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172238));
    s_pDAT_1720e4 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401720e4));
    s_pDAT_1720cc = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401720cc));
    s_pDAT_172114 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_pDAT_17214c = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pDAT_1722a0 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401722a0));
    s_pDAT_1722b0 = reinterpret_cast<const uint64_t*>(ResolveAddr(Module::Exe, 0x1401722b0));
    s_pDAT_1722c0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401722c0));
    s_pUVScale    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172148));
    s_fn_131090 = reinterpret_cast<fn_f1_t>(ResolveAddr(Module::Exe, 0x140131090));
    s_fn_131c90 = reinterpret_cast<fn_f1_t>(ResolveAddr(Module::Exe, 0x140131c90));
    s_fn_132010 = reinterpret_cast<fn_f1_t>(ResolveAddr(Module::Exe, 0x140132010));
    s_fn_9f3e0  = reinterpret_cast<fn_gridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_18660  = reinterpret_cast<fn_sub8660_t>(ResolveAddr(Module::Exe, 0x140018660));
    s_slot_dot      = reinterpret_cast<fn_vec3Dot_t>(ResolveSlot(0x140151298));
    s_slot_vec3     = reinterpret_cast<fn_vec3Ctor_t>(ResolveSlot(0x140152a30));
    s_slot_xfAssign = reinterpret_cast<fn_transformAssign_t>(ResolveSlot(0x140151280));
    s_slot_xfCtor   = reinterpret_cast<fn_transformCtor_t>(ResolveSlot(0x140151418));
    s_slot_max      = reinterpret_cast<fn_scalarMax_t>(ResolveSlot(0x140151e78));
    // Picking-registration closure (port of Entity_impl_8f50 = PickingComponent OnAdded).
    s_slot_isDerived = reinterpret_cast<fn_isDerived_t>(ResolveSlot(0x140152a98));
    s_tidPickingDesc = ::Object::GetTypeIdByName("PickingDescription");
    s_tailInit = true;
}

// Register the citizen's mesh in the Picking::Scene so it becomes clickable.
// Faithful port of Entity_impl_8f50 (PickingComponent::OnAdded, exe 0x140128f50),
// specialised to the move-tick (we already hold the sibling ModelComponent at
// self+0x68 and the PickingComponent at self+0x70 — no Entity::FindComponent walk
// needed). Runs once per citizen when its pick-id (PickingComponent+0x28) is still
// unregistered (<0). Returns the new pick-id, or -1 if it could not register
// (description not a PickingDescription / no pick-mesh table — standalone-safe).
// Emit a one-shot reason string explaining why RegisterCitizenPickMesh bailed, so a
// live run pinpoints the remaining gap without a debugger. Cheap (first 8 only).
static void PickRegFail(const char* why) {
    if (!MoveTickTraceOn()) return;
    static int n = 0;
    if (n < 8) { ++n; fprintf(stderr, "[PickReg] FAIL: %s\n", why); fflush(stderr); }
}

static int RegisterCitizenPickMesh(uint8_t* self) {
    int64_t model    = *reinterpret_cast<int64_t*>(self + 0x68);   // ModelComponent
    int64_t pickComp = *reinterpret_cast<int64_t*>(self + 0x70);   // PickingComponent
    if (model == 0 || pickComp == 0) { PickRegFail("model/pickComp null"); return -1; }

    int16_t variantIdx   = *reinterpret_cast<int16_t*>(model + 0x164);  // sVar1
    int16_t modelVariant = *reinterpret_cast<int16_t*>(model + 0x166);  // sVar2
    if (modelVariant < 0) { PickRegFail("modelVariant<0"); return -1; } // (-1 < sVar2) gate

    // The pick mesh comes from the PickingComponent's description (+0x18), which must
    // be a PickingDescription. Its pick-mesh table is at desc+0x20, indexed exactly
    // like the render mesh: *(*(desc+0x20) + variantIdx*0x10) + modelVariant*8.
    void* desc = *reinterpret_cast<void**>(pickComp + 0x18);
    if (desc == nullptr || s_slot_isDerived == nullptr || s_tidPickingDesc == 0) {
        PickRegFail("desc null / isDerived null / tidPickingDesc 0"); return -1;
    }
    if (!s_slot_isDerived(desc, s_tidPickingDesc)) { PickRegFail("desc !PickingDescription"); return -1; }
    int64_t meshTable = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(desc) + 0x20);
    if (meshTable == 0) { PickRegFail("meshTable(desc+0x20) null"); return -1; }
    int64_t group = *reinterpret_cast<int64_t*>(meshTable + static_cast<int64_t>(variantIdx) * 0x10);
    if (group == 0) { PickRegFail("mesh group null"); return -1; }
    void* meshHandle = *reinterpret_cast<void**>(group + static_cast<int64_t>(modelVariant) * 8);
    if (meshHandle == nullptr) {
        PickRegFail("meshHandle null"); return -1;
    }

    // Transform snapshot (0x1C bytes, model+0x148..+0x163) — the move-tick wrote it
    // this frame; the original copies it into a stack Transform passed by pointer.
    uint8_t xform[0x1C];
    std::memcpy(xform, reinterpret_cast<void*>(model + 0x148), sizeof(xform));

    void* scene = *reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x70);
    void* owner = *reinterpret_cast<void**>(pickComp + 0x30);   // entity IBP value
    uint32_t id = reinterpret_cast<Picking::Scene*>(scene)->AddMesh(   // B.3.7 bridge
        Object::ExternalPtr<Picking::PickingMesh>(reinterpret_cast<Picking::PickingMesh*>(meshHandle)),
        *reinterpret_cast<const Math::Transform*>(xform), owner, 0, 0, nullptr);
    *reinterpret_cast<uint32_t*>(pickComp + 0x28) = id;
    return static_cast<int>(id);
}

static inline float fabs_f(float v) {
    // matches the disasm's CVTPS2PD; ANDPD abs-mask; CVTPD2PS — plain magnitude.
    return v < 0.0f ? -v : v;
}

} // namespace

extern "C" void CharacterComponent_SinCosf(float x, float* s, float* c);   // character_trig.cpp

extern "C" void CharacterComponent_MoveTick_1094c0(uint8_t* self, void* time) {
    EnsureInit();
    // ---- HEAD (exercised) -------------------------------------------------
    // if (-1 < *(int*)(self+0x60)) run the executor on the stack at self+0x48.
    if (-1 < *reinterpret_cast<int*>(self + 0x60)) {
        CitizenAI_ActionExecutor_c4c50(reinterpret_cast<int64_t*>(self + 0x48), time);
    }
    // Crash-safety gate: char render-index < 0 (always in standalone) → skip the tail.
    if (*reinterpret_cast<int*>(self + 0x40) < 0) {
        return;
    }

    // ---- TAIL (DORMANT in standalone: only reached when char-index >= 0) --
    // Faithful transcription of the ModelComponent transform/animation update. Every
    // engine slot / exe sub-callee below is null-guarded; the block touches the inert
    // ModelComponent(+0x68)/MeshInstance/Geometry/Picking rendering layer.
    EnsureTailInit();

    uint8_t rootOnly = s_pRootOnlyUpdates ? *s_pRootOnlyUpdates : 0;

    int64_t model = *reinterpret_cast<int64_t*>(self + 0x68);
    float fx = *reinterpret_cast<float*>(model + 0x158);
    float fy = *reinterpret_cast<float*>(model + 0x15c);
    float fz = *reinterpret_cast<float*>(model + 0x160);
    float pos[3] = { fx, fy, fz };
    const float mtEntryX = fx, mtEntryY = fy, mtEntryZ = fz;  // for movement trace

    float fVar14 = *reinterpret_cast<float*>(self + 0x3c);
    float fVar10 = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x68) + 0x168) -
                   *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x78) + 0x24);
    float fVar13 = fVar14 - fVar10;
    // Exe .rdata math constants. In STANDALONE mode ResolveAddr(Module::Exe, dataAddr)
    // returns null for these (the registry only maps reimplemented functions/vtables, not
    // raw .rdata floats), so the ternaries MUST fall back to the real values — a 0.0f
    // fallback collapses the angle normalization (180<=|d| becomes 0<=|d|, always true)
    // and the lerp (uvScale 1.0 -> 0.0), making the applied yaw diverge unboundedly.
    // Values read from the exe .rdata (verified): 0x140172220=180, 0x140172238=360,
    // 0x1401720e4=0.1 (turn epsilon), 0x1401722a0=-180, 0x14017214c=1.0 (g_fUVScale).
    float c172220 = s_pDAT_172220 ? *s_pDAT_172220 : 180.0f;
    float c172238 = s_pDAT_172238 ? *s_pDAT_172238 : 360.0f;
    float c1720e4 = s_pDAT_1720e4 ? *s_pDAT_1720e4 : 0.1f;
    float c1722a0 = s_pDAT_1722a0 ? *s_pDAT_1722a0 : -180.0f;
    // g_fUVScale is loaded from 0x14017214c (=1.0) per disasm at 0x14010961f — NOT the
    // 0x140172148 sibling (=0.99897) the old s_pUVScale pointed at.
    float uvScale = s_pDAT_17214c ? *s_pDAT_17214c : 1.0f;

    if (c172220 <= fabs_f(fVar13)) {
        if (fVar13 <= 0.0f) {
            fVar13 = fVar13 + c172238;
        } else {
            fVar13 = -(c172238 - fVar13);   // XOR sign-flip
        }
    }
    if (c1720e4 < fabs_f(fVar13)) {
        float t = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x78) + 0x28);
        fVar14 = (fVar13 + fVar10) * (uvScale - t) + t * fVar10;
        if (fVar14 <= c172220) {
            if (fVar14 < c1722a0) fVar14 = fVar14 + c172238;
        } else {
            fVar14 = fVar14 - c172238;
        }
    }

    // this = MeshInstance::RootAnimationOffset(model+0x28) (Vector3 by-value into retbuf);
    // speed = sqrt(this.Dot(this)) * self+0x38 — magnitude of the root-motion offset.
    float rootAnimVec[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    if (model)
        // Retail retslot door; its body moves to the call site as exactly one
        // construction into the SAME storage the door wrote (the 4th float stays 0).
        ::new (static_cast<void*>(rootAnimVec)) Math::Vector3(
            reinterpret_cast<const Graphics::MeshInstance*>(
                *reinterpret_cast<int64_t*>(self + 0x68) + 0x28)
                ->RootAnimationOffset());   // B.3.5 bridge
    float dotOut = 0.0f;
    InlV3Dot(rootAnimVec, &dotOut, rootAnimVec);   // was s_slot_dot (Runtime call)
    // PERF (2026-09-05): 0x140132010 is our own WorkImpl_Sqrtf_132010 (= sqrtf); the
    // builtin compiles to SQRTSS, which is correctly rounded, so this is bit-identical
    // and saves an indirect call per character per tick (1.4% of the phase).
    float speed = __builtin_sqrtf(dotOut);
    speed = speed * *reinterpret_cast<float*>(self + 0x38);
    if (0.0f < speed) {
        float ax = speed * *reinterpret_cast<float*>(self + 0x2c);
        float ay = speed * *reinterpret_cast<float*>(self + 0x30);
        float az = speed * *reinterpret_cast<float*>(self + 0x34);
        // two Vector3 ctors in the original (locals), result folded into pos.
        pos[0] = fx + ax; pos[1] = fy + ay; pos[2] = fz + az;
        fx = pos[0]; fy = pos[1];
    }

    int* gameWorld = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    bool blockedHandled = false;
    if (rootOnly == 0) {
        int dim = *gameWorld;
        int ix = static_cast<int>(fx + static_cast<float>(dim >> 1));
        int iy = static_cast<int>(fy + static_cast<float>(dim >> 1));
        if (ix < 0) ix = 0;
        if (dim <= ix) ix = dim - 1;
        if (iy < 0) iy = 0;
        if (dim <= iy) iy = dim - 1;
        int64_t cellBase = *reinterpret_cast<int64_t*>(gameWorld + 0x20);
        // [PerfX] TERRAIN_SIDECAR: cells[dim*iy+ix].type via the mirror (perf_terrain.h).
        if (PerfX::TerrainGet(reinterpret_cast<const uint8_t*>(cellBase), gameWorld[0x22],
                              static_cast<int>(static_cast<int64_t>(dim) * iy + ix)) == 0x0b) {
            pos[2] = 0.0f; pos[0] = fx; pos[1] = fy;
            blockedHandled = true;     // goto LAB_14010981e
        }
    }

    if (!blockedHandled) {
        if (*reinterpret_cast<char*>(*reinterpret_cast<int64_t*>(self + 0x78) + 0x3c) == '\0') {
            if (rootOnly == 0) {
                float triOut[4] = { 0, 0, 0, 0 };
                float* pf = nullptr;
                if (s_fn_9f3e0) pf = s_fn_9f3e0(gameWorld, triOut, pos, '\0', nullptr);
                // pos.z = Scalar(gridZ).Max(desc+0x44)  — clamp terrain height to the
                // description's min. Const-member by-value ABI: (this=gridZ, retbuf, other).
                float maxThis  = pf ? pf[2] : 0.0f;
                float maxOther = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x78) + 0x44);
                float maxRet   = maxThis;
                if (pf) { fx = pf[0]; fy = pf[1]; }
                InlScalarMax(&maxThis, &maxRet, &maxOther);   // was s_slot_max (Runtime call)
                pos[2] = maxRet;
                pos[0] = fx; pos[1] = fy;
            }
        } else {
            pos[2] = *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x78) + 0x40);
            pos[0] = fx; pos[1] = fy;
        }
    }

    // LAB_14010981e:
    fVar14 = fVar14 + *reinterpret_cast<float*>(*reinterpret_cast<int64_t*>(self + 0x78) + 0x24);
    // deg->half-angle: 0x1401720cc=0.0174533 (pi/180) * 0x140172114=0.5 (half angle for
    // the (-sin,cos) heading pair). Real-value fallbacks for standalone (see above).
    float c1720cc = s_pDAT_1720cc ? *s_pDAT_1720cc : 0.017453292f;
    float c172114 = s_pDAT_172114 ? *s_pDAT_172114 : 0.5f;
    float ang = fVar14 * c1720cc * c172114;
    // Retail calls sinf (0x140131c90) then cosf (0x140131090) on the same angle; both
    // are ours (character_trig.cpp), so take the pair from one reduction. The registry
    // slots stay resolved for any other caller.
    float s_ang = 0.0f, c_ang = 0.0f;
    CharacterComponent_SinCosf(ang, &s_ang, &c_ang);
    float negSin = -s_ang;

    // Build the source Transform (0x1C bytes). Layout VERIFIED vs disasm at
    // 0x140109856-0x14010987c (source built at RSP+0x68, copied into model+0x148):
    //   +0x00=0  +0x04=0  +0x08=negSin  +0x0c=cos  +0x10=pos.x  +0x14=pos.y  +0x18=pos.z
    // The first 8 bytes are a zeroed 2D translation; +0x08/+0x0c are the heading
    // (-sin, cos) pair; +0x10.. is the 3D world position. (Earlier port had pos.xy
    // and the sin/cos pair SWAPPED — corrupts the citizen render transform. The
    // original locals are, in ascending address: local_d0=0, local_cc=0,
    // local_c8=negSin, local_c4=cos, local_c0=CONCAT44(pos.y,pos.x), local_b8=pos.z.)
    float xform[8];
    xform[0] = 0.0f;   xform[1] = 0.0f;
    xform[2] = negSin; xform[3] = c_ang;
    xform[4] = pos[0]; xform[5] = pos[1];
    xform[6] = pos[2]; xform[7] = 0.0f;

    model = *reinterpret_cast<int64_t*>(self + 0x68);
    if (s_slot_xfAssign) s_slot_xfAssign(reinterpret_cast<void*>(model + 0x148), xform);
    *reinterpret_cast<float*>(model + 0x168) = fVar14;
    if (s_fn_132010) { /* unused result guard symmetry */ }
    // [PerfX GEOMXF_LASTONLY -- NON-RETAIL] retail pushes the transform into the
    // Geometry (bounds, attached children, scene quadtree) on EVERY speed iteration; only
    // the last one is ever drawn, and the render-side interpolation is snapped from that
    // same last update. Intermediate iterations skip the push. model+0x148 (the gameplay
    // position, read by StatusIcon/Particle/pick code) is still written every tick above.
    // [PerfX GEOM_CULL -- NON-RETAIL] on the last iteration an OFF-SCREEN citizen also
    // skips the push: nothing draws it, the frustum margin (6 units, ~12 frames of pan)
    // updates it before it can enter the view, and PerfX pushes model+0x148 for every
    // culled model when the sim pauses.
    bool pushGeom = true;   // was a Geometry::SetTransform door-readiness test
    if (pushGeom) {
        if (rootOnly != 0) {
            if (PerfX::GeomXfLastOnly()) { pushGeom = false; PerfX::Count(PerfX::kCntGeomXfSkipped); }
        } else if (PerfX::GeomCull() && PerfX::ModelCulled(reinterpret_cast<void*>(model))) {
            pushGeom = false; PerfX::Count(PerfX::kCntGeomCulled);
        }
    }
    if (pushGeom)
        reinterpret_cast<Graphics::Geometry*>(model + 0x28)
            ->SetTransform(*reinterpret_cast<const Math::Transform*>(model + 0x148));   // B.3.5 bridge

    // ---- movement trace (proves the transform write moves the mesh) --------
    if (MoveTickTraceOn()) {
        static void* s_tracked[4] = { nullptr, nullptr, nullptr, nullptr };
        static int   s_nTracked = 0;
        static long  s_calls = 0;
        int slot = -1;
        for (int i = 0; i < s_nTracked; ++i)
            if (s_tracked[i] == self) { slot = i; break; }
        if (slot < 0 && s_nTracked < 4) { slot = s_nTracked; s_tracked[s_nTracked++] = self; }
        if (slot >= 0) {
            float dx = pos[0] - mtEntryX, dy = pos[1] - mtEntryY, dz = pos[2] - mtEntryZ;
            bool moved = (dx*dx + dy*dy + dz*dz) > 1e-10f;
            // Facing diagnostic (TODO #3-B): desiredFacing = self+0x3c (set by the
            // MoveTo action's UpdateFacing toward the next waypoint); appliedYaw =
            // model+0x168 + heading pair just written into model+0x148. If the
            // citizen moves but appliedYaw never changes, the facing pipeline is
            // not turning the mesh; if desiredFacing changes but appliedYaw lags,
            // it is the smoothing (desc+0x28). model is the ModelComponent (self+0x68).
            float desiredFacing = *reinterpret_cast<float*>(self + 0x3c);
            float appliedYaw    = model ? *reinterpret_cast<float*>(model + 0x168) : 0.0f;
            // Log every move, plus a heartbeat every 240 calls even when static.
            if (moved || (s_calls % 240) == 0) {
                fprintf(stderr,
                    "[MoveTick] cit#%d self=%p chIdx=%d pos=(%.4f,%.4f,%.4f) "
                    "delta=(%.5f,%.5f,%.5f) speed=%.4f vel38=%.4f stackTop=%d "
                    "facing(want=%.3f applied=%.3f)\n",
                    slot, (void*)self, *reinterpret_cast<int*>(self + 0x40),
                    pos[0], pos[1], pos[2], dx, dy, dz, speed,
                    *reinterpret_cast<float*>(self + 0x38),
                    *reinterpret_cast<int*>(self + 0x60),
                    desiredFacing, appliedYaw);
                fflush(stderr);
            }
        }
        ++s_calls;
    }

    if ((*reinterpret_cast<int64_t*>(self + 0x70) != 0) && (rootOnly == 0)) {
        uint8_t localXform[136];
        if (s_slot_xfCtor)
            s_slot_xfCtor(localXform, reinterpret_cast<void*>(*reinterpret_cast<int64_t*>(self + 0x68) + 0x148));
        uint32_t pickId = *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(self + 0x70) + 0x28);
        // First time through (pick-id still unregistered) register the citizen mesh in
        // the Picking::Scene so it becomes clickable.
        if (static_cast<int>(pickId) < 0) {
            int newId = RegisterCitizenPickMesh(self);
            pickId = static_cast<uint32_t>(newId);
            if (MoveTickTraceOn()) {
                static int s_pickLog = 0;
                if (s_pickLog < 8) { ++s_pickLog;
                    fprintf(stderr, "[PickReg] self=%p pickId=%d\n", (void*)self, newId);
                    fflush(stderr);
                }
            }
        }
        if (-1 < static_cast<int>(pickId)) {
            void* pickScene = *reinterpret_cast<void**>(
                reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x70);
            // [PerfX PICK_CULL -- NON-RETAIL] an off-screen citizen cannot be clicked, so
            // its pick-scene transform is deferred; PerfX replays it if the sim pauses.
            if (!PerfX::PickCullSkip(reinterpret_cast<void*>(model), pickScene, pickId,
                                     reinterpret_cast<PerfX::PickXfCtor_t>(s_slot_xfCtor),
                                     &PickSetXfMember)) {
                reinterpret_cast<Picking::Scene*>(pickScene)->SetTransform(pickId, *reinterpret_cast<const Math::Transform*>(localXform));   // B.3.7 bridge
            }
        }
    }

    if (s_fn_18660) {
        s_fn_18660(*reinterpret_cast<float**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x88),
                   *reinterpret_cast<int*>(self + 0x40));
    }
}

