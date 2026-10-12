# PFBENCH hand-in (2026-10-12)

PATHRESEARCH.md §5, the MEASUREMENT side only: the pathfinding columns in the stress harness,
the report section, the spread town, unit tests. No pathfinding algorithm changed; no new
`BANISHED_*` flag. Static gate only (build debug + perf, unit): the game was NOT run by this
block, so every column is unverified on a live run until the coordinator's pair (§4).

## 1. What was added

| Where | What |
|---|---|
| `src/diag/pf_stats.h` (new, header-only) | The window math: `PfStats::Request` / `Shadow`, `Window` (worker side), `MainWindow` (main side), `Percentile` (the harness's existing nearest-rank rule, index `floor((n-1)*p)`), `Summarize` -> the CSV row. Times are raw ticks until `Summarize` converts them. Samples capped at 131072 per window for the percentiles only; totals, max and fails see every request. |
| `src/diag/stress_harness.h:68-105` | `g_pfProfOn`, the hooks `PfProf_Enqueue / _Search / _Taken / _Main`, the reserved `StressHarness_PathShadow(...)`, the `PfMainTimer` scope bracket; `StressHarness_Spread()` (:45). |
| `src/diag/stress_harness.cpp` | `g_pfProfOn` (:73) set to 1 in `Resolve` when the harness is on (:106); slot stamp table + lock + windows (:153-179); CSV header columns (:469-475); the row: one lock to swap the windows out, `Summarize`, the columns and a `[Stress::pf]` stderr line (:509-550); sim-seconds accounting in `UpdateEnd` (:612-629; the frame's `m_gameTime` step x speed; the window resets with the world); the hook bodies (:713-757). |
| `src/game/Action/action_pf_astar.cpp:649-659` | `ApplyCommand` case 5: RDTSC before / after `grid.Run`, then `PfProf_Search(slot, t0, t1, cells, sequence - open.count)`. |
| `src/game/Action/action_pf_astar.cpp:803-805` | `PathfinderThread::RequestPath`: `PfProf_Enqueue(slot, queueCount)` before the command is queued. |
| `src/game/Action/action_base_move.cpp:298` | `TakePath`: `PfProf_Taken(slot)` (end-to-end latency). |
| `src/game/Action/action_pf_roots_route.cpp:115` | `Map::PathDistance`: `PfMainTimer(PF_MAIN_PATHDISTANCE)`. |
| `src/game/Action/action_pf_roots_road.cpp:56` | `Map::FillZones`: `PfMainTimer(PF_MAIN_FILLZONES)`. |
| `src/game/Action/keystone_a_zone.cpp:76` | `RunPathQuery` (road tool / boat route, request + spin): `PfMainTimer(PF_MAIN_SYNCWAIT)`. |
| `src/game/Game/starttown.cpp:1236-1242, 1345-1373` | The spread layout (§5). |
| `src/app/options.cpp:201`, `options.h:99` | `stressSets < 0` is no longer clamped to 0 (it means spread). No new flag. |
| `tools/stress_report.py` | `summarise_pf`, `print_pathfinding`: the PATHFINDING section and its own baseline table; `n/a` for every pf row when either CSV lacks the columns. |
| `tools/stress_run.sh` | `--spread` (= `BANISHED_STRESS_SETS=-N`); doc lines. Also a FIX: `BUILD` held both the build script path and the 0/1 build switch, so a run without `--no-build` executed `1 build perf` and failed; the script path is now `BUILDER`. |
| `perf/README.md` "Pathfinding columns", `.claude/skills/stress-test/SKILL.md` | The columns and the spread switch. |
| `tests/unit/pf_stats_test.cpp` | 7 tests: percentile rule (empty, one, even, 20 shuffled, clamped p), window totals, the unit conversion of every column, the empty window, shadow tallies, window independence across the swap, the sample cap. |

## 2. Gating: the faithful hot path is untouched when the harness is off

Pattern: the existing `g_compProfOn` (`stress_harness.h:50`, read by `entitylist.cpp:521`): a
plain global int, set once at boot from `Opt().stress` (`Resolve`, `stress_harness.cpp:106`), read
with one predictable branch at each hook. With `BANISHED_STRESS` unset `g_pfProfOn` is 0 for the
life of the process and:

- `ApplyCommand` case 5 (`action_pf_astar.cpp:653`, `:655`): `pfT0 = g_pfProfOn ? rdtsc : 0`
  and `if (pfT0 != 0)`; no call, no store. `grid.Run` and everything after it (the B6 row 47 lock
  scope, the row 961 race and its signal fence) are byte-for-byte as before.
- `RequestPath` (`:805`), `TakePath` (`action_base_move.cpp:298`): `if (g_pfProfOn)`.
- `PathDistance` / `FillZones` / `RunPathQuery`: `PfMainTimer`'s constructor loads `g_pfProfOn`
  once; its destructor tests the saved `t0`.
- NOTHING is inside `FindPath`, `Relax`, `PathOpenSet::Insert` / `Delete`, `Neighbour`,
  `Heuristic` or `Run`: the expansion count is read AFTER `grid.Run` from the search's own
  state (`search.sequence`, reset to 0 by `FindPath` and bumped once per `Relax`, minus
  `search.open.count`, the entries still open).
- No call with side effects sits inside a printf: the summary is computed into a local
  `PfStats::Summary` first and the `fprintf`s only read its fields.

With the harness ON, the worker pays two RDTSCs and one uncontended mutex per request (never per
node); the main thread pays one mutex per enqueue and per take, and two RDTSCs plus one relaxed
atomic add per `PathDistance` / `FillZones` / `RunPathQuery` call. The worker's lock and its
window allocations happen after `grid.Run` and before the result copy, so `ready` is published a
few hundred ns later than without the harness. Retail's timing is already machine-dependent
(PATHRESEARCH §1.5); the coordinator should still check that `sim ms / iteration` of a faithful
run stays within noise of the last baseline (§4).

Thread safety: the slot stamp table is written by the main thread before `EnqueueNode` takes the
pathfinder lock and read by the worker after it dequeued the command under that lock (the lock
orders them). The worker's samples and the main thread's e2e / queue-max go into windows under
`s_pfLock`; the report row swaps both out under the lock once. The three main-thread brackets
are relaxed `std::atomic<long long>` sums, exchanged to 0 at the row (they may be called from the
loader thread during world build).

## 3. The columns (appended after `private_mb`)

`pf_requests, pf_req_per_sim_s, pf_search_us_p50, pf_search_us_p95, pf_search_us_max,
pf_expanded_mean, pf_expanded_p95, pf_wait_us_p50, pf_wait_us_p95, pf_queue_max, pf_fail,
pf_worker_busy_pct, pf_len_mean, pf_e2e_us_p50, pf_e2e_us_p95, pf_shadow_n,
pf_shadow_equal_cost, pf_shadow_same_cells, pf_shadow_worse, pf_shadow_speedup,
pf_sync_wait_ms, ph_pathdistance_ms, ph_fillzones_ms`

Meanings: perf/README.md "Pathfinding columns". Beyond the plan's list: `pf_e2e_us_p50/p95`
(enqueue -> `TakePath`, the latency a citizen actually stands still for, including the walker's
once-per-update poll), `pf_shadow_n` and `pf_shadow_speedup` (retail search ticks / extension
search ticks over the paired requests). The shadow columns are written as 0 until something
calls `StressHarness_PathShadow`.

Report (`tools/stress_report.py`, after the component profile):
```
PATHFINDING  requests 418/window (41.2 per sim s)  worker busy 37.0%
  search us   p50 310  p95 2900  max 61000      expanded mean 2150 p95 14800
  wait us     p50 40.0  p95 9000  queue max 63.0   failed 24.0   path len mean 45.5
  e2e us      p50 900  p95 12000  (enqueue -> TakePath, includes the walker's poll)
  shadow      not run (no extension called StressHarness_PathShadow)
  main        PathDistance 0.02 ms/frame  FillZones 0.00  sync spin 0.00 ms
  vs baseline (n/a = the baseline CSV has no pf_* columns)
  metric                   baseline        now    delta
  search us p95                 n/a       2900      n/a
  ...
```
(synthetic numbers: the report was exercised on a CSV built from `current.csv` plus invented pf
columns, and on `current.csv` itself, which prints `PATHFINDING  n/a (this CSV has no pf_*
columns: written before PFBENCH)`.) Steady-state aggregation: window means of the per-window
percentiles, the worst max, summed fails, shadow percentages weighted by each window's count.
REGRESSION thresholds 15 % on search p95, wait p95, worker busy %, expanded, e2e and the three
main-thread costs.

## 4. How the coordinator runs the pair

Everything launches the game, so it is the coordinator's. Perf build, nothing else heavy running.

1. Faithful, default town (the control; also re-cut the baseline so later reports compare pf
   columns instead of `n/a`):
   `STRESS_PROFILE=faithful STRESS_ARGS="--label pf_faithful" tools/build.sh stress`
   then, if the coordinator wants it, `cp perf/results/<stamp>_pf_faithful/standalone_stress.csv
   perf/baselines/current.csv` (keep the old one as `perf/baselines/<date>_<commit>.csv`).
2. Faithful, path-heavy: `STRESS_PROFILE=faithful STRESS_ARGS="--label pf_spread --spread
   --baseline perf/results/<stamp>_pf_faithful/standalone_stress.csv" tools/build.sh stress`.
   If `pf_worker_busy_pct` is still < 50 %, the plan's heavier knobs:
   `--spread --map large --citizens 1600 --speed 10`.
3. When `ext.fast_paths` exists: the same STRESS_ARGS with the extension on and everything else
   off (PATHRESEARCH §5 "The A/B pair", the settings.cfg edit), `--baseline` = run 2's CSV.
   Decide on worker busy %, search p95, wait p95, `sim ms / iteration`, and, in shadow mode,
   `pf_shadow_worse == 0`. Three runs per side (§5 of the plan).
4. Sanity on run 1: `pf_requests` > 0 every window, `pf_fail` small, `pf_expanded_mean` in the
   plan's predicted 600-10,000 range for a medium town, and `sim ms / iteration` within noise of
   the previous baseline (the harness's own cost).

## 5. The spread scenario

Done through an existing flag's new value, no new flag: `BANISHED_STRESS_SETS=-N` (`--spread` in
`stress_run.sh`) = N sets in the spread layout. `StartTown_StressSpawnStructures` already sites
every building by a ring search from the town centre; it now searches from an anchor. Compact
(the default, any N >= 0) is anchor = centre, ring limit `mapW/2 - 4`: identical to before.
Spread: the town hall at the centre as before, every set building (workplaces, storage, market,
the hostel) around `(cx - mapW/4, cy)`, every house around `(cx + mapW/4, cy)` (both clamped 12
cells inside the map), ring limit `mapW - 8`; the same `fits()` tests apply (workable, reachable
from the town centre, no pocket, door reachable). On medium that is ~190 cells between the two
halves, on large ~256 (the walker's 0x100-cell re-request boundary). The CSV meta line now says
`layout=spread|compact` and prints `sets` as |N|.

## 6. Doubts

1. **`pf_expanded` for retail is an upper bound**: `sequence - open.count` = expansions +
   decrease-key re-inserts (a decrease-key deletes and re-inserts an open entry, PATHRESEARCH
   §1.2). An exact pop count would need a counter inside `FindPath` (forbidden) or a whole-grid
   scan of `closedStamp` per request (too costly). `ext.fast_paths` must report the same
   quantity (inserts minus still-open) or say in its hand-in what it reports; in shadow mode
   both counts go to `StressHarness_PathShadow`.
2. **Cross-thread RDTSC**: the wait is (worker's TSC at search start) - (main's TSC at enqueue).
   That needs an invariant, core-synchronised TSC; true on current desktop CPUs, and the same
   clock `[COMPPROF]` already uses. A negative difference is dropped (no wait sample).
3. `pf_queue_max` is `queueCount` read unlocked (relaxed) at each command-5 enqueue and counts
   every queued command kind (0-5), not only requests.
4. A result slot >= 8192 gets no wait / e2e sample (still counted as a request). The stress
   town should stay far below that; `pf_requests` vs the sample counts is not reported.
5. `pf_len_mean` is the worker's full path length (capped 0x4000), not the 256 cells MoveTo keeps.
6. `pf_req_per_sim_s` uses the frame's `m_gameTime` step x speed; if a frame's `Time` is not
   the sim step (variable dt), this is still simulated time but not "frames x 1/60".
7. The spread layout was not run. Risks: a seed whose west or east quarter is water sends the
   ring search back toward the centre (it still places, just less spread); the hostels live with
   the workplaces; wider rings make siting slower (each candidate still pays the same tests).
   `[Stress::place] SPREAD layout:` and the DONE line's `maxRing` show what happened.
8. Not built here: the plan's shadow-mode switch (a level of `BANISHED_STRESS_COMPPROF`, open
   question 4) and the request log `standalone_pathlog.bin` (for 4.3). Both belong to the
   extension / GPU-bench blocks; `StressHarness_PathShadow` is the seam for the first.

## Coordinator's notes (2026-10-12)

- Baseline (faithful, compact, main `dab117dc1`): `perf/results/20261012_093810_pf_faithful` -- requests
  28.3 / sim s, worker busy 5.8%, search p50 27 us / p95 1034 us / max 9.5 ms, expanded mean 1603,
  wait p95 2.7 ms, e2e p95 20 ms, failed 1772 of 2827 per window; sim 0.17 ms / iteration (-0.4%
  vs the old baseline: the harness's cost is within noise). Machine load CLEAN.
- Spread (faithful): `perf/results/20261012_094046_pf_spread` -- 15.8 / sim s, busy 4.9%, search p95
  2008 us, expanded mean 2818 (p95 13954), queue max 1232, wait p95 4.3 ms, PathDistance 0.33 ms/frame,
  sim 0.19. Worker busy stays under 5%: in this town the pathfinder's cost is latency, not throughput.
- The shadow switch (doubt: plan Q4) is `BANISHED_STRESS_COMPPROF=3` (level 1 + shadow).
