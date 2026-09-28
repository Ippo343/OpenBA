// code/Game/Work/work_leaf_gather.cpp
// Wave (Game-gap / Work-leaf) — gather/forester/storage leaf helpers.
//
// Faithful 1:1 ports of exe sub-helpers referenced via ResolveAddr(Module::Exe,
// 0xADDR) from code/Game/Work/{gatherrawmaterials,foresterwork}.cpp. They resolve
// to 0 (silently skipped) unless registered. This file provides the closure-safe subset only.
//
// CLOSURE-SAFETY: each function below was confirmed (snapshot notes/_reg_snapshot.txt)
// to call ONLY already-registered exe sub-callees, sibling ports, or engine/runtime
// dispatch slots that resolve at runtime (CheckPtr 0x140152ab0, IsDerivedFrom
// 0x140152a98). Registering a half-null helper is silent corruption, strictly worse
// than the safe no-op of leaving it unregistered.
//
// The intricate pathfinding/copy-ctor helpers in the same batch (0x1400bf790,
// 0x1400ba9f0, 0x1400881d0, 0x1400beae0) were DEFERRED for the same reason.
//
// ⚠️ STALE-LIST CORRECTION (2026-07-16). This header used to list 0x1400f2f00 as
// deferred behind an unregistered closure of "0x1400c29a0, 0x140042890, 0x140088730,
// 0x14008d3c0, 0x14002c070, 0x1400c0ad0, 0x140115e60, 0x1400f6ef0 ...". That list has
// been overtaken by events and was actively misleading:
//   • 0x1400c29a0, 0x140042890, 0x140115e60, 0x1400f6ef0 — since REGISTERED
//     (Phase 2 action child-ctor waves / work_base + work_walkpos batches).
//   • 0x1400f2f00 itself — since ported (code/Game/Work/pickup.cpp, the real
//     Pickup::Execute; Ghidra mislabels it Pickup_Serialize).
//   • 0x14008d3c0 (+ 0x14012d990, 0x14012d560) — since ported in
//     code/Game/Work/pickup_gate.cpp.
//   • 0x1400c0ad0 (+ 0x1400c9b70) — since ported in
//     code/Game/Work/work_base_actionctor.cpp.
//   • 0x14002c070 (+ 0x14002c380) — deliberately NOT registered: it is MSVC's
//     inlined std::sort over Pickup's 32-byte candidate records (ascending on the
//     distance int32 at +0x08). Reimplemented locally as Pickup_SortByDistance in
//     code/Game/Work/pickup.cpp.
// Still genuinely unregistered from that old list: 0x1400c7cd0 / 0x1400ca520.
// Verify with a fresh grep over standalone/ before trusting any address list in a
// comment — including this one.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "Component/GrowthDescription.h"   // ENDGAME B.3.9 D5: GrowthDescription::TypeId()

// ============================================================
// 0x140116540 — gatherrawmaterials.cpp helper
//   Free-storage-capacity query: returns (storage capacity field +0x38) minus the
//   current queried count. ABI: int fn(longlong self).
//   self+0x10 = an InternalBasePtr to the owning entity.
// Sub-callees (all registered / engine):
//   0x140006270 Entity_FindComponent_Storage      [reg]
//   0x14012cf10 StorageComponent_QueryCount        [reg]
// ============================================================
namespace {
typedef void* (*FindCompStorage_t)(intptr_t entity, void* retIBP);
typedef int   (*StorageQueryCount_t)(void* storageComp);

FindCompStorage_t   s_FindCompStorage_6270 = nullptr;
StorageQueryCount_t s_StorageQueryCount_cf10 = nullptr;
bool                s_init_116540 = false;

void Ensure_116540() {
    if (s_init_116540) return;
    s_FindCompStorage_6270   = reinterpret_cast<FindCompStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_StorageQueryCount_cf10 = reinterpret_cast<StorageQueryCount_t>(ResolveAddr(Module::Exe, 0x14012cf10));
    s_init_116540 = true;
}
} // namespace

int WorkLeaf_FreeStorageCapacity_116540(intptr_t self) {
    Ensure_116540();
    void* ibpStorage[4] = { nullptr, nullptr, nullptr, nullptr };

    // CheckPtr(self+0x10); FindComponent_Storage(*(self+0x10), &ibpStorage)
    if (!s_FindCompStorage_6270) return 0;
    s_FindCompStorage_6270(*reinterpret_cast<intptr_t*>(self + 0x10), ibpStorage);

    // CheckPtr(&ibpStorage); iVar1 = *(int*)(storage + 0x38)
    void* storage = ibpStorage[0];
    if (!storage) return 0;
    int capacity = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(storage) + 0x38);

    // CheckPtr(&ibpStorage); iVar2 = StorageComponent_QueryCount(storage)
    int count = s_StorageQueryCount_cf10 ? s_StorageQueryCount_cf10(storage) : 0;

    return capacity - count;
}

// ============================================================
// 0x14000c3c0 — foresterwork.cpp helper (ForesterWork_helper_c3c0)
//   FindComponent by type-ID DAT_1401c77e8 (GrowthDescription / NatResDesc2) over a
//   component list. Container layout: count at entity+0x30, ptr-array base at
//   entity+0x28 (NO CheckPtr calls — same shape as WorkAcc_FindComponent_6330).
//   ABI: void* fn(longlong entity, void* retOut). Writes match (or null) to *retOut.
// Sub-callees:
//   GetTypeId via component vtable +0x10                [engine indirect]
//   slot 0x140152a98 IsDerivedFrom                      [engine dispatch]
//   DAT_1401c77e8 type-ID global                        [runtime-filled by RegisterType]
// ============================================================
void* WorkLeaf_FindGrowthDescription_c3c0(intptr_t entity, void* retOut) {
    const uint32_t typeId = GrowthDescription::TypeId();  // 0x1401c77e8
    void* found = nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x30);
    uintptr_t arrBase = *reinterpret_cast<uintptr_t*>(entity + 0x28);
    for (int i = 0; i < n; i++) {
        void* comp = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
        // GetTypeId is component vtable slot +0x10.
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.x bridge (slot 2)
        if (t == typeId) {
            void* c = *reinterpret_cast<void**>(arrBase + static_cast<int64_t>(i) * 8);
            found = (fn_IsDerivedFrom && fn_IsDerivedFrom(c, typeId)) ? c : nullptr;
            break;
        }
    }
    *reinterpret_cast<void**>(retOut) = found;
    return retOut;
}

// ============================================================
// 0x140120c30 — foresterwork.cpp helper (FUN_140120c30)
//   Footprint occupancy/ownership test over a 2x2 cell window of the map cell grid.
//   For each cell (col-1..col, row-1..row, excluding the (0,row==base) corner case
//   matching the original guard) it counts:
//     iVar11 = total non-null cells touched
//     iVar8  = cells whose resolved owner (FUN_14000c460 -> +0x18, IsDerivedFrom
//              DAT_1401c77e8 filter) equals param_1 (the queried work base)
//   Returns false (0) if iVar8 > *(int*)(self+0x5c) OR iVar11 > *(int*)(self+0x58),
//   else true (1).
//   ABI: longlong fn(Base* self, int col, int row).
//   Cell grid: grid = *(int**)((*g_pGameWorldSingleton) + 0xf8); mapW = *grid;
//     cellArrBase = *(void**)((uintptr_t)grid + 0x80);
//     cell = *(void**)(cellArrBase + (mapW*row + col)*0x30 + 0x8)
// Sub-callees:
//   0x14000c460 FUN_14000c460 (owner-resolve)          [reg]
//   slot 0x140152a98 IsDerivedFrom                     [engine dispatch]
//   g_pGameWorldSingleton (DAT_1401c3dd8)              [runtime global]
//   DAT_1401c77e8 type-ID global                        [runtime-filled]
// ============================================================
namespace {
typedef void* (*OwnerResolve_t)(intptr_t cell, void* retIBP);
OwnerResolve_t  s_OwnerResolve_c460 = nullptr;
bool            s_init_120c30 = false;
void Ensure_120c30() {
    if (s_init_120c30) return;
    s_OwnerResolve_c460 = reinterpret_cast<OwnerResolve_t>(ResolveAddr(Module::Exe, 0x14000c460));
    s_init_120c30 = true;
}
} // namespace

long long WorkLeaf_FootprintOwnerCheck_120c30(intptr_t self, int col, int row) {
    Ensure_120c30();
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return 1;

    // grid = *(int**)((*g_pGameWorldSingleton) + 0xf8); mapW = *grid
    int* grid = *reinterpret_cast<int**>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0xf8);
    if (!grid) return 1;
    int mapW = *grid;

    int touched = 0;   // iVar11
    int owned   = 0;   // iVar8

    // Original loop window: rowIdx from (row-1) .. (row), colIdx from (col-1) .. (col),
    // skipping the cell where (colDelta==0 && rowIdx==row) — i.e. the (col, row) corner
    // when colDelta is 0. Faithful port of the nested do/while with bound guards.
    for (long long rowIdx = static_cast<long long>(row) - 1;
         rowIdx - row < 2; rowIdx++) {
        for (long long colDelta = -1; colDelta < 2; colDelta++) {
            // ((colDelta != 0) || (rowIdx != row)) && bounds-check
            if ((colDelta != 0) || (rowIdx != row)) {
                long long c = static_cast<long long>(col) + colDelta;
                if (c >= 0 && c < mapW && rowIdx >= 0 && rowIdx < mapW) {
                    void* cell = *reinterpret_cast<void**>(
                        *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80)
                        + (static_cast<long long>(mapW) * rowIdx + c) * 0x30 + 0x8);
                    if (cell != nullptr) {
                        touched++;
                        void* outIBP = nullptr;
                        if (s_OwnerResolve_c460) s_OwnerResolve_c460(reinterpret_cast<intptr_t>(cell), &outIBP);
                        if (outIBP != nullptr) {
                            void* obj = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(outIBP) + 0x18);
                            void* owner = nullptr;
                            if (fn_IsDerivedFrom && fn_IsDerivedFrom(obj, GrowthDescription::TypeId()))
                                owner = obj;
                            if (owner == reinterpret_cast<void*>(self))
                                owned++;
                        }
                    }
                }
            }
        }
    }

    if (owned   > *reinterpret_cast<int*>(self + 0x5c) ||
        touched > *reinterpret_cast<int*>(self + 0x58))
        return 0;
    return 1;
}
