# Economy / Goal-Directed Citizen Behavior — Sequenced Plan

**Branch:** `wave/phase3-keystone-d`
**Written:** 2026-07-01, after the A* pathfinding fix (commit 81fcfda).
**Purpose:** turn the "citizens fan out in a star" behavior into retail-faithful
start-game behavior (housed families, laborers hauling starting resources to the
barn, gathering/eating food, working jobs). This is the Phase-3 exit criteria =
the work/economy loop. It is a multi-phase keystone; each phase below is designed
to be spun out to a successive agent.

> **⚠️ CORRECTION (2026-07-01, Ghidra-verified — see `reference_citizencomponent_field_map`):**
> The candidate-list `+0x68` gate is **the active DISEASE** (`External<Disease>`), NOT
> housing and NOT profession. Both prior theories were wrong. Healthy citizen ⇒ `+0x68==0`
> = the normal state; it does **not** block work. (Cross-checked live: housed+healthy
> citizens show `+0x68==0` with residence `+0x248` set.)
> - **The real employment gates are `+0x1f0` (profession object) + `+0x238` (workplace).**
>   Both are empty at start because the economy components are **inert**, so the town work
>   registries never populate: `townMgr+0x60` (work-task list, read by FindWorkplace),
>   `townMgr+0x100` (profession BST, read by FindAvailableBuilding), `gameWorld+0x118`
>   (tile work-node BST). No registries → `FindWorkplace` returns nothing → no `+0x238` →
>   citizens can only Idle/Leisure/Wander (the star-scatter).
> - `AssignHousing` sets `+0x248`; `AssignWorkplace` (0x1401163d0) sets `+0x238`/`+0x240`;
>   profession `+0x1f0` is set only by the ProfessionUI worker-count path (no auto-assign
>   at game start — fresh citizens stay Laborer). Laborers still Gather/haul once a work
>   task/workplace exists.
> - **Housing (Phase 2) is ALREADY IMPLEMENTED** (`BANISHED_LIVE_RESIDENCE` +
>   `BANISHED_LIVE_HOUSING`, verified live: 6 houses assigned, families rostered,
>   crash-free) — but it does NOT unlock Work. Re-scope Phase 2 to "wire housing into the
>   default combo + feed births/food."
> - **Reordered critical path:** Phase 1 = graduate StorageComponent + WorkComponent +
>   WorkPlaceComponent live + seed the town registries (esp. the `townMgr+0x100`
>   per-profession node-CREATE, the one open RE gap) → then CitizenComponent::Update
>   (already gated) drives FindWorkplace/AssignWorkplace → laborers get tasks and act.

---

## Current state (measured 2026-07-01, full live-citizen combo + traces)

**What already works — do NOT re-investigate:**
- **A* pathfinding is correct and grid-faithful.** Every `[PFQuery9cfb0]` returns
  `READY-with-PATH`; paths stay inside the walkable region (`rgn→same rgn`,
  `type=2`, `mask=0xe1f`). Citizens follow real routes (`[MoveTick]` shows smooth
  per-cell deltas, terrain-clamped z, facing updates). **The user's "grid/restriction
  rules" concern is resolved.** (Bug A AVL contiguous-locals + Bug B Vector2::Dot ABI,
  committed 81fcfda — see [[project_citizen_pathfinding_zero_dest]].)
- The whole decide→select→push→execute→move-tick pipeline is live (env-gated). The
  CitizenAI candidate brain, 14 state handlers, executor, and move-tick all run
  crash-free. See [[project_phase3_keystone_d]].

**Why citizens still star-scatter (this is FAITHFUL, not a pathing bug):**
The candidate builder (`CitizenAI_BuildCandidateStateList`,
`code/Game/Component/citizenai_candidatelist.cpp`) is a faithful port. For a fresh
start-town adult it produces `{SeekHappiness p5, Education p3, Idle p2, Leisure p2}`.
SeekHappiness/Education fail (no tavern/school), so the citizen falls through to
**Leisure → random cell** = the "star fan-out". This is exactly what retail does for
an **unhoused, unemployed** citizen. The gates, measured live:

| Gate | Field | Live value | Effect |
|------|-------|-----------|--------|
| Disease | `CitizenComponent+0x68` (active illness) | `0` (healthy) | NOT a work gate — healthy is normal; the SICK+task fast-path only |
| **Workplace** | `CitizenComponent+0x238` | `0` (no job) | Gather (id6/7) skipped; the primary gate — empty because registries unbuilt |
| Profession | `CitizenComponent+0x1f0` | Laborer default | no auto-assign; laborers still gather once a task exists |
| Residence | `CitizenComponent+0x248` | set by housing (done) | housing mechanics only — does NOT gate Work |
| Hunger | `+0x44` | **stuck `0.0`** (thr 10/18) | eat/food candidate never fires — **BUG** |
| Thirst | `+0x48` | **stuck `0.0`** | drink candidate never fires — **BUG** |
| Resources | `HasSufficientResources` | returns FALSE | *would* add Gather, but needs a workplace |

So there are **two independent problems**:
1. **No world-state** (no housing assigned, no jobs assigned, buildings' economy
   components inert) → the AI correctly picks idle/leisure. This is the bulk of the work.
2. **Needs (hunger/thirst) don't accumulate** → a real, isolated bug that kills the
   food-seeking fallback. Cheap to fix, high visible payoff.

---

## The gating chain (dependency order)

```
Phase 1  Building economy components LIVE + town registry populated
             │  (StorageComponent, ResidenceComponent, WorkPlaceComponent,
             │   AttendanceComponent live; buildings registered in gameWorld+0xf0)
             ▼
Phase 2  Housing assignment ─────────────► citizens housed (+0x68)
             │                                   └─► Work becomes a candidate; go-home behavior
             ▼
Phase 3  Needs accumulation + food/storage consumption
             │  (fix hunger/thirst; ConsumeFood pulls from stocked barn; stock the barn)
             ▼
Phase 4  Job / workplace assignment + laborer default ─► citizens employed (+0x238)
             │                                   └─► Gather/Work states; hauling
             ▼
Phase 5  ✅ Production execution end-to-end (RemoveNaturalResource → FUN_1401260f0 → RegisterRawMaterial;
             │  town stock increments, verified; entity-lifecycle side gated pending materialization)
             ▼
Phase 6  ✅ DONE — citizen MoveTo/arrival steering FIXED (curCell fix) → citizens NAVIGATE + arrive;
             │  organic forester cut sequence drains to RemoveNaturalResource crash-free
             ▼
Phase 7  ✅ ORGANIC PRODUCE LOOP CLOSED (cut-sequence target +0x28 → tree, committed bc755c5).
             │  Population response (hunger relief / births / deaths / depletion) = already-ported
             │  CitizenComponent::Update code, runs crash-free by default; full food-relief + birth/
             │  death end-to-end gated on live stocked FOOD storage (Phase-1 remainder) + entity
             │  lifecycle (BANISHED_PRODUCE_SPAWN_ENTITY).  [was Phase 6]
```

Phases 1→2→4 are the critical path for "citizens do purposeful things." Phase 3 can
run in parallel with 2/4 (it's mostly independent) and delivers the "food part" the
user specifically asked about with the least effort — **consider doing Phase 3's
hunger-accumulation fix FIRST as a quick win** (see note under Phase 3).

---

## Cross-cutting facts (read before any phase)

- **Most economy component CODE is already ported but INERT** (valid spawn, right
  type-id, no Update body). Bring-up = `COMPONENT_LIVE_FACTORY[_S]` + fill standalone
  gaps + verify. This is mechanical, not fresh RE. See the GrowthComponent worked
  example and `component_factory_base.h`.
- **Do NOT fill shared `.rdata` type-id globals** (`DAT_1401c7xxx`) — it un-dormants
  ~30 readers and causes cascade crashes. Use self-contained
  `Object::GetTypeIdByName("<Name>")` inside each reimpl (the proven citizen-brick
  pattern). Verify each id via its `*_RegisterType` DATA writer in Ghidra.
- **Town manager** = `*(gameWorld+0xf0)`. Known offsets: `+0x170` resource-stock
  records (stride `0x20`: `+0` count / `+8` cap / `+0x10` produced-accumulator);
  `+0x100` profession BST; `+0x20` residence list; `+0xb0` ProfessionList (already
  live). Verified empty (`bldgCount=0`) — buildings spawn but are never registered.
- **Key component type-ids** (verify via RegisterType before use): StorageComponent
  `c7e78`, ResidenceComponent `c7cf0`, WorkPlaceComponent `c80e0`, WorkPlaceDescription
  `c8118`, AttendanceDescription `c3f78` (already a REAL factory, capacity @desc+0x20
  default 15), WorkComponent `c8070`, RawMaterialComponent `c7b30`, RawMaterialDescription
  `c7cb8`, MapComponent `c79e0`, InteractComponent `c7900`.
- **Test env combo** (from project dir, NOT cd'd):
  `BANISHED_SPAWN_ADULTS=1 BANISHED_LIVE_REGIONMAP=1 BANISHED_CITIZENAI_TRACE=1 mingw32-make sa-run`
  plus the default SA_LIVE_ENV (toolbar/citizen/character/movetick/statemachine).
  Logs: `C:/GOG Games/Banished/standalone_error.log` (stderr, world-build + traces),
  `standalone_debug.log` (stdout). Wait ~40s, `taskkill //F //IM reimpl-x64.exe`.
- **Verification method** (proven all session): symbolize crash RVAs via
  `nm reimpl-x64.exe | grep ' [tT] '` + awk nearest-symbol-below `0x140000000+RVA`.
  A/B every change: default build (envs off) must stay byte-identical + crash-free.
- **Process:** prefer opus subagents for RE/port; LEAD verifies every offset vs the
  Ghidra disasm; disjoint-file contract when fanning out (each agent writes only its
  own `<unit>.cpp` + `reg_<unit>.cpp`, never touches `address_registry.cpp`/builds —
  lead does the single wire+build+verify pass). See [[feedback_prefer_opus_for_complex_re]],
  [[feedback_verify_final_file_not_agent_report]], [[feedback_agent_merge_safety]].

---

## Phase 1 — PROGRESS (2026-07-01)
- ✅ AVL registry insert cluster already ported+registered (Keystone A `keystone_a_restree.cpp`):
  node-alloc 0x140017460 / find-or-create 0x1400172f0 / wrapper 0x140017230 / rebalance
  0x140014b40 / value-ctor 0x140015ec0; array-ADD 0x140003110 (reg_action_hunt);
  TownManager ctor 0x140090a30 = Load step 10. 🔑 `FUN_140002cc0`=REMOVE, `FUN_140003110`=ADD.
- ✅ **`AddWorkplaceToProfessionTree` (0x1400895a0) ported + registered** — new
  `code/Game/Game/town_workplace_registry.cpp` + `reg_town_workplace_registry.cpp`,
  wired in address_registry.cpp, builds clean. Exposes `TownRegistry_RegisterWorkplace(townMgr,
  wpComp)` for the spawn wiring. DORMANT (no caller yet).
- ✅ **`sub_0e70` job-finder ported faithfully** (citizencomponent.cpp — replaced the stub with
  the full townMgr+0x20 scan; runtime-verified: runs crash-free for all ~22 citizens; A/B-safe —
  faithful no-op when townMgr+0x20 empty, default build byte-clean, world builds 10951 ents).
- ✅ **townMgr+0x20 population wired** — StartTown_SpawnWorkplace appends the ForesterLodge entity
  IBP via FUN_140003110 (regCount 0→1). Diagnostic env `BANISHED_PHASE1_EMPLOY`.
- ✅ **REFRAME (2nd RE agent, template+xref verified): `sub_0e70` is the RESIDENCE/household finder
  (townMgr+0x20, needs ResidenceComponent c7cf0 = houses only), NOT the workplace path.** The
  ForesterLodge legitimately has no c7cf0 (WoodHouse.rsc has `residence`; lodge has `work`+`workplace`
  only) — adding one would be UNFAITHFUL. **Forester employment = townMgr+0x100 profession BST +
  FindAvailableBuilding (0x14008d100), and is PLAYER-DRIVEN via the ProfessionUI worker-count
  cascade (impl_dd10 0x14007dd10 → cbf0/ca20/c5f0/c4b0). There is NO organic auto-assigner** (the
  optimizer only rebalances existing workers). See [[reference_citizen_employment_path]].
- ✅ **Registration corrected to townMgr+0x100** (starttown.cpp `TownRegistry_RegisterWorkplace`) —
  the faithful list for a workplace. Self-skips because the inert WorkPlaceComponent's profession
  key (+0x48) reads 0 (populated by WorkPlaceDescription Serialize/Startup — Phase 5). Dormant +
  faithful; A/B-safe, crash-free.
- ✅ **PHASE 1 INFRASTRUCTURE COMPLETE (committed).** The employment plumbing is in place:
  town_workplace_registry.cpp (AddWorkplaceToProfessionTree), sub_0e70 residence finder (faithful),
  townMgr+0x100 registration wiring. Actual worker ASSIGNMENT deferred to **Phase 5 (ProfessionUI)**.
- 📋 **PHASE 5 HANDOFF (worker assignment):** (1) bring WorkPlaceComponent live enough to populate
  +0x48 profession key (from WorkPlaceDescription) so TownRegistry_RegisterWorkplace actually inserts;
  (2) port FindAvailableBuilding (0x14008d100) + the profession-assign cascade impl_dd10/cbf0/ca20/
  c5f0/c4b0; (3) wire the ProfessionUI worker-count widget to drive it. Then a citizen gets the
  profession (+0x1f0) + workplace (+0x238) → Gather(6)/Work(5) candidate → paths to the lodge.

## Phase 1 — Building economy components live + town registry

**Objective:** make the start-town buildings (StorageBarn, StorageYard, 6 WoodHouses)
*real*: their StorageComponent/ResidenceComponent/WorkPlaceComponent/AttendanceComponent
components have live Update/Startup, hold real counts/roster slots, and the buildings
are registered into the town manager (`gameWorld+0xf0`) so citizen finds resolve.

**Why first:** every later phase does `Entity_FindComponent_*` on a building and reads
storage counts / roster slots / workplace descriptors. Today those return null or read
garbage from inert components, and the town registry is empty. Nothing downstream works
until buildings are real.

**Entry points:**
- Component bring-up recipe: `COMPONENT_LIVE_FACTORY_S` in `component_factory_base.h`
  (GrowthComponent worked example). Candidates to graduate: StorageComponent (c7e78),
  StorageDisplayComponent, ResidenceComponent (c7cf0), WorkPlaceComponent (c80e0).
  AttendanceComponent/Description already real.
- **Town-registry insert — FULLY MAPPED (Ghidra, `reference_town_registry_node_create`).**
  TownManager = the mislabeled `TradeComponent` at `*(gameWorld+0xf0)` (ctor 0x140090a30,
  all BSTs start empty; retail fills them LAZILY per-workplace). The `+0x100` profession-BST
  node-create = **`AddWorkplaceToProfessionTree` 0x1400895a0** (find-or-create AVL insert).
  🔑 CORRECTION: `FUN_140002cc0`=REMOVE, `FUN_140003110`=ADD (so WorkPlaceComponent::OnCreate
  is the *un-register* path). **Recipe (option B, faithful):** port the AVL family
  (node-alloc 0x140017460 MemAlloc(0x38), find-or-create 0x1400172f0, wrapper 0x140017230,
  rebalance TradeWork_impl_4b40, value-ctor helper_5ec0, array-grow 0x140003110), then per
  fully-built workplace at spawn call `AddWorkplaceToProfessionTree(townMgr,
  &workplaceEntityRef, professionKey)` (key=`WorkPlaceComponent+0x48`, ref=`*(WPC+0x10)`) AND
  append the AttendanceComponent to `townMgr+0x60` (FindWorkplace `sub_1930` 0x140111930
  scans +0x60, not the BST — both must be populated). Node = 0x38B: +0x00 key/+0x08 arr/
  +0x10 count/+0x14 cap/+0x18 parent/+0x20 left/+0x28 right/+0x30 balance.
- ⚠️ **Default Easy start-town has NO employment workplace** (houses' WorkPlaceComponent =
  residence roster, key 0; StorageBarn doesn't employ). A real producer (ForesterLodge, via
  the existing `BANISHED_LIVE_WORKPLACE` harness — see [[project_economy_loop]] forester
  half) must be spawned + registered for a citizen to actually get a job.
- Existing spawn site: `code/Game/Game/starttown.cpp` (already writes MapComponent tile
  coords at +0x28..0x2e; already spawns fully-built meshes). Registry seed belongs here.

**Verify:** after spawn, `Entity_FindComponent_Storage(barn)` returns non-null with a
readable count; the town registry enumerates the spawned buildings (bldgCount > 0);
`HasSufficientResources` reflects the barn's actual stock.

**Agent spin-out:** one agent per component family (Storage / Residence / WorkPlace),
disjoint files; lead integrates + does the registry seed. ~2-3 agents.

---

## Phase 2 — DONE (2026-07-01)

**✅ PHASE 2 COMPLETE — housing wired into the default combo + verified live.**
Per the re-scope note at the top of this file, housing was already *implemented*
(the founding-family `AssignHousing` path in `starttown.cpp`); Phase 2's remaining
work was to enable it by default and verify. Done:
- **Makefile `SA_LIVE_ENV`** now includes `BANISHED_LIVE_RESIDENCE=1
  BANISHED_LIVE_HOUSING=1`, so a plain `mingw32-make sa-run` (and the launch.json
  configs that mirror it) launch with housed founding families — no hand-set envs.
- **Verified live (multiple runs, crash-free):** `[startTown::housing]` reports
  all 6 founding families rostered into 6 captured WoodHouses (~21-25 members
  housed depending on RNG child counts); each member's residence ref
  (`CitizenComponent+0x248`) is set AND the house's `WorkPlaceComponent` occupant
  roster (`+0x60`) matches the family size; the deferred occupied-house mesh
  visuals apply at frame 3 ("crash-free"). Zero REDZONE/exception markers.
- **A/B safe:** the change is launch-time env only (a make-recipe var list), so the
  compiled binary is byte-identical; the `LiveEnabled()` code gates still no-op on a
  bare direct launch (envs unset). No rebuild semantics changed.
- **Boundary note:** housing does NOT change the candidate set (still
  `{SeekHappiness p5, Education p3, Idle p2, Leisure p2}`) — this is faithful. A
  housed, jobless, fed citizen leisure-wanders in retail too. "Path home" behavior
  is gated on **Phase 3 (needs)**: needs are stuck at 0, so citizens never get a
  sleep/eat trigger to return home. Work (id5) is gated on **Phase 4 (workplace)**.
  So the correct next phase for visible goal-directed behavior is **Phase 3** (the
  hunger-accumulation quick-win) and then **Phase 4** (workplace assignment).
- **Organic Update path** (`CitizenComponent::Update` AssignHousing, gated on
  adulthood at citizencomponent.cpp:682) is a backstop for aged-up/new citizens; it
  no-ops for start-town children by design. The founding-family path is the faithful
  start-town mechanism and is the one wired on.

## Phase 2 — Housing assignment (original spec, superseded by the DONE block above)

**Objective:** `CitizenComponent::Update`'s AssignHousing finds an available WoodHouse
ResidenceComponent and links it → citizen housed (`+0x68 != 0`). Family members share a
house. Housed citizens stop homeless-wandering and (with a job) can Work.

**Blocking dependency:** Phase 1 (ResidenceComponent live + findable + registered).

**Entry points:**
- `CitizenComponent::Update` AssignHousing section (~citizencomponent.cpp:633,
  `LAB_140113873`). Today gated behind age (fixed via SPAWN_ADULTS) and
  `CanFindHousing` (0x140115be0). CanFindHousing currently bails because no residence is
  findable — Phase 1 unblocks it.
- The 6 WoodHouses spawn fully-built (commit c0b2faa) but their ResidenceComponent is
  inert → not assignable. Phase 1 makes them assignable.

**Verify:** `[CandList]` shows `housed=1` for citizens; candidate set gains Work (id5);
citizens path to their house instead of random cells.

**Agent spin-out:** single agent (one Update section + CanFindHousing) after Phase 1.

---

## Phase 3 — PROGRESS (2026-07-01) — ★ QUICK-WIN DONE (needs now accumulate)

**✅ NEEDS-ACCUMULATION QUICK-WIN COMPLETE + VERIFIED.** Root cause of the "hunger/thirst
stuck at 0.0" bug was found and fixed:

- **Root cause (NOT any of the three hypothesized candidates).** Instrumented the hunger
  branch live: `gameFlags=0` (accumulation branch taken, not the `>>1&1` zeroing),
  `rate(desc+0xC0)=100.0`, `div(DAT_14017225c)=3600.0`, no disease — all fine. The bug
  was **`dt_sec = System::Time::AsSeconds(deltaTime)` returning 0.0** every tick, so
  `hunger += dt_sec*rate/div` was always `+= 0`. Same `dt_sec` feeds age + thirst, so
  those were frozen too.
- **Why dt_sec was 0:** `citizencomponent.cpp` read the `Time::AsSeconds` fn ptr from the
  **wrong dispatch slot** `0x140152a70` (which actually resolves to `Math::Vector3::operator==`).
  The correct slot is **`0x140151318`** (Ghidra: first indirect CALL in
  `CitizenComponent::Update` @0x140112eb4 = `CALL [0x140151318]`). It was also unregistered
  in standalone, so the read returned NULL → `dt_sec` fell back to `0.0f`. Likewise the
  `Time::Time(float)` slot was wrong (`0x140152a80` = `SoundEffect::_typeInfo`); correct is
  **`0x140152b00`** (used by the food/fuel-consume loops; already registered).
- **Fix (2 files, minimal, A/B-safe):** (1) `citizencomponent.cpp` — corrected both slot
  addresses; (2) `standalone/address_registry.cpp` — registered `0x140151318` →
  `?AsSeconds@Time@System@@QEBAMXZ` (`0x140152b00` was already registered). DLL mode is
  unaffected (the exe loader fills these slots itself); byte-faithful accumulation logic.
- **Verified live (default sa-run combo, no accel):** `dt=0.01667` (60 Hz), hunger climbs
  `0.088→0.177→…→0.711` and thirst `0.126→…→1.022` — steady linear accumulation toward the
  warn thresholds (hunger 18 / thirst 15, read from `desc+0xCC`/`+0xEC`). World builds,
  housing applies, process stable, **crash-free**.
- **Food-seeking candidate CONFIRMED (accelerated verify run):** once hunger/thirst cross
  their warn thresholds, `CitizenAI_BuildCandidateStateList` puts **id 0xf (thirst) / id 0xe
  (hunger) at priority 7 at the TOP** of the candidate list, outranking SeekHappiness(5)/
  Education(3)/Idle/Leisure(2). i.e. a hungry/thirsty citizen now selects the food/water-
  seeking state instead of leisure-wandering — the "go eat" behavior the user asked about.
- **Deferred to Phase 1 (live stocked storage):** the downstream of the eat/drink state —
  physically pathing to a **stocked** StorageBarn, `ConsumeFood`/`ConsumeFuel` pulling food,
  and the barn count decrementing — still needs a live StorageComponent with real stock
  (buildings' economy components are inert; town registry `gameWorld+0xf0` unbuilt). The
  candidate now fires; the consume no-ops until storage is live. Fold full-consume into the
  Phase 1 Storage agent as originally planned.

## Phase 3 — Needs accumulation + food/storage consumption ★ quick win available

**Objective:** hunger (`+0x44`) / thirst (`+0x48`) accumulate over time; the food-
consumption loop (`ConsumeFood`, citizencomponent.cpp:582-593) actually pulls food from
a **stocked** storage barn; the barn starts with food. Then hungry citizens generate a
food-seeking candidate and path to food — the "food part" the user asked about.

**★ QUICK WIN (do this before/independent of the rest):** hunger is stuck at exactly
`0.0` even though the accumulation code exists (citizencomponent.cpp:389-396,
`+0x44 += dt_sec*hungerRate/hungerDiv`). Root is one of: (a) `gameFlags>>1 & 1` set
(zeroes hunger, line 395); (b) `hungerRate = desc+0xC0` reads 0 (rdata-null fallback —
see [[reference_standalone_rdata_null_fallback]]); (c) the housed+employed zeroing at
lines 385-386 mis-firing. Instrument these three, fix, and hunger will climb toward the
threshold (10) → the top-priority eat candidate fires. **This is a small, isolated,
high-visibility fix.** NOTE: verify what state-id the hunger candidate (id `0xe`/`0`)
actually dispatches to and that its handler paths to storage/food (the id→handler map in
the memory may be stale; confirm in Ghidra `ValidateAndEnterState` 0x14010f1f0).

**Blocking dependency:** the *quick win* (accumulation) has none. Full food consumption
needs Phase 1 (a stocked, live StorageComponent to eat from).

**Entry points:** citizencomponent.cpp:305-409 (aging/hunger/thirst),
582-606 (food/fuel consume loops), `ConsumeFood` (0x140112990). Barn stocking:
`starttown.cpp` + `RegisterRawMaterial` (0x140088ef0, Phase-B-verified accounting) to
seed starting food into `townMgr+0x170`.

**Verify:** `[CandList]` shows `hunger` climbing; once past threshold, an eat candidate
appears at top priority and the citizen paths to the barn; barn food count decrements on
consume.

**Agent spin-out:** one agent for the accumulation quick-win; fold full consume into
Phase 1's Storage agent (needs live storage).

---

## Phase 4 — ✅ COMPLETE (2026-07-01) — citizens get employed + enter work states

**✅✅ PHASE 4 DONE + COMMITTED (dcf1f5b).** Exit criteria met: citizens are assigned a workplace
(+0x238) + profession (+0x1f0), the CitizenAI candidate set gains Gather(6)/Gather2(7), and the
employed citizens DISPATCH the Gather work-states. With the lodge registered + findable (FIND half
below), `StartTown_EmployForesters` (starttown.cpp, env BANISHED_LIVE_WORKPLACE) seats idle adult
laborers via `CitizenComponent_AssignWorkplace` (brick 4-2) + sets +0x1f0 = forester `Profession*`
— driving the SAME outcome as the player-driven ProfessionUI worker-count cascade programmatically
(avoids the intricate distance-optimizer cbf0). Verified live: "employed 4/4 forester(s)", CandList
gains {id=6}{id=7}, citizens dispatch [Gather] (bail "no target" = the Phase-5 harvest boundary),
crash-free; A/B default byte-clean. Citizen source = founding-family CitizenComponents captured during
housing (townMgr+0xa0 is unpopulated in standalone). NEXT = Phase 5 production execution.

**✅ FIND HALF DONE + COMMITTED (05197b0): `FindAvailableBuilding` (0x14008d100) ported + the
spawned ForesterLodge is now FINDABLE.** It registers into the townMgr+0x100 profession BST and
`FindAvailableBuilding(profKey)` returns the lodge — live-verified `[Phase4Employ] ... MATCH`,
crash-free; default combo byte-clean. Three fixes: (1) **keystone_a_restree MemAlloc bug** — the AVL
node alloc resolved NULL in standalone (slot 0x140152b60 unregistered + `SAPEAXII` export absent) so
the BST insert silently failed (count++ but root null); added the proven `SAPEAX_KH` fallback →
un-breaks the whole keystone-A restree family. (2) seat WPC+0x48 profession key from the lodge's
WorkPlaceDescription (desc+0x20 = forester `Profession*`). (3) set WPC+0x50 = 1 (workplace-enable flag;
retail sets it via the activation tail `Entity_impl_fc00(WPC,1)` 0x14012fc00 — FindAvailableBuilding's
reachability gate is `FUN_140002e10`→WPC+0x50). Capacity recovered via CalcMaxWorkers (0x14012e5c0 → 4).
**REMAINING = the ASSIGN half:** the ProfessionUI worker-count seat cascade `impl_dd10` (0x14007dd10,
UI-bound) → cbf0/ca20/c5f0/c4b0 → AssignWorkplace (done) → citizen+0x238 → Work/Gather. cbf0 is a
~200-line distance-optimizer setting +0x1f0/AssignHousing — needs careful RE of the +0x1f0→+0x238 chain.



**✅ BRICK 4-2 DONE + COMMITTED (adf0d94): `CitizenComponent::AssignWorkplace` (0x1401163d0)
+ roster helpers ported.** `code/Game/Component/citizen_assignworkplace.cpp` +
`standalone/reg_citizen_assignworkplace.cpp`. Faithful 1:1: AssignWorkplace (set
+0x238/+0x240; remove-from-old-roster helper_a550 0x14012a550 / add-to-new-roster
helper_a710 0x14012a710 via `Entity_FindComponent_Storage`; recurse family list
+0x260/+0x268), CountWorkers 0x140129ff0, RosterCompare 0x140129e20 (all Ghidra-verified,
lead + agent). Roster bookkeeping (storage+0x50 `{data,count,cap}[]` stride 0x10, +0x58
profession count) is the faithful core, unconditional; every mesh/status-icon/3D-audio
side effect is inert in standalone and guarded to no-op (`AudioScene()=*(gw+0x78)` null;
null-wrapped MeshInstance/model IBP skipped). Registered → the organic
`sub_0e70`→AssignWorkplace path now sets +0x238 for real (was NULL/skipped). A/B GREEN:
default economy combo crash-free, candidate sets byte-identical, housing intact; also
un-nulls the `ResidenceComponent::OnCreate`→helper_9ef0 eviction path (verified safe live).

**🔑 KEYSTONE RE-MAP (corrects prior notes; all lead-Ghidra-verified this session):**
- `FUN_140002cc0` = **REMOVE** (search/compact/decrement), `FUN_140003110` = ADD. So
  `ResidenceComponent::OnCreate` (0x14012a0e0 = helper_9ef0 evict + `002cc0` on townMgr+0x20)
  **UN-registers** from townMgr+0x20 — it is teardown, NOT the registrar. (An earlier recon
  claim that OnCreate registers into +0x20 was WRONG.) The real +0x20 ADD site is still
  UNPINNED (not any Residence/WorkPlace/Work OnCreate).
- `Entity_FindComponent_Storage` (0x140008090) matches **c7cf0 EXACTLY** (GetTypeId==c7cf0).
  So whatever sits in `citizen+0x238` MUST carry a literal `c7cf0` ResidenceComponent
  (worker-roster). `c7cf0` is instantiated **only** by `ResidenceComponent_Factory`
  (0x140037440) from a **description's component list** — NOTHING bolts a c7cf0 onto an
  already-built entity. ⇒ the ForesterLodge top entity (21 comps, no c7cf0) can NEVER be the
  workplace; the roster lives on a **separate MultiObject job-host SUB-ENTITY** (its own
  description lists residence-roster + InteractComponent c7900), and `citizen+0x238` points
  at THAT sub-entity. `FUN_14012f520` (WorkPlaceComponent activation) seats `WPC+0x48`
  profession from `*(gw+0xb0)+0x38` and registers into townMgr+0x100 / gw+0x118 — but does
  NOT create a c7cf0 and does NOT touch townMgr+0x20.
- **Candidate gates (citizenai_candidatelist.cpp / 0x14010dca0, verified):** Gather (id6/7)
  needs `citizen+0x238`; **Work (id5) needs `citizen+0x68`** — main branch: `+0x68==0` →
  FindHousing/Education (this is the LIVE state → why no Work), `+0x68!=0` → Work. Early
  branch: `+0x68!=0 && +0x228!=0` → Work. AssignHousing sets +0x248 NOT +0x68. **What sets
  +0x68 (and whether it's a cheap unlock vs the disease field) is the open decision point.**

**Live baseline (full economy combo, crash-free):** `sub_0e70` runs per adult citizen but
`townMgr+0x20` regCount=0; candidate set `{SeekHappiness8, Education3, Leisure12, Idle9}`,
aiState=12 leisure-wander. Lodge spawns (BANISHED_LIVE_WORKPLACE) but has no c7cf0; all
profession keys=0.

**✅ townMgr+0x20 ADD site PINNED (recon, disasm-verified): `ResidenceDescription_helper_9de0`
(0x140129de0)** — `FUN_140003110(townMgr+0x20, *(townMgr+0x28)+1, &(*(comp+0x10)))` grow+append.
Registered as ResidenceComponent lifecycle-event-2 handler via `ResidenceDescription_helper_a350`
(0x14012a350) → dispatched through the ResidenceComponent controller jumptable (0x140037a00 →
DAT_1401da9a8), which is why it's not a plain vtable slot. Fires automatically when a c7cf0-bearing
entity reaches lifecycle-state 2 if instantiated through the normal component-create path. sub_0e70
verified to iterate townMgr+0x20 (array) / +0x28 (count). So brick 4-3 ADD = call 9de0 on the roster
sub-entity's c7cf0 (do NOT call 0x14012a0e0 — that REMOVEs).

**⚠️ Even a cheap +0x68 Work-candidate unlock is NOT sufficient for VISIBLE work:** State_Work reads
`+0x228` (active work-task, set by the ported FindWorkplace/sub_1930 scanning **townMgr+0x60**, empty)
and Gather reads `+0x238` (needs c7cf0). Both visible-work paths need a registry populated with a
component the start-town lodge LACKS (c7cf0 for +0x20/Gather, or AttendanceComponent for +0x60/Work-
task). ⇒ the visible organic-employment outcome requires the **MultiObject job-host sub-entity
materialization** (a distinct substantial subsystem) regardless of +0x68. That is the Phase-4 keystone
for a dedicated next session.

**✅ +0x68 RESOLVED = strictly the DISEASE field (recon, exhaustive writer analysis).** Single
writer `CitizenComponent_impl_6b90` (0x140116b90) `*(cz+0x68)=diseaseDescriptor` (+ duration
+0x70/+0x78, town notification gw+0x138, contagion). Callers = random outbreak (helper_2620
0x140112620), contagion spread (helper_2470 0x140112470), debug (0x14005ff30). Type c5948 =
disaster/affliction descriptor. Update (0x140112e70) only READS +0x68. The cc:568 "has profession"
read is a MISREAD — it's the SICK status icon. **NO cheap unlock: setting +0x68 marks the citizen
diseased (death timer + contagion) and still yields no Work candidate.** CORRECTION to my earlier
read: a HEALTHY adult (+0x68==0) IS the normal path and DOES reach Work/Gather (states 6/7) — gated
purely on **+0x238 (workplace) != 0** (+ age>=adult + no active-task +0x208). State 5 (the +0x68!=0
branch) is the sick-handling state, not general Work.

**⇒ THE SINGLE PHASE-4 BLOCKER = +0x238 (empty workplace).** How it's reached is now settled by the
GAME TEMPLATES (ToolKit/resource/Template, the ground truth):

**🔑🔑 TEMPLATE-VERIFIED CORRECTION (2026-07-01) — the "c7cf0 job-host sub-entity" theory is REFUTED;
producers use the WorkPlaceComponent path, and it is PLAYER-DRIVEN.**
- `residence` (⇒ ResidenceComponent c7cf0) is declared ONLY by housing: WoodHouse/StoneHouse/Hostel.
  `multiobject` (⇒ MultiObjectComponent sub-entities) ONLY by Cemetery/Pasture/Tunnel/WoodBridge.
  **The ForesterLodge (and every gather/hunt/fish producer) declares NEITHER** — it has `work`
  [retail data elided]
  profession=forester) + `storage`/`interact`/`radius`/`forester`. So there is NO c7cf0 and NO
  MultiObject sub-entity for producers — the roster lives on **WorkComponent(c8070, capacity) +
  WorkPlaceComponent(c80e0, +0x58/+0x60 worker roster)**.
- ⇒ `sub_0e70`/townMgr+0x20/c7cf0 is the **HOUSING/occupant finder** (houses only), NOT the producer
  path. `Entity_FindComponent_Storage`(c7cf0) returns null on a producer → AssignWorkplace's a710
  roster-add is (correctly) the guarded no-op path for producers; the producer's roster is managed by
  the WorkPlaceComponent path instead.
- ⇒ **Forester employment = WorkPlaceComponent activation (FUN_14012f520: seat WPC+0x48 profession +
  register into townMgr+0x100 profession BST + gw+0x118) → `FindAvailableBuilding` (0x14008d100, reads
  townMgr+0x100, returns the lodge when its c80e0 roster < c8070 capacity) → the ProfessionUI
  worker-count assign cascade (`CitizenComponent_impl_dd10` 0x14007dd10 → 0x14008cbf0/ca20/c5f0/c4b0)
  → AssignWorkplace (brick 4-2) sets +0x238.** This is **PLAYER-DRIVEN** (the player sets the worker
  count via ProfessionUI); there is NO organic auto-assigner for producers (the optimizer FUN_1400d62a0
  only rebalances existing workers). This re-confirms the ORIGINAL [[reference_citizen_employment_path]]
  framing; this session's earlier c7cf0-sub-entity detour was wrong (game data settles it).

**REMAINING FAITHFUL PHASE-4 WORK (next session, well-scoped, brick 4-2 already in place):**
1. Bring WorkPlaceComponent live enough to run FUN_14012f520 on the spawned lodge (seat WPC+0x48 from
   `*(gw+0xb0)+0x38` = the forester Profession + register into townMgr+0x100). Precondition: the
   forester Profession must be seated in the ProfessionList (gw+0xb0) — verify it loads from
   `Game/Profession/Profession.rsc:forester`.
2. Port `FindAvailableBuilding` (0x14008d100) + the assign cascade (impl_dd10 0x14007dd10 / 0x14008cbf0
   / ca20 / c5f0 / c4b0).
3. Drive it programmatically (mirror the ProfessionUI worker-count increment) to seat one forester →
   AssignWorkplace sets +0x238 → the healthy adult's Gather(6/7)/Work candidates fire → citizen paths
   to the lodge and works. (Or, for the HOUSING/occupant sub_0e70 path: populate townMgr+0x20 with the
   founding houses' c7cf0 via 9de0 — but that is the residence finder, a separate concern.)

### Phase 4 — original spec (superseded by the PROGRESS block above)

**Objective:** `FindWorkplace` (called from CitizenComponent::Update when housed,
citizencomponent.cpp:457) assigns citizens to a workplace, or a laborer default when
none. Sets `+0x238` → Gather/Work states unlock → citizens haul/gather/work purposefully.

**Blocking dependency:** Phase 1 (WorkPlaceComponent live + registry) + Phase 2 (housed
is a precondition for FindWorkplace).

**Entry points:** `CitizenComponent_sub_1930` (0x140111930, scans buildings for c80e0
matching the citizen's target desc-id), `sub_40f0` (0x1400440f0, gates on
`citizen+0x30 < desc+0x20` capacity + reachability), `FindAvailableBuilding`
(0x14008d100, reads the profession BST → returns building = `workplace+0x10` with roster
< capacity). CitizenAIComponent heavy Update AssignWorkplace = 0x1163d0 (inert). The
laborer/hauling default is the retail "go to the barn at start" behavior — verify how an
unemployed adult becomes a laborer that hauls loose starting resources to storage.

**Verify:** `[CandList]` shows `wp=1`; candidate set gains Gather (id6/7) / Work; citizens
path to workplaces or haul to the barn.

**Agent spin-out:** one agent (workplace-find + laborer default) after Phases 1+2.

---

## Phase 5 — ✅ COMPLETE (2026-07-01) — production executes end-to-end; town stock increments

**✅✅ PHASE 5 DONE — a harvested tree produces raw material into the town stock, verified live.**
The full produce chain is ported, registered, wired, and **crash-free**:

- **The real produce chain (Ghidra-verified this session):** the forester's cut-sequence builder
  `PerformWork_Action_Ctor3` (0x1400c1eb0, ported `WorkAct_PerformWorkCtor_c1eb0`, registered) appends
  a **RemoveNaturalResource** action to the citizen's action stack (NOT a separate CreateRawMaterial —
  the plan's earlier phrasing was loose). When RemoveNaturalResource executes, its Main (0x1400cab10,
  ported) calls **FUN_1401260f0** (the harvest-produce helper, already ported as
  `ActCNR_RemoveNatRes_1260f0` in `action_base_cnr.cpp` + registered at 0x1401260f0), which spawns the
  raw-material item + calls **RegisterRawMaterial** (0x140088ef0 = `CreateRawMaterial_helper_8ef0`,
  Phase-B-verified accounting) → `townMgr+0x170[cat].+0x10 += amount`.
- **Bugs fixed to make the produce actually run + increment stock in standalone (all in this session):**
  1. `ActRm_FindComponent_223c0` read the raw `.rdata` type-id global `DAT_1401c7c80` (RawMaterialComponent),
     which is unfilled in standalone → the find returned null → downstream `PositionEntity` crashed on
     `null+0x10`. **Fixed:** self-resolve via `GetTypeIdByName("RawMaterialComponent")` (proven pattern).
  2. `ActRm_PositionEntity_9760` wrote the visual transform to the spawned item's inert MapComponent
     (`objA`) / Geometry (`objB`) components (null in standalone) → `null+0x148` fault. **Fixed:** guard
     both null (inert visual layer).
  3. `DAT_140172204` (90.0f rotation) fell back to `0.0f` in standalone (rdata-null). **Fixed:** 90.0f.
  4. Feeding the removed tree entity + the spawned item entity to the **live world entity list**
     (entityList+0xd0 / gameWorld+0xe8) makes the next frame's `EntityList_PreUpdate → Serialize` fault
     in Runtime-x64.dll on the partially-materialized standalone entities. **Fixed:** restructured
     `ActCNR_RemoveNatRes_1260f0` so **RegisterRawMaterial (the economy accounting) always runs**, and the
     entity-lifecycle side (tree-removal-queue append + item clone/position/register) is gated behind
     `BANISHED_PRODUCE_SPAWN_ENTITY` (default OFF) until entity materialization is live. The stock count
     is what the economy loop reads; the physical ground-pile is the inert visual layer.
- **VERIFIED LIVE (full economy combo + `BANISHED_FORCE_HARVEST`):** the real forester scan finds a real
  mature tree and invokes the produce → `[ORGANIC-PRODUCE] RemoveNatRes harvest: amount=3 ... stock 0->3`,
  process stable full duration, **0-byte crash log**. A/B default build (economy envs off) byte-clean +
  crash-free.
- **`BANISHED_FORCE_HARVEST` (default OFF)** is a gated verification harness in `foresterwork.cpp`: when
  the real forester scan finds+validates a tree, it invokes the produce directly (mirroring
  RemoveNaturalResource::Main's flag setup) to exercise the production chain **independent of the citizen
  physically arriving** — because organic arrival is gated on the MoveTo/pathfinder keystone (see Phase 6).

**⚠️ Why forcing is still needed for organic delivery (the Phase-6 handoff):** citizens are DRIVEN
(ForesterWork pushes the cut sequence: MoveTo→chop→RemoveNaturalResource) and they physically MOVE, but
they **drift in a straight line via walk-anim root-motion** (constant facing/delta) instead of steering
along an A* path — so MoveTo never detects arrival, the cut sequence never drains, and
RemoveNaturalResource never executes organically. This affects EVERY citizen (not forester-specific) and
is the `project_citizen_pathfinding_zero_dest` keystone — a SEPARATE subsystem from the economy produce
chain. Fixing it (Phase 6 below) removes the need for `BANISHED_FORCE_*` harnesses.

**Remaining faithfulness polish (low priority, non-blocking):** two produce sub-helpers are still
null-guarded rather than ported — `FUN_1400f8d50` (tree work-node BST search, reads the empty gw+0x118
tree; cosmetic surface-material routing) and `FUN_140022480` (place-near WorkPlaceDescription find). Both
only matter inside the gated entity-spawn block. See [[project_economy_loop]].

---

## Phase 6 — ✅ COMPLETE (2026-07-01) — citizen MoveTo/arrival steering FIXED; citizens navigate + arrive

**✅✅ PHASE 6 DONE — the MoveTo/arrival keystone is fixed and verified; citizens now steer along the
A* path, turn at waypoints, and STOP at their target.** Root cause + fix + organic verification:

- **ROOT CAUSE (Ghidra-verified vs the real `MoveTo::Execute` 0x1400c8570, `MoveTo_Slot5`):** the reimpl's
  CASE-2 traversal compared the wrong cells. The real code advances the path index when the citizen's
  **current cell** (`iVar11 = curCell`) equals `pathNodes[pathIndex]`, and reads terrain speed at
  `curCell`. The port used **`destCell`** (the final destination cell) in BOTH places. `destCell` only
  equals `pathNodes[pathIndex]` when the index already points at the last node → the index never advanced
  for intermediate waypoints → `UpdateFacing` (0x1400c8d20) was never re-called → the citizen bee-lined in
  a FIXED heading past its target forever and only "arrived" if the straight line happened to pass through
  the destination cell. `MoveTo::Execute` was a DISABLED HOOK (bug class 3), so this was never A/B-caught.
- **FIX (`code/Game/Action/moveto.cpp`, 2 lines):** the terrain-speed lookup and the waypoint-reached
  comparison now use **`curCell`**, matching the real function exactly.
- **VERIFIED LIVE (default sa-run combo, `BANISHED_MOVETICK_TRACE`):** citizen `want` heading now changes
  continuously (e.g. cit#0: 28 distinct headings) as it curves toward successive waypoints, `applied`
  yaw tracks it (visible turning), and the citizen **STOPS at its target** (`delta=0 speed=0 stackTop`
  pops) then picks a new leisure target — retail-faithful navigation. 17+ arrivals per run, crash-free.
  The old "straight line off into the distance forever" runaway is GONE.
- **ORGANIC FORESTER DELIVERY PROVEN (no `BANISHED_FORCE_HARVEST`):** with `BANISHED_FORCE_FORESTERWORK`,
  the citizen now **organically walks to the tree and the cut sequence DRAINS** all the way to the produce
  trigger: `[ORGANIC-PRODUCE] RemoveNaturalResource::Main REACHED`, crash-free. My arrival fix newly
  exposed 4 downstream dominoes in the forester chain, all fixed clean:
  1. `MoveTo::Execute` curCell (THE keystone).
  2. `PerformWork::AbstractImpl` (chop) — `s_fYawMin/s_fYawMax` (0x1401722a0/0x140172220) are rdata-null
     in standalone → real-value fallbacks (`performwork.cpp`).
  3. `Math::Vector3::FromYawPitch` dispatch slot 0x1401512a0 unregistered → registered to
     `?FromYawPitch@Vector3@Math@@SA?AV12@AEBVScalar@2@0@Z` (`address_registry.cpp`).
  4. `AnimateNaturalResource` + the shared `WorkBase_FindComponent_52c0` returning null (inert
     resource-entity animation layer) → standalone-inert null-guards (`action_base_movedeps.cpp`,
     `animatenaturalresource.cpp`, `removenaturalresource.cpp`).
- **REMAINING ECONOMY GAP (NOT movement — handed to Phase 7 / economy cut-construction):** the organic
  `RemoveNaturalResource` action's **target `+0x28` is wired to the CITIZEN, not the tree** (diagnostic:
  `target +0x28 == citizen address`), so the NR-component find resolves null and the produce is skipped
  crash-safely (no organic stock increment YET). The fix is in the cut-sequence builder
  (`PerformWork_Action_Ctor3`/ForesterWork), a construction detail separate from MoveTo/arrival. The
  produce ACCOUNTING itself (`FUN_1401260f0` → `RegisterRawMaterial` → stock++) is already Phase-5-verified.
- **A/B GREEN:** default sa-run (no force envs) navigates + arrives crash-free; the always-on change
  (FromYawPitch slot registration) is a pure additive real-runtime-fn registration.

### Phase 6 — original spec (superseded by the COMPLETE block above)

**Objective:** make citizens physically NAVIGATE to a target cell (steer along an A* path and
DETECT ARRIVAL), instead of drifting in a straight line via walk-anim root-motion. This is the single
gate that lets the whole economy loop run ORGANICALLY (forester walks to a tree → chops → produces;
hungry citizen walks to a stocked barn → eats) with **no `BANISHED_FORCE_*` harnesses**. Re-oriented to
here (2026-07-01) after Phase 5 verified the produce chain works but is only reachable via forcing.

**The observed bug (measured this session, full economy combo + `BANISHED_MOVETICK_TRACE`):** every
citizen (forester AND regular) moves with a **constant facing + constant per-tick delta** (e.g. forester
`want=-7.864°` fixed, delta=(0.00205,0.01486) every tick) — a straight-line bee-line that overshoots the
target and never stops. The move-tick TAIL runs (chIdx>=0, position updates, terrain z follows), so the
citizen physically moves; what's missing is the **MoveTo action steering the heading toward the next
A* waypoint each frame + signalling arrival**. So MoveTo either computes an empty/trivial path or the
move-tick applies a fixed root-motion velocity that MoveTo never course-corrects.

**Blocking dependency:** none (independent of the economy work; it's the pathfinder/movement subsystem).

**Entry points / where to look (see [[project_citizen_pathfinding_zero_dest]] + [[project_phase3_keystone_d]]
task #34):** `MoveTo::AbstractImpl`/`Execute` (moveto.cpp) — does it compute a real path and write the
per-frame heading/velocity the move-tick reads? The A* path setup (`PathSetup_c8c10` →
`SetTerrainCosts_e550` → `Pf_EnqueueCmd1_a6a30`, which task #34 flagged deref'ing a null worker/queue at
+0x170) — is the pathfinder queue object actually seeded so A* returns a real waypoint list, or does it
bail to an empty path (→ root-motion drift)? The move-tick (`CharacterComponent_MoveTick_1094c0`,
citizenai_executor.cpp) — is it reading a MoveTo-updated heading, or a stale one set once? Note the
81fcfda "A* correct + grid-faithful" claim in this file's header refers to the A* GRID search returning
valid cell paths; the DELIVERY of the citizen along that path (steering + arrival) is what's broken here.

**Verify:** with the economy combo (no force harnesses), a citizen's `[MoveTick]` facing CHANGES as it
navigates (turns at waypoints) and it STOPS at the target; the forester's pushed cut sequence DRAINS
(`newTop+0x60` returns to <0) and `RemoveNaturalResource::Main` fires organically → `[ORGANIC-PRODUCE]`
without `BANISHED_FORCE_HARVEST`.

**Agent spin-out:** pathfinder/movement specialist; opus + lead Ghidra-verify; this is a keystone, likely
multi-brick (path-compute correctness → per-frame steering → arrival detection).

---

## Phase 7 — ✅ ORGANIC PRODUCE LOOP CLOSED (2026-07-01, committed bc755c5); population response scoped

**✅✅ ORGANIC PRODUCE LOOP CLOSED + VERIFIED END-TO-END (no BANISHED_FORCE_HARVEST).** The
Phase-6 handoff item — "the organic RemoveNaturalResource action's target +0x28 is wired to the
CITIZEN, not the tree, so NR-find resolves null and produce is skipped" — is fixed.
- **Root cause (Ghidra-verified):** the forester cut-sequence builder
  `WorkAct_PerformWorkCtor_c1eb0` (0x1400c1eb0) passed the RemoveNaturalResource copy-ctor
  (`RemoveNaturalResource_Action_CopyCtor` 0x1400cadf0) its two IBP args SWAPPED. The ctor stores
  `*param_2 → self+0x28` (resourceEntity/TREE) and `*param_3 → self+0x20` (citizenEntity); the real
  c1eb0 calls `(self, &tree, &citizen, flag)` but the port called `(self, &citizen, &tree, flag)`.
  ⇒ +0x28 held the citizen → `RemoveNaturalResource::Main`'s `FindNRComp(citizen)` returned null →
  produce skipped (crash-safe). Fix = swap the two args back (1 line, `work_base_actionctor.cpp`).
- **VERIFIED LIVE** (`BANISHED_LIVE_WORKPLACE + MATURE_TREES + FORCE_FORESTERWORK`, **NO
  FORCE_HARVEST**): forester organically walks to a mature tree → cuts → the cut sequence now carries
  the correct TREE target → `[ORGANIC-PRODUCE] RemoveNaturalResource::Main REACHED` → `RemoveNatRes
  harvest: amount=1` → `RegisterRawMaterial cat=2 qty=1 stock 0->1`, crash-free. The old "target
  +0x28=citizen — skipping produce" message is GONE. A/B default (envs off) byte-clean + crash-free.
- **Verification-harness note:** organic verification also needed mature trees. Standalone trees carry
  no live GrowthComponent (growth stays 0.0, failing the forester's maturity gate). `BANISHED_MATURE_TREES`
  (default OFF, `starttown.cpp`) now sets the tree NR component's growth (+0x28=1.0), mature (+0x2c=1)
  and harvestable (+0x2d=1) — the old-growth end-state a live GrowthComponent + harvest-marking layer
  would produce — so the cut sequence reaches the produce accounting.

**Population response (the broader Phase-7 objective):** consumed food relieves hunger; starvation →
death; food/housing → births; resources deplete/replenish. STATUS:
- The mortality/aging/needs/food-consume/partner-birth logic is **already ported in
  `CitizenComponent::Update`** and runs crash-free by default (`BANISHED_LIVE_CITIZEN_UPDATE` in
  SA_LIVE_ENV). Needs accumulate (Phase 3 verified). Aging, hunger/thirst death caps
  (`HandleLifeEvent`), old-age/health death, `TryFindPartner`, `ConsumeFood`/`ConsumeFuel`,
  workplace/housing (re)assignment are all present + guarded.
- **What remains gated (NOT this slice):** (1) "consumed food relieves hunger" needs a live stocked
  **FOOD** StorageComponent + a food producer (gatherer/farmer/hunter/fisher) — the forester produces
  LOG, not food; `ConsumeFood` no-ops until storage is live (Phase-1 storage remainder, as flagged in
  Phase 3). (2) resource **depletion** (tree removal on harvest) is gated behind
  `BANISHED_PRODUCE_SPAWN_ENTITY` (entity-lifecycle materialization). (3) births/deaths need long/
  accelerated time horizons to observe. These are Phase-1-storage / entity-materialization dependencies,
  not produce-loop work.

### Phase 7 — original spec (superseded by the block above)

**Objective:** consumed food relieves hunger; sustained starvation → death; adequate
food/housing → births; resources deplete/replenish. Closes the loop so the town is a
living system.

**Blocking dependency:** Phases 3 + 5 + 6 (organic delivery, so consumption/hauling actually runs).

**Entry points:** CitizenComponent::Update mortality/aging (already ported, guarded),
food-stock decrement, birth logic (CitizenAI LongTermPlan / partner). Mostly bring-up +
verification of already-ported code.

---

## Recommended agent sequence (the "do 1 then 2 then 3")

1. **Phase 3 quick-win** (hunger accumulation) — isolated, fast, directly delivers the
   food-seeking behavior the user described. Good warm-up + morale.
2. **Phase 1** (building components live + town registry) — the foundation; 2-3 agents.
3. **Phase 2** (housing assignment) — 1 agent.
4. **Phase 4** (workplace/laborer assignment) — 1 agent.
5. **Phase 3 full** (food consumption from stocked barn) — folded with Phase 1 storage.
6. **Phase 5** (production) — ✅ DONE (stock increments; verified via BANISHED_FORCE_HARVEST).
7. **Phase 6** (fix MoveTo/arrival) — the pathfinder-steering keystone; unblocks organic testing.
8. **Phase 7** (population) — resume from checkpoints, once organic delivery runs.

Each phase lands crash-safe and env-gated independently; default build stays
byte-identical. Verify with the test combo + A/B after every merge.
