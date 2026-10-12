# PATHDIST hand-in (2026-10-12)

Block PATHDIST, base main `379eb552f`. ELPROF §6 item 1: `Map::PathDistance` -> `RegionGraph::PathLength`
-> `Run` -> `FindPath` on a bucket open set, with byte-identical results. Faithful perf work: no toggle, no
registry row, no settings line, no new flag. The game was NOT run (agent rule); only `tools/build.sh build perf`
and `tools/build.sh unit` were run.

| Commit | What |
|---|---|
| `f5392e17f` perf | `src/game/Action/action_pf_region_search.cpp` (new `RegionGraph::FindPath`), `region_search.h`, retail's body renamed `FindPathAvl`, the shadow `VerifyPathLength`, the hook in `PathLength` |
| `7f7e99941` test | `tests/unit/region_search_test.cpp` (3 tests), the new TU added to `tests/unit/sources.cmake` |

## 1. The retail order, proved from the code

All lines are in `src/game/Action/action_pf_astar.cpp`.

- **The key compare.** `PathOpenSet::Insert` 0x14001d450 (`:186`) goes LEFT while
  `key.f < node.f || (key.f == node.f && node.seq < key.seq)`. It stops on an exact match (`:196`), which
  never happens: seq is unique. So the in-order sequence is f ascending, and among equal f, seq DESCENDING.
- **The pop.** `FindPathAvl` (`:500`) takes the leftmost node, which is the minimum: the smallest f, and
  among equal f the largest seq (the newest insert, LIFO).
  - The goal is tested on pop and left in the set.
  - Any other vertex is deleted (`:507`), then closed: `openStamp - 1`, `closedStamp = generation`.
- **The re-key.** `PathSearch::Relax` 0x14001d320 deletes an open vertex's old entry by walking the tree
  with that entry's own key (`:243`). It then inserts the new key with `seq = sequence++` (`:257`).
- **No structural dependence.** The key is a strict total order, so ANY structure that returns the minimum
  under it pops the same vertex. The AVL's shape and rebalancing cannot leak into the order.

The cell search's FASTPATHS claim (min f, LIFO among equal f) is therefore also the region search's order. One
difference matters. The region heuristic is a truncated Euclidean distance between centroids, and it is not
consistent: `trunc(a + b)` can be `trunc(a) + trunc(b) + 1`. So:
- f can DROP from one pop to the next;
- a closed vertex can be reopened.

The unit tests count both (the "f drops" and "reopened closed" figures in §4).

## 2. The new structure (`action_pf_region_search.cpp`)

- **Buckets.** One LIFO stack per integer f:
  - `head[f]` is an index into an entry pool;
  - an entry is `{vertex, g, next}`, 12 bytes.
- **Cursor.** It points at the lowest bucket that may be non-empty, and a push below it moves it down.
  The cursor is not monotone (see the f drops above).
- **Lazy delete.** A re-key pushes a new entry and leaves the old one where it is.
  - The pop skips an entry unless `nodes[v].openStamp == generation && nodes[v].gCost == e.g` (`:107`).
  - Why that is exact: a vertex already reached in this search is only ever relaxed with a STRICTLY
    smaller g. The open case needs `t < gCost`; the closed case skips on `gCost <= t`. So the stale entries
    of a vertex all carry a larger g than its live one. A closed vertex has no live entry, because its
    `openStamp` was decremented when it was popped.
- **Pop.** The first live entry from the top of the lowest bucket. The top of a bucket is its newest push,
  which is the largest seq, because seq rises by one per push. So this is exactly the AVL's leftmost node.
- **The node stores** are retail's, in retail's order:
  - Relax's `closedStamp - 1`, `parent`, `openStamp`, `gCost`, `sequence + 1`;
  - the pop's `openStamp - 1`, `closedStamp`.
- **`search.open.count`** is kept exactly as retail's: +1 opening, -1 popping, 0 net for a re-key. It is still
  the loop condition.
- **Not written any more:**
  - `search.open.root` and its pool. They stay null and as built, unless the shadow left a tree; the
    shadow's next `FindPathAvl` entry releases it, and so does `ReleaseStorage`'s `Clear`.
  - `PathSearchNode::openNode`. Only `Relax` reads it, on the AVL path, for vertices it opened itself under
    a fresh generation.
- **Who reads `search.open`.** Grepped `search.open` / `open.count`: outside the search, only `Init`
  (`region_graph_build.cpp`), `ReleaseStorage` and the grid search's users. The grid search
  (`PathfinderThread +0x78`, the worker's) is a separate `PathSearch` instance and is untouched, so PFBENCH's
  `sequence - open.count` is unaffected.
- **sqrtf.** The two heuristic `sqrtf` calls are now `__builtin_sqrtf`, as the edge-cost call already was
  (the same correctly rounded `sqrtss`; `WorkImpl_Sqrtf_132010` is `sqrtf`).
- **Return value.** FindPath now returns plain 0 / 1. Run tests only the low byte.

## 3. The identity argument (summary)

The open set is a strict total order on (f asc, seq desc), so the popped vertex is a function of the set of live
keys, not of the structure that holds them. The new search holds the same live keys at every step: the same
relaxations with the same f, the same seq numbering, and the same deletions (explicit in retail, lazy here).
So it pops the same vertex at every step. Everything downstream is unchanged code over the same node array:
- the stores, and therefore the parent chain;
- Run's extraction (0x2000 clamp);
- PathLength's sum;
- PathDistance's cache Store.

That gives the same region path, the same `PathLength` integer, and the same `regionPairCache` contents in the
same order.

## 4. Unit tests (`tests/unit/region_search_test.cpp`)

**The setup.**
- The test links `action_pf_region_search.cpp` alone and runs the PRODUCTION `RegionGraph::FindPath` on a raw
  `RegionGraph` record (calloc, as retail's MemAlloc + Init; no C++ ctor or dtor).
- The reference is retail's Run / FindPath / Relax / PathLength sum with the AVL replaced by a `std::set`
  under the AVL's key. The real `PathOpenSet` cannot link into `unit`, which is the FASTPATHS precedent.
- Every graph runs 16 queries on ONE persistent state: generations bump, stale stamps are left from earlier
  queries, and 5% of queries have start == goal.

**What is compared after every query:**
- found;
- the path entries (Run's extraction);
- the PathLength sum;
- `search.sequence` (relaxations);
- `search.open.count`;
- expansions;
- generation;
- every node's parent / openStamp / closedStamp / gCost.

| Test | Graphs / queries | Result | Coverage |
|---|---|---|---|
| block graphs (8x8 blocks, 1-4 regions per block, lattice centroids) | 400 / 6400 (6282 found, 118 none) | **0 mismatches** | 41,452 tie-decided pops, 30,070 re-keys, 398 reopened closed vertices, 3,397 f drops, 80,030 expansions |
| wild graphs (duplicate centroids, zero-length / long / directed edges, -1 and duplicate entries, self-loops, up to 700 vertices) | 400 / 6400 (5396 found, 1004 none) | **0 mismatches** | 73,533 tie-decided pops, 47,052 re-keys, 59 reopened, 1,640 f drops, 200,776 expansions |
| corridor: 9,000 regions in one serpentine path | 5 queries | **0 mismatches** | f in the thousands (the bucket array grows mid-search); the 0x2000 path clamp; start == goal |

- **Mutation check (run once, then reverted).** With the reference's tie order flipped to FIFO, the same
  tests report 5,582 and 3,556 mismatches. So ties really do decide pops in these graphs, and the LIFO claim
  is what the tests prove.
- **Suite.** `tools/build.sh unit`: **1063 tests, 0 failures** (1060 + 3).

## 5. The runtime shadow (COMPPROF=4)

- **The hook.** `RegionGraph::PathLength` (`action_pf_region.cpp:770`):
  `if (g_verifyOn) VerifyPathLength(start, goal, count, local_8008, sum);`
  - It sits after retail's sum.
  - Off, it costs one load of `g_verifyOn` through `.refptr` and a not-taken branch. This was checked in the
    perf object: one `.refptr.g_verifyOn` reloc and one call reloc to `VerifyPathLength`.
- **The body.** `RegionGraph::VerifyPathLength` (`action_pf_astar.cpp:616`, noinline, NOT RETAIL):
  1. bumps the generation as Run would;
  2. runs retail's verbatim AVL body `FindPathAvl` (`:455`);
  3. extracts the path and sums it as Run and PathLength do;
  4. compares count, every entry, the sum, `search.sequence` and `search.open.count`;
  5. calls `StressHarness_Verify("PathDistance", ok, start << 32 | goal)`.
- **What it covers.** Only cache misses reach PathLength, so the shadow sees every region search the game
  actually runs (including `BuildStructure`'s direct PathLength call), not every PathDistance call.
- **For the coordinator:**
  - The `[COMPPROF]` report should show `PathDistance` checks > 0 and mismatches 0.
  - A `[VERIFY] MISMATCH PathDistance` line prints `detail` = (start << 32) | goal, the region pair to
    replay.

## 6. Memory and allocation

- **Scratch.** One process-lifetime static, because RegionGraph is a pinned 0x108 record.
  - `head`: int32 per f value. It grows by doubling to the largest f seen, starting at 256 entries
    (1 KB). On a 1024-cell map f stays in the low thousands, about 4-16 KB.
  - `pool`: 12 bytes per push. It is cleared, not freed, per search, and its capacity is the largest search
    so far.
  - The corridor test's worst case retained 562 KB. Real towns should stay in the tens of KB.
- **Allocations.** None in steady state; growth only on a new maximum.
- **Reset per search.** `std::fill` over the previous search's touched f range only, plus `pool.clear()`.
- **AVL pool.** The AVL pool is no longer touched by the production path, so no `MemAlloc` for open-set
  nodes at all (retail grew it by doubling chunks). Under the shadow it behaves as retail.
- **Threads.** RegionGraph's search runs on the sim thread only, as its own `search` member already
  requires. The static scratch adds no new constraint.

## 7. Doubts

- **The PFBENCH timer.** At COMPPROF=4 the `PfMainTimer` in `Map::PathDistance` also times the shadow
  (the AVL search plus the extraction). Don't read `main ms/frame PathDistance` from a level-4 run. The
  perf figure needs a level 1 or 2 run, A/B against main.
- **No speed number.** The expected gain comes from ELPROF's split: AVL Insert / Delete / Rebalance /
  ChildHeight ~22% of the bucket phase, plus most of FindPath's 14% self (the leftmost descent and the
  re-key's tree walk). I did not measure it; that is the coordinator's stress A/B.
- **The relax lambda is not inlined.** GCC -O2 kept it out of line (two call sites, with
  `vector::push_back` growth inside). That is one call per relaxation, still far cheaper than an AVL insert.
  `always_inline` is an easy follow-up if the profile shows it.
- **f >= 0 is assumed, not checked.** g is a sum of `int(sqrtf(non-negative))` and so is h. A negative f
  needs signed overflow of a centroid square or of g. Retail would hit UB there too, but its AVL would
  survive it, while the bucket index would not. Centroids are map cells (< 1024), so the largest square is
  about 2M and g stays far below 2^31.
- **The test reference.** It is a std::set port, not the real `PathOpenSet`. The real AVL is compared only by
  the runtime shadow, which needs the coordinator's level-4 stress run.
- **`FindPathAvl` is ~90 lines of retail code** that production no longer calls (shadow only). If the shadow
  is retired, the body can go, but then the only record of retail's open-set usage for 0x14001dcd0 is the
  comment in `action_pf_region_search.cpp`.
