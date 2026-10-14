# EROSION -- the droplet erosion stage, exact and cheaper (loading-speed session, LOADPLAN §3)

Block EROSION. `wg_f390_erosion` (retail `0x1400cc080`, `src/game/Game/worldgen_f390_erosion.cpp`) was
3,582 ms of the new game on the perf exe (large map, LOADPLAN §2.1). Ruling 2 applies: the output must be
byte-identical, so the same seed gives the same heightfield, world and save.

## 1. Where the time goes

- **Work size.** `Game/Terrain/{Valleys,Mountains}.rsc` set `_erosion` to 4M / 8M / 16M droplets for map
  sizes 256 / 384 / 512 (grids 512^2 / 768^2 / 1024^2). The bench measures 19-36 steps per droplet on
  realistic fields, so the large map runs 0.3-0.6 G steps at about 10 ns each.
- **No libm, no allocation.** The stage calls no `pow`, `sqrt` or `exp`; those belong to the stamp and carve
  brushes (WGPAR). It allocates nothing. There is no FMA: GCC targets plain x86-64 here, so no
  `-march` / `-mfma` flag applies and nothing is contracted. `Random::Rand` is out of line and costs about 2%.
- **What the perf build's hot loop does** (`objdump` / `-S` of the TU at the perf flags):
  - 9 data-dependent `comiss` / `ja` branches per step, one per neighbour.
  - `hfv->heightmap.data` is reloaded at the top of every step and again after the first store, because
    `-fno-strict-aliasing` lets a float store alias the pointer.
  - The cell's height is reloaded from memory, which is a store-to-load forward of the value the previous
    step just wrote.
  - Loop state is spilled to the stack.
- **The real limit is the dependency chain plus branch prediction.** The bench separates the two:
  - On a ramp, where every branch predicts, the original runs at **5.2 ns/step** (about 25 cycles). That is
    the chain: the compares, the transfer arithmetic `t -> newBest`, and the store/forward into the next
    step's load.
  - On noise and on a real heightmap it runs at **9.8-10.9 ns/step**. The difference is branch mispredicts
    on the choice of "which neighbour is lowest", which carries about 3 bits of entropy per step.

## 2. What changed (lines: `wg_f390_erosion` only, `worldgen_f390_erosion.cpp`; the brushes are untouched)

The scan keeps the same branches, the same float operations in the same order on the same cells, and the
same `Random` draws in the same order. There are four changes:

1. **The heightmap base is read once.** cc080 re-reads `hf+0x08`, but nothing in this stage resizes or moves
   the heightmap, so every re-read returns the same pointer.
2. **The cell's height is carried in a register.** After a move, `hm[bestIdx]` holds exactly `newBest`: it
   was the step's last store, to a different cell than its first store. The next step's load would return
   those same bits. This removes a store-to-load forward from the chain.
3. **The self compare is dropped.** At that point `hm[curIdx] == hCur` and `bestH <= hCur`, and `h < h` is
   false for every float, NaN included.
4. **Neighbour indices are written as `curIdx +- edge +- 1`.** These are the same int values. The next index
   is still computed inside the branch arms, so a predicted branch hands the next step its addresses
   without waiting on the compares.

**Variants measured and rejected** (all were IDENTICAL; `tools/erosion_bench`):

| variant | noise 1024 | ramp | verdict |
|---|---|---|---|
| branch-free sequential scan (cmov / minss) | 1.11x | -- | GCC still branched the last compare |
| pairwise tree, NaN sanitised | 0.73x | -- | GCC emitted `ucomiss` / `jp` branches |
| pairs from +inf, merged (exact, NaN-free) | 1.15-1.32x | **0.45x** | branch-free means full latency (12 ns/step) where the path is predictable |
| integer-key tree (cmov only) | 0.95x | -- | key transform plus 3-wide cmovs cost more than they save |
| `switch` on the direction (predictable addresses) | 1.03x | -- | the direction is not predictable on noise |
| previous cell merged last from a register | 0.92x | -- | |
| next droplet drawn early + prefetch (same draw order) | +-0 | -- | the 4 MB field sits in L2/L3; not kept |

The real pre-erosion field could be noise-like or ramp-like. The kept form is the only one that wins on both.

## 3. Bench (perf flags `-O2 -fno-strict-aliasing -fno-lifetime-dse`, best of 3, 9800X3D; machine shared with other agents' builds, ratios are paired)

`sh tools/erosion_bench/bench.sh` times the tree's body against the pre-EROSION body (copied into
`bench.cpp`) and memcmp's the heightmap and the `Random` state after each run.

| field | droplets | steps/droplet | before | after | speedup |
|---|---|---|---|---|---|
| value noise 1024^2 (large map) | 16M | 36.3 | 6,211 ms (10.7 ns/step) | 5,188 ms (8.9) | **1.20x** |
| value noise 768^2 | 8M | 31.6 | 2,662 ms | 2,242 ms | 1.19x |
| real 512^2 heightmap (`heightmap_dump.raw`) | 4M | 19.2 | 749 ms | 624 ms | 1.20x |
| ramp 1024^2 (every branch predicted) | 200k | 491 | 514 ms (5.2 ns/step) | 397 ms (4.0) | 1.29x |

Every run printed IDENTICAL. Expected in game: `wg.erosion` 3.58 s -> about **3.0 s**. The coordinator's
census will give the real figure.

**The single-thread floor.** The arithmetic chain per step (transfer -> next height -> compares) is about
4 ns/step when every branch predicts. On unpredictable terrain each step pays about one mispredict
(about 5 ns). Exact single-thread work cannot remove either: the next cell depends on the compares of
heights the previous step just wrote. Realistically the floor is about 1.2-1.3x of the original. The
rest has to come from parallelism (§5).

## 4. Proof of identity

- **`tests/unit/worldgen_erosion_test.cpp`** (5 tests, in `unit`). It runs the tree's body and the
  pre-EROSION body (kept in the test only) on the same heightfield and seed, and compares the heightmap and
  the whole `Random` state byte for byte. The `Random` state proves the draws match in number and order.
  - Inputs: value noise with sea; four ramps (every direction, plus the 0.8 clamp); 4-level tie fields
    (first-neighbour tie-break, plateaus); fields salted with +-0, denormals, +-inf, NaN (both signs,
    quiet and signalling).
  - Edge cases: dim 2; no size row; 0 and negative iterations.
  - Mutation check: a one-line tie-break mutation of the body fails 35 checks.
  - The unit links `worldgen_f390_erosion.cpp`. `link_stubs.cpp` gains host `Random(uint32_t)` /
    `Rand(uint32_t)` stand-ins: an xorshift over the same state words, because `value_types.cpp` brings
    `System::Timer` and collides with the existing `Random()` stub. It also gains a `kSpec_WgPathVec` row
    stub.
- **Retail fact found on the way.** Finite heights above about 2^27 make cc080 itself loop forever. There a
  1-ulp gap is already > 12.8, so the transfer clamps to 0.8 and two cells trade the gap back and forth.
  Both bodies hang identically. Worldgen heights are tens of units, so this is not reachable; the test
  does not salt such values.
- **World identity:** the coordinator's `[WorldHash]` kit (3 seeds x 3 sizes, old exe vs new exe) is the
  check that must pass before merge.

## 5. Research: exact parallel droplets (not implemented)

Measured with `sh tools/erosion_bench/bench.sh 1 census` (the reference, instrumented). Droplet d
*conflicts* in a window of B consecutive droplets when it reads a cell that an earlier droplet in the
same window wrote:

| field | distinct cells read / written per droplet | conflict B=8 | B=32 | B=128 | B=512 |
|---|---|---|---|---|---|
| noise 1024^2 | 165 / 36 | 1.9% | 7.5% | 22% | 47% |
| real 512^2 | 89 / 20 | 2.0% | 8.1% | 25% | 52% |
| ramp | 2322 / 492 | 32% | 50% | 69% | 87% |

Drainage convergence does not blow these up on the real map: about 2% with 8 in flight.

**An exact scheme.** It has three parts:

- **Producer.** The commit thread draws the start cells in order, so the RNG sequence is unchanged.
- **Speculating workers.** Each worker runs droplets against the live heightmap through a private overlay.
  The overlay holds the droplet's own writes, which its later steps read back. Each worker logs the
  droplet's read set and its final write values.
- **Commit thread.** It processes droplets strictly in order and keeps `version[cell]` = the index of the
  last committed writer. Droplet j is valid iff no cell it read has a version written after j's
  speculation snapshot. If valid, its logged writes are applied (value, then version). If not, j is re-run
  live on the commit thread.
- **Why it is exact.** A torn or late read is always caught, because the value is stored before the version
  and validation runs after all earlier droplets have committed. A valid droplet's writes are a subset of
  its reads, so they cannot collide with an earlier committed write.

**Estimated ceiling.** The commit thread does about 165 version loads + 36 stores per droplet (about
60-90 ns), against about 360 ns to run a droplet. Re-runs at 2-4% add about 10 ns. The commit thread
therefore caps the speedup at about **4x**. The workers pay about 1.5x for overlay and logging, so 7
workers give about 4.5x of throughput. Realistic: **2.5-3.5x on 8 physical**, so `wg.erosion` about
3.0 s -> 0.9-1.2 s. With one core it runs inline.

**Why "truncate the batch at the first conflict" fails.** Without truncation, conflicts cascade. With it,
rounds must be large to amortise the fork-join wake, and at B=128-512, 22-52% of droplets conflict. The
valid prefix is then only about 25 droplets per round, which is slower than serial.

**Shape with `System::ParallelFor`** (WGPAR's helper, on main since `7c5d3174b`):

- One `ParallelFor` call per stage, with one chunk per role: chunk 0 is producer + commit, the rest are
  speculating workers. Nothing is called per droplet.
- Handoff goes through lock-free rings of droplet slots.

**Cost and risk.** It needs about 400-600 lines: rings, overlay, logs, version array (4 MB), and
termination. The hard part is proving exactness under concurrency: relaxed atomics on the heightmap,
store/version ordering, and a re-run path whose result must equal cc080's sequential body exactly. The
unit comparison above (plus a randomised stress over thread counts) is the proof tool. Not implemented in
this block: the helper was not on main when the research was done. If wanted, it should be its own
block, with the census rerun on a real pre-erosion large-map field. That field can be dumped from a
debug run at the `wg.erosion` stamp.

## 6. Open items

- The coordinator runs the `[WorldHash]` identity kit (seeds x sizes) and the `[LoadTime]` census for
  `wg.erosion`.
- Parallel erosion (§5): a decision for the user / coordinator.
