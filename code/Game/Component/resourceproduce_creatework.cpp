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
#include "engine_funcs.h"
#include "resolve.h"
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

// ENDGAME B.3.8 MI-B: each work is now built as its REAL class (code/Game/Work/<X>.h):
// `new (mem) X()` runs the base init and the leaf's field zeroing, calls the work's
// *_ResolveFuncs (the reason the hand builds had to call it themselves: a work posted
// with null helper pointers died the first time the scheduler asked it anything), and
// the compiler stamps the class's own table; then Work::InitFromEntity is the body of
// the entity-seeded base ctor 0x1400f72e0 (Work_Ctor25). Same bytes as the hand build,
// with no read-back of the Work vtable addresses (0x140161320/620/5a0/3a0/710/220).
namespace {

// 0x140002ed0 Entity_FindComponent_Position — the finder FishingWork_Ctor actually
// uses. (It is a known misnomer: it resolves the InteractComponent, not a "position"
// component. Kept faithful to retail's choice rather than "corrected".)
typedef void  (*fn_FindComp_t)(uintptr_t entity, uintptr_t* out);
typedef int   (*fn_ChildSum_t)(uintptr_t posComp, int kind);      // 0x140123070

fn_FindComp_t      s_findPos    = nullptr;
fn_ChildSum_t      s_childSum   = nullptr;
// 0x140003ea0 Entity_FindComponent_StateMachine — only ForesterComponent's slot 18
// needs it (to read the live state id before re-arming the two work flags).
fn_FindComp_t      s_findSM     = nullptr;
bool               s_resolved   = false;

void Resolve() {
    if (s_resolved) return;
    s_findPos    = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_childSum   = reinterpret_cast<fn_ChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_findSM     = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_resolved = true;
}

void* AllocWork(int size) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    return fn_MemAlloc(g_pMemAllocSingleton, size, 8);
}

// The tail shared by all three: hand the raw object back as an InternalBasePtr.
uintptr_t* WrapIBP(uintptr_t* out, void* obj) {
    if (fn_InternalBasePtr_Ctor) new (out) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(obj));   // B.3.3 bridge
    else *out = reinterpret_cast<uintptr_t>(obj);
    return out;
}

}  // namespace

// ── FishingComponent::CreateWork — retail 0x14011efe0 ─────────────────────────────
// The work is 0x70 bytes and its ctor (0x1400eaac0) sizes TWO arrays off the dock's
// child attach points: array at +0x48 grows to the count of kind-0 children, array at
// +0x58 resizes to the count of kind-2 children and is then zero-filled. Those are the
// fishing spots the citizen cycles through, so an unsized pair means a fisherman with
// nowhere to stand.
extern "C" uintptr_t* FishingComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();
    void* mem = AllocWork(0x70);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    // +0x48..+0x68 zeroed by the ctor (citizenRefs / spotStates / retryCount).
    new (mem) FishingWork();
    static_cast<FishingWork*>(mem)->InitFromEntity(&entity);

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);

    if (s_findPos && s_childSum) {
        uintptr_t posIbp = 0;
        s_findPos(entity, &posIbp);
        if (posIbp) {
            // posIbp IS the component — the finder returns its out-param, so retail's
            // `*pIVar3` and this local are the same value. Dereferencing again handed
            // the component's VTABLE to the child-count helper, whose garbage result
            // then sized the two arrays and blew up inside the Runtime allocator.
            const uintptr_t pos = posIbp;
            const int n0 = s_childSum(pos, 0);
            const int n2 = s_childSum(pos, 2);
            // B.3.2 bridge: 0x1400032e0 is Array<void*>::SetSize, 0x1400021d0 is Array<int32>::SetSize
            // (retail FishingWork_Ctor calls them at 1400eab83 / 1400eab8e). I32NoInit:
            // 0x1400021d0 leaves a grown tail as-found. Safe — the zero loop right below
            // (retail 1400eaba0) writes all +0x60 slots before any is read.
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(w + 0x48)).SetSize(n0);
            GameColl::AsArray<GameColl::I32NoInit>(reinterpret_cast<void*>(w + 0x58)).SetSize(n2);
            // Zero the kind-2 slot array the ctor allocates (retail's trailing loop).
            const int cnt = *reinterpret_cast<int32_t*>(w + 0x60);
            uint32_t* slots = *reinterpret_cast<uint32_t**>(w + 0x58);
            if (slots) for (int i = 0; i < cnt; ++i) slots[i] = 0;
        }
    }
    return WrapIBP(out, mem);
}

// ── HuntComponent::CreateWork — retail 0x1401226a0 ────────────────────────────────
// Retail writes vtable_AutoPickupWork then immediately vtable_HuntWork over it (a
// compiler artefact of the two-level ctor); only the final store is observable.
extern "C" uintptr_t* HuntComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();
    void* mem = AllocWork(0x58);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    // The ctor zeroes the leaf fields (+0x48 onward) as retail's tail does.
    new (mem) HuntWork();
    static_cast<HuntWork*>(mem)->InitFromEntity(&entity);

    return WrapIBP(out, mem);
}

// ── HerbalistComponent::CreateWork — retail 0x14004b160 ───────────────────────────
// This is the Gatherer's Hut work (GathererShelter.rsc carries HerbalistDescription).
extern "C" uintptr_t* HerbalistComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();
    void* mem = AllocWork(0x60);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    // The ctor zeroes the leaf fields (+0x48 onward) as retail's tail does.
    new (mem) HerbalistWork();
    static_cast<HerbalistWork*>(mem)->InitFromEntity(&entity);

    return WrapIBP(out, mem);
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
//     if (st != 14 && comp[+0x50] == 0 && comp[+0x51] == 0) *(u16*)(comp+0x50) = 0x101
//
// i.e. "if we are NOT in the paused state and the player has turned BOTH plant and
// cut off, turn both back on". Retail relies on it because a lodge whose two flags
// are both clear has nothing to do, yet state 13 (producing) was entered anyway —
// this is the repair that keeps such a lodge from minting a work that can never run.
// sm+0x52 is the pending state id and sm+0x50 the current one; the sign test is
// retail's "no pending transition" check.
extern "C" uintptr_t* ForesterComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();

    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(comp + 0x10);

    if (s_findSM) {
        uintptr_t smIbp = 0;
        s_findSM(entity, &smIbp);
        char st = *reinterpret_cast<char*>(smIbp + 0x52);
        if (st < 0) st = *reinterpret_cast<char*>(smIbp + 0x50);
        if (st != 14 && *reinterpret_cast<char*>(comp + 0x50) == 0
                     && *reinterpret_cast<char*>(comp + 0x51) == 0)
            *reinterpret_cast<uint16_t*>(comp + 0x50) = 0x101;
    }

    void* mem = AllocWork(0x60);
    if (!mem) return WrapIBP(out, nullptr);

    // The ctor zeroes +0x48/+0x50/+0x58 and runs ForesterWork_ResolveFuncs.
    new (mem) ForesterWork();
    static_cast<ForesterWork*>(mem)->InitFromEntity(&entity);

    return WrapIBP(out, mem);
}

// ── MarketComponent::CreateWork — retail 0x14004ea40 ─────────────────────────
// ⚠ Ghidra labels 0x14004ea40 "MarketWork_Ctor". It is NOT the MarketWork ctor — it
// is MarketComponent's slot 18, the same override every ResourceProduceComponent
// subclass supplies. Exactly the misnomer flagged at the top of this file for
// HerbalistComponent (0x14004b160 "HerbalistWork_Ctor").
//
// Byte-for-byte the same shape as HuntComponent's: MemAlloc(0x58) + Work_Ctor25 over
// the entity at comp+0x10 + vtable_MarketWork + zero {+0x48 qword, +0x50 dword}.
// There is NO Forester-style prologue in the retail body — no state-machine read, no
// flag re-arm — so none is added here.
//
// A live probe measured this slot still holding the inert stub, i.e. no MarketWork was
// ever minted: state 13's enter (0x14012ae60) calls slot 18 to create the work and post
// it to the citizen reservation ledger, so a stubbed slot silently means "market never
// offers a job".
uintptr_t* MarketComponent::CreateWork(uintptr_t* out) {
    Resolve();
    void* mem = AllocWork(0x58);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = reinterpret_cast<uintptr_t>(_entity);

    // The ctor zeroes +0x48/+0x50 and runs MarketWork_ResolveFuncs.
    new (mem) MarketWork();
    static_cast<MarketWork*>(mem)->InitFromEntity(&entity);

    return WrapIBP(out, mem);
}

// 0x14004ea40 door: registered by address (standalone/reg_component_economy.cpp).
extern "C" uintptr_t* MarketComponent_CreateWork(void* self, uintptr_t* out) {
    return reinterpret_cast<MarketComponent*>(self)->MarketComponent::CreateWork(out);   // B.3.8 bridge
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
//   entity   = *(self + 0x10)                    -- the owner IBP, CheckPtr'd first
//   descRoot = *(entity + 0x28)                  -- ComponentEntity+0x28 is the entity
//                                                   DESCRIPTION, not the component array
//                                                   (that is +0x30). Retail searches the
//                                                   description's CHILD list here.
//   cpDesc   = first child whose GetTypeId == ConsumeProduceDescription (0x1401c7388)
//   work     = ConsumeProduceAction_Ctor(MemAlloc(0x78,8), &entity,
//                                        &cpDesc[+0x38], cpDesc[+0x3c])
//
// ⚠ retail dereferences cpDesc UNGUARDED after the loop — with no matching child it
// reads [0x38] off a null and faults. That deref is left faithful: a ConsumeProduce
// building always carries its own description, and a guard here would silently mint a
// work with garbage recipe ints instead of restoring the behaviour.
//
// ⚠ the second int is genuinely an INT, not a float: retail loads it with
// `MOV R9D,dword ptr [RBX+0x3c]` — the integer register, not XMM3. Checked
// deliberately because a float arg declared int lands in the wrong register.
bool ConsumeProduce_ResolveFuncs();

extern "C" void* ConsumeProduceAction_Ctor(void* mem, uintptr_t* entityIbp,
                                           const int* productIdx, int second);

// ── ConsumeProduceAction ctor — retail 0x1400e9200 ────────────────────────────
// Not the plain 0x1400e9340 ctor the Work FACTORY uses: this is the 4-argument one
// slot 18 calls, which seeds the recipe fields and sizes the two work arrays off the
// building's child attach points. Same childSum(0)/childSum(2) shape as FishingWork's
// ctor, but the arrays live at +0x58 (IBP array, grown) and +0x68 (int array, resized
// then zero-filled with the count at +0x70).
extern "C" void* ConsumeProduceAction_Ctor(void* mem, uintptr_t* entityIbp,
                                           const int* productIdx, int second) {
    Resolve();
    if (!mem) return nullptr;
    const uintptr_t w = reinterpret_cast<uintptr_t>(mem);

    // The REAL ConsumeProduce (Work/ConsumeProduce.h): base init, its field zeroing
    // and ConsumeProduce_ResolveFuncs; then Work_Ctor25's body; then the 4-arg ctor's
    // own seeding below (the same stores the hand build wrote).
    new (mem) ConsumeProduce();
    static_cast<ConsumeProduce*>(mem)->InitFromEntity(entityIbp);

    *reinterpret_cast<uint8_t*>(w + 0x48)  = 0;
    *reinterpret_cast<uint32_t*>(w + 0x4c) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x50) = productIdx ? *productIdx : 0;
    *reinterpret_cast<uint32_t*>(w + 0x54) = static_cast<uint32_t>(second);
    *reinterpret_cast<uint64_t*>(w + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(w + 0x60) = 0;
    *reinterpret_cast<uint64_t*>(w + 0x68) = 0;
    *reinterpret_cast<uint64_t*>(w + 0x70) = 0;

    if (s_findPos && s_childSum) {
        uintptr_t posIbp = 0;
        s_findPos(*entityIbp, &posIbp);
        if (posIbp) {
            // posIbp IS the component — the finder returns its own out-param. Retail's
            // `*pIVar3` and this local are the same value; dereferencing again would
            // hand the component's VTABLE to the child-count helper (the trap the
            // fishing port hit).
            const int n0 = s_childSum(posIbp, 0);
            const int n2 = s_childSum(posIbp, 2);
            // B.3.2 bridge: 0x1400032e0 is Array<void*>::SetSize, 0x1400021d0 is Array<int32>::SetSize
            // (retail ConsumeProduceAction_Ctor calls them at 1400e92d8 / 1400e92e3).
            // I32NoInit: 0x1400021d0 leaves a grown tail as-found. Safe — the zero loop
            // right below (retail 1400e9300) writes all +0x70 slots before any is read.
            GameColl::AsArray<GameColl::Ptr8>(reinterpret_cast<void*>(w + 0x58)).SetSize(n0);
            GameColl::AsArray<GameColl::I32NoInit>(reinterpret_cast<void*>(w + 0x68)).SetSize(n2);
            const int cnt = *reinterpret_cast<int32_t*>(w + 0x70);
            uint32_t* slots = *reinterpret_cast<uint32_t**>(w + 0x68);
            if (slots) for (int i = 0; i < cnt; ++i) slots[i] = 0;
        }
    }
    return mem;
}

extern "C" void ConsumeProduce_DumpRecipes(void* cpDesc, int selected);

extern "C" uintptr_t* ConsumeProduceComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();
    ConsumeProduce_ResolveFuncs();


    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);
    if (!entity) return WrapIBP(out, nullptr);

    // ComponentEntity +0x28 = description (+0x30 is the component array — do not
    // confuse the two; the search below is over DESCRIPTIONS).
    const uintptr_t descRoot = *reinterpret_cast<uintptr_t*>(entity + 0x28);
    void* cpDesc = nullptr;
    if (descRoot) {
        const uint32_t want = ConsumeProduceDescription::TypeId();   // cell 0x1401c7388 (D5);
                                                                      // TypeId_ConsumeProduceComponent
                                                                      // is a documented misnomer.
        const int    n   = *reinterpret_cast<int32_t*>(descRoot + 0x30);
        void** const arr = *reinterpret_cast<void***>(descRoot + 0x28);
        for (int i = 0; i < n && arr; ++i) {
            void* c = arr[i];
            if (!c) continue;
            typedef uint32_t (*GetTypeId_t)(void*);
            const uint32_t tid = (reinterpret_cast<GetTypeId_t>(
                *reinterpret_cast<uintptr_t*>(*reinterpret_cast<uintptr_t*>(c) + 0x10)))(c);
            if (tid == want) {
                if (fn_IsDerivedFrom && fn_IsDerivedFrom(c, want)) cpDesc = c;
                break;
            }
        }
    }

    void* mem = AllocWork(0x78);
    void* obj = nullptr;
    if (mem) {
        // Faithful unguarded description read — see the note above.
        const uintptr_t d = reinterpret_cast<uintptr_t>(cpDesc);
        const int first  = *reinterpret_cast<int32_t*>(d + 0x38);
        const int second = *reinterpret_cast<int32_t*>(d + 0x3c);
        obj = ConsumeProduceAction_Ctor(mem, &entity, &first, second);
    }


    // Dump the recipe table once. "No materials" on the panel is ambiguous between
    // "the selected recipe eats something the town has none of" and "hauling never
    // delivers"; the table plus the live selection separates them.
    ConsumeProduce_DumpRecipes(cpDesc,
                               *reinterpret_cast<int*>(
                                   reinterpret_cast<uintptr_t>(self) + 0x50));

    static int n = 0;
    if (n < 4) { ++n;
        fprintf(stderr, "[ConsumeProduce] CreateWork self=%p entity=%p cpDesc=%p "
                        "work=%p\n", self, (void*)entity, cpDesc, obj);
        fflush(stderr);
    }
    return WrapIBP(out, obj);
}
