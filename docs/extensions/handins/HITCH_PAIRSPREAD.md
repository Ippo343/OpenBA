# HITCH_PAIRSPREAD hand-in (2026-10-18)

Branch `pairspread` from `hitch` `8dfa0921a`. The CitizenList share of the hitch rows at the PathGraph
rebuild (every 300 simulated seconds). **Result: faithful, no extension.** The 7-pairs-per-tick spread
was never the expensive part. Two exact speed-ups remove what was: a region path-length memo, and a
flat index for the PathGraph's region-pair map. Both are proved by COMPPROF=4 shadows with 0 mismatches.
Not merged.

| Commit | What |
|---|---|
| `2f81815c5` diag | `[Stress::pairs]` line per drain: BuildPathGraph's steps, ProcessPaths' search vs the rest, the memo's counts |
| `e8b8e5c8c` perf | `RegionGraph::PathLength` memo (`action_pf_region_search.cpp`, `region_search.h`), its unit test, the `PathLengthMemo` shadow |
| `4abef31a3` perf | `PathCostIndex` (`src/game/Entity/path_cost_index.{h,cpp}`): the PathGraph+0x18 map as a hash table, the `PathCostIndex` shadow |
| (this commit) docs | this hand-in |

## 1. Where the time goes

The task's model was 350 region searches per frame for the ~28 frames of the spread, +3.5 ms each.
`[Stress::pairs]` says otherwise, on the faithful `hitch` code (first window of every run):

| part | cost | where |
|---|---|---|
| BuildPathGraph | **2.3 ms in ONE tick**: free the old tree 0.4 ms, list the pairs 1.9 ms | the rebuild-start frame |
| ProcessPaths' 9,902 pairs | about 15 ms of `PathDistance` + 1.4 ms of the rest, over 28 frames | about 0.55 ms per frame |
| drain: ReassignWorkers + AssignCitizens | 3.3-3.9 ms + 1.6-1.9 ms (early drains); 0.1 + 1.8-2.0 ms (from frame ~2200) | the drain frame |

- **Only ~4,000 of the 9,902 pairs search.** The rest are same-region pairs (cost 1) or pair-cache hits.
  `ProcessPaths` passes `useCache = 0`, but that only skips the `Store`; the `Find` still answers
  pairs other callers stored.
- **One search is ~3.7 µs.** About 143 searches per frame at 50x is about 0.5 ms.
- **The listing.** BuildPathGraph does 58 x 278 de-duplication lookups in a 9,902-node AVL, plus 9,902
  `MemAlloc` + AVL inserts. Before that it does 9,902 `MemFree` of the old tree.
- **So the hitch rows' `cit+3.6..6.2` are the rebuild-start frame and the drain frame**, not the spread.
  Frame numbers in the hitch CSV are `worldFrame + 1`. The coordinator's rows were 1081 / 1441 / 3602 /
  7202 (rebuild start, 28 frames before a drain) and 1109 / 5783 / 6143 (drain).

**What reads the stored costs, and when.** The map at PathGraph+0x18 is read in three places.
- **ProcessPaths** finds or inserts each pair's key and stores the cost.
- **BuildPathGraph** checks a key before listing a pair, and inserts a 0 placeholder.
- **RegionPairCost** (0x1400d6060) answers AssignCitizens' commute queries at the drain. A miss
  computes, stores and returns 0 (retail bug 6).

Nothing reads it between the drain and the next rebuild. Nothing walks it in order, reads its
`count`, or saves it. `ArrayPlusTree_Clear` frees it at teardown.

## 2. The fixes, both faithful

### 2a. The PathLength memo (`e8b8e5c8c`)

- **The claim.** `RegionGraph::PathLength` 0x1400a8ba0 is a pure function of `(start, goal)` and the
  region graph: the centroids `x / y` and the neighbour lists.
  - The search compares stamps for equality with a fresh generation, so nothing a previous search left
    changes the result (PATHDIST section 1).
  - Every write to the graph happens inside `RegionGraph::Flush` (0x1400a8a90): `RebuildBlock`,
    `RebuildAdjacency`, `AllocRegion`, `BuildRegion`. `Init` only marks blocks dirty.
  - So a result stays valid until the next Flush that rebuilds something.
- **The memo.** It is keyed by `(graph, start, goal)`, ordered, because the A* is not symmetric. Each
  graph-changing Flush drops it whole (`NoteGraphChanged`), and so does a different graph. The table is
  open addressing, from 2^14 to 2^19 slots (8 MB at the cap), and starts over when full.
- **Where it sits.** After PathLength's own Flush and before the search. `Map::PathDistance`'s pair-cache
  `Find` / `Store` are untouched, so the cache's ring, stamps and evictions are retail's, as
  HITCH_PFBURST section 3 requires.
- **What a hit skips.** The search's side effects on `search.*`: the generation, the stamps and the
  parents. Only the next search reads them, and it starts a new generation.
- **Why it pays.** In the stress town the graph does not change after the first minute: `graph_changes=0`
  between every pair of drains of the 12,000-frame run. So from the second rebuild on, 97-100% of the
  window's searches hit.

### 2b. The PathGraph region-pair map as a flat index (`4abef31a3`)

- **The change.** `PathGraph::CostFind / CostSet / CostAdd / CostClear` replace the inline AVL searches,
  `PathCostTreeInsert` and the free in ProcessPaths, BuildPathGraph and RegionPairCost. The teardown and
  the constructor clear the index.
  - Retail's semantics are kept: insert-or-find keeps a present value, ProcessPaths overwrites.
  - `m_bst` stays empty (root null, count 0). The struct layout is unchanged.
- **Why it is the same state.** The keys are unique under a strict `(high, low)` order and the map is
  never walked. A map that answers the same membership and the same value for every key is the same
  state.

### Proof (COMPPROF=4)

At COMPPROF=4 a memo hit still runs the search, and the retail AVL is maintained beside the index, with
every lookup compared.

| leg | shadow | checks | mismatches |
|---|---|---|---|
| `20261018_103746_pairspread_memo_verify` (memo only) | PathLengthMemo | 785,378 | 0 |
| `20261018_111748_pairspread_index_verify` (both) | PathLengthMemo | 752,351 | 0 |
| | PathCostIndex | 1,046,000 | 0 |
| | pathgraph_assign_seq | 33 drains | 0 |
| | PathDistance, TickList, FrustumCull, every other | all | 0 |

Both legs are 12,000 frames at 50x.

**Unit tests.** `tools/build.sh unit perf`: 1168 tests, 0 failures, 3 skipped (no kit fonts).
`region_search_path_length_memo` is new.

## 3. Measurements

Every leg is 12,000 frames at 50x, the fed stress town, `STRESS_EXT=spread_warmth` (fixes off),
`--lock pairspread`, and the leg built and deployed its own exe (md5 checked in `exe_identity.txt`).
- **Before.** `hitch` `8dfa0921a`, built in a second worktree.
- **After.** This branch, md5 `1d940397...`.

### `[Stress::pairs]`, after

| window | BuildPathGraph | search over the window | rest over the window | memo hits / lookups |
|---|---|---|---|---|
| 1st (frame 360) | 0.45 ms | 14.4 ms | 0.21 ms | 17 / 4.4k |
| 2nd onwards | 0.33-0.34 ms | 1.1-1.4 ms | 0.20 ms | 2.5k-5k / 3.5k-5.2k |

Before, BuildPathGraph took 2.3 ms and the rest 1.4 ms. The search spread is now about 0.05 ms per frame.

### Hitch rows (harness rows excluded)

| leg | machine | rows (frame: excess ms, deltas over the median) |
|---|---|---|
| before `20261018_112934_pairspread_before` | CLEAN | 180: 23.9 (update-other); 300: 10.9; **1109: 8.23, cit+6.2 ent+1.9 pd 2.8 (drain)**; 7202: 8.11, ent+9.5 cit+4.3 (2 Updates, rebuild start); 11994: 7.27, ent+6.5 |
| after `20261018_113357_pairspread_after` | CLEAN | 180: 24.0; 300: 12.1; **749: 7.30, cit+6.1 ent+0.6 pd 2.9 (the first drain)** |
| after2 `20261018_113950_pairspread_after2` | BUSY | 180: 27.1; 300: 11.7; 1082: 7.30, ent+6.6 cit+0.1; 7559: 8.37, ent+7.7 pd 6.5 (cold wave) |

- **Frames 180 and 300** are in every leg, before the first rebuild: not this block's.
- **Frames 1082 and 7559 are EntityList.** 1082 is the 300 s EntityList burst; 7559 is a cold-wave scan,
  ext.spread_warmth's residual.
- **No rebuild-start row is left in either after-leg.** No `cit+` delta over 0.2 ms appears at any
  rebuild start.
- **The one CitizenList row left is the FIRST drain** (frame 749, 0.16 ms over the bar, one leg of two).
  - Its ReassignWorkers costs 3.3-4.1 ms. It runs `CalcDistance` -> `PathDistance(useCache = 1)` for
    occupied x empty residences, and at the first drain those residence-to-residence pairs are new
    region searches: the `pd 2.9`.
  - From the second drain on, the memo answers them: ReassignWorkers 1.3-1.7 ms against 3.3-3.9 ms
    before.
  - From frame ~2200 no empty residence is left, and it is 0.1 ms in both.
- **The drain's AssignCitizens is unchanged** at 1.5-2.1 ms. Its cost is the per-need candidate work,
  not the map lookups.

### update_ms_per_iter (18 steady rows)

| leg | machine | update ms / iter | CitizenList ms / frame | EntityList ms / frame |
|---|---|---|---|---|
| before | CLEAN | 0.1639 | 0.129 | 7.61 |
| after | CLEAN | 0.1621 (-1.1%) | 0.085 (-34%) | 7.56 |
| after2 | BUSY | 0.1683 | 0.093 (-28%) | 7.81 |

The BUSY leg's +2.7% is EntityList (+2.6%), which this block does not touch.

The stress town is not reproducible run to run: the before leg, the coordinator's
`20261018_033233_hitch_all_s50_f12000_warmth` and the after legs already differ in `busy` at frame 600,
before any rebuild could matter. So the shadows are the identity proof, not a CSV comparison.

## 4. Why no extension

- **The task's fallback (b) was for a costly spread.** The spread costs about 0.5 ms per frame
  unmemoized and about 0.05 ms with the memo.
- **Moving the pairs per wall frame would only delay the drain.** At 64 pairs per frame that is 155 frames,
  2.6 s wall instead of 0.47 s. It would not touch the two frames that carried the rows.
- **The first drain's ReassignWorkers cannot be spread faithfully.** Its searches are retail's, and its
  `EntityAnchorCell` random draws pin the call order (HITCH_PATHGRAPH section 6).
- **If the coordinator wants the 0.16 ms gone,** the candidate is an extension that splits ReassignWorkers
  from AssignCitizens across two ticks. That is class B, because the seats move one tick. It was not built.

## 5. Open, and doubts

- **The resave gate and `tools/build.sh run` were not run.** They are the coordinator's. The changes are
  exact by construction and by shadow, so neither should move.
- **No 10x / 20x legs were run.** At 10x the rebuild frames carry a fifth of the 50x work.
- **A real town changes its graph often.** Every building placed or finished triggers a Flush, which drops
  the memo. Then the memo costs one probe and one store per search, and saves nothing until the graph
  settles. This is bounded and exact, but it was not measured on a growing town.
- **Memory.** The memo is up to 8 MB at its cap, then it starts over. The PathCostIndex is 512 KB at 2^15
  slots for 10k pairs, and grows while under half full.
- **The `[Stress::pairs]` diagnostic** costs four RDTSC reads per pair, only while the stress harness runs
  (`g_pfProfOn`), and prints one `ErrLine` per drain.
- **The baseline worktree** `C:/code/GIT/wt_pairspread_base` (detached at `8dfa0921a`) can be removed
  with `git worktree remove`.
