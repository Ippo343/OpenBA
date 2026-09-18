---
name: stress-test
description: Run and read the performance stress test (mingw32-make sa-stress, BANISHED_STRESS) and the profilers (BANISHED_SAMPLE sampling profiler, per-component Update profile). Use for any perf measurement, perf regression check, A/B comparison or baseline update.
---

# Stress test / performance baseline

Full details: [perf/README.md](../../../perf/README.md). Read it before changing a baseline.

## Run
```bash
mingw32-make sa-stress                 # perf exe (-O2 -g, logs stripped, standalone/build-perf/)
mingw32-make sa-stress STRESS_ARGS="--label x --speed 5 --citizens 600 --sets 3 --frames 6000"
mingw32-make sa-stress-debug           # same town on the -O0 exe: functional check only
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

## Profiling order
1. `BANISHED_SAMPLE=1000` — sampling profiler (`standalone/sampler.cpp`), report with
   `tools/sample_report.py`. Use this first.
2. The per-component table second: it cannot see libm, runtime or walk time.

## Related flags
`BANISHED_MAP_SIZE=small|medium|large`, `BANISHED_CLIMATE=mild|fair|harsh`,
`BANISHED_CAMERA_PIN=town[,dist]`. All flags: `standalone/options.h`.

## Keep the measurement clean
Nothing else heavy may run during an A/B (no repo-wide searches, no second build). Trace
flags are a struct load via `Opt()`, so a per-call `if (Opt().someTrace)` in tick code is
cheap — but never add a `getenv`.
