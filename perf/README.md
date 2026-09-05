# Stress test + performance baseline

`BANISHED_STRESS=1` turns a fresh autostart new game into a stress town and measures it.
Two purposes: exercise large-scale interactions (hundreds of citizens, every building
type, every profession, full storage/market/trade loops running at once), and give
optimisation work a number to move.

## Run it

```bash
mingw32-make sa-stress                                   # perf build, 800 citizens @10x, ~2.5 min
mingw32-make sa-stress STRESS_ARGS="--label wip --speed 5 --citizens 600"
mingw32-make sa-stress-debug                             # -O0 debug exe: functional check only
tools/stress_run.sh --help                               # all knobs
```

`sa-stress` builds `standalone/build-perf/reimpl-x64.exe` (`-O2 -g`, logs stripped, its
own object dir so it never mixes with the debug objects), deploys it, runs the town to
completion and leaves everything under `perf/results/<timestamp>_<label>/`:

| file | what |
|---|---|
| `standalone_stress.csv` | one row per 600 frames (see columns below) |
| `standalone_stress_compprof.txt` | per-component-type Update cost, every window |
| `standalone_error.log` | the run's stderr (`[Stress…]`, `[SIMPROF]`, `[Stress::staff]`) |
| `report.txt` | `tools/stress_report.py` output, incl. the diff against the baseline |

The run ends itself: the harness opens the in-game pause menu at `BANISHED_STRESS_FRAMES`
(default 6000 sim frames = 100 s at 60 Hz = 60 000 sim ticks at the default 10x) and clicks Quit Game, so the process exit is
a clean `Engine::Shutdown`. A crash leaves `standalone_crash.log` and the script exits 2.

## What the town is

Built by `StartTown_StressSpawnStructures` (`code/Game/Game/starttown.cpp`) on top of the
normal Easy founding town, on a **medium (384) map**, seed 31337 (12345's town area is a
~11k-cell pocket between river and mountains; 31337 has ~27k connected cells):

- **3 sets** (`BANISHED_STRESS_SETS`) of every ordinary placed building: storage barn +
  yard, market, well, school, hospital, chapel, cemetery, cutter, forester, herbalist,
  toolmaker, tailor, tavern, iron mine, quarry, hunter lodge, gatherer hut, hostel; one
  town hall.
- **One each** of the buildings that need special siting, via their existing demo blocks:
  fishing dock and trading post (river), crop field, orchard, pasture (drag areas), plus a
  second density-sited hunter lodge and gatherer hut.
- **Houses** for the target population (`BANISHED_STRESS_CITIZENS`, default 800):
  alternating wood/stone, one per family, target/3 + 2 of them.
- **Citizens**: one family (2 adults + 0..3 children) per stress house, spawned at the
  house through the retail founding-spawn path, housed by the founding-housing pass.
- **Jobs**: from world frame 600 a staffing pass reads every workplace's authored
  `_defaultWorkers` and seats that many through `CitizenEmploy_SeatWorkers`, the same
  cascade a Town Hall spinner click runs. Retried every 300 frames (workplaces that open
  late get staffed late), then every 1800.
- **Food**: the seven start-stock counts are multiplied by citizens/10 for the seeding
  call, so the town does not starve inside the measurement window.

Every building is COMPLETED (instant-build bracket), sited 1 tile apart on workable
ground around the founding cluster. Placement is deterministic per seed. Siting rules
(2026-09-04, after the 10x population collapse was traced to walled-in house doors):

- **Door (access) cell**: learned per template from the first placed instance via retail's
  `Entity_impl_7fb0` (0x140087fb0). It is a walkable notch INSIDE the footprint rectangle,
  and `CitizenComponent::Update` re-validates every idle citizen's home by comparing the
  citizen's tile region against the door's region — a door facing water or another
  footprint evicts every resident, and a homeless citizen never eats.
- **Pocket test**: a whole-map flood from the town cell over free cells is kept up to date;
  a candidate is rejected if its footprint (door excepted) would cut any reachable cell
  off, or leave its own door unreachable (interior doors — yards, markets, fields — skip
  the door test). `BANISHED_STRESS_ALLOW_POCKETS=1` restores the old layout.
- The `[Stress::place] DONE` line reports `pocketRejects`, `doorWalled` and per-reason
  reject counts; `BANISHED_STRESS_KINDMAP=1` dumps the terrain kind map.

Env bundle applied by `StressHarness_Init` (only for variables you did NOT set):
`BANISHED_AUTOSTART=1 BANISHED_MAP_SIZE=medium BANISHED_SEED=12345 BANISHED_DIFFICULTY=easy BANISHED_CLIMATE=fair
BANISHED_FPS_CAP=off BANISHED_FORCE_SPEED=<speed> BANISHED_SIMPROF=<interval>
BANISHED_PAUSEMENU=<frames> BANISHED_PAUSEMENU_BUTTON=6 BANISHED_CAMERA_PIN=town,90` plus
the `BANISHED_BUILD_DEMO_*` flags above plus `BANISHED_CROPFIELD_FORCE_START=2
BANISHED_ORCHARD_FORCE_START=1`. Weather is a function of game time and the seeded RNG, so
the starting season repeats exactly for a given seed; if a seed starts in snow, pick another
seed rather than expecting variation.

## Why 800 citizens at 10x is the default

Measured 2026-09-03 (perf build, 6000-frame runs, steady state):

| town | update ms / frame | ms / sim iteration | notes |
|---|---|---|---|
| 400 citizens, 1x | 0.45 | 0.45 | first baseline |
| 400 citizens, 5x | 2.54 | 0.51 | |
| 400 citizens, 10x | 3.18 | 0.32 | per-iteration cost FALLS: animation and other per-frame work amortise over 10 ticks |
| 800 citizens, 1x | 0.66 | 0.66 | |
| 1200 citizens, 1x | 0.82 | 0.82 | ~+0.18 ms per extra 400 citizens |
| **800 citizens, 10x** (2026-09-03) | **5.10** | 0.51 | seed 12345, pre-fix town; population 802 -> 702 over 6000 frames |
| 800 citizens, 10x (2026-09-04 baseline) | 3.94 | 0.39 | seed 31337, door/pocket siting: population 802 -> 794 |
| 800 citizens, 10x (after the perf wave, 20 iterations) | 2.42 | 0.24 | see "Optimisation log" below; `perf/baselines/2026-09-04_post-perf-wave.csv` (the 0.39 run is `2026-09-04_pre-perf-wave.csv`); icons OFF |
| 800 citizens, 10x (PerfX knobs, 2026-09-05) | 1.92 | 0.19 | see "Non-retail experimental knobs" below; now `perf/baselines/current.csv` (= `2026-09-05_perfx.csv`); icons ON |

Speed multiplies the same work; citizens change its shape (AI, pathing, hauling). The
default takes both. `BANISHED_STRESS_SPEED=1 BANISHED_STRESS_CITIZENS=400` reproduces the
gentler town when a 1x number is wanted. Two things the heavy default exposed, both CLOSED
2026-09-04: **the population collapse at 10x** (802 -> 702 by frame 6000, 96 by 18000) was
the stress siting walling in house access cells — see the siting rules above; it now holds
802 -> ~794 over 6000 frames with the remaining deaths being ordinary starvation of the
~1% whose door region still differs from their tile region. **The heap corruption during
teardown** was two out-of-bounds writers in the sim: `CF_MakeAreaWork` zeroing 8 bytes past
the 0x60-byte TendArea, and `ConsumeProduce` bounding its produce-position loops by the
citizen-slot count instead of the status-array count. Both were caught live with
`BANISHED_REDZONE=watch` (allocator watchdog thread, no shim) and a hardware write
watchpoint (`standalone/hw_watch.cpp`). The run script still reports a post-quit crash as
"crashed during teardown" (exit 6) with the numbers intact.

## CSV columns

| column | meaning |
|---|---|
| `wall_s` | seconds since the world's first tick |
| `world`, `frame` | world number in this process, world-relative sim frame |
| `speed`, `sim_iters` | game speed, sim iterations in the window (= frames × speed) |
| `fps`, `frame_ms_mean/p95` | rendered frames per wall second; Draw-entry-to-Draw-entry period |
| `update_ms_mean/p95` | `GameUpdateLoop::Update` wall time per sim frame |
| `update_ms_per_iter` | update ms ÷ sim iterations: cost of ONE 60 Hz tick — the cross-speed, cross-build number |
| `draw_ms_mean` | `GameUpdateLoop::Draw` CPU time (submission; GPU time is not in it) |
| `ph_*_ms` | SIMPROF split of update ms: PairCache, Animation, EntityList, Weather, CitizenList, PostSim (UI/camera/audio) |
| `entities` | EntityList count (gameWorld+0xE8 → +0x10) |
| `citizens`, `home238`, `work248`, `busy` | town registry count; citizens with +0x238 / +0x248 set (home / workplace); AI with a non-empty action stack |
| `age_mean`, `hunger_mean` | population means (hunger climbing forever = nobody eats) |
| `ws_mb`, `private_mb` | working set / private bytes; steady growth is a leak |

## Recording and using a baseline

```bash
mingw32-make sa-stress STRESS_ARGS="--label baseline"
cp perf/results/<stamp>_baseline/standalone_stress.csv perf/baselines/current.csv
git add perf/baselines/current.csv
```

Every later `sa-stress` diffs against `perf/baselines/current.csv` automatically
(`--baseline none` to skip, `--baseline path` for another). `stress_report.py` flags a
`REGRESSION` when a metric moves past its threshold (10 % for timing, 15 % for memory).
Only compare runs of the **same** town (citizens, sets, seed, map) and prefer
`sim ms / iteration` as the headline: it is independent of game speed and of the GPU.

Baseline hygiene: same machine, nothing else heavy running, perf build only, and let it
finish (the first two rows are warm-up and are excluded from the steady-state numbers).
A perfectly flat `fps` (e.g. 139.9 every row on a 144 Hz display) is the swap chain's vsync,
not the CPU: the frame limiter is off but Present still waits. `update_ms`, `draw_ms` and
the per-iteration cost are unaffected; only treat `fps`/`frame_ms` as meaningful when they
move. Committed baseline `perf/baselines/current.csv` (= `2026-09-03_initial.csv`): -O2, Fair
climate, 402 citizens, 200 stress buildings incl. 4 fields + 4 orchards, 14.3k entities, 215
employed: 0.45 ms per sim iteration, draw 1.4 ms CPU, EntityList ~75 % of the sim,
CitizenAI/Citizen/Character Updates dominate it; memory flat.
Keep old baselines as `perf/baselines/<date>_<commit>.csv` when you replace `current.csv`
so a regression can be bisected by re-diffing.

## Retail as the target

Retail cannot be driven by these env vars, but the same world can be put in front of it:
save the stress town from our exe (`BANISHED_PAUSEMENU_BUTTON=3` = Save, or the autosave
harness) and load that save in `Application-x64.exe`. Retail prints no timings, so its
number comes from an external frame-time capture (PresentMon / RenderDoc / the profile
build `Application-x64-profile.exe`). Until that is wired up, treat retail as "60 fps at
1x with this town on this machine" and the goal as: our `sim ms / iteration` low enough
that speed 10 still holds 60 Hz — i.e. under ~1.6 ms per iteration with the render frame
on top.

## Known gaps in the town

- **No roads, no bridges.** Roads are drag-tool tiles and bridges are multi-segment
  drag objects; neither has a placement path outside the player tools yet. Every stress
  building is sited only where its footprint centre path-reaches the town cell, so nothing
  is cut off by a river, but travel is over raw terrain (slower, and no bridge/tunnel
  traffic is exercised). Porting a scripted road/bridge lay-down is the next realism step.
- **One pasture, one dock, one trading post** (their siting is bespoke); fields and
  orchards are per set and switched on by the force-start harnesses.
- **No construction traffic.** Everything is instant-built; builders/haulers to sites are
  not exercised. `BANISHED_BUILD_DEMO_INDEX` can leave one founding building unbuilt.

## Sampling profiler

`BANISHED_SAMPLE=<hz>` (default 1000; `standalone/sampler.cpp`) runs a thread that suspends
the sim thread on a timer, unwinds its stack and tags the sample with the SIMPROF phase.
Samples go to `standalone_samples.txt`; `tools/sample_report.py <file> --tag 4` (4 =
EntityList) prints self/inclusive time per function, `--callers X` / `--callees Y` the
edges. Exe frames are symbolised with addr2line, Runtime frames with the nearest DLL export
(verify a hot Runtime offset in Ghidra before trusting the name). Use it whenever the
`[COMPPROF]` table does not add up: COMPPROF only sees vtable dispatch, the sampler sees
libm, the Runtime and the walk itself. Sampling costs ~5% of update time; do not compare a
sampled run's numbers against the baseline.

## Optimisation log (2026-09-04, 800x10, sim ms / iteration)

| step | ms/iter | note |
|---|---|---|
| baseline after the population + heap fixes | 0.39 | `perf/baselines/2026-09-04_pre-perf-wave.csv` |
| skip `InternalBasePtr::CheckPtr` (Runtime body is `RET 0`; `g_checkPtrIsNop`, kill-switch `BANISHED_CHECKPTR_NOP=0`) | 0.39 | no measurable change |
| paired double-polynomial sin/cos in the character move tick (`character_trig.cpp`, `BANISHED_FAST_TRIG=0` restores libm) | 0.36 | MinGW sinf/cosf go through x87 long double: 16.7% of the phase |
| no `nearbyint`/`floorf` libcalls in per-tick code | 0.31 | nearbyint alone was 12% of the phase |
| particle un-stamp seen-set: hash table + once-per-frame per component (the 64-entry array also silently CAPPED at 64 emits) | 0.30 | |
| **register dispatch slot 0x140151f30 -> Runtime `MeshInstance::_rootOnlyUpdates`** | **0.25** | retail sets it on every intermediate speed iteration; the move tick then skips terrain + picking and MeshInstance::Update goes root-only. Unregistered, so 10x did 10 full updates |
| RDTSC instead of QPC in COMPPROF, `[HomeLoss]` watch opt-in (`BANISHED_STRESS_HOMEWATCH=1`) | 0.24-0.25 | profiling overhead is now within noise (`BANISHED_STRESS_COMPPROF=0` measures the same); run-to-run noise is about ±0.01 |
| tried, no gain: `-O3`, `-flto`, `-march=x86-64-v2` (`make standalone-perf PERF_EXTRA=...`, all 0.24 vs 0.25 at -O2, within run noise); successor prefetch in the tree walk (`BANISHED_TREE_PREFETCH=1`, 0.26; two levels 0.27); tree-walk corruption guards off (`BANISHED_TREE_GUARDS=0`, 0.24) | 0.25 | |
| iterations 16-20, each within noise (0.24): compile-time no-op CheckPtr in the perf build (`-DBANISHED_CHECKPTR_STATIC_NOP`, exe 1.3 MB smaller); memoised CharMgr cell-level divides; bit-identical inline copies of the trivial Runtime math exports (Vector3 ctor/Dot, Box::Intersect, Scalar::Max) in the move tick, proximity query and MoveTo; process-wide getenv cache (`standalone/env_cache.h`, force-included — ~370 getenv sites, 20% of the CitizenList phase was getenv); cached per-tick ResolveAddr of two .rdata floats | 0.24 | **final: 0.24 ms/iter = 38% below the 0.39 baseline after 20 iterations; the 50% target was not reached** |

What is left is faithful work: the update-tree pointer chase (~12%), the character move
tick (~25%: CharMgr cell registration, action executor, Runtime transform/animation calls),
MoveTo enter's spatial proximity query (~7%), residence heat ticks, ambient environment
scans, and the 300 s storage path-graph rebuild (7 A* pair costs per tick, the CitizenList
phase's 1800-frame spikes). Each was checked against its retail decompile before being
left alone. Reaching 50% would mean doing less than retail does per tick.

## Non-retail experimental knobs ("PerfX", 2026-09-05)

`standalone/perf_experimental.{h,cpp}` does LESS than retail per sim tick where the
difference cannot be seen: presentation work is deferred to the last speed iteration of a
frame, skipped for entities outside the camera frustum (6 world-unit margin, ~12 frames of
pan), or run at a lower rate. Gameplay state is never touched; everything skipped is
replayed when the sim goes idle (pause) so a paused camera pan shows the real pose,
position, icon and pick transform.

Build flag: `-DBANISHED_PERF_EXPERIMENTAL=1` makes the knobs default-on — `make
standalone-perf` sets it (`PERF_EXPERIMENTAL=0` to build them default-off; the stress
run's `opt=` line and the `[PerfX] master=...` stderr line say which). The release exe
gets `PERF_EXPERIMENTAL_RELEASE` (default 0); debug builds leave it undefined (off).
Runtime overrides, read once: `BANISHED_PERF_EXPERIMENTAL=0|1` master, then per knob:

| env | what is skipped | why it is invisible |
|---|---|---|
| `BANISHED_PERF_ANIM_CULL` | off-screen `MeshInstance::Update` stays root-only on the last iteration | the animation clock and root-motion offset still advance (retail's intermediate-iteration path); the pose is recomputed from the clock on the first visible tick, or by a zero-time update on pause |
| `BANISHED_PERF_GEOMXF_LASTONLY` | move-tick `Geometry::SetTransform` on intermediate iterations | only the last iteration is drawn; the render-side interpolation snaps from that same update; `model+0x148` (gameplay position) is still written every tick |
| `BANISHED_PERF_GEOM_CULL` | the last-iteration `Geometry::SetTransform` for off-screen citizens | nothing draws it; the margin updates it before it can enter the view; pushed on pause |
| `BANISHED_PERF_ICON_LASTONLY` / `_ICON_CULL` | status-icon repositioning on intermediate iterations / for off-screen owners | pure sprite placement; replayed on pause |
| `BANISHED_PERF_PICK_CULL` | `Picking::Scene::SetTransform` for off-screen citizens | cannot be clicked; replayed on pause |
| `BANISHED_PERF_PARTICLE_CULL` | ticking off-screen particle systems (frozen, not simulated) | the plume simply continues when the camera reaches it (margin +10 for rising smoke) |
| `BANISHED_PERF_AMBIENT_THROTTLE=n` | the biome histogram rescan of ambient-audio emitters, every n-th tick (8) | the region only changes when the map does; the temperature crossfade is still per tick |
| `BANISHED_PERF_TREE_SNAPSHOT` | the AVL pointer chase of the update-tree walk (flat in-order snapshot, rebuilt when the tree changes) | identical visit order — a data-structure change, not a behaviour change |
| `BANISHED_PERF_ANIM_BATCH_ALL` | at speed N, the N−1 intermediate root-only `MeshInstance::Update` calls per model whose per-tick root-motion offset cannot differ; the last iteration advances the animation by the ticks it skipped in one call | the move tick reads walk speed from `MeshInstance::RootAnimationOffset()`. For an instanced animation (`AnimationState` count 1, layer pool id ≥ 0, instanced byte set) that reads the `AnimationManager` pool pose, which `AnimationManager::Update` still refreshes with the real tick on every iteration; for an animation with no root motion (no non-zero node-0 keyframe position, cached per `Animation`) the offset is 0 for any dt. Those models are batched. A model mid-blend (count > 1), on a non-instanced root-motion animation, or with its instanced byte not yet set keeps the plain per-tick update (`PerfX::NeedsPerTickRootUpdate`, ~1.3% of the intermediate updates in the 800x10 town), because there the offset is the instance's own `+0x100`, `Animation::GetRootPoseAtTime` scales it by the dt it is given, and the blend weight changes per tick. Verified with `BANISHED_MOVETICK_TRACE=1` (4 citizens, 1500 frames, 10x): the per-tick root offset — `delta` divided by the citizen's own speed factor `vel38` — has mean 0.01482 / p50 0.014999 / max 0.01501 with batching on, 0.01484 / 0.014999 / 0.01501 off, and the blend-ramp samples (0.002–0.007) reappear (229 vs 228; the first ANIM_BATCH version had 0 and held values instead). The earlier "−2% mean step" (0.01418 vs 0.01447) was the run-to-run mix of `vel38` (0.69–1.06 per citizen and path), which two batching-OFF runs also show (0.01447 vs 0.01393). What still moves to the last iteration: animation events and the "animation finished" notification of a batched non-looping animation (up to N−1 ticks late). Bit-identical drawn pose. `_ANIM_BATCH_CULLED` is the off-screen-only variant (no measurable gain on its own) |

`BANISHED_PERF_TRACE=1` prints a census every 600 frames (`[PerfX] frame ... modelCulled=
... geomCulled= ... particleSkipped= ...`) and the pause-time flush counts.

### Optimisation log (2026-09-05, 800x10, sim ms / iteration, icons ON — see below)

| step | ms/iter | note |
|---|---|---|
| HEAD `aa71278` measured today (3 runs) | 0.266 | the recorded 0.24 was an icons-OFF run; see the caveat below |
| retail-faithful fixes only (`BANISHED_PERF_EXPERIMENTAL=0`) | 0.245 | `GetTypeIdByName` pointer cache (StatusIcon's per-tick name lookup was 9% of the phase), the per-burn `[HeatTick]` stderr line gated (1.7%), inline `sqrtf` in the move tick |
| + anim cull, geomXf last-only, icon last-only, ambient throttle | 0.231 | |
| + tree snapshot walk | 0.211 | |
| + particle freeze, pick cull, geom cull, icon cull, per-frame visibility cache, CharMgr cell memo | 0.209–0.214 | each within run noise (±0.005); kept because they remove work that grows with town size and zoom-in |
| iteration 11: Pickup resolver re-ran its whole ResolveAddr table every tick (one dependency stays missing so its "resolved" flag never set), `CitizenStatusIcon_SetFlag` called getenv per call, A* heuristic sqrt inlined (bit-identical) | 0.2095 | bucket-phase profile via the new sampler sub-tag 8 (`EntityList/buckets`) |
| **iteration 12: ANIM_BATCH_ALL** — one root-only animation update per model per frame instead of N | **0.192 / 0.196** (two runs) | treeWalk 1.36 → 1.11 ms/frame; culled-only variant 0.211 (no gain) |
| iteration 13: ANIM_BATCH_ALL made exact for walk speed — per-model decision (instanced / no root motion → batch; blend or non-instanced root motion → per tick), last iteration advances by the ticks actually skipped | **0.197 / 0.194** (two runs) | per-tick root offset now matches batching-off (see the knob table); ~1.3% of intermediate updates stay per-tick |
| tried, reverted: heading sincos memo table, `always_inline` node handler, 3-deep prefetch | — | neutral or worse |

**Result: 0.194 ms/iter (mean of two runs) = −27% same-day vs HEAD (0.266), −50% vs the recorded
0.39 pre-wave baseline, reached at iteration 12 with ANIM_BATCH_ALL and kept at iteration 13 (0.194–0.197) once it was made exact for walk speed; without it the plateau is 0.21.** What remains is
retail gameplay work per tick: the action executor + MoveTo (~13% of the phase), the
root-only animation update that root motion needs (~8%), CharMgr cell registration (3%),
MoveTo enter's proximity query (3%), the region A* in the CitizenList phase (7% of the
update), and the entity/component pointer chase of the walk itself. In the bucket phase specifically
(`--tag 8`): the CharMgr proximity query MoveTo::Enter runs (16% of the phase, builds a 9 KB
quadtree walker per query exactly like retail), the region A* behind FindBestTarget (15%), and
the standalone allocator critical section around every Allocate/Free (~13%, needed for the
async pathfinder threads). Candidates that would need a decision because they change
behaviour or safety: a heap open set for the region A* (order of expansion), dropping the
allocator lock, or skipping child mesh-instance updates for culled models (their animation
clocks would drift).

**Measurement caveat — the status-icon setting.** `GameStatusIconEnable` (registry,
`HKCU\Software\Shining Rock Software LLC\Banished`) changes the HEAD number by 10%
(0.240 off vs 0.266 on: every icon runs a per-tick name lookup + `Geometry::SetTransform`).
Only compare runs whose `standalone_error.log` shows the same `[GameSettings] Load:
StatusIconEnable=` value. Two more traps hit this session: a failed perf build leaves the
previous exe in the game dir (check the `build=` stamp in `report.txt` against
`standalone/build-perf/reimpl-x64.exe`), and `BANISHED_SAMPLE` + `BANISHED_STRESS_COMPPROF=2`
together make the run ~15x slower — use one or the other.

## Reading a regression

1. `report.txt` → which phase moved (EntityList vs CitizenList vs Animation vs PostSim).
2. If EntityList: `standalone_stress_compprof.txt` → which component type's Update grew.
3. Bisect with `git bisect run tools/stress_run.sh --frames 6000 --label bisect` and a
   small shell wrapper that greps `REGRESSION` out of `report.txt`.
