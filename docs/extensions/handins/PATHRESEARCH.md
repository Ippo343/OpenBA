# PATHRESEARCH hand-in (2026-10-12)

Research only: no code, no build, no run. The ask (the user, verbatim): "research what pathfinding
improvements make sense and then implement. I've seen some good papers on A* on GPUs, we should
benchmark in the stress town to test." Target: one class B extension `ext.<id>` (DESIGN §2), built by
later agents at the seam INVENTORY §5.2 names, benchmarked faithful vs extension in the stress town.

Every time figure below is an ESTIMATE reasoned from the code. Nothing in the tree measures the
worker thread today (§5 adds that).

## 1. Retail's pathfinder as built

### 1.1 The grid and the cost model

| Fact | Value | Where |
|---|---|---|
| Grid | one cell per map unit, `Map::size` square; cell index = `y * size + x` | `Game/Map.h` (+0x00), `action_base_move.cpp:377` PathSetup |
| Map sizes | small 256, medium 384 (the stress town), large 512 | `Dialog/newgamedialog.cpp:591`; `[PfSeed] dim=` in archived runs |
| Cells | 65,536 / 147,456 / 262,144 | `size * size` |
| Per-cell worker state | one byte: terrain type (bits 0-6), blocked (bit 7) | `PathSearch.h` GridPathSearch +0xE0; seeded `pathfinder_seed.cpp:166-174` |
| Per-cell search node | 0x18 bytes for EVERY cell (3.5 MB medium, 6.3 MB large) | `pathfinder_seed.cpp:99`, `PathSearchNode` |
| Moves | 8-connected (`allowDiagonal` = the profile's road flag, 1 for citizens); no corner-cut test | `Neighbour` 0x1400a5ec0, `action_pf_geom.cpp:66` |
| Step cost | `stepCost[type of the cell entered] * 10` orthogonal, `* 14` diagonal | `FindPath` 0x14001d040, `action_pf_astar.cpp:340` |
| Heuristic | octile at unit cost: `10*(dx+dy) - 6*min(dx,dy)` (Manhattan / Chebyshev variants unused by citizens) | `Heuristic` 0x1400a6030, `action_pf_geom.cpp:101`; inlined `action_pf_astar.cpp:375` |
| Passable | bit `type` of `passableTypes` = the mover's flags OR the two endpoint types; `respectBlocked` is 0 for every MoveTo, so the blocked bit is ignored by citizens | `Map::RequestPath` 0x14009cfb0 `action_base_move.cpp:331`; `SetMovementCosts` `:234` sends `SetSearchFlags(road, 0, 0)` |

Movement profiles (`Map::SetMovementCosts` 0x14009e550, `action_base_move.cpp:234`), cost per terrain type:

| Type | 0 | 1 | 2 | 3 | 4 | 5-6 | 7 | 8 | 9 | 10 |
|---|---|---|---|---|---|---|---|---|---|---|
| meaning (from the writers) | road | road | open ground (removal, fields, graves write 2) | ? | ? | quarry pit (5), high ground (6) | shallow water | deep water | road / bridge | low ground |
| profile 0 (citizens) | 3 | 3 | 4 | 8 | 16 | 16 | 16 | 16 | 3 | 4 |
| profile 1 | 16 | 16 | 4 | 4 | 16 | 16 | 4 | 4 | 4 | 4 |
| profile 2 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 1 |
| profile 3 (boats) | 10 | 10 | 10 | 10 | 10 | 10 | 5 | 1 | 5 | 10 |

Type 11 is in the walkable set (0xe1f) and no profile sets it, so it keeps the ctor's cost 1. Types
from `ClassifyTerrain` 0x14009e2c0 (`Component/keystone_a_map.cpp:127-132`: 8 below -1.4 height, 7
below -0.25, 2 flat, 10 / 6 by height 2.0) and the `SetCellTerrain` writers. MoveTo's speed reads
types 1 and 9 as one road speed and type 0 as another (`moveto.cpp:442`).

### 1.2 The search

- **Algorithm.** Textbook A* from the mover's cell to the destination cell (`PathSetup` passes
  current-cell as `start`; `action_base_move.cpp:404-408`). Stops when the goal is POPPED. A closed node
  is reopened if a cheaper g turns up (never happens: the heuristic is consistent). **No node limit,
  no time limit, no partial result**: an unreachable goal inside the same zone exhausts the zone.
- **Open set.** An AVL tree keyed `(fcost asc, seq desc)`, i.e. min f, ties LIFO (the newest insert
  first), nodes from a chunk pool (`PathOpenSet::Insert` 0x14001d450 `action_pf_astar.cpp:164`;
  `PathOpenNode` 0x30 bytes). The minimum is found by walking to the leftmost node on every pop
  (`:300`). Decrease-key is a tree search for the old entry, an AVL delete and a fresh insert
  (`PathSearch::Relax` 0x14001d320 `:215`).
- **Stamps.** Per-cell open/closed are generation stamps, so nothing is cleared between searches.
- **Path out.** `GridPathSearch::Run` 0x14001ce00 (`:396`) walks the parent chain twice, into a 64 KB
  stack buffer (cap 0x4000 cells). `ApplyCommand` case 5 (`:647`) copies it into the result slot.
- **What the walker keeps.** `MoveTo` takes at most **0x100 cells** (`moveto.cpp:319`;
  `MoveTo::path[0x100]` is serialized, `moveto.cpp:163`). A longer trip walks 256 cells, finds the
  path exhausted (`:417`), and issues a NEW full request from where it stands (`:340`). A trip of
  L cells costs about L/256 searches.

### 1.3 The worker-thread protocol

- **One worker**, `PathfinderThread` (pinned 0x1F8 layout, `Game/PathfinderThread.h`), at `Map+0x68`.
- **One FIFO command queue** under one `CriticalSection`. Commands: 0 passable mask, 1 one step cost,
  2 cell terrain, 3 cell blocked, 4 search flags, 5 path request (`ApplyCommand` 0x1400a6430,
  `action_pf_astar.cpp:612`). Commands 0, 1 and 4 are sent only when the main thread's staged copy
  differs (`SetStepCost` etc.). **The search parameters are worker-global state set by the commands
  queued before the request**, so a request's meaning depends on queue order.
- **Run loop.** `Start()` wakes the thread; `Run` 0x1400a6360 pops and applies until the queue is
  empty, then the thread parks (`pathfinder_seed.cpp:62`). Every enqueue Starts it if parked.
- **Results.** `AllocResult` 0x1400a6240 hands the requester a `{slot, token}` ticket, growing the
  slot table under the lock. The worker sizes the slot's array under the lock, then copies the cells
  and sets `ready` UNLOCKED (row 961: retail's race with a concurrent table grow, kept).
- **Polling.** `MoveTo::Execute` checks the ticket every update (`moveto.cpp:294-315`); until `ready`,
  the citizen does not walk. `TakePath` 0x1400a6d50 (`action_base_move.cpp:296`) copies and frees
  the slot on the main thread, without the lock.
- **Synchronous users.** The road tool (`Map::FindRoadPath` 0x14009ce00) and boat routes
  (`QueryPath` 0x1400a6e30) enqueue and then SPIN on the main thread, yielding, until their result
  is ready (`keystone_a_zone.cpp:73-99`). They wait behind every citizen request already queued.
- **`runInline`** (+0x5C) is never set nonzero anywhere in the tree; every command is queued.

### 1.4 The region graph and `PathDistance` (main thread)

- **Zones** (`MapCell::zone`): a full-map 4-way flood fill over the walkable set 0xe1f, redone by
  `Map::FillZones` 0x14009c480 whenever the stale flag is set. The flag is set when a cell changes
  between walkable and the 0x1E0 set (`action_pf_roots_road.cpp:119`). `RequestPath` calls
  FillZones first (`action_base_move.cpp:332`): the first request after a building placement pays a
  147k-cell flood on the main thread. Zones reject cross-zone requests before they are queued.
- **Region graph** (`Map+0x70`, `RegionGraph`): 8x8-cell blocks, each block's walkable cells
  flood-labelled into regions with a centroid and an adjacency list; dirty blocks are rebuilt lazily
  (`Flush` 0x1400a8a90, `action_pf_region.cpp:699`). This is HPA*'s abstraction layer without
  intra-cluster costs.
- **`Map::PathDistance`** 0x14009f810 (`action_pf_roots_route.cpp:113`): synchronous, main thread.
  Region A* (Euclidean between centroids, `RegionGraph::FindPath` 0x14001dcd0) and the length is the
  sum of centroid distances (`PathLength` 0x1400a8ba0, `action_pf_region.cpp:746`). It ignores step
  costs and roads. Results are cached per region pair in a 0x20000-entry ring for 120 s of the
  per-iteration dt (`RegionPairCache::Expire`, `gameupdateloop.cpp:770`).
- **Who uses it**: 19 call sites (job choice, storage choice `PickBestDropTarget`
  `action_pf_roots_route.cpp:389`, market, leisure, housing, fire). Job and storage choice rank by
  this cost-blind distance; the walk then uses the cost-weighted grid A*.

### 1.5 Determinism

- Path CONTENT is deterministic: one worker, FIFO order, integer costs, a total order on the open set
  (f, then seq). The same request after the same command prefix yields the same cells.
- Path TIMING is not: which sim update first sees `ready` depends on the worker's wall-clock speed.
  Two retail runs of the same save diverge on any machine-speed difference. A class B extension
  cannot break a timing determinism retail does not have; it must keep content determinism (same
  inputs -> same cells) to stay debuggable.

### 1.6 Where the time goes (reasoned, not measured)

Per expanded node, the worker does: a leftmost walk, an AVL delete with rebalance, and for each of 8
neighbours `Neighbour()` (2 integer divisions) plus 4 more `%` / `/` by `width` in the cost and
heuristic (`action_pf_astar.cpp:334`, `:365`). That is ~48 integer divisions per expansion, plus a
tree search, a delete and an insert for every improved open neighbour. Search nodes are 24 bytes
strided over the whole map, so every neighbour touch is a likely cache miss. Estimate: 0.3-0.6 us per
expansion on a current desktop CPU.

**The heuristic is weak by a factor of 3-4.** It prices a step at 10, but the cheapest citizen step
costs 30 (road) and open ground 40. A* with h = c*/4 expands an egg-shaped region around the start
instead of a corridor. Counting the cells with `g + h < C*` on open ground (cost 4, a straight trip
of L cells):

| Trip length L (cells) | retail h (scale 10) | h scaled by the cheapest passable cost (30) | exact-cost corridor |
|---|---|---|---|
| 32 | ~1,570 expansions | ~360 | ~L, a few dozen |
| 64 | ~6,270 | ~1,420 | ~L |
| 128 | ~25,100 | ~5,700 | ~L |

The retail column grows as L squared; at 0.3-0.6 us per expansion, a 64-cell trip is ~2-4 ms of worker
time. Typical requests: home to workplace or storage in a medium town, 20-80 cells: roughly 600 to
10,000 expansions. Worst case: a goal reachable by zone but not under the mover's passable mask
floods the whole zone (up to ~147k nodes medium, ~50-90 ms) while every later request waits.

Main-thread costs: the FillZones reflood after a walkable change, region A* in `PathDistance` on a
cache miss, and the road-tool / boat spin behind the queue.

## 2. Where the player feels it

**What the code shows:**
1. **Queueing.** One worker, FIFO. At speed 10 with hundreds of movers, requests arrive ~10x faster
   per wall second while each search is still milliseconds. A citizen whose ticket is not ready
   stands still (`moveto.cpp:315`). Hypothesis to measure: the queue is the citizens' idle time in
   large towns at high speed.
2. **Main-thread hitches.** The road tool and boat routes spin behind the whole queue
   (`keystone_a_zone.cpp:93`). The first request after a placement refloods all zones.
3. **Long trips replan every 256 cells**, each a full search to the far goal (`moveto.cpp:319`).
4. **Weak road preference.** Both roads cost 3, ground 4 (25% cheaper), while the community wiki
   gives roads +17% (dirt) and +37% (stone) speed. Stone and dirt are identical to the planner, so
   a stone road "doesn't pull" traffic more than dirt. Diagonal shortcuts off a road win often.
5. **Distance-blind choices.** Job and storage choice use centroid distances that ignore roads and
   terrain cost (§1.4), so a citizen can pick a storage that is closer as the crow flies but slower
   to walk to.
6. **No replanning when blocked**: citizens ignore the blocked bit (`respectBlocked` 0); a path
   through a cell that a new building claims is followed as computed.

**Folklore (community reports, not verified against the code):**
- Roads: stone +37%, dirt +17% speed; citizens "skip" roads when it is faster and diagonal crossings
  mislead them ([Banished wiki, Stone road](https://banished.fandom.com/wiki/Stone_road),
  [Dirt road](https://banished.fandom.com/wiki/Dirt_road),
  [Steam thread](https://steamcommunity.com/app/242920/discussions/0/343787283746497988)). Matches
  point 4.
- Citizens march across the map to a distant barn
  ([r/Banished](https://redlib.ssps.io/r/Banished/comments/1kjtudb/how_do_i_fix_logistics),
  [Steam](https://steamcommunity.com/app/242920/discussions/0/2565312625309764626)). Consistent
  with point 5, and with job choice made once by distance and kept.
- "Lag with large populations"
  ([review summary](https://vaporlens.app/app/242920/banished.md)): unattributed; no source names
  pathfinding.

## 3. Literature survey

| Technique | Key paper | Regime it targets | Verdict here |
|---|---|---|---|
| **GPU A\*, one big search** (GA*) | Zhou & Zeng, "Massively Parallel A* Search on a GPU", AAAI 2015 ([AAAI](https://ojs.aaai.org/index.php/AAAI/article/view/9367)) | ONE huge search (puzzles, very large graphs); many priority queues + parallel hash dedupe; up to ~30-45x over 1 CPU thread | **No.** Our searches are 10^3-10^4 nodes: a kernel launch plus readback exceeds the whole CPU search. |
| **GPU, many agents at once** | Bleiweiss, "GPU Accelerated Pathfinding", Graphics Hardware 2008 ([EG diglib](https://diglib.eg.org/items/c290217d-2341-42b1-9eec-3ec6b9c02ef8/full)) | thousands of agents, one thread per query, small roadmap graphs; >10x query throughput | **Benchmark only.** Throughput fits; latency, batch size and determinism do not (below). |
| **GPU, grid decomposition** | "GPU Accelerated Multi-agent Path Planning Based on Grid Space Decomposition", ICCS 2012 ([ScienceDirect](https://www.sciencedirect.com/science/article/pii/S1877050912003249)) | many agents on grids; gives up optimality | No: suboptimal, and the same latency problem. |
| **Jump Point Search / JPS+** | Harabor & Grastien, AAAI 2011 ([PDF](https://harabor.net/data/papers/harabor-grastien-aaai11.pdf)); "Improving JPS", ICAPS 2014 ([AAAI](https://ojs.aaai.org/index.php/ICAPS/article/view/13633)) | uniform-cost 8-connected grids; JPS+ precomputes jump distances | **No.** Our grid has 5+ cost classes; JPS's symmetry pruning is invalid. JPS+ precompute is invalidated by every road. |
| **Weighted JPS (JPSW)** | Carlson et al., "Optimal Pathfinding on Weighted Grid Maps", AAAI 2023 ([AAAI](https://ojs.aaai.org/index.php/AAAI/article/view/26458)) | per-cell weights, online, optimal, up to 10x over A* | **Later, maybe.** Its cost model (cell-averaged edges) differs from ours (cost of the entered cell); porting means re-deriving the pruning rules. High risk for a first build. |
| **HPA\*** | Botea, Müller & Schaeffer, J. Game Dev. 2004 ([PDF](https://webdocs.cs.ualberta.ca/%7emmueller/ps/hpastar.pdf)) | long paths on big maps; up to 10x, ~1% suboptimal | **Partly already there.** The region graph is HPA*'s cluster layer without crossing costs. A corridor-restricted grid search over it is candidate 3's sibling; skipped for now: the graph is main-thread owned and cost-blind. |
| **Flow fields / Dijkstra maps** | Emerson, "Crowd Pathfinding and Steering Using Flow Field Tiles", Game AI Pro ([T&F](https://www.taylorfrancis.com/books/9780429100277/chapters/10.1201/b16725-29)) | many agents to ONE goal | **No for now.** Our goals are specific cells (interaction nodes, adjacent cells), not a few shared targets; a field per (goal, profile, mask) must be rebuilt on every terrain edit. Revisit if §5's logs show goal concentration. |
| **Theta\* / any-angle** | Nash, Daniel, Koenig & Felner, JAIR 2010 ([arXiv](https://arxiv.org/abs/1401.3843)) | visually straight paths | **Quality, not speed.** The walker steps waypoint-to-cell (`moveto.cpp:464`); any-angle legs cross mixed terrain and change the cost model. A post-pass string-pull is cheaper if the zigzag matters. |
| **Bidirectional (MM)** | Holte, Felner, Sharon & Sturtevant, AAAI 2016 ([AAAI](https://ojs.aaai.org/index.php/AAAI/article/view/10436)) | weak heuristics, deep searches | **No.** Fixing the heuristic (below) removes most of what it would buy, at twice the bookkeeping. |
| **Better open set** | Dial's bucket queue (CACM 1969); radix heaps | integer costs, bounded edge weights | **Yes.** Costs are integers and f is monotone; a circular bucket array with a LIFO stack per bucket reproduces retail's (f, LIFO) pop order exactly. |
| **Tighter admissible heuristic** | standard A* theory (consistency of a scaled octile) | any weak h | **Yes.** Scale h by the cheapest passable step cost: still admissible and consistent, paths stay cost-optimal, 4x+ fewer expansions on ground. |
| **D\* Lite / LPA\*** | Koenig & Likhachev, AAAI 2002 ([PDF](https://idm-lab.org/bib/abstracts/papers/aaai02b.pdf)) | one agent replanning in a changing map | **No.** Retail never replans on blockage, and per-agent search state for 800 citizens is far more memory than the searches cost. |
| **More worker threads** | (engineering) | throughput when a queue backs up | **Yes, second.** Byte-identical paths if each worker owns its scratch and the cell-edit commands are a barrier. |
| **ALT landmarks / path cache** | Goldberg & Harrelson, SODA 2005 ([MSR](https://www.microsoft.com/en-us/research/publication/computing-the-shortest-path-a-search-meets-graph-theory/)) | static graphs, many queries | **No.** Roads lower costs constantly, which makes stale landmark bounds inadmissible; ~9 MB per profile for 16 landmarks on a medium map. A path cache is weak because goals are specific cells. |

**GPU A\*, honestly.** Each request is small (10^3-10^4 expansions, ~0.1-3 ms on the CPU after
candidate 1, less than a frame) and a citizen waits on it, so latency matters more than throughput.
A D3D11 compute dispatch plus a readback costs at least a frame of latency (7 ms at 140 fps)
unless polled asynchronously. To amortise that, a batch needs hundreds of concurrent requests; the
stress town issues on the order of tens per frame. The device belongs to the render thread: the
immediate context is not thread-safe, so the worker cannot dispatch; it would hand batches to the
renderer. The renderer we own (`src/render/dx11/`) has no compute path today (no
`CreateComputeShader` in `src/`). Under Linux the stress town runs on lavapipe (CPU Vulkan), where a
GPU path is slower by construction. Parallel GPU A* breaks ties by atomic order, so the same
request can yield different equal-cost paths across runs and vendors. To be worth shipping it
would have to show, in the stress town, lower request latency at p95 than the CPU candidate AND a
worker-thread reduction that the CPU pool (candidate 2) cannot match; neither looks reachable at
these sizes.

## 4. Ranked recommendation

All three share one seam (INVENTORY §5.2): a once-per-game selection stored OUTSIDE the pinned
`PathfinderThread` (a null-by-default static pointer, set when the B lock is taken at new game /
load, before the first command 5), read once per request in `ApplyCommand` case 5:
`grid.Run(...)` or the extension's body. No toggle inside `FindPath`, `Relax`, `Insert` / `Delete`,
`Neighbour`, `Heuristic` or `TakePath`. None of the three changes `Map::PathDistance`; none adds
per-cell or per-path saved state, so all are class B.

### 4.1 FIRST: `ext.fast_paths` — tight heuristic + bucket queue + division-free neighbours

- **What.** A new search body over the same inputs (the worker's `stepCost`, `passableTypes`,
  `allowDiagonal`, `cellFlags`, `width`), its own scratch outside the pinned layout:
  (a) h = octile x `minCost`, with `minCost` = the smallest `stepCost[t]` over the passable types,
  computed once per request; (b) a circular bucket queue sized `14 * maxCost + 15`, one LIFO stack
  per f, stale entries skipped lazily; (c) neighbours by offset from one `(x, y)` per expansion, no
  division; (d) compact SoA node arrays (g, parent, stamp).
- **Expected win.** Expansions down 4x on open ground, more on road trips (§1.6); cost per
  expansion down 2-4x (no tree, no divisions). Worker time per request 8-20x lower. Visible in the
  stress town only as worker time and queue wait unless the queue was backing up (§5 decides).
- **Byte-identical?** Not with (a): equal COST, possibly different cells, because the expansion
  order changes. With (a) off, (b)+(c)+(d) reproduce retail's pop order exactly (same f, same LIFO
  tie-break, same closed-reopen rule) and so the same cells; the shadow check in §5 proves it per
  request. Build it in two steps: (b)-(d) first, verified identical; then (a).
- **Why acceptable for a locked B.** Retail's A* is cost-optimal (admissible, consistent h); so is
  this one. Every path has exactly retail's cost; only the choice among equal-cost routes moves.
  The sim diverges (different cells, different terrain speeds along the way), which is what class B
  and the lock exist for.
- **Determinism.** Single worker, FIFO, integer arithmetic, total pop order: same inputs, same cells.
- **Fallback.** If `maxCost * 14 + 15` exceeds the bucket array (it cannot with profiles 0-3: max 16)
  or the flags select Manhattan / Chebyshev, call retail's `grid.Run`.
- **Risk.** Low. One seam; the faithful body untouched. Scratch lifetime must follow the Map (free
  it where `~PathfinderThread` runs, `keystone_a_gamedtor.cpp`). Row 961's race is unchanged (case
  5's copy stays retail's).
- **Effort.** 2-3 agent-days including the §5 instrumentation and the shadow check.
- **Why first.** It attacks the dominant term (the heuristic) at the one seam, keeps the cost
  contract, needs no threading, and step one is provably identical, so the benchmark has a clean
  control.

### 4.2 SECOND: `ext.path_workers` — a worker pool (composable with 4.1)

- **What.** The dispatcher still pops the FIFO queue and applies commands 0, 1 and 4 to a "current
  parameters" record in order. A command 5 snapshots those parameters into the request and hands
  it to one of N workers (N = cores - 2, max 4), each with its own scratch. Commands 2 and 3
  (cell edits) are a barrier: wait for in-flight searches, apply, continue. Cell edits are rare next
  to requests.
- **Expected win.** Queue wait divided by ~N when the queue backs up; nothing when it does not.
  Synchronous road-tool / boat queries stop waiting behind citizens if they get a priority lane.
- **Byte-identical?** Yes for content (each request sees exactly retail's parameters and cell state);
  only timing changes, and retail's timing is already machine-dependent (§1.5). Arguably class A;
  kept as B because the arrival tick of every path changes the sim.
- **Risk.** Medium. Results must be published under the lock (the extension's own case 5, not
  retail's racy copy, which N writers would widen); memory N x ~2-3.5 MB on a medium map.
- **Effort.** 3-4 agent-days.

### 4.3 THIRD (benchmark only, not shipped): a GPU batch prototype

The user wants numbers, so measure it rather than argue it. Offline, not in the game: dump the
requests and the cost state from a stress run (§5, the `pathlog`), then replay them in a standalone
bench that links the extension body, retail's body, and a D3D11 compute prototype
(Bleiweiss-style: one thread per request, a per-thread binary heap in groupshared or device memory,
batched per sim frame). It reports per-request latency including upload, dispatch and readback,
batch throughput, and path-cost equality. Effort 5-8 agent-days. It does not touch the game, the
seam or the save. Ship only if it beats 4.1 + 4.2 at p95 latency on the logged stress batches.

**Considered and not ranked (gameplay, the user's call, §6):** speed-proportional step costs (stone
cheaper than dirt), and a cost-aware `PathDistance` so job and storage choice match the walk.

## 5. Benchmark plan

DIRECTION.md §2.2 ("no new instruments") was the port phase's rule. The extensions phase measures
(DESIGN rule 6), so the new numbers go into the existing stress harness and nowhere else: no new
tool, no new `BANISHED_*` flag. Gate them on `Opt().stress` (the harness is already on); a level of
the existing `stressCompProf` knob can enable the shadow check (§6 asks).

**Where the timers live** (retail's inner loops untouched):
- On the worker, in `ApplyCommand` case 5 only: QPC/RDTSC before and after `grid.Run` (or the
  extension's body). Expansions = a counter the extension body returns; for retail, the open-set
  pool's `total`/`inUse` deltas or `sequence` (the insert count, `PathSearch +0x04`) read AFTER
  `grid.Run`, not inside FindPath.
- Enqueue time: stamped in the request's command node when `RequestPath` 0x1400a6c10 queues it.
  `PathCommand` is a fixed 0x20 layout, so the stamp goes in a side table keyed by result slot.
  Wait = start-of-search minus enqueue. Queue depth = `queueCount` sampled at each enqueue.
- Main thread: brackets around `Map::PathDistance` and `Map::FillZones` call sites in the harness's
  existing phase-timer style (`PhaseTimer`, `gameupdateloop.cpp:769`), and around the synchronous
  `RunPathQuery` spin.
- Accumulators are worker-local and drained under the existing lock when the CSV row is emitted.

**New CSV columns** (appended to `standalone_stress.csv`, one row per interval as today):

| Column | Meaning |
|---|---|
| `pf_requests` | command-5 requests completed in the window |
| `pf_req_per_sim_s` | requests per simulated second |
| `pf_search_us_p50`, `pf_search_us_p95`, `pf_search_us_max` | worker wall time per request |
| `pf_expanded_mean`, `pf_expanded_p95` | nodes expanded per request |
| `pf_wait_us_p50`, `pf_wait_us_p95` | enqueue-to-start wait |
| `pf_queue_max` | deepest queue seen |
| `pf_fail` | requests that returned 0 cells |
| `pf_worker_busy_pct` | worker busy time / window wall time |
| `pf_len_mean` | cells per result path |
| `pf_shadow_equal_cost`, `pf_shadow_same_cells`, `pf_shadow_worse` | shadow mode only (below) |
| `pf_sync_wait_ms` | main-thread time spent in road-tool / boat spins |
| `ph_pathdistance_ms`, `ph_fillzones_ms` | main-thread costs per frame |

**Report lines** (`tools/stress_report.py`, existing tool, new section):
```
PATHFINDING  requests 412/window (41.2 per sim s)  worker busy 37%
  search us   p50 310  p95 2900  max 61000      expanded mean 2150 p95 14800
  wait us     p50 40   p95 9000  queue max 63   failed 3
  shadow      equal cost 100.0%  same cells 71.4%  worse 0
  main        PathDistance 0.02 ms/frame  FillZones 0.00  sync spin 0.0 ms
```
(numbers illustrative.) The baseline diff adds `pf_search_us_p95`, `pf_wait_us_p95` and
`pf_worker_busy_pct`; a faithful-profile regression on `sim ms / iteration` stays a defect.

**Shadow mode: the decisive comparison.** Because path timing makes two runs diverge (§1.5), an
A/B of two runs compares two different towns. In shadow mode the worker runs retail's
`grid.Run` (whose result drives the game) AND the extension body on the same inputs, on its own
scratch, and records both times, both expansion counts, both path costs (sum of step costs along
the cells) and whether the cells match. Paired, same-input numbers; `pf_shadow_worse` must be 0
(an extension path costing MORE than retail's is a bug). Step 4.1(b)-(d) must show same cells 100%.

**The path-heavy scenario.** The default stress town houses 800 citizens beside their work and is
probably not path-bound. Make requests the load with existing knobs first: `--map large`,
`--citizens 1600`, `--sets 1` (fewer, farther workplaces), `--speed 10`. If that is still not
path-bound (worker busy < 50%), the town builder needs a "spread" layout (houses on one side of
the map, workplaces and storage on the other: every trip 150+ cells, so every trip crosses the
256-cell re-request boundary on large maps), which is a harness change to `starttown.cpp`'s
siting, not a flag. Run each pair 3 times.

**Request log** (for 4.3 and offline regression): in shadow mode, append each request's
`{start, goal, passableTypes, flags, stepCost[0..10]}` and, at each cell-edit command, the edit,
to `standalone_pathlog.bin`. The replay bench rebuilds the exact worker state.

**The A/B pair.** Faithful: `STRESS_PROFILE=faithful`. Extension: no profile, `settings.cfg` with
`fix.*=0` and `ext.fast_paths=1` (the run script edits and restores settings.cfg the way it already
does for `GameAutoPause`), so the only difference is the extension. Decide on: worker busy %,
search p95, wait p95, sim ms / iteration unchanged or better, shadow worse = 0.

## 6. Open questions for the coordinator

1. **Equal-cost but different paths inside a locked game: acceptable?** (4.1a.) If not, ship only
   4.1(b)-(d) and 4.2, which keep retail's cells exactly; the win drops from ~8-20x to ~2-4x per
   search.
2. **Is GPU a hard requirement**, or is a benchmark-only GPU prototype (4.3) enough to show the user
   the numbers?
3. **Class of a byte-identical speedup.** 4.1(b)-(d) and 4.2 produce retail's cells; only timing
   moves. Class B (recorded, locked) as proposed, or class A (live)?
4. **The shadow check's switch.** Reuse a level of `BANISHED_STRESS_COMPPROF` (existing flag), or does
   the user allow one stress-only knob?
5. **The spread scenario.** May the stress town builder grow a second layout?
6. **Gameplay items** (not in this plan): speed-proportional road costs (stone 3 -> 2?) and a
   cost-aware `PathDistance` for job / storage choice. Both are class B and change how towns play;
   only the user can want them.
7. **Profile for a single-extension A/B**: the run script's settings edit (§5), or a
   `/profile:only:<id>` switch in the registry?
