// code/Game/Work/work_base_walkpos.cpp
// Wave 2 (Work) shared-helper cluster — walkable-position search + build-progress step.
//
// Faithful 1:1 ports of 3 Work-referenced exe helpers (decompiled + disasm-verified in Ghidra):
//   0x140122be0  FUN_140122be0             — walkable-position search over one build-record array
//   0x140122b00  FUN_140122b00             — recursive walkable-position search across children
//   0x1400f6ef0  BuildStructure_impl_6ef0  — build-progress / time-budget step
//
// These were deferred in the Work-tail wave because their exe sub-callee closure
// (0x140003f60 ClearPathQueue, 0x1400a00f0 ValidateCell, 0x14009f810 MapPathCost) was
// unregistered. As of the Action wave ALL of those are registered, so the cluster is now
// closure-complete except for the Math::Transform::operator* dispatch slot 0x140151470,
// which this wave registers (RegisterDispatchSlot -> ResolveExport, a pure Runtime math op).
//
// IMPORTANT correction over notes/wave2_drafts/work_base_impl2_full.cpp.draft:
//   The draft's 0x140122be0 dropped the Transform::operator* INPUT vector (R8). Disasm at
//   0x140122d08 `CALL [0x140151470]` shows the member-ABI value-return call is
//     RCX = &transform (7 floats from the path obj at +0x148..+0x160)
//     RDX = &outVec3   (hidden return)
//     R8  = &record[i] (the per-record input Vector3; record array base = *(param_2),
//                       count = *(param_2+8), element stride 0x18)
//   i.e. out = obj.transform * record[i].pos. The draft applied the transform to garbage.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cmath>

// ============================================================
// Lazily-resolved exe sub-helpers
// ============================================================
typedef void*    (*fn_ClearPathQueue_t)(uintptr_t entity, void* retIBP);          // 0x140003f60
typedef int      (*fn_MapPathCost_t)(int* mapGrid, uint32_t cellA, int cellB, uint8_t cache); // 0x14009f810
typedef uint64_t (*fn_ValidateCell_t)(int* mapGrid, uint32_t pathType, uint32_t srcCell, int dstCell); // 0x1400a00f0
// Math::Transform::operator*(Vector3 const&) const -> Vector3 (member value-return ABI):
//   RCX=transform, RDX=retVec3, R8=inVec3.  Dispatch slot [0x140151470].
typedef void     (*fn_TransformMul_t)(void* transform, float* outVec3, const float* inVec3);

static fn_ClearPathQueue_t  s_fn_ClearPathQueue = nullptr;   // 0x140003f60
static fn_MapPathCost_t     s_fn_MapPathCost    = nullptr;   // 0x14009f810
static fn_ValidateCell_t    s_fn_ValidateCell   = nullptr;   // 0x1400a00f0
static fn_TransformMul_t    s_fn_TransformMul   = nullptr;   // [0x140151470]

static bool s_walkInit = false;
static void EnsureWalk() {
    if (s_walkInit) return;
    s_fn_ClearPathQueue = reinterpret_cast<fn_ClearPathQueue_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_MapPathCost    = reinterpret_cast<fn_MapPathCost_t>(ResolveAddr(Module::Exe, 0x14009f810));
    s_fn_ValidateCell   = reinterpret_cast<fn_ValidateCell_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    uintptr_t slot = ResolveAddr(Module::Exe, 0x140151470);
    if (slot) s_fn_TransformMul = *reinterpret_cast<fn_TransformMul_t*>(slot);
    s_walkInit = true;
}

// GameWorld object = *g_pGameWorldSingleton (the Ghidra symbol g_pGameWorldSingleton is the
// already-dereferenced object; in this codebase g_pGameWorldSingleton is the pointer slot).
static inline uintptr_t WalkGameWorld() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }

// Clamp helper mirroring the inline (x<0 -> 0; x>=w -> w-1) the decompiler emits everywhere.
static inline int WalkClampCell(int v, int w) {
    if (v < 0) v = 0;
    if (v >= w) v = w - 1;
    return v;
}

// ============================================================
// 0x140122be0 — walkable-position search over one build-record array
// ============================================================
// param_1 = PositionComponent (entity IBP at +0x10; its cleared-path obj holds the Transform
//           at +0x148..+0x160). param_2 = build-record array {data@+0, count@+8}, each record
//           0x18 bytes with a Vector3 input at its start. param_3 = source world pos (float[2]+),
//           param_4 = best-pos out (float[3]), param_5 = best-distance out (int).
// Returns true if *param_5 < 0xffff (a reachable record was found).
bool WorkImpl_WalkablePos_122be0(uintptr_t param_1, uintptr_t param_2,
                                 float* param_3, float* param_4, int* param_5) {
    EnsureWalk();

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x10));
    uintptr_t pathObj = 0;   // ClearPathQueue fills this IBP; CheckPtr resolves it to the obj ptr
    if (s_fn_ClearPathQueue) s_fn_ClearPathQueue(*reinterpret_cast<uintptr_t*>(param_1 + 0x10), &pathObj);

    int* mapGrid = *reinterpret_cast<int**>(WalkGameWorld() + 0xf8);
    int w = *mapGrid;
    int srcCol = WalkClampCell(static_cast<int>(param_3[0] + static_cast<float>(w >> 1)), w);
    int srcRow = WalkClampCell(static_cast<int>(param_3[1] + static_cast<float>(w >> 1)), w);
    uint32_t srcCell = static_cast<uint32_t>(w * srcRow + srcCol);

    uintptr_t recBase  = *reinterpret_cast<uintptr_t*>(param_2);       // *(param_2)
    int       recCount = *reinterpret_cast<int*>(param_2 + 8);         // *(param_2+8)
    for (int i = 0; i < recCount; i++) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&pathObj);
        // Transform = 7 floats at obj+0x148 (quat 4 + translate 3).
        float xform[7];
        for (int k = 0; k < 7; k++)
            xform[k] = *reinterpret_cast<float*>(pathObj + 0x148 + k * 4);
        const float* inVec = reinterpret_cast<const float*>(recBase + static_cast<int64_t>(i) * 0x18);
        float pos[3] = {0.0f, 0.0f, 0.0f};
        if (s_fn_TransformMul) s_fn_TransformMul(xform, pos, inVec);

        int w2 = *mapGrid;
        int col = WalkClampCell(static_cast<int>(pos[0] + static_cast<float>(w2 >> 1)), w2);
        int row = WalkClampCell(static_cast<int>(pos[1] + static_cast<float>(w2 >> 1)), w2);
        uint32_t destCell = static_cast<uint32_t>(w2 * row + col);

        uint64_t reach = s_fn_ValidateCell ? s_fn_ValidateCell(mapGrid, 0xe1f, srcCell, static_cast<int>(destCell)) : 0;
        if (static_cast<char>(reach) != '\0') {
            int dist = s_fn_MapPathCost ? s_fn_MapPathCost(mapGrid, srcCell, static_cast<int>(destCell), '\x01') : -1;
            if (dist < 0) {
                int gw = *mapGrid;
                int dRow = static_cast<int>(srcCell) / gw - static_cast<int>(destCell) / gw;
                int dCol = static_cast<int>(srcCell) % gw - static_cast<int>(destCell) % gw;
                dist = static_cast<int>(std::sqrt(static_cast<float>(dRow * dRow + dCol * dCol))) + gw;
            }
            if (dist < *param_5) {
                *param_5 = dist;
                param_4[0] = pos[0];
                param_4[1] = pos[1];
                param_4[2] = pos[2];
            }
        }
    }
    return static_cast<uint32_t>(*param_5) < 0xffff;
}

// ============================================================
// 0x140122b00 — recursive walkable-position search across children
// ============================================================
// param_1 = PositionComponent. Resolves its BuildComponent (param_1+0x18, IsDerivedFrom
// DAT_1401c7938), runs WalkablePos_122be0 on this entity's record-array slot
// (build + param_2*0x10 + 0x28), then recurses into each child (array base +0x28, count +0x30).
// Returns the OR of all sub-results.
uint8_t WorkImpl_WalkablePosRecursive_122b00(uintptr_t param_1, int param_2,
                                             float* param_3, float* param_4, int* param_5) {
    void* buildRaw = *reinterpret_cast<void**>(param_1 + 0x18);
    uint32_t* pTid7938 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7938));
    void* build = nullptr;
    if (pTid7938 && fn_IsDerivedFrom && fn_IsDerivedFrom(buildRaw, *pTid7938))
        build = buildRaw;

    uint8_t result = WorkImpl_WalkablePos_122be0(
        param_1,
        reinterpret_cast<uintptr_t>(build) + static_cast<int64_t>(param_2) * 0x10 + 0x28,
        param_3, param_4, param_5) ? 1 : 0;

    int       childCount = *reinterpret_cast<int*>(param_1 + 0x30);
    uintptr_t childArr   = *reinterpret_cast<uintptr_t*>(param_1 + 0x28);
    for (int i = 0; i < childCount; i++) {
        uintptr_t* slot = reinterpret_cast<uintptr_t*>(childArr + static_cast<int64_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
        uint8_t sub = WorkImpl_WalkablePosRecursive_122b00(*slot, param_2, param_3, param_4, param_5);
        result = static_cast<uint8_t>(result | sub);
    }
    return result;
}

// ============================================================
// 0x1400f6ef0 — BuildStructure_impl_6ef0: build-progress / time-budget step
// ============================================================
// param_1 = work/progress record, param_2 = target entity IBP, param_3/param_4 = grid cells,
// param_5 = current Time. Returns a ulonglong whose low byte is a bool "should continue".
uint64_t WorkImpl_BuildStructureStep_6ef0(uintptr_t param_1, void* param_2IBP,
                                          uint32_t param_3, uint32_t param_4, void* param_5Time) {
    EnsureWalk();

    // +0x30 += (*param_5 - +0x38)
    *reinterpret_cast<int64_t*>(param_1 + 0x30) +=
        (*reinterpret_cast<int64_t*>(param_5Time) - *reinterpret_cast<int64_t*>(param_1 + 0x38));

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2IBP);
    int64_t gateA = *reinterpret_cast<int64_t*>(
        *reinterpret_cast<uintptr_t*>(WalkGameWorld() + 0xb0) + 0x30);

    uintptr_t target = *reinterpret_cast<uintptr_t*>(param_2IBP);
    if (*reinterpret_cast<int64_t*>(target + 0x1f0) != gateA) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2IBP);
        target = *reinterpret_cast<uintptr_t*>(param_2IBP);
        if (*reinterpret_cast<int64_t*>(target + 0x248) != 0) {
            uint64_t resDefault = *reinterpret_cast<uint64_t*>(WalkGameWorld() + 0x130);
            uintptr_t mapInfo   = *reinterpret_cast<uintptr_t*>(WalkGameWorld() + 0xf0);
            int budget = s_fn_MapPathCost
                ? s_fn_MapPathCost(*reinterpret_cast<int**>(WalkGameWorld() + 0xf8),
                                   param_3, static_cast<int>(param_4), '\x01')
                : -1;

            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_2IBP);
            void* obj = *reinterpret_cast<void**>(*reinterpret_cast<uintptr_t*>(param_2IBP) + 0x18);
            uint32_t* pTid7190 = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7190));
            void* stage = (pTid7190 && fn_IsDerivedFrom && fn_IsDerivedFrom(obj, *pTid7190)) ? obj : nullptr;
            uintptr_t st = reinterpret_cast<uintptr_t>(stage);

            int   field108 = *reinterpret_cast<int*>(st + 0x108);
            int   field0fc = *reinterpret_cast<int*>(st + 0xfc);
            int   field100 = *reinterpret_cast<int*>(st + 0x100);
            float field10c = *reinterpret_cast<float*>(st + 0x10c);
            int   field104 = *reinterpret_cast<int*>(st + 0x104);

            // Time cap = field104 * 300.0f seconds.
            int64_t cap = 0;
            if (fn_Time_Ctor_Float) fn_Time_Ctor_Float(&cap, static_cast<float>(field104) * 300.0f);

            if ((*reinterpret_cast<int*>(mapInfo + 0xa8) / 2 < *reinterpret_cast<int*>(param_1 + 0x40)) &&
                (*reinterpret_cast<int64_t*>(param_1 + 0x30) < cap)) {
                *reinterpret_cast<int64_t*>(param_1 + 0x30) *= 2;
            }
            if ((*reinterpret_cast<int64_t*>(param_1 + 0x30) < 0) ||
                (cap < *reinterpret_cast<int64_t*>(param_1 + 0x30))) {
                *reinterpret_cast<int64_t*>(param_1 + 0x30) = cap;
            }

            float accSec = fn_Time_AsSeconds ? fn_Time_AsSeconds(reinterpret_cast<void*>(param_1 + 0x30)) : 0.0f;
            float nowSec = fn_Time_AsSeconds ? fn_Time_AsSeconds(param_5Time) : 1.0f;
            uint32_t progress = static_cast<uint32_t>(((accSec / nowSec) / static_cast<float>(field104 * 300))
                                * static_cast<float>(field100 - field0fc)) + static_cast<uint32_t>(field0fc);

            uint64_t local_res10_v = resDefault;
            if (static_cast<int>(progress) < budget) {
                *reinterpret_cast<int*>(param_1 + 0x40) += 1;
                local_res10_v = static_cast<uint64_t>(progress);
            } else {
                uint64_t uVar8 = static_cast<uint64_t>(progress);
                if (budget <= field108) {
                    return (uVar8 & 0xffffffffffffff00ull) | 1u;
                }
                uVar8 = resDefault;
                float cmp = *reinterpret_cast<float*>(resDefault + 0x18);
                if (field10c < cmp || field10c == cmp) {
                    return (uVar8 & 0xffffffffffffff00ull) | 1u;
                }
            }
            return local_res10_v & 0xffffffffffffff00ull;
        }
    }
    return static_cast<uint64_t>(1);
}
