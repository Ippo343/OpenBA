# HITCH_PATHGRAPH hand-in (2026-10-18)

Branch `pathgraph`, based on `hitch` `f2364f3af`. The CitizenList spike every 300 simulated seconds
(frames 721, 1081, 1441, 1802, 2161 at 50x; the "1800-frame spike" at 10x). **Result: faithful, no
extension.** The spike was not retail's work. It came from two port divergences in the PathGraph drain,
and fixing them to match retail removes it. A third change is a faithful container speed-up, proved by a
shadow. Not merged.

| Commit | What |
|---|---|
| `295f02912` diag | `[Stress::pathgraph]` line per drain: pairs, ReassignWorkers / AssignCitizens ms, cost-query counts (`StressHarness_PathGraphDrain`, `g_pathGraphCost`) |
| `a988bbb69` behaviour | AssignCitizens reads its costs from the PathGraph region-pair tree: retail `0x1400d6060` ported as `PathGraph::RegionPairCost`; the home cell is retail's inline node-0 read; retail bug 6 |
| `696a2924c` behaviour | BuildPathGraph collects every residence (Step 1's append tested the capacity and stopped at 5) |
| `f393998c1` perf | AssignCitizens' per-need cost trees as sorted vectors with lazy removal; the COMPPROF=4 shadow replays the old loop |
| (this commit) docs | this hand-in, `perf/README.md` note |

Retail was read with objdump of `Application-x64.exe`. The Ghidra project was locked by another session
(lock file from 00:13, no MCP bridge up), so no Ghidra names were changed.

## 1. What the spike does, in retail

`CitizenBehavior::Update` (`0x14008c2b0`) runs the PathGraph every 300 s:

1. **BuildPathGraph** (`0x1400d70c0`), one tick.
   - It collects the anchor cell of every residence (`town+0x20`).
   - It pairs each workplace in the profession tree (`town+0x100`) with every residence.
   - It keeps one pair per region pair `(max, min)`, deduplicated through a placeholder in the tree at
     `PathGraph+0x18`.
2. **ProcessPaths** (`0x1400d6f00`), 7 pairs per tick.
   - Each pair's cost is `Map::PathDistance(a, b, 0)`, with the Euclidean distance when it is under 2.
   - The cost is stored under the region pair.
3. **The drain**, in the tick where the last pair is done:
   - **ReassignWorkers** (`0x1400d62a0`): the single-household residence consolidation. It costs
     occupied x empty residences x qualifying occupants `CalcDistance` calls.
   - **AssignCitizens** (`0x1400d43b0`): the employment allocator.
     - For every idle citizen x staffed workplace it asks `0x1400d6060` for the commute.
     - `0x1400d6060` answers from the tree at `PathGraph+0x18`: retail never searches a path here.
     - It then runs the priority loop.

Retail does the whole drain in one tick, as the port does. Its AssignCitizens is tree lookups.

## 2. What the port did instead

- **AssignCitizens priced every commute with an exact PathDistance.** The port's `ComputeCost` was
  described as "sub_6060 minus its region-pair cache (a pure optimization that returns the same value)".
  - It is not the same value. A same-region pair's Euclidean fallback belongs to the pair ProcessPaths
    measured, not to the citizen.
  - At 800 citizens this was about 26k region searches in one tick: **9-21 ms**.
- **BuildPathGraph collected 5 residences of 278.**
  - Retail's Step 1 (`0x1400d7162`-`0x1400d71f9`) is `Array<int>::Add`'s inlined `SetSize(count + 1)`.
    Its "size unchanged" test compares the new size with the count (`[rbp-0x61]`), so it never skips.
  - The port compared the new size with the capacity. From the sixth residence on, new size 6 equals
    capacity 6, and nothing more was appended.
  - So the tree held 168 region pairs where retail holds 9902. With the first fix alone, AssignCitizens
    missed the tree about 3.3k times per drain.
- **The home cell drew a random number.**
  - Retail reads the home's InteractComponent node 0 inline (`0x1400d5390`-`0x1400d5477`).
  - The port called `EntityAnchorCell` (`0x140087fb0`), whose multi-node arm draws `Rand` and
    discards it (retail passes node 0 to `0x140122e20` at `0x140088064` too).
  - Now `HomeNodeCell` does the inline read, with no draw.

**Retail bug kept (`docs/retail_bugs.md` #6).** On a tree miss, `0x1400d6060` computes the cost
(`0x140008830`) and stores it, but returns 0. The returned cost is R15D, which only the found arm writes.
With the residence list fixed, the stress town has **0 misses** per drain.

## 3. How the cost scales

- **Rebuild pairs.** The count is the distinct region pairs of workplaces x residences. The stress town
  has 58 workplaces x 278 residences, which gives 9902 pairs. At 7 per tick they take about 1415 ticks,
  about 28 frames at 50x and about 47 s at 1x.
  - Each of those frames carries about 350 `PathDistance` calls, about **+3.5 ms** of CitizenList at 50x.
  - That is retail's work, spread as retail spreads it. A normal town has at most a few hundred pairs.
- **Drain, AssignCitizens.** It costs idle citizens x staffed workplaces tree lookups (25.7k at 803 x 54),
  plus assignments x needs O(1) checks.
- **Drain, ReassignWorkers.** It costs occupied x empty single-household residences x occupants
  `CalcDistance` calls. This is faithful and unchanged: **2.1-3.8 ms**.

## 4. The container speed-up (`f393998c1`)

- **What changed.** The port modelled retail's per-need `{cost, citizen}` AVL trees as `std::multiset`
  and dropped an assigned citizen from every need with a linear scan: 3.7 ms per drain.
  - The order `(cost, citizen)` is strict, so one sort after Phase 3 gives the same in-order sequence.
  - An assigned citizen leaves every need at once. So a need's best is its first candidate still in the
    idle set, and it changes only when the citizen was that need's head, which is exactly when the old
    code re-scored.
- **Proof.** At COMPPROF=4 the old multiset loop runs as a shadow (`ShadowAssignSequence`), and the two
  `(need, citizen)` selection sequences are compared (`StressHarness_Verify "pathgraph_assign_seq"`).

## 5. Proof and measurements

### COMPPROF=4 leg

`perf/results/20261018_022123_pathgraph_verify` (50x, 2400 frames):

| check | result |
|---|---|
| `pathgraph_assign_seq` | 6 drains, 0 mismatches |
| every other shadow (PathDistance 129932, FrustumCull, TickList, ...) | 0 mismatches |
| `differ` (cached answer != the old exact per-cell cost) | 16 / 47 / 56 / 60 / 20 of ~26k per drain |

`differ` says how far the old port was from retail. The two fixes are `behaviour:` commits because they
change the sim back to retail's. Identity with the old port is neither possible nor wanted.

### The drain

From the `[Stress::pathgraph]` lines:

| leg | pairs | cost calls | tree misses | AssignCitizens ms | ReassignWorkers ms |
|---|---|---|---|---|---|
| before (`014520`) | 168 | 25.7k-27.9k | n/a | 9.3 / 18.5 / 20.1 / 21.3 / 15.9 | 3.4 / 3.5 / 2.5 / 2.4 / 2.2 |
| RegionPairCost only (`015016`) | 168 | 25.7k-27.2k | 3.2k-3.6k | 7.0 / 10.9 / 10.8 / 10.3 / 12.3 | 3.4 / 2.8 / 1.4 / 1.3 / 2.4 |
| all three (`022400`) | 9902 | 16.7k-27.9k | 0 | 1.7 / 1.8 / 1.7 / 1.7 / 1.3 | 3.8 / 3.3 / 3.1 / 2.1 / 3.6 |

### The hitch rows at the 360-frame cadence

Excess over the rolling median, in ms. The drain moved from frame 721 to 749 because the rebuild now has
9902 pairs to drain, not 168.

| leg | machine | 721 / 749 | 1081 / 1109 | 1441 / 1469 | 1802 / 1829 | 2161 / 2188 |
|---|---|---|---|---|---|---|
| before `20261018_014520_pathgraph_before` | BUSY | 14.75 | 29.84 | 24.09 | 25.30 | 23.46 |
| before `20261018_022627_pathgraph_ab_before` (1800 frames) | BUSY | 15.66 | 30.73 | 24.19 | 19.87 | - |
| after `20261018_022400_pathgraph_after3` | BUSY | none | none | none | none | none |
| after `20261018_023121_pathgraph_ab_after` (1800 frames) | CLEAN | none | none | none | - | - |

"None" means no ph_citizenlist row at all: the drain frames are under the 7.14 ms criterion.

The clean after-leg still has one row at frame 1081, 13.14 ms, attributed to **ph_entitylist**.
- Its phase deltas are `ent+9.3 cit+3.5`.
- The EntityList burst at the 300 s mark is in the before-legs too (`ent+6.2` at 1081).
- The `cit+3.5` is the first frame of ProcessPaths' restored spread (section 3).

### update_ms_per_iter

Rows 1-3 only. Row 4 is the stress town starving at 50x in every leg (citizens 112-499), so the
whole-run mean `stress_report` prints (+5.5% for `after3`) is not a valid comparison.

| leg | machine | rows 1-3 mean ms/iter |
|---|---|---|
| before `014520` | BUSY | 0.1350 |
| before `022627` | BUSY | 0.1377 |
| RegionPairCost only `015016` | CLEAN | 0.1343 |
| after `022400` | BUSY | 0.1377 |
| after `023121` | CLEAN | 0.1333 |

Both clean after-legs are below both before-legs, so there is no regression above the noise. The machine
was shared with two other agents all evening, so no fully clean pair exists.

`tools/build.sh unit perf`: 1165 tests, 0 failures, 3 skipped (no kit fonts).

## 6. Open

- **The resave gate and `tools/build.sh run` were not run here.** Those are the coordinator's. The fixes
  change which citizens get seated, which homes are consolidated, and one RNG draw per multi-node home.
- **ReassignWorkers is now the larger half of the drain**, at 2-4 ms of faithful retail work. A cache
  across its pair loop is constrained, because `EntityAnchorCell`'s random-number draw per call has to be
  kept.
- **The 10x and 20x legs were not run.** The cadence is sim-time, so the same drain is every 1800 / 900
  frames.
