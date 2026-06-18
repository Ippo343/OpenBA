// code/Game/Work/work_base_reimpl.cpp
// Shared Work base-class / ECS helpers referenced from many Work classes.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from
// nearly every code/Game/Work/*.cpp. In DLL mode they resolve to the original exe
// code; in standalone mode they resolve to 0 (silently skipped) unless registered.
// This file provides faithful reimplementations and a single registration point
// (RegisterWorkBaseBatch in standalone/reg_work_base.cpp) so the shared call sites
// fire real logic in the standalone exe.
//
// Wave 2 (Work) — shared base-helper sub-batch. Each function is a 1:1 port of the
// decompiled exe helper; do not "improve" behavior.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cmath>

// ============================================================
// FindComponent family
// ============================================================
// FUN_140002ed0 / 2f90 / 3050 / 52c0 / 6270 are byte-for-byte identical except for
// the component type-ID global they compare against. Each scans the entity's
// component list (ptr array at entity+0x30, count at entity+0x38), calls each
// component's GetTypeId (vtable +0x10), and on the first type match runs an
// IsDerivedFrom check before returning the component through an out InternalBasePtr.
//
// Caller signature (matches the s_fn_FindComponent_* typedefs in the Work .cpp files
// and the fn_FindComponent_* decls in engine_funcs.h): the function returns the
// out-IBP it was handed.   void* fn(uintptr_t entity, void* retIBP)

// Generic body shared by all variants. pTypeId points at the exe's type-ID storage
// global (a uint32_t filled in by the component's RegisterType at startup).
static void* FindComponentByTypeId(uintptr_t entity, void* retIBP, const uint32_t* pTypeId) {
    void* found = nullptr;
    if (pTypeId) {
        int n = *reinterpret_cast<int*>(entity + 0x38);
        uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
        for (int i = 0; i < n; i++) {
            void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
            void* comp = *slot;
            // GetTypeId is component vtable slot +0x10.
            uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
            if (t == *pTypeId) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(slot);
                void* c = *slot;
                found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, *pTypeId)) ? c : nullptr;
                break;
            }
        }
    }
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(retIBP, found);
    return retIBP;
}

// Lazily-resolved type-ID storage globals (exe .rdata, filled by RegisterType).
static const uint32_t* s_pTid_2ed0 = nullptr;  // 0x1401c7900  (Position/Interact)
static const uint32_t* s_pTid_2f90 = nullptr;  // 0x1401c7e08  (MeshInstance/StatusIcon)
static const uint32_t* s_pTid_3050 = nullptr;  // 0x1401c8070
static const uint32_t* s_pTid_52c0 = nullptr;  // 0x1401c7b30  (NaturalResource/BuildState)
static const uint32_t* s_pTid_6270 = nullptr;  // 0x1401c7e78  (Storage)
static bool s_tidInit = false;

static void EnsureTypeIds() {
    if (s_tidInit) return;
    s_pTid_2ed0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7900));
    s_pTid_2f90 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e08));
    s_pTid_3050 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c8070));
    s_pTid_52c0 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7b30));
    s_pTid_6270 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_tidInit = true;
}

// Reimpl of Entity_FindComponent_Position (FUN_140002ed0, type 0x1401c7900)
void* WorkBase_FindComponent_2ed0(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_2ed0);
}

// Reimpl of Entity_FindComponent_MeshInstance (FUN_140002f90, type 0x1401c7e08)
void* WorkBase_FindComponent_2f90(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_2f90);
}

// Reimpl of FUN_140003050 (type 0x1401c8070)
void* WorkBase_FindComponent_3050(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_3050);
}

// Reimpl of FUN_1400052c0 (type 0x1401c7b30)
void* WorkBase_FindComponent_52c0(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_52c0);
}

// Reimpl of Entity_FindComponent_Storage (FUN_140006270, type 0x1401c7e78)
void* WorkBase_FindComponent_6270(uintptr_t entity, void* retIBP) {
    EnsureTypeIds();
    return FindComponentByTypeId(entity, retIBP, s_pTid_6270);
}

// ============================================================
// Recursive binary-tree post-order free
// ============================================================
// Both are simple recursive post-order frees of a binary tree, differing only in
// the byte offsets of the two child pointers within a node. The first arg (tree
// root / allocator owner) is unused by the original beyond being forwarded.
// Caller signature: void fn(void* root, void* node)

// Reimpl of FUN_140014c30 (TreeNodeCleanup) — children at +0x10 and +0x18.
void WorkBase_TreeNodeFree_14c30(void* root, void* node) {
    if (node) {
        WorkBase_TreeNodeFree_14c30(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x10));
        WorkBase_TreeNodeFree_14c30(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x18));
        if (fn_MemFreeStatic) fn_MemFreeStatic(node);
    }
}

// Reimpl of FUN_14000e320 (TreeNodeFree) — children at +0x18 and +0x20.
void WorkBase_TreeNodeFree_e320(void* root, void* node) {
    if (node) {
        WorkBase_TreeNodeFree_e320(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x18));
        WorkBase_TreeNodeFree_e320(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x20));
        if (fn_MemFreeStatic) fn_MemFreeStatic(node);
    }
}

// ============================================================
// Work base lifecycle
// ============================================================

// Reimpl of FUN_1400c5400 (TaskListCleanup) — cancel all pending work actions.
// self layout: [0]=action-list array base, +8=count(int), +0x10=IBP, +0x18=int sentinel.
// For each populated list slot, walk the singly-linked action chain (next at node+0x10),
// invoking each action's vtable +0x40 (cancel/abort); then DeletePtr the slot IBP and
// re-init it to null. Finally reset the +0x18 sentinel to -1 and clear the +0x10 IBP.
// Caller signature: void fn(void* self)
void WorkBase_TaskListCleanup_c5400(void* self) {
    uintptr_t* p = reinterpret_cast<uintptr_t*>(self);
    uintptr_t base = reinterpret_cast<uintptr_t>(self);
    int count = *reinterpret_cast<int*>(base + 8);
    uintptr_t arr = p[0];
    for (int i = 0; i < count; i++) {
        uintptr_t slot = arr + static_cast<uintptr_t>(i) * 8;
        void* node = *reinterpret_cast<void**>(slot);
        if (!node) continue;
        void* cur = node;
        do {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&cur);
            // action vtable +0x40 — cancel/abort (implicit this = cur)
            (*reinterpret_cast<void(**)(void*)>(*reinterpret_cast<uintptr_t*>(cur) + 0x40))(cur);
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&cur);
            cur = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cur) + 0x10);  // node[2] = next
        } while (cur != nullptr);
        if (fn_InternalBasePtr_DeletePtr) fn_InternalBasePtr_DeletePtr(reinterpret_cast<void*>(slot));
        uintptr_t nul = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nul, nullptr);
        *reinterpret_cast<uintptr_t*>(slot) = nul;
    }
    *reinterpret_cast<int*>(base + 0x18) = -1;
    if (p[2] != 0) {
        if (fn_InternalBasePtr_DeletePtr) fn_InternalBasePtr_DeletePtr(&p[2]);
        uintptr_t nul = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&nul, nullptr);
        p[2] = nul;
    }
}

// Reimpl of FUN_1400f7400 (Work::Serialize) — serialize a Work task (version 3 stream).
// Two task-target IBPs at +0x10 (must derive ComponentEntity) and +0x18 (must derive
// type 0x1401c8070) are post-validated and nulled on type mismatch; ints at +0x20/+0x28;
// +0x24 is version-gated (copied from +0x20 pre-v1); two Times + an int at +0x30..+0x40
// added at v3+.   Caller signature: void fn(void* self, void* stream)
void WorkBase_Serialize_f7400(void* self, void* stream) {
    EnsureTypeIds();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    uint64_t ser[3];  // IO::Serializer, 0x18 bytes on the original's stack
    if (!g_fn_Serializer_Ctor) return;
    g_fn_Serializer_Ctor(ser, stream, 3);

    if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(reinterpret_cast<void*>(s + 0x10), ser);
    if (*reinterpret_cast<void**>(s + 0x10) && fn_IsDerivedFrom && g_TypeId_ComponentEntity) {
        if (!fn_IsDerivedFrom(*reinterpret_cast<void**>(s + 0x10), *g_TypeId_ComponentEntity))
            *reinterpret_cast<void**>(s + 0x10) = nullptr;
    }
    if (fn_InternalBasePtr_Serialize) fn_InternalBasePtr_Serialize(reinterpret_cast<void*>(s + 0x18), ser);
    if (*reinterpret_cast<void**>(s + 0x18) && fn_IsDerivedFrom && s_pTid_3050) {
        if (!fn_IsDerivedFrom(*reinterpret_cast<void**>(s + 0x18), *s_pTid_3050))
            *reinterpret_cast<void**>(s + 0x18) = nullptr;
    }
    if (g_fn_Serialize_Int) {
        g_fn_Serialize_Int(ser, reinterpret_cast<void*>(s + 0x20), nullptr);
        g_fn_Serialize_Int(ser, reinterpret_cast<void*>(s + 0x28), nullptr);
    }
    int ver = g_fn_Serializer_GetVersion ? g_fn_Serializer_GetVersion(ser) : 0;
    if (ver < 1)
        *reinterpret_cast<int*>(s + 0x24) = *reinterpret_cast<int*>(s + 0x20);
    else if (g_fn_Serialize_Int)
        g_fn_Serialize_Int(ser, reinterpret_cast<void*>(s + 0x24), nullptr);
    if ((g_fn_Serializer_GetVersion ? g_fn_Serializer_GetVersion(ser) : 0) > 2) {
        if (g_fn_Time_Serialize) {
            g_fn_Time_Serialize(reinterpret_cast<void*>(s + 0x30), ser);
            g_fn_Time_Serialize(reinterpret_cast<void*>(s + 0x38), ser);
        }
        if (g_fn_Serialize_Int) g_fn_Serialize_Int(ser, reinterpret_cast<void*>(s + 0x40), nullptr);
    }
    if (g_fn_Serializer_Dtor) g_fn_Serializer_Dtor(ser);
}

// Reimpl of FUN_1400f7100 (Work::CanStartWork / ValidateTask) — does the given
// CitizenAIComponent IBP currently own a CompleteTask whose id (+0x30) matches this
// Work's task id (self+0x20)? The CitizenAIComponent's action container is at +0x278
// (array base +0x48, count +0x60 inclusive). Returns 1 (+ the matching task IBP through
// `out`) on the first match, else 0.
// Caller signature: uint8_t fn(void* self, void* citizenAI, void* taskIBP, void** out)
uint8_t WorkBase_ValidateTask_f7100(void* self, void* /*citizenAI*/, void* taskIBP, void** out) {
    if (*reinterpret_cast<uintptr_t*>(taskIBP) == 0) return 0;
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(taskIBP);
    void* obj = *reinterpret_cast<void**>(taskIBP);
    uint32_t ciTid = g_TypeId_CitizenAIComponent_FC ? *g_TypeId_CitizenAIComponent_FC : 0;
    if (!fn_IsA || !ciTid || !fn_IsA(obj, ciTid)) return 0;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(taskIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(taskIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t container = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    int count = *reinterpret_cast<int*>(container + 0x60) + 1;
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(container + 0x48);

    uint32_t completeTaskTid = 0;
    if (uint32_t* pCT = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5e50))) completeTaskTid = *pCT;
    int wantId = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(self) + 0x20);

    for (int i = 0; i < count; i++) {
        void* elem = *reinterpret_cast<void**>(arrBase + static_cast<uintptr_t>(i) * 8);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&elem);
        void* ptr = (fn_IsDerivedFrom && completeTaskTid && fn_IsDerivedFrom(elem, completeTaskTid)) ? elem : nullptr;
        uintptr_t res = 0;
        if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(&res, ptr);
        if (res != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&res);
            if (*reinterpret_cast<int*>(res + 0x30) == wantId) {
                if (out) *out = reinterpret_cast<void*>(res);
                return 1;
            }
        }
    }
    return 0;
}

// ============================================================
// Map distance / connectivity helpers
// ============================================================

// Reimpl of FUN_140008830 (Map_CalcDistance) — pathfinding-aware cell distance.
// Tries the cached path cost (Map_CalcPathCost, 0x14009f810); on no-path (<0) falls
// back to Euclidean grid distance (sqrtf) + gridWidth penalty. FUN_140132010 in the
// original is sqrtf (verified). Caller: int fn(int* mapGrid, uint32_t cellA, uint32_t cellB)
int WorkBase_CalcDistance_8830(int* mapGrid, uint32_t cellA, uint32_t cellB) {
    int cost = fn_Map_CalcPathCost ? fn_Map_CalcPathCost(mapGrid, cellA, static_cast<int>(cellB), 1) : -1;
    if (cost < 0) {
        int gridW = *mapGrid;
        int dRow = static_cast<int>(cellA) / gridW - static_cast<int>(cellB) / gridW;
        int dCol = static_cast<int>(cellA) % gridW - static_cast<int>(cellB) % gridW;
        cost = static_cast<int>(sqrtf(static_cast<float>(dRow * dRow + dCol * dCol))) + gridW;
    }
    return cost;
}

// FUN_14009c480 — connectivity/zone refresh for the (pathType, destCell, srcCell) query.
// The original passes all four arguments; resolve with the full 4-arg signature so we
// don't drop the trailing register args.
typedef void (*fn_MapConnRefresh_9c480_t)(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell);
static fn_MapConnRefresh_9c480_t s_fn_MapConnRefresh_9c480 = nullptr;
static bool s_mapConnInit = false;
static void EnsureMapConn() {
    if (s_mapConnInit) return;
    s_fn_MapConnRefresh_9c480 = reinterpret_cast<fn_MapConnRefresh_9c480_t>(
        ResolveAddr(Module::Exe, 0x14009c480));
    s_mapConnInit = true;
}

// Reimpl of FUN_1400a00f0 (Map::CanPathTo / WorldGrid_ValidateCell) — are two cells
// mutually reachable? Refreshes connectivity (FUN_14009c480), then ORs the two cells'
// pass-type bits with pathType; if both cell types survive the mask it returns 1 when a
// road/overlay bit (0x1e0) is set or when both cells share a non-zero region (uint16 @
// +0x26). Cell records: array at grid+0x80, stride 0x30, pass byte +0x29.
// Caller: bool fn(int* grid, uint32_t pathType, uint32_t destCell, int srcCell)
//   (low byte of the returned uint64 is the bool; the WorldGrid_ValidateCell int-variant
//    callers read the low 32 bits).
uint64_t WorkBase_ValidateCell_a00f0(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell) {
    EnsureMapConn();
    if (s_fn_MapConnRefresh_9c480) s_fn_MapConnRefresh_9c480(mapGrid, pathType, destCell, srcCell);

    uintptr_t cellArr = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(mapGrid) + 0x80);
    uintptr_t recSrc  = cellArr + static_cast<uintptr_t>(static_cast<uint32_t>(srcCell)) * 0x30;
    uintptr_t recDest = cellArr + static_cast<uintptr_t>(destCell) * 0x30;

    uint32_t typeSrc  = 1u << (*reinterpret_cast<const uint8_t*>(recSrc  + 0x29) & 0x1f);
    uint32_t typeDest = 1u << (*reinterpret_cast<const uint8_t*>(recDest + 0x29) & 0x1f);
    uint32_t combined = typeDest | typeSrc;
    uint64_t ret = combined;

    uint32_t mask = pathType | combined;
    if ((mask & typeDest) && (mask & typeSrc)) {
        if (mask & 0x1e0u) return (ret & 0xffffffffffffff00ull) | 1u;
        uint16_t regionDest = *reinterpret_cast<const uint16_t*>(recDest + 0x26);
        if (regionDest != 0) {
            uint16_t regionSrc = *reinterpret_cast<const uint16_t*>(recSrc + 0x26);
            ret = regionSrc;
            if (regionSrc != 0 && regionDest == regionSrc)
                return (ret & 0xffffffffffffff00ull) | 1u;
        }
    }
    return ret & 0xffffffffffffff00ull;
}
