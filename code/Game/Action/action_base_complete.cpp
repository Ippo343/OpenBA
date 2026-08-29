// code/Game/Action/action_base_complete.cpp
// Wave 3 (Action) — CompleteTask / ShowTool exe sub-helpers (closure-safe subset).
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from the
// CompleteTask / ShowTool action .cpp files. They resolve to 0 (silently skipped)
// unless registered.
// This file provides faithful 1:1 reimplementations and a single registration point
// (RegisterActionCompleteBatch in standalone/reg_action_complete.cpp) so the shared
// call sites fire real logic in the standalone exe.
//
// Wired (closure-safe): CompleteTask BST helpers 0x1400f8760 / 0x1400f8800 / 0x1400f8e30
// and ShowTool 0x1401173b0 / 0x1401176e0. Each is a 1:1 port of the decompiled exe
// helper (Ghidra); do NOT "improve" behavior.
//
// DEFERRED (NOT in this file): DropOffRawMaterial transfer helpers 0x1400c7570 /
// 0x1400c7620 — their closure pulls in unregistered code sub-callees (the deep
// create-raw-material spawn FUN_1400c78c0, MoveTo action ctor FUN_1400c9170, the
// task-list append FUN_1400c54e0, and an allocator dispatch slot). Drafted in
// notes/wave3_drafts/action_base_complete.cpp.draft; wire once that closure lands.
//
// REFACTOR NOTE (this commit): the original draft re-resolved the InternalBasePtr /
// IsDerivedFrom dispatch slots through an ad-hoc local "ResolveIATSlot" helper. Those
// four ops already exist as engine_funcs fn_ pointers (resolved by mangled-name export
// at startup, so they work whether or not the .rdata slot is registered) — we use those
// directly. Genuine graphics dispatch slots (SetMesh / Vector4 ctor / Time ctor /
// Scene::AddObject) are read via the project-standard ReadDispatchSlot (ResolveAddr the
// .rdata slot, deref one level), exactly like action_base_move.cpp; an unregistered slot
// resolves to null and the guarded graphics op is safely skipped.
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "scene_watch.h"
#include <windows.h>

static bool ACT_Trace() {
    static int t = -1;
    if (t < 0) {
        const char* e = std::getenv("BANISHED_BUILD_TRACE");
        t = (e && e[0] && std::strcmp(e, "0") != 0) ? 1 : 0;
    }
    return t == 1;
}

namespace {

// Read a .rdata dispatch slot: ResolveAddr gives us the slot storage; deref one level
// to obtain the runtime function pointer the original prologue would have called.
// Returns null if the slot is unregistered (standalone) — callers guard every use.
static void* ReadDispatchSlot(uintptr_t slotAddr) {
    uintptr_t slot = ResolveAddr(Module::Exe, slotAddr);
    return slot ? *reinterpret_cast<void**>(slot) : nullptr;
}

// Thin guarded wrapper around the engine_funcs InternalBasePtr::CheckPtr pointer
// (resolved via ?CheckPtr@InternalBasePtr@Object@@QEAAXXZ at startup).
inline void CheckPtr(void* p) {
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(p);
}

} // namespace

// ============================================================
// CompleteTask task-manager BST helpers
// ============================================================
// The task manager (gameState+0x118) holds an intrusive binary search tree of task
// nodes rooted at taskMgr+0x30. Node layout (verified in disassembly):
//   +0x00 int   key (task index)
//   +0x08 IBP   task-owner object (CheckPtr'd before use)
//   +0x18 ptr   left child
//   +0x20 ptr   right child
// Each helper walks the tree to the node whose key == taskIdx, then invokes a vtable
// method on the owner object with copies of the two ref args.
//
// Caller copies (decompiled):
//   entityCopy = *param_2  (entityRef ptr, passed as RDX = &copy)
//   targetCopy = *param_4  (targetRef ptr, passed as R8  = &copy)
//   vtable(owner)(owner, &entityCopy, &targetCopy, taskParam)

namespace {
// Walk the taskMgr+0x30 BST; return the node whose key == taskIdx, else null.
static void* FindTaskNode(uintptr_t taskMgr, int taskIdx) {
    int* node = *reinterpret_cast<int**>(taskMgr + 0x30);
    while (node != nullptr) {
        int key = *node;
        if (taskIdx < key) {
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x18);  // left
        } else if (key < taskIdx) {
            node = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(node) + 0x20);  // right
        } else {
            return node;  // exact match
        }
    }
    return nullptr;
}
} // namespace

// Reimpl of CompleteTask_helper_8760 — try-complete a task: walk to the matching node,
// CheckPtr the owner IBP, invoke owner vtable +0x30 with (entityCopy, targetCopy, param).
// Returns the vtable call result (bool-ish in low byte). Caller signature:
//   bool fn(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam)
bool ActCt_TryCompleteTask_8760(void* taskMgr, void* entityRef, int taskIdx,
                                void* targetRef, uint32_t taskParam) {
    void* node = FindTaskNode(reinterpret_cast<uintptr_t>(taskMgr), taskIdx);
    if (ACT_Trace())
        std::fprintf(stderr, "[BuildTrace] TryCompleteTask taskMgr=%p taskIdx=%d taskParam=0x%x -> node=%p\n",
                     taskMgr, taskIdx, (unsigned)taskParam, node);
    if (node == nullptr) return false;

    void* ibp = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(node) + 0x8);
    CheckPtr(ibp);
    void* targetCopy = *reinterpret_cast<void**>(targetRef);
    uintptr_t* owner = *reinterpret_cast<uintptr_t**>(ibp);
    void* entityCopy = *reinterpret_cast<void**>(entityRef);

    typedef uint64_t (*vfn_t)(void* self, void* a, void* b, uint32_t p);
    vfn_t vf = *reinterpret_cast<vfn_t*>(*owner + 0x30);
    uint64_t r = vf(owner, &entityCopy, &targetCopy, taskParam);
    if (ACT_Trace())
        std::fprintf(stderr, "[BuildTrace] TryCompleteTask -> owner=%p OnComplete returned 0x%llx\n",
                     (void*)owner, (unsigned long long)r);
    return (r & 0xff) != 0;
}

// Reimpl of CompleteTask_helper_8800 — task reassign: same BST walk, but invokes owner
// vtable +0x38 instead of +0x30 (void return). Caller signature:
//   void fn(void* taskMgr, void* entityRef, int taskIdx, void* targetRef, uint32_t taskParam)
void ActCt_TaskReassign_8800(void* taskMgr, void* entityRef, int taskIdx,
                             void* targetRef, uint32_t taskParam) {
    void* node = FindTaskNode(reinterpret_cast<uintptr_t>(taskMgr), taskIdx);
    if (node == nullptr) return;

    void* ibp = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(node) + 0x8);
    CheckPtr(ibp);
    void* targetCopy = *reinterpret_cast<void**>(targetRef);
    uintptr_t* owner = *reinterpret_cast<uintptr_t**>(ibp);
    void* entityCopy = *reinterpret_cast<void**>(entityRef);

    typedef void (*vfn_t)(void* self, void* a, void* b, uint32_t p);
    vfn_t vf = *reinterpret_cast<vfn_t*>(*owner + 0x38);
    vf(owner, &entityCopy, &targetCopy, taskParam);
}

// ============================================================
// CitizenAI_impl_8e30 — task cleanup after completion.
// Walks the task-manager BST (param_1+0x30) to the matching task node, then:
//  1. For each of the 14 action-list BSTs at param_1+0x40 (stride 0x10), find and
//     remove (TradeDescription_impl_ca60 = node remove/rebalance) the node whose key
//     == taskIdx.
//  2. Find/remove the matching node in the param_1+0x10 ORDER tree (key compared
//     against the task IBP's +0x24 field; this tree uses the 0x28-node layout:
//     parent@+0x08, less@+0x10, greater@+0x18 — NOT the 0x30-node layout the id map
//     and category buckets use; remove via CitizenAI_impl_a6e0). A long-lived offset
//     inversion here (fixed 2026-07-26) meant the order-tree node was never found, so
//     sched+0x10/+0x18 grew monotonically and re-posted work got ever-earlier queue
//     positions.
//  3. Call owner vtable +0x50 (returns a key ptr); look that key up in the param_1+0x120
//     BST (left@+0x20, right@+0x28; entry list at node+0x8) and remove the matching
//     entry via ca60.
//  4. Call owner vtable +0x48 then +0x40 (cancel/cleanup), DeletePtr the task IBP, then
//     remove the task node itself from the param_1+0x30 BST via ca60.
// Caller signature: void fn(void* taskMgr, int taskIdx)
//
// ca60 (0x14001ca60) / a6e0 (0x14001a6e0) are intrusive-BST removes already registered
// by the Work wave (reg_work_cellbst.cpp / reg_work_bst.cpp) — closure-safe.

namespace {
typedef void (*fn_BstRemove_t)(void* treeOwner, void* node);  // 0x14001ca60 / 0x14001a6e0
static fn_BstRemove_t s_ca60 = nullptr;  // TradeDescription_impl_ca60 (generic BST remove)
static fn_BstRemove_t s_a6e0 = nullptr;  // CitizenAI_impl_a6e0      (BST_Remove variant)
static bool s_8e30Init = false;
static void Ensure8e30() {
    if (s_8e30Init) return;
    s_ca60 = reinterpret_cast<fn_BstRemove_t>(ResolveAddr(Module::Exe, 0x14001ca60));
    s_a6e0 = reinterpret_cast<fn_BstRemove_t>(ResolveAddr(Module::Exe, 0x14001a6e0));
    s_8e30Init = true;
}
} // namespace

void ActCt_TaskCleanup_8e30(void* selfV, int taskIdx) {
    Ensure8e30();
    uintptr_t self = reinterpret_cast<uintptr_t>(selfV);

    // 1. Locate the task node in the param_1+0x30 BST.
    int* task = *reinterpret_cast<int**>(self + 0x30);
    while (task != nullptr) {
        int key = *task;
        if (taskIdx < key) {
            task = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(task) + 0x18);
            continue;
        }
        if (key < taskIdx) {
            task = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(task) + 0x20);
            continue;
        }
        break;  // key == taskIdx
    }
    if (task == nullptr) return;

    // 2. Remove the matching node (key == taskIdx) from each of the 14 action-list BSTs.
    uintptr_t listSlot = self + 0x40;
    for (int li = 0; li < 14; li++) {
        int* n = *reinterpret_cast<int**>(listSlot);
        while (n != nullptr) {
            int k = *n;
            if (taskIdx < k) {
                n = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(n) + 0x18);
            } else if (k < taskIdx) {
                n = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(n) + 0x20);
            } else {
                if (s_ca60) s_ca60(reinterpret_cast<void*>(listSlot), n);
                break;
            }
        }
        listSlot += 0x10;
    }

    // 3. Remove from the param_1+0x10 BST keyed on the task IBP's +0x24 field.
    CheckPtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(task) + 0x8));
    {
        uintptr_t taskObj = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(task) + 0x8);
        int wantKey = *reinterpret_cast<int*>(taskObj + 0x24);
        int* n = *reinterpret_cast<int**>(self + 0x10);
        while (n != nullptr) {
            int k = *n;
            // Retail 8e30 step: `want < nodeKey -> node+0x10`, `nodeKey < want ->
            // node+0x18` (0x28-node order tree: less@+0x10, greater@+0x18). These two
            // offsets were inverted here until 2026-07-26, which made this search
            // descend the wrong subtree and the erase below effectively never run.
            if (wantKey < k) {
                n = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(n) + 0x10);  // less
            } else if (k < wantKey) {
                n = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(n) + 0x18);  // greater
            } else {
                if (s_a6e0) s_a6e0(reinterpret_cast<void*>(self + 0x10), n);
                break;
            }
        }
    }

    // 4. owner vtable +0x50 → key ptr; look up in the param_1+0x120 BST and remove entry.
    CheckPtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(task) + 0x8));
    {
        uintptr_t* owner = *reinterpret_cast<uintptr_t**>(reinterpret_cast<uintptr_t>(task) + 0x8);
        uint64_t keyTmp = 0;
        typedef uint64_t* (*vfn50_t)(void* self, void* out);
        vfn50_t vf50 = *reinterpret_cast<vfn50_t*>(*owner + 0x50);
        uint64_t* keyPtr = vf50(owner, &keyTmp);

        // Walk the +0x120 BST (value@+0, left@+0x20, right@+0x28; entry list at node+0x8).
        uint64_t* node = *reinterpret_cast<uint64_t**>(self + 0x120);
        if (node != nullptr) {
            while (true) {
                uint64_t v = *node;
                if (*keyPtr < v) {
                    node = reinterpret_cast<uint64_t*>(node[4]);  // +0x20 left
                } else if (v < *keyPtr) {
                    node = reinterpret_cast<uint64_t*>(node[5]);  // +0x28 right
                } else {
                    break;
                }
                if (node == nullptr) break;
            }
        }

        if (node != nullptr) {
            // entry sub-BST rooted at node[1] (node+0x8); key compared against taskIdx.
            int* e = reinterpret_cast<int*>(node[1]);
            void* entryTreeOwner = reinterpret_cast<void*>(node + 1);  // node+0x8
            while (e != nullptr) {
                int k = *e;
                if (taskIdx < k) {
                    e = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(e) + 0x18);
                } else if (k < taskIdx) {
                    e = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(e) + 0x20);
                } else {
                    if (s_ca60) s_ca60(entryTreeOwner, e);
                    break;
                }
            }
        }
    }

    // 5. owner vtable +0x48 then +0x40 (cancel/cleanup); DeletePtr the task IBP; then
    //    remove the task node from the param_1+0x30 BST.
    CheckPtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(task) + 0x8));
    {
        uintptr_t* owner = *reinterpret_cast<uintptr_t**>(reinterpret_cast<uintptr_t>(task) + 0x8);
        (*reinterpret_cast<void(**)(void*)>(*owner + 0x48))(owner);
    }
    CheckPtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(task) + 0x8));
    {
        uintptr_t* owner = *reinterpret_cast<uintptr_t**>(reinterpret_cast<uintptr_t>(task) + 0x8);
        (*reinterpret_cast<void(**)(void*)>(*owner + 0x40))(owner);
    }
    if (fn_InternalBasePtr_DeletePtr)
        fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(task) + 0x8));
    if (s_ca60) s_ca60(reinterpret_cast<void*>(self + 0x30), task);
}

// ============================================================
// ShowTool helpers (Entity_impl_73b0 / FUN_1401176e0)
// ============================================================
// These attach/detach a "tool" mesh to a citizen entity. They call several Runtime
// graphics functions; each is mirrored via the same exe dispatch slot the original uses
// (ReadDispatchSlot — null-safe if unregistered) or via the named export. Sub-callees
// are NOT registered; an unresolved op is safely skipped (no tool mesh, no corruption).

namespace {
// Graphics dispatch slots used by 73b0 / 176e0.
typedef void  (*fn_SetMesh_t)(void* meshInstance, void* meshPtr);          // 0x140151498 (registered)
typedef void* (*fn_Vector4Ctor_t)(void* retVec4, const float* scalar);    // 0x140152970
typedef void  (*fn_TimeCtor_t)(void* retTime, float seconds);             // 0x140152968
typedef uint32_t (*fn_SceneAddObject_t)(void* scene, void* geometry);     // 0x140151370 (registered)

// Named Runtime graphics exports.
typedef void (*fn_GeometryAttach_t)(void* dstGeom, void* srcGeom, int nodeIdx);
typedef void (*fn_GeometryDetach_t)(void* dstGeom, void* srcGeom);
typedef void (*fn_SceneRemoveObject_t)(void* scene, uint32_t objId);
typedef int  (*fn_GetNodeIdx_t)(void* graphicsMesh, const char* name);

static fn_SetMesh_t          s_SetMesh        = nullptr;
static fn_Vector4Ctor_t      s_Vector4Ctor    = nullptr;
static fn_TimeCtor_t         s_TimeCtorMesh   = nullptr;  // 0x140152968 (MeshInstance update arg)
static fn_SceneAddObject_t   s_SceneAddObject = nullptr;
static fn_GeometryAttach_t   s_GeomAttach     = nullptr;
static fn_GeometryDetach_t   s_GeomDetach     = nullptr;
static fn_SceneRemoveObject_t s_SceneRemove   = nullptr;
static fn_GetNodeIdx_t       s_GetNodeIdx     = nullptr;
typedef void (*fn_MIDefCtor_t)(void* meshInstance);       // 0x140151490 (MeshInstance default-ctor)
static fn_MIDefCtor_t        s_MI_DefCtor     = nullptr;
static float*                s_pUVScale       = nullptr;  // g_fUVScale (DAT_14017214c)
static uint32_t*             s_pTid_FenceDesc = nullptr;  // DAT_1401c7a88
static uint32_t*             s_pTid_RawMat    = nullptr;  // DAT_1401c7cb8
static bool                  s_showInit       = false;

// [diag] BANISHED_SHOWTOOL_TRACE support. ReadableU is used ONLY by the probe, never as a
// guard on the faithful path -- it decides whether the trace prints a value or a 0.
// It must be a REAL page test, not a range test: the value this open is chasing
// (0x100000001) passes any plausible range check, so a range-only probe would fault on
// exactly the pointer it was added to report.
inline bool ReadableU(uintptr_t a) {
    if (a < 0x10000 || a >= 0x00007FFFFFFFFFFFULL) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(reinterpret_cast<void*>(a), &mbi, sizeof(mbi)) != sizeof(mbi)) return false;
    if (mbi.State != MEM_COMMIT) return false;
    const DWORD prot = mbi.Protect;
    if (prot & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return (prot & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                    PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

inline bool ToolTraceOn() {
    static int s = -1;
    if (s < 0) { const char* e = std::getenv("BANISHED_SHOWTOOL_TRACE"); s = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return s == 1;
}

static void EnsureShow() {
    if (s_showInit) return;
    s_SetMesh        = reinterpret_cast<fn_SetMesh_t>(ReadDispatchSlot(0x140151498));
    // ⚠ 0x140152970 is NOT a Vector4 ctor — the disasm at 0x14011765b shows that slot
    // family is Graphics: 0x140152970 = GraphicsMesh::GetNodeIndexFromName (returns int
    // in EAX). Calling it as a ctor garbage-derefs the "returned" v[0..3]. Left NULL on
    // purpose (audit 2026-07-26) so its block stays skipped until the real Vector4 ctor
    // slot is identified; do NOT register 0x140152970 to "fix" the skip.
    s_Vector4Ctor    = nullptr;
    // ⚠ was ReadDispatchSlot(0x140152968) — that slot is Graphics::Geometry::Attach
    // (dst, src, nodeIdx), not the Time ctor; retail's Time(0.0) there goes through
    // 0x140152b00 (disasm 0x14011766c). Dormant until now only because 0x140152968 is
    // unregistered. Bind the REAL Time-from-float slot.
    s_TimeCtorMesh   = reinterpret_cast<fn_TimeCtor_t>(ReadDispatchSlot(0x140152b00));
    s_SceneAddObject = reinterpret_cast<fn_SceneAddObject_t>(ReadDispatchSlot(0x140151370));
    s_GeomAttach     = reinterpret_cast<fn_GeometryAttach_t>(
        ResolveExport("?Attach@Geometry@Graphics@@QEAAXAEAV12@H@Z"));
    s_GeomDetach     = reinterpret_cast<fn_GeometryDetach_t>(
        ResolveExport("?Detach@Geometry@Graphics@@QEAAXAEAV12@@Z"));
    s_SceneRemove    = reinterpret_cast<fn_SceneRemoveObject_t>(
        ResolveExport("?RemoveObject@Scene@Graphics@@QEAAXI@Z"));
    s_GetNodeIdx     = reinterpret_cast<fn_GetNodeIdx_t>(
        ResolveExport("?GetNodeIndexFromName@GraphicsMesh@Graphics@@QEBAHPEBD@Z"));
    s_MI_DefCtor     = reinterpret_cast<fn_MIDefCtor_t>(ReadDispatchSlot(0x140151490));
    s_pUVScale       = reinterpret_cast<float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pTid_FenceDesc = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7a88));
    s_pTid_RawMat    = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
    s_showInit = true;
}

// g_pGameWorldSingleton is the DAT_1401c3dd8 singleton (declared in engine_funcs.h).
} // namespace

// Reimpl of FUN_1401176e0 — hide/detach the current tool mesh from the entity.
// Reads the active scene-object id at entity+0x1dc; if valid (>=0), detaches the tool
// geometry from the citizen's model geometry (entity+0x278's +0x68 model, +0x28 geom)
// and removes the scene object, then marks the id slot invalid (-1).
// Caller signature: void fn(void* entity)
void ActCt_HideTool_1176e0(void* entityV) {
    EnsureShow();
    uintptr_t e = reinterpret_cast<uintptr_t>(entityV);
    if (*reinterpret_cast<int*>(e + 0x1dc) < 0) return;

    CheckPtr(reinterpret_cast<void*>(e + 0x278));
    uintptr_t model = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(e + 0x278) + 0x68);
    CheckPtr(&model);
    if (s_GeomDetach) s_GeomDetach(reinterpret_cast<void*>(model + 0x28), reinterpret_cast<void*>(e + 0xb8));

    if (s_SceneRemove && g_pGameWorldSingleton) {
        void* scene = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x60);
        s_SceneRemove(scene, *reinterpret_cast<uint32_t*>(e + 0x1dc));
    }
    *reinterpret_cast<int64_t*>(e + 0x1d8) = -1;
}

// Reimpl of Entity_impl_73b0 — show a tool mesh on the entity.
//   - If a tool is already shown (entity+0x1dc >= 0), hide it first (FUN_1401176e0).
//   - toolId >= 0: pick the mesh from the model's mesh table (entity+0x288 → +0x138 array)
//     using g_fUVScale.
//   - toolId < 0: "find best material from storage" path — find StorageComponent on the
//     parent entity, pick the inventory slot with the highest count, then locate a
//     FenceDescription (DAT_1401c7a88) component for the mesh and a RawMaterialDescription
//     (DAT_1401c7cb8) component for the UV scale. (This is the crash-prone path the
//     ShowTool call site deliberately guards off; ported faithfully for completeness.)
//   - Set the mesh on the MeshInstance (entity+0xb8), write the UV scale vector to
//     entity+0x1c4..+0x1d0, attach the geometry at the model's "tool" node, then add the
//     geometry to the game world scene and record the resulting object id.
// Caller signature: void fn(void* entity, int toolId)
void ActCt_ShowToolOnEntity_1173b0(void* entityV, int toolId) {
    EnsureShow();
    uintptr_t e = reinterpret_cast<uintptr_t>(entityV);

    // [diag] BANISHED_SHOWTOOL_TRACE — dump every link 73b0 is about to deref, BEFORE it
    // derefs them. The post-load re-show (CitizenComponent slot 11, +0x1d8 != -1) faults
    // on this path with a READ of 0x100000001, and a crash log alone cannot say WHICH
    // link is bad: the toolId>=0 branch chains desc(+0x288) -> meshArray(+0x138) ->
    // element[toolId], and the tail chains charComp(+0x278) -> model(+0x68) -> +0xe8.
    // Printing both chains turns the next run into a measurement instead of a repro.
    // Reports the value it READ at each step, so a link that is merely null is
    // distinguishable from one holding stream garbage. See
    // [[reference_diagnostics_must_report_outcomes]].
    if (ToolTraceOn()) {
        static int n = 0;
        if (n++ < 24) {
            const uintptr_t parentP = ReadableU(e + 0x10) ? *reinterpret_cast<uintptr_t*>(e + 0x10) : 0;
            const uintptr_t chP     = ReadableU(e + 0x278) ? *reinterpret_cast<uintptr_t*>(e + 0x278) : 0;
            const uintptr_t modelP  = ReadableU(chP + 0x68) ? *reinterpret_cast<uintptr_t*>(chP + 0x68) : 0;
            const uintptr_t descP   = ReadableU(e + 0x288) ? *reinterpret_cast<uintptr_t*>(e + 0x288) : 0;
            const uintptr_t arrP    = ReadableU(descP + 0x138) ? *reinterpret_cast<uintptr_t*>(descP + 0x138) : 0;
            const int32_t   arrCnt  = ReadableU(descP + 0x140) ? *reinterpret_cast<int32_t*>(descP + 0x140) : -1;
            const int32_t   arrCap  = ReadableU(descP + 0x144) ? *reinterpret_cast<int32_t*>(descP + 0x144) : -1;
            const uintptr_t elem    = (arrP && toolId >= 0 && ReadableU(arrP + static_cast<uintptr_t>(toolId) * 8))
                                    ? *reinterpret_cast<uintptr_t*>(arrP + static_cast<uintptr_t>(toolId) * 8) : 0;
            std::fprintf(stderr,
                "[ToolTrace] e=%p toolId=%d sceneId=%d | parent=%p flags=%#x | char=%p model=%p mesh(+0xe8)=%p"
                " | desc=%p arr(+0x138)=%p cnt=%d cap=%d elem[%d]=%p\n",
                reinterpret_cast<void*>(e), toolId,
                ReadableU(e + 0x1dc) ? *reinterpret_cast<int32_t*>(e + 0x1dc) : -999,
                reinterpret_cast<void*>(parentP),
                ReadableU(parentP + 0x40) ? *reinterpret_cast<uint32_t*>(parentP + 0x40) : 0u,
                reinterpret_cast<void*>(chP), reinterpret_cast<void*>(modelP),
                reinterpret_cast<void*>(ReadableU(modelP + 0xe8) ? *reinterpret_cast<uintptr_t*>(modelP + 0xe8) : 0),
                reinterpret_cast<void*>(descP), reinterpret_cast<void*>(arrP), arrCnt, arrCap,
                toolId, reinterpret_cast<void*>(elem));
            std::fflush(stderr);
        }
    }

    CheckPtr(reinterpret_cast<void*>(e + 0x10));
    uintptr_t parent = *reinterpret_cast<uintptr_t*>(e + 0x10);
    if (((*reinterpret_cast<uint32_t*>(parent + 0x40) >> 10) & 1) != 0) return;  // removed flag

    if (*reinterpret_cast<int*>(e + 0x1dc) >= 0) ActCt_HideTool_1176e0(entityV);

    CheckPtr(reinterpret_cast<void*>(e + 0x278));
    uintptr_t model = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(e + 0x278) + 0x68);

    void* meshPtr = nullptr;
    float uvScale = 0.0f;

    if (toolId < 0) {
        // find best material from storage on parent entity
        CheckPtr(reinterpret_cast<void*>(e + 0x10));
        uint64_t storageIBP = 0;
        fn_FindComponent_Storage(*reinterpret_cast<void**>(e + 0x10), &storageIBP);
        CheckPtr(&storageIBP);
        uintptr_t storage = static_cast<uintptr_t>(storageIBP);
        if (*reinterpret_cast<int*>(storage + 0x30) < 1) return;

        // pick inventory slot (stride 0x10, count field at +8) with the highest count
        void* bestItem = nullptr;
        int bestCount = 0;
        int idx = 0;
        do {
            CheckPtr(&storageIBP);
            storage = static_cast<uintptr_t>(storageIBP);
            uintptr_t inv = *reinterpret_cast<uintptr_t*>(storage + 0x28);
            uintptr_t slot = inv + static_cast<uintptr_t>(idx) * 0x10;
            int c = *reinterpret_cast<int*>(slot + 8);
            if (bestCount < c) {
                bestItem = *reinterpret_cast<void**>(slot);
                bestCount = c;
            }
            idx++;
            CheckPtr(&storageIBP);
            storage = static_cast<uintptr_t>(storageIBP);
        } while (idx < *reinterpret_cast<int*>(storage + 0x30));

        if (bestItem == nullptr) return;
        uintptr_t item = reinterpret_cast<uintptr_t>(bestItem);

        // locate FenceDescription component for the mesh
        void* fenceComp = nullptr;
        if (s_pTid_FenceDesc && *reinterpret_cast<int*>(item + 0x30) > 0) {
            int n = *reinterpret_cast<int*>(item + 0x30);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(item + 0x28);
            for (int i = 0; i < n; i++) {
                void* comp = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
                uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
                if (t == *s_pTid_FenceDesc) {
                    void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
                    if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTid_FenceDesc)) fenceComp = c;
                    break;
                }
            }
        }
        meshPtr = **reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(fenceComp) + 0x20 + 8);

        // locate RawMaterialDescription component for the UV scale
        void* rawComp = nullptr;
        if (s_pTid_RawMat && *reinterpret_cast<int*>(item + 0x30) > 0) {
            int n = *reinterpret_cast<int*>(item + 0x30);
            uintptr_t arr = *reinterpret_cast<uintptr_t*>(item + 0x28);
            for (int i = 0; i < n; i++) {
                void* comp = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
                uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
                if (t == *s_pTid_RawMat) {
                    void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
                    if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *s_pTid_RawMat)) rawComp = c;
                    break;
                }
            }
        }
        uvScale = *reinterpret_cast<float*>(reinterpret_cast<uintptr_t>(rawComp) + 0x50);
    } else {
        // mesh table lookup from the model description at entity+0x288 → +0x138
        uintptr_t descArr = *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(e + 0x288) + 0x138);
        meshPtr = *reinterpret_cast<void**>(descArr + static_cast<uintptr_t>(toolId) * 8);
        uvScale = s_pUVScale ? *s_pUVScale : 1.0f;
    }

    // Ensure the held-item MeshInstance (entity+0xb8) is a constructed object before
    // any SetMesh/Attach/Update/AddObject. Retail treats +0xb8 as an embedded, already-
    // constructed Graphics::MeshInstance; in standalone a builder/worker showing its
    // first tool has never had that slot initialized (zeroed vtable), so MeshInstance::
    // Update calls a null vtable and Scene::AddObject faults on the uninitialized geometry
    // (READ of +0x40). Default-construct it in place with sceneId(+0x120) = -1 — the same
    // guard showtool.cpp uses for carried items. (This was missing from the registered
    // ActCt_ variant, crashing the FIRST build-labor ShowTool.)
    if (*reinterpret_cast<void**>(e + 0xb8) == nullptr && s_MI_DefCtor) {
        s_MI_DefCtor(reinterpret_cast<void*>(e + 0xb8));
        *reinterpret_cast<uint32_t*>(e + 0xb8 + 0x120) = 0xffffffff;   // sceneId = -1
    }

    // set mesh on the MeshInstance (entity+0xb8)
    if (s_SetMesh) s_SetMesh(reinterpret_cast<void*>(e + 0xb8), meshPtr);

    // write the UV-scale vector (Vector4 broadcast of uvScale) to entity+0x1c4..+0x1d0
    float scalar = uvScale;
    uint8_t vec4[32];
    if (s_Vector4Ctor) {
        uint32_t* v = reinterpret_cast<uint32_t*>(s_Vector4Ctor(vec4, &scalar));
        *reinterpret_cast<uint32_t*>(e + 0x1c4) = v[0];
        *reinterpret_cast<uint32_t*>(e + 0x1c8) = v[1];
        *reinterpret_cast<uint32_t*>(e + 0x1cc) = v[2];
        *reinterpret_cast<uint32_t*>(e + 0x1d0) = v[3];
    }

    // attach the tool geometry to the model's "tool" node
    CheckPtr(&model);
    void* graphicsMesh = *reinterpret_cast<void**>(model + 0xe8);
    CheckPtr(&model);
    int nodeIdx = (s_GetNodeIdx && graphicsMesh) ? s_GetNodeIdx(graphicsMesh, "tool") : -1;
    if (s_GeomAttach) s_GeomAttach(reinterpret_cast<void*>(model + 0x28), reinterpret_cast<void*>(e + 0xb8), nodeIdx);

    // MeshInstance::Update(time=0) via vtable +0x8 (skip if the instance has no vtable)
    uint8_t timeVal[8];
    if (s_TimeCtorMesh && *reinterpret_cast<uintptr_t*>(e + 0xb8) != 0) {
        s_TimeCtorMesh(timeVal, 0.0f);
        (*reinterpret_cast<void(**)(void*, void*)>(*reinterpret_cast<uintptr_t*>(e + 0xb8) + 8))(
            reinterpret_cast<void*>(e + 0xb8), timeVal);
    }

    // add to scene + record object id
    if (s_SceneAddObject && g_pGameWorldSingleton) {
        void* scene = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x60);
        uint32_t objId = s_SceneAddObject(scene, reinterpret_cast<void*>(e + 0xb8));
        *reinterpret_cast<int*>(e + 0x1d8) = toolId;
        *reinterpret_cast<uint32_t*>(e + 0x1dc) = objId;
        SceneWatch_Note(objId, reinterpret_cast<void*>(e + 0xb8), "ShowTool 0x1401173b0");
    }
}
