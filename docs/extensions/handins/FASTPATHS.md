# FASTPATHS hand-in (2026-10-12)

Block FASTPATHS, base main `defef548a` (Merge EXTSAVE). Builds PATHRESEARCH §4.1 as one class B
extension, `ext.fast_paths` (default off, formatVersion 1): (a) a heuristic scaled by the cheapest step
cost the search can pay, (b) a bucket queue that pops in retail's (f, LIFO) order, (c) division-free
neighbours, (d) SoA scratch outside the pinned `PathfinderThread`. The user's decisions: equal-cost paths
that differ from retail are acceptable inside a locked game; class B; GPU is a separate prototype.
The game was NOT run (agent rule). Only `tools/build.sh build`, `build perf` and `unit` were run.

## 1. What was built

| Commit | What |
|---|---|
| `c2ca19be9` | `src/ext/fast_paths/fast_search.{h,cpp}`: the search, standard C++ only |
| `b138efa8c` | the registry row, `src/ext/fast_paths/fast_paths.{h,cpp}` (selection, dispatch, type counts, shadow), the seams in `ApplyCommand` and `GameUpdateLoop::Load` |
| `1f68f9f31` | `tests/unit/ext_fast_paths_test.cpp`, 6 tests (unit 1039 -> 1045, 0 failures) |

- **The search.** `FastPaths::Searcher::Run` (`fast_search.cpp:130`) takes `SearchInput`. Its fields are
  exactly what `GridPathSearch::Run` 0x14001ce00 reads: width, cellFlags, stepCost[0x20], passableTypes,
  allowDiagonal / manhattan / respectBlocked, start and goal, plus `heuristicScale`. It writes retail's output
  (the path start-first, at most 0x4000 cells, and the count; 0 means no path). It also returns
  `cost` (g of the goal), `expanded` (cells closed) and `inserted` (relaxations, i.e. retail's
  `PathSearch::sequence`). `Status::Unsupported` means "run retail's search instead" (§2).
- **The scale.** `HeuristicScale` (`fast_search.cpp:91`) is the minimum `stepCost[t]` over the types t that
  are passable AND present. `PresentTypes` scans a grid. The proof that this scale keeps h consistent is in
  the `fast_search.cpp` header comment and summarised in §4.
- **The glue.** `fast_paths.cpp`:
  - `SelectForGame` `:97`
  - `Run` `:114` (fast, or retail's `grid.Run` when the fast search reports Unsupported, then the shadow
    hook when armed)
  - `NoteCellTerrain` `:139`
  - `Shadow` `:146`
  - `FastPaths_ShadowHook` `:188`. Its one marked line is where PFBENCH adds
    `StressHarness_PathShadow(r)`. The harness header is not included.
- **Registry.** `Ext::Extensions::fast_paths` (`registry.h`), and the row "Faster pathfinding" in
  `registry.cpp` (Tier::Extension, Class::B, default false, formatVersion 1).

### A finding the spec missed: type 11
PATHRESEARCH §4.1 says "minCost = the smallest stepCost over the passable types". Citizens search with
the walkable mask 0xe1f (`MoveTo` passFlags, `action_pf_roots_drop.cpp:211`). That mask includes type 11,
and no profile sets type 11's cost (`Map::SetMovementCosts`, `action_base_move.cpp:234`), so it keeps the
grid ctor's 1 (`pathfinder_seed.cpp` GridCtor). Taken over the mask, the minimum is therefore always 1. That
is retail's heuristic, and (a) would gain nothing. The admissible minimum is over the types the grid HOLDS:
4 on open ground, 3 once a road exists. So the worker keeps a count per terrain type:
- a full scan of `cellFlags` on the first request after each `SelectForGame`. The grid was primed by direct
  stores in `Pathfinder_Seed`, and no command reports those;
- then command 2's hook keeps the counts current.

Scan, hook and requests all run on the worker, in queue order, so the counts always describe the grid
that request sees. If a type-11 cell ever appears, the scale drops to 1 for as long as it exists, and the
extension is still correct.

## 2. The seam, and where the selection is read

- **The selection.** `FastPaths::g_mode` is a `std::atomic<uint8_t>` in `fast_paths.cpp`, outside the
  pinned 0x1F8 layout (INVENTORY §5.2). Values: 0 retail, bit 0 fast, bit 1 shadow.
- **Who sets it.** `GameUpdateLoop::Load` calls `SelectForGame()` twice:
  - `gameupdateloop_load.cpp:735`, right after `Ext::LockForGame(this)`. This is final for a new game.
  - `:1329`, after step 31 (worldgen, or `SaveRestore` on a load) and before step 32. On a load,
    `ExtSave_ReadTail` has run `Ext::LockForGameWithSet` from the manifest inside `SaveRestore`
    (EXTSAVE §2), so the set is final on every route here. This is the call that counts.
- **Why that is before the first request.** Command 5 is issued only by:
  - `MoveTo::PathSetup` (sim tick);
  - the road / path tools (`roadtool*.cpp`, `pathtool_setselected.cpp`);
  - boat routes (`BoatComponent::SetDestination`).

  All of them run after Load returns. On a load the pathfinder itself is created inside `SaveRestore`
  (`gamegrid_serialize.cpp:304`), so nothing can queue a request to it before the first call.
  `runInline` is never set in the tree; if it were, the inline caller would read the same global.
- **Case 5** (`action_pf_astar.cpp:650`):
  `iVar2 = FastPaths::Engaged() ? FastPaths::Run(grid, ...) : grid.Run(...)`. Everything after it is
  retail's: the locked resize, the unlocked copy and the ready store. Row 961 is unchanged (F6 is a
  separate fix). For PFBENCH: the timer brackets this one statement.
- **Case 2** (`:635`): `if (FastPaths::Engaged()) FastPaths::NoteCellTerrain(*pbVar3, cmd.arg2);` runs
  before retail's store.
- **Fallback.** `Run` calls retail's `grid.Run` when the fast search returns Unsupported. That happens for:
  - a passable cell whose type is >= 32 (retail indexes past `stepCost[0x20]`);
  - a negative cost;
  - an f range wider than 2^16 buckets, or an int32 overflow risk;
  - an f outside the proven bucket window.

  The first such request in a game logs `[FastPaths] a request the fast search does not reproduce ...` once.
  None of these occurs with retail's four profiles.
- **Trace.** Each `SelectForGame` that selects anything other than retail writes
  `[FastPaths] selected for this game: fast=<0|1> shadow=<0|1>` to `standalone_error.log`. A game with the
  extension on logs it twice. The faithful profile logs nothing.

## 3. Semantics: retail rule -> our line

| # | Retail (action_pf_astar.cpp / action_pf_geom.cpp) | fast_search.cpp |
|---|---|---|
| R1 | Neighbour 0x1400a5ec0 direction order 0..7 (`geom:66`), bounds test (`geom:81`), 4 or 8 dirs (`astar:323-326`), no corner-cut test | `kDx/kDy` `:70`, bounds `:244`, `dirs` |
| R2 | walkable = (!respectBlocked or bit 7 clear) and bit (type & 0x1f) of passableTypes (`geom:85-88`) | the 256-entry `cost[]` table built per request `:137` |
| R3 | step = stepCost[type & 0x7f] * 10 orthogonal / * 14 diagonal (`astar:333-343`) | `:253` |
| R4 | the goal is tested on POP (`astar:314-316`); start == goal is a 1-cell path | `:230` |
| R5 | closed and g not strictly better -> skip; better -> un-close (`astar:347-350`) | `:255` |
| R6 | relax when not open or strictly cheaper (`astar:352-353`); re-key = new seq (Relax `:215`) | `:261`, `relax` lambda |
| R7 | pop min f, ties to max seq (AVL key, Insert `:164`; leftmost descent) | bucket ring + LIFO stacks, stale skip `:220` |
| R8 | no budget, no time limit, no partial path: empty open set = 0 (`astar:303-305`) | `:218` |
| R9 | Run: parent chain, start-first, indices < 0x4000 kept, count clamped; null buffer = unclamped (`astar:396-439`) | `:280` |
| R10 | endpoint types OR-ed into the mask by `Map::RequestPath` (`action_base_move.cpp:339`) before command 0 | inherited: the mask the worker holds already has them |

The heuristic is retail's unit-cost distance (Heuristic 0x1400a6030, `geom:101`, inlined at `astar:355-376`)
times the scale. With scale 1 it is retail's.

## 4. Why it is exact on cost, and the expansion numbers

- **Consistency.** Let s be the cheapest enterable step cost. One move changes the unit distance by at
  most 10 (orthogonal) or 14 (diagonal). A move costs at least s*10 or s*14. So h = s*H is consistent on
  every edge, and admissible because h(goal) = 0. Every path therefore has exactly retail's cost; only the
  choice among equal-cost routes can differ.
- **Monotone f.** Consistency also makes f non-decreasing, which is what the bucket queue needs.
  Live f values lie in [F, F + 14*(maxCost + s)]. With retail's profiles that window is at most 448, so the
  ring has 512 buckets.
- **Unit tests** (`tests/unit/ext_fast_paths_test.cpp`). The reference is a line-for-line port of
  Run / FindPath / Relax / Neighbour / Heuristic with a `std::set` under retail's key. The real
  `GridPathSearch` cannot link into `unit`.

  | Test | Cases | Result |
  |---|---|---|
  | scale 1 | 4000 random grids, sizes 3-64, all 4 profiles, 5 masks, walls, blocked bits, 4- and 8-neighbour | 0 mismatches in cells, count, cost, expansions and relaxations: the (f, LIFO) claim, proved |
  | real scale | 4000 | 0 cost mismatches; every path re-walked and its step costs summed; 0 cases expand more; same cells 3086 / 3920 found |
  | no path | 500 | agree, with equal expansions |
  | clamp | serpentine > 0x4000 cells | the clamp agrees |
  | unsupported | | the unsupported inputs are refused |

  **Measured expansion ratio (ours / retail):**

  | Grid | Ratio |
  |---|---|
  | Mixed random grids, overall | **0.625** |
  | ... citizen profile only | 0.560 |
  | Open ground 128x128, citizens, no roads | **0.010** (the heuristic is exact on uniform ground: a corridor) |
  | The same with 6 dirt-road lines (scale 3 against ground 4) | **0.354** |

  PATHRESEARCH predicted about 0.23. Real towns sit between the road and no-road rows: the more road, the
  weaker the scale relative to the ground the trip crosses.

## 5. Perf expectations for the benchmark (PFBENCH)

- **Per expansion.** No tree walk, no AVL delete or insert, one division instead of about 48, and 16
  bytes per cell instead of 24 in four dense arrays. Expect 2-4x per expansion.
- **Per request.** Multiplied with the ratios above: about 5-10x less worker time per request in a town
  with roads, 20x or more on open-ground trips.
- **Fixed per-request overhead.** The 256-entry cost table and the 32-type mask, and a 512-int bucket
  reset: well under 1 us.
- **One-time scan.** 65k-262k bytes on the first request of each game (tens of us).
- **Where it shows.** In the stress town only as worker busy %, search p95 and wait p95. Sim ms per
  iteration moves only if the queue was backing up (PATHRESEARCH §2).
- **Shadow mode.** `FastPaths::SetShadowArmed(true)` plus a `SelectForGame`, or call it while the game
  runs. Each request runs both searches again and scans retail's node stamps once (the `retailExpanded`
  count). It is a measuring mode, not a play mode.

## 6. Faithful zero-cost argument

- **Selection.** Under `/profile:faithful` (the gate) and by default, `Ext().fast_paths` is false and
  nothing arms the shadow, so `g_mode` = 0.
- **Case 5** costs one byte load through MinGW's `.refptr` indirection, a `test` and a not-taken `jne`
  (checked in the perf object: `action_pf_astar.cpp.obj`, `movzbl (%rax)` / `test %al` / `jne`), then
  retail's `call GridPathSearch::Run`.
- **Case 2** costs the same predicate before retail's store.
- **No other faithful change.**
  - Retail's search bodies (`FindPath`, `Relax`, `Insert` / `Delete`, `Neighbour`, `Heuristic`, `Run`) and
    `TakePath` are untouched.
  - `SelectForGame` is two calls per game load.
  - The worker scratch is a function-local static, never constructed in faithful.
  - No allocation, no log line, no save byte: a B extension writes its pseudo-mod and manifest only when
    active (EXTSAVE §3).
  - With this row registered, `SaveModsProvider()` now returns the provider outside the faithful profile.
    It reports 0 records while the extension is off. That was EXTSAVE's design for the first real B row.

## 7. What the coordinator verifies

1. **The faithful gate.** Invariants unchanged; no `[FastPaths]` line; resave golden 5/5.
2. **The all-on gate** (`/profile:all`):
   - boots, runs the stress town and quits clean;
   - `[FastPaths] selected for this game: fast=1 shadow=0` appears twice;
   - no `does not reproduce` line.
3. **EXTSAVE §4.3, live for the first time** (this is the first real B row):
   - save under `/profile:all` -> `[ExtSave] wrote the B / C manifest`;
   - reload -> locked from the manifest, the Extensions dialog shows "Faster pathfinding" locked;
   - load in retail -> one modsMissing for `ext:fast_paths`, and Yes loads.
4. **PFBENCH.** Wire `FastPaths_ShadowHook` and arm with `SetShadowArmed`. The shadow must show
   `pf_shadow_worse` = 0 and equal cost 100%.

## 8. Doubts

- **The type-count hook in case 2** is a second faithful predicate, beyond the one in case 5 the brief
  named. Without it the scale over passable types is 1 (type 11), and (a) gives nothing. The alternative,
  a per-request grid scan, costs more than the search.
- **Shadow timing order.** Shadow runs retail first, then ours, on a warm cache for the shared
  `cellFlags`. That flatters whichever runs second slightly. PFBENCH may want to alternate the order.
- **Scratch lifetime.** The scratch is a process-lifetime static (about 2.4 MB medium, 4.2 MB large, plus
  the entry pool), not freed with the Map. PATHRESEARCH asked for it to follow the Map. It is reused by the
  next game and freed at exit.
- **The double log line** per game (both `SelectForGame` calls). Harmless, slightly noisy.
- **Road tool and boat routes** use the same dispatch. Their plans can take a different equal-cost shape,
  which the player can see in the road tool's preview. Class B covers it, but it is the one place the
  divergence is visible without watching a citizen.
- **Cost-equal is not time-equal.** MoveTo's speed reads road types differently (`moveto.cpp:442`), so an
  equal-cost path with more road cells walks faster or slower than retail's. That is inherent to (a) and
  was accepted.
- **Shadow is folded into the seam commit**, not committed separately. The case-5 hook is the
  `kModeShadow` bit, so the faithful dispatch stays one predicate.
