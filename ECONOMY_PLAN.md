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
Phase 5  Production execution end-to-end (work-completion → CreateRawMaterial → RegisterRawMaterial)
             ▼
Phase 6  Population response (food consumed → hunger relief; births/deaths; depletion)
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
- 🔴 **FINAL BLOCKER (runtime-confirmed): the spawned ForesterLodge has NO ResidenceComponent
  (c7cf0 worker-roster) and NO AttendanceComponent** (21 comps: Map/Interact(1333608116)/Work/
  WorkPlace(1731492573)/… but not c7cf0). Both finders (sub_0e70 + FindAvailableBuilding) REQUIRE
  a ResidenceComponent worker-roster on the workplace → sub_0e70 skips the lodge → no citizen
  employed. The roster (c7cf0) + InteractComponent work-positions live on the **MultiObject job-HOST
  sub-entity**, materialized at BUILD-COMPLETION — a path standalone skips ([[project_economy_loop]]
  MultiObject-host detour). **⇒ Remaining Phase-1 work = materialize the workplace's MultiObject
  job-host sub-entity (or hand-add a ResidenceComponent worker-roster + InteractComponent
  work-positions) so a finder can seat a worker.** Distinct substantial subsystem — next session.
- Infrastructure landed this session (all faithful, A/B-safe, build+run-verified, uncommitted):
  town_workplace_registry.cpp (+reg), sub_0e70 port, townMgr+0x20 wiring, address_registry.cpp.

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

## Phase 2 — Housing assignment

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

## Phase 4 — Job / workplace assignment + laborer default

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

## Phase 5 — Production execution end-to-end

**Objective:** a worked job produces a resource into storage. Largely done for the
forester (find→decide→cut→execute crash-free, commit a3c9a4d) — remaining gap is
work-COMPLETION chaining: citizen physically reaches the target, the work-timer
(self+0x30) completes, a harvest-complete handler pushes CreateRawMaterial (0x1400c6b40)
→ RegisterRawMaterial (0x140088ef0, verified). See [[project_economy_loop]].

**Blocking dependency:** Phase 4 (employed citizens driving work).

**Entry points:** ForesterWork (0x1400eaec0), PerformWork::Main, the work-completion
chaining (deep inert subsystem). Trace which pushed action runs vs aborts
(`[ORGANIC-PRODUCE]` traces already in place, gated BANISHED_FORESTER_SCAN_TRACE).

**Agent spin-out:** one agent, resume from the existing economy_loop checkpoint.

---

## Phase 6 — Population response

**Objective:** consumed food relieves hunger; sustained starvation → death; adequate
food/housing → births; resources deplete/replenish. Closes the loop so the town is a
living system.

**Blocking dependency:** Phases 3 + 5.

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
6. **Phase 5** (production) then **Phase 6** (population) — resume from checkpoints.

Each phase lands crash-safe and env-gated independently; default build stays
byte-identical. Verify with the test combo + A/B after every merge.
