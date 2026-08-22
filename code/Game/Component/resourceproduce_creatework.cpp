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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

// Each Work reimpl resolves its own engine-function pointers in a *_ResolveFuncs()
// that only the FACTORY path calls. CreateWork allocates and constructs directly —
// exactly as retail does — so it must call the resolver itself, or the work goes out
// with null helper pointers and dies the first time the scheduler asks it anything.
// That is precisely how this first crashed: HerbalistWork::Slot10 (GetTarget) called
// a null s_fn_FindSubEntity from inside the work-scheduler insert. The VEH dump read
// it as "VCALL-of-0", but RAX == RCX == the entity is the signature of a null static
// function pointer, not of a vtable dispatch.
bool FishingWork_ResolveFuncs();
bool HuntWork_ResolveFuncs();
bool HerbalistWork_ResolveFuncs();
bool ForesterWork_ResolveFuncs();
bool MarketWork_ResolveFuncs();

// Work vtables are installed by the Work reimpls and published through the address
// registry, so read them back the same way every other cross-module reference does
// rather than reaching for the file-static arrays.
namespace {

constexpr uintptr_t kFishingWorkVtable   = 0x140161320;
constexpr uintptr_t kHuntWorkVtable      = 0x140161620;
constexpr uintptr_t kHerbalistWorkVtable = 0x1401615a0;
constexpr uintptr_t kForesterWorkVtable  = 0x1401613a0;
constexpr uintptr_t kMarketWorkVtable     = 0x140161710;

typedef void* (*fn_WorkCtor25_t)(void* self, void* pEntityIBP);   // 0x1400f72e0
// 0x140002ed0 Entity_FindComponent_Position — the finder FishingWork_Ctor actually
// uses. (It is a known misnomer: it resolves the InteractComponent, not a "position"
// component. Kept faithful to retail's choice rather than "corrected".)
typedef void  (*fn_FindComp_t)(uintptr_t entity, uintptr_t* out);
typedef int   (*fn_ChildSum_t)(uintptr_t posComp, int kind);      // 0x140123070
typedef void  (*fn_ArrayGrow_t)(void* vec, int count);            // 0x1400032e0
typedef void  (*fn_ArrayResize04_t)(void* vec, int count);        // 0x1400021d0

fn_WorkCtor25_t    s_workCtor25 = nullptr;
fn_FindComp_t      s_findPos    = nullptr;
fn_ChildSum_t      s_childSum   = nullptr;
fn_ArrayGrow_t     s_arrayGrow  = nullptr;
fn_ArrayResize04_t s_arrayRes04 = nullptr;
// 0x140003ea0 Entity_FindComponent_StateMachine — only ForesterComponent's slot 18
// needs it (to read the live state id before re-arming the two work flags).
fn_FindComp_t      s_findSM     = nullptr;
bool               s_resolved   = false;

void Resolve() {
    if (s_resolved) return;
    s_workCtor25 = reinterpret_cast<fn_WorkCtor25_t>(ResolveAddr(Module::Exe, 0x1400f72e0));
    s_findPos    = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_childSum   = reinterpret_cast<fn_ChildSum_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_arrayGrow  = reinterpret_cast<fn_ArrayGrow_t>(ResolveAddr(Module::Exe, 0x1400032e0));
    s_arrayRes04 = reinterpret_cast<fn_ArrayResize04_t>(ResolveAddr(Module::Exe, 0x1400021d0));
    s_findSM     = reinterpret_cast<fn_FindComp_t>(ResolveAddr(Module::Exe, 0x140003ea0));
    s_resolved = true;
}

void* AllocWork(int size) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    return fn_MemAlloc(g_pMemAllocSingleton, size, 8);
}

// The tail shared by all three: hand the raw object back as an InternalBasePtr.
uintptr_t* WrapIBP(uintptr_t* out, void* obj) {
    if (fn_InternalBasePtr_Ctor) fn_InternalBasePtr_Ctor(out, obj);
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
    FishingWork_ResolveFuncs();
    void* mem = AllocWork(0x70);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    if (s_workCtor25) s_workCtor25(mem, &entity);
    if (uintptr_t vt = ResolveAddr(Module::Exe, kFishingWorkVtable))
        *reinterpret_cast<uintptr_t*>(mem) = vt;

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uint64_t*>(w + 0x48) = 0;
    *reinterpret_cast<uint64_t*>(w + 0x50) = 0;
    *reinterpret_cast<uint64_t*>(w + 0x58) = 0;
    *reinterpret_cast<uint64_t*>(w + 0x60) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x68) = 0;

    if (s_findPos && s_childSum && s_arrayGrow && s_arrayRes04) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        uintptr_t posIbp = 0;
        s_findPos(entity, &posIbp);
        if (posIbp) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIbp);
            // posIbp IS the component — the finder returns its out-param, so retail's
            // `*pIVar3` and this local are the same value. Dereferencing again handed
            // the component's VTABLE to the child-count helper, whose garbage result
            // then sized the two arrays and blew up inside the Runtime allocator.
            const uintptr_t pos = posIbp;
            const int n0 = s_childSum(pos, 0);
            const int n2 = s_childSum(pos, 2);
            s_arrayGrow(reinterpret_cast<void*>(w + 0x48), n0);
            s_arrayRes04(reinterpret_cast<void*>(w + 0x58), n2);
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
    HuntWork_ResolveFuncs();
    void* mem = AllocWork(0x58);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    if (s_workCtor25) s_workCtor25(mem, &entity);
    if (uintptr_t vt = ResolveAddr(Module::Exe, kHuntWorkVtable))
        *reinterpret_cast<uintptr_t*>(mem) = vt;

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uint64_t*>(w + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x50) = 0;

    return WrapIBP(out, mem);
}

// ── HerbalistComponent::CreateWork — retail 0x14004b160 ───────────────────────────
// This is the Gatherer's Hut work (GathererShelter.rsc carries HerbalistDescription).
extern "C" uintptr_t* HerbalistComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();
    HerbalistWork_ResolveFuncs();
    void* mem = AllocWork(0x60);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    if (s_workCtor25) s_workCtor25(mem, &entity);
    if (uintptr_t vt = ResolveAddr(Module::Exe, kHerbalistWorkVtable))
        *reinterpret_cast<uintptr_t*>(mem) = vt;

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uint64_t*>(w + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x50) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x58) = 0;

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
    ForesterWork_ResolveFuncs();

    const uintptr_t comp = reinterpret_cast<uintptr_t>(self);
    uintptr_t entity = *reinterpret_cast<uintptr_t*>(comp + 0x10);

    if (s_findSM) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&entity);
        uintptr_t smIbp = 0;
        s_findSM(entity, &smIbp);
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&smIbp);
        char st = *reinterpret_cast<char*>(smIbp + 0x52);
        if (st < 0) st = *reinterpret_cast<char*>(smIbp + 0x50);
        if (st != 14 && *reinterpret_cast<char*>(comp + 0x50) == 0
                     && *reinterpret_cast<char*>(comp + 0x51) == 0)
            *reinterpret_cast<uint16_t*>(comp + 0x50) = 0x101;
    }

    void* mem = AllocWork(0x60);
    if (!mem) return WrapIBP(out, nullptr);

    if (s_workCtor25) s_workCtor25(mem, &entity);
    if (uintptr_t vt = ResolveAddr(Module::Exe, kForesterWorkVtable))
        *reinterpret_cast<uintptr_t*>(mem) = vt;

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uint64_t*>(w + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x50) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x58) = 0;

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
extern "C" uintptr_t* MarketComponent_CreateWork(void* self, uintptr_t* out) {
    Resolve();
    // CreateWork allocates and constructs DIRECTLY, bypassing the factory path that
    // would otherwise call this — see the note at the top of the file. Skipping it is
    // how the Herbalist path shipped a work with null static function pointers.
    MarketWork_ResolveFuncs();
    void* mem = AllocWork(0x58);
    if (!mem) return WrapIBP(out, nullptr);

    uintptr_t entity = *reinterpret_cast<uintptr_t*>(
        reinterpret_cast<uintptr_t>(self) + 0x10);

    if (s_workCtor25) s_workCtor25(mem, &entity);
    if (uintptr_t vt = ResolveAddr(Module::Exe, kMarketWorkVtable))
        *reinterpret_cast<uintptr_t*>(mem) = vt;

    uintptr_t w = reinterpret_cast<uintptr_t>(mem);
    *reinterpret_cast<uint64_t*>(w + 0x48) = 0;
    *reinterpret_cast<uint32_t*>(w + 0x50) = 0;

    return WrapIBP(out, mem);
}
