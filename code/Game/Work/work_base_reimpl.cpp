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
#include "Work/exe_bodies.h"
// Wave 2 (Work) — shared base-helper sub-batch. Each function is a 1:1 port of the
// decompiled exe helper; do not "improve" behavior.
//

#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Action/exe_bodies.h"      // B3R: Pf_IsDstReachable_f810, Pf_ZoneRefresh_9c480
#include "Component/exe_bodies.h"   // B3R: FloorComponent_FindResData
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
#include "Component/CharacterComponent.h"            // B4 WORK-c: Work::ValidateTask
#include "Component/MapComponent.h"                  // B4 WORK-c: Work::InitFromEntity
#include "Action/Action.h"
#include "Game/Map.h"                                // B4 WORK-c: Map, MapCell


// ============================================================
// FindComponent family
// ============================================================
// FUN_140002ed0 / 2f90 / 3050 / 52c0 / 6270 / 15140 are instances of
// ComponentEntity::FindComponent<T>() (Entity/ComponentEntity.h). These names stay as
// B4 FORWARD forwarders, returning the out-IBP they were handed, until their callers
// hold a typed entity.
namespace {
// The one receiver cast of this file's six B4 FORWARD finders; it takes the place of
// the shared loop body (FindComponentByTypeId) the six names called before.
template <class T>
void* ForwardFindComponent(uintptr_t entity, void* retIBP) {
    return new (retIBP) ::Object::InternalPtr<T>(
        reinterpret_cast<ComponentEntity*>(entity)->FindComponent<T>());   // B4 FORWARD: receiver typed by the callers' blocks
}
}  // namespace

// Reimpl of Entity_FindComponent_Position (FUN_140002ed0, type 0x1401c7900 = InteractComponent)
void* WorkBase_FindComponent_2ed0(uintptr_t entity, void* retIBP) {
    return ForwardFindComponent<InteractComponent>(entity, retIBP);
}

// Reimpl of Entity_FindComponent_MeshInstance (FUN_140002f90, type 0x1401c7e08 = StatusIconComponent)
void* WorkBase_FindComponent_2f90(uintptr_t entity, void* retIBP) {
    return ForwardFindComponent<StatusIconComponent>(entity, retIBP);
}

// Reimpl of FUN_140003050 (type 0x1401c8070 = WorkComponent)
void* WorkBase_FindComponent_3050(uintptr_t entity, void* retIBP) {
    return ForwardFindComponent<WorkComponent>(entity, retIBP);
}

// Reimpl of FUN_1400052c0 (type 0x1401c7b30 = NaturalResourceComponent)
void* WorkBase_FindComponent_52c0(uintptr_t entity, void* retIBP) {
    return ForwardFindComponent<NaturalResourceComponent>(entity, retIBP);
}

// Reimpl of Entity_FindComponent_Storage (FUN_140006270, type 0x1401c7e78 = StorageComponent)
void* WorkBase_FindComponent_6270(uintptr_t entity, void* retIBP) {
    return ForwardFindComponent<StorageComponent>(entity, retIBP);
}

// ============================================================
// Work_Ctor25 (0x1400f72e0) — the shared Work base constructor.
// ============================================================
// Object::Internal::Internal(self); vtable = vtable_Work (0x140161a80); +0x10 = entity
// ptr (from *param2); sentinels +0x20/+0x28 = -1; and when an entity is present, compute
// the work target cell (+0x28) from its MapComponent footprint (via FUN_140004680) and
// store its WorkComponent (+0x18, via FUN_140003050). Faithful 1:1 port.
//
// ENDGAME B.3.9 OBJ: `Work(void*)` (work_base_standalone.cpp) is the ONE entry point.
// The by-address door `Work_Ctor25_Impl` (0x1400f72e0: the `??0Internal` door through
// import slot 0x140152768, a stamp of Work's table, then this body) went with its last
// callers, the attendance / destroy / fire CreateWork builders, which now construct the
// real leaf (LocationWork / ClearStorage / FireWork) through `Work(void*)`.
void Work::InitFromEntity(void* pEntityIBP) {
    // LEFT: Work(void*) and InitFromEntity(void*) keep their callers' untyped
    // entity reference (the leaves' ctors in other blocks).
    ComponentEntity* const* entityRef = reinterpret_cast<ComponentEntity* const*>(pEntityIBP);
    field_10 = nullptr;
    field_10 = entityRef ? *entityRef : nullptr;   // the tree's guard (retail reads *param_2)
    field_18 = nullptr;
    field_20 = -1;
    field_24 = 0;
    field_28 = -1;
    field_2c = 0;
    field_30._ticks = 0;
    field_38._ticks = 0;
    field_40 = 0;

    if (field_10 != nullptr) {
        // 0x140004680 = FindComponent<MapComponent>, called twice as retail does.
        if (field_10->FindComponent<MapComponent>() != nullptr) {
            MapComponent* footprint = field_10->FindComponent<MapComponent>();
            // The footprint / singleton / map null arms are the tree's guards;
            // retail reads **(gw + 0xF8) unguarded.
            if (footprint && GameUpdateLoop::_singleton != nullptr) {
                Map* map = GameUpdateLoop::_singleton->m_game;
                int mapSize = map ? map->size : 0;
                field_28 = (((int)footprint->tileY1 >> 1) + (int)footprint->tileY0) * mapSize +
                           (((int)footprint->tileX1 >> 1) + (int)footprint->tileX0);
            }
        }
        // 0x140003050 = FindComponent<WorkComponent>, on the entity re-read from *param_2.
        field_18 = (*entityRef)->FindComponent<WorkComponent>();
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
        System::MemoryAllocator::MemFree(node);
    }
}

// Reimpl of FUN_14000e320 (TreeNodeFree) — children at +0x18 and +0x20.
void WorkBase_TreeNodeFree_e320(void* root, void* node) {
    if (node) {
        WorkBase_TreeNodeFree_e320(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x18));
        WorkBase_TreeNodeFree_e320(root, *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(node) + 0x20));
        System::MemoryAllocator::MemFree(node);
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
            // 0x1400c5444: RCX = cur only.
            reinterpret_cast<Action*>(cur)->CheckEntity();   // B.3.x bridge (slot 8)
            cur = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(cur) + 0x10);  // node[2] = next
        } while (cur != nullptr);
        // Slot address recomputed from the CURRENT array pointer (may have moved).
        slot = p[0] + static_cast<uintptr_t>(i) * 8;
        reinterpret_cast< ::Object::InternalBasePtr*>(reinterpret_cast<void*>(slot))->DeletePtr(
            );   // B.3.3 bridge
        uintptr_t nul = 0;
        new (&nul) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *reinterpret_cast<uintptr_t*>(slot) = nul;
    }
    *reinterpret_cast<int*>(base + 0x18) = -1;
    if (p[2] != 0) {
        reinterpret_cast< ::Object::InternalBasePtr*>(&p[2])->DeletePtr(
            );   // B.3.3 bridge
        uintptr_t nul = 0;
        new (&nul) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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
    IO::Serializer ser(*reinterpret_cast<IO::Stream*>(stream), 3);   // Persist's stream is void*

    // LEFT: +0x10 / +0x18 are InternalPtrs retail serializes through
    // InternalBasePtr::Serialize; the members are declared T* (their readers in
    // other blocks spell them so), so the Serialize view is a cast.
    reinterpret_cast<Object::InternalBasePtr*>(&field_10)->Serialize(ser);
    if (field_10) {
        if (!Object::IsDerivedFromNullSafe(field_10, ComponentEntity::TypeId()))
            field_10 = nullptr;
    }
    reinterpret_cast<Object::InternalBasePtr*>(&field_18)->Serialize(ser);
    if (field_18) {
        if (!Object::IsDerivedFromNullSafe(field_18, WorkComponent::TypeId()))
            field_18 = nullptr;
    }
    ser.Serialize(field_20, nullptr);
    ser.Serialize(field_28, nullptr);
    int ver = ser.GetVersion();
    if (ver < 1)
        field_24 = field_20;
    else
        ser.Serialize(field_24, nullptr);
    if (ser.GetVersion() > 2) {
        field_30.Serialize(ser);
        field_38.Serialize(ser);
        ser.Serialize(field_40, nullptr);
    }
}   // ~Serializer after the last field, as retail

// 0x1400f7100 (see Work.h). RCX = this (+0x20 read), RDX never read, R8 = the
// citizen InternalPtr, R9 = the optional out slot.
bool Work::ValidateTask(void* /*unusedRdx*/, CitizenComponent** citizen, CompleteTask** out) {
    if (*citizen == nullptr) return false;
    // DWORD_1401c7158 = CitizenComponent (ENDGAME B.3.9 D5: by cell).
    if (!(*citizen)->IsA(CitizenComponent::TypeId())) return false;

    CharacterComponent* character = (*citizen)->character;
    // 0x1401c5e50 (CompleteTask's own cell) is UNREGISTERED -- the old raw-.rdata read
    // always failed and fell through to this same CompleteTask::TypeId() call, so
    // calling it directly is not a behaviour change (ENDGAME B.3.9 D5 note).
    uint32_t completeTaskTid = CompleteTask::TypeId();
    // The bound (+0x60 + 1) is re-read every pass, as retail (0x1400f7204).
    for (int i = 0; i < character->currentAction + 1; i++) {
        Action* action = character->actions[i];
        // The tid arm is the tree's guard; retail calls IsDerivedFrom directly.
        CompleteTask* task = (completeTaskTid && Object::IsDerivedFromNullSafe(action, completeTaskTid))
                                 ? static_cast<CompleteTask*>(action) : nullptr;
        if (task != nullptr && task->taskIndex == field_20) {
            if (out) *out = task;
            return true;
        }
    }
    return false;
}

// B4 FORWARD: 20 outside callers (the Work leaves' Validate slots in WORK-a / WORK-b
// files) still hand the receiver, the citizen reference and the out slot untyped.
uint8_t WorkBase_ValidateTask_f7100(void* self, void* unusedRdx, void* taskIBP, void** out) {
    return reinterpret_cast<Work*>(self)->ValidateTask(unusedRdx, reinterpret_cast<CitizenComponent**>(taskIBP),
                                                      reinterpret_cast<CompleteTask**>(out));
}

// ============================================================
// Map distance / connectivity helpers
// ============================================================

// Reimpl of FUN_140008830 (Map_CalcDistance) — pathfinding-aware cell distance.
// Tries the cached path cost (Map_CalcPathCost, 0x14009f810); on no-path (<0) falls
// back to Euclidean grid distance (sqrtf) + gridWidth penalty. FUN_140132010 in the
// original is sqrtf (verified). Caller: int fn(int* mapGrid, uint32_t cellA, uint32_t cellB)
int WorkBase_CalcDistance_8830(int* mapGrid, uint32_t cellA, uint32_t cellB) {
    int cost = Pf_IsDstReachable_f810(mapGrid, cellA, cellB, 1);
    if (cost < 0) {
        int gridW = *mapGrid;
        int dRow = static_cast<int>(cellA) / gridW - static_cast<int>(cellB) / gridW;
        int dCol = static_cast<int>(cellA) % gridW - static_cast<int>(cellB) % gridW;
        cost = static_cast<int>(sqrtf(static_cast<float>(dRow * dRow + dCol * dCol))) + gridW;
    }
    return cost;
}

// FUN_14009c480 — connectivity/zone refresh for the (pathType, destCell, srcCell) query:
// Pf_ZoneRefresh_9c480 (Action/exe_bodies.h), called by name with all four arguments.

// Reimpl of FUN_1400a00f0 (Map::CanPathTo / WorldGrid_ValidateCell) — are two cells
// mutually reachable? Refreshes connectivity (FUN_14009c480), then ORs the two cells'
// pass-type bits with pathType; if both cell types survive the mask it returns 1 when a
// road/overlay bit (0x1e0) is set or when both cells share a non-zero region (uint16 @
// +0x26). Cell records: array at grid+0x80, stride 0x30, pass byte +0x29.
// Caller: bool fn(int* grid, uint32_t pathType, uint32_t destCell, int srcCell)
//   (low byte of the returned uint64 is the bool; the WorldGrid_ValidateCell int-variant
//    callers read the low 32 bits).
uint64_t WorkBase_ValidateCell_a00f0(int* mapGrid, uint32_t pathType, uint32_t destCell, int srcCell) {
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

    Pf_ZoneRefresh_9c480(mapGrid);   // 0x14009c480 (RCX only)

    // LEFT: the outside callers hand the Map as its leading `int size`.
    Map* map = reinterpret_cast<Map*>(mapGrid);
    const MapCell& recSrc  = map->cells[static_cast<uint32_t>(srcCell)];
    const MapCell& recDest = map->cells[destCell];

    uint32_t typeSrc  = 1u << (recSrc.terrainType & 0x1f);
    uint32_t typeDest = 1u << (recDest.terrainType & 0x1f);
    uint32_t combined = typeDest | typeSrc;
    uint64_t ret = combined;

    uint32_t mask = pathType | combined;
    if ((mask & typeDest) && (mask & typeSrc)) {
        if (mask & 0x1e0u) return (ret & 0xffffffffffffff00ull) | 1u;
        uint16_t regionDest = recDest.zone;
        if (regionDest != 0) {
            uint16_t regionSrc = recSrc.zone;
            ret = regionSrc;
            if (regionSrc != 0 && regionDest == regionSrc)
                return (ret & 0xffffffffffffff00ull) | 1u;
        }
    }
    return ret & 0xffffffffffffff00ull;
}
