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

#ifdef STANDALONE_EXE
namespace Object { uint32_t GetTypeIdByName(const char* name); }  // factory_registration.h
#endif

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
#ifdef STANDALONE_EXE
    // Raw .rdata type-id globals are NULL in standalone; fall back to the runtime type
    // registry so these FindComponent helpers actually match (else they return null and
    // callers deref null). Names verified from decomp/application.md TypeInfo column.
    static uint32_t v_2ed0, v_3050, v_6270;
    if (!s_pTid_2ed0) { v_2ed0 = ::Object::GetTypeIdByName("InteractComponent"); s_pTid_2ed0 = &v_2ed0; }
    if (!s_pTid_3050) { v_3050 = ::Object::GetTypeIdByName("WorkComponent");      s_pTid_3050 = &v_3050; }
    if (!s_pTid_6270) { v_6270 = ::Object::GetTypeIdByName("StorageComponent");   s_pTid_6270 = &v_6270; }
#endif
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
// Work_Ctor25 (0x1400f72e0) — the shared Work base constructor.
// ============================================================
// Object::Internal::Internal(self); vtable = vtable_Work (0x1401612e0); +0x10 = entity
// ptr (from *param2); sentinels +0x20/+0x28 = -1; and when an entity is present, compute
// the work target cell (+0x28) from its MapComponent footprint (via FUN_140004680) and
// store its WorkComponent (+0x18, via FUN_140003050). Faithful 1:1 port.
//
// NOT registered before: works built via the factory use Work_GenericCtor, but direct
// Ctor ports (e.g. GatherRawMaterials_Ctor_Impl) call 0x1400f72e0 — NULL in standalone —
// leaving the Work base (incl. the +0x10 entity ptr) uninitialised, so Execute later
// deref'd *(work+0x10)=null. Registering this fixes those direct-construction paths.
void* Work_Ctor25_Impl(void* self, void* pEntityIBP) {
    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    uintptr_t vtWork = ResolveAddr(Module::Exe, 0x1401612e0);
    if (vtWork) *reinterpret_cast<uintptr_t*>(s) = vtWork;

    *reinterpret_cast<uint64_t*>(s + 0x10) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x10) = pEntityIBP ? *reinterpret_cast<uint64_t*>(pEntityIBP) : 0;
    *reinterpret_cast<uint64_t*>(s + 0x18) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x20) = 0xffffffff;
    *reinterpret_cast<uint32_t*>(s + 0x24) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x28) = 0xffffffff;
    *reinterpret_cast<uint32_t*>(s + 0x2c) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x30) = 0;
    *reinterpret_cast<uint64_t*>(s + 0x38) = 0;
    *reinterpret_cast<uint32_t*>(s + 0x40) = 0;

    if (*reinterpret_cast<uint64_t*>(s + 0x10) != 0) {
        typedef void* (*Find_t)(uintptr_t, void*);
        Find_t f4680 = reinterpret_cast<Find_t>(ResolveAddr(Module::Exe, 0x140004680));
        uintptr_t tmp = 0;
        if (f4680) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(s + 0x10);
            void* r = f4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &tmp);
            if (r && *reinterpret_cast<uintptr_t*>(r) != 0) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(s + 0x10);
                r = f4680(*reinterpret_cast<uintptr_t*>(s + 0x10), &tmp);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(r);
                uintptr_t comp = *reinterpret_cast<uintptr_t*>(r);
                if (comp && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
                    int* mapGrid = *reinterpret_cast<int**>(gw + 0xf8);
                    int mapSize = mapGrid ? *mapGrid : 0;
                    int16_t f2e = *reinterpret_cast<int16_t*>(comp + 0x2e);
                    int16_t f2a = *reinterpret_cast<int16_t*>(comp + 0x2a);
                    int16_t f2c = *reinterpret_cast<int16_t*>(comp + 0x2c);
                    int16_t f28 = *reinterpret_cast<int16_t*>(comp + 0x28);
                    *reinterpret_cast<int*>(s + 0x28) =
                        (((int)f2e >> 1) + (int)f2a) * mapSize + (((int)f2c >> 1) + (int)f28);
                }
            }
        }
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(pEntityIBP);
        void* r2 = WorkBase_FindComponent_3050(
            pEntityIBP ? *reinterpret_cast<uintptr_t*>(pEntityIBP) : 0, &tmp);
        if (r2) *reinterpret_cast<uint64_t*>(s + 0x18) = *reinterpret_cast<uint64_t*>(r2);
    }
    return self;
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
#ifdef STANDALONE_EXE
    // The IsA gate reads DWORD_1401c7158 (== g_TypeId_CitizenAIComponent_FC, actually the
    // CitizenComponent type-id — see citizenui.cpp:794). That raw .rdata global is null in
    // the standalone exe unless some other path (e.g. the citizen UI dialog) has already
    // healed it, so heal it here too — otherwise ciTid==0 makes CanStartWork bail (return 0)
    // and every registered haul task is judged invalid, driving the enqueue↔invalidate reset
    // loop that stops any construction haul from completing.
    if (!ciTid) { static uint32_t v = 0; if (!v) v = ::Object::GetTypeIdByName("CitizenComponent"); ciTid = v; }
#endif
    if (!fn_IsA || !ciTid || !fn_IsA(obj, ciTid)) return 0;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(taskIBP);
    uintptr_t ci = *reinterpret_cast<uintptr_t*>(taskIBP);
    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(ci + 0x278));
    uintptr_t container = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    int count = *reinterpret_cast<int*>(container + 0x60) + 1;
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(container + 0x48);

    uint32_t completeTaskTid = 0;
    if (uint32_t* pCT = reinterpret_cast<uint32_t*>(ResolveAddr(Module::Exe, 0x1401c5e50))) completeTaskTid = *pCT;
#ifdef STANDALONE_EXE
    // 0x1401c5e50 == CompleteTask TypeInfo (decomp/application.md). Raw .rdata type-id
    // globals resolve to NULL in the standalone exe, so completeTaskTid stays 0 and the
    // IsDerivedFrom(elem, completeTaskTid) below can NEVER recognise the CompleteTask node
    // the haul chain pushed onto the citizen — CanStartWork then returns 0 for a citizen
    // that IS validly hauling, and GatherRawMaterials::Update invalidates+removes it every
    // tick (the enqueue↔reset loop). Heal via the runtime type registry by name.
    if (!completeTaskTid) { static uint32_t v = 0; if (!v) v = ::Object::GetTypeIdByName("CompleteTask"); completeTaskTid = v; }
#endif
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
    // [STANDALONE GUARD] srcCell == -1 when an entity's cell can't be resolved
    // (Entity_impl_7fb0 returns -1 because the building's MapComponent isn't found
    // via FUN_140004680 in standalone — unfilled type-id). (uint32_t)(-1)*0x30 then
    // wild-reads the cell array. The real game always passes in-range cells. Bail
    // "not reachable" on any out-of-range cell. Surfaced by BANISHED_FORCE_WORK
    // (CheckWorkplaceValid anchors the inert workplace building).
    int dim = mapGrid ? *mapGrid : 0;
    int64_t cellCount = static_cast<int64_t>(dim) * dim;
    if (srcCell < 0 || static_cast<int64_t>(srcCell) >= cellCount ||
        static_cast<int64_t>(destCell) >= cellCount)
        return 0;

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
