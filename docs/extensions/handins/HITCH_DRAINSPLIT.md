# HITCH_DRAINSPLIT -- the PathGraph drain in two frames, `ext.drain_split` (class B, default OFF)

Block DRAINSPLIT, 2026-10-18. Branch `drainsplit` from `hitch` `9ecc6e4fb`. The extension the
pairspread agent proposed (HITCH_PAIRSPREAD section 4): the one CitizenList hitch row left after the
PathGraph fixes is the FIRST drain of a game (frame 749 at speed 50, 7.30 ms over the 7.14 ms bar), where
`ReassignWorkers` (3.3-4.1 ms, cold residence-to-residence searches) and `AssignCitizens` (1.5-2.1 ms) run
in one tick. Not merged.

| Commit | What |
|---|---|
| `937eac30e` ext | `src/ext/drain_split/`, the registry row and `Ext::LockEpoch`, the three seams, `[Stress::drainsplit]`, three unit tests |
| (this commit) docs | this hand-in, the INVENTORY row E7 |

## 1. What the extension does, and the one change from the task

- **ReassignWorkers stays in the drain tick.** `AssignCitizens` is *owed*.
- **The owed half runs in the first `CitizenBehavior::Update` of the NEXT FRAME**, at the drain's own
  place in that Update: after `CountResources` and `CheckThresholds`, before the path-graph branch.
- **Not the next sim tick, as the task said.** At speed N a frame holds N ticks, so the tick after the
  drain is in the same frame N-1 times out of N. The frame would carry both halves again, and the hitch
  would stay. "The next frame" is the version that moves the work out of the hitch frame.
  - At speed 1 the next frame is the next tick: the task's version exactly.
  - At speed 50 the gap is 1 to 50 ticks (up to 0.83 sim-seconds), but always one frame of real time,
    about 7-16 ms. That is inside the user's class-B rule (memory "class B lag acceptance").
- **One exception.** If the path-graph branch is due while the debt is still pending, the owed half runs
  first, in that tick. It cannot happen in a normal run, see section 2.

## 2. What the two halves share, and what the gap can change

**What `AssignCitizens` reads** (`citizen_assign_citizens.cpp`, retail 0x1400d43b0). It reads every
input fresh when it runs:
- the town's citizen list (`town+0xa0`), and for each citizen its age and description, school, home,
  workplace and profession;
- the profession -> workplace map (`town+0x100`) and each workplace's live roster (`WorkPlaceComponent+0x60`);
- the PathGraph's region-pair cost map (`PathGraph+0x18`), through `RegionPairCost` (0x1400d6060).

It keeps nothing from one call to the next. Its scratch containers are local.

**What `ReassignWorkers` hands it.** Nothing but the world. `ReassignWorkers` (0x1400d62a0) re-homes the
occupants of single-household residences (`SetHome`, 0x1401163d0) and moves their goods. Through the world
that changes the home cells `AssignCitizens` prices commutes from. No member of the PathGraph is read or
written by `ReassignWorkers`.

**The cost map is the one state that matters, and nothing writes it in the gap.**
- Only three bodies touch it: `ProcessPaths` (fills it), `BuildPathGraph` (clears and re-lists it) and
  `RegionPairCost` (stores a miss).
- `ProcessPaths` and `BuildPathGraph` run only from the path-graph branch of `CitizenBehavior::Update`.
- After a drain, retail's clamp leaves the accumulator below the 300 s interval. So the branch is not
  entered again for about 300 sim-seconds, long after the debt is paid.
- The owed half still runs before the branch in any tick that would enter it (`Take`'s `pathBlockRuns`).
  So a rebuild can never clear the map under a pending `AssignCitizens`.
- `PathPairs_Clear` (0x140025b30) stays in the drain tick. It resets only the pair count, which
  `AssignCitizens` never reads.

**What the sim can change in the gap**, between the drain's `CitizenBehavior::Update` and the owed one.
- The rest of the drain's frame, and the start of the next: the tutorial counters, the frame's
  achievement check, the UI and the player's tools, the save / load step, and in every following tick
  `RegionPairCache::Expire`, the animation, `EntityList::Update` (every citizen's AI), the calendar and
  the weather.
- So a citizen can die and leave the list, be born, move home, take or lose a school, a workplace or a
  profession through its own AI. The player can demolish a workplace or a house.
- **None of that breaks a precondition of `AssignCitizens`.** Retail's own drain tick is preceded by ticks
  that make exactly these changes. Its only preconditions are the ones every `CitizenBehavior::Update`
  meets. So the owed half is handled the way retail handles any drain: it sees the world as it is when it
  runs. Nothing is replayed, guarded or patched.

**What a consumer can observe.**
- **Seats one frame late.** Idle citizens are seated, re-seated or stripped of a workplace (Phase 5B) one
  frame later. In that frame they keep the workplace and profession they had after `ReassignWorkers`.
- **The candidate set moves.** The idle set and the commute costs are those of the run's tick:
  - a citizen who became idle in the gap is a candidate, one who stopped being idle is not;
  - a citizen re-homed in the gap is priced from the new home;
  - a citizen who died is not in the list.
- **Workplace counts in the gap** read the pre-assignment rosters: anything that reads a workplace's
  roster or a citizen's workplace in the gap, the town hall's profession counts included, sees the
  state after `ReassignWorkers` and before `AssignCitizens`. Retail never shows that state.
- **The random stream.** `AssignCitizens` draws no random number. But the gap's AI decisions see
  different seats, so the sim diverges from the gap on. That is why the row is class B.

**Across games.**
- **Not serialized.** The PathGraph's pairs, pending index and cost map are not serialized
  (`citizenlist_serialize.cpp` writes only the accumulator `+0x190`), and neither is the debt.
  - A save written in the gap loads without that drain's `AssignCitizens`. The next drain, 300 s later,
    runs one.
  - This is the same as retail's load dropping an unfinished `ProcessPaths` window. The continuing game
    is unaffected. At speed 50 the gap spans a frame boundary, where the save step runs, on every drain;
    at speed 1 as well.
- **The epoch.** The debt is tagged with `Ext::LockEpoch()`, new in the registry. It advances on every
  `Install`, `LockForGame`, `LockForGameWithSet` and owning `UnlockForGame`.
  - A load, a new game or the return to the menu between the halves therefore DROPS the debt.
  - A PathGraph that the next world allocates at the same address cannot collect it.
- **The toggle while pending.** As a class B row it is locked for the game, so the dialog's toggle only
  records the request. Only a new lock or an Install changes the active value, and both drop the debt.

## 3. The extension

- **Files.** `src/ext/drain_split/drain_split.{h,cpp}` hold the bookkeeping, with no game headers, and are
  unit tested. `drain_split_town.cpp` is the game side, exe only: `DeferAssign`, `RunDue` and the gap
  count.
- **The row.** `drain_split`, "Split the job reassignment": `Tier::Extension`, `Class::B`, default off,
  formatVersion 1, pseudo-mod `ext:drain_split`, settings key `ext.drain_split`.
- **No icon.** The 256 x 256 Extensions sprite sheet is full, as for `async_save_compress`. The row shows
  without a pictogram. Growing the sheet is the user's call.
- **Seams**, all marked `EXT SEAM ... NON-RETAIL`:
  - `GameUpdateLoop::Update`: `DrainSplit::BeginFrame()` beside `SpreadWarmth::BeginFrame()`. It counts
    frames and reads the row once per frame.
  - `PathGraph::ProcessPaths`' drain: `split = DrainSplit::g_on && DeferAssign(...)`, then
    `if (!split) AssignCitizens()`. Off, that is one bool test per drain.
  - `CitizenBehavior::Update`, before the path-graph branch:
    `if (DrainSplit::g_pending) RunDue(pathGraph, branchDue)`. Off, that is one bool test per tick, and
    nothing sets `g_pending` while the row is off.
- **An overlap** (a drain while a debt is pending) is unreachable by section 2. If it ever happened, the
  drain would run its own `AssignCitizens`, retail's drain, and count it in `overlaps`.
- **The harness.**
  - `[Stress::pathgraph]` is printed by the owed half, so its `assign=` is the deferred run.
  - A per-split `[Stress::drainsplit] owed half:` line gives the citizens at the drain and at the run,
    those gone, and those changed (home / work / profession / idle).
  - The cumulative `[Stress::drainsplit] frame=...` line is printed per harness row: splits, runs, forced
    runs, drops, overlaps, gap ticks and the gap's citizen changes.
  - The snapshot is a pointer -> {home, workplace, profession, idle} map taken after `ReassignWorkers`,
    only while the harness runs. A citizen gone in the gap is never dereferenced.

**Unit tests** (`tests/unit/ext_drain_split_test.cpp`):
- `ext_drain_split_registry_row`: class B, format 1, default off, locked, the pseudo-mod.
- `ext_drain_split_lock_epoch`: every lock change advances the epoch; a foreign unlock and a refused
  `LockForGameWithSet` do not.
- `ext_drain_split_owed_half`, the debt's life:
  - off owes nothing;
  - the debt waits out the drain's frame, at speed 1 too, and the path-graph branch forces it;
  - an overlap is refused;
  - another PathGraph, a load (`LockForGameWithSet`), the menu plus a new game, and an Install that
    turns the row off each drop it;
  - the dialog's toggle under the lock does not.

## 4. Proof

- **The exe.** Every leg is speed 50, the fed stress town, `--lock drainsplit`. Every leg built and
  deployed the same exe, md5 `81e464ca2b08...`, from this branch.
- **Profiles.** Every leg runs `STRESS_EXT=...`, so the fixes are off.

**Extension OFF, COMPPROF=4**: `perf/results/20261018_131001_drainsplit_verify_off`, 2,400 frames, CLEAN.

| shadow | result |
|---|---|
| `pathgraph_assign_seq` | 6 drains, 0 mismatches |
| PathLengthMemo, PathCostIndex, PathDistance, TickList, FrustumCull and every other | 0 mismatches |
| `[Stress::drainsplit]` lines | none: the row is off, nothing is owed |

**Extension ON, COMPPROF=4**: `20261018_133807_drainsplit_verify_on`, 2,400 frames, CLEAN.
- Every shadow is still at 0 mismatches, `pathgraph_assign_seq` included: 6 drains.
- That shadow lives inside `AssignCitizens`, so it holds wherever the call runs.
- The divergence is in WHEN the allocator runs and what the world is at that moment. No shadow can see
  that, because no retail twin of the late run exists.

**The divergence, counted.** These are the cumulative `[Stress::drainsplit]` lines at frame 12000.

| leg | splits | runs | forced | dropped | overlaps | gap ticks: sum, max | citizens at the drains | gone | born | changed |
|---|---|---|---|---|---|---|---|---|---|---|
| `..._drainsplit_ext` | 33 | 33 | 0 | 0 | 0 | 767, 35 | 34,633 | 0 | 1 | 0 |
| `..._drainsplit_ext_h2` | 33 | 33 | 0 | 0 | 0 | 767, 35 | 34,791 | 0 | 0 | 0 |
| `..._drainsplit_verify_on` (2,400 frames) | 6 | 6 | 0 | 0 | 0 | 198, 35 | 5,443 | 1 | 0 | 1 (work + profession) |

- **Each owed half ran in the next frame**, 1 to 35 ticks after its drain, 23 on average. The gap is
  fixed by the drain's tick offset in its frame.
- **No forced run, no drop and no overlap** happened in any leg.
- **Almost nothing changes in the gap.** In 101,000 citizen-gaps, two citizens' inputs changed: one left
  the list and one changed workplace and profession. The divergence is that the seats are taken one frame
  later from essentially the same candidates. The sim then diverges through what the AI does in that frame.

## 5. Measurements

**The drain frames.** The two legs below ran with `BANISHED_STRESS_HITCH_MS=2`, the existing option, so
that the drain frames are recorded. At the default bar of 7.14 ms, neither pair showed any drain row, in
either leg:
- ext `20261018_131328_drainsplit_ext`, BUSY;
- base `20261018_131943_drainsplit_base`, BUSY.

So the frame-749 row of HITCH_PAIRSPREAD, 0.16 ms over the bar in one leg of two, did not reappear in
this pair either. Hitch CSV frame = `worldFrame + 1`.

| drain | base `20261018_133152_drainsplit_base_h2` (BUSY): excess / CitizenList ms | ext `20261018_132549_drainsplit_ext_h2` (CLEAN): excess / CitizenList ms |
|---|---|---|
| 1st (749) | 5.18 / 5.19 | 749: 3.69 / 3.73 (ReassignWorkers); 750: under 2 ms excess (the owed AssignCitizens, 1.61 ms) |
| 2nd (1109) | 3.50 / 3.04 | 1109: 2.17 / 1.62; 1110: 2.49 / 1.71 |
| 3rd (1469) | 3.58 / 3.06 | both halves under 2 ms excess |
| 4th (1829) | 3.29 / 3.13 | both halves under 2 ms excess |
| 5th (2189) | 3.25 / 2.85 | 2190: 2.50 / 2.24 (AssignCitizens alone; ReassignWorkers 0.42 -> 0.1 ms) |
| from ~2550 on | 2.0-4.0 / 1.9-2.9 | the same, one frame later: 2.0-3.6 / 1.8-2.8 |

**The drain's halves**, from `[Stress::pathgraph]`. They are the same in both legs, because the split
moves the work and does not change it:

| drain | ReassignWorkers ms | AssignCitizens ms |
|---|---|---|
| 1st | 3.57-3.61 | 1.61-1.62 |
| 2nd-5th | 1.15-1.49 | 1.59-1.95 |
| from ~2550 on (no empty residence left) | 0.09-0.11 | 1.58-2.0 |

**What this buys.**
- **The first drain frame loses AssignCitizens' 1.6 ms**: 5.2 -> 3.7 ms of CitizenList. The drains of
  the next three minutes are split into two frames of 1.3-1.7 ms each.
- **From the sixth drain on, the split buys nothing.** ReassignWorkers is 0.1 ms once no empty residence
  is left, so the frame cost is AssignCitizens alone, moved one frame later.
- **On the drain frame itself, a frame of the stress town is ~9-11 ms** (update 8.5-10 ms). The worst
  drain frame, the first, is now 3.7 ms over its median, against 5.2 ms. In the earlier legs it was
  7.3 ms over a different median, because the bar is relative.

**Per-iteration cost** (`update_ms_per_iter`, mean of rows 2-20):

| pair | ext | base | note |
|---|---|---|---|
| `_h2` (ext CLEAN, base BUSY) | 0.1614 | 0.1600 | +0.9%, the towns diverge: 1,204 vs 1,203 citizens at the end |
| first pair (both BUSY), rows 1200-9000 | 0.1480 | 0.1576 | the base town lost ~300 citizens after frame 9000, so rows 9600 on are excluded |

Within noise: the seams cost nothing measurable, and the split does no extra work. The `_h2` pair's
CitizenList per frame is 0.0855 vs 0.0854 ms.

**Machine load.** verify_off CLEAN, ext BUSY, base BUSY, ext_h2 CLEAN, base_h2 BUSY, verify_on CLEAN.
The other sessions compiled during the BUSY legs. The drain halves' ms come from `[Stress::pathgraph]`
TSC timing and agree across all legs to within 0.05 ms.

## 6. Checks, open items and doubts

- **Checks.** Unit `tools/build.sh unit perf --jobs 2`: 1173 tests, 0 failures, 3 skipped (no kit
  fonts). The three skips are the usual ones; the three new tests pass. `tools/acceptance/acceptance.sh`:
  0 lines off target.
- **Not run.** The game gate (`tools/build.sh run`) and the resave gate are the coordinator's. With the
  row off the sim is unchanged, as the off leg's shadows show, and nothing new is serialized.
- **The task said "the next sim tick"; this ships "the next frame"** (section 1). The next tick would leave
  both halves in the same frame 49 times out of 50 at speed 50. If the user wants the literal next tick,
  change `Take`'s `nextFrame` test to "any later Update". The unit test's case 1 then changes.
- **The payoff is small, and only early in a game.** It is 1.6 ms off the first drain frame, and two
  half-frames for the next few drains. After that the drain is AssignCitizens alone, and the split only
  moves it. In a real town the first drain comes 300 s into the game, and ReassignWorkers runs on a
  handful of houses. The row is worth shipping only if the user wants the stress town's first-drain
  frame flattened. Its divergence is real but almost empty, as section 4 shows.
- **The save in the gap.** A save written in the gap drops that drain's AssignCitizens in the loaded game
  (section 2). This was reasoned, not tested by a run: no leg saved in a gap.
- **No icon**, because the sprite sheet is full.
