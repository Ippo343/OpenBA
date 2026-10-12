# PATHWORKERS hand-in (2026-10-12)

Block PATHWORKERS, base main `5081534bb`. Builds PATHRESEARCH §4.2 as one class B extension,
`ext.path_workers` (default off, formatVersion 1): retail's single pathfinder worker becomes a dispatcher
in front of a pool of N search threads. Every path is byte-identical to the serial worker's; only the
moment it is published changes. It composes with `ext.fast_paths`. The game was NOT run (agent rule):
only `tools/build.sh build`, `build perf` and `unit` were run.

## 1. What was built

| Commit | What |
|---|---|
| `ad3369633` | `src/ext/path_workers/pool.{h,cpp}` (the core, standard C++), `path_workers.{h,cpp}` (the game glue), the registry row, the `ext.fast_paths` composition API, the seams |
| `34e2fbad9` | `tests/unit/ext_path_workers_test.cpp`, 5 tests (unit 1052 -> 1057, 0 failures) |
| this one | this hand-in, the STATUS bullet |

- **The core** (`pool.h`, `pool.cpp`). A FIFO job queue under a `std::mutex`, two condition variables (work
  queued / pool idle), an in-flight count (queued + running). `Submit`, `Barrier`, `WorkerMain(i)`, `Stop`.
  It creates no thread. `DispatchCommand` (`pool.h:126`) is the dispatcher's one decision per command:
  - 0 / 1 / 4 (parameters): applied at once, in queue order.
  - 5 (request): the parameters as they stand NOW are copied into a `Job` with the request; the job is queued.
  - 2 / 3 (cell edits): `Barrier()` (wait until no job is queued or running), then applied.
- **The threads.** `PoolThread` (`path_workers.cpp:123`) is a `System::Thread` subclass: the engine's primitive
  from `src/engine/system/Thread.h`, since the tree had no `std::thread`. Each is Started once; its `Run` is
  `Pool::WorkerMain`, which blocks on the pool's condition variable between jobs and returns after `Stop`.
  `~Thread` then signals its stop event and joins it. The only `<thread>` use in the game is
  `std::thread::hardware_concurrency()` for N; the unit tests drive `WorkerMain` from `std::thread`.
  The core uses `std::mutex` / `std::condition_variable`, which `src/platform/common/event_std.cpp` already
  uses in the game build.
- **N** = hardware threads - 2, clamped to [1, 4] (`ChooseWorkers`, `pool.cpp:10`): the main thread and the
  dispatcher keep theirs. N = 1 runs the same pool with one worker; its results are the serial path's.
- **Registry.** `Ext::Extensions::path_workers` (`registry.h`), row "Parallel pathfinding" in `registry.cpp`
  (Tier::Extension, Class::B, default false, formatVersion 1).

## 2. The seams (file:line)

| Seam | Where | Faithful cost |
|---|---|---|
| Dispatch | `Game/pathfinder_seed.cpp:68`, `PathfinderThread::Run` 0x1400a6360, top of the loop: `if (PathWorkers::Engaged()) { PathWorkers::Dispatch(*this); return; }` | one byte load + not-taken branch per command (perf object: `movzbl (%rdi)` with the `.refptr` load hoisted out of the loop) |
| Teardown | `Game/keystone_a_gamedtor.cpp:196`, first statement of `~PathfinderThread` 0x1400a6720: `if (PathWorkers::Engaged()) PathWorkers::ReleaseFor(this);` | one byte load per pathfinder destruction (once per game) |
| Selection | `Game/gameupdateloop_load.cpp:737` and `:1332`, right after each `FastPaths::SelectForGame()` | two calls per game load |

- **Untouched:** `ApplyCommand` 0x1400a6430 and its case 5 (row 961's race kept in the faithful body), the search
  bodies, `TakePath`, the enqueuers.
- **`g_mode`** (`path_workers.h`) has two bits: bit 0 = selected for this game, bit 1 = a pool is alive. `Engaged()`
  is true while either is set. If a later selection turns the extension off while a pool still belongs to a
  pathfinder, the dispatcher drains that pool (`Barrier`) and then runs commands serially until `ReleaseFor`.
  This should never happen (retail destroys the old Map before a new game runs), but it stays exact if it does.
- **The selection is final before the first request** (FASTPATHS §2 argument, same call sites). The pool is
  built lazily by the dispatcher on its first command after selection (`BuildState`, `path_workers.cpp:143`).
  It logs `[PathWorkers] pool up: N workers, grid WxW, node scratch X MB`. Selection logs
  `[PathWorkers] selected for this game: workers=N` (twice per game, like FastPaths). Teardown logs
  `[PathWorkers] pool down: N workers joined, R requests`. The faithful profile logs nothing.
- **Priority lane: not built.** The road-tool / boat spins (`QueryPath`, `pathtool_setselected.cpp`) send an
  ordinary command 5, and the dispatcher cannot tell them from a citizen's without a new field. With N workers
  they wait behind at most the jobs already queued ahead of them in the pool, not behind every search in turn.

## 3. Why every result is byte-identical to the serial path

1. **Each search is a pure function of three inputs:** the parameters (step costs, passable mask, the three
   flags), the cell bytes, and (start, goal). `GridPathSearch::Run` resets its open set and bumps a generation
   stamp, so nothing carries over from an earlier search. That holds on a fresh per-worker record too: its
   nodes start at the resize fill `{0, -1, 0, 0, 0}`, generation 1, so no stamp ever matches.
   `FastPaths::Searcher` is likewise per-request (stamps). The fast search adds a fourth input: the
   present-type mask.
2. **Parameters.** The dispatcher applies 0 / 1 / 4 in queue order to `PathfinderThread::grid`'s own fields
   through retail's `ApplyCommand`. At each command 5 it copies them into the job (`GameHost::MakeJob`). That
   copy is exactly what the serial worker would hold at that queue position. The pool worker writes the
   copy into its own `GridPathSearch` before searching.
3. **Cells.** Only commands 2 / 3 write `cellFlags`, and only on the dispatcher, after a barrier. So:
   - every edit queued before a request is applied before the request is dispatched;
   - no edit queued after it can land while it runs.
   Nothing else writes `cellFlags` after `Pathfinder_Seed`.
4. **The fast mask.** `ext.fast_paths`' per-type counts stay on the dispatcher. The counts' rescan, the
   case-2 hook (inside `ApplyCommand`) and `PresentTypesNow` all run on the dispatcher, in queue order, as
   before. The mask is taken at dispatch and travels in the job.
5. **Same search code.** The worker runs retail's own `GridPathSearch::Run`, or `FastPaths::RunWith` (the same
   `Searcher` code as the single-worker `Run`, which is unchanged).

**Why FIFO completion is not required.** Each result goes to its own slot, and every reader is keyed by its
own ticket:
- `MoveTo::Execute` (`moveto.cpp:294-315`) and `TakePath` read only their ticket's slot, after `ready`;
- `QueryPath`'s spin (`keystone_a_zone.cpp:88`) and the path tool's spin (`pathtool_setselected.cpp:405`)
  test `token == ticket.token && ready`.

A slot returns to the free list only through `TakePath` or the path tool's clear, both after `ready`. A
pending ticket is never freed early: an abandoned MoveTo leaks its slot, as in retail. So two in-flight jobs
never target one slot, and nothing reads a slot other than its own. The order in which slots become ready is
invisible except as timing, and retail's timing is already machine-dependent (PATHRESEARCH §1.5).

**Unit evidence** (`tests/unit/ext_path_workers_test.cpp`):

| Test | Result |
|---|---|
| `pool_matches_serial` | 3000 random command streams (750 per N = 1..4): grids 4-40 wide, random masks / costs / flags, 50% requests, 20% cell edits, half with the fast scale. 118,006 requests (76,102 found), 47,062 cell edits: **0 mismatches** in count and cells per slot, every slot published exactly once; 0 edits applied while a job was queued or running; 0 jobs saw the cell bytes change during their search (checksum before / after). Completion order is shaken with yields. |
| `edits_matter` | Control: hoisting each stream's edits ahead of its requests changes 232 / 300 streams, so the barrier is load-bearing in the streams above. |
| `stop_under_load` | 4000 jobs, `Stop` at once: all 4000 run exactly once, 4 threads join, a `Submit` after `Stop` is ignored. |
| `stop_before_start_and_idle` | Jobs queued and `Stop` called before any thread exists still all run. An idle pool's `Barrier` returns and `Stop` releases parked threads. |
| `choose_workers` | The N rule. |

The mock is retail's protocol reduced to what it touches. The real `GridPathSearch` cannot link into `unit`,
so its search is `FastPaths::Searcher`, which `ext_fast_paths_test` proves equal to retail's at scale 1.

## 4. Lock discipline

- **The command queue:** `PathfinderThread::lock`, exactly as retail. The dispatcher's pop
  (`path_workers.cpp:237-248`) is `Run`'s pop statement for statement.
- **The pool:** its own `std::mutex`. The dispatcher never holds the pathfinder lock while it waits in
  `Barrier`.
- **The publish** (`GameRunner::RunJob`, `path_workers.cpp:113-118`) holds `PathfinderThread::lock` for the
  whole publish: `path.SetSize(n)`, the cell copy, then `ready = 1` as a release store, then unlock.
  - That excludes `AllocResult`'s table grow (main thread, same lock), so row 961's window does not exist on
    this path. Retail's unlocked copy is never used by a pool worker.
  - The main thread's unlocked reads (MoveTo's poll, `TakePath`) see `ready` only after the cells.
  - Contention: one short critical section per published path (a 0x100-cell copy is typical).
- **No deadlock.** A pool worker takes only the pool mutex (never while searching) and the pathfinder lock
  (publish only). The dispatcher takes the pathfinder lock (pop only) and the pool mutex. The main thread
  takes the pathfinder lock briefly (enqueue, grow, spins, and `DrainQueue`, which releases it on every
  iteration).

## 5. Memory

Per pool worker, allocated when the pool is built and freed at teardown:

| Item | Size | Small 256² / medium 384² / large 512² |
|---|---|---|
| Node array | 0x18 bytes per cell (retail's record) | 1.5 / 3.4 / 6.0 MB |
| Open-set chunks | grow to the worker's largest open set, as retail's | -- |
| Path buffer | 64 KB | -- |
| `FastPaths::Searcher` (only with `ext.fast_paths`) | 16 bytes per cell plus bucket entries | 1.0 / 2.25 / 4.0 MB |

At N = 4 on a medium map: about 13.5 MB of node arrays, 22.5 MB with fast_paths. On a large map: 24 MB and 40 MB.
The cell bytes are shared, not copied.

## 6. Shutdown

The stop protocol mirrored is `Map::~Map` 0x14009ed60: `DrainQueue` (start the worker while commands are
queued, yield until the queue is empty, `WaitForFinish`), then `~PathfinderThread`. Under the extension:

1. `Dispatch` returns once the command queue is empty. It does NOT wait for jobs, so the worker's run flag
   clears as in retail and every later enqueue Starts it again. `DrainQueue` therefore behaves as today.
2. `~PathfinderThread`'s first statement calls `ReleaseFor(this)` (`path_workers.cpp:259`):
   - `Pool::Stop`: every job still queued runs and publishes into the results table, which is still alive
     at this point;
   - each `PoolThread` is deleted (`~Thread` joins it);
   - the per-worker scratch is freed;
   - bit 1 is cleared.
3. Only then does retail's body free the results, the cell bytes and the queue. **No pool thread outlives the
   grid it reads.**

`Map::~Map` is the only destruction site (map teardown, game reload, quit). Exiting the process without
destroying the Map leaves parked pool threads that the OS kills, which is harmless.

## 7. What the coordinator measures

- **The faithful gate.** Invariants unchanged; no `[PathWorkers]` line; resave 5/5. The faithful changes are
  two byte loads (per command, per pathfinder dtor) and two calls per Load.
- **The all-on gate.** Boots, runs the stress town, quits clean. Expect:
  - `[PathWorkers] selected ... workers=N` twice;
  - `pool up` once per game;
  - `pool down` at quit / reload.
  `ext.fast_paths` composes with it, so the all-on profile runs both.
- **A/B** (`tools/stress_run.sh`): `STRESS_EXT=path_workers` against faithful, and `STRESS_EXT=fast_paths,path_workers` against `STRESS_EXT=fast_paths`. Columns:

  | Column | What it shows under the pool |
  |---|---|
  | `pf_wait_us_p95` | enqueue to search start. Includes the hand-off to the pool, so it is THE column for this extension. |
  | `pf_e2e_us_p95` | enqueue to `TakePath`, what a citizen feels |
  | `pf_queue_max` | the command FIFO's depth at enqueue. It should fall, because the dispatcher drains the FIFO into the pool at once. The pool's own backlog shows up in wait p95, not here. |
  | `pf_search_us_*` | should be unchanged per request (expect slightly higher from cache sharing) |
  | `pf_worker_busy_pct` | **the SUM over pool workers** (one `PfProf_Search` per request, from the worker that ran it), so it can exceed 100% at N > 1. Divide by N for per-worker utilisation. The dispatcher's own time is not counted. `pf_stats.h` needed no change: its window is mutex-guarded, and each `PfProf_Search` reads only its own slot's enqueue stamp. |
  | sim ms / iteration | must not regress on the faithful profile (DESIGN rule 6) |

- **Expectation.** PFBENCH's compact and spread towns at 800 citizens keep one worker under 5% busy (spread:
  4.87%). So the queue rarely backs up, and the pool should show almost nothing there: wait p95 falls only by
  the share of requests that queued behind another. The win appears when the queue backs up, which needs
  more requests per second and longer searches. The stress knobs that should show it:
  - `--citizens 1600 --map large --spread --speed 10`, faithful vs `path_workers`. Faithful wait p95 should
    climb into tens of ms there.
  - Add `--sets 1` for fewer, farther workplaces.

  With `ext.fast_paths` on, searches are ~8x cheaper and the queue backs up even less: the pool's gain on top
  of fast_paths is expected to be small except in that large / 1600 town.
- **Shadow mode under the pool:** not supported. `FastPaths_ShadowHook` uses the single `Worker()` state, so
  pool workers do not call it. Measure the shadow with `path_workers` off (the shadow compares search bodies;
  the pool changes none).

## 8. Doubts

- **Not run in the game.** The protocol is unit-tested on a mock. The game glue (the pop, the publish, the
  per-worker `GridPathSearch` built like `GridCtor`, `System::Thread` lifetime) is covered only by review and
  the build. The all-on gate is the first live test.
- **Wake latency.** A pool worker parks on a condition variable (winpthreads on MinGW). Wake-up costs are
  microseconds, but a job that waits for a free worker sits in the pool queue, not the command FIFO.
  PFBENCH's queue depth does not see that (see `pf_queue_max` above).
- **Cell-edit bursts.** A building placement can queue many 2 / 3 commands. Each one is a barrier, but only
  the first waits; the rest find the pool idle. The cost is one mutex round-trip per edit.
- **The road-tool preview with `ext.fast_paths`** is unchanged by the pool: same cells as without it.
- **N from `hardware_concurrency`.** On a 4-thread machine N = 2; on 2 threads, N = 1. The pool still exists
  at N = 1: that keeps the dispatcher off the search, so a long search no longer blocks the FIFO from
  draining, though requests still run one at a time.

## Benchmark (coordinator, 2026-10-12; idle machine, every report `machine load: CLEAN`)

| run (large map, 1600 citizens, spread, speed 10) | search us p50 / p95 / max | wait p50 / p95 | e2e p95 | worker busy | req / sim s | sim ms/iter |
|---|---|---|---|---|---|---|
| faithful (`20261012_105902_pfh_faithful`) | 1223 / 8887 / 30304 | 826 ms / 947 ms | 961 ms | 58.9% | 25.0 | 0.32 |
| ext.path_workers, 4 workers (`20261012_110144_pfh_workers`) | 36 / 3699 / 18071 | 18 us / 1.0 ms | 18.8 ms | 22.9% (sum of 4) | 41.0 | 0.31 |
| ext.fast_paths (`20261012_110422_pfh_fast`) | 4.1 / 674 / 4118 | 6.8 us / 1.8 ms | 15.1 ms | 4.6% | 40.5 | 0.31 |
| both (`20261012_110655_pfh_both`) | 4.8 / 780 / 4865 | 15 us / 39 us | 14.7 ms | 5.5% | 41.2 | 0.31 |

`STRESS_EXT=<ids>` ran each cell (settings.cfg per-entry profile). `[PathWorkers] pool up: 4 workers, grid
512x512, node scratch 24.0 MB` in standalone_error.log on the pool runs. First live runs of the pool: no
crash, no hang, sim within noise.
