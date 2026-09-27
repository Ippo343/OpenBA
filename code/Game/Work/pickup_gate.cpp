// code/Game/Work/pickup_gate.cpp
// Pickup "does the town want this resource?" gate — faithful 1:1 ports of
// 0x14008d3c0 + its two helpers 0x14012d990 and 0x14012d560.
//
// 0x14008d3c0 is called from Pickup::Execute (0x1400f2f00) BEFORE anything else
// happens; if it answers false the whole pickup is abandoned ("no work available").
// It walks the town manager's storage-location list and asks, for each entry:
// "does this storage location accept this resource category, and can it physically
// take this amount?".
//
// Type-id globals (raw .rdata resolves NULL in standalone — see
// reference_standalone_rdata_null_fallback — so each falls back to the runtime name
// lookup; names recovered from their RegisterType DATA writes via Ghidra xrefs):
//   0x1401c7ee8 = StorageLocationComponent    (StorageLocationComponent_RegisterType)
//   0x1401c7f20 = StorageLocationDescription  (StorageLocationDescription_Register)
//   0x1401c7e78 = StorageComponent
//   0x1401c7cb8 = RawMaterialDescription
//
// Sub-callees (all already registered): 0x140006270 Entity_FindComponent_Storage,
// 0x14012cd80 StorageComponent_impl_cd80 (CanAdd). Plus engine dispatch slots
// 0x140152a90 IBP ctor / 0x140152a98 IsDerivedFrom.
//
// Bodies are unconditional so this compiles as-is in the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include "Object/factory_registration.h"  // ::Object::GetTypeIdByName (SA type-id fallback)
#include "Component/StorageLocationDescription.h"   // ENDGAME B.3.8 CB3: StorageLocationDescription::TypeId()
#include "Component/RawMaterialDescription.h"   // ENDGAME B.3.8 CB3: RawMaterialDescription::TypeId()
#include "Component/StorageLocationComponent.h"   // ENDGAME B.3.8 CB3: StorageLocationComponent::TypeId()
#include "Component/StorageComponent.h"   // ENDGAME B.3.8 CB3: StorageComponent::TypeId()

namespace {

typedef void*    (*PG_IbpCtor_t)(void* ibp, void* obj);
typedef bool     (*PG_IsDerivedFrom_t)(void* obj, uint32_t tid);
typedef void*    (*PG_FindStorage_t)(int64_t entityInner, void* retIBP);      // 0x140006270
typedef char     (*PG_CanAdd_t)(void* storageComp, int64_t resEntity, int qty); // 0x14012cd80

PG_IbpCtor_t       s_pg_IbpCtor     = nullptr;
PG_IsDerivedFrom_t s_pg_IsDerived   = nullptr;
PG_FindStorage_t   s_pg_FindStorage = nullptr;
PG_CanAdd_t        s_pg_CanAdd      = nullptr;

const uint32_t* s_pg_pTid_SLComp = nullptr;  // 0x1401c7ee8
const uint32_t* s_pg_pTid_SLDesc = nullptr;  // 0x1401c7f20
const uint32_t* s_pg_pTid_Stor   = nullptr;  // 0x1401c7e78
const uint32_t* s_pg_pTid_RawDsc = nullptr;  // 0x1401c7cb8
bool s_pg_init = false;

void Ensure_PickupGate() {
    if (s_pg_init) return;
    s_pg_init = true;
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a90)) s_pg_IbpCtor   = *reinterpret_cast<PG_IbpCtor_t*>(s);
    if (uintptr_t s = ResolveAddr(Module::Exe, 0x140152a98)) s_pg_IsDerived = *reinterpret_cast<PG_IsDerivedFrom_t*>(s);
    if (!s_pg_IbpCtor)   s_pg_IbpCtor   = reinterpret_cast<PG_IbpCtor_t>(fn_InternalBasePtr_Ctor);
    if (!s_pg_IsDerived) s_pg_IsDerived = reinterpret_cast<PG_IsDerivedFrom_t>(fn_IsDerivedFrom);
    s_pg_FindStorage = reinterpret_cast<PG_FindStorage_t>(ResolveAddr(Module::Exe, 0x140006270));
    s_pg_CanAdd      = reinterpret_cast<PG_CanAdd_t>(ResolveAddr(Module::Exe, 0x14012cd80));
    s_pg_pTid_SLComp = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7ee8));
    s_pg_pTid_SLDesc = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7f20));
    s_pg_pTid_Stor   = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7e78));
    s_pg_pTid_RawDsc = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7cb8));
}

static uint32_t PG_Tid(const uint32_t* p, uint32_t classTypeId) {
    if (p && *p) return *p;
    return classTypeId;   // ENDGAME B.3.8 CB3: X::TypeId() (was the by-name lookup)
}

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
        return (s_pg_IsDerived && s_pg_IsDerived(c, tid)) ? c : nullptr;
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
        return (s_pg_IsDerived && s_pg_IsDerived(c, tid)) ? c : nullptr;
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
    Ensure_PickupGate();
    uintptr_t s = static_cast<uintptr_t>(self);

    void* obj = *reinterpret_cast<void**>(s + 0x18);
    uint32_t tidDesc = PG_Tid(s_pg_pTid_SLDesc, StorageLocationDescription::TypeId());
    void* desc = (s_pg_IsDerived && s_pg_IsDerived(obj, tidDesc)) ? obj : nullptr;

    int mask = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(desc) + 0x20);
    if (mask == 0) {
        void* storIBP = nullptr;
        void* ret = s_pg_FindStorage
                        ? s_pg_FindStorage(*reinterpret_cast<int64_t*>(s + 0x10), &storIBP)
                        : nullptr;
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
    Ensure_PickupGate();

    uint32_t tidRawDsc = PG_Tid(s_pg_pTid_RawDsc, RawMaterialDescription::TypeId());
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
    Ensure_PickupGate();
    uintptr_t tm = static_cast<uintptr_t>(townMgr);

    // `if (0 < (int)param_1[1])` — count is read as the low 32 bits of field[1].
    int count = static_cast<int>(*reinterpret_cast<int64_t*>(tm + 0x08));
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(tm + 0x00);

    uint32_t tidSLComp = PG_Tid(s_pg_pTid_SLComp, StorageLocationComponent::TypeId());
    uint32_t tidStor   = PG_Tid(s_pg_pTid_Stor, StorageComponent::TypeId());

    for (int i = 0; i < count; ++i) {
        void** slot = reinterpret_cast<void**>(arr + static_cast<uintptr_t>(i) * 8);

        uintptr_t ent = *reinterpret_cast<uintptr_t*>(slot);
        if (!ent) continue;
        void* slComp = PG_FindCompByTid(ent, tidSLComp);
        void* slIbp = nullptr;
        if (s_pg_IbpCtor) s_pg_IbpCtor(&slIbp, slComp);

        // The original re-reads the list entry (and re-CheckPtrs) for the second scan.
        ent = *reinterpret_cast<uintptr_t*>(slot);
        if (!ent) continue;
        void* stComp = PG_FindCompByTid(ent, tidStor);
        void* stIbp = nullptr;
        if (s_pg_IbpCtor) s_pg_IbpCtor(&stIbp, stComp);

        if (PickupGate_Accepts_12d990(reinterpret_cast<int64_t>(slIbp), resEntity)) {
            if (s_pg_CanAdd && s_pg_CanAdd(stIbp, resEntity, amount)) return 1u;
        }
    }
    return 0u;
}
