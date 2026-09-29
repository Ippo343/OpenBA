// code/Game/Component/resourceproduce_creatework.cpp
// Component vtable slot 18 (+0x90) — CreateWork — for the three food-production
// ResourceProduceComponent subclasses.
//
// Slot 18 is `_purecall` on the ResourceProduceComponent base vtable (0x1401643f0),
// i.e. pure virtual: every concrete subclass MUST override it. The shared state
// machine's state-13 enter (0x14012ae60) calls it to mint the building's Work object
// and then posts that work to the citizen reservation ledger. With the components
// inert, this was never reached — which is exactly why FishingWork / HuntWork /
// HerbalistWork are all fully ported and registered yet nothing ever ran them.
//
// Retail bodies:
//   FishingComponent   0x14011efe0 — MemAlloc(0x70) + FishingWork_Ctor(0x1400eaac0)
//   HuntComponent      0x1401226a0 — MemAlloc(0x58) + Work_Ctor25 + vtable_HuntWork
//                                    + zero {+0x48 q, +0x50 d}
//   HerbalistComponent 0x14004b160 — MemAlloc(0x60) + Work_Ctor25 + vtable_HerbalistWork
//                                    + zero {+0x48 q, +0x50 d, +0x58 d}
// All three end with InternalBasePtr_Ctor(out, obj) and return `out`.
//
// ⚠ Ghidra labels 0x14004b160 "HerbalistWork_Ctor". It is NOT a ctor — it is
// HerbalistComponent's slot 18. The real HerbalistWork ctor is elsewhere. Same
// class of misnomer as "HerbalistComponent_Slot13" (which is really the shared base's).

#include "Component/coll_bridge.h"
#include "Component/ConsumeProduceDescription.h"
#include "object/TypeInfo.h"
#include "system/MemoryAllocator.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "Work/FishingWork.h"      // ENDGAME B.3.8 MI-B: the real Work classes
#include "Work/HuntWork.h"
#include "Work/HerbalistWork.h"
#include "Work/ForesterWork.h"
#include "Work/MarketWork.h"
#include "Work/ConsumeProduce.h"
#include "Component/MarketComponent.h"
#include <new>
#include "Action/exe_bodies.h"   // Pf_RecursiveChildSum_123070
#include "Component/FishingComponent.h"
#include "Component/HuntComponent.h"
#include "Component/HerbalistComponent.h"
#include "Component/ForesterComponent.h"
#include "Component/ConsumeProduceComponent.h"
#include "Component/InteractComponent.h"
#include "Component/StateMachineComponent.h"
#include "Component/ComponentDescription.h"
#include "Entity/ComponentEntity.h"

// ENDGAME B.3.8 MI-B: each work is now built as its REAL class (code/Game/Work/<X>.h):
// `new (mem) X()` runs the base init and the leaf's field zeroing, calls the work's
// *_ResolveFuncs (the reason the hand builds had to call it themselves: a work posted
// with null helper pointers died the first time the scheduler asked it anything), and
// the compiler stamps the class's own table; then Work::InitFromEntity is the body of
// the entity-seeded base ctor 0x1400f72e0 (Work_Ctor25). Same bytes as the hand build,
// with no read-back of the Work vtable addresses (0x140161320/620/5a0/3a0/710/220).
namespace {

// The exe helpers:
//   0x140002ed0 FindComponent<InteractComponent> — Entity_FindComponent_Position, the
//               finder FishingWork_Ctor actually uses. (It is a known misnomer: it
//               resolves the InteractComponent, not a "position" component. Kept faithful
//               to retail's choice rather than "corrected".)
//   0x140123070 Pf_RecursiveChildSum_123070 — child attach-point count of a kind
//   0x140003ea0 FindComponent<StateMachineComponent>; only ForesterComponent's slot 18
//               needs it (to read the live state id before re-arming the two work flags).

template <typename W>
W* AllocWork(int size) {
    return static_cast<W*>(System::MemoryAllocator::MemAlloc(size, 8));
}

// The tail shared by all of them: hand the object back as an InternalBasePtr.
uintptr_t* WrapIBP(uintptr_t* out, Work* obj) {
    ::new (out) ::Object::InternalPtr<Work>(obj);
    return out;
}

}  // namespace

// The bodies below are the classes' slot-18 members' bodies; each class's
// CreateWork (in its own file, another block's) calls its C name here. The receiver
// casts are B4 FORWARD receivers: the C names take void*.

// ── FishingComponent::CreateWork — retail 0x14011efe0 ─────────────────────────────
// The work is 0x70 bytes and its ctor (0x1400eaac0) sizes TWO arrays off the dock's
// child attach points: citizenRefs grows to the count of kind-0 children, spotStates
// resizes to the count of kind-2 children and is then zero-filled. Those are the
// fishing spots the citizen cycles through, so an unsized pair means a fisherman with
// nowhere to stand.
extern "C" uintptr_t* FishingComponent_CreateWork(void* selfV, uintptr_t* out) {
    FishingComponent* self = reinterpret_cast<FishingComponent*>(selfV);   // B4 FORWARD receiver
    FishingWork* work = AllocWork<FishingWork>(0x70);
    if (!work) return WrapIBP(out, nullptr);

    ComponentEntity* entity = self->_entity;

    // +0x48..+0x68 zeroed by the ctor (citizenRefs / spotStates / retryCount).
    new (work) FishingWork();
    work->InitFromEntity(&entity);

    {
        InteractComponent* pos = entity->FindComponent<InteractComponent>();   // 0x140002ed0
        if (pos) {
            const int n0 = Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(pos), 0);
            const int n2 = Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(pos), 2);
            // B.3.2 bridge: 0x1400032e0 is Array<void*>::SetSize, 0x1400021d0 is Array<int32>::SetSize
            // (retail FishingWork_Ctor calls them at 1400eab83 / 1400eab8e). I32NoInit:
            // 0x1400021d0 leaves a grown tail as-found. Safe — the zero loop right below
            // (retail 1400eaba0) writes all the spot slots before any is read.
            GameColl::AsArray<GameColl::Ptr8>(&work->citizenRefs).SetSize(n0);
            GameColl::AsArray<GameColl::I32NoInit>(&work->spotStates).SetSize(n2);
            // Zero the kind-2 slot array the ctor allocates (retail's trailing loop).
            const int cnt = work->spotCount;
            int32_t* slots = work->spotStates;
            if (slots) for (int i = 0; i < cnt; ++i) slots[i] = 0;
        }
    }
    return WrapIBP(out, work);
}

// ── HuntComponent::CreateWork — retail 0x1401226a0 ────────────────────────────────
// Retail writes vtable_AutoPickupWork then immediately vtable_HuntWork over it (a
// compiler artefact of the two-level ctor); only the final store is observable.
extern "C" uintptr_t* HuntComponent_CreateWork(void* selfV, uintptr_t* out) {
    HuntComponent* self = reinterpret_cast<HuntComponent*>(selfV);   // B4 FORWARD receiver
    HuntWork* work = AllocWork<HuntWork>(0x58);
    if (!work) return WrapIBP(out, nullptr);

    ComponentEntity* entity = self->_entity;

    // The ctor zeroes the leaf fields (+0x48 onward) as retail's tail does.
    new (work) HuntWork();
    work->InitFromEntity(&entity);

    return WrapIBP(out, work);
}

// ── HerbalistComponent::CreateWork — retail 0x14004b160 ───────────────────────────
// This is the Gatherer's Hut work (GathererShelter.rsc carries HerbalistDescription).
extern "C" uintptr_t* HerbalistComponent_CreateWork(void* selfV, uintptr_t* out) {
    HerbalistComponent* self = reinterpret_cast<HerbalistComponent*>(selfV);   // B4 FORWARD receiver
    HerbalistWork* work = AllocWork<HerbalistWork>(0x60);
    if (!work) return WrapIBP(out, nullptr);

    ComponentEntity* entity = self->_entity;

    // The ctor zeroes the leaf fields (+0x48 onward) as retail's tail does.
    new (work) HerbalistWork();
    work->InitFromEntity(&entity);

    return WrapIBP(out, work);
}

// ── ForesterComponent::CreateWork — retail 0x14011f780 ────────────────────────────
// Ghidra names this "ForesterWork_CopyCtor". It is neither a copy ctor nor part of
// ForesterWork: it is ForesterComponent's slot 18, the same override every other
// ResourceProduceComponent subclass supplies. The forester lodge's work object is
// 0x60 bytes, same shape as HerbalistWork's (an AutoPickupWork subclass: retail
// writes vtable_AutoPickupWork and then vtable_ForesterWork over it, and only the
// second store is observable).
//
// ⚠ The one thing that is genuinely Forester-specific is the PROLOGUE, and it is not
// bookkeeping — it re-arms the lodge's two work flags:
//
//     sm = FindComponent_StateMachine(entity)
//     st = sm[+0x52]; if (st < 0) st = sm[+0x50]
//     if (st != 14 && !plantTrees && !cutTrees) plantTrees = cutTrees = true
//
// i.e. "if we are NOT in the paused state and the player has turned BOTH plant and
// cut off, turn both back on". Retail relies on it because a lodge whose two flags
// are both clear has nothing to do, yet state 13 (producing) was entered anyway —
// this is the repair that keeps such a lodge from minting a work that can never run.
// sm+0x52 is the pending state id and sm+0x50 the current one; the sign test is
// retail's "no pending transition" check.
extern "C" uintptr_t* ForesterComponent_CreateWork(void* selfV, uintptr_t* out) {
    ForesterComponent* self = reinterpret_cast<ForesterComponent*>(selfV);   // B4 FORWARD receiver
    ComponentEntity* entity = self->_entity;

    {
        StateMachineComponent* sm = entity->FindComponent<StateMachineComponent>();   // 0x140003ea0
        char st = sm->stateMachine.pendingState;
        if (st < 0) st = sm->stateMachine.currentInput;
        if (st != 14 && !self->plantTrees && !self->cutTrees) {
            // retail: one word store of 0x101 over +0x50 / +0x51
            self->plantTrees = true;
            self->cutTrees = true;
        }
    }

    ForesterWork* work = AllocWork<ForesterWork>(0x60);
    if (!work) return WrapIBP(out, nullptr);

    // The ctor zeroes +0x48/+0x50/+0x58 and runs ForesterWork_ResolveFuncs.
    new (work) ForesterWork();
    work->InitFromEntity(&entity);

    return WrapIBP(out, work);
}

// ── MarketComponent::CreateWork — retail 0x14004ea40 ─────────────────────────
// ⚠ Ghidra labels 0x14004ea40 "MarketWork_Ctor". It is NOT the MarketWork ctor — it
// is MarketComponent's slot 18, the same override every ResourceProduceComponent
// subclass supplies. Exactly the misnomer flagged at the top of this file for
// HerbalistComponent (0x14004b160 "HerbalistWork_Ctor").
//
// Byte-for-byte the same shape as HuntComponent's: MemAlloc(0x58) + Work_Ctor25 over
// the owner entity + vtable_MarketWork + zero {+0x48 qword, +0x50 dword}.
// There is NO Forester-style prologue in the retail body — no state-machine read, no
// flag re-arm — so none is added here.
//
// A live probe measured this slot still holding the inert stub, i.e. no MarketWork was
// ever minted: state 13's enter (0x14012ae60) calls slot 18 to create the work and post
// it to the citizen reservation ledger, so a stubbed slot silently means "market never
// offers a job".
uintptr_t* MarketComponent::CreateWork(uintptr_t* out) {
    MarketWork* work = AllocWork<MarketWork>(0x58);
    if (!work) return WrapIBP(out, nullptr);

    ComponentEntity* entity = _entity;

    // The ctor zeroes +0x48/+0x50 and runs MarketWork_ResolveFuncs.
    new (work) MarketWork();
    work->InitFromEntity(&entity);

    return WrapIBP(out, work);
}

// ── ConsumeProduceComponent::CreateWork — retail 0x140118d00 ──────────────────
// The SIXTH member of this family, and the one that was still missing: the tavern
// (and every other consume→produce workshop) had slot 18 holding the inert stub, so
// state 13's enter never minted a ConsumeProduce work and the building never brewed.
// Measured: a four-minute staffed-tavern soak logged ZERO "[ConsumeProduce]" lines —
// not even ResolveFuncs, which the factory path prints — proving the work class was
// never instantiated at all, while the panel showed the tavern staffed 1 of 1.
//
// This one differs from its five siblings in that it reads two authored ints out of
// the building's ConsumeProduceDescription and passes them to a purpose-built work
// ctor (0x1400e9200), rather than zeroing a couple of fields inline:
//
//   entity   = the owner, CheckPtr'd first
//   descRoot = entity->_description             -- the entity DESCRIPTION, not the
//                                                  component array. Retail searches the
//                                                  description's CHILD list here.
//   cpDesc   = first child whose GetTypeId == ConsumeProduceDescription (0x1401c7388)
//   work     = ConsumeProduceAction_Ctor(MemAlloc(0x78,8), &entity,
//                                        &cpDesc->_toolType, cpDesc->_i3c)
//
// ⚠ retail dereferences cpDesc UNGUARDED after the loop — with no matching child it
// reads [0x38] off a null and faults. That deref is left faithful: a ConsumeProduce
// building always carries its own description, and a guard here would silently mint a
// work with garbage recipe ints instead of restoring the behaviour.
//
// ⚠ the second int is genuinely an INT, not a float: retail loads it with
// `MOV R9D,dword ptr [RBX+0x3c]` — the integer register, not XMM3. Checked
// deliberately because a float arg declared int lands in the wrong register.

// ── ConsumeProduceAction ctor — retail 0x1400e9200 ────────────────────────────
// Not the plain 0x1400e9340 ctor the Work FACTORY uses: this is the 4-argument one
// slot 18 calls, which seeds the recipe fields and sizes the two work arrays off the
// building's child attach points. Same childSum(0)/childSum(2) shape as FishingWork's
// ctor, but the arrays live at +0x58 (IBP array, grown) and +0x68 (int array, resized
// then zero-filled with the count at +0x70).
static ConsumeProduce* ConsumeProduceAction_Ctor(ConsumeProduce* work, ComponentEntity** entity,
                                                 const int* productIdx, int second) {
    if (!work) return nullptr;

    // The REAL ConsumeProduce (Work/ConsumeProduce.h): base init, its field zeroing
    // and ConsumeProduce_ResolveFuncs; then Work_Ctor25's body; then the 4-arg ctor's
    // own seeding below (the same stores the hand build wrote).
    new (work) ConsumeProduce();
    work->InitFromEntity(entity);

    work->field_48 = 0;
    work->field_4c = 0;
    work->field_50 = productIdx ? *productIdx : 0;
    work->field_54 = second;
    work->field_58 = nullptr;
    work->field_60 = 0;
    work->field_64 = 0;
    work->field_68 = nullptr;
    work->field_70 = 0;
    work->field_74 = 0;

    {
        InteractComponent* pos = (*entity)->FindComponent<InteractComponent>();   // 0x140002ed0
        if (pos) {
            const int n0 = Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(pos), 0);
            const int n2 = Pf_RecursiveChildSum_123070(reinterpret_cast<int64_t>(pos), 2);
            // B.3.2 bridge: 0x1400032e0 is Array<void*>::SetSize, 0x1400021d0 is Array<int32>::SetSize
            // (retail ConsumeProduceAction_Ctor calls them at 1400e92d8 / 1400e92e3).
            // I32NoInit: 0x1400021d0 leaves a grown tail as-found. Safe — the zero loop
            // right below (retail 1400e9300) writes all the slots before any is read.
            GameColl::AsArray<GameColl::Ptr8>(&work->field_58).SetSize(n0);
            GameColl::AsArray<GameColl::I32NoInit>(&work->field_68).SetSize(n2);
            const int cnt = work->field_70;
            int32_t* slots = work->field_68;
            if (slots) for (int i = 0; i < cnt; ++i) slots[i] = 0;
        }
    }
    return work;
}

// LEFT: declared nowhere else (consumeproduce_recipe_dump.cpp, COMP-c).
extern "C" void ConsumeProduce_DumpRecipes(void* cpDesc, int selected);

extern "C" uintptr_t* ConsumeProduceComponent_CreateWork(void* selfV, uintptr_t* out) {
    ConsumeProduceComponent* self = reinterpret_cast<ConsumeProduceComponent*>(selfV);   // B4 FORWARD receiver

    ComponentEntity* entity = self->_entity;
    if (!entity) return WrapIBP(out, nullptr);

    // The entity's description (not the component array — the search below is over
    // DESCRIPTIONS).
    const ComponentDescription* descRoot = entity->_description;
    ConsumeProduceDescription* cpDesc = nullptr;
    if (descRoot) {
        const uint32_t want = ConsumeProduceDescription::TypeId();   // cell 0x1401c7388 (D5);
                                                                      // TypeId_ConsumeProduceComponent
                                                                      // is a documented misnomer.
        const int    n   = descRoot->_subDescCount;
        Description** const arr = descRoot->_subDescs;
        for (int i = 0; i < n && arr; ++i) {
            Description* c = arr[i];
            if (!c) continue;
            if (c->Type() == want) {
                if (Object::IsDerivedFromNullSafe(c, want))
                    cpDesc = static_cast<ConsumeProduceDescription*>(c);
                break;
            }
        }
    }

    ConsumeProduce* mem = AllocWork<ConsumeProduce>(0x78);
    ConsumeProduce* obj = nullptr;
    if (mem) {
        // Faithful unguarded description read — see the note above.
        const int first  = static_cast<int32_t>(cpDesc->_toolType);
        const int second = cpDesc->_i3c;
        obj = ConsumeProduceAction_Ctor(mem, &entity, &first, second);
    }


    // Dump the recipe table once. "No materials" on the panel is ambiguous between
    // "the selected recipe eats something the town has none of" and "hauling never
    // delivers"; the table plus the live selection separates them.
    ConsumeProduce_DumpRecipes(cpDesc, self->consumeResourceId);

    static int n = 0;
    if (n < 4) { ++n;
        fprintf(stderr, "[ConsumeProduce] CreateWork self=%p entity=%p cpDesc=%p "
                        "work=%p\n", selfV, static_cast<void*>(entity),
                        static_cast<void*>(cpDesc), static_cast<void*>(obj));
        fflush(stderr);
    }
    return WrapIBP(out, obj);
}
