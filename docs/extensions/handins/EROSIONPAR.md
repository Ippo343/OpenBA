# EROSIONPAR -- the droplet erosion stage, exactly parallel (loading-speed session, LOADPLAN §3)

Block EROSIONPAR. `wg_f390_erosion` (retail `0x1400cc080`, `src/game/Game/worldgen_f390_erosion.cpp`) cost about 3.0 s
of the large-map new game after block EROSION (EROSION.md). It now runs its droplets speculatively on the
`System::ParallelFor` pool and commits them in droplet order. Ruling 2 holds: the heightmap and the `Random` state are
bit-identical to the sequential body for every input and every worker count.

**Bench, 1024^2 grid, 16M droplets (the large map's work):** 6.10 s pre-EROSION, 5.29 s sequential (EROSION),
**2.97 s with 3 workers (2.05x)**, **2.61-2.67 s with 7 workers (2.29-2.34x)**. IDENTICAL on every run. The game
figure is the coordinator's census.

## 1. As built

### 1.1 One droplet, three kinds of memory access

The droplet is one function template, `wg_erosion_droplet<Mem>`. Its float expressions, branches and order are
EROSION's body, unchanged. Only the heightmap access goes through `Mem`:

| `Mem` | used by | Load | Store |
|---|---|---|---|
| `ErodeDirect` | `wg_f390_erosion_sequential` (0 workers, or fewer than `minParallel` = 16,384 droplets) | `hm[i]` | `hm[i] = v` |
| `ErodeLive` | the commit thread's own runs | `hm[i]` (the commit is the only writer) | relaxed atomic store, logged for the footprint |
| `ErodeSolo` | the commit thread while SOLO (§1.5) | `hm[i]` | relaxed atomic store, no log |
| `ErodeSpec` | the speculators | the droplet's own last store to `i` if it made one, else a relaxed atomic load of `hm[i]` | appended to a private log |

`Mem::Center(i, col, row)` is called once per step, before that step's reads. A speculation stops there when it
reaches `stepCap` steps.

`ErodeSpec`'s overlay relies on how a droplet stores: two cells per step, cur then best, and best becomes the next
step's cur. So the only stored cells a droplet can read back are earlier cells of its own path.

- The previous cell is read back on every step. It is kept in registers and picked with a cmov.
- Every older path cell is marked in a per-worker bitmap of the grid when it stops being the previous cell. A marked
  cell is looked up by searching the log backwards.

### 1.2 The pipeline: one ParallelFor call, W+1 roles

Role 0 is the **commit**; roles 1..W are **speculators**. Speculators and commit share only the heightmap, a ring of
256 droplet slots and three atomics: `committed`, `specNext` and `finished`.

- **The commit, for each droplet j:**
  1. It draws the start cells up to j + `lookahead` with the stage's `Random`, two `Rand(dim-1)` per droplet in
     droplet order. These are the sequential body's draws. Each start cell is published in a slot as FREE(j) with a
     release store.
  2. It takes slot j:
     - FREE: the commit runs j itself (TAKEN, `ErodeLive`).
     - SPEC: it waits for the speculator.
     - DONE: it **validates**, then applies the logged stores. If validation fails, it re-runs j live.
     - FAIL (stepCap reached): it re-runs j live.
  3. It publishes `committed = j+1` with a release store.
- **A speculator:**
  1. It claims j with `fetch_add` on `specNext`. It waits on the slot's own cache line until j is drawn, then moves the
     slot FREE -> SPEC with a CAS. If the commit took j first, the speculator catches up to `committed`.
  2. It loads `g0 = committed` (acquire) and runs j through `ErodeSpec`.
  3. It writes into the slot: the step centres, the box of those centres, and one entry per stored cell holding the
     last value stored there. It compacts the log by walking it backwards, which also empties the bitmap.
  4. It publishes DONE or FAIL with a release store.
- **`gap` (default 6).** Speculators leave droplets closer than `gap` to `committed` to the commit. With few
  speculators the commit would otherwise idle while it waits for them. With 3 workers it now runs about 12-15% of the
  droplets itself, and the rest of its time goes to validating.

### 1.3 Validation and the footprint

Each committed droplet k leaves a commit-local **footprint**:

- the box of its centres grown by one cell, which contains every cell k read or stored;
- its stored cells, either the slot's compacted log or `ErodeLive`'s log;
- or the mark "unbounded", when a live run stored more than its log holds or the droplet was run SOLO.

Speculation j is valid iff no k with g0 <= k < j stored a cell in the 3x3 around one of j's centres. The check runs
in two stages:

1. Box against box. This is one branch per k, because random boxes make each of the four comparisons a coin flip.
2. Only when the boxes overlap: j's read cells are marked in a commit-local byte grid and k's stores are looked up in
   it. An unbounded k fails j at this point.

The ring stays consistent because `lookahead` < 256/2. A speculator claims j only after j is drawn, so g0 >= j -
lookahead. The slots and footprints of g0..j-1 are therefore not redrawn before j is validated.

### 1.4 Why it is exact

- **A valid j.** The validation runs on the commit thread after droplets 0..j-1, so it sees every footprint exactly.
  - Every store to a cell j read came from a droplet before g0. Droplets >= j had not run, and g0..j-1 stored none of
    those cells.
  - The release of `committed` after droplet g0-1 and the speculator's acquire of g0 order all those stores before j's
    reads. No later store exists in the cells' modification order, so each of j's heightmap reads returned the
    committed value.
  - j's reads of cells it stored itself come from its overlay, which holds what j stored. Run live, j reads back the
    same values.
  - The same template and the same inputs give the same floats. ParallelFor copies the caller's MXCSR into its
    workers, and GCC contracts nothing (no FMA target, EROSION.md §1).
  - Writing each stored cell with j's last value there leaves the heightmap as the sequential body's stores of
    droplet j leave it. Between two stores of droplet j to a cell, only j reads that cell.
- **An invalid j, an overflow, or an unclaimed j.** It runs live on the commit thread after 0..j-1, against the
  committed heightmap. This is literally the sequential droplet j.
- **The Random.** Only the commit draws, in droplet order, exactly 2 x iters times. The final state is identical.
- **Who runs what never changes the result.** Speculation only reads the heightmap and writes its own slot and
  overlay. Timing, thread count, `lookahead`, `gap`, `stepCap` and SOLO change only who runs a droplet, never what it
  computes.

### 1.5 SOLO: the conflict-heavy fallback

On fields where most droplets conflict, speculation costs more than it saves: the commit validates long footprints
and then re-runs the droplets anyway. The bench ramp is the example: 491-step droplets, 60% conflicts. That ramp ran
at 0.41x before this fallback.

- Per block of 1,024 droplets, the commit counts failed validations.
- Above 30%, it runs alone (`solo` flag; speculators claim nothing) for `soloLen` droplets, then probes speculation
  for one block.
- `soloLen` doubles while probes fail, up to 65,536, and resets when a probe passes.

The ramp now runs at 1.10-1.15x of the pre-EROSION body, about 13% behind the sequential body. Realistic fields never
enter SOLO (0.0% in every noise and real-map run).

### 1.6 Memory

Per run (1025^2 grid, 7 workers), allocated once and freed at the end:

| item | size |
|---|---|
| slot logs: 256 x (1,024 centres + 2,048 stores) | 5 MB |
| per-speculator scratch log | 16 KB each |
| per-speculator bitmap | 131 KB each |
| the commit's mark grid | 1 MB |

Nothing is allocated per droplet.

### 1.7 Knobs (`WgErosionTuning`, `Game/worldgen_erosion_par.h`, for the tests and the bench only)

| knob | default | role |
|---|---|---|
| `lookahead` | 16 | the speculation window |
| `stepCap` | 1,024 | real droplets run 19-36 steps |
| `minParallel` | 16,384 | below it, the sequential body |
| `speculators` | 0 | 0 = every pool worker |
| `gap` | 6 | droplets nearest the commit left to it |

The worker count comes only from the pool: `Cpu::ParallelWorkers(physical)`, the roster. There is no flag and no
settings key.

### 1.8 A fix in `System::ParallelFor` (WGPAR's pool, `src/engine/system/parallel_for.cpp`)

A re-run of the unit after the merge hung: the caller sat in `done.wait` while every worker was parked. The cause is
in the pool:

- A pool rebuilt by `SetWorkers`, or after `Shutdown`, keeps its old `epoch`. Each new thread started with
  `seen = 0`, so it woke at once for the finished job.
- When that wake overlapped the next call's publication, the thread claimed a chunk of the new job and checked in
  after the caller had reset `checkedIn`. It then woke again for the new epoch and checked in a second time.
- The count could step past `workers`. The caller then waits forever (`checkedIn == workers` is never true), or it
  returns while a chunk still runs.

**Fix:** `Start` records the epoch under the pool mutex while `callLock` holds off every call, and each thread
starts from it.

**The game was never exposed.** Its pool is built once, at epoch 0, and never rebuilt. Only the tests and the bench
rebuild pools.

**Regression test:** `wg_parallel_for_rebuilt_pool_waits_for_the_next_job`. It runs 400 rounds of rebuild, wait
0-300 us, call, and checks that every chunk is done by the return. The old pool, run as a standalone driver with the
same loop, hung inside 60 s; the fixed one passes 400/400.

## 2. Proof of identity

- **`tests/unit/worldgen_erosion_par_test.cpp`** (6 tests, in `unit`). It compares `wg_f390_erosion_tuned` against
  `wg_f390_erosion_sequential`: the heightmap and the whole `Random` state, byte for byte.
  - Configurations: 1 / 2 / 4 / 7 workers, crossed with the game tuning, gap 0, gap = lookahead, lookahead 1 / 4 /
    24 / 127 and step cap 3.
  - Inputs: EROSION's (value noise with sea, four steep ramps plus a gentle one, tie fields, ±0 / denormals / ±inf /
    NaN salts).
  - The stress test:
    - dim 2 / 3 / 4 grids, where every droplet overlaps every other;
    - a bowl that drains every droplet to the middle;
    - step cap 1 / 2, so every speculation of a moving droplet overflows;
    - step cap 1 on a ramp, so the commit's live logs overflow and footprints become unbounded;
    - six randomised (dim, lookahead, cap) rounds.
  - Counters prove that each slow path ran: conflicts > 1,000, overflows > 1,000, cell checks > 0, SOLO > 0.
  - The no-op arms are covered: 0 / negative iterations, and fewer droplets than `minParallel`.
  - **Mutation check:** accepting every speculation without validating it fails 39 checks across 5 tests.
- **`tests/unit/worldgen_erosion_test.cpp`** now runs both `wg_f390_erosion` (parallel when the pool has workers) and
  `wg_f390_erosion_sequential` against the pre-EROSION reference.
- **The bench** memcmp's the heightmap and the `Random` state of every timed run against the pre-EROSION body.
  Everything below was IDENTICAL.
- **Not yet run (coordinator):** the `[WorldHash]` kit (3 seeds x 3 sizes, old exe vs new exe) and the census.

## 3. Bench

Command: `sh tools/erosion_bench/bench.sh 3 0 3 7`. Perf flags, best of 3. 9800X3D, 8 physical cores. Machine CLEAN:
no other build ran. `ref` is the pre-EROSION body; workers 0 is the tree's sequential body (EROSION).

| field | droplets | steps/droplet | ref | workers 0 | **workers 3** | **workers 7** |
|---|---|---|---|---|---|---|
| value noise 1024^2 (large map) | 16M | 36.3 | 6,104 / 6,115 ms | 5,292 / 5,281 (1.15x) | **2,971 / 2,972 (2.05x)** | **2,607 / 2,669 (2.29-2.34x)** |
| value noise 768^2 | 8M | 31.6 | 2,643 / 2,632 | 2,270 / 2,263 (1.16x) | 1,297 / 1,292 (2.04x) | 1,111 / 1,101 (2.38x) |
| real 512^2 heightmap dump | 4M | 19.2 | 749 / 749 | 638 / 637 (1.17x) | 415 / 408 (1.81-1.83x) | 359 / 358 (2.09x) |
| ramp 1024^2 (pathological) | 200k | 491 | 520 / 516 | 407 / 405 (1.28x) | 458 / 448 (1.13x, SOLO 95%) | 474 / 463 (1.10x, SOLO 95%) |

Each cell gives two seeds. The speedup in brackets is against `ref`. Against the tree's sequential body, the large
map gains 1.78x with 3 workers and about 2.0x with 7.

**Expected in game:** `wg.erosion` about 3.0 s -> about **1.5-1.7 s** (7 workers) / about **1.7 s** (3 workers, a
4-core PC).

### Conflict rates (default tuning: lookahead 16, gap 6)

| field | workers | validated | conflict (re-run) | commit ran it (gap) | cell check |
|---|---|---|---|---|---|
| noise 1024 | 3 | 83% | 4.6-4.8% | 12% | 5.4% |
| noise 1024 | 7 | 92% | 7.7-7.8% | 0% | 9.0% |
| real 512 dump | 3 | 81% | 4.2% | 14.5% | 5.2% |
| real 512 dump | 7 | 92% | 8.1-8.3% | 0% | 10% |

With 7 workers the speculators run ahead, so the window equals `lookahead`. The measured rates match EROSION's census
(B=8: 2%, B=32: 7.5-8%).

### What bounds it (rdtsc profile of the commit and the speculators, 512 dump)

- **A speculation costs about 1,300 cycles for 19 steps, about 70 per step, against about 50 sequential.** Most of
  the extra comes from heightmap lines arriving from the core that last stored them, usually the commit's. The
  overlay is cheap: replacing the bitmap branch for the previous cell with a cmov changed nothing measurable.
- **With 3 workers the speculators are the bound.** The commit idled about 250 cycles per droplet waiting for them;
  that idle time is what `gap` gives back.
- **With 7 workers the commit is the bound, at about 300-400 cycles per droplet:**

  | part | cycles per droplet |
  |---|---|
  | validation | about 80-130 |
  | apply | about 90; lines owned by speculators' caches |
  | re-runs | about 100 (8% x a cold live run) |
  | slot handoff | the rest |

**Tried and dropped:**

| attempt | result |
|---|---|
| a per-cell version array (EROSION §5's plan) | 4 MB cold array, about 830 cycles per validation; slower than sequential |
| per-pair box tests with four branches | about 15 cycles per pair from mispredicts |
| prefetchw of the apply cells before validation | slower |
| spinning speculators on a shared `drawn` counter | replaced by waiting on the slot's own line |
| lookahead 24 | 12% conflicts with 7 workers |
| lookahead 8 | the commit waits |

The fork-join batch design in the brief (draw a batch, speculate it against a frozen heightmap, commit) was not built.
Against a frozen snapshot a droplet conflicts with every earlier droplet of its batch: 47% at B=512 (EROSION §5).
Batches small enough to keep conflicts low (B <= 32, about 12 us of work) cannot amortise a ParallelFor wake of 10-50
us. The streaming ring keeps the window at about `lookahead` droplets with one wake per stage.

## 4. Files

| file | change |
|---|---|
| `src/game/Game/worldgen_f390_erosion.cpp` | the droplet template, `wg_f390_erosion_sequential`, the parallel job, `wg_f390_erosion` (stage entry). `wg_f390_carve` / `wg_f390_smooth` are untouched. |
| `src/game/Game/worldgen_erosion_par.h` | new: the tuning, the counters, `wg_f390_erosion_tuned` and `wg_f390_erosion_sequential` |
| `tests/unit/worldgen_erosion_par_test.cpp` | new |
| `tests/unit/worldgen_erosion_test.cpp` | also checks the sequential body |
| `src/engine/system/parallel_for.cpp` | the rebuilt-pool check-in race (§1.8) |
| `tests/unit/wg_parallel_test.cpp` | its regression test |
| `tools/erosion_bench/` | worker counts, counters, `L=` / `S=` / `G=` and a case filter |

## 5. Static gate (worktree)

| check | result |
|---|---|
| `tools/build.sh build debug --jobs 2` | OK, 0 warnings in the touched files (109 pre-existing) |
| `tools/build.sh build perf --jobs 2` | OK, 0 warnings in the touched files (183 pre-existing) |
| `tools/build.sh unit debug --jobs 2` | 1,207 tests, 0 failures (after merging main and the pool fix) |
| `python tools/castmetric.py src` | total 2 |
| `python tools/abi_audit.py` | OK |
| `tools/acceptance/acceptance.sh` | 0 lines off target |

## 6. Open items

- **The coordinator.** Run the `[WorldHash]` identity kit (3 seeds x 3 sizes), the gate and the `[LoadTime]` census
  for `wg.erosion`, on P4 and unrestricted masks.
- **The real large-map field.** The bench's large-map row is value noise. A dump of the real pre-erosion 1025^2 field
  (taken at the `wg.erosion` stamp in a debug run) would confirm the 36-step / 8% conflict figures. The real 513^2
  dump behaves like noise.
- **Headroom, if wanted.** The 7-worker ceiling is the commit's cross-core traffic: the slot handoff, applying lines
  the speculators hold, and cold re-runs. Speculators applying their own validated stores, with an `applied` prefix
  counter in place of `committed`, would move the apply off the commit. It is a larger protocol change; it was not
  attempted.
- **Retail's endless droplet** (finite heights above about 2^27, EROSION §4) hangs the parallel path exactly where it
  hangs the sequential one. The speculation gives up at `stepCap`, and the commit's live re-run loops just as cc080
  does. Unreachable in worldgen.
