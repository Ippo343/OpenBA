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
    "BANISHED_LIVE_STATUSICON",
    "BANISHED_LIVE_STATUSICON_DRAW",
    "BANISHED_LIVE_CITIZEN_STATUSICON",
    "BANISHED_LIVE_CITIZEN",
    "BANISHED_LIVE_CITIZEN_UPDATE",
    "BANISHED_LIVE_CITIZENAI",
    "BANISHED_LIVE_CHARACTER",
    "BANISHED_LIVE_CHARACTER_MOVETICK",
    // GRADUATED 2026-08-18 — the action Enter tick (CharacterComponent slot 9 ->
    // Character_UpdateAnimation -> action vtable +0x30). This is the ONLY dispatcher of
    // slot +0x30, so with it off HuntAnimal::Enter never ran: the hunter chased deer
    // forever, never killed, and the hunting cabin's production panel sat at 0 — which
    // read as "the panel display is dead" rather than "nothing writes to it".
    // A/B, seed 12345, demo hunter lodge staffed 3/3, ~8 min each arm:
    //   OFF: [ActionTick] never printed at all (slot 9 not installed), [HuntEnter] census
    //        never printed (Enter called 0 times), [TrackerUpsert] 0, panel items=0.
    //   ON : REACHED_KILL=6, [TrackerUpsert] UPSERT=18 NO_TRACKER=0 (venison qty 100/60,
    //        leather qty 4) all onto the LODGE entity, and ProductionUI list=0 items=2.
    //   No crash in either arm (standalone_crash.log empty at 459 s).
    // Its stale-char-slot hazard is covered: CharacterComponent::OnRemove (slot 7) is
    // installed by BANISHED_LIVE_CHARACTER_MOVETICK, which is default-on above.
    "BANISHED_LIVE_CHARACTER_UPDATE9",
    // GRADUATED 2026-08-29 — WorkComponent slot 7 (+0x38, 0x14012e6c0), the removal
    // hook that cancels the work this component posted (-> eb50 -> Goals::RemoveWork
    // 0x1400f8e30). Ghidra names it "OnCreate"; it is the REMOVE side, and nothing
    // installed it, so it had never run once. A raw-material pile posts a Pickup
    // through its WorkComponent and then dies on RawMaterialComponent::Update's expiry
    // timer -- with slot 7 empty the Pickup stayed in the goals trees pointing at the
    // freed pile, and the next save faulted in Work::Serialize (0x1400f7400) deref'ing
    // the dead target. This was the in-game "crash when saving", reported 2026-08-29.
    // Pickup::OnComplete returning false is retail-faithful (0x1400f2ec0 does the same),
    // so CompleteTask never removes a Pickup -- slot 7 is the ONLY route.
    // A/B, autostart town, probe = BANISHED_WORK_SER_PROBE:
    //   OFF: every Pickup leaked -- work ids 3..22, one per haul, 0 exceptions; save
    //        walked one and took an ACCESS_VIOLATION on a 0x1482_00000000 vtable.
    //   ON : 28 slot-7 cancels (ids 3..31), 0 DANGLING-CREATED, 0 STALE at save,
    //        SAVE DONE + Auto.sav 4.27 MB, crash log empty.
    // Paired with the retail ctor seeds (workId=-1, workFlags=0x80000000) the inert
    // factory was leaving zeroed -- load-bearing, since 0 is a VALID work id and an
    // unseeded component would cancel work 0 instead of no-oping.
    "BANISHED_LIVE_WORKCOMP_ONREMOVE",
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
    "BANISHED_LIVE_STORAGEDISPLAY",
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
    "BANISHED_LIVE_PICKING",
    "BANISHED_LIVE_PARTICLE",
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
    "BANISHED_LIVE_TRADE_COMPONENT",
    // The two controller state machines: TradeComponent's (merchant timer/spawn/dock)
    // and BoatComponent's own 5-state machine (the hull actually moving along its route).
    "BANISHED_LIVE_TRADE_CTRL",
    "BANISHED_LIVE_BOAT_CTRL",
    // CitizenComponent vtable slot 6 (+0x30, the OnAdded broadcast) -> OnAdd 0x140114000,
    // the append into the town citizen list townMgr+0xa0. Graduated 2026-07-26: without it
    // newborns were created, entered the world entity list, and joined no population system
    // at all — the town could never grow. 21 000-frame soak with it on: 24 -> 35 citizens,
    // fourteen births, no crash, and it makes StartTown_RegisterTownCitizens redundant
    // (that pass now reports 0 registered / 24 already done by the broadcast).
    "BANISHED_LIVE_CITIZEN_ONADD",
    // GRADUATED 2026-08-22 — tunnel terrain deformation, in two parts.
    //
    // The report was "a placed tunnel builds but has no depth at the mouth". The black
    // overlay a player sees there is DecalComponent (Terrain/TiledDecals/TunnelMaterial),
    // which was already live; the DEPTH is not a mesh or a sprite at all — it is a real
    // carve of the terrain heightmap, and nothing was performing it.
    //
    // TERRAFORM: TerraformComponent was spawned by COMPONENT_INERT_FACTORY, so every
    // instance carried the shared stub vtable. Its Shutdown and OnRemove bodies had been
    // ported by an earlier wave and were unreachable, and the three functions that do the
    // work had no port at all: 0x140056b50 (walk a TerraformDescription picking mesh,
    // transform each vertex by the owner's ModelComponent transform at +0x148, write the
    // heightmap), 0x140057120 (footprint terrain-type 10 -> 6) and 0x140057310 (vtable
    // slot 12, gated on msg 4). notes/component_goals.md called the component COMPLETE
    // and decomp/components/audit.md said "OnCreate passthrough"; both were wrong.
    //
    // MULTIOBJECT: slot 12 (0x140125490) is the child-entity message FORWARDER — it walks
    // the child array at +0x48 and dispatches slot 12 on every component of every child.
    // It was inert too, and that was the actual root cause: Tunnel.rsc's own
    // `TerraformDescription terraform {}` block is EMPTY. The floor/ceiling picking meshes
    // live on Template/TunnelStart.rsc — the two MOUTH SEGMENTS. BuildComponent state 8
    // broadcasts msg 4 through 0x1400c2b70, which walks ONE entity (the master), so
    // without the forwarder the segments that own the meshes were never told.
    //
    // POSITIVE RUN (2026-08-22, user-built tunnel, both flags on, 41 min uptime, empty
    // standalone_crash.log). Three components received msg 4 where one did before:
    //   master  FootprintPass rect=(77,122 15x3) converted=21
    //   mouth A ceiling verts=82 applied=13  floor verts=4 applied=4
    //   mouth B ceiling verts=82 applied=16  floor verts=4 applied=4
    // The two 1x1 entities are exactly the ends of the master's 15x3 span. User confirmed
    // the deformation and shadow render and that citizens path through the tunnel.
    //
    // applied < verts is the mask test working, not a partial failure: vertices whose
    // surrounding cells fail _ceilingModifyBits/_floorModifyBits are skipped by design.
    //
    // ⚠ MULTIOBJECT IS AN UNFILTERED RELAY — it forwards EVERY message id to EVERY
    // component of EVERY child of EVERY multi-part structure (bridges, roads, multi-tile
    // buildings), not just msg 4 on tunnels. It is faithful to retail 0x140125490, and
    // components that do not implement slot 12 hold stubs, but it is the widest-reaching
    // half of this wave. If a multi-part structure starts behaving oddly, kill-switch this
    // one first (BANISHED_LIVE_MULTIOBJECT=0) before suspecting the terraform half.
    "BANISHED_LIVE_TERRAFORM",
    "BANISHED_LIVE_MULTIOBJECT",
    // GRADUATED 2026-08-27 — quarry pit carve + decal verified together: msg-4 carve at
    // _initialDepth, Slot15 production re-carve, terrain chunks 119/120 rebuilt with the
    // carved heights (hmin=-358 = -1.4*256), pit + rock decal both visible on screen.
    // The final blocker was TerraformComponent_RegionRecalc silently no-oping on a null
    // s_fn_RegionDirty when no tunnel had initialized terraform's statics.
    "BANISHED_LIVE_QUARRY",
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
    // DestroyComponent state 6 — the COLLAPSE/RUBBLE arm fire and tornado take
    // (destroycomponent_state6.cpp). Verified 2026-08-23: with 2 houses force-ignited the
    // fire spread to a third and all 3 walked state-6 enter -> countdown -> rubble steps
    // (mesh variant 2 -> 1 -> 0, 8 steps over a 4-minute soak) with an empty crash log, no
    // null-vtable-slot skips in the action-abort walk, and no assert. Held opt-in for
    // exactly one run: it cancels live citizen actions and empties a live StorageComponent
    // mid-frame, which is the churn class that has faulted before. Without it the whole
    // disaster lane is cosmetically dead — a burning building keeps its intact mesh until
    // the fire's own state machine forces state 7.
    "BANISHED_LIVE_DESTROY_COLLAPSE",
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
    // The town hall's nomad-arrival state machine + the nomad walker
    // (immigrationcomponent_states.cpp / nomadcomponent_states.cpp). Graduated
    // 2026-09-05 after the Immigrants achievement (Ach27) was awarded natively three
    // ways: auto-ALLOW through the SM node, the panel's real Allow button, and a
    // DENY run whose nomads walked back out and were removed (docs/waves/
    // achievements_wave.md 5.7). Retail's timer puts a group 2..6 in-game years
    // apart, so a normal run only pays the idle/waiting tick.
    "BANISHED_LIVE_IMMIGRATION",
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
    // The orchard trio. OrchardComponent's controller (its 6-state table: 2 idle, 9 plant,
    // 10 tend, 12 replant, 11 harvest, 20 clear) plus the two components its yield depends
    // on — GrowthComponent (tree maturity at natRes+0x28) and FruitComponent (ripeness at
    // fruit+0x30). All three graduate together because none of them produces an observable
    // result alone: OrchardComponent without growth plants trees that never mature, and
    // growth without fruit matures trees that never bear.
    //
    // Graduated 2026-08-10, the session that closed the last two gaps in the chain. The
    // whole loop is measured end to end, not inferred:
    //  * plant   — grid built (count(+0x58)=12), slot histogram 12x state 1, 12 tiles bound
    //  * tend    — state 10 live, seasonal TendArea post/drop on the real month gate
    //  * harvest — 12/12 slots picked, msg=1 arriving with ASCENDING slot indices,
    //              HarvestOrchard executed=12, machine leaves state 11 for state 10
    //  * YIELD   — 6 piles x 50 reach town STORAGE (a new resType 0 -> 300, measured by
    //              summing every StorageComponent's inventory, twice on separate runs) and
    //              then tick DOWN to 285 as the town eats it. Storage was the missing
    //              measurement for three sessions: counting spawned piles is a different
    //              question, since a pile nobody hauls is a yield of zero.
    //  * cut     — the panel's CUT (cmd 3 -> node event 5) drives state 20, which clears
    //              the footprint and returns to state 9 to REPLANT (9->10->20->9->10)
    // Zero-byte crash log on every run above.
    //
    // ⚠ GROWTH and FRUIT are NOT orchard-only: GrowthComponent in particular is the finder
    // every runtime-spawned natural resource goes through, so this changes behaviour for
    // wild trees too. That breadth is the reason they were held opt-in this long; it is
    // also why they are worth defaulting on now that the resource pipeline they feed is
    // proven. Each remains a kill-switch (=0) for an A/B run.
    //
    // The BANISHED_ORCHARD_* harnesses stay opt-in and are NOT part of this: they are
    // player stand-ins (WORKERS seats a citizen, GROW fast-forwards maturity, FORCE_START
    // pokes the master switch, CUT clicks a destructive button), not features.
    "BANISHED_LIVE_ORCHARD",
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
    "BANISHED_SPAWN_DOSPAWN",
    "BANISHED_SPAWN_INITIAL",
    // The panel WORK CHECKBOX on the four non-food workplaces — component vtable slots
    // 13/14 (CanCommand / Command). Graduated 2026-08-15 on a measured A/B, not on a
    // quiet run: with the flags off the slot-coverage probe read the inert stub in both
    // slots on all four classes and every CanCommand id returned 0 (which is exactly what
    // greys the checkbox every frame via ControlUI_Slot7); with them on, both slots hold
    // real bodies and each class reports a class-correct, non-zero mask. A click through
    // slot 14 then CHANGES that mask on all four — Forester 0x01->0x03, Attendance /
    // ConsumeProduce / Market 0x03->0x01 — so this is "the button works", not merely
    // "the button is wired". Zero-byte crash log across the run.
    //
    // Retail inheritance reproduced exactly, byte-verified from each class vtable:
    // ConsumeProduce (0x140162a80) and Market (0x140159c40) inherit the ResourceProduce
    // base pair 0x14012b520/0x14012b5a0 verbatim; Forester (0x140163460) overrides with
    // 0x14011f920/0x14011f890 because it has TWO toggles (+0x50 plant / +0x51 cut); and
    // Attendance (0x140158ce0) overrides with 0x140043f10/0x140043e80 because its
    // checkbox IS the WorkPlaceComponent accepting flag.
    //
    // ⚠ ConsumeProduce and Market additionally needed the CONTROLLER_LIVE_FACTORY_STATES
    // promotion, and wiring 13/14 alone was measurably NOT enough for them: the base
    // slot 13 derives its mask from the state machine and the base slot 14 posts to the
    // component's own node, so with slots 16/17 stubbed a click measured 0x02 -> 0x02.
    "BANISHED_LIVE_CONSUMEPRODUCE_CMD",
    "BANISHED_LIVE_MARKET_CMD",
    // ── Forester / Attendance: promoted from "command pair only" to full live
    //    controllers 2026-08-15, which is why the two flags are renamed rather than
    //    added. BANISHED_LIVE_FORESTER_CMD / _ATTENDANCE_CMD gated slots 13/14 alone;
    //    the new gates cover 13/14 AND the state machine, so leaving the old names in
    //    the table would have silently dropped the command pair back to the inert stub
    //    (measured: a control run with the new gates unset reports 15/22 and 10/22).
    //
    //    Graduated on the same measurement that motivated them. Slot-coverage probe,
    //    identical seed and demo spawners, gates on vs off:
    //      Forester    17/22 -> 22/22   (state 13 PRODUCING, workplace accepting=1)
    //      Attendance  12/22 -> 22/22   (state  2 OPEN,      workplace accepting=1)
    //    Both classes' state machines now link and tick: Forester runs the SHARED
    //    ResourceProduce table (2 idle -> 13 producing -> 14 paused) and mints a real
    //    ForesterWork through slot 18; Attendance runs its OWN two-state table
    //    (2 open / 3 closed, node at +0x38 not +0x28), registering in the town
    //    attendance list, minting a LocationWork and posting it to the citizen ledger.
    //    Four-minute soak with Attendance's per-frame trans38 running every frame on a
    //    placed school: zero-byte crash log.
    //
    //    ⚠ Forester's ctor default is behaviour, not padding: ForesterComponent_Ctor
    //    writes +0x50 = 0x101 ("plant AND cut both ON"), so its factory is hand-expanded
    //    rather than using CONTROLLER_LIVE_FACTORY_STATES (whose PopulateFn cannot reach
    //    the instance). Without that write every lodge would be born with both work
    //    flags clear.
    "BANISHED_LIVE_FORESTER",
    "BANISHED_LIVE_ATTENDANCE",
    // The four remaining ResourceProduce food classes — graduated 2026-08-17. Held
    // opt-in long after each was individually verified, which made the DEFAULT build
    // the broken one: with the inert factory the state node at comp+0x28 is never
    // installed, so the shared ResourceProduce state machine never runs, slot 18
    // CreateWork is never called, and NO WORK IS EVER POSTED. Workers seat normally
    // (roster fills, accepting=1) and then stand around with nothing to claim — the
    // "we assign workers and nothing gets produced" report. Orchard and Pasture were
    // already default-on, which is exactly why only these four looked broken.
    //
    // A/B at seed 12345, FORCE_SPEED=40, 7200 frames, BANISHED_FOOD_DIAG=1200, demo
    // buildings placed by BANISHED_BUILD_DEMO_*. Every arm had the buildings staffed
    // and accepting in BOTH halves, so this measures production, not seating:
    //
    //   CropField + Fishing   OFF: dock registered(+0x50)=0 rate(+0x54)=0.000,
    //                              7 resource types, town food 4441 -> 1755 (pure drain)
    //                          ON: dock registered=1 rate=1.444, fish resType 161 -> 374
    //                              (+213), a 9th resType (crops) appears once a crop is
    //                              chosen, food 4663 -> 3093
    //   Hunt + Gatherer       OFF: HuntComponent progress(+0x50) pinned at 0 for the whole
    //                              run, Herbalist GATHERABLE climbing 15 -> 26 (nobody
    //                              picking), 7 resource types, food 4434 -> 1762
    //                          ON: progress 0 -> 9.57e16, GATHERABLE drained 15 -> 2,
    //                              11 resource types (four the OFF arm never had), food
    //                              4434 -> 2170
    //
    // Each ON arm asserts a value only the live path can produce (a non-zero dock rate,
    // a moving hunt progress, resource types that do not exist otherwise), so these are
    // positive runs rather than clean ones. No crash log in any ON arm.
    "BANISHED_LIVE_CROPFIELD",
    "BANISHED_LIVE_FISHING",
    "BANISHED_LIVE_HUNTING",
    "BANISHED_LIVE_GATHERER",

    // ── Workplace profession seat for non-constructed buildings ───────────────────
    // Seats WorkPlaceComponent+0x48 from the description on a workplace that never
    // passes through the BuildComponent hand-off. WorkplaceUI_Startup hides the whole
    // workPlace ribbon (worker up/down spinner AND stop-sign icon) when +0x48 == 0, so
    // a player-placed crop field or orchard had no worker arrows for its entire life.
    //
    // MEASURED 2026-08-18, player-built crop field + orchard:
    //   OFF: [WorkPlaceUI] 0 shown / 5466 HIDDEN, profession(+0x48)=NULL; the field is
    //        never staffable ([FoodWorkers] "+0x50=0" for 9500 frames).
    //   ON:  [ProfKey] SEATED NATURAL for CropFieldComponent AND OrchardComponent
    //        (both key 0x...778a68 - they share the Farmer profession), ribbon
    //        2 shown / 0 hidden, arrows visible and the buildings work.
    // The ON arm asserts a value only the live path produces (a SEATED NATURAL line
    // and a non-zero +0x48 on an entity with no BuildComponent), so this is a positive
    // run. Houses/storage keep key 0 - their description declares no profession - and
    // construction sites are excluded outright, so the Builder hand-off is untouched.
    "BANISHED_LIVE_SM_BUILT_HOOK",

    // ── The REAL WorkPlaceComponent controller (supersedes the hook above) ────────
    // Retail wiring, Ghidra-verified against vtable 0x140164d30, ctor 0x14012f2e0 and
    // the state-table builder 0x14012f3a0: state 0 enter = ee40 -> f520 "become a
    // Builder workplace", state 2 enter = edf0 "built: leave Builder, re-key to the
    // description's own profession", state 3 enter = ed60 "demolish: release + erase".
    //
    // Graduated 2026-08-20 to fix a REPORTED fault the stand-ins could not fix: the
    // SM_BUILT_HOOK above fires on the entity's FIRST TICK with no state gate, and a
    // player-placed crop field has no BuildComponent for its guard to catch. Measured
    // with BANISHED_SM_STATE_TRACE=1 on a field dragged over trees:
    //   OFF: [SMState] FIRST TICK CropFieldComponent state(+0x51)=0 immediately
    //        followed by [SMSeat] ... -> SEATING NOW. The Farmer key is installed at
    //        state 0 -- while the tile is still being cleared -- and since
    //        WorkPlaceUI_OnCreate (0x140087080) shows the workPlace ribbon whenever
    //        +0x48 != 0, the panel offered FARMER worker arrows on a construction
    //        site. Retail shows the Builder there and the Farmer only at completion.
    //   ON:  the controller's own node takes the 0 -> 2 transition, so [WPCCtrl]
    //        "state-0 (construction/f520)" precedes "state-2 (built/edf0)" and the
    //        natural key appears only on the second -- a value only the live path can
    //        produce (the stand-in never logs a state-0 enter at all).
    // Promoting this retires all three stand-ins, each already gated on
    // WorkPlaceComponent_ControllerLive(): BuildComponent_State8Finalize's explicit
    // edf0 call, the registration half of StartTown_SpawnStructures, and (added with
    // this graduation) StateMachineComponent_SeatWorkplaceOnBuilt.
    "BANISHED_LIVE_WPC_CONTROLLER",
    // GRADUATED 2026-08-22 — TornadoComponent (slots 3/4/8). Held opt-in as an
    // "unproven feature" until the particle half existed; with TornadoInstance::Update
    // ported (commit bb9a1f9) the whole mechanic runs end to end and has been watched
    // in-game, rendering.
    // POSITIVE RUN (BANISHED_FORCE_TORNADO=1, FORCE_SPEED=40): spawn -> 129-waypoint
    // fractal walk -> 340 trees felled over 7274 damage ticks -> "particle finished,
    // entity queued for destruction", crash log empty. Nothing about that outcome is
    // reachable with the gate off: the component owns the movement, so the funnel just
    // sits at its spawn point and damages nothing.
    //
    // ⚠ building=0 on all 7274 ticks. NOT evidence the building arm is dead — arm 3
    // (the (2R+1)^2 cell sweep) is the same code that produced the 340 tree hits and
    // calls the same counter; the forced path simply walked through forest, not town.
    // Aim a forced tornado at the town centre before concluding anything either way.
    // GRADUATED 2026-08-22 — the fire disaster lane itself (FireComponent vtable
    // slots 3/9/15: the per-frame burn tick, the spread roll, and the water-applied
    // message). Graduated together with BANISHED_LIVE_WELL below, which is the only
    // thing that can END a fire; the two were measured as a pair because either one
    // alone is only half a mechanic.
    // A/B, seed 12345, 24 extra houses, forced fire every 8000 frames, 12 min per arm,
    // with NO BANISHED_LIVE_* set in either arm -- the lane had to arm itself from this
    // table, which is the positive-run requirement and the demonstration in one:
    //   NO WELL: 27 fires, ALL ended reason=timer-expired, 0 buckets (water search
    //            missed 7000 of 7000), 48 further ignitions refused being-demolished
    //            as the fire kept reaching collapsing structures, and the town fell
    //            41 -> 20 citizens. Spread census caught=65 rolled-no=108 empty=219.
    //   WELL   : 7 fires, ALL ended reason=buckets-reached-threshold, 700 buckets
    //            peaking at 100/100, water search found=746 missed=4, and the town
    //            grew 26 -> 100 monotonically. Spread barely got a chance: 8 attempts.
    //   standalone_crash.log empty in both arms.
    // So the mechanic is complete in both directions: fire spreads and destroys a town
    // that cannot fight it, and the brigade stops every fire in a town that can.
    "BANISHED_LIVE_FIRE",
    // GRADUATED 2026-08-22 — WellComponent's controller state machine (state 2 "built"
    // -> TownManager::RegisterWaterSource). Until this lane existed only the UNregister
    // half was ported, so townMgr+0xE0/+0x140 were never written and the FireWork spiral
    // search returned found=0 on every run ever recorded, with wells standing in the town.
    // A/B, seed 12345, 24 extra houses, forced fire every 8000 frames, ~12 min each arm:
    //   OFF: 0 registrations, water search found=0 / missed=12000, 0 buckets, all 25 fires
    //        ended reason=timer-expired, and the town COLLAPSED 46 -> 30 citizens as the
    //        houses burned (45 further ignitions refused reason=being-demolished).
    //   ON : 1 registration, found=500 / missed=0, 500 buckets (peaking 100/100), all 5
    //        fires ended reason=buckets-reached-threshold, town grew 26 -> 99 monotonically.
    //   standalone_crash.log empty in both arms; no new UNRESOLVED/ABORTED/INCOMPLETE lines.
    // The two arms are frame-identical through 15000, i.e. the flag changes the fire
    // outcome and nothing else.
    "BANISHED_LIVE_WELL",
    "BANISHED_LIVE_TORNADO",
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

