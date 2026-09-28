// code/Game/Work/pickup_gate.cpp
// Pickup "does the town want this resource?" gate — faithful 1:1 ports of
// 0x14008d3c0 + its two helpers 0x14012d990 and 0x14012d560.
//
// 0x14008d3c0 is called from Pickup::Execute (0x1400f2f00) BEFORE anything else
// happens; if it answers false the whole pickup is abandoned ("no work available").
#include "Work/exe_bodies.h"
// It walks the town manager's storage-location list and asks, for each entry:
// "does this storage location accept this resource category, and can it physically
// take this amount?".
//
// Type-id cells (ENDGAME B.3.9 D5: read BY CELL through the class's own TypeId(),
// names recovered from their RegisterType DATA writes via Ghidra xrefs):
//   0x1401c7ee8 = StorageLocationComponent    (StorageLocationComponent_RegisterType) -- registered
//   0x1401c7f20 = StorageLocationDescription  (StorageLocationDescription_Register)  -- UNREGISTERED
//   0x1401c7e78 = StorageComponent            -- registered
//   0x1401c7cb8 = RawMaterialDescription      -- registered
//
// Sub-callees (all already registered): 0x140006270 Entity_FindComponent_Storage,
// 0x14012cd80 StorageComponent_impl_cd80 (CanAdd). Plus engine dispatch slots
// 0x140152a90 IBP ctor / 0x140152a98 IsDerivedFrom.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include "engine_funcs.h"
#include "Action/exe_bodies.h"      // B3R: 0x14012cd80
#include "object/Internal.h"
#include <new>
#include <cstdint>
#include "Object/factory_registration.h"
#include "Component/StorageLocationDescription.h"   // ENDGAME B.3.8 CB3: StorageLocationDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/StorageLocationComponent.h"   // ENDGAME B.3.8 CB3: StorageLocationComponent::TypeId()
#include "Component/StorageComponent.h"   // ENDGAME B.3.8 CB3: StorageComponent::TypeId()

namespace {

// B3R: slot 0x140152a90 is ??0InternalBasePtr (placement new below), slot 0x140152a98
// is the raw member Object::Base::IsDerivedFrom; 0x140006270 and 0x14012cd80 are
// called by name.

// ⚠️ THE TWO SEARCHES BELOW WALK DIFFERENT STRUCTURES WITH DIFFERENT LAYOUTS. They look
// interchangeable and are NOT — sharing one helper crashed twice, once per call site:
//   8d3c0 searches a ComponentEntity's COMPONENTS:      array @ +0x30, count @ +0x38
//   12d990 searches a description object's DESCRIPTIONS: array @ +0x28, count @ +0x30
// (12d990's param_2 is `*(targetEntity + 0x28)` — the entity's description/template, not
// the entity — and it looks for RawMaterialDESCRIPTION, not a component.) Both verified in
// their own disassembly. Using the wrong pair reads the low half of the array POINTER as
// the count and iterates a garbage base => ACCESS_VIOLATION. Do not merge these.

// 8d3c0's search: ComponentEntity components — array @ +0x30, count @ +0x38.
// (`if (0 < *(int *)(lVar2 + 0x38))` then `*(longlong *)(lVar2 + 0x30) + lVar10 * 8`;
// matches cleararea.cpp Phase 3 and FUN_1400c2ea0.) Per-slot CheckPtr, as the original.
void* PG_FindCompByTid(uintptr_t entity, uint32_t tid) {
    int count = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        void* comp = *slot;
        if (!comp) continue;
        if (reinterpret_cast<Object::Base*>(comp)->Type() != tid) continue;   // B.3.8 bridge
        void* c = *slot;
        return reinterpret_cast<const Object::Base*>(c)->IsDerivedFrom(tid) ? c : nullptr;
    }
    return nullptr;
}

// 12d990's search: description list — array @ +0x28, count @ +0x30. Faithful to the
// original's shape: NO per-slot CheckPtr, and it BREAKS on the first type-id match
// (returning null when that hit fails IsDerivedFrom, rather than scanning on).
void* PG_FindDescByTid(uintptr_t descObj, uint32_t tid) {
    int count = *reinterpret_cast<int*>(descObj + 0x30);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(descObj + 0x28);
    for (int i = 0; i < count; ++i) {
        void* d = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        if (!d) continue;
        if (reinterpret_cast<Object::Base*>(d)->Type() != tid) continue;   // B.3.8 bridge
        void* c = *reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);
        return reinterpret_cast<const Object::Base*>(c)->IsDerivedFrom(tid) ? c : nullptr;
    }
    return nullptr;
}

} // namespace

// ============================================================
// Pickup_helper_d560 (0x14012d560) — accept-mask of a StorageLocationComponent.
//   m = (desc @ self+0x18, as StorageLocationDescription) + 0x20
//   if (m == 0) m = (StorageComponent of *(self+0x10)) + 0x3c
// NOTE: the original dereferences the IsDerivedFrom result unguarded (+0x20) and the
// found StorageComponent unguarded (+0x3c). Kept faithful — do not add null guards
// without re-deriving why the original is safe here.
// ============================================================
int PickupGate_AcceptMask_12d560(int64_t self) {
    uintptr_t s = static_cast<uintptr_t>(self);

    void* obj = *reinterpret_cast<void**>(s + 0x18);
    // 0x1401c7f20 (StorageLocationDescription's own cell) is UNREGISTERED -- the old
    // PG_Tid raw-.rdata read always failed and fell through to this same
    // StorageLocationDescription::TypeId() call, so calling it directly is not a
    // behaviour change (ENDGAME B.3.9 D5 note).
    uint32_t tidDesc = StorageLocationDescription::TypeId();
    void* desc = reinterpret_cast<const Object::Base*>(obj)->IsDerivedFrom(tidDesc) ? obj : nullptr;

    int mask = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x20);
    if (mask == 0) {
        void* storIBP = nullptr;
        void* ret = WorkBase_FindComponent_6270(*reinterpret_cast<int64_t*>(s + 0x10), &storIBP);
        mask = *reinterpret_cast<int*>(*reinterpret_cast<uintptr_t*>(ret) + 0x3c);
    }
    return mask;
}

// ============================================================
// Pickup_helper_d990 (0x14012d990)
//   "Does this storage location's accept-mask cover this resource's category?"
//     resCat = (RawMaterialDescription component of resEntity) + 0x5c
//     return (resCat & AcceptMask(slComp)) != 0
// NOTE: the original dereferences the RawMaterialDescription result unguarded (+0x5c)
// when the component is absent. Kept faithful.
// ============================================================
uint64_t PickupGate_Accepts_12d990(int64_t slComp, int64_t resEntity) {

    uint32_t tidRawDsc = RawMaterialDescription::TypeId();
    // NOTE: description-list layout (+0x28/+0x30), NOT the component layout — see the
    // warning above PG_FindCompByTid. `resEntity` here is the entity's description object.
    void* rawDesc = PG_FindDescByTid(static_cast<uintptr_t>(resEntity), tidRawDsc);

    uint32_t resCat = *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(rawDesc) + 0x5c);
    uint32_t mask   = static_cast<uint32_t>(PickupGate_AcceptMask_12d560(slComp));
    return ((resCat & mask) != 0) ? 1u : 0u;
}

// ============================================================
// Pickup_helper_d3c0 (0x14008d3c0) — THE gate.
//   param_1 = town manager (*(gw+0xf0)); field[0] = array ptr, field[1] = count (int)
//   param_2 = the resource description entity
//   param_3 = amount
// For each storage-location entity in the list: find its StorageLocationComponent and
// its StorageComponent, then require BOTH d990 (accepts this category) and
// StorageComponent_impl_cd80 (can physically take `amount`). Returns 1 on the first
// match, 0 if none — including when the list is empty.
//
// Measured 2026-07-16: list0.count == 1 in standalone at game start (the Barn), so this
// loop does run; the "empty list" hypothesis was ruled out by probe, not assumed.
// ============================================================
uint64_t PickupGate_TownWants_8d3c0(int64_t townMgr, int64_t resEntity, int amount) {
    uintptr_t tm = static_cast<uintptr_t>(townMgr);

    // `if (0 < (int)param_1[1])` — count is read as the low 32 bits of field[1].
    int count = static_cast<int>(*reinterpret_cast<int64_t*>(tm + 0x08));
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(tm + 0x00);

    uint32_t tidSLComp = StorageLocationComponent::TypeId();
    uint32_t tidStor   = StorageComponent::TypeId();

    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);

        uintptr_t ent = *reinterpret_cast<uintptr_t*>(slot);
        if (!ent) continue;
        void* slComp = PG_FindCompByTid(ent, tidSLComp);
        void* slIbp = nullptr;
        new (&slIbp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(slComp));

        // The original re-reads the list entry (and re-CheckPtrs) for the second scan.
        ent = *reinterpret_cast<uintptr_t*>(slot);
        if (!ent) continue;
        void* stComp = PG_FindCompByTid(ent, tidStor);
        void* stIbp = nullptr;
        new (&stIbp) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(stComp));

        if (PickupGate_Accepts_12d990(reinterpret_cast<int64_t>(slIbp), resEntity)) {
            if (static_cast<char>(Pf_StorageCanAccept_cd80(reinterpret_cast<int64_t>(stIbp), resEntity, amount)))   // the door read AL
                return 1u;
        }
    }
    return 0u;
}
