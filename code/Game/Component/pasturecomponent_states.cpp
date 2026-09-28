// code/Game/Component/pasturecomponent_states.cpp
// Animal-spawn wave / pasture lane — the PastureComponent CONTROLLER state machine.
//
// The pasture's behaviour is not in its own Update: PastureComponent is a
// ControllerComponent whose embedded state node (at comp+0x48, back-pointer at
// comp+0x60) is linked and ticked by the entity's StateMachineComponent. The state
// table is built once by PastureDescription_helper_81a0 (0x1401281a0):
//
//   sub_6940(2, helper_70c0, .., sub_67e0)   -> state 2  enter=70c0  perFrame=67e0
//   sub_6a60(3)                              ->   state 2 transitions -> {3}
//   sub_6940(3, helper_6e40, .., 0)          -> state 3  enter=6e40  (no perFrame)
//   sub_6a60(6) sub_6a60(4) sub_6a60(5)      ->   state 3 transitions -> {6,4,5}
//
// sub_6940 writes the record's +0x00 id, +0x08 fnA and +0x18 fnB; the node vtable's
// slot-3 dispatcher (0x140036bf0) tail-calls entry+0x08 and the slot-7 dispatcher
// (0x140036cb0) tail-calls entry+0x18 — i.e. fnA is ENTER and fnB is the PER-FRAME
// handler, exactly the enterHandler / trans38Handler fields of ControllerLive's
// StateTable. sub_6a60 appends {id,0} to the last record's transition list.
//
// The controller's GetDefaultState (primary vtable slot 16, 0x1400026c0) is
// `MOV EAX,2; RET` — the pasture's DEFAULT state is 2, the operational one, so a
// built pasture starts producing as soon as its state machine starts. State 3 is
// the teardown (release every animal back to the world's free-roaming pool and
// unregister the pasture).
//
// ⚠ Return-AL discipline: both ENTER handlers end `& 0xffffffffffffff00` (AL = 0,
// the "walk-stop" tail that lets sibling state nodes still run); the per-frame
// handler returns AL = 1. Ported verbatim — returning 1 from an enter here would
// starve every node behind the pasture.
//
// Faithful offset-by-offset port. Opt-in via BANISHED_LIVE_PASTURE (see the live
// factory in pasturecomponent.cpp); needs BANISHED_LIVE_STATEMACHINE so the node is
// actually linked and ticked.

#include "Component/controller_state_table.h"
#include "Component/PastureComponent.h"
#include "Component/AnimalComponent.h"
#include "Component/LivestockDescription.h"
#include "Component/RawMaterialComponent.h"
#include "Component/AutoPickupComponent.h"   // ENDGAME B.3.9 CB: 0x1401c3fb0 cell class
#include <new>
#include "object/TypeInfo.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"
#include "Object/factory_registration.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "system/Value.h"   // ENDGAME B.3.4b System caller batch
#include "Component/exe_bodies.h"
#include "Action/exe_bodies.h"
#include "Work/exe_bodies.h"
#include "Entity/componententity_funcs.h"   // ComponentEntity_CtorFromDescription, EntityList_RegisterEntity_Impl
#include "Game/exe_constants.h"

// Spawn side (pasturecomponent_spawn.cpp).
extern "C" void Pasture_SpawnLivestock(void* pasture, void* parentAnimalIBP);

// ============================================================
// Helpers
// ============================================================
// The exe helpers this file calls, by retail address:
//   0x140002cc0 CemeteryListErase_Reimpl (DynArray8_EraseByValue)
//   0x140003110 ActHunt_DynArraySetCount_3110 (DynArray8_ResizeFill; count+1 appends)
//   0x140004680 FloorComponent_FindResData    0x140005500 ActHunt_FindComponent_5500
//   0x140002ed0 WorkBase_FindComponent_2ed0   0x140002e10 AttendanceComponent_GetJobHostComponent
//   0x1400c3470 ActHunt_NotifyEntityChanged_c3470
//   0x140105ed0 PastureComponent_Helper5ed0   0x1401240a0 PastureComponent_ReassignEntityRef
//   0x140128570 PastureComponent_ComputeCapacity
//   0x14012fc00 WorkPlaceComponent_SetAccepting
//   0x14009f770 MapGrid_StampCellRefs_9f770 (below)
//   0x140123070 Pf_RecursiveChildSum_123070 (footprint cell count)
//   0x140122e20 Pf_EntityResolvePos_2e20     (footprint cell position)
//   0x140006330 WorkAcc_FindComponent_6330   (RawMaterialDescription on a template)
//   0x140129760 ActRm_PositionEntity_9760
//   0x140013320 ActRm_FindComp_013320 (TrackerComponent finder; NOT a storage finder,
//               the fifth instance of the "misnamed component finders" trap)
//   0x140058fd0 ActRm_ArrayUpsert_58fd0 (the tracker's {template,count} upsert, which
//               alone feeds the production-statistics panel)
//   0x140088f90 ActRm_GetResourceCategory_88f90
//   0x140003050 WorkBase_FindComponent_3050   0x14012eb50 WorkComponent_RegisterWorkSlot
//   0x14012ec60 CitizenAI_WorkScheduler_Post  0x140044810 ActRm_DynArrayPush4_44810
//   0x1400c48a0 EntityList_RegisterEntity_Impl
//   0x1400c3720 ComponentEntity_CtorFromDescription
//   .rdata 0x140172204 = 90.0f, the quarter-turn the dropped pile is rotated by.
namespace {

uint32_t s_tidAnimalComp   = 0;   // 0x1401c6f98  AnimalComponent
uint32_t s_tidLivestockDsc = 0;   // 0x1401c79a8  LivestockDescription
uint32_t s_tidRawMatComp   = 0;   // 0x1401c7c80  RawMaterialComponent

bool s_resolved = false;

void Resolve() {
    if (s_resolved) return;
    s_resolved = true;
    s_tidAnimalComp   = AnimalComponent::TypeId();       // cell 0x1401c6f98 (D5)
    s_tidLivestockDsc = LivestockDescription::TypeId();  // cell 0x1401c79a8 (D5)
    s_tidRawMatComp   = RawMaterialComponent::TypeId();  // cell 0x1401c7c80 (D5)
}


inline uint64_t MakeIBP(void* obj) {
    uint64_t r = 0;
    new (&r) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(obj));   // B.3.3 bridge
    return r;
}
inline uintptr_t World() {
    return (GameUpdateLoop::_singleton != nullptr)
             ? reinterpret_cast<uintptr_t>(GameUpdateLoop::_singleton) : 0;
}
inline uintptr_t GameState() {
    uintptr_t w = World();
    return w ? *reinterpret_cast<uintptr_t*>(w + 0xf0) : 0;
}

// Pasture field accessors (raw offsets, matching the retail decompile).
inline void*&   PastureType(void* p)   { return *reinterpret_cast<void**>((uintptr_t)p + 0x28); }
inline uintptr_t PastureAnimals(void* p){ return *reinterpret_cast<uintptr_t*>((uintptr_t)p + 0x30); }
inline int&     PastureCount(void* p)  { return *reinterpret_cast<int*>((uintptr_t)p + 0x38); }
inline int&     PastureCap(void* p)    { return *reinterpret_cast<int*>((uintptr_t)p + 0x40); }
inline int&     PastureAccum(void* p)  { return *reinterpret_cast<int*>((uintptr_t)p + 0x44); }
inline void*    PastureOwner(void* p)  { return *reinterpret_cast<void**>((uintptr_t)p + 0x10); }

// The component find-loop retail inlines everywhere: walk the entity's component
// IBP array (entity+0x30, count entity+0x38), compare vtable+0x10 (GetTypeId) to
// `tid`, then confirm with IsDerivedFrom.
void* FindCompOnEntity(uintptr_t entity, uint32_t tid) {
    if (!entity || !tid) return nullptr;
    int n = *reinterpret_cast<int*>(entity + 0x38);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(entity + 0x30);
    if (!arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        void** slot = reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        void* comp = *slot;
        if (!comp) continue;
        uint32_t t = reinterpret_cast<Object::Base*>(comp)->Type();   // B.3.8 bridge
        if (t != tid) continue;
        return (Object::IsDerivedFromNullSafe(comp, tid)) ? comp : nullptr;
    }
    return nullptr;
}

// Same loop over a TEMPLATE/description sub-block list (+0x28 array, +0x30 count).
void* FindDescOnTemplate(uintptr_t tmpl, uint32_t tid) {
    if (!tmpl || !tid) return nullptr;
    int n = *reinterpret_cast<int*>(tmpl + 0x30);
    uintptr_t arr = *reinterpret_cast<uintptr_t*>(tmpl + 0x28);
    if (!arr) return nullptr;
    for (int i = 0; i < n; ++i) {
        void* d = *reinterpret_cast<void**>(arr + (uintptr_t)i * 8);
        if (!d) continue;
        uint32_t t = reinterpret_cast<Object::Base*>(d)->Type();   // B.3.8 bridge
        if (t != tid) continue;
        return (Object::IsDerivedFromNullSafe(d, tid)) ? d : nullptr;
    }
    return nullptr;
}

}  // namespace

// ============================================================
// PastureComponent_Slot14_sub_3cc0 (0x140123cc0) — bind ONE livestock component
// into `pastureIBP` (or, with a null target, release it into the free-roaming pool).
//
//   if (animal->pasture == target) return;                 // already there
//   erase the animal from its current owner list (the world free list at
//       gameState+0x80 when unowned, else oldPasture+0x30) and, when it HAD a
//       pasture, re-run AnimalComponent_helper_5ed0 so its tile/bounds are recomputed
//   animal->pasture = target
//   append it to the new owner list (target+0x30, or the world free list) and, when
//       it now HAS a pasture, FUN_1401240a0 to copy the pasture rect onto the animal
// ============================================================
extern "C" void Pasture_BindAnimal(void* animalComp, void* pastureIBP) {
    Resolve();
    if (!animalComp || !pastureIBP) return;
    uintptr_t a = reinterpret_cast<uintptr_t>(animalComp);
    void* target = *reinterpret_cast<void**>(pastureIBP);
    void* cur = *reinterpret_cast<void**>(a + 0x28);
    if (cur == target) return;

    uintptr_t gs = GameState();
    uint64_t selfIBP = MakeIBP(animalComp);
    if (cur == nullptr) {
        if (gs) CemeteryListErase_Reimpl(reinterpret_cast<void*>(gs + 0x80), &selfIBP);
    } else {
        CemeteryListErase_Reimpl(reinterpret_cast<void*>((uintptr_t)cur + 0x30), &selfIBP);
        uint64_t animIBP = 0;
        ActHunt_FindComponent_5500(*reinterpret_cast<uintptr_t*>(a + 0x10), &animIBP);
        if (animIBP) PastureComponent_Helper5ed0(reinterpret_cast<void*>(animIBP));
    }

    *reinterpret_cast<void**>(a + 0x28) = target;
    uint64_t againIBP = MakeIBP(animalComp);
    if (target) {
        uintptr_t t = reinterpret_cast<uintptr_t>(target);
        ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(t + 0x30),
                                      *reinterpret_cast<int*>(t + 0x38) + 1, &againIBP);
        PastureComponent_ReassignEntityRef(a);
    } else if (gs) {
        ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(gs + 0x80),
                                      *reinterpret_cast<int*>(gs + 0x88) + 1, &againIBP);
    }
}

// ============================================================
// PastureDescription_helper_sub_7140 (0x140127140) — ADOPT free-roaming livestock.
//
// Runs on the pasture's state-2 enter and every state-2 frame. If the pasture is
// under capacity, it scans the world's free-roaming livestock list (gameState+0x80,
// count +0x88) for animals whose ENTITY was cloned from the same prototype as the
// pasture's assigned type (entity+0x28 == pasture+0x28) and binds them in, stopping
// as soon as the pasture is full.
//
// Retail snapshots every match into a heap array before binding, because binding
// erases from the very list it is walking. We snapshot the same way into a fixed
// buffer, stopping the collection once we already hold more candidates than the
// pasture can take — behaviourally identical, no allocation.
//
// This is the path that turns the "easy start" free-roaming cows/sheep/chickens
// (spawned by the start-config simple pool) into a stocked pasture.
// ============================================================
extern "C" void Pasture_AdoptWanderers(void* pasture) {
    Resolve();
    if (!pasture) return;
    uintptr_t gs = GameState();
    if (!gs) return;
    int freeCount = *reinterpret_cast<int*>(gs + 0x88);
    if (freeCount == 0) return;

    const int capacity = static_cast<int>(PastureComponent_ComputeCapacity((uintptr_t)pasture));
    if (PastureCount(pasture) >= capacity) return;

    void* wanted = PastureType(pasture);
    uintptr_t freeList = *reinterpret_cast<uintptr_t*>(gs + 0x80);
    if (!freeList) return;

    static const int kMaxCandidates = 64;
    void* candidates[kMaxCandidates];
    int nCand = 0;
    const int needed = capacity - PastureCount(pasture);
    for (int i = 0; i < freeCount && nCand < kMaxCandidates && nCand < needed; ++i) {
        uint64_t compIBP = *reinterpret_cast<uint64_t*>(freeList + (uintptr_t)i * 8);
        if (!compIBP) continue;
        uint64_t ownerIBP = *reinterpret_cast<uint64_t*>(compIBP + 0x10);
        if (!ownerIBP) continue;
        if (*reinterpret_cast<void**>(ownerIBP + 0x28) != wanted) continue;
        candidates[nCand++] = reinterpret_cast<void*>(compIBP);
    }
    if (nCand == 0) return;

    for (int i = 0; i < nCand; ++i) {
        uint64_t pastureIBP = MakeIBP(pasture);
        Pasture_BindAnimal(candidates[i], &pastureIBP);
        if (PastureCount(pasture) >= capacity) break;
    }
}

// ============================================================
// PastureDescription_helper_7370 (0x140127370) — REPRODUCTION.
//
// For each livestock in the pasture, find the AnimalComponent on its entity and test
// the breed bit (flags bit 21 / 0x200000, set by the animal's own behaviour update).
// When set: clear the bit (BTR 0x15), notify the entity of the flag change, and — if
// the pasture holds fewer than capacity+3 animals (retail's `LEA R13D,[RAX+3]`) —
// spawn a calf/lamb/chick with this animal as the parent.
// ============================================================
extern "C" void Pasture_Reproduce(void* pasture) {
    Resolve();
    if (!pasture) return;
    const int limit = static_cast<int>(PastureComponent_ComputeCapacity((uintptr_t)pasture)) + 3;

    for (int i = 0; i < PastureCount(pasture); ++i) {
        uintptr_t arr = PastureAnimals(pasture);
        if (!arr) return;
        uint64_t* slot = reinterpret_cast<uint64_t*>(arr + (uintptr_t)i * 8);
        if (!*slot) continue;
        uint64_t ownerIBP = *reinterpret_cast<uint64_t*>(*slot + 0x10);
        void* animal = FindCompOnEntity(static_cast<uintptr_t>(ownerIBP), s_tidAnimalComp);
        uint64_t animalIBP = MakeIBP(animal);
        if (!animalIBP) continue;

        uintptr_t an = static_cast<uintptr_t>(animalIBP);
        uint32_t oldFlags = *reinterpret_cast<uint32_t*>(an + 0x20);
        if ((oldFlags & 0x200000u) == 0) continue;          // not ready to breed

        uint32_t newFlags = oldFlags & ~0x200000u;
        *reinterpret_cast<uint32_t*>(an + 0x20) = newFlags;
        if (((newFlags ^ oldFlags) & 0xffff) != 0 && *reinterpret_cast<int64_t*>(an + 0x10) != 0) {
            ActHunt_NotifyEntityChanged_c3470(*reinterpret_cast<void**>(an + 0x10));
        }
        if (PastureCount(pasture) < limit) {
            std::fprintf(stderr, "[Pasture] BIRTH: pasture=%p parent=%p (count=%d limit=%d)\n",
                         pasture, animal, PastureCount(pasture), limit);
            std::fflush(stderr);
            uint64_t parent = animalIBP;
            Pasture_SpawnLivestock(pasture, &parent);
        }
    }
}

// ============================================================
// PastureComponent slot 15 (+0x78 "OnMessage", 0x140127720) — the BREED TAG.
//
// This is the missing link of livestock reproduction. The herder's work chain
// ends in a SendComponentMessage(msg=0) at the pasture (same dispatch the build
// labor credit uses, see buildcomponent.cpp Slot15). On each message the retail
// handler picks ONE RANDOM pastured animal and ORs flag bit 23 (0x800000) into
// its AnimalComponent — the "breed-eligible" mark. AnimalComponent::Update's
// offspring latch requires exactly that bit (plus +0x40 pose byte and adult
// age) before it will arm the offspring timer that sets bit 21, which
// Pasture_Reproduce then consumes to spawn a calf/lamb/chick. NOTHING else in
// the exe ever sets bit 23, so without this slot no livestock can ever breed —
// the reproduction rate is deliberately driven by herdsman labor.
//
// ABI matches BuildComponent's slot 15: (self RCX, int msgCode RDX, rest unused).
// The `(new ^ old) & 0xffff` notify can never fire here (bit 23 is above the
// mask); kept for fidelity like the identical dead test in helper_7400.
// ============================================================
void PastureComponent::OnMessage(int msg, void* /*data*/, uint32_t /*arg0*/, uint32_t /*arg1*/) {
    Resolve();
    if (msg != 0) return;
    const int count = PastureCount(this);
    if (count == 0) return;
    uintptr_t w = World();
    uintptr_t arr = PastureAnimals(this);
    if (!w || !arr) return;

    void* rng = *reinterpret_cast<void**>(w + 0xd8);
    uint32_t idx = reinterpret_cast<System::Random*>(rng)->Rand(static_cast<uint32_t>(count));   // B.3.4b bridge
    void** slot = reinterpret_cast<void**>(arr + (uintptr_t)idx * 8);
    if (!*slot) return;

    uint64_t entIBP = *reinterpret_cast<uint64_t*>((uintptr_t)*slot + 0x10);
    uint64_t animalIBP = 0;
    ActHunt_FindComponent_5500(static_cast<uintptr_t>(entIBP), &animalIBP);
    if (!animalIBP) return;

    uintptr_t an = static_cast<uintptr_t>(animalIBP);
    uint32_t oldF = *reinterpret_cast<uint32_t*>(an + 0x20);
    uint32_t newF = oldF | 0x800000u;
    *reinterpret_cast<uint32_t*>(an + 0x20) = newF;
    if (((newF ^ oldF) & 0xffffu) != 0 && *reinterpret_cast<int64_t*>(an + 0x10) != 0) {
        ActHunt_NotifyEntityChanged_c3470(*reinterpret_cast<void**>(an + 0x10));
    }
}

// 0x140127720 door: registered by address (standalone/reg_pasture_live.cpp) and
// called by name from standalone/pasture_harness.cpp.
extern "C" void Pasture_MsgReceive(void* pasture, int msg) {
    if (!pasture) return;
    reinterpret_cast<PastureComponent*>(pasture)->PastureComponent::OnMessage(msg, nullptr, 0, 0);   // B.3.8 bridge
}

// ============================================================
// Shared release body — PastureDescription_helper_6e40 (0x140126e40), byte-identical
// to the PastureComponent vtable-slot-7 body at 0x1401277e0.
//
// Walk the pasture's animal list from the BACK, unassigning each animal (pasture ptr
// -> null) so it returns to the world's free-roaming pool, then erase the pasture
// itself from the world pasture registry (gameState+0x90).
//
// ⚠ The loop terminates because the unassign path ERASES the animal from
// pasture+0x30, shrinking pasture+0x38. FUN_140002cc0 / FUN_140003110 are
// DynArray8_EraseByValue / DynArray8_ResizeFill — an earlier port of this function
// mislabelled them "AddToList"/"InsertInList", so nothing ever shrank the list and
// the `while (count >= 1)` spun forever.
// ============================================================
extern "C" void Pasture_ReleaseAllAnimals(void* pasture) {
    Resolve();
    if (!pasture) return;
    uintptr_t gs = GameState();

    int guard = 0;
    while (PastureCount(pasture) >= 1) {
        if (++guard > 4096) {                       // cannot happen in retail; see above
            std::fprintf(stderr, "[Pasture] release loop guard tripped on %p (count=%d)\n",
                         pasture, PastureCount(pasture));
            std::fflush(stderr);
            break;
        }
        uintptr_t arr = PastureAnimals(pasture);
        if (!arr) break;
        uint64_t* slot = reinterpret_cast<uint64_t*>(
            arr + (uintptr_t)(PastureCount(pasture) - 1) * 8);
        void* animal = reinterpret_cast<void*>(*slot);
        if (!animal) break;
        uintptr_t a = reinterpret_cast<uintptr_t>(animal);
        void* cur = *reinterpret_cast<void**>(a + 0x28);
        if (cur == nullptr) break;                  // already free: retail would spin here

        uint64_t ibp = MakeIBP(animal);
        CemeteryListErase_Reimpl(reinterpret_cast<void*>((uintptr_t)cur + 0x30), &ibp);
        void* animalComp = FindCompOnEntity(*reinterpret_cast<uintptr_t*>(a + 0x10),
                                            s_tidAnimalComp);
        uint64_t animalIBP = MakeIBP(animalComp);
        if (animalIBP) PastureComponent_Helper5ed0(reinterpret_cast<void*>(animalIBP));

        *reinterpret_cast<void**>(a + 0x28) = nullptr;
        uint64_t backIBP = MakeIBP(animal);
        if (gs)
            ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(gs + 0x80),
                                          *reinterpret_cast<int*>(gs + 0x88) + 1, &backIBP);
    }

    uint64_t selfIBP = MakeIBP(pasture);
    if (gs) CemeteryListErase_Reimpl(reinterpret_cast<void*>(gs + 0x90), &selfIBP);
}

// ============================================================
// State 2 ENTER — PastureDescription_helper_70c0 (0x1401270c0).
//   * adopt any matching free-roaming livestock (sub_7140)
//   * append this pasture to the world pasture registry (gameState+0x90/+0x98)
//   * find the owner's WorkPlaceComponent and set its active flag to 0
// Returns AL = 0 (`& 0xff..00`) so sibling state nodes still run.
// ============================================================
extern "C" char Pasture_State2Enter(void* data, void* /*sm*/) {
    Resolve();
    if (!data) return 0;
    std::fprintf(stderr, "[Pasture] state 2 ENTER (operational) pasture=%p type=%p\n",
                 data, PastureType(data));
    std::fflush(stderr);

    Pasture_AdoptWanderers(data);

    uintptr_t gs = GameState();
    uint64_t selfIBP = MakeIBP(data);
    if (gs)
        ActHunt_DynArraySetCount_3110(reinterpret_cast<void*>(gs + 0x90),
                                      *reinterpret_cast<int*>(gs + 0x98) + 1, &selfIBP);

    uint64_t wpc = 0;
    AttendanceComponent_GetJobHostComponent(*reinterpret_cast<void**>((uintptr_t)data + 0x10), &wpc);
    if (wpc) WorkPlaceComponent_SetAccepting(static_cast<int64_t>(wpc), false);

    return 0;
}

// ============================================================
// State 3 ENTER — PastureDescription_helper_6e40 (0x140126e40): teardown.
// Returns AL = 0.
// ============================================================
extern "C" char Pasture_State3Enter(void* data, void* /*sm*/) {
    if (!data) return 0;
    std::fprintf(stderr, "[Pasture] state 3 ENTER (teardown) pasture=%p count=%d\n",
                 data, PastureCount(data));
    std::fflush(stderr);
    Pasture_ReleaseAllAnimals(data);
    return 0;
}

// ============================================================
// The state-2 PRODUCTION tail of sub_67e0 — split out so a missing helper skips
// production without touching the adopt/reproduce head.
//
//   stamp the footprint cells with the assigned type (FUN_14009f770)
//   livestockDesc = LivestockDescription on the assigned template
//   produced      = livestockDesc+0x20 (the raw-material template it yields)
//   rawDesc       = RawMaterialDescription on `produced`  (FUN_140006330)
//   stock/limit   = gameState+0x170 table, entry stride 0x20, index livestockDesc+0x28
//   while (pasture+0x44 > rawDesc+0x40):
//       pasture+0x44 -= rawDesc+0x40
//       if (stock < limit) drop one pile of `produced` on a free footprint cell
//
// LANDED 2026-08-09: the FUN_140013320 -> CreateRawMaterial_helper_8fd0 tail is now
// called (see the loop below). It is the TrackerComponent upsert, not "storage
// bookkeeping" — it is what the production-statistics panel (ProductionUI) reads, so
// the panel was blank purely because this pair was never called. The pile-quantity
// write (rawComp+0x28 = perUnit) that retail does in the same statement group landed
// with it.
//
// DEFERRED (still not called): retail's
// cell path also RESERVES the target cell when it is occupied — FUN_1400223c0 to read
// the occupant, then tile flag byte +0x29 = 2 plus FUN_1400a8c90 / FUN_1400a6fe0 to
// republish it to the pathfinder — and pokes the work component through FUN_140003050
// -> FUN_140044810 (type 0x1401c3fb0). We only place on an ALREADY-free cell and skip
// that tail.
// ============================================================
namespace {


void PastureProduce(void* self) {
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* tmpl = PastureType(self);
    if (!tmpl) return;
    uintptr_t gs = GameState();
    uintptr_t w = World();
    if (!gs || !w) return;
    int* grid = *reinterpret_cast<int**>(w + 0xf8);
    if (!grid) return;

    uint64_t mapIBP = 0;
    FloorComponent_FindResData(*reinterpret_cast<void**>(s + 0x10), &mapIBP);
    if (!mapIBP) return;
    uintptr_t mc = static_cast<uintptr_t>(mapIBP);
    MapGrid_StampCellRefs_9f770(grid, *reinterpret_cast<int16_t*>(mc + 0x28),
                                *reinterpret_cast<int16_t*>(mc + 0x2a),
                                *reinterpret_cast<int16_t*>(mc + 0x2c),
                                *reinterpret_cast<int16_t*>(mc + 0x2e), tmpl);

    void* lsDesc = FindDescOnTemplate(reinterpret_cast<uintptr_t>(tmpl), s_tidLivestockDsc);
    if (!lsDesc) return;
    int64_t produced = *reinterpret_cast<int64_t*>((uintptr_t)lsDesc + 0x20);
    if (produced == 0) return;

    uint64_t rawDescIBP = 0;
    WorkAcc_FindComponent_6330(static_cast<uintptr_t>(produced), &rawDescIBP);
    if (!rawDescIBP) return;
    const int perUnit = *reinterpret_cast<int*>(static_cast<uintptr_t>(rawDescIBP) + 0x40);
    if (perUnit <= 0) return;

    uint64_t posIBP = 0;
    WorkBase_FindComponent_2ed0(*reinterpret_cast<uintptr_t*>(s + 0x10), &posIBP);

    uintptr_t stockTable = *reinterpret_cast<uintptr_t*>(gs + 0x170);
    const int resIdx = *reinterpret_cast<int*>((uintptr_t)lsDesc + 0x28);

    // ACCUMULATOR METER (BANISHED_PASTURE_TRACE=1). The production loop below is a
    // pure function of pasture+0x44 vs rawDesc+0x40; when the panel shows nothing the
    // first question is whether the accumulator is moving at all (it is credited by
    // LivestockComponent::Update, one tick per pastured animal per interval), not
    // whether the drop path works. Report both numbers, not just the outcome.

    int rounds = 0;
    while (PastureAccum(self) > perUnit && ++rounds <= 16) {
        PastureAccum(self) -= perUnit;
        if (!stockTable) { continue; }
        const int limit = *reinterpret_cast<int*>(stockTable + (uintptr_t)resIdx * 0x20 + 8);
        const int stock = *reinterpret_cast<int*>(stockTable + (uintptr_t)resIdx * 0x20);
        if (stock >= limit) { continue; }
        if (!posIBP) {
            continue;
        }

        // Find a footprint cell with no occupant and drop the produce pile there.
        const int nCells = Pf_RecursiveChildSum_123070(static_cast<int64_t>(posIBP), 2);
        int placedCell = -1;
        for (int c = 0; c < nCells && placedCell < 0; ++c) {
            float xy[3] = {0, 0, 0};
            Pf_EntityResolvePos_2e20(reinterpret_cast<void*>(posIBP), xy, 2, c, nullptr);
            const int dim = *grid;
            int gx = static_cast<int>(xy[0] + static_cast<float>(dim >> 1));
            int gy = static_cast<int>(xy[1] + static_cast<float>(dim >> 1));
            if (gx < 0) gx = 0;
            if (gx >= dim) gx = dim - 1;
            if (gy < 0) gy = 0;
            if (gy >= dim) gy = dim - 1;
            const int cell = dim * gy + gx;
            // grid is an int*; retail's `*(longlong*)(grid + 0x20)` is byte offset 0x80.
            uintptr_t tiles = *reinterpret_cast<uintptr_t*>(
                reinterpret_cast<uintptr_t>(grid) + 0x80);
            if (!tiles) break;
            uint64_t occ = *reinterpret_cast<uint64_t*>(tiles + (uintptr_t)cell * 0x30 + 8);
            if (occ == 0) { placedCell = cell; break; }
        }
        if (placedCell < 0) {
            // OCCUPANT DUMP: name what actually holds every candidate cell. cell+8 is
            // the ENTITY raster; a pasture whose own footprint claim (or its own
            // livestock) sits on every drop cell can never place a pile, and the panel
            // then reads identically to 'no production'.
            break;
        }

        void* mem = System::MemoryAllocator::MemAlloc(0x48, 8);
        void* ent = mem ? ComponentEntity_CtorFromDescription(
                              reinterpret_cast<ComponentEntity*>(mem), reinterpret_cast<void*>(produced))
                        : nullptr;
        if (!ent) break;
        uint64_t entIBP = MakeIBP(ent);
        void* rawComp = FindCompOnEntity(reinterpret_cast<uintptr_t>(ent), s_tidRawMatComp);
        if (rawComp) {
            // Position the pile on the cell with a random quarter-turn:
            // CreateRawMaterial_helper_9760(comp, cell, RandTol(rng,2) * 90.0f).
            const float quarter = kRdata_140172204;
            int roll = reinterpret_cast<System::Random*>(*reinterpret_cast<void**>(w + 0xd8))->RandTol(2);   // B.3.4b bridge
            ActRm_PositionEntity_9760(reinterpret_cast<int64_t>(rawComp), placedCell,
                                      static_cast<float>(roll) * quarter);
        }
        // Retail writes the pile's quantity onto the RawMaterialComponent right here
        // (0x1401267e0: `*(local_c8+0x28) = *(rawDesc+0x40); *(local_c8+0x30) = 0;`).
        // We were skipping it, so every pile we dropped carried quantity 0 — found
        // while porting the tracker tail below, NOT a pre-existing known gap.
        if (rawComp) {
            *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(rawComp) + 0x28) = perUnit;
            *reinterpret_cast<uint64_t*>(reinterpret_cast<uintptr_t>(rawComp) + 0x30) = 0;
        }

        // ---- retail tail: TrackerComponent upsert, BEFORE the town stock bump ----
        // 0x1401267e0:
        //     entity = *(self+0x10);
        //     if (entity) { FUN_140013320(entity,&trackerIBP);
        //                   if (trackerIBP) helper_8fd0(tracker, produced, perUnit); }
        //     idx = FUN_140088f90(<unused>, produced);  // stock bump below
        // This is what fills TrackerComponent+0x38 (the "produced" list) that
        // ProductionUI's panel reads, so without it the production page is blank even
        // when piles are appearing. Same three-call shape as the already-ported
        // ActRm_RegisterRawMaterial_88ef0 (0x140088ef0), which is a useful cross-check.
        int64_t pastureEntity = *reinterpret_cast<int64_t*>(s + 0x10);
        if (pastureEntity) {
            uint64_t trackerIBP = 0;
            ActRm_FindComp_013320(static_cast<uintptr_t>(pastureEntity), &trackerIBP);
            if (trackerIBP) {
                ActRm_ArrayUpsert_58fd0(static_cast<uintptr_t>(trackerIBP), produced, perUnit);
            }
        }

        EntityList_RegisterEntity_Impl(*reinterpret_cast<void**>(w + 0xe8), &entIBP,
                                       *reinterpret_cast<int*>(produced + 0x20));
        // NB (pre-existing, unchanged): retail calls RegisterEntity AFTER the stock
        // bump, we call it before. Left as-is rather than reordered under this task.
        // FUN_140088f90's first argument is unused by the callee
        // (ActRm_GetResourceCategory_88f90(void* /*unused*/, entity)), so retail's
        // register-shared `puVar21` — gameWorld, or the entity, or the tracker,
        // depending on which branch ran — is observationally irrelevant; passing the
        // world here is equivalent.
        {
            uint32_t idx = static_cast<uint32_t>(ActRm_GetResourceCategory_88f90(
                reinterpret_cast<void*>(w), static_cast<uintptr_t>(produced)));
            if (static_cast<int>(idx) >= 0 && stockTable) {
                int* pStock = reinterpret_cast<int*>(stockTable + (uintptr_t)(int)idx * 0x20 + 0x10);
                *pStock += perUnit;
            }
        }
        // ---- retail tail 0x140126d2e..0x140126dee (ENDGAME B.3.9 CB) ----
        // The pasture's AutoPickupComponent (an inlined FindComponent over
        // [0x1401c3fb0] at 0x140126d58 / 0x140126d89) records the new pile:
        // FUN_140003050 on the pile -> that component's +0x28, handed to
        // FUN_140044810 (which appends it only while the auto-pickup is enabled).
        // The tree had dropped this block (it only ever resolved the cell into an
        // unused variable, and the cell is unregistered).
        if (void* autoPickup = FindCompOnEntity(static_cast<uintptr_t>(pastureEntity),
                                                AutoPickupComponent::TypeId())) {
            uint64_t found = 0;
            void* compRef = WorkBase_FindComponent_3050(static_cast<uintptr_t>(entIBP), &found);
            const uint32_t v = *reinterpret_cast<uint32_t*>(
                *reinterpret_cast<uintptr_t*>(compRef) + 0x28);
            ActRm_DynArrayPush4_44810(reinterpret_cast<uintptr_t>(autoPickup), v);
        }

        std::fprintf(stderr, "[Pasture] produce: pasture=%p resource=%p cell=%d "
                     "(accum now %d)\n", self, (void*)produced, placedCell, PastureAccum(self));
        std::fflush(stderr);
    }
}

}  // namespace

// ============================================================
// State 2 PER-FRAME — PastureDescription_helper_81a0_sub_67e0 (0x1401267e0).
//   sub_7140 (adopt) -> helper_7370 (reproduce) -> production/slaughter tail.
// Returns AL = 1 (retail's `CONCAT71(..,1)`).
// ============================================================
extern "C" char Pasture_State2PerFrame(void* data, void* /*sm*/, void* /*time*/) {
    Resolve();
    if (!data) return 1;
    Pasture_AdoptWanderers(data);
    Pasture_Reproduce(data);
    PastureProduce(data);
    return 1;
}

// ============================================================
// Dispatcher + state table.
// ============================================================
// PastureCtrlTag lives in PastureComponent.h (the member type needs it).
using PastureDispatch = ::ControllerLive::Dispatcher<PastureCtrlTag>;


static void PastureCtrl_PopulateTable() {
    using namespace ::ControllerLive;
    StateTable& t = PastureDispatch::table;
    if (t.count != 0) return;                       // idempotent
    // state 2 (the controller's DEFAULT state): operational.
    //   enter = helper_70c0, per-frame (trans38) = sub_67e0, transitions -> {3}
    TableRegisterState(t, 2, reinterpret_cast<void*>(&Pasture_State2Enter),
                       nullptr, reinterpret_cast<void*>(&Pasture_State2PerFrame), nullptr);
    TableAddTransition(t, 3, nullptr);
    // state 3: teardown. enter = helper_6e40, no per-frame, transitions -> {6,4,5}
    TableRegisterState(t, 3, reinterpret_cast<void*>(&Pasture_State3Enter),
                       nullptr, nullptr, nullptr);
    TableAddTransition(t, 6, nullptr);
    TableAddTransition(t, 4, nullptr);
    TableAddTransition(t, 5, nullptr);
    std::fprintf(stderr, "[Pasture] state table populated: %d states (2=operational, "
                 "3=teardown)\n", t.count);
    std::fflush(stderr);
}

// PastureWork factory-with-entity (code/Game/Work/pasturework.cpp): MemAlloc(0x58,8)
// + the retail ctor 0x1400f2d80. `entityRefSlot` points at an 8-byte entity ref.
extern "C" void* PastureWork_CreateForEntity(void* entityRefSlot);

// ============================================================
// PastureComponent slot 13 (0x140127dd0) — capability query for the three
// pasture commands. Shared preamble sums the FREE capacity across every OTHER
// pasture of the same livestock type (world registry gameState+0x90/+0x98).
//
//   cmd 0 "empty the pasture":  our count fits elsewhere (0 < count <= free)
//   cmd 1 "split the herd":     count > 9 AND free >= 5
//   cmd 2 "toggle the herder":  bit 0 = has a livestock type (+0x28 != 0),
//                               bit 1 = workplace currently accepting (+0x50)
//
// The cmd 0/1 results are additionally masked by hasType (uVar5 in retail).
// Slot 14 dispatches this through the vtable and tests only bit 0.
// ============================================================
uint32_t PastureComponent::CommandMask(int cmd) {
    Resolve();

    int freeElsewhere = 0;
    uintptr_t gs = GameState();
    const int n = gs ? *reinterpret_cast<int*>(gs + 0x98) : 0;
    uintptr_t list = gs ? *reinterpret_cast<uintptr_t*>(gs + 0x90) : 0;
    for (int i = 0; i < n && list; ++i) {
        void* p = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!p || p == this) continue;
        if (PastureType(p) != PastureType(this)) continue;
        freeElsewhere += static_cast<int>(PastureComponent_ComputeCapacity((uintptr_t)p))
                         - PastureCount(p);
    }

    const uint64_t hasType = (PastureType(this) != nullptr) ? 1u : 0u;
    if (cmd == 0) {
        const int cnt = PastureCount(this);
        return (cnt > 0 && cnt <= freeElsewhere) ? hasType : 0;
    }
    if (cmd == 1) {
        if (PastureCount(this) > 9)
            return (freeElsewhere < 5) ? 0 : hasType;
        return 0;
    }
    if (cmd == 2) {
        uint64_t wpcIBP = 0;
        AttendanceComponent_GetJobHostComponent(*reinterpret_cast<void**>((uintptr_t)this + 0x10), &wpcIBP);
        uint64_t r = hasType;
        if (wpcIBP && *reinterpret_cast<char*>(wpcIBP + 0x50) != 0) r |= 2;
        return r;
    }
    return 0;
}

// 0x140127dd0 door: registered by address (standalone/reg_pasture_live.cpp).
extern "C" uint64_t Pasture_CanCommand(void* pasture, int cmd) {
    if (!pasture) return 0;
    return reinterpret_cast<PastureComponent*>(pasture)->PastureComponent::CommandMask(cmd);   // B.3.8 bridge
}

// ============================================================
// PastureComponent slot 14 (0x140127a60) — execute a pasture command.
// Guard: vtable slot 13 (above), bit 0. This is the slot the pasture dialog's
// SetLivestockType invokes with cmd 2 the first time a type is assigned — the
// ORGANIC start of the herder chain:
//
//   cmd 2: toggle the herder. Flip the owner entity's WorkPlaceComponent
//     accepting flag (Entity_impl_fc00). If it lands OFF: stop the entity's
//     work slot (FUN_14012eb50). If it lands ON: MemAlloc(0x58) + the retail
//     PastureWork ctor and post it to the work scheduler
//     (CitizenAI_impl_ec60, code=1) — a citizen then claims it and tends the
//     pasture, whose completed cycles send the slot-15 msg-0 breed tag.
//
//   cmd 0 / cmd 1: transfer animals to the other same-type pastures — all of
//     them (cmd 0) or floor(half) (cmd 1, keep = count - count/2). Fill the
//     pasture with the most free capacity first, then sweep the rest, moving
//     the FIRST animal of our slot array each step via Pasture_BindAnimal
//     (which erases it from our array, so the loop advances).
// ============================================================
void PastureComponent::Command(int cmd) {
    Resolve();

    const uint64_t can = CommandMask(cmd);   // slot 13, a virtual dispatch in retail
    if ((can & 1) == 0) {
        return;
    }

    if (cmd == 2) {
        uint8_t* selfB = reinterpret_cast<uint8_t*>(this);
        uint64_t wcIBP = 0, wpcIBP = 0;
        WorkBase_FindComponent_3050(*reinterpret_cast<uintptr_t*>(selfB + 0x10), &wcIBP);
        AttendanceComponent_GetJobHostComponent(*reinterpret_cast<void**>(selfB + 0x10), &wpcIBP);
        if (!wcIBP || !wpcIBP) {   // standalone guard; retail assumes both
            std::fprintf(stderr, "[Pasture] cmd 2: missing %s%s on %p — herder toggle skipped\n",
                         wcIBP ? "" : "WorkComponent ", wpcIBP ? "" : "WorkPlaceComponent",
                         this);
            std::fflush(stderr);
            return;
        }

        const char cur = *reinterpret_cast<char*>(wpcIBP + 0x50);
        WorkPlaceComponent_SetAccepting(static_cast<int64_t>(wpcIBP), cur == 0);
        if (*reinterpret_cast<char*>(wpcIBP + 0x50) == 0) {
            WorkComponent_RegisterWorkSlot(reinterpret_cast<void*>(wcIBP));   // 0x14012eb50
            WorkPlaceComponent_SetAccepting(static_cast<int64_t>(wpcIBP), false);
        } else {
            uint64_t entRef = *reinterpret_cast<uint64_t*>(selfB + 0x10);
            void* work = PastureWork_CreateForEntity(&entRef);
            uint64_t workIBP = MakeIBP(work);
            CitizenAI_WorkScheduler_Post(static_cast<uintptr_t>(wcIBP), 1, 1, &workIBP);
            WorkPlaceComponent_SetAccepting(static_cast<int64_t>(wpcIBP), true);
        }
        return;
    }

    // cmd 0 / cmd 1 — transfer animals out.
    uintptr_t gs = GameState();
    const int n = gs ? *reinterpret_cast<int*>(gs + 0x98) : 0;
    uintptr_t list = gs ? *reinterpret_cast<uintptr_t*>(gs + 0x90) : 0;
    const int keep = (cmd != 0)
        ? PastureCount(this) - (PastureCount(this) >> 1) : 0;

    void* best = nullptr;
    int bestFree = 0;
    for (int i = 0; i < n && list; ++i) {
        void* p = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!p || p == this) continue;
        if (PastureType(p) != PastureType(this)) continue;
        const int freeN = static_cast<int>(PastureComponent_ComputeCapacity((uintptr_t)p))
                          - PastureCount(p);
        if (freeN > bestFree) { bestFree = freeN; best = p; }
    }

    int moved = 0;
    auto transferInto = [&](void* dst) {
        while (keep < PastureCount(this)) {
            if (static_cast<int>(PastureComponent_ComputeCapacity((uintptr_t)dst))
                    <= PastureCount(dst)) break;
            uintptr_t arr = PastureAnimals(this);
            if (!arr) break;                        // standalone guard
            void** first = reinterpret_cast<void**>(arr);
            if (!*first) break;                     // standalone guard
            uint64_t dstRef = reinterpret_cast<uint64_t>(dst);
            Pasture_BindAnimal(*first, &dstRef);
            ++moved;
        }
    };
    if (best) transferInto(best);
    for (int i = 0; i < n && list; ++i) {
        void* p = *reinterpret_cast<void**>(list + (uintptr_t)i * 8);
        if (!p || p == this || p == best) continue;
        if (PastureType(p) != PastureType(this)) continue;
        transferInto(p);
    }
}

// 0x140127a60 door: registered by address (standalone/reg_pasture_live.cpp).
extern "C" void Pasture_Command(void* pasture, int cmd) {
    if (!pasture) return;
    reinterpret_cast<PastureComponent*>(pasture)->PastureComponent::Command(cmd);   // B.3.8 bridge
}

// ============================================================
// 0x14009f770 — map-grid cell owner-ref stamp (shared leaf).
// ------------------------------------------------------------
// Writes an 8-byte ref (the owning entity/template value the caller passes)
// into cell+0x10 for every cell of the (x,y,w,h) footprint rect. Cell array at
// mapGrid+0x80, stride 0x30. Retail callers: the pasture state-2 per-frame
// (sub_67e0 — stamps the pasture's cells so pathing/occupancy queries see it)
// and HarvestOrchard_impl_0800. Was resolved through the registry but never
// registered — the null guard made footprint stamping a silent no-op until
// 2026-07-30 (deferred item from the animal wave's Update 2).
// ============================================================
extern "C" uint64_t MapGrid_StampCellRefs_9f770(void* grid, int x, int y, int w, int h,
                                                void* ref) {
    if (!grid || w <= 0) return 0;
    const int dim = *reinterpret_cast<int*>(grid);
    uintptr_t cells = *reinterpret_cast<uintptr_t*>(reinterpret_cast<uintptr_t>(grid) + 0x80);
    if (!cells) return 0;
    for (int64_t dx = 0; dx < w; ++dx) {
        for (int64_t dy = 0; dy < h; ++dy) {
            const int64_t cell = (dy + y) * dim + x + dx;
            *reinterpret_cast<void**>(cells + 0x10 + cell * 0x30) = ref;
        }
    }
    return 0;
}

// Built ONCE at static-init time, as retail's PastureDescription setup does (B38 D6/D7).
// Before, every PastureComponent factory call re-ran it from both threads. The
// "[Pasture] state table populated" line now prints before standalone_error.log opens.
static const bool s_pastureStateTableBuilt = (PastureCtrl_PopulateTable(), true);
