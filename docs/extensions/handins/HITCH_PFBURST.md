# HITCH_PFBURST -- the EntityList PathDistance bursts, and `ext.spread_warmth` (class B, default OFF)

Block PFBURST, 2026-10-18. Branch `pfburst` from `hitch` (`dc0dad510`: the hitch recorder and the food
top-up). Part of the HITCH hunt (perf/README.md "Hitches"): EntityList-phase frames carrying 9-25 ms of
main-thread `Map::PathDistance` in the fed speed-50 stress town.

**Short version.**
- **The caller** is the cold citizens' warm-house scan: `CitizenAIComponent::State_ElderTransition`
  (0x14010d2c0, the warmth state 1 / 0xf; "ElderTransition" is Ghidra's misnomer) runs
  `CitizenBehavior::FindElderResidence` (0x14008ec70), which calls `Map::PathDistance` from EVERY
  residence to the cold citizen's cell.
- **The trigger** is a cold wave. When winter takes the town's citizens over their description's `_fec`
  coldness threshold, dozens of them cross it inside one frame's 50 sim ticks. A burst frame ran about
  44 scans of 278 residences: ~12,400 PathDistance calls, 4,700-5,500 of them region A* searches.
- **Retail does the same.** The port is faithful, and the retail disassembly shows the same loop (§2).
  It only shows at high speed, where one frame holds 50 ticks of decisions.
- **No faithful fix reaches the target** (§3). Every search must run to keep identical results, and
  they already cost 2-3 µs each on PATHDIST's bucket queue.
- **The fix is the extension `ext.spread_warmth`**: class B, default off, formatVersion 1, pseudo-mod
  `ext:spread_warmth`. A per-frame budget of 512 region searches or 4096 candidates. Past it, a scan
  is deferred to retail's own failure tail, the 3.75 game-second cooldown.
- **Measured** (§5). Faithful: three cold-wave rows of 13.5-17 ms PathDistance and 15-18 ms excess.
  Extension, two clean runs: no cold-wave row at all except one of 6.3 ms PathDistance and 7.19 ms
  excess, 0.05 ms over the criterion. `update_ms_per_iter` is no worse in any row.

## 1. How the caller was found

A temporary per-call probe, not committed. In `Map::PathDistance` a scope object took
`__builtin_return_address(0)` and the kind: no search, pair-cache hit, or miss. On every miss it captured a
12-deep `RtlCaptureStackBackTrace`. The harness aggregated per Update and printed `[Stress::pdburst]` for an
Update whose PathDistance total passed 2 ms. Stacks were symbolized with `addr2line -i` on the perf exe.

Faithful run `perf/results/20261018_021213_pfburst_probe`, frame 2743 (EntityList phase 24.0 ms against a
median of 7.1):

| caller | none | hit | miss | ms |
|---|---|---|---|---|
| `CellCost` <- `FindElderResidence` <- `State_ElderTransition` <- `ValidateAndEnterState` <- `TransitionState` <- `Decide` <- `EntityList::UpdateImpl` | 146 | 7,656 | 4,708 | 13.31 |
| the six other callers together (Leisure, FindBestTarget, FindAdultTarget, ...) | 339 | 361 | 26 | 0.10 |

The same stack carried every EntityList burst of that run (frames 2742-2744, 7617-7618, 11595-11596) and of
`20261018_030110_pfburst_faithful_probe2`, which had frames 3091, 7064, 11838 and 11839:

| frame | hit | miss | region expansions | ms |
|---|---|---|---|---|
| 3091 | 7,080 | 5,475 | 390,585 | 16.5 |
| 11839 | 9,579 | 4,537 | 591,247 | 23.0 |

The probe's stack capture adds about 1 µs per miss to those ms.

**Not this block's.** The probe also shows the CitizenList-phase rebuild frames, every 360 frames in that
run. There `PathGraph::AssignCitizens` -> `ComputeCost` (`citizen_assign_citizens.cpp:157`) issues about
14,000 misses, 27 ms, because it passes `useCache = 0`. Sent to the `pathgraph` agent, who owns that frame.

## 2. Retail's behaviour, and how the burst scales

- **The scan.** The retail disassembly of 0x14008ec70 makes these calls per residence, in order:
  - `InteractComponent::NodePosition` 0x140122e20;
  - `Map::PathDistance` 0x14009f810, with `r9b = 1`, so the pair cache is used;
  - the sqrtf fallback 0x140132010;
  - `ValidateCell` 0x1400a00f0;
  - the residence finder 0x140008090;
  - `CheckHeating` 0x14012ac30.

  0x14010d2c0 calls 0x14008ec70 once and then 0x1400bd790, the push. The port is that body.
- **Calls per burst frame** = the cold citizens whose coldness crossed `_fec` in that frame's ticks, times
  the residence count. Here that is 278 residences and up to ~45 citizens per frame.
- **Searches per burst frame** = the new region pairs among those calls: the cold citizens' regions against
  every residence's region, minus the pairs still in the 120 game-second pair cache.
- **Cost per search** is the region graph's span between the two regions: 50-150 expansions on average,
  up to ~1,100.
- **How it scales.** The burst grows with houses times the cold cohort, so with the town's size squared.
  It comes once per cold wave: about frames 2750-3100, 7050-7500 and 11600-11900 of a 12,000-frame run.
- **Why not every frame.** At speed 1 the same work is spread over 50 frames.

## 3. Why no faithful speed-up reaches the target

To be byte-identical, every one of the scan's PathDistance calls must run and store exactly as retail's:

- **Search order matters.** The region A*'s heuristic, a truncated centroid distance, is not admissible
  against truncated edge sums (PATHDIST §1). So a search's result depends on its own pop order, and one
  shared Dijkstra from the citizen's region would not reproduce it.
- **The cache is observable.** Each miss's `RegionPairCache::Store` stamps the entry, advances the ring,
  and may evict the oldest entry. Later lookups by every caller see those entries. Skipping or merging
  calls changes what later frames read.

What remains is the per-search constant: 2-3 µs on PATHDIST's bucket open set, 35-45 ns per expansion.
Reaching an excess under 7.14 ms on a 4,500-6,000-search frame needs about 3-4x on that already-lean
loop. The candidates were an h memo per goal region and a cached edge cost per region. Each costs an
invalidation hazard against `RegionGraph::Flush` and gains well under 2x, so (a) was not built.

## 4. The extension

- **Files.** `src/ext/spread_warmth/spread_warmth.{h,cpp}` hold the rules, the counters and the
  `ScanMeter`. The registry row is `spread_warmth`, "Spread warmth searches": `Tier::Extension`,
  `Class::B`, default off, formatVersion 1. The icon is `tools/ext_icons/hitch.py`, baked into
  `ext_sprites_data.h`.
- **Seams.** There are two, both marked `EXT SEAM ... NON-RETAIL`:
  - `GameUpdateLoop::Update` calls `SpreadWarmth::BeginFrame()` beside `WalkBatch::BeginFrame()`. It reads
    the row's locked value once per frame and resets the frame's budget.
  - `State_ElderTransition`, inside the cold branch:
    - `deferred = SpreadWarmth::Defer(residence count)`;
    - if not deferred, the scan runs inside a `ScanMeter<RegionPairCache>`;
    - `if (!doPush && !deferred)` guards the home fallback.

  With the row off, `Defer` is `g_on && ...`, which is false, and the meter is inert. The faithful
  path pays one bool test on the cold path, and never in a loop.
- **The budget, per frame.**
  - **Searches.** The meter reads `regionPairCache->newest` before and after the scan. `Store` advances it
    by exactly one per new pair, and the scan stores every miss, so the advance is the scan's search
    count. It wraps at `ring.size`.
  - **Candidates.** The residence count stands for the hits and the per-candidate component reads.
  - **The rule.** A scan is deferred once the frame has charged 512 searches or 4096 candidates. The
    first scan of a frame always runs. One scan can therefore overshoot by its own cost, and a town
    always serves at least one citizen per frame.
- **What a deferral does.** No scan, no home fallback, then retail's failure tail: `timers[1]` and
  `timers[15]` get `kCooldown`, 3.75 game-seconds (DAT_140172190). This is exactly retail's state after a
  scan that found no warm house. Phase 6 of `Decide` (`timers[15] > 0`) then waits, and the citizen asks
  again one cooldown later.
- **The no-cooldown variant was rejected.** Returning "no action" without a cooldown made phase 6 re-run
  `TransitionState(1)` on every remaining tick of the frame: a fresh shuffle and a re-entered candidate
  each tick.
- **The candidate-only budget came first** (commit 1, 2048 candidates). In
  `20261018_025724_pfburst_spread_warmth_probe2` one capped frame still reached 5.6 ms. Frame 7452 had
  1,016 misses averaging 145 expansions, double a typical wave's. Commit 2 meters the searches.
- **The counters.** `[Stress::warmth]` is printed on each harness row while the row is on: scans,
  candidates, searches, `maxFrameSearches`, deferrals, frames with a deferral, and the longest run of
  such frames.

### Why class B, and the divergence

A deferred citizen goes for warmth one or more cooldowns later than retail. Meanwhile `TransitionState`
continues down its shuffled candidate list, so other states may start, and the shuffle draws the sim's
`System::Random`. The game therefore diverges from retail. Nothing new is serialized.

The size of the delay, at speed 50:
- the run logs 600 frames per 10 s, about 0.83 game-seconds per frame;
- one cooldown is therefore about 4.5 frames, about 75 ms of real time;
- the longest deferral streak seen was 3 frames; deferrals per run were 116 and 153 out of 1,079 and
  2,203 scans.

That is within human reaction time, the user's rule for class B lag (memory "class B lag acceptance").
At speed 1 a frame is one tick, and the budget binds only if several citizens turn cold on the same tick.

## 5. Measurements

Every run is speed 50, 12,000 frames, the default fed stress town, perf exe, `--lock pfburst`.
- **Exe.** The final three legs ran exe md5 `3ac99465...`, built and deployed by the leg itself.
- **Machine load.** CLEAN on all three final legs.
- **Profiles.** The faithful leg runs `/profile:faithful`. The extension legs run
  `STRESS_EXT=spread_warmth`, which turns the fixes off as well.

**Hitch rows dominated by the EntityList phase with `pf_pathdistance_ms` > 2:**

| run | frame | excess ms | update ms | EntityList ms (median) | PathDistance ms |
|---|---|---|---|---|---|
| faithful `20261018_031238_pfburst_final_faithful` | 2987 | 15.30 | 22.25 | 21.71 (6.82) | 13.50 |
| | 7493 | 16.63 | 24.43 | 23.91 (7.53) | 15.18 |
| | 11903 | 18.26 | 26.84 | 26.37 (8.09) | 17.00 |
| ext `20261018_031626_pfburst_final_ext1` | 11624 | 7.19 | 15.44 | 14.82 (7.86) | 6.32 |
| ext `20261018_032013_pfburst_final_ext2` | 1083 | 7.31 | 19.49 | 18.53 (5.76) | 2.67 |

The coordinator's reference run, `20261018_015253_hitch_fed_s50_f12000` on the main checkout, had the same
class at frames 3052, 3053, 7065 and 11879, with 9.0-21.4 ms of PathDistance.

**Frame 1083 is not this class.** It is in every run, faithful and extension, at 2.1-2.7 ms of
PathDistance against a +12-14 ms EntityList delta. It is the "different cause" row of the task (frame
4351 in the reference) and is left for whoever takes it.

**Frame 11624 of ext1 is the residual**: one scan that is expensive by itself. The run was not probed. In
the probed extension run `20261018_030719_pfburst_spread_warmth_probe3`, the same shape was frame 7282:
- one `FindElderResidence` scan, 170 misses at about 28 µs each (up to 886 expansions);
- 4.8 ms, from a citizen far from the town, so that every search crosses the map.

The first-scan rule lets such a scan through. Splitting one scan across frames would mean a per-citizen
partial scan and a second copy of the faithful loop, so it was not done (§6).

**Sim cost per frame row** (`upd/f` ms per frame, citizens):

| frame | faithful | ext1 | ext2 |
|---|---|---|---|
| 3000 | 7.57, 958 | 7.29, 958 | 7.25, 960 |
| 6000 | 7.99, 1039 | 7.75, 1045 | 7.81, 1046 |
| 9000 | 8.34, 1101 | 7.39, 1100 | 8.24, 1116 |
| 12000 | 8.76, 1171 | 8.63, 1182 | 8.50, 1130 |

There is no regression, so the faithful path's seam costs nothing measurable. The faithful leg's 12000
row reads 8.76 ms. The coordinator's reference without the seam read 9.15 under its own load.

**Deaths, not attributed.**
- Ext2 ended with 1,130 citizens and 63 starvation deaths. Ext1 ended with 1,182 and 17; faithful with
  1,171 and 10.
- The probed run `..._probe3` collapsed in its first winter, from 946 to 525 citizens: 185 starvation and
  218 cold deaths. Its hunger had already risen to 10.2 at frame 1800, before its first deferral, and
  until the first deferral an extension run is the faithful sim.
- The fed town is marginal in some runs. Two clean extension runs did not reproduce the collapse, but two
  runs do not exclude a contribution.
- The coordinator should run more extension legs before the row ships.

## 6. Unfinished, and doubts

- **The single expensive scan.** A residual of up to about 6 ms of PathDistance remains, one row at
  0.05 ms over the criterion.
  - Fixing it needs either the scan split across frames (class B, a per-citizen partial-scan state, and
    the faithful loop duplicated into the ext) or a faithful per-search speed-up.
  - The per-search speed-up would also help the `pathgraph` rebuild frames' 14,000 uncached searches.
- **The deaths** of §5 need more extension legs.
- **Not done by this block.** The game gate (`tools/build.sh run`) and the resave gate were not run; they
  are the coordinator's. Acceptance was run: 0 lines off target. Unit: 1167 tests, 0 failures, 3
  skipped, including the two new `ext_spread_warmth_*` tests. The COMPPROF=4 shadow is not applicable,
  because the extension never touches the search, the cache or their results.
- **The probe is not committed.** Its source is in this hand-in's §1 description, and the session
  scratchpad kept the diff.
