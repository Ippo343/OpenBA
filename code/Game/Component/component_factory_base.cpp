// code/Game/Component/component_factory_base.cpp
// Shared inert-component factory infrastructure — see component_factory_base.h.

#include "Component/component_factory_base.h"
#include "Object/internal_factory_audit.h"
#include "engine_funcs.h"
#include "resolve.h"
#include <cstring>
#include <cstdlib>


namespace {

// Kill-switch for the Object::Internal unregistration below (BANISHED_INTERNAL_UNREG=0
// restores the old, leaking behavior for an A/B run against the ObjAudit tally).
bool UnregisterOnFree() {
    static int cached = -1;
    if (cached < 0) {
        const char* v = std::getenv("BANISHED_INTERNAL_UNREG");
        cached = (v && (std::strcmp(v, "0") == 0 || std::strcmp(v, "false") == 0)) ? 0 : 1;
    }
    return cached != 0;
}

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
    if (self && UnregisterOnFree() && fn_ObjectInternal_Dtor && ObjAudit::IsRegistered(self)) {
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

void EnsureVtable() {
    if (g_vtableInit) return;
    for (size_t i = 1; i < ComponentFactory::kInertVtableSlots; ++i)
        g_InertComponentVtable[i] = reinterpret_cast<void*>(&Inert_Stub);
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
}

}  // namespace

namespace ComponentFactory {

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
    "BANISHED_LIVE_STATUSICON",
    "BANISHED_LIVE_STATUSICON_DRAW",
    "BANISHED_LIVE_CITIZEN_STATUSICON",
    "BANISHED_LIVE_CITIZEN",
    "BANISHED_LIVE_CITIZEN_UPDATE",
    "BANISHED_LIVE_CITIZENAI",
    "BANISHED_LIVE_CHARACTER",
    "BANISHED_LIVE_CHARACTER_MOVETICK",
    "BANISHED_LIVE_STATEMACHINE",
    "BANISHED_LIVE_RESIDENCE",
    "BANISHED_LIVE_HOUSING",
    "BANISHED_LIVE_STORAGE",
    "BANISHED_LIVE_REGIONMAP",
    "BANISHED_LIVE_STORAGEDISPLAY",
    "BANISHED_LIVE_DECAL",
    "BANISHED_LIVE_TOOLS",
    "BANISHED_LIVE_PICKING",
    "BANISHED_LIVE_PARTICLE",
    "BANISHED_SELECTION_OUTLINE",
    "BANISHED_LIVE_EVENTS",
    "BANISHED_LIVE_CITIZEN_CARRY",
    // CitizenComponent vtable slot 6 (+0x30, the OnAdded broadcast) -> OnAdd 0x140114000,
    // the append into the town citizen list townMgr+0xa0. Graduated 2026-07-26: without it
    // newborns were created, entered the world entity list, and joined no population system
    // at all — the town could never grow. 21 000-frame soak with it on: 24 -> 35 citizens,
    // fourteen births, no crash, and it makes StartTown_RegisterTownCitizens redundant
    // (that pass now reports 0 registered / 24 already done by the broadcast).
    "BANISHED_LIVE_CITIZEN_ONADD",
    // Organic construction pipeline — verified end-to-end 2026-07-22 (foundation ->
    // real gather/haul -> real build labor via MsgReceive -> completion -> staffed).
    // The three were formerly independent opt-ins; graduated together as one feature.
    "BANISHED_LIVE_BUILDCTRL",     // BuildComponent state machine (foundation->1->8->2)
    "BANISHED_BUILD_LIVE_HAUL",    // posts gather + build work to the citizen ledger
    "BANISHED_BUILDER_NATURAL",    // site->profession BST + tick-driven Builder pool
    // Loose-pile collection — verified 2026-07-24. RawMaterialComponent slot-6 (OnAdded)
    // self-posts a Pickup work on entity registration; a laborer claims it and dispatches
    // the haul. Held opt-in only because the haul used to crash on execution — that was the
    // InternalFactory removal-lifecycle corruption, now fixed (see internal_factory_audit.h).
    "BANISHED_LIVE_RAWMAT_PICKUP",
    // DestroyComponent live states — verified end-to-end 2026-07-25 (demolish -> haul-away
    // -> refund + replacement spawn -> replacement constructed -> correct finished dialog),
    // for both a genuinely-built house and a start-town house. Held opt-in until now as
    // "unproven": the removal churn it drives used to corrupt the heap, which was the
    // InternalFactory lifecycle bug (fixed; see internal_factory_audit.h). The whole
    // building-upgrade path is dead without it — DestroyComponent installs no vtable slots
    // and no state table, so the UpgradeUI click pumps a state machine that does not exist.
    "BANISHED_LIVE_DESTROY",
    // CitizenComponent::OnRemove (vtable slot 7) — verified end-to-end 2026-07-25 by A/B:
    // with it OFF, killing 5 citizens leaves all 5 in townMgr+0xa0 and the game takes an
    // ACCESS_VIOLATION shortly after; with it ON the list drains exactly (22->17, one per
    // death), a whole-town mass kill drains to 0, and the step-4 family branch runs its
    // recursive orphan chain to completion (a couple + 3 under-age children: 20->15) with
    // no spin-guard trip, ObjAudit bad=0 thousands of frames later. The default build
    // CRASHES on citizen death without this. Two deviations, both forced by our inert
    // factory rather than chosen: an uninitialised-body guard before HideTool (a zero-filled
    // body reads scene id 0 where the ctor writes -1, and RemoveObject(scene,0) freezes),
    // and progress guards on the two ring walks.
    "BANISHED_LIVE_CITIZEN_ONREMOVE",
    // WorkPlaceComponent::SetAccepting accepting=false (the pause path: closed indicator,
    // icon clears, roster eviction) — verified live 2026-07-25 once the real bug was fixed
    // at source. The three faults it first hit were all BuildUI_Slot9 passing a c8070
    // WorkComponent instead of the c80e0 WorkPlaceComponent — the finder for c8070 was
    // named fn_FindComponent_Workplace, since renamed to fn_FindComponent_Work (and the
    // c80e0 one from fn_FindComponent_BuildState2 to fn_FindComponent_WorkPlace);
    // with the correct component every operand is a live object.
    // Two pause clicks evicted real citizens with zero guard trips, and the closed icon
    // renders above the building and in the dialog status bar. The compensation guards
    // added while chasing the wrong cause have been removed; one declared deviation remains
    // (a no-progress break, because an unterminated eviction loop is an in-world freeze).
    "BANISHED_LIVE_WPC_PAUSE",
    // WorkPlaceComponent's ctor-body defaults — +0x40 = self and the accepting byte
    // +0x50 = 1, which retail's WorkPlaceComponent_Ctor (0x14012f2e0) writes
    // UNCONDITIONALLY. Our port had them inside the controller-gated block, so on the
    // default path every workplace in the game was born accepting=0 and
    // FindAvailableBuilding (which requires +0x50 != 0) could never return it.
    //
    // Graduated 2026-07-31 on a two-arm release soak, identical seed and settings. The
    // WPC census shows 8/8 constructions taking the defaults and the sweep flips
    // accepting=0 -> 8; both arms hold a stable 27-citizen town at frames 1500/3000/4500
    // with zero guard trips. Both arms then die IDENTICALLY just past 4500 (READ of
    // 0x140152a90 on the citizen-death teardown, NomadComp_FUN_140103480 <-
    // CitizenComponent_OnRemove) — a pre-existing default-path crash present with the
    // flag off, tracked separately, and — as for BANISHED_LIVE_DESTROY below — not a
    // reason to hold this. The soak was NOT clean; it was A/B-IDENTICAL, which is the
    // claim being made here.
    "BANISHED_LIVE_WPC_BORN_ACCEPTING",
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
    // The DestroySM's WorkPlaceComponent transitions — 0x14012f520 (enter the Builder
    // profession on demolish/upgrade) and 0x14012f670 (leave it) — verified 2026-07-25 on
    // the forced mid-construction upgrade harness: the staffed demo site logs
    // "f520 re-seat ... roster=0", i.e. all three seated builders released, and the
    // FUN_1400f8400 NOT-IMPLEMENTED skip is gone (that re-key is ported and registered now,
    // citizenai_work_scheduler.cpp CitizenAI_WorkScheduler_Rekey).
    //
    // A/B'd against the same scenario with the flag OFF. Both runs reach the upgrade spawn
    // and both then die the SAME way — a queued DropOffRawMaterial whose +0x38 still names
    // the demolished subject, its own fields already torn down (resType=0 count=-1). That
    // crash is the pre-existing removal-lifecycle dangling reference, present with the flag
    // off, so it is not a reason to hold this. It IS a reason not to call the upgrade path
    // clean; see notes/CITIZEN_ONREMOVE_BRIEF.md.
    "BANISHED_LIVE_DESTROY_WORKERS",
    // BuildComponent's OnAdded broadcast (vtable slot 6 / +0x30, retail 0x140108d40 ->
    // body 0x140108720) — the one-shot that fills the site's construction mesh list
    // (+0x58/+0x60) and picking list (+0x68/+0x70) at EntityList_RegisterEntity time and
    // reads the real build multiplier into +0x4c. Graduated 2026-07-27.
    //
    // Held opt-in for two sessions on two concerns, both now settled by measurement:
    //  * "It un-frames the founding town." It did, and that was OUR gap, not the slot's:
    //    the start-town spawner never opened retail's instant-build bracket around
    //    SetModel + RegisterEntity (gw+0xd0 bit 0), which is exactly how retail stops this
    //    slot from un-framing a pre-built building. Bracket ported; measured 0 resets.
    //  * "It changes the bridge's build target from x1 to x13, and no build has completed
    //    with a real multiplier." Wrong attribution: State1Enter (0x140108290) reads +0x4c
    //    from the SAME MultiObjectComponent finder (0x140004740) and runs AFTER
    //    registration, so it sets the target for every site that enters state 1 whether
    //    this slot ran or not. Measured with the flag OFF: bridge sites report
    //    "costQty=4 mult=15" and "mult=21" — the real span, not 1. The flag is
    //    target-neutral; there was never an unverified multiplier riding on it.
    // Positive evidence with it on: 9 slot-6 dispatches, the bridge master's mesh list
    // filled with all its segments (0 lazy BC8_EnsureMeshList fallbacks), and the organic
    // demo-house build still completes end-to-end (30/30 credits -> 8->2 -> OneShot +
    // Finalize), 0-byte crash log.
    // One visible consequence, faithful to retail: a site registered while the instant bit
    // is clear has its model un-framed until state 8 gives it construction frame 0 — i.e.
    // a placed foundation shows no building mesh until construction actually starts.
    "BANISHED_LIVE_BUILD_ONADDED",
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
    // PickingComponent vtable slot 6 (+0x30, OnAdded — retail 0x140128f50), so the pick
    // mesh registers inside ComponentEntity_OnAddedPass instead of from a hand-rolled call
    // after EntityList_RegisterEntity. Graduated 2026-07-28, user-verified in real play.
    //
    // The slot is not a nicety — the ORDER is the fix. OnAddedPass dispatches slot 6 in
    // component ARRAY order, and on a WoodHouse that is Model=[3] Picking=[5] Build=[16].
    // At [5] the model still carries the placement FOUNDATION DECAL (modelVariant 0), so
    // OnAdded's `modelVariant < 0` guard passes and the mesh registers; only at [16] does
    // BuildComponent's own OnAdded un-frame the model to -1 for the build ramp. Our
    // explicit call ran after RegisterEntity — effectively last — so it ALWAYS saw -1 and
    // bailed, and a hand-placed building was unclickable (measured: pickingId -1 -> 27).
    // RegisterMesh now also honours retail's `pickingId(+0x28) < 0` guard, which makes the
    // legacy explicit calls idempotent no-ops rather than double-registrations.
    "BANISHED_LIVE_PICK_ONADDED",
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
    // MapComponent slot 6's other half. Retail slot 6 is MapComponent::Draw (0x140124370) =
    // `if (MapDescription(mc+0x18)->+0x30) sub_41b0(mc)`, and 41b0 is paint (desc+0x32) AND
    // occupancy (desc+0x31 == 0) under one bit-0x100000 gate; our port wired only the paint,
    // so no register-time path ever claimed a cell. desc+0x30 is the "claim at spawn"
    // switch: natural resources set it and take their tile immediately, buildings leave it
    // clear and claim their footprint when ClearArea finishes (MAP_SOLIDIFY above).
    // Proven by the orchard: without it CreateNaturalResource spawns the tree, nothing
    // writes it into the tile record, and OrchardComponent::OnMessage(0) reads a null out of
    // *(mapGrid+0x80)[tile]+8. With it, 12/12 orchard slots bind (2026-08-10).
    "BANISHED_LIVE_MAP_SPAWN_CLAIM",
    // ZoneComponent vtable slots 6/7 (Draw 0x1401301f0 / OnCreate 0x140130080) -> sub_d360,
    // the writer of cell+0x00 — the FIRST slot EditTool's cell pick (FUN_1400a0280)
    // resolves. This is retail's placement-time cell registration: for every non-'.'
    // footprint cell it writes the owning entity into cell+0x00, notifies the pathfinder
    // mirror (a6f00 cmd 3), and bumps the '.'-reserve counter at cell+0x28; slot 7 is the
    // NULL-raster teardown. A WoodHouse carries a ZoneComponent at array index [1], so in
    // retail a just-placed foundation is cell-pickable immediately (user-verified in the
    // shipped game) even though it has no pick mesh. Everything below the slots (d360,
    // a6f00, the 88e20 zone-map insert) was ported AND registered for months; the inert
    // factory just shipped the stub vtable. Default-on 2026-07-28: A/B on clearplaceover
    // measured slot0 mine=16/empty=4('.') CELL-PICK OK vs control mine=0 DEAD, trees keep
    // their +0x08 cells, 0 crash; default and FAB-scenario runs clean.
    "BANISHED_LIVE_ZONE_RASTER",
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
    // ClearAreaComponent + ClearResourceComponent controllers (the state-0 harvest
    // dispatch, its per-frame done-test, and the state-1/2 finalize). Graduated
    // 2026-07-31, user-verified in real play: a pasture placed over trees clears and
    // the build ribbon's resource counter counts up as citizens chop.
    //
    // It was left opt-in when the clear wave closed, and two things had been quietly
    // riding on a flag nobody set:
    //   * The build ribbon's ClearArea counter reads ClearAreaComponent+0x48, and
    //     State0Enter is its ONLY writer. Inert controller => +0x48 stayed at its ctor
    //     zero while the footprint still held N standing resources, and the label
    //     (snapshot - stillStanding, unclamped in retail too — see buildui.cpp:420-429
    //     against 0x1400b1a50) rendered as "-9 out of 0". No arithmetic was ever wrong.
    //   * BANISHED_LIVE_MAP_SOLIDIFY above has been default-on since 2026-07-28 while
    //     being, by its own comment, "inert unless BANISHED_LIVE_CLEARCTRL is on" —
    //     ClearArea's state-2 ENTER is the only site in the exe that broadcasts event 1.
    //     So no placed building was registering its footprint cells at all.
    // Measured on: 31 [ClearCtrl] traces during the founding-town spawn (zero with it
    // off), every founding house taking the State0Enter `remaining == 0` short-circuit
    // exactly as the comment at clearareacomponent_states.cpp:438-442 predicts. That
    // short-circuit is load-bearing, not vestigial — do not "simplify" it to match
    // retail's helper_7960 shape without re-running that spawn.
    "BANISHED_LIVE_CLEARCTRL",
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

