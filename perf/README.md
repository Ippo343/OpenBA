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
normal Easy founding town, on a **medium (384) map**, seed 12345:

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

Every building is COMPLETED (instant-build bracket), sited 1 tile apart on workable,
path-reachable ground around the founding cluster. Placement is deterministic per seed.

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
| **800 citizens, 10x** | **5.10** | 0.51 | most work per frame; population 802 -> 702 over 6000 frames |

Speed multiplies the same work; citizens change its shape (AI, pathing, hauling). The
default takes both. `BANISHED_STRESS_SPEED=1 BANISHED_STRESS_CITIZENS=400` reproduces the
gentler town when a 1x number is wanted. Two things the heavy default exposed and that are
still OPEN: **the population collapses at 10x** — 802 -> 702 by frame 6000, 338 by 12000,
96 by 18000, with mean hunger climbing past 14 (402 -> 359 in 6000 frames at 400 citizens;
no deaths at 1x within 18000 frames, so it is a game-time effect past ~30k ticks). That is
why the default run is 6000 frames: a 10x run longer than that measures a dying town. The
teardown fault first seen at 800x10 was the area-work null-allocator bug (fixed); the run
script still reports a post-quit crash as "crashed during teardown" (exit 6) with the
numbers intact.

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

## First optimisation lead

In the committed 800x10 baseline the EntityList phase is 4.9 of the 5.2 ms update, but
the per-component Update dispatch inside it (the `[COMPPROF]` total) is only 2.0 ms. The
other ~3 ms per frame is EntityList_Update's own bookkeeping around the dispatch: the
BST in-order walk, the removal/add passes and the 5-bucket cursor handling, run 10 times
per frame. Of the component time, CitizenAIComponent is 65 % (8.6 us per call).

## Reading a regression

1. `report.txt` → which phase moved (EntityList vs CitizenList vs Animation vs PostSim).
2. If EntityList: `standalone_stress_compprof.txt` → which component type's Update grew.
3. Bisect with `git bisect run tools/stress_run.sh --frames 6000 --label bisect` and a
   small shell wrapper that greps `REGRESSION` out of `report.txt`.
