---
name: stress-test
description: Run and read the performance stress test (tools/build.sh stress, BANISHED_STRESS) and the profilers (BANISHED_SAMPLE sampling profiler, per-component Update profile). Use for any perf measurement, perf regression check, A/B comparison or baseline update.
---

# Stress test / performance baseline

Full details: [perf/README.md](../../../perf/README.md). Read it before changing a baseline.

## Run
```bash
tools/build.sh stress                  # perf exe (-O2 -g, logs stripped, build/cmake-perf/)
STRESS_ARGS="--label x --speed 5 --citizens 600 --sets 3 --frames 6000" tools/build.sh stress
tools/build.sh stress debug            # same town on the -O0 exe: functional check only
bash <(tr -d '\r' < tools/linux_run.sh) stress <gamedir> <outdir> 3600   # the Linux exe (WSL2, lavapipe)
```
`BANISHED_STRESS=1` builds a medium map with 3 sets of every placeable building, 800
citizens in their own houses, 10x speed, every workplace staffed through the real seat
cascade, camera pinned on the town, FPS cap off, and a clean self-quit after 6000 sim
frames (60k ticks at 10x).

## Output: `perf/results/<stamp>_<label>/`
- `standalone_stress.csv` — one row per 600 frames: update/draw ms, SIMPROF phase split,
  population/employment/entities, memory
- the per-component-type Update profile
- `report.txt` from `tools/stress_report.py`, auto-diffed against `perf/baselines/current.csv`

**Headline metric: `sim ms / iteration`** (speed- and GPU-independent).

**Pathfinding** (PFBENCH): every row also carries `pf_*` worker columns (requests, search
us p50/p95/max, expanded, wait us, queue max, fails, worker busy %, path length, the
reserved `pf_shadow_*`) and the main-thread `pf_sync_wait_ms`, `ph_pathdistance_ms`,
`ph_fillzones_ms`; the report prints them under PATHFINDING (`n/a` against a baseline that
predates them). Path-heavy town: `STRESS_ARGS="--spread"` (= `BANISHED_STRESS_SETS=-3`).
Columns: perf/README.md "Pathfinding columns"; A/B recipe: `docs/extensions/handins/PFBENCH.md`.

**Hitches** (HITCH): every Draw-to-Draw period more than `BANISHED_STRESS_HITCH_MS` (default
7.14 ms = one 140 fps frame) over the rolling median of its class is a row of
`standalone_stress_hitch.csv`; `tools/hitch_report.py <results dir>` (run automatically, in
`report.txt` under `== hitches`) attributes each to the part that grew (phase / draw / outside /
harness) and, for a `BANISHED_SAMPLE=4000` run, lists the functions enriched inside the hitches.
Measure at 10x, 20x AND 50x: sim-periodic and frame-periodic causes differ. perf/README.md "Hitches".

## Profiling order
1. `BANISHED_SAMPLE=1000` — sampling profiler (`src/diag/win/sampler.cpp`), report with
   `tools/sample_report.py`. Use this first.
2. The per-component table second: it cannot see libm, runtime or walk time.
   `BANISHED_STRESS_COMPPROF=2` adds the `/tree` rows and the `/char` rows (characters by
   on/off-screen x walk/pathwait/timed/idle/other, with the D1/D2/E1 ceilings; how to read
   them: `docs/extensions/handins/OFFSCREEN.md` §3).

## Related flags
`BANISHED_MAP_SIZE=small|medium|large`, `BANISHED_CLIMATE=mild|fair|harsh`,
`BANISHED_CAMERA_PIN=town[,dist]`. All flags: `src/app/options.h`.

## Keep the measurement clean
Nothing else heavy may run during an A/B (no repo-wide searches, no second build). Trace
flags are a struct load via `Opt()`, so a per-call `if (Opt().someTrace)` in tick code is
cheap — but never add a `getenv`.
