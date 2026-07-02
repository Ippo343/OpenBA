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

**Remaining faithfulness polish (CORRECTED 2026-07-01 — the prior "null-guarded rather than ported"
claim was STALE):** the two produce sub-helpers `FUN_1400f8d50` (tree work-node BST search @ gw+0x118)
and `FUN_140022480` (place-near WorkPlaceDescription find) ARE ported/resolved (`s_fn_8d50 =
R(0x1400f8d50)`, `s_fn_022480 = R(0x140022480)` in `action_base_cnr.cpp`) and are CALLED inside
`ActCNR_RemoveNatRes_1260f0` — they are simply gated behind `BANISHED_PRODUCE_SPAWN_ENTITY` (the
entity-lifecycle block), not stubbed. So there is no per-helper porting work left here; the only gate is
the shared entity-materialization one (Defer 1 below). See [[project_economy_loop]].

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

## Remaining defers — AUDIT (2026-07-01) — all share ONE root cause

A pass over the 7 completed phases to close open defers / unfaithful quick wins found that
the four remaining items are **not independent quick wins** — they all bottleneck on the same
keystone: **standalone entity/component/manager materialization** (inert component factories +
manager-side grids/registries that the Runtime builds but the standalone world-build does not).
Each remaining bypass exists *because* of that gap and cannot be removed without it. Findings:

1. **✅ Audit correction — Phase-5 sub-helpers were NOT stubbed.** `FUN_1400f8d50` /
   `FUN_140022480` are ported + called in `ActCNR_RemoveNatRes_1260f0`, gated behind
   `BANISHED_PRODUCE_SPAWN_ENTITY`. No porting work remained; the plan text was stale (fixed).

2. **✅ `BANISHED_PRODUCE_SPAWN_ENTITY` (Defer 1, ROOT) — CLOSED by Brick M-B (2026-07-01).**
   The "partially-materialized entity" was specifically the NULL ComponentEntity vtable
   (0x14015fa78 unregistered) + the missing removal-side grid occupancy un-mark. Both fixed
   (see Brick M-B below); the gate is retired (default ON, `=0` opt-out). Tree removal +
   ground-pile item spawn run crash-free organically.

3. **Phase-1 storage live + stocked food (Defer 2).** StorageComponent inert → no stocked FOOD
   → `ConsumeFood` no-ops (hunger relief never happens). Same materialization class.

4. **Phase-4 assign cascade (Defer 3).** `StartTown_EmployForesters` pokes citizen+0x1f0/+0x238
   directly instead of the faithful ProfessionUI seat cascade `impl_dd10 →
   cbf0/ca20/c5f0/c4b0 → AssignWorkplace`. Ghidra scope (this pass): `FUN_14008cbf0` is a
   ~200-line distance-optimizer that iterates **townMgr+0xa0** (the town citizen list) — which
   is *unpopulated* in standalone (the reason EmployForesters uses the captured founding list).
   So even a faithful cbf0 port needs townMgr+0xa0 populated = registry materialization. Also
   note: producer employment is **player-driven** in retail (0 default workers), so the
   auto-seat is itself a demo harness, not a retail behavior.

5. **`BANISHED_MATURE_TREES` (Defer 4) — PARTIALLY CLOSED THIS PASS.** The prior root-cause
   claim ("trees carry NO GrowthComponent; factory doesn't materialise it") was **DISPROVEN
   live**: the tree template declares `growth`, the factory DOES create a GrowthComponent, and
   `FindComponent(GrowthComponent)` returns non-null. The real bug: `GrowthComponent::Startup`
   linked +0x28/+0x38 but **+0x30 (the ModelComponent sibling) was NULL**, because the shared
   finder `FUN_140003f60` looks up the ModelComponent by the raw .rdata type-id `0x1401c7a50`,
   which **resolves NULL in standalone**. **FIXED (verified):** a self-contained
   `GetTypeIdByName("ModelComponent")` fallback in `GrowthComponent_Startup`
   (`growthcomponent.cpp`) — scoped there so the verified shared produce/animation callers of
   3f60 stay byte-identical. With the fix + `BANISHED_LIVE_GROWTH`, all three siblings link and
   `StartTown_GrowthInitResource` stages trees at random growth (retail-faithful). **✅ FULLY CLOSED
   by Brick M-A (2026-07-01):** the follow-on "DoHarvestReset faults on the unbuilt `resourceMgr+0x80`
   grid" claim was itself a MISDIAGNOSIS. `*(gameWorld+0xF8)` is the `Game` object and its `+0x80`
   grid IS built in standalone (`MapTileGrid_Build`, the front-half of `CreateNaturalResource_impl_ef20`).
   The fault was a stray extra deref in the `DoHarvestReset` reimpl (wrote near-null); removing it makes
   live growth reach harvest-reset crash-free (verified: 8 harvest-resets, 0-byte crash log). So this is
   NOT the Defer-1/materialization keystone. `MATURE_TREES` is no longer needed to avoid a crash —
   it's now only a default-OFF convenience (instant forester-harvestable trees). See Brick M-A above.

**Net:** the genuinely-closeable items were closed (stale-claim correction; the c7a50/+0x30
sibling-link bug). The rest are one keystone — **standalone entity/component/manager
materialization** (Defer 1) — which is a dedicated multi-brick effort, not a quick win. The
`BANISHED_FORCE_*` / `MATURE_TREES` harnesses cannot be retired until it lands; they remain
default-OFF so the default build stays faithful + byte-clean.

### NEXT-SESSION PLAN — the materialization keystone (4 bricks, do in this order)

Prefer opus subagents + lead Ghidra-verify; A/B the default build after every brick; do NOT
close any of these by adding null-guards (that re-introduces the very bypasses being removed).

- **Brick M-A — ✅ DONE (2026-07-01). Natural-resource manager tile grid (`resourceMgr+0x80`).**
  Unblocks **Defer 4** (`MATURE_TREES` no longer needed to avoid a crash). **The premise was a
  MISDIAGNOSIS — the grid is NOT unbuilt.** RE (Ghidra-verified): `*(gameWorld+0xF8)` is the `Game`
  object, and its `+0x80` per-tile grid (`{data, size@+0x88, cap@+0x8c}`, stride 0x30, `dim*dim`
  records, per-tile `+0x10`=8-byte ownerPos / `+0x2c`=growth byte / `+0x29`=type) IS built in
  standalone by `MapTileGrid_Build` — the faithful front-half of `CreateNaturalResource_impl_ef20`
  (0x14009ef20 → grid alloc `MemAlloc(dim*dim*0x30,8)`), run unconditionally at world-build
  (gameupdateloop_load.cpp:1053). The real fault: the `DoHarvestReset` reimpl had a **stray extra
  deref** — it computed `*( *(uintptr_t*)gridBase ) + 0x10 + resIdx*0x30` (reading the zeroed first
  qword of tile-record 0 → wrote near-null). The original writes `*( gridBase + 0x10 + resIdx*0x30 )`
  directly. **FIX = remove the extra deref** (`growthcomponent.cpp` DoHarvestReset, 1 line).
  **VERIFIED** (`BANISHED_LIVE_GROWTH=1 GROWTH_TRACE=1 GROWTH_FORCE_HARVEST=1`): 8 `[GrowthHarvest]`
  harvest-resets ran with valid `gridBase` + in-bounds `resIdx` (105…1127), **0-byte crash log**;
  default combo (no LIVE_GROWTH) crash-free + byte-clean. Harnesses (default OFF): `BANISHED_GROWTH_TRACE`
  (adds a per-harvest trace), `BANISHED_GROWTH_FORCE_HARVEST` (forces a tiny growthScale so the next
  live tick exercises DoHarvestReset deterministically). No new grid-build code was needed.

- **Brick M-B — ✅ DONE (2026-07-01). Dirty-list lifecycle fault — Defer 1 CLOSED;
  `BANISHED_PRODUCE_SPAWN_ENTITY` retired (now default ON, `=0` opt-out).** Three root causes, all
  materialization (no null-guards):
  1. **NULL ComponentEntity vtable (THE keystone).** `0x14015fa78` was never registered, so EVERY
     standalone entity was constructed with `vtable == NULL` (`ResolveAddr` → NULL in
     `componententity.cpp EnsureResolved`). Invisible in the spawn/tick flow (nothing virtual-calls
     an entity) — but the removal path's `InternalBasePtr::DeletePtr` (Runtime 0x1800c0150) does the
     first-ever virtual dispatch (`CALL [vtable[0]]` = scalar dtor, Ghidra-verified at the faulting
     instruction) and crashed. Same root as the historical "PreUpdate → Serialize" fault (slot 3).
     FIX: `g_ComponentEntity_StandaloneVtable[12]` in `componententity.cpp` — flat replica of the
     .rdata block (primary 4 slots + RTTI gap + embedded base vtable at `0x14015faa0` = &[5]),
     wired to the already-ported Dtor/Serialize/Slot5/Slot8 + lazy GetTypeId; registered for both
     addresses in `address_registry.cpp`.
  2. **Dangling tile-grid entity-link after removal.** `Object::InternalBasePtr` is a RAW pointer
     (Ctor stores, `CheckPtr` is a release NO-OP — Ghidra-verified) — retail avoids dangling by
     discipline: entity mark-removal `FUN_1400c2ea0` walks components calling vtable+0x38 →
     MapComponent (0x1401242f0, already ported) → `impl_4750` (0x140124750) → `dee0` un-marks the
     footprint (nulls cell+0x08 for every cell, reclassifies terrain), gated on comp+0x20 bit
     0x100000 ("occupancy written"). All stubs in standalone → the freed tree stayed linked at
     cell+0x08 → CitizenAI leisure box-scan faulted on it. FIX: `StartTown_UnmarkResourceCellGrid`
     (dee0 port, starttown.cpp) + `MapComponent_DetachOccupancy_4750` (mapcomponent.cpp, registered
     at 0x140124750) + MapComponent inert vtable slot 7 → the ported OnCreate handler + the
     spawn-side collapse now sets bit 0x100000 (mirroring `MapComponent_Slot12`).
  3. **FORCE_HARVEST harness ordering (not a product bug).** Retail's cut sequence runs ALL
     tree-touching actions (AnimateNaturalResource ×2, chop PerformWork) BEFORE
     RemoveNaturalResource (stack pops reverse-append; c1eb0-verified) — after removal only
     citizen-targeting actions remain, so retail never touches the freed tree. The harness
     force-produced (removed the tree) and THEN queued the real cut on the same tree → its pending
     AnimateNR ran on freed memory. FIX: harness skips queueing the cut for a force-harvested tree.
  **VERIFIED:** 8.5-min soak, 37 organic produces (stock →48) with full tree-removal + ground-pile
  item spawn, 0 crashes; default-on re-verify (no env) 6 organic produces crash-free; default
  `sa-run` combo A/B crash-free; DLL-mode build clean. Repro env note: the forester scan needs
  `BANISHED_LIVE_REGIONMAP=1` (else `Map_CalcDistance` falls back to euclidean+gridW and the
  radius gate rejects every cell — the scan finds nothing and never reaches produce).

- **Brick M-C — ✅ DONE (2026-07-02). Town citizen list (`townMgr+0xa0`) + real employ cascade —
  Defer 3 CLOSED; the `StartTown_EmployForesters` field-poke is RETIRED.** Design decision: chose
  **(b)** — port the REAL cascade + drive it via the existing env-gated demo (`BANISHED_LIVE_WORKPLACE`),
  consistent with M-A/M-B/M-D. What landed (`code/Game/Component/citizen_employ_cascade.cpp` +
  `standalone/reg_citizen_employ_cascade.cpp`):
  1. **townMgr+0xa0 ADD site FOUND + ported** = `CitizenComponent_OnAdd` (0x140114000) — wraps the
     CitizenComponent in an IBP and appends to `townMgr+0xa0` via the already-ported `FUN_140003110`.
     It's a vtable OnAdded slot (DATA-xref only), inert in standalone → the employ pass calls it per
     founding citizen to populate the real town-citizen list the cascade scans.
  2. **The full seat cascade ported 1:1** — `cbf0` (distance-optimized relocate), `ca20` (nearest
     employed laborer), `c5f0` (most-laborer house, incl. the `FUN_1400896a0` house shuffle), `c4b0`
     (first idle laborer) — plus a headless driver `CitizenEmploy_SeatWorkers(townMgr, profKey, count)`
     mirroring `dd10`'s (0x14007dd10) per-worker `FindAvailableBuilding + cbf0→ca20→c5f0→c4b0` fallback
     (dd10 itself reads a NumberEditElement widget we don't have). All ~7 deps were already ported.
  3. **Two faithful seeding prerequisites** (the inert standalone citizen init `FUN_140117080` is
     guarded off, so `+0x1f0` stays 0): the founding colonists are seeded as LABORERS
     (`+0x1f0 = ProfessionList(gw+0xb0)+0x30`, what retail colonists carry) so the cascade's
     `+0x1f0==laborerKey` filter matches them, and registered into townMgr+0xa0.
  🔑 **CORRECTED FIELD SEMANTICS (Ghidra-verified; the prior "+0x238=workplace, cascade→AssignWorkplace"
  note was WRONG):** `AssignWorkplace` (0x1401163d0) sets **+0x238** = RESIDENCE (rosters into c7cf0
  ResidenceComponent); `AssignHousing` (0x140111ee0) sets **+0x248** = WORKPLACE (rosters into c80e0
  WorkPlaceComponent) — the names are BACKWARDS vs effect. The real cascade seats a worker via
  `+0x1f0=profession` + `AssignHousing`→`+0x248=workplace` (it does NOT touch `+0x238`).
  **VERIFIED LIVE** (`BANISHED_LIVE_WORKPLACE + SPAWN_ADULTS + LIVE_REGIONMAP + MATURE_TREES +
  EMPLOY_TRACE`): `foundingCitizens=20 seeded=20 → seated 4/4 forester(s)`; each seat shows
  `+0x1f0`=forester + `+0x248`=lodge, **0-byte crash log**. A/B default `sa-run` (no economy envs):
  byte-clean, zero cascade output, crash-free, world builds (9038 ents) + housing applies.
  **Note on the visible Gather:** the old field-poke set `+0x238`=lodge, which tripped the adult Gather
  candidate gate (`+0x238!=0` = *housed*) — semantically abusing the residence field. The faithful
  cascade correctly leaves `+0x238`=0 (the founding citizens' residence link was never set — Phase-2
  housing uses `AssignHousing`→`+0x248`), so the seated foresters are *employed* (`+0x248`) but not
  *housed* (`+0x238`) → the organic Gather candidate stays gated on residence materialization (a
  separate concern, and the employ pass is opt-in `BANISHED_LIVE_WORKPLACE`, never in the default demo;
  `BANISHED_FORCE_FORESTERWORK` still drives work directly). See
  [[reference_employ_cascade_field_semantics]].

- **Brick M-D — ✅ DONE (2026-07-02). Live town storage + stocked food + the EAT chain — Defer 2
  CLOSED** (env `BANISHED_LIVE_STORAGE`, default OFF; verified with `BANISHED_NEEDS_ACCEL=100` +
  `BANISHED_LIVE_REGIONMAP=1` + `BANISHED_STORAGE_TRACE=1`). **VERIFIED LIVE: 6 hungry citizens
  organically walk to the StorageBarn and EAT — hunger 23.6→0 at 1.0/unit, barn slot count
  decrements per unit, 0-byte crash log.** The plan's "ConsumeFood relieves hunger" wording was
  LOOSE — ConsumeFood (0x140112990) is only the happiness/variety accumulator; the real relief =
  **EatFromStorage (0x1401152e0, Ghidra-mislabeled `CitizenComponent_ReleaseWorkplace`)**, reached
  via eat state 0/0xe → `e370` food search (scans **townMgr+0x00 storage-location list** + 0x10
  markets) → `PerformWork_Action_Ctor6` pushes MoveTo→PerformWork(8)→SendCompMsg(msg 2 → the
  CitizenComponent vtable+0x78 dispatcher). Full chain map in memory `reference_eat_food_chain`.
  What landed (two agent bricks + lead integration):
  1. **World half** (`town_storage_seed.cpp` + starttown wiring): build-complete storage init
     `BuildDescription_helper_7ff0` port (0x140107ff0 — sets +0x38 = _volumeLimit×area, +0x3c =
     _storageFlags; it IGNORES `_available`, resolving the "+0x21 gates Draw" puzzle — Draw/OnAdded
     only serves _available=true storages like yard/cart/citizen-inventory); townMgr+0x00
     registration via the d230 port for `storagelocation`-bearing buildings ONLY (barn+yard —
     registering houses was tried and reverted as unfaithful); townMgr+0x110 raw-material template
     registry via the already-ported 091e60 dedup-append (mirrors a3300's 0x1400a33b4 loop); the
     seven-mask seeder `Game_impl_1f00`/`Game_SpawnResources` ports (0x1400a1f00/0x140091660,
     counts from startConfig+0xfc..+0x114: Easy = 150 log / 70 stone / 50 iron / 50 tool /
     **2400 FOOD** / 400 firewood / 40 clothing — all placed via cd80 accept + AddItem d000).
  2. **Citizen half** (`citizen_needsupdate.cpp` + wiring): CitizenComponent_NeedsUpdate
     (0x140112260, the 9-case vtable+0x78 message dispatcher) + warm-house thirst helper_21e0
     ports; vtable slot 15 wired in the CitizenComponent live factory (manual macro expansion);
     the SendComponentMessage AbstractImpl/GetTarget were already ported+wired; a scoped msg-2
     typeid heal in the CopyCtor (ctor6's typeid comes from unregistered .rdata c7158 → 0).
     Also fixed a REAL pre-existing bug: EatFromStorage used the ResidenceComponent finder
     (0x140008090) where retail uses the StorageComponent finder (0x140006270, disasm-verified);
     and QueryCount was rewritten volume-weighted (matDesc+0x48 × count) per the real cf10.
  3. **Cross-cutting root causes fixed during verification** (each found via gate traces):
     (a) InteractDescription::Serialize read the load-direction flag from the WRONG level
     (`*(serializer+8)` instead of the stream's `*(stream+8)`) → the use/build/create interact
     point lists NEVER built → ChildSum=0 → d5d0 found no barn access point → every food search
     failed. (b) `Pf_EntityResolvePos_2e20` dropped the Ghidra-hidden R8 point argument in BOTH
     transform calls (rotate + operator*) and ignored p3/p4 — rebuilt faithfully from disasm
     (node = desc pair[p3] array + p4*0x18, {offset@0, dir@0xc}); scoped ModelComponent-finder
     fallback added (3f60's c7a50 typeid stays deliberately unfilled). (c) **StorageComponent
     type-id 0x1401c7e78 added to reg_economy_typeids** (filled from GetTypeIdByName after
     startup, like its 13 peers) — four independent crash/no-op sites all traced to the shared
     6270 finder's dead typeid (EatFromStorage, ef40, e370/dd30 mask fallbacks, ConsumeListAppend
     64b0's reserve bump). (d) `BANISHED_NEEDS_ACCEL=<mult>` harness (default OFF) scales ONLY
     hunger/thirst accumulation for verification.
  **A/B:** default `sa-run` (no economy envs) soak crash-free, no [StorageSeed]/[EatMsg] output
  (gates off). Remaining fold-ins for later bricks: retire the msg-0 warm-house typeid ambiguity,
  port NeedsUpdate sub-helpers fde0/17a0/1540 (msg 1/4/5, currently resolve-or-skip), and a FOOD
  producer (gatherer/farmer) so stock replenishes organically (Defer: seeded stock only).

Suggested order: **M-A ✅ → M-B ✅ → M-D ✅ → M-C ✅** — ALL FOUR MATERIALIZATION BRICKS DONE.
M-A was a 1-line extra-deref fix; M-B closed the real materialization keystone (the NULL
ComponentEntity vtable) and retired `PRODUCE_SPAWN_ENTITY`; M-D landed live town storage + the full
organic EAT chain; M-C populated townMgr+0xa0 (real `CitizenComponent_OnAdd` registrar) + ported the
real ProfessionUI seat cascade (`dd10`→`cbf0/ca20/c5f0/c4b0`) and retired the `EmployForesters`
field-poke. All four Defers (1/2/3/4) are closed. Each brick lands crash-safe + A/B-clean; the
remaining `FORCE_*`/`MATURE_TREES` harnesses are default-OFF conveniences that drive verification
paths directly.

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
