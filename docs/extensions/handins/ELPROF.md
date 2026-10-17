# ELPROF -- where EntityList's time goes (heavy town, speed 10), and what threading could buy

Coordinator, 2026-10-12 afternoon. Asked by the user after the E2E split showed EntityList at
~88% of Update and the only limiter of fps at high speed. Two runs on an idle machine (both
`machine load: CLEAN`, RTSS stopped), large map, 1600 citizens, spread layout, speed 10, every
extension off:

| run | what it adds | sim ms/iter | EntityList ms/frame |
|---|---|---|---|
| `20261012_134830_el_prof_tree` | `BANISHED_STRESS_COMPPROF=2`: the slot-8 (per-iteration) ticks attributed per component type (`/tree` rows) | 0.35 (profiling overhead +0.03) | 3.02 |
| `20261012_135123_el_prof_sample` | `BANISHED_SAMPLE=1000`: the sampling profiler, `tools/sample_report.py --exe build/cmake-perf/OpenBA.exe --tag 4` / `--tag 8` | 0.34 | 2.91 |

Unprofiled reference (`20261012_105902_pfh_faithful`): sim 0.32 ms/iter, Update 3.09 ms/frame,
EntityList 2.62 of it (CitizenList 0.21, PostSim 0.15, Animation 0.05, the rest < 0.05).

## 1. The phase split (COMPPROF, steady-state window, per Update frame = 10 iterations)

| part | ms/frame | per iteration | notes |
|---|---|---|---|
| tree walk (slot-8 ticks, every iteration) | 2.05 (2.35 profiled) | 0.205 | 26,300 nodes/frame = 2,630 entities per iteration |
| 5-bucket round-robin (slot-9 Update, throttled) | 0.35 steady; 1.3-1.5 in the first two windows | 0.035 | 300 ticks per frame = 30 per iteration, quota 50/15/8/5/2 |
| drain / preUpdate | 0.02 | | |

The tree walk is 85% of EntityList at steady state. Per entity per iteration it costs 78 ns on
average, with no single hot spot.

## 2. The tree walk by component type (`/tree` rows, window frame=3600)

| component (slot-8 tick) | ms/frame | share of EntityList | calls/frame | ns/call |
|---|---|---|---|---|
| CharacterComponent (move tick + action executor) | 1.144 | 49% | 19,996 | 57 |
| ModelComponent (PerfX batch/cull decision, MeshInstance::Update) | 0.234 | 10% | 20,037 | 12 |
| ParticleComponent | 0.211 | 9% | 7,674 | 27 |
| StateMachineComponent | 0.137 | 6% | 6,180 | 22 |
| StatusIconComponent | 0.011 | 0.5% | 708 | 16 |
| the walk itself (snapshot, flag tests, `InternalPtr::Get`, dispatch) | ~0.6 (incl. ~0.3 of profiling) | ~13% | | |

Per iteration the tree holds ~2,000 characters (each with a model), ~770 particle systems, ~620
building state machines and ~70 status icons.

## 3. Sampler, EntityList tree phase (13,009 samples; self = leaf, incl = on stack)

| function | self | incl | what it is |
|---|---|---|---|
| `ActionStack::Execute` | 11.6% | 23.2% | the three-phase node walk (precheck / main / pop) of the citizen's action stack, once per iteration per character |
| `ComponentEntity::TickComponents` | 9.1% | 95.3% | the per-entity component loop: flag tests, `Get()`, vtable dispatch |
| `CharacterComponent_MoveTick_1094c0` | 9.0% | 48.8% | heading / turn / step integration after the executor |
| `MoveTo::Execute` | 4.8% | 5.7% | path poll + step along the path |
| `CharacterGrid::Place` | 4.6% | 4.6% | the spatial hash relink (already memoised for a standing citizen) |
| `StateMachine::Tick` | 4.6% | 6.1% | building state machines |
| `CachedVisible` + `PlanesVisible` + `BatchEntry` + `NeedsPerTickRootUpdate` + `AnimBatchDecide` | 12.5% | | PerfX's per-model per-iteration bookkeeping (hash-table probes deciding whether to skip the pose update); the work it skips would cost more |
| `WalkUpdateTreeSnapshot` | 3.5% | 3.5% | the flat in-order walk |
| `PerfX::TerrainGet` + `GetTerrainPos` + `MapQuery_SampleHeight` + `WorldToCell` + `ClampCell` | 6.6% | | terrain height under the character, last iteration only (`rootOnly`) |
| `SinPoly` + `CosPoly` + `SinCosD` + `CharacterComponent_SinCosf` | 5.5% | | heading trig (already the polynomial version) |
| `PC_Culled` + `ParticleCullSkip` + `PC_UnstampOwnEmitters` + `ParticleSystemInstance::Update` + `ParticleComponent::Tick` | 6.6% | 7.5% | particles, mostly the cull decision |
| `SendComponentMessage::AbstractImpl` -> `CitizenAIComponent::OnMessage` -> `ValidateAndEnterState` | | 3.9% | state transitions fired from the action stack (contains `Map::PathDistance` 1.9%, `State_Leisure` 1.6%) |
| `CitizenAI_ActionFlush_c4bb0` | | 3.7% | |
| `Graphics::MeshInstance::Update` + `AnimationState::Update` + `RootAnimationOffset` | | ~3.5% | the pose / root motion that PerfX did not skip |

## 4. Sampler, bucket phase (4,932 samples)

`CitizenAIComponent::ValidateAndEnterState` is 60% of it, and inside that `Map::PathDistance` is
46% of the whole phase: `RegionGraph::PathLength` -> `Run` -> `FindPath` on the region graph with
the same `PathOpenSet` AVL open set as the cell search (`AvlRebalance` 10%, `Insert` 6%,
`ChildHeight` 3%, `Relax` 2%, `FindPath` self 14%). `CitizenBehavior::FindBestTarget` is 26%
(`CharacterGrid::QueryNearestCharacter` 11% self). `AmbientEnvironmentComponent::Update` 2.7%.
In the first two windows of every run (citizens taking jobs and homes) the bucket phase is 4x
its steady-state cost and CitizenAIComponent is 80% of it; those windows are also the ones that
matter at speed 50 before the town starves.

## 5. Would threading do anything? The numbers

Amdahl on the measured shares, EntityList only, assuming perfect scaling on 4 threads for the
parallel part and no synchronisation cost (so an upper bound):

| what is made parallel | share of EntityList | EntityList after | Update after (3.09 ms) | fps at speed 50 in the 1,600-citizen windows (today 48-67) |
|---|---|---|---|---|
| the entity-local ticks only: model (10%), particle (9%), state machine (6%), the move tick's integration / trig / terrain (~20%) | 45% | -34% | -29% | ~70-95 |
| every slot-8 tick, with the shared-state writers somehow made safe | 85% | -64% | -54% | ~105-145 |
| the bucket phase too | 100% | -75% | -64% | ~135-190 |

What stands in the way, by weight:

- **The action executor + CitizenAI transitions (~30% of EntityList)** mutate shared game state
  in the middle of the walk: pathfinder enqueue (a lock already), the work scheduler and
  storage reservations through `OnMessage` / `ValidateAndEnterState`, `PathDistance`'s region
  pair cache, entity creation / removal through the staging arrays, `RefreshEntityFlags` on the
  update tree. Entity A's transition is visible to entity B ticked later in the same iteration.
  Running these on N threads either races or defers them to a serial phase, and deferring
  changes the order in which citizens claim work, homes and targets: a different (valid) sim,
  never byte-identical to retail, and different again per thread count. That makes any walk
  threading a **class C** extension by DESIGN.md, with no shadow-mode check possible.
- **`CharacterGrid::Place` (4.6%)** relinks per-cell lists; parallel placement needs per-cell
  locks or a deferred relink (the latter changes `QueryNearestCharacter` results within the
  iteration).
- **The tree walk itself (13%)** and `TickComponents`' dispatch are serial pointer chasing at
  ~25 ns per entity; splitting the snapshot into N ranges is trivial, but a tick that removes
  or creates entities touches the staging arrays (lockable).
- Sync cost: a fork/join per iteration at speed 50 is 50 barriers per frame; at 5-10 us each
  that is 0.25-0.5 ms, i.e. 2-3% of Update, acceptable.

Verdict: threading the entity-local 45% is safe (no shared writes: the model / particle /
state-machine ticks read the frustum cache and write their own component; the move tick's
integration writes only its own transform) and worth about -30% of Update, which at speed 50
is roughly 60 -> 85 fps in the heavy windows. Going further means threading the citizen AI, a
class C change with a non-deterministic sim. The user wanted data before any multi-threaded
work: the 45% figure is that data, and it is an upper bound.

## 6. Serial candidates the profile points at (faithful results, class B at most)

1. **`Map::PathDistance` on a bucket queue** -- the region search uses the same AVL open set the
   cell search did; `ext.fast_paths`' LIFO bucket queue reproduces retail's (f, LIFO) pop order
   exactly, so the same technique gives the identical region path and the identical
   `PathLength` sum. 46% of the bucket phase at steady state and more in the busy windows
   (CitizenAI 1.1-1.5 ms/frame in windows 1-2). Expected: bucket phase -30-40%, the busy
   windows -0.4 ms/frame. Could join `ext.fast_paths` under its existing lock.
2. **PerfX bookkeeping (12.5% of the tree phase)** -- three hash probes per model per iteration
   to decide "skip". A per-Update verdict stored in a dense side array indexed by the scene
   object id (`modelId`), computed on iteration 0, turns that into one load. Expected -8-10% of
   EntityList. Perf build only (PerfX is already non-retail there).
3. **The walk (13%)** -- the snapshot already removes the BST; the remaining cost is per-entity
   `_entityFlags` / component-flag tests and `Get()` on every component of every entity. A
   per-entity cached list of its slot-8 components (rebuilt on flag change, which goes through
   `RefreshEntityFlags`) would skip the non-ticking components. Expected -5% of EntityList.
4. `ActionStack::Execute` self time (11.6%) -- the executor re-walks the node list three times
   per iteration and stores `_status` on every node each time. Faithful restructuring (same
   calls in the same order, fewer stores) is possible but each percent needs a shadow check.

Items 1-3 together are about -20% of EntityList (-17% of Update) with faithful results; the
threading of the entity-local ticks is another -30% on top; those ticks are independent of each
other (each writes only its own component), so the split could be class B with a shadow check
comparing the resulting transforms against the serial walk. Suggested order: 1 (reuses
FASTPATHS, self-checking), then 3 and 2, then the entity-local thread split as a prototype with
the transform shadow.

## 7. Files

- Runs: `perf/results/20261012_134830_el_prof_tree`, `perf/results/20261012_135123_el_prof_sample`
  (the sample file is `C:/GOG Games/Banished/standalone_samples.txt` at the time of writing;
  the results dir carries the copy).
- Logs: `docs/runtime_port/phase_d/runs/stress_el_prof_tree.out`, `stress_el_prof_sample.out`.
