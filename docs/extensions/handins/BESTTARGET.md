# BESTTARGET hand-in (2026-10-12)

Block BESTTARGET, base main `7f77a506a`. The bucket phase's `CitizenBehavior::FindBestTarget` and
`CharacterGrid::QueryNearestCharacter`. The result is a byte-identical replacement of the
QueryNearestCharacter walk. It is faithful perf work: no toggle, no registry row, no settings line and no new
flag. The game was NOT run (agent rule). Only `tools/build.sh build perf` and `tools/build.sh unit` were run.

| Commit | What |
|---|---|
| `2b7b8bd04` perf | `src/game/Component/charactergrid_nearest.h` (new: `CharGridNearest::Query`). `action_pf_roots_misc.cpp`: retail's body renamed `QueryNearestCharacterRetail`, the new `QueryNearestCharacter`, the shadow `VerifyNearest`. `CharacterGrid.h`: one member function declared, no field. |
| `164b4e00c` test | `tests/unit/charactergrid_nearest_test.cpp` (4 tests; globbed, no sources.cmake change) |

## 0. The brief's premise, corrected by the profile

The two functions are **not** nested. In the parked sampler run (perf exe of `7f77a506a`, tag 8, 3,639 samples),
`--callers` / `--callees` show:

- **`QueryNearestCharacter`** is called only by `MoveTo::Enter` (632 samples) and `HuntAnimal::Enter` (2).
- **`FindBestTarget`** never calls it. FindBestTarget's 498 samples (13.7%) break down as follows:
  - `Map::PathDistance`: 404. That is the region search PATHDIST already replaced (`RegionPairCache::Find`,
    `FindPath`).
  - `FindBestAdjacentCell`: 30.
  - `EntityAnchorCell`: 17.
  - `FindComponent<*>`: about 20.
  - Self: 2.
- **Who calls FindBestTarget:** `State_Education` 216, `State_SeekHappiness` 214, `State_GatherResources` 51,
  `State_AdultTransition` 17.

So FindBestTarget has nothing of its own to make cheaper. Its cost is PathDistance, called 1 or 2 times per
candidate.

## 1. (a) What the query computes

**`CharacterGrid::QueryNearestCharacter` 0x140092260** returns the character nearest to `point`, under these
rules:
- **Radius.** Distance is Euclidean in xyz and must be strictly below `radius`.
- **Filters.** It skips `*exclude`. When `typeId != 0`, it also drops characters whose entity has no component of
  exactly that Type().
- **Ties.** Equal distances go to the **first visited** character, because the test is `dsq < best`, not `<=`.

**How it searches.** It walks the loose implicit quadtree:
- The grid is 2048 world units with 8 levels, so a level-7 cell is 16 units wide.
- A node's box is its center ± 2 × extent, so it is twice the nominal cell.
- The walk is a DFS on a stack. Children are pushed 1..4 and popped 4..1.
- Retail runs two tests when it pops a node:
  - nodes with `cellOccupancy <= 0` (an empty subtree) are dropped;
  - nodes whose box misses the query box are dropped.
- For each node that passes, retail walks the cell's circular slot list through `prev`, starting at the head.

**What each call visits:**
- **Caller:** `MoveTo::Enter`, once per MoveTo action entered, with radius 0.5 and no type filter. The answer
  only sets `MoveTo +0x474`, which says another character is close and facing the same way.
- **Cells:** every character sits in a level-7 cell, because its description radius is far below 2048 / 2^7. So a
  0.5-radius probe intersects the loose boxes of 2-3 cells per axis: **4-9 cells, roughly a 32-48 unit square
  of characters**.
- **In the heavy town** that is tens of candidates per call. The candidate loop takes 2.0x the walk's samples. At
  a few ns of overlapped misses per candidate, that suggests roughly 50-100 candidates per call. This is an
  estimate from the profile, not a count.
- **The walk** pops about 100-200 nodes per call: 8 levels, a few intersecting nodes per level, and 4 children
  tested per subdivided node.

## 2. (b) Where the time goes (instruction-level, `objdump -d` x leaf histogram)

QueryNearestCharacter's true inclusive count is about **790 samples, 21.7% of the bucket phase**. The 634 that
`sample_report` lists as inclusive leave out leaves that `addr2line` names after inlined callees: `Vector3::Dot`
75, `Array<CharacterGridSlot>::operator[]` 51, `InlBoxIsect` 31.

| Part | Samples | Share of the query | What it is |
|---|---|---|---|
| Candidate loop | 443 | 56% | |
| - position loads + dsq | 353 | 45% | `character->modelComponent` (+0x68), then `model->translation` (+0x158): two dependent cache misses per candidate. 128 samples sit on the single `movss 0x158(%rax)` |
| - slot / list / exclude | 90 | 11% | `slots[ent].character`, `.prev`, `cmp *exclude` |
| Walk (pop, occupancy load, box test) | 218 | 28% | `cellOccupancy[cell]` (an 87 KB array, random access) and the six-compare box test, for every pushed child |
| `CharacterGridWalker::Init` | 65 | 8% | clears all 256 nodes (6 KB of stores) on every call |
| `CharacterGridWalker::Subdivide` | 59 | 7% | the call, plus four 36-byte node writes |
| other | 6 | 1% | |

**Elsewhere in the bucket phase:**
- **PathDistance calls in scoring:** none. The query is pure geometry.
- **`WorkImpl_Sqrtf`:** under FindBestTarget's fallback (the `PathDistance < 0` branch), not under the query.

## 3. (c) The byte-identical cheaper version (implemented)

`CharGridNearest::Query<Policy>` (`src/game/Component/charactergrid_nearest.h`) makes three changes. Each one
keeps the result, the visit order and every float operation.

1. **No walker clear.**
   - Retail's Init stores zero over 256 nodes. Each node the query reads (`nodes[recIdx]` with `recIdx <= top`) was
     written by a push first, and Subdivide reads only the node it pops. So those stores are dead.
   - The fast walk keeps a 96-node stack, written only by pushes. The frame is 0xd98 bytes, so it needs no
     `___chkstk_ms` (retail's frame is 0x24d8).
   - The stack holds at most 1 + 3(levels-1) nodes, which is 22 for the game's 8 levels.
   - For `levels > 31`, Query returns false without writing anything, and the caller runs the retail body.
2. **Children are tested when they are made, not when they are popped.**
   - Retail pushes all four children. Later it pops and drops any whose occupancy is <= 0 or whose box misses.
     The drop has no side effect.
   - Nothing the test reads changes during a query: occupancy, the walker geometry, the query box and `levels`.
     The query writes only `*best` and locals. The only calls are the filter's `Type()` getters.
   - So testing at push time keeps exactly the set of nodes retail processes. LIFO among the survivors gives the
     same relative order (4, 3, 2, 1).
   - Both tests are retail's to the operation:
     - The box is `ext = 2*e`, `min = pad + (c - ext)`, `max = pad + (ext + c)`, with pads (0, 0, -1) and
       (0, 0, +1), then the Box::Intersect compares.
     - The child centers are Subdivide's: `h = e*0.5`, `n = h*-1`, `cx + n`, `h + cx`, `n + cx` and `h*1 + cx`, in
       its operand order.
     - The build has no `-march` and no `-ffast-math`, so SSE2 has no FMA and no contraction or reassociation can
       touch them.
3. **Candidates are gathered and prefetched in chunks of 16.**
   - The cell list is walked through the slot array, in the same `prev` order with the same termination. The
     character pointers go into a buffer.
   - Two prefetch passes follow:
     - **`&c->modelComponent`.** A hint only, so it cannot fault.
     - **`&model->translation`.** Only when `typeId == 0` and only for candidates other than `*exclude`. Those are
       exactly the candidates whose model pointer retail itself loads.
   - Scoring then runs in buffer order, which is retail's order:
     - skip `*exclude`, re-read on every candidate as retail does;
     - the filter;
     - `Vector3 diff(pos - point)`, then `diff.Dot(diff)`. This is the same `Dot` (y·y + x·x + z·z);
     - the strict `dsq < fr2` update of `*best`.
   - The only reordering is that slot reads move ahead of the filter's virtual `Type()` calls. Those are pure
     getters, and nothing writes the slot array during a query.

**The identity argument.** The same nodes are processed in the same order, and each one walks the same list in
the same order. Each candidate gets the same dsq and the same strict comparison against the same running
minimum. So the same character is returned, ties included.

**Expected gain (not measured; the coordinator runs the stress town).**
- What it removes for certain: Init's 65 samples and most of Subdivide's 59, plus the pop / skip work for children
  that fail a test.
- The candidate loop's misses still happen. The prefetch passes only let them overlap. Out-of-order execution
  already overlaps part of them, so that gain is uncertain.
- Estimate: 120-250 of the query's ~790 samples, which is 3-7% of the bucket phase. It counts most in the
  job-and-home windows, where the bucket phase is 4x.

## 4. Unit tests (`tests/unit/charactergrid_nearest_test.cpp`)

**The setup.**
- The PRODUCTION template runs on a test grid: slots with circular `prev` lists built by `Link`'s own insert,
  `cellHead`, `cellOccupancy`, `worldSize` and `levels`.
- The reference is retail line for line: `Init`, including the 256-node clear, `Subdivide` and the 0x140092260
  loop. The real bodies cannot link into `unit` (the PATHDIST precedent). The runtime layer of the proof is the
  COMPPROF=4 shadow.
- Each query compares the returned character, by pointer identity.

**What the grids contain:**
- characters on 0.25 / 0.5 / 1.0 lattices plus 8% exact duplicates, so ties are frequent;
- query points on or beside characters;
- radii 0, 0.5, 0.75, 1, 2.5, 3, 8 and 40;
- z in [-2, 2] against the walker's ±1 z pads;
- characters at random levels;
- 10-50% stale cells (listed where the character was, not where it is);
- shuffled slot indices;
- the type filter and a random exclude;
- odd world sizes (2047.3, 100.7) with inexact centers;
- occupancy counts perturbed to 0 / -1 / 1 so they disagree with the lists.

| Test | Queries | Result | Coverage |
|---|---|---|---|
| town (2048, 8 levels, 600 characters in ±60) | 16,000 | **0 mismatches** | 9,118 found, 2,354 order-decided ties, 1.87 M candidates, 375 k nodes |
| typed (HuntAnimal filter, 3 types) | 12,000 | **0** | 6,953 found, 1,157 ties |
| odd (2047.3, 6 levels, continuous, bad occupancy) | 12,000 | **0** | 4,673 found, 793 ties |
| small (100.7, 4 levels, 50% stale, bad occupancy) | 18,000 | **0** | 3,696 found, 633 ties |
| mutations (reference changed, must disagree) | 2 x 16,000 | ties-to-later: **1,359** mismatches; child order 4..1 pushed reversed: **88** | proves the comparison sees both the tie rule and the DFS order |
| deep grid (levels 32) | 1 | returns false, `*best` untouched | the fallback path |

Suite: `tools/build.sh unit`, **1078 tests, 0 failures** (1074 + 4).

## 5. The runtime shadow (COMPPROF=4)

- **The hook.** `CharacterGrid::QueryNearestCharacter`:
  `if (g_verifyOn) VerifyNearest(this, *best, point, radius, typeId, exclude);`
  - When the flag is off it costs one `.refptr` load and a branch that is not taken (checked in the perf
    disassembly at `0x14000f1fe`).
  - `VerifyNearest` is `noinline`.
- **What it compares.** It runs `QueryNearestCharacterRetail`, the real retail body with the real walker
  `Init` / `Subdivide`, into a local, then calls `StressHarness_Verify("BestTarget", ref == fast, fast ^ ref)`.
- **Side effects.** Retail's body writes only its out-param and its stack walker. So the shadow changes nothing
  the game sees.
- **For the coordinator:** a COMPPROF=4 heavy-town run should report `BestTarget` with N checks and 0 mismatches.

## 6. Not done, and the best non-identical idea

- **FindBestTarget.** There is nothing to change here. Its time is PathDistance (PATHDIST's region search plus
  `RegionPairCache::Find`, 158 self samples in the phase). The next lever is the cache lookup, not the target
  scan.
- **The large remaining cost is the candidate count.** A 0.5-radius probe scans 4-9 cells of 16 units because the
  boxes are loose (2x).
  - **The idea.** A tight cell set would visit only cells whose nominal box, widened by r plus a small float
    margin, contains the point. That is 1-4 cells.
  - **The invariant it needs.** It is exact only if every character's stored cell matches its current position
    at query time, so that a character within r of the point lies in one of those cells.
  - **Why that is unproven.** `Place` runs at the end of the move tick (`citizenai_executor.cpp:522`), but other
    writers of `ModelComponent::transform` (spawns, `AnimalComponent::Place`, enter / leave building) are not
    proven to re-place before the next query. The unit tests deliberately include such stale cells, and retail's
    answer depends on them: a stale character outside the tight set is visited by retail and skipped by the
    tight walk.
  - **Status.** Not implemented. If wanted, it would be a non-faithful (class B) change, or it would need a proof
    that every translation writer reaches `Place` first.

## 7. Doubts

- The prefetch passes are a guess at memory-level parallelism. If the stress-town A/B shows no gain from them,
  they can be dropped and the other two changes kept. The identity proof does not depend on them.
- `RadiusQuery` 0x140092990 (`citizen_updatehealth.cpp`, tornado) uses the same walker and has the same dead
  6 KB clear. It is not in this profile, so it is untouched.

## Benchmark (coordinator, 2026-10-12; idle machine, `machine load: CLEAN`; BESTTARGET + PERFXTBL together, vs the ELPERF runs)

| town | sim ms/iter | tree walk ms/frame | buckets ms/frame | speed-50 windows 1-3 Update ms / fps |
|---|---|---|---|---|
| heavy, ELPERF (`20261012_144605`) | 0.29 | 1.955 | 0.531 | 9.1 / 11.4 / 13.5 -- 225 / 150 / 85 |
| heavy, ELPERF2 (`20261012_160153`, `20261012_16xxxx_elperf2_50_heavy`) | 0.29 | 1.918 | 0.551 | 9.2 / 11.4 / 13.3 -- 217 / 148 / 93 |
| compact, ELPERF (`20261012_144816`) | 0.16 | 1.094 | 0.285 | |
| compact, ELPERF2 (`20261012_160403`) | 0.16 | 1.074 | 0.285 | |

Within run noise on every column: the tree walk is -1.9% / -1.8% (PERFXTBL's share; with
`animBatchAll=1` the per-iteration probe was only `BatchEntry`, ~4% of the walk in the
sampler, so about half of it shows), the bucket phase is unchanged (BESTTARGET's estimate of
3-7% does not show; the shadow proves it identical, the A/B does not prove it faster). Both
stay merged on their identity proofs; neither moves the speed-50 fps. One run per cell.
