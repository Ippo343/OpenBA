// code/Game/Component/component_factory_base.cpp
// Shared inert-component factory infrastructure — see component_factory_base.h.

#include "Component/component_factory_base.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstring>
#include <cstdlib>


// Ported Component base Serialize (absorbresourcecomponent.cpp, retail 0x1401189b0).
void Component_SerializeReimpl(void* self, void* stream);

namespace {

// Shared scalar-deleting destructor (vtable slot +0x00). For a freshly-spawned
// inert component the body is zeroed (no owned arrays/refs to release), so this
// only frees the object itself when the deleting flag (bit 0) is set — matching
// the tail of every component's *_ScalarDtor.
//
// InitInert/InitInertTyped run the real Object::Internal::Internal, which enters the
// object into the InternalFactory handle table (obj+0x08 = index, obj+0x0c = generation).
// Freeing without the matching ~Internal leaves a dangling table entry: the next
// unrelated ~Internal swap-removes it and executes `*(int*)(freedPtr+0x08) = idx`,
// a 4-byte store into a block the allocator has already reused. That is the
// demolish-churn heap corruption (see Object/internal_factory_audit.h mode (A)).
// Every real component's *_ScalarDtor in the exe ends in ~Internal, so calling it
// here is also the faithful behavior.
// Guarded by IsRegistered: if fn_ObjectInternal_Ctor was NULL when this instance was
// built (or it came from some other path), obj+0x08 is heap garbage and calling
// ~Internal would swap-remove through that garbage index — corruption mode (B).
// Only objects genuinely present in the table are unregistered.
void* Inert_ScalarDtor(void* self, uint32_t flags) {
    if (self && fn_ObjectInternal_Dtor && ObjAudit::IsRegistered(self)) {
        fn_ObjectInternal_Dtor(self);
    } else {
        ObjAudit::NoteUnregisteredFree(self, "InertComponent");
    }
    if ((flags & 1) && fn_MemFreeStatic) fn_MemFreeStatic(self);
    return self;
}

// Safe no-op for every non-dtor slot. Returns 0 in RAX, which reads correctly as
// void / int / pointer under the Win64 ABI regardless of the slot's real
// signature.
void* Inert_Stub(void*, ...) { return nullptr; }

// One vtable shared by every inert component that does NOT need a real GetTypeId
// (legacy InitInert path). 48 slots is comfortable headroom over the largest real
// component vtable, so any in-range virtual call lands on a stub rather than
// reading past the array.
void* g_InertComponentVtable[ComponentFactory::kInertVtableSlots] = {
    reinterpret_cast<void*>(&Inert_ScalarDtor),  // +0x00 ScalarDtor
    // slots +0x08 .. — filled below at first use.
};
bool g_vtableInit = false;

// Component base stream-Serialize (retail 0x1401189b0, ported in
// absorbresourcecomponent.cpp). Save/load wave 2026-08-27: slot 3 (+0x18) must
// NOT be an inert no-op — the save/load persist machinery vcalls it on every
// component, and retail's base restores the owner-entity IBP (+0x10), the
// capability flags (+0x20) and the description ref (+0x18). A no-op left every
// unported component with a NULL owner after load (the post-load +0x58 refresh
// then crashed in the first component that dereferenced it). Types with a real
// ported Serialize still win — their InstallLive/Finish fills overwrite [3]
// after FillTypedVtable runs.
void Component_SerializeReimpl_Fwd(void* self, void* stream) {
    ::Component_SerializeReimpl(self, stream);
}

void EnsureVtable() {
    if (g_vtableInit) return;
    for (size_t i = 1; i < ComponentFactory::kInertVtableSlots; ++i)
        g_InertComponentVtable[i] = reinterpret_cast<void*>(&Inert_Stub);
    g_InertComponentVtable[3] = reinterpret_cast<void*>(&Component_SerializeReimpl_Fwd);
    g_vtableInit = true;
}

// Fill a per-type inert vtable (Foundation E): every slot a stub except +0x00
// (scalar dtor) and +0x10 (the type's real GetTypeId). Idempotent — slot +0x10 is
// the fill sentinel, so this runs once per type and is reused for every instance.
void FillTypedVtable(void** vt, void* getTypeIdFn) {
    if (vt[2] == getTypeIdFn) return;            // already filled for this type
    vt[0] = reinterpret_cast<void*>(&Inert_ScalarDtor);   // +0x00 ScalarDtor
    for (size_t i = 1; i < ComponentFactory::kInertVtableSlots; ++i)
        vt[i] = reinterpret_cast<void*>(&Inert_Stub);
    vt[2] = getTypeIdFn;                          // +0x10 GetTypeId (real)
    vt[3] = reinterpret_cast<void*>(&Component_SerializeReimpl_Fwd);  // +0x18 base Serialize
}

}  // namespace

namespace ComponentFactory {

// The address every inert slot carries. Exposed so harnesses can tell a slot that has
// been reimplemented from one still holding the shared stub — the vtable array itself
// lives in an anonymous namespace and is not reachable from outside this file, and
// "is this slot live" is otherwise unanswerable from a probe.
void* InertStubSlot() {
    EnsureVtable();
    return g_InertComponentVtable[1];
}

// The base-only stream-Serialize every typed vtable starts with (slot [3]).
// Exposed so the slot-3 inventory dump (factory_registration.cpp) can tell a
// class with a REAL wired serializer from one silently losing its per-instance
// state on save/load (save/load wave 2026-08-28 — this trap shipped 21+ times).
void* BaseSerializeSlot() {
    EnsureVtable();
    return g_InertComponentVtable[3];
}

void InitInert(void* self, size_t size, uint32_t flags) {
    EnsureVtable();
    // Object::Internal base (sets +0x00 vtable + internal linkage at +0x08).
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)s            = g_InertComponentVtable;  // +0x00 our shared vtable
    *(void**)(s + 0x10)   = nullptr;                 // +0x10 owner IBP
    *(void**)(s + 0x18)   = nullptr;                 // +0x18 description
    *(uint32_t*)(s + 0x20) = flags;                  // +0x20 capability flags

    // Zero the component body (everything past the Component base, +0x28..size).
    // The Object::Internal base fields (+0x00 vtable, +0x08 linkage) and the
    // Component base (+0x10/+0x18/+0x20) are set explicitly above.
    if (size > 0x28)
        std::memset(s + 0x28, 0, size - 0x28);
}

void* MakeInert(size_t size, uint32_t flags) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, 8);
    if (!p) return nullptr;
    InitInert(p, size, flags);
    return p;
}

void InitInertTyped(void* self, size_t size, uint32_t flags,
                    void** vtable, void* getTypeIdFn) {
    FillTypedVtable(vtable, getTypeIdFn);
    if (fn_ObjectInternal_Ctor) fn_ObjectInternal_Ctor(self);

    uint8_t* s = reinterpret_cast<uint8_t*>(self);
    *(void**)s            = vtable;                   // +0x00 per-type vtable
    *(void**)(s + 0x10)   = nullptr;                  // +0x10 owner IBP
    *(void**)(s + 0x18)   = nullptr;                  // +0x18 description
    *(uint32_t*)(s + 0x20) = flags;                   // +0x20 capability flags
    if (size > 0x28)
        std::memset(s + 0x28, 0, size - 0x28);
}

void* MakeInertTyped(size_t size, uint32_t flags,
                     void** vtable, void* getTypeIdFn) {
    if (!fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    void* p = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)size, 8);
    if (!p) return nullptr;
    InitInertTyped(p, size, flags, vtable, getTypeIdFn);
    return p;
}

// Live-feature check for COMPONENT_LIVE_FACTORY and every BANISHED_LIVE_* gate.
// Policy (2026-07-19): once a feature is verified working it goes DEFAULT-ON via
// kDefaultOnFlags below; the env var then only serves as a kill-switch (=0/false).
// Flags NOT in the table keep the original opt-in behavior (env var or the master
// BANISHED_LIVE_COMPONENTS must be set), so unproven/harness gates stay dormant.
static bool EnvOn(const char* v) {
    return v && v[0] && std::strcmp(v, "0") != 0 && std::strcmp(v, "false") != 0;
}
// Verified-working features: the Makefile's former SA_LIVE_ENV set (all
// "user-verified working" per its comment) + BANISHED_LIVE_CITIZEN_CARRY
// (promoted per its own "fold into SA_LIVE_ENV once verified" note).
static const char* const kDefaultOnFlags[] = {
    "BANISHED_LIVE_TOOLBAR",
    "BANISHED_GAME_PRELOAD",
    "BANISHED_LIVE_STATUSICON_DRAW",
    "BANISHED_LIVE_CHARACTER_MOVETICK",
    // GRADUATED 2026-08-30 — ambient environment/emitter audio (the river, wind,
    // forest, wetland, field and winter beds). Without it a new game registers ZERO
    // 3D sound effects, which is not a quiet town but a silent one: Audio::Scene's
    // effect table stays empty for the whole session. That also made every A/B
    // against "does ambient audio work" a null test, since both arms measured a
    // world with no ambient emitter in it.
    // A/B, standalone:
    //   OFF: new game, 90 s -> Scene effect table live=0 for the entire run.
    //   ON : new game, 90 s -> live=3 (Environment.rsc mountain/wetland/winter),
    //        nullBuffers=0, crash log empty; tutorial load -> live=7, nullBuffers=0,
    //        stable past frame 5400.
    // Depends on the reference-mode preload in GameUpdateLoop_Serialize_95a90: with
    // ambient audio on but that preload off, a tutorial load leaves RiverEffect.rsc
    // and WindEffect.rsc with null SoundBuffers and faults inside XAudio2.
    "BANISHED_AMBIENT_AUDIO",
    "BANISHED_LIVE_STATEMACHINE",
    "BANISHED_LIVE_RESIDENCE",
    "BANISHED_LIVE_HOUSING",
    "BANISHED_LIVE_STORAGE",
    "BANISHED_LIVE_REGIONMAP",
    "BANISHED_LIVE_DECAL",
    // GRADUATED 2026-08-29 — walkable floors (dock / bridge decks). Two halves, one flag:
    // FloorComponent slots 6/7/12 register the deck picking mesh into the MAP's
    // Picking::Scene and bump each covered cell's floor counter (cellRec+0x2d), and the
    // matching refine in mapquery.cpp's f3e0 ray-casts that scene so a citizen's per-frame
    // Z lands on the deck instead of the riverbed under it.
    // POSITIVE RUN (seed 12345, BANISHED_BUILD_DEMO_FISHINGDOCK=1, ~2 min, no crash):
    // two docks placed their floors — the demo one at tile (78,134) in shallow water
    // (deck z 0.157 vs terrain 0.004, a subtle correction) and a second at (99,120) out
    // in deep water, where the same trace reads deck z -0.075 / 0.339 against terrain
    // -1.853 / -1.883 — a FULL BODY HEIGHT of correction. 126k refine hits, zero crashes;
    // the gate-off arm logged 0 hits and an identical world build. User-verified visually
    // on both docks: fishermen stand on the boards and path along them as intended.
    "BANISHED_LIVE_FLOOR",
    "BANISHED_LIVE_TOOLS",
    "BANISHED_SELECTION_OUTLINE",
    "BANISHED_LIVE_EVENTS",
    "BANISHED_LIVE_CITIZEN_CARRY",
    // GRADUATED 2026-08-22 — the TradeUI checkout (buttonTrade, 0x140081420). This was
    // the whole of the "pressing Trade does nothing" report: the handler was ported,
    // registered and correctly dispatched, and returned early on an opt-in gate, so the
    // click was inert by design and looked broken.
    // POSITIVE RUN (seed 12345, demo trading post, merchant docked, gate forced on):
    // a player click on Trade bought iron tools from the docked merchant and the panel
    // updated — a result only the live path can produce.
    // Its one previously-unported dependency, 0x14005ace0 (the livestock spawn), is now
    // ported in code/Game/Component/tradecomponent_livestock.cpp and registered, so no
    // arm of the checkout is a stub any more.
    "BANISHED_LIVE_TRADE_CHECKOUT",
    // GRADUATED 2026-08-22 — the rest of the trade lane, in one go, on the strength of a
    // full chain running end to end: merchant timer -> boat spawn -> A* route -> sail ->
    // dock (both gates at their exact retail thresholds) -> manifest -> auto-trade
    // delivering 20 units, plus a player click on Trade buying iron tools. Storage,
    // town trade credit and the availability pools all move.
    //
    // These four were the three stacked type-id/component gates that made the trading
    // post crash on click (see project_trade_storage_tool_wave) plus the two controller
    // state machines. None of them ever needed fix code — they were pre-existing work
    // behind flags nothing set. With them in the table a plain `make sa-run` gets a
    // working trading post for the first time.
    //
    // ⚠ ELEMENT_TIDS CARRIES A KNOWN, MEASURED, UNEXPLAINED DELTA. A 6-run A/B
    // (seed 424242, ~500 s/arm, 2026-08-21 `ebdda28`) moved deer AnimalUpd MoveTo
    // meanAttempts 1.45 (off) -> 1.76 (on); TRADE_TIDS was indistinguishable from off, so
    // ELEMENT_TIDS is the channel. It is NOT frame pacing (mean fps 140.5-140.8 in every
    // arm). The mechanism was never identified; the leading, incomplete hypothesis is an
    // RNG-stream shift, since BANISHED_SEED pins world-gen only and the world RNG at
    // gw+0xd8 is unseeded. Graduated anyway as an explicit call: the trade lane is
    // verified and the delta is a pathing-efficiency wobble, not a failure. If deer or
    // hunting throughput regresses, re-run that A/B before looking anywhere else.
    "BANISHED_LIVE_ELEMENT_TIDS",
    "BANISHED_LIVE_TRADE_TIDS",
    // Residence registry (starttown.cpp:897) — call ResidenceComponent's init half
    // (0x140129ef0) directly instead of dispatching vtable slot 7, whose body calls
    // 0x140002cc0 = ERASE-BY-VALUE. Every house registered into townMgr+0x20 and was
    // erased one line later, so the list read 0 for the entire run.
    //
    // That single empty list silently disabled the whole second generation.
    // CitizenComponent::Update's find-your-own-home scan (sub_0e70) skips its entire body
    // when townMgr+0x28 == 0, so it always returned null; the guardian link +0x258 is
    // cleared ONLY on that scan succeeding (sub_1fa0(self, null)); and the marriage gate
    // requires +0x258 == 0. Net effect: no citizen could ever move out, therefore none
    // could ever marry, therefore the town had exactly its six founding couples forever
    // and died out when they aged past maxAge (~15).
    //
    // Graduated 2026-08-15 on an A/B at identical seed (12345, FORCE_SPEED=40), food held
    // abundant by BANISHED_FOOD_TOPUP so starvation could not confound it. Flag OFF:
    // count(+0x28)=0 at every census, 12 ERASE lines, noGuardian=0 and ELIGIBLE=0 through
    // frame 10000 with 24 EMPTY houses, spouse pinned at 12. Flag ON: count=30, ZERO ERASE
    // lines, noGuardian=1 and ELIGIBLE=1 by frame 2000, and spouse 12 -> 14 by frame 4000 —
    // the first second-generation marriages this project has measured. spouse > 12 is a
    // value only the live path can produce, which is what makes this a positive run rather
    // than an absence-of-symptom one.
    "BANISHED_LIVE_RESIDENCE_REGISTRY",
    // House-seed reservation ordering (starttown.cpp StartTown_SeedHouseInventory).
    // The food budget reserved the full REQUESTED fuel volume (400 * 4 = 1600) while the
    // fuel clamp ran second and took only what food left (225 units / 900 volume) — so
    // food reserved against ~700 volume of fuel that never landed, `cap - fuelVolume -
    // carryLoad` went negative, and maxFoodUnits was 0. **Every house started with ZERO
    // food, on pure defaults, with no seed env var set.** No BANISHED_SEED_FOOD value
    // could help: the clamp yields 0 for 6000, 130 or 1 once fuelVolume + carryLoad >= cap.
    //
    // Graduated 2026-07-31 on a deterministic mechanism check, not an outcome metric:
    // with the flag on, `foodBudget=900`, food seeds 130/78 units, `GetResourceCount(res)`
    // matches, and there are ZERO food-CLAMPED lines; fuel still gets 192-205 units and the
    // house packs to 998/1000. Reproducible at n=1 because it is binary.
    //
    // NOT claimed: any longevity ranking. Seeded runs still go extinct (~9000 frames vs
    // ~7500 for the control), and run-to-run variance is large because the gameplay RNG at
    // gw+0xd8 is UNSEEDED — BANISHED_SEED pins world-gen only. Frames-to-extinction is a
    // single threshold crossing and must not be used to rank fixes. See
    // reference_banished_seed_pins_worldgen_only.
    "BANISHED_SEED_FOOD_FIRST",
    // StateMachineComponent slot 4 (link the controller chain at Startup) + slot 6
    // (the one-shot Tick inside RegisterEntity) — retail's insta-build engine, and
    // the ordering guarantee the rest of the start town depends on. Graduated
    // 2026-07-27 together with LIVE_STORAGECTRL below, which needs it.
    //
    // Held opt-in because "linking earlier only moves the +0x52 default-state write
    // earlier, and that write is what the 2026-07-20 stockpile regression turned on."
    // That risk is measured out now: the LINK-DECIDE trace shows the stockpile taking
    // StorageLocationComponent's own default of 2 (its live node), not DestroyComponent's
    // 4 — the out-of-order destroy guard blocks that — and the 7 other founding
    // structures taking BuildComponent's 1 and running the real 1 -> 8 -> 2 chain to
    // completion inside the spawn bracket (`state(+0x50)=2 pending=-1`, no fallback
    // stamp needed on any of them).
    "BANISHED_LIVE_SM_ONADDED",
    // StorageLocationComponent live controller. Its state-2 ENTER is d230
    // (0x14012d230 = the townMgr+0x00 append) and its state-3 ENTER is d1f0 (the
    // matching erase) — verified to be the ONLY places retail registers/unregisters a
    // town storage location: the function is in no vtable, and its only xrefs are
    // .pdata plus SetupCrossRefs/impl_d410. Both are now real handlers instead of the
    // nulls they were left as, so the StorageBarn AND the stockpile register
    // themselves and the StartTown_WireStorage stand-in stands down.
    //
    // MUST ship with LIVE_SM_ONADDED above, which is why they graduate together:
    // Game_SpawnResources round-robins the starting stock over townMgr+0x00, so the
    // registration has to happen inside the spawn loop. With lazy linking the SM first
    // ticks a frame later and the seeder sees an empty list — measured storageLocs=0
    // and placed=0 for all seven masks. With both on: registrations at spawn time,
    // storageLocs=2, and 150/70/50/50/2400/400/40 placed, identical to the stand-in
    // baseline. The half-configuration double-registers (2 -> 4, d230 has no dedup)
    // and now warns loudly from StartTown_WireStorage.
    "BANISHED_LIVE_STORAGECTRL",
    // MapComponent vtable slot 12 (+0x60, the message slot — retail 0x1401243c0, literally
    // `CMP EDX,1 ; JZ 0x1401241b0 ; RET`), i.e. "on message 1, run the occupancy write".
    // Graduated 2026-07-28 alongside the above.
    //
    // This is WHEN a building claims its footprint cells, and it is not at placement:
    // MapComponent::Draw's own gate is `MapDescription+0x30 != 0`, and that byte is 0 for a
    // WoodHouse, so slot 6 never reaches 41b0. The broadcaster is
    // ModelComponent_helper_2b70(entity, msgId) and the ONLY caller in the exe that passes
    // 1 is ClearAreaDescription_helper_77f0 — ClearArea's state-2 ENTER (`MOV EDX,1; CALL
    // c2b70`). Every other +0x60 dispatch site is `XOR EDX,EDX` (checked: all three
    // BuildComponent sites and both sub_8720 sites). So a building becomes solid when its
    // CLEARING FINISHES, which is also what makes dc20's stale-occupant eviction safe — by
    // then citizens have harvested the trees and it only sweeps stragglers.
    // We already ported the state-2 handler and broadcast event 1; this slot was a stub, so
    // the message landed on nothing and no placed building ever registered its cells.
    // Inert unless BANISHED_LIVE_CLEARCTRL is on (nothing else broadcasts event 1).
    "BANISHED_LIVE_MAP_SOLIDIFY",
    // ⚠ KNOWN DEVIATION FROM RETAIL, deliberately default-on. In dc20 (0x14009dc20) a
    // footprint cell takes the owner entity in the ENTITY raster (cell+0x08) for every mask
    // char except '.' — instruction-verified: `CMP CL,0x2e ; SETNZ BL` at 0x14009ddef, and
    // `TEST BL,BL ; JZ` at 0x14009de21 picks owner-vs-null. With this flag on, a '_' cell
    // (the building's own walkable strip) keeps its class-2 stamp and still evicts stale
    // occupants, but does NOT take cell+0x08.
    //
    // Why: retail's FishingWork::Slot4 (0x1400e9f20) contains two mechanisms that are only
    // coherent when the dock's four mode-2 "create" drop cells read cell+0x08 == 0 —
    //   (1) the state-2 latch is cleared ONLY by `spotState==2 && cell+8==0`, and
    //   (2) the no-free-spot fallback hands the entity found at cell+8 to f8620, which
    //       dispatches that work's Execute — finding ITSELF there is unbounded recursion.
    // Those cells are '_' on FishermansDock.rsc's _pathBitmap, so the claim latches all
    // four spots after exactly spotCount catches and the dock stops for good, and it is
    // also why our fallback needs a self-entity guard against a measured stack overflow.
    //
    // Blast radius is bounded: buildings block placement and answer the cell pick through
    // the ZONE raster (cell+0x00 / the '.'-reserve counter at cell+0x28), not through
    // cell+0x08 — f5d0 (0x14009f5d0), the per-cell placement predicate, reads `*cell` and
    // `cell[0x28]` and never touches cell+8. dc20's stale-occupant eviction runs before the
    // mask sample, so a building placed over trees still clears them on '_' cells too.
    //
    // Measured 2026-08-15, same harness, single change: default (this flag on) => 44 fish
    // piles spawned, each landing EXACTLY on a drop-spot cell, spots recycling; with
    // BANISHED_LIVE_WALKWAY_DROPCELLS=0 => 4 piles, all four spots latched at state 2,
    // every pile rerouted off its spot by CreateRawMaterial.
    //
    // OPEN: which retail step leaves those cells unowned is NOT identified. Every link was
    // checked byte-faithful — dc20/dee0/41b0/4750 (claim+release), d360 (zone), 2e20 and the
    // PointList parse (the drop points ARE mid-deck), the mask string and its rotation, the
    // MapComponent rect, bd40 (the reroute). The claim arrives through retail's own door
    // (ClearArea state-2 enter -> msg 1 -> MapComponent slot 12), and no release for the
    // dock's rect ever follows it. Find that step and this flag should go away.
    "BANISHED_LIVE_WALKWAY_DROPCELLS",
    // Wild deer herds — verified end-to-end 2026-07-29 and soak-passed 2026-07-29
    // (20+ real minutes at FORCE_SPEED=10 ≈ a full deer lifespan: 16 herds, 108
    // deaths vs 119 AdjustSize repop spawns, per-herd census stable across three
    // repop rounds, a starved herd re-seeded from the retail min-2 target, empty
    // crash log). Aging arithmetic verified retail-identical (Update dt*1/3600,
    // Startup deathAge roll, Populate initial-age seeding age=RandFloat*base —
    // early deaths are the faithful uniform-age tail, not a drain). The three
    // gates are one feature: population spawns herds, herd ticks the members,
    // animal update ages/wanders them.
    "BANISHED_LIVE_HERD",
    "BANISHED_LIVE_POPULATION",
    "BANISHED_LIVE_ANIMAL_UPDATE",
    // Pasture chain — adopt/spawn/production verified 0071c53; reproduction
    // proven 2026-07-29 after porting the missing slot-15 breed tag (msg 0 ->
    // random pastured animal gets flag bit 23; the latch->bit 21->Reproduce
    // chain then births — first lamb observed in a 15-min tend-harness soak).
    // Default runs have no pastures until the fenced-area tool lands, so this
    // is inert without the demo rig; organic births still need the herder
    // chain (PastureComponent_Slot14 cmd 2 -> PastureWork), tracked as an open.
    "BANISHED_LIVE_PASTURE",
    // InteractComponent vtable slot 6 (+0x30, OnAdded -> retail 0x140123110), which
    // rebuilds the owner's interact use-location list (+0x28 array / +0x30 count) from its
    // MultiObjectComponent children. Graduated 2026-07-30: this is what lets a MULTI-PART
    // construction site receive materials at all.
    //
    // GatherRawMaterials' target-position step is CalcBestTargetPos (0x140122be0) over
    // InteractDescription+0x38 {ptr,count}, then CalcChildTargetPos (0x140122b00) recursing
    // over that child list. A fenced pasture master's own description carries ZERO access
    // points (measured: accessPts=0) — its shape is computed by the tool, not declared in
    // Pasture.rsc — so with the list empty every haul bailed "no target pos (found==0)"
    // forever and the site sat at 0 materials indefinitely. Same for a bridge master, whose
    // own footprint is all water.
    // Measured with it on: the pasture master rebuilds children=1 from 29 MultiObject
    // entries, 0 gather bails (was 722 in the control run), 56/56 logs delivered, both
    // sites reach state 8 and take real per-mesh build labor to completion, 0-byte crash
    // log. It also corrects a mis-attribution: a footprint containing 23 trees built fine
    // with the clear chain OFF, so "a tool-placed site over naturals never builds" was
    // never about clearing — it was this.
    "BANISHED_LIVE_INTERACT_ONADD",
    // StorageComponent vtable slot 6 (+0x30, OnAdded — retail 0x14012c640 =
    // StorageComponent::Draw; read straight off the real vtable at 0x140164720, where
    // [3]=0x14012c730 Shutdown and [6]=0x14012c640). Flags 0x81 has bit 0, so
    // ComponentEntity_OnAddedPass has always broadcast to this slot — it landed on the
    // inert stub. Graduated 2026-08-09.
    //
    // This is the CARRY CAPACITY of every citizen: Draw sets +0x38 = _volumeLimit
    // (desc+0x24) x area. Standalone compensated with four hand-calls in starttown.cpp
    // (CitizenCarryStorageInit), all on the FOUNDING cohort — no birth path calls them, so
    // every citizen born after world start kept capacity 0 forever. Capacity 0 makes
    // PickupStoredRawMaterial request (0-0)/unitVolume = 0 items, the only rejection is
    // `available < requested` which 0 passes, and AddItem (0x14012d000) appends a
    // {resType,0} stack. Pf_SpillAllStorage_7570's exit test is the stack COUNT at +0x30
    // while its work is gated on the entry's own count, so a zero-count stack spins it
    // forever — an in-world FREEZE (first reached by the pasture cull, 2026-07-31). Both
    // loops are byte-faithful; retail relies on a zero-count stack never existing, which is
    // exactly what this slot guarantees. Do not guard them.
    //
    // A/B measured 2026-08-09 with the force-birth harness (BANISHED_FORCE_BIRTH, which
    // calls the real delivery leaf helper_1b90 — an organic birth needs 900s of gestation
    // and never fires in a test-length run): ON => three newborns at capacity 100, the six
    // founding-colonist verifier lines report 100 ALREADY SET before the hand-call, 22 of
    // 29 slot dispatches set a real capacity and 7 take the faithful `desc+0x21 == 0` skip
    // (buildings, whose capacity comes from TownStorage_BuildCompleteInit_7ff0 instead —
    // it deliberately does not test that byte). OFF => the same newborn at capacity 0.
    "BANISHED_LIVE_STORAGE_ONADDED",
    "BANISHED_LIVE_GROWTH",
    "BANISHED_LIVE_FRUIT",
    // Wild-food spawning (SpawnComponent). Three formerly-independent opt-ins that
    // are one feature — a tree's SpawnDescription is the SOLE producer of the four
    // resources the Gatherer's Hut accepts (Onion/Blueberry/Mushroom/Root); they are
    // not a worldgen pool. Graduated together 2026-08-13:
    //   * LIVE_SPAWN    — SpawnComponent live factory: slot 4 Startup sizes the
    //                     per-entry timer array, slot 9 Update ticks the growth window
    //   * SPAWN_DOSPAWN — the per-tick spawn actually instantiates (was held off
    //                     because the closure creates real entities)
    //   * SPAWN_INITIAL — the worldgen seed pass (5cc0), so a NEW MAP starts with
    //                     wild food on the ground instead of growing it over months
    // SPAWN_INITIAL was the one that crashed: it took an ACCESS_VIOLATION in
    // Graphics::Scene::Clean 4/4 runs until the call was moved AFTER
    // fn_EntityList_RegisterEntity to match retail's a12b0 order (register at
    // 0x1400a1bf2, THEN 5cc0 at 0x1400a1cb2) — registration is what runs the
    // OnAdded/+0x30 pass that puts the parent's ModelComponent into the scene.
    // Post-fix: 0/4 crashes, seed output unchanged at CREATED=249.
    // See notes/generation_deviations.md (deviation #4).
    // Each remains a kill-switch (=0) for an A/B run.
    "BANISHED_LIVE_SPAWN",
    "BANISHED_SPAWN_INITIAL",
    // GRADUATED 2026-08-26 — the CitizenComponent type-id .rdata slot DWORD_1401c7158
    // (standalone/reg_citizenai_typeids.cpp). Registration-gated, so with it off the slot
    // is UNRESOLVED and roughly thirty readers take a null-guarded skip; several of them
    // had already grown private GetTypeIdByName("CitizenComponent") copies to work around
    // exactly that, which is the shape of a gate that should not exist any more.
    // A/B, seed 424242, autostart, ~4 min each arm, identical binary:
    //   OFF: [CITID] c7158 reg=0 val=2704017556, and the boot sweep prints
    //        "NOT IMPLEMENTED: exe function at 0x1401c7158".
    //   ON : [CITID] c7158 reg=<addr> val=2704017556 — the SAME CRC the private by-name
    //        copies resolve — and that NOT IMPLEMENTED line is GONE. It is the ONLY
    //        difference between the two runs' normalised error logs; tag sets are
    //        identical, crash log empty in both arms.
    // Scope of what this does NOT prove: the soak neither saves nor loads, so the
    // Serialize-side IsDerivedFrom validators (the bulk of the readers) stayed dormant,
    // and BuildStructure's CheckCitizenHasTask never fired in either arm — see the note
    // on it in code/Game/Work/buildstructure.cpp, whose "TypeInfo_CompleteTask" label for
    // this slot contradicts the RegisterType ground truth and is a separate open.
    // The value-vs-pointer guards in residence/workplace/attendance were tightened in the
    // same commit so a registered-but-not-yet-filled slot cannot clear live refs.
    "BANISHED_LIVE_CITIZENAI_CITID",
    // GRADUATED 2026-09-04 — GameUpdateLoop::Load step 1, the ExternalList::Find of
    // "Game/Disease/Diseases.rsc" stored at GameWorld+0xA8. Retail always does this
    // Find; our port hard-nulled the slot behind an opt-in, and a null there is not
    // "no diseases yet" but "the disease system does not exist": CitizenComponent::
    // CatchDisease (0x140112620) returns on the null before it rolls anything, and
    // DebugUI::Slot4's diseaseCombo populate loop is bounded by
    // ExternalList::GetResourceCount over that same slot — so the debug panel's
    // Disaster tab rendered its disease dropdown with ZERO rows and no diagnostic.
    // Every consumer downstream (Infect -> SpreadDisease -> FindCareBuilding ->
    // RecoverOrDie, plus the hospital/herbalist care path) was already ported and
    // registered; the list was the one missing input. Set the var to 0 to A/B it.
    "BANISHED_LIVE_DISEASELIST",
    // GRADUATED 2026-09-05 -- the Mods menu (ModDialog + ModDetailsDialog,
    // code/Game/Dialog/mod_dialog_vtables.cpp). Driven unattended with
    // BANISHED_AUTOMENU_BUTTON=7 BANISHED_AUTOMENU_CLICKS=...: list builds with
    // thumbnails, Enable toggles, Up/Down/Top reorder, Details opens and closes,
    // Cancel reverts (RefreshList), Ok saves (SaveList) + reloads the menu with the
    // new package set mounted, and the registry ModEnabled_*/ModIndex_* keys
    // persist across a clean Quit. Five defects fixed on the way (int-typed
    // pointer arg to ImageBuffer::Create, fn_MemFree never assigned, wrong
    // OnElementEvent overload, by-name Sprite ctor, String::operator+= ABI).
    "BANISHED_LIVE_MODDIALOG",
};
bool LiveEnabled(const char* envVar) {
    if (envVar) {
        const char* v = std::getenv(envVar);
        if (v && v[0]) return EnvOn(v);  // explicit setting wins, on OR off
        for (const char* f : kDefaultOnFlags)
            if (std::strcmp(f, envVar) == 0) return true;
    }
    return EnvOn(std::getenv("BANISHED_LIVE_COMPONENTS"));
}

}  // namespace ComponentFactory

