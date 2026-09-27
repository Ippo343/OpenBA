// code/Game/Work/work_base_reimpl.cpp
// Shared Work base-class / ECS helpers referenced from many Work classes.
//
// These exe sub-helpers are resolved via ResolveAddr(Module::Exe, 0xADDR) from
// nearly every code/Game/Work/*.cpp. They resolve to 0 (silently skipped) unless
// registered.
// This file provides faithful reimplementations and a single registration point
// (RegisterWorkBaseBatch in standalone/reg_work_base.cpp) so the shared call sites
// fire real logic in the standalone exe.
//
// Wave 2 (Work) — shared base-helper sub-batch. Each function is a 1:1 port of the
// decompiled exe helper; do not "improve" behavior.
//

#include "engine_funcs.h"
#include "resolve.h"
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch C
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <windows.h>
#include <new>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Work/Work.h"         // ENDGAME B.3.8 (WRK): Work::Persist / Work::InitFromEntity
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/InteractComponent.h"   // ENDGAME B.3.8 CB3: InteractComponent::TypeId()
#include "Component/WorkComponent.h"   // ENDGAME B.3.8 CB3: WorkComponent::TypeId()
#include "Component/StorageComponent.h"   // ENDGAME B.3.8 CB3: StorageComponent::TypeId()
#include "Component/TradeComponent.h"   // ENDGAME B.3.8 CB3: TradeComponent::TypeId()
#include "Component/CitizenComponent.h"   // ENDGAME B.3.8 CB3: CitizenComponent::TypeId()
#include "Action/CompleteTask.h"   // ENDGAME B.3.8 CB3: CompleteTask::TypeId()
#include "Component/StatusIconComponent.h"          // ENDGAME B.3.9 D5
#include "Component/NaturalResourceComponent.h"     // ENDGAME B.3.9 D5
#include "Entity/ComponentEntity.h"                 // ENDGAME B.3.9 D5


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

// Generic body shared by all variants (ENDGAME B.3.9 D5: typeId is the class's own
// TypeId() value, not a cell pointer -- every cell in this family is registered, so
// the value is always available and no resolve/heal is needed).
static void* FindComponentByTypeId(uintptr_t entity, void* retIBP, uint32_t typeId) {
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t list = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < n; i++) {
        void** slot = reinterpret_cast<void**>(list + static_cast<int64_t>(i) * 8);
        void* comp = *slot;
        // GetTypeId is component vtable slot +0x10.
        uint32_t t = (*reinterpret_cast<uint32_t(**)(void*)>(*reinterpret_cast<uintptr_t*>(comp) + 0x10))(comp);
        if (t == typeId) {
            void* c = *slot;
            found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, typeId)) ? c : nullptr;
            break;
        }
    }
    if (fn_InternalBasePtr_Ctor) new (retIBP) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(found));   // B.3.3 bridge
    return retIBP;
}

// Reimpl of Entity_FindComponent_Position (FUN_140002ed0, type 0x1401c7900 = InteractComponent)
void* WorkBase_FindComponent_2ed0(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeId(entity, retIBP, InteractComponent::TypeId());
}

// Reimpl of Entity_FindComponent_MeshInstance (FUN_140002f90, type 0x1401c7e08 = StatusIconComponent)
void* WorkBase_FindComponent_2f90(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeId(entity, retIBP, StatusIconComponent::TypeId());
}

// Reimpl of FUN_140003050 (type 0x1401c8070 = WorkComponent)
void* WorkBase_FindComponent_3050(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeId(entity, retIBP, WorkComponent::TypeId());
}

// Reimpl of FUN_1400052c0 (type 0x1401c7b30 = NaturalResourceComponent)
void* WorkBase_FindComponent_52c0(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeId(entity, retIBP, NaturalResourceComponent::TypeId());
}

// Reimpl of Entity_FindComponent_Storage (FUN_140006270, type 0x1401c7e78 = StorageComponent)
void* WorkBase_FindComponent_6270(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeId(entity, retIBP, StorageComponent::TypeId());
}

// Reimpl of Entity_FindComponent_Building (FUN_140015140, type 0x1401c4b80 = TradeComponent).
// Byte-for-byte the same component scan as the family above; the Ghidra name is a misnomer
// (it matches the TradeComponent type-id, not a generic "building"). Ported to close the
// TradeWork::Slot4 closure gap.
void* WorkBase_FindComponent_15140(uintptr_t entity, void* retIBP) {
    return FindComponentByTypeId(entity, retIBP, TradeComponent::TypeId());
}

// ============================================================
// Work_Ctor25 (0x1400f72e0) — the shared Work base constructor.
// ============================================================
// Object::Internal::Internal(self); vtable = vtable_Work (0x140161a80); +0x10 = entity
// ptr (from *param2); sentinels +0x20/+0x28 = -1; and when an entity is present, compute
// the work target cell (+0x28) from its MapComponent footprint (via FUN_140004680) and
// store its WorkComponent (+0x18, via FUN_140003050). Faithful 1:1 port.
//
// NOT registered before: works built via the factory use Work_GenericCtor, but direct
// Ctor ports (e.g. GatherRawMaterials_Ctor_Impl) call 0x1400f72e0 — NULL in standalone —
// leaving the Work base (incl. the +0x10 entity ptr) uninitialised, so Execute later
// deref'd *(work+0x10)=null. Registering this fixes those direct-construction paths.
//
// ENDGAME B.3.8 (WRK): ONE body, two entry points (B8-1). `Work::InitFromEntity` is
// the ctor MINUS its `??0Internal` call and vptr store; `Work::Work(void*)` runs it
// after the compiler's stamps, and this by-address door (reached from the component
// CreateWork bodies of other blocks, which then stamp the leaf table over it) runs it
// after `??0Internal` and the 0x140161a80 stamp — the table Work::Vtable() serves.
void* Work_Ctor25_Impl(void* self, void* pEntityIBP) {
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);
    // Was 0x1401612e0, which is not a vtable at all: it is the INTERIOR of FireWork's
    // 13-slot vtable (starts 0x1401612a0, ends 0x140161308 where the "FishingWork" name
    // string sits), i.e. FireWork vtable + 0x40 = slot 8. Ghidra reports ZERO xrefs to
    // 0x1401612e0; every Work-hierarchy ctor/dtor LEAs 0x140161a80 instead
    // (0x1400f730a LEA RAX,[0x140161a80] right here in retail's Work_Ctor25).
    // Work is abstract (no ctor to run on a raw object): stamp the compiler table.
    *reinterpret_cast<const void**>(self) = Work::Vtable();
    reinterpret_cast<Work*>(self)->InitFromEntity(pEntityIBP);   // B.3.8 bridge (door onto a raw object)
    return self;
}

void Work::InitFromEntity(void* pEntityIBP) {
    field_10 = nullptr;
    field_10 = pEntityIBP ? *reinterpret_cast<void**>(pEntityIBP) : nullptr;
    field_18 = nullptr;
    field_20 = -1;
    field_24 = 0;
    field_28 = -1;
    field_2c = 0;
    field_30 = nullptr;
    field_38 = nullptr;
    field_40 = 0;

    if (field_10 != nullptr) {
        typedef void* (*Find_t)(uintptr_t, void*);
        Find_t f4680 = reinterpret_cast<Find_t>(ResolveAddr(Module::Exe, 0x140004680));
        uintptr_t tmp = 0;
        if (f4680) {
            void* r = f4680(reinterpret_cast<uintptr_t>(field_10), &tmp);
            if (r && *reinterpret_cast<uintptr_t*>(r) != 0) {
                r = f4680(reinterpret_cast<uintptr_t>(field_10), &tmp);
                uintptr_t comp = *reinterpret_cast<uintptr_t*>(r);
                if (comp && g_pGameWorldSingleton && *g_pGameWorldSingleton) {
                    uintptr_t gw = reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton);
                    int* mapGrid = *reinterpret_cast<int**>(gw + 0xf8);
                    int mapSize = mapGrid ? *mapGrid : 0;
                    int16_t f2e = *reinterpret_cast<int16_t*>(comp + 0x2e);
                    int16_t f2a = *reinterpret_cast<int16_t*>(comp + 0x2a);
                    int16_t f2c = *reinterpret_cast<int16_t*>(comp + 0x2c);
                    int16_t f28 = *reinterpret_cast<int16_t*>(comp + 0x28);
                    field_28 = (((int)f2e >> 1) + (int)f2a) * mapSize + (((int)f2c >> 1) + (int)f28);
                }
            }
        }
        void* r2 = WorkBase_FindComponent_3050(
            pEntityIBP ? *reinterpret_cast<uintptr_t*>(pEntityIBP) : 0, &tmp);
        if (r2) field_18 = *reinterpret_cast<void**>(r2);
    }
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
    // ⚠ Retail re-reads BOTH the array pointer (*param_1) and the capacity
    // ((int)param_1[1]) on EVERY iteration — the +0x40 Cancel vcalls below can
    // push onto this same list (FUN_1400c54e0) and grow/realloc it mid-loop.
    // An earlier port cached both once; the first cancel-push that forced a
    // grow left the cached pointer dangling and the walk read freed memory
    // (crash: organic herder's first PastureWork claim, GrazingWorkCtor →
    // cleanup → MoveTo cancel → push → walk of the stale array).
    for (int i = 0; i < *reinterpret_cast<int*>(base + 8); i++) {
        uintptr_t slot = p[0] + static_cast<uintptr_t>(i) * 8;
        void* node = *reinterpret_cast<void**>(slot);
        if (!node) continue;
        void* cur = node;
        do {
            // action vtable +0x40 — cancel/abort (implicit this = cur)
            (*reinterpret_cast<void(**)(void*)>(*reinterpret_cast<uintptr_t*>(cur) + 0x40))(cur);
            cur = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cur) + 0x10);  // node[2] = next
        } while (cur != nullptr);
        // Slot address recomputed from the CURRENT array pointer (may have moved).
        slot = p[0] + static_cast<uintptr_t>(i) * 8;
        if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(slot))->DeletePtr(
            );   // B.3.3 bridge
        uintptr_t nul = 0;
        if (fn_InternalBasePtr_Ctor) new (&nul) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<uintptr_t*>(slot) = nul;
    }
    *reinterpret_cast<int*>(base + 0x18) = -1;
    if (p[2] != 0) {
        if (fn_InternalBasePtr_DeletePtr) reinterpret_cast< ::Object::InternalBasePtr*>(&p[2])->DeletePtr(
            );   // B.3.3 bridge
        uintptr_t nul = 0;
        if (fn_InternalBasePtr_Ctor) new (&nul) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        p[2] = nul;
    }
}

// standalone/work_ser_probe.cpp - BANISHED_WORK_SER_PROBE=1 stale-target census.
// ⚠ NOT a null-guard on the derefs below: with the env var unset (the default) this
// returns immediately and the faithful retail deref stands untouched. It exists only
// to turn "save crashed" into a log line naming the stale Work while diagnosing.

// Reimpl of FUN_1400f7400 (Work::Serialize) — serialize a Work task (version 3 stream).
// Two task-target IBPs at +0x10 (must derive ComponentEntity) and +0x18 (must derive
// type 0x1401c8070) are post-validated and nulled on type mismatch; ints at +0x20/+0x28;
// +0x24 is version-gated (copied from +0x20 pre-v1); two Times + an int at +0x30..+0x40
// added at v3+.   Caller signature: void fn(void* self, void* stream)
void Work::Persist(void* stream) {
    alignas(8) uint64_t ser[3];  // IO::Serializer, 0x18 bytes on the original's stack
    ::new (static_cast<void*>(ser)) IO::Serializer(*reinterpret_cast<IO::Stream*>(stream), 3);   // B.3.4 bridge

    reinterpret_cast<Object::InternalBasePtr*>(&field_10)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    if (field_10 && fn_IsDerivedFrom) {
        if (!fn_IsDerivedFrom(field_10, ComponentEntity::TypeId()))
            field_10 = nullptr;
    }
    reinterpret_cast<Object::InternalBasePtr*>(&field_18)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4 bridge
    if (field_18 && fn_IsDerivedFrom) {
        if (!fn_IsDerivedFrom(field_18, WorkComponent::TypeId()))
            field_18 = nullptr;
    }
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(field_20, nullptr);   // B.3.4 bridge
    reinterpret_cast<IO::Serializer*>(ser)->Serialize(field_28, nullptr);   // B.3.4 bridge
    int ver = reinterpret_cast<IO::Serializer*>(ser)->GetVersion();   // B.3.4 bridge
    if (ver < 1)
        field_24 = field_20;
    else
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(field_24, nullptr);   // B.3.4 bridge
    if (reinterpret_cast<IO::Serializer*>(ser)->GetVersion() > 2) {   // B.3.4 bridge
        reinterpret_cast<System::Time*>(&field_30)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
        reinterpret_cast<System::Time*>(&field_38)->Serialize(*reinterpret_cast<IO::Serializer*>(ser));   // B.3.4b bridge
        reinterpret_cast<IO::Serializer*>(ser)->Serialize(field_40, nullptr);   // B.3.4 bridge
    }
    reinterpret_cast<IO::Serializer*>(ser)->~Serializer();   // B.3.4 bridge
}

uint8_t WorkBase_ValidateTask_f7100(void* self, void* /*citizenAI*/, void* taskIBP, void** out) {
    if (*reinterpret_cast<uintptr_t*>(taskIBP) == 0) return 0;
    void* obj = *reinterpret_cast<void**>(taskIBP);
    // DWORD_1401c7158 = CitizenComponent (ENDGAME B.3.9 D5: by cell, read directly --
    // the registered cell always holds this id, so the old healing fallback is gone).
    uint32_t ciTid = CitizenComponent::TypeId();
    if (!fn_IsA || !fn_IsA(obj, ciTid)) return 0;

    uintptr_t ci = *reinterpret_cast<uintptr_t*>(taskIBP);
    uintptr_t container = *reinterpret_cast<uintptr_t*>(ci + 0x278);
    int count = *reinterpret_cast<int*>(container + 0x60) + 1;
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(container + 0x48);

    // 0x1401c5e50 (CompleteTask's own cell) is UNREGISTERED -- the old raw-.rdata read
    // always failed and fell through to this same CompleteTask::TypeId() call, so
    // calling it directly is not a behaviour change (ENDGAME B.3.9 D5 note).
    uint32_t completeTaskTid = CompleteTask::TypeId();
    int wantId = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(self) + 0x20);

    for (int i = 0; i < count; i++) {
        void* elem = *reinterpret_cast<void**>(arrBase + static_cast<uintptr_t>(i) * 8);
        void* ptr = (fn_IsDerivedFrom && completeTaskTid && fn_IsDerivedFrom(elem, completeTaskTid)) ? elem : nullptr;
        uintptr_t res = 0;
        if (fn_InternalBasePtr_Ctor) new (&res) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(ptr));   // B.3.3 bridge
        if (res != 0) {
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
