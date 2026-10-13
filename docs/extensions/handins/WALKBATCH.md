# WALKBATCH -- `ext.walk_batch`, the walk batch at speed N (OFFSCREEN D2)

Block WALKBATCH, 2026-10-12. OFFSCREEN design D2 (`handins/OFFSCREEN.md` section 4.2) built as the
extension `ext.walk_batch`, default OFF. **SHIPPED AS CLASS A** (the user's decision, 2026-10-13,
section 13): `kPlaceEveryTick = true`, a live toggle, formatVersion 0, no pseudo-mod, no lock. It was
first built and measured as class B (formatVersion 1, pseudo-mod `ext:walk_batch`, the batched tick
skipping `Place`); that variant stays in the tree as `kPlaceEveryTick = false`. Sections 1-10 describe
the design as built first; where they say "class B" they describe the `false` variant. Section 11
is the variant, section 13 the measurements and the decision.

**Short version.** On the N-1 intermediate iterations of a frame at speed N, a character whose move
tick is exactly `MoveTo::Execute`'s walking branch followed by the tail's step gets a short tick: the
same arithmetic, in the same order, **written straight into the live fields** (position, yaw, the
speed `+0x38`, the facing `+0x2C` / `+0x3C`, the path index; a waypoint calls the real
`MoveTo::UpdateFacing`). Skipped: the executor's loop and virtual dispatch, the trig, the rotation
store and the Geometry push (and, in the class B variant, `CharacterGrid::Place`; the shipped class A
setting places the walker every tick). The last iteration is always the faithful walk, which writes
the rotation of the caught-up yaw. Because the batch writes through, **only the drawn rotation lags**
inside the frame in the shipped setting (the grid too in the class B variant); every other read of a
walker is retail's at every tick, and nothing has to be materialised for `Clear()`,
`AbortForEntity`, `FlagForRemoval` or a save (section 1.6). Predicted gain (class B): ~4-6% of
EntityList; measured (section 13): -3% / -4% for the class B variant, -1% / -2% for the shipped
class A setting.

## 1. The design against the real bodies

### 1.1 What one walking tick does in retail (intermediate iteration)

`CharacterComponent::Tick` = `CharacterComponent_MoveTick_1094c0` (`citizenai_executor.cpp:295`):

1. **Executor** (`ActionStack::Execute`, :148). For a top entry that is one entered MoveTo
   (`_status` set, `_next` null) the executor is exactly one `MoveTo::Execute` call: it returns 1, so
   `bVar2` is set and the loop breaks; `current` is unchanged, so no pop and no flush.
2. **`MoveTo::Execute` case 2** (`moveto.cpp:347-476`), the walking branch: reads the model position
   (`character->modelComponent->transform.translation`), `curCell` / `destCell` (`WorldToCell`, the
   half-map offset, clamped); not the destination cell; `pathCount >= 2`; `pathIndex < pathCount`;
   the terrain byte of `curCell` (the sidecar mirror) gives the speed: follow flag `+0x474` ->
   desc `+0x30`; byte 1 or 9 -> desc `+0x34`; 0 -> desc `+0x38`; anything else -> 1.0 (0x14017214c);
   **writes** `character +0x38 = speed * MoveTo +0x478`. Waypoint rule: if `curCell == path[pathIndex]`,
   **writes** `pathIndex + 1` and calls `UpdateFacing` (0x1400c8d20), which **writes** character
   `+0x28 = false`, `+0x2C` (the normalised direction to the next waypoint cell's centre, via
   `GetTerrainPos`), `+0x3C` (its heading).
3. **The tail** with `MeshInstance::_rootOnlyUpdates` set (:314-573): the heading recurrence `fVar14`
   from `+0x3C`, the live yaw, desc `+0x24` / `+0x28`; `speed = sqrt(|RootAnimationOffset()|^2) * +0x38`
   (Dot in (y, x, z) order); if `0 < speed` the step `pos = f + speed * +0x2C` (componentwise); the
   STILLPATH decision (never taken by a stepping character, section 1.4); z = desc `+0x40` when desc
   `+0x3c`, else the stepped z (the terrain and the 0x0b test are last-iteration only); `yaw = fVar14 +
   desc +0x24`; the trig -> rotation `(+0, +0, -sin, cos)`; **writes** the seven transform dwords and the
   yaw; the Geometry push (unless `ext.render_culling`'s GEOMXF_LASTONLY); the pick push is
   last-iteration only; **`CharacterGrid::Place(characterIndex)`**.

### 1.2 The batched tick (`src/ext/walk_batch/walk_batch.cpp`)

`Eligible` reads, in the executor's and Execute's order: `actionStack.current >= 0`,
`characterIndex >= 0`, `actions[current]` non-null, its vptr `== MoveTo::Vtable()` (MoveTo is final),
`_status` set, `_next` null, `character == self`, then `DecideWalk` (`walk_batch_rules.h`): `pathIndex
>= 0`, `curCell != destCell`, `pathCount >= 2`, `pathIndex < pathCount`, follow flag clear. Anything
else is the real tick (`CharacterComponent::Tick`), at its own tick.

`Step` then does steps 2 and 3 above minus the trig, the rotation store, the push and `Place`:

| field | read | written | rule (pure function in `walk_batch_rules.h`, a literal copy of the retail line) |
|---|---|---|---|
| model `+0x158..+0x160` (translation) | yes | x, y, z | `StepPos`: the step when `0 < speed`, z = desc `+0x40` when desc `+0x3c` |
| model `+0x168` (yaw) | yes | yes | `Heading` (the recurrence) + desc `+0x24` |
| character `+0x38` | yes | yes | `TerrainSpeed(byte of curCell)` x MoveTo `+0x478` |
| character `+0x2C`, `+0x3C`, `+0x28` | yes | at a waypoint | the real `MoveTo::UpdateFacing()` |
| MoveTo `+0x60` (pathIndex) | yes | at a waypoint | `+1` |
| MoveTo `+0x30` desc, `+0x38` destination, `+0x64`, `+0x68[]`, `+0x474`, `+0x478` | yes | no | |
| `RootAnimationOffset()` | the real call | | `RootSpeed`: sqrt((y*y + x*x) + z*z) x `+0x38` |
| model `+0x148` rotation | no | **no** (last iteration) | |
| Geometry transform, CharacterGrid slot | no | **no** (last iteration) | |

The per-cell speed rule is read every tick from the live terrain byte (a road built mid-frame is seen
at its tick, as retail). The waypoint rule is retail's (the cell compare, then `UpdateFacing` on the
live node), so a span crosses any number of waypoints exactly.

### 1.3 The catch-up on the last iteration

There is nothing to catch up but the grid and the rotation: the position, yaw, speed, facing and path
index were written every tick. The last iteration's faithful tick (executor, tail) then runs from
retail's state; its rotation is a pure function of the yaw it writes (one trig), so the frame ends with
the rotation retail's would have, and its `Place` puts the character in the grid cell of its final
position. "Heading smoothing advanced N steps" is the recurrence run once per tick by `Step` (four
float ops; no closed form needed).

**The animation clock, `ext.anim_batch` on or off.** `Step` calls the real `RootAnimationOffset()`
at the tick where the tail would, so it reads exactly what the tail reads in either configuration:
the pool pose of the iteration for a pool-instanced clip (AnimationManager refreshes it every
iteration), or the instance's own `+0x100` for a blend / non-instanced clip, whatever `anim_batch`
did to the model's own clock. That is also why the brief's `NeedsPerTickRootUpdate` decline is not
needed: it protected a reconstruction of the root offset from the citizen's clock, and this design
does not reconstruct it. The model tick (and `anim_batch`'s decision there) runs after the character
tick, unchanged.

### 1.4 STILLPATH composes: a walker never takes the still path

The still path is a branch of the tail, taken only when `rootOnly && !(0 < speed)` and the bits match.
(a) A batched tick does not run the tail at all, so the still path is not reached and its memo is not
written. (b) The last iteration has `rootOnly` clear: no still path. (c) A declined intermediate tick
runs the full faithful tail; if it is still, the decision compares the LIVE rotation with the memo of
the yaw the tail is about to write. After batched ticks the live rotation can be stale (the batch
does not write it), which can only make the comparison fail, so the full tail runs and writes the
right rotation. The still path is exact by its own proof (STILLPATH section 1.2: it reads the live
state at the point of use). A walker with `0 < speed` is batched or runs the full tail; with
`speed <= 0` (`+0x38 == 0`, a zero root offset) the batched step writes the same bits the full tail
would (no step) and leaves the rotation, which is then also unchanged.

### 1.5 `MoveTo::Enter` at bucket rate

`EntityList::Update`'s bucket phase runs after the walk of each iteration; `CharacterComponent::Update`
-> `UpdateAnimation` -> `MoveTo::Enter` sets or clears the follow flag `+0x474` from a
`QueryNearestCharacter` on the grid. `DecideWalk` reads the flag live every tick, so a set flag
declines the walker's next tick (the coordinator's rule; the follow speed would be one more branch,
but followers are by construction the characters closest to others, and keeping their ticks exact
keeps their grid placement exact). What Enter itself sees differs from retail through the grid (the
class B evidence, section 3).

### 1.6 `Clear()` / `AbortForEntity` / `FlagForRemoval` mid-frame, and the save

Nothing is pending outside the live fields, so nothing has to be materialised:

- **Position readers** (the ~20 families of OFFSCREEN section 4.1: a new path's start cell after a
  `Clear()`, Work start-cell searches, the drop target, disease proximity, the herd box...) read the
  model translation, which is retail's at every tick.
- **`Clear()` / `AbortForEntity` / `Work::Cleanup` / `Deposit`** pop or replace the MoveTo; the next
  tick reads the new top node and declines unless it is again a walking MoveTo. No batched state
  refers to a node (the decision re-reads `actions[current]` every tick), so a freed node is never
  touched.
- **`FlagForRemoval` / death**: the entity is skipped by the walk from its 0x400 bit on, and
  `CharacterComponent::OnRemove` -> `CharacterGrid::Release` unlinks the slot from the cell the grid
  holds (its last real `Place`). The grid is never half-updated by the batch (it is not touched), so
  Release is consistent.
- **The save** (`GameUpdateLoop::SaveRestore`) is reached only from the dialogs (the UI pump, the
  Extensions / Mods dialog reload, the load route), outside the `do { ... } while (iteration <
  speedMultiplier)` loop of `GameUpdateLoop::Update`. The last iteration of every frame is faithful
  (`rootOnly = iteration < speedMultiplier - 1`), so between frames every walker's rotation and grid
  cell are what the last real tick wrote. The rotation, the only serialized field the batch skips
  (`transform.Serialize` in ModelComponent::Persist), is therefore always current when a save is
  written. The grid is not serialized.

## 2. The decline rules (the real tick runs, exact)

| rule | why |
|---|---|
| the last iteration of the frame, and every iteration at speed 1 | `Select(rootOnly)`: the faithful walk |
| empty stack, `characterIndex < 0` | the move tick's head: no executor / no tail |
| a null top slot, a node not yet entered (`_status` clear), a chain (`_next`), a top node that is not MoveTo, a MoveTo of another character | the executor's pop / precheck / chain paths |
| `pathIndex < 0` | case 1: the pathfinder poll, `TakePath`, `AbstractImpl` |
| the character stands in its destination cell | the arrival branch: the 0.05 test, the facing to the exact destination, the pop and the instantaneous nodes, all at their own tick |
| `pathCount < 2`, `pathIndex >= pathCount` | Execute's no-write return and the stop (`SetAnimation`, `pathIndex = -1`) |
| the follow flag `+0x474` | section 1.5 |
| `BANISHED_MOVETICK_TRACE` on | its log counts every tail (read once, in `BeginFrame`) |

**"Within one step of the destination"** (the brief's rule) is not needed: the decision is made per
tick from the live position, so the tick that steps into the destination cell is a plain walking tick
in retail too, and the next one declines. `ext_walk_batch_two_worlds_bit_identical` checks it (no
batched tick starts in a destination cell, and the worlds stay bit-identical through every arrival).

## 3. Why the first build was class B (the `kPlaceEveryTick = false` variant; section 11 for the shipped class A)

With the extension on and N > 1, inside a frame:

1. **The character grid lags.** `Place` runs only on a walker's real ticks, so between them the grid
   holds the cell of its last real tick (the previous frame's last iteration, or its last declined
   tick). `CharacterGrid::QueryNearestCharacter` (`MoveTo::Enter`'s follow flag for other walkers, the
   hunts, `FindBestTarget`) can see a walker up to N-1 ticks behind. Deterministic (no camera, no
   timing): two runs with one seed and one setting are the same game.
2. **The rotation lags** (model `+0x148..+0x154`); no sim read of a walker's rotation was found (the
   transform readers in `src/game` read buildings' interaction nodes or translations; the particle
   attach is presentation). Retail's Geometry push on intermediate iterations is skipped too (as
   `ext.render_culling`'s GEOMXF_LASTONLY does: render interpolation only).

Everything else -- position, yaw, speed, facing, path index, the arrival tick, the order of the
instantaneous nodes -- is retail's at every tick. The registry text says it plainly: other citizens see
where a walker stands up to a frame late; retail loads the save with the missing-mod warning.

## 4. The registry row and the selection

`src/ext/registry.{h,cpp}`: `Extensions::walk_batch` and the row `walk_batch`, `Tier::Extension`,
`Class::B`, default false, formatVersion 1, display name "Batched walking at high speed", the
description above (AS FIRST BUILT; shipped as `Class::A`, formatVersion 0, section 13). The class B lock, the pseudo-mod `ext:walk_batch` (ModInfo `+0x14 = 0x010007ff`,
`+0x18 = 1`), the block-1 manifest and the load notice are the registry's generic B machinery, exactly
as `anim_batch` uses it (no per-extension code).

Selection (DESIGN hook rules 3 and 4):

- `GameUpdateLoop::Update`, once per frame beside `PerfX::BeginFrame`: `WalkBatch::BeginFrame()` reads
  `Ext::Ext().walk_batch` (the per-game value under the lock) into `WalkBatch::g_frameOn`.
- The sim loop, once per iteration: `if (WalkBatch::Select(rootOnly)) m_entityList->UpdateWalkBatch(time);
  else m_entityList->Update(time);` (`Select` = `g_frameOn && rootOnly`, two loads).
- `EntityList::Update` is `UpdateImpl<false>` (retail's body, unchanged), `UpdateWalkBatch` is
  `UpdateImpl<true>`; the only difference is the snapshot walk's per-component tick
  (`MakeWalkTick<kWalkBatch>`): the batched one sends a `CharacterComponent` (by its vptr, as the walk
  prefetch tells it) to `WalkBatch::TickCharacter` (out of line, in `walk_batch.cpp`) and every other
  component to `TickOneComponent`. No toggle in any loop; no function-pointer table; no virtual.
- The snapshot's corrupt-tree fallback (the retail tree walk) is the same in both.

## 5. The shadows (`BANISHED_STRESS_COMPPROF=4`) and the row (`=2`)

Under COMPPROF=4 with the extension on, `WalkBatch::Tick` runs every character tick for real (the sim
is then the faithful sim) and checks, for every tick the batch would take: the batched step runs on
the live object, its result is read and undone, the real tick runs, and the two are compared.

| verify name | what | expected |
|---|---|---|
| `WalkBatchSim` | detail bit 0: a position component differs by more than 1e-4; bit 1: the position bits differ at all; bit 2: yaw, `+0x38`, `+0x2C`, `+0x3C`, `+0x28` or the path index differ | **0** (the step is retail's arithmetic) |
| `WalkBatchSpan` | a tick the batch would take whose real tick changed the stack (an arrival, a pop, a Clear from inside) | **0** (the decline rule) |
| `WalkBatchGrid` | the real tick's grid cell differs from the cell the batch would have left the walker in (its last real `Place`, tracked per grid index and frame) | **nonzero**: the class B evidence, as `AnimBatchSim` |

This is per tick rather than per batched frame: comparing whole spans would need a copy of the
executor, the node and the model to replay retail on; running retail on the live object and the
batch on a scratch of the fields it writes checks every tick of every span, including the waypoint
ticks, with no model of retail. The class B divergence that accumulates across a span is the grid's
(the others are proven equal per tick), which `WalkBatchGrid` counts.

`[COMPPROF] /char walkbatch` (COMPPROF=2, after the stillpath rows): batched ticks/frame and their
share of the walk class's intermediate ticks, their ms/frame and ns/call (the same RDTSC bracket as
every `/tree` tick; `WalkBatch::TickCharacter` replicates `TickOneComponent`'s profiling arm), the
walk-class intermediate ticks that ran the full tick in the same window, and `est. saved` = batched x
(full ns/call - batched ns/call). The batched ticks are also in the `/char` walk cells, so the
D2 bound row of the same run shrinks by what the batch removed.

## 6. The faithful build: what was checked

`objdump -d` of each touched TU compiled with the perf flags from main (`9b2f99a55`) and from this
branch, every function compared after stripping addresses:

- `citizenai_executor.cpp` (not edited): **identical**, including `CharacterComponent_MoveTick_1094c0`
  (672 instructions), `ActionStack::Execute`, `CitizenAI_ActionFlush_c4bb0`.
- `moveto.cpp` (one `DEFINE_EXE_VTABLE(MoveTo, "_ZTV6MoveTo")` appended): every MoveTo slot
  **identical** (Execute 217 instructions, Enter, AbstractImpl, Persist...); the ctor before the new
  function differs only in its trailing alignment padding.
- `entitylist.cpp`: every function **identical** except `EntityList::Update` and
  `Collection::Set<...>::Clear` (block order). `EntityList::Update` is the same source and the same
  instruction mix (81 calls, 34 prefetches, 1663 vs 1654 instructions) but GCC allocates registers
  differently in the bigger translation unit (+16 stack references, one pointer of the staged
  prefetch spilled). Three measures already bring it this close: the batched character arm is out of
  line in `walk_batch.cpp`, `UpdateImpl` is always inlined into both entry points, and the batched
  walk's snapshot rebuild omits the `reserve` hint so the faithful copy keeps its single inlined
  `std::vector::reserve`. **The faithful profile must therefore be measured, not assumed**: the
  coordinator's A/B `STRESS_EXT=none` on this branch vs main (section 9).
- `gameupdateloop.cpp`: `GameUpdateLoop::Update` gains the `BeginFrame` call and the per-iteration
  pick (two loads and a branch per iteration).

## 7. Unit tests (`tests/unit/ext_walk_batch_test.cpp`, 4 tests; 1104 tests, 0 failures)

- `ext_walk_batch_registry_row`: class B, formatVersion 1, default off, the key, the lock (a request
  off waits for the next game), the pseudo-mod `ext:walk_batch`, the load status, a save's manifest
  locking it on.
- `ext_walk_batch_rules_match_retail`: `WorldToCell` against a literal copy on four map sizes, the
  terrain speed for all 256 bytes, the step's edge cases (no step at 0 / NaN, -0.0 z, b3c).
- `ext_walk_batch_decline_rules`: `DecideWalk`'s truth table and Execute's test order.
- `ext_walk_batch_two_worlds_bit_identical`: 24 walkers on a 32x32 map of mixed terrain (normal, road
  1 / 9, other, 0x0b) with three descriptions (one b3c), per-walker speed multipliers and a hashed
  per-tick root offset, at speeds 1, 3, 5, 10, 20 for 6,000 iterations each. The REFERENCE world runs
  literal copies of `MoveTo::Execute` (arrival, stop, the speed rule, the waypoint) and of the move
  tick's tail (the recurrence, the step, the last-iteration terrain z and 0x0b test, the trig, the grid
  placement) on every tick; the BATCHED world runs the rules exactly as `Step` composes them on every
  intermediate tick `DecideWalk` accepts. Between iterations the script toggles follow flags, clears
  and re-paths walkers from where they stand, teleports them and hands idle ones new paths. After
  every iteration position, yaw, speed, facing and path index must be bit-identical; after every frame
  the rotation and the grid cell too. Result: 0 mismatches; 289,514 batched ticks (0 at speed 1),
  7,656 of them at a waypoint; the grid cell differs from the reference's on 2,411 (speed 3) to 20,716
  (speed 20) intermediate walker ticks, 2.5% to 15% of them (the class B evidence). Mutations run
  once and reverted: dropping the destination-cell decline fails it (and counts batched ticks in a
  destination cell); re-associating the root-offset dot product or perturbing the heading recurrence
  by one ulp fails it.

Gates: `tools/build.sh build perf --jobs 2` OK, `tools/build.sh unit perf --jobs 2` 1104 / 0,
`python tools/abi_audit.py` OK, `tools/acceptance/acceptance.sh` 0 lines off target,
`python tools/castmetric.py src` total 2 (the two new vptr reads carry `// CAST-OK: vslot`).

## 8. Files

| file | change |
|---|---|
| `src/ext/walk_batch/walk_batch_rules.h` (new) | the pure rules: `WorldToCell`, `TerrainSpeed`, `Heading`, `RootSpeed`, `StepPos`, `DecideWalk` |
| `src/ext/walk_batch/walk_batch.{h,cpp}` (new) | `BeginFrame` / `g_frameOn` / `Select`, `Tick` (Eligible + Step), `TickCharacter` (the COMPPROF bracket), the three shadows, the counters |
| `src/ext/registry.{h,cpp}` | `Extensions::walk_batch`, the row |
| `src/game/Entity/EntityList.h`, `entitylist.cpp` | `UpdateWalkBatch`, `UpdateImpl<kWalkBatch>`, `MakeWalkTick`, `WalkUpdateTreeSnapshot<kWalkBatch>`, `RebuildWalkSnapshot<kReserve>` |
| `src/game/Game/gameupdateloop.cpp` | `WalkBatch::BeginFrame()`, the per-iteration pick |
| `src/game/Action/moveto.cpp` | `DEFINE_EXE_VTABLE(MoveTo, ...)` (the vptr test) |
| `src/diag/stress_harness.cpp` | the `/char walkbatch` row and its window reset |
| `tests/unit/ext_walk_batch_test.cpp` (new) | section 7 |
| `docs/extensions/INVENTORY.md` | row E4 |

No new `BANISHED_*` flag, no `Opt()` field, no `getenv`, no door; the faithful bodies (the move tick,
MoveTo, the executor) are not edited.

## 9. Prediction and the runs for the coordinator (idle machine)

**What a batched tick removes**: the executor's loop and its virtual call to `Execute`, the trig
(~6 ns), the transform build and its seven stores, the Geometry push (with `render_culling` off),
`Place` (for a walker the rect memo misses every tick, so the cell computation runs: ~5-10 ns), the
trace and pick tests. **What it keeps**: the data the tick needs (the action slot, the MoveTo's lines,
the model line, the description, the pool pose for the root offset -- EXECSELF's misses), plus one
out-of-line call. The D2 row priced an intermediate walker tick at ~28 ns profiled (0.31 ms/frame over
~11,000 ticks), of which the profiler bracket is ~10 ns. **Prediction: ~12-16 ns saved per batched
tick, ~85-90% of intermediate walker ticks batched (the rest are arrivals, waypoints at a destination
approach, followers), i.e. ~0.13-0.17 ms/frame = ~4-6% of EntityList in the heavy spread town at
speed 10, a third to a half of the 12% ceiling; more at speed 50 (a larger intermediate share), zero
at speed 1.**

Runs, in order (heavy = `--map large --citizens 1600 --sets 3 --spread --speed 10`, as STILLPATH):

1. **Shadows**: `BANISHED_STRESS_COMPPROF=4 STRESS_EXT=walk_batch tools/build.sh stress` (heavy). Read
   `[VERIFY] WalkBatchSim` (expect 0), `WalkBatchSpan` (expect 0), `WalkBatchGrid` (expect nonzero);
   the existing shadows (`StillFastPath`, `TickList`, `TerrainSidecar`...) must stay at 0.
2. **The row**: `BANISHED_STRESS_COMPPROF=2 STRESS_EXT=walk_batch,render_culling,anim_batch tools/build.sh stress`
   (heavy): `/char walkbatch` batched share and `est. saved`, against the `/char bound walk
   intermediate (D2)` row of a `STRESS_EXT=render_culling,anim_batch` run.
3. **The A/B** (COMPPROF=0, interleaved pairs, CLEAN): `STRESS_EXT=render_culling,anim_batch` vs
   `STRESS_EXT=walk_batch,render_culling,anim_batch`; and alone, `STRESS_EXT=none` vs
   `STRESS_EXT=walk_batch`. Read EntityList / treeWalk ms/frame and sim ms/iter. Windows after ~frame
   3000 diverge in population (class B) as `anim_batch`'s did; compare the early windows.
4. **The faithful profile** (section 6): `STRESS_EXT=none` on this branch vs main, interleaved: the
   faithful `EntityList::Update` was recompiled with different register allocation and must not be
   slower.
5. The gate as usual (faithful forced; the extension is off there, so the resave golden must hold).

## 10. Doubts

- **The faithful walk's code is not byte-identical** (section 6); the move tick is. If run 4 shows a
  regression, the fallback is to compile the batched update in its own translation unit (it needs the
  snapshot statics exposed, a larger change) -- say so and I will do it.
- **`Place` is the class B line and the main saving at once.** A cheaper variant would call `Place` on
  every batched tick (no grid lag: the extension would then be exact except the rotation, i.e. class A
  by the sim's reads found here) and save only the executor and the trig, roughly STILLPATH's ~7-10 ns
  plus the executor overhead. The COMPPROF=4 `WalkBatchGrid` count says how often other characters
  could have seen the difference; if the user prefers an exact walk, that variant is a three-line
  change in `Step` (and the registry class).
- **The follow-flag decline** costs the followers' ticks (likely a few percent of walkers in a dense
  town); handling `+0x30` would be one branch in `TerrainSpeed`. Kept as the brief asked.
- **`UpdateFacing` and `GetTerrainPos`** are called on the live node exactly as `Execute` calls them;
  the shadow undoes their writes (`+0x28`, `+0x2C`, `+0x3C`) and relies on `GetTerrainPos` being a pure
  query (it is a height lookup on the map, `mapquery.cpp`).
- Not run in the game by this block.

## 11. The two settings of `WalkBatch::kPlaceEveryTick` (coordinator follow-up 1)

`walk_batch_rules.h`: `static constexpr bool kPlaceEveryTick` -- `true` SHIPPED (class A, section 13),
`false` the measured class B variant. With `true`, `Step`
ends with `CharacterGrid::Place(characterIndex)`, where the faithful tail ends: after the position
write, same index, same model state. `Place` reads only the model translation and desc `+0x20`
(`CharMgr_helper_28c0`, `charmanager.cpp:76`), both already final, and the tail's Geometry push in
between touches neither; so the grid slot, its cell links, the occupancy counts and the rect memo
evolve exactly as retail's. Under COMPPROF=4 the shadow takes the batch's own `Place` as the reference
cell, so `WalkBatchGrid` must then read 0. The unit test runs both settings
(`ext_walk_batch_two_worlds_bit_identical`, `..._place_every_tick`): position / path / speed / facing
bit-identical every tick in both; with Place every tick the grid cell also on every tick (grid lag
exactly 0), without it nonzero.

**What each setting makes observable inside a frame** (the last iteration is faithful in both, so
between frames both are retail's state):

| | `kPlaceEveryTick = false` (class B variant) | `kPlaceEveryTick = true` (shipped, class A) |
|---|---|---|
| position, yaw, `+0x38`, `+0x2C` / `+0x3C`, path index, arrival tick, node order | retail's every tick | retail's every tick |
| CharacterGrid (what `QueryNearestCharacter` returns to MoveTo::Enter's follow flag, the hunts, `FindBestTarget`) | up to N-1 ticks late: **sim-visible** | retail's every tick |
| model rotation `+0x148..+0x154` | stale until the last iteration | stale until the last iteration |
| Geometry push (render transform, attached tool, bounds) | last iteration only | last iteration only |
| the save | never mid-frame (section 1.6) | same |

**With Place every tick, does the SIM read anything different from retail inside a frame? No reader
was found.** The only state that differs mid-frame is the walker's model rotation and its render-side
Geometry transform. Every reader of either in `src/game` is presentation or input:
`ParticleComponent::Place` / the node-follow transform (emitter placement; the particle sim and
`StopEmitter` do not read it), `GetNodeToWorld` (particles, highlight, tool render),
`meshInstance._transform` (tutorial highlight, `ext.eye_camera`), `tool_base_screenrect.cpp` /
`pickingcomponent.cpp` (player tools, between frames). The model-transform readers in the sim
(`action_pf_road.cpp:551`, `work_base_walkpos.cpp:77`, floor / quarry / terraform) take buildings'
models, and every animal / herd / hunt read of a character model takes `translation` only; the
rotation is serialized, but a save is never written mid-frame. The STILLPATH decision reads the
live rotation of the character's OWN declined ticks; a stale rotation only makes it decline, and the
full tail it then runs is exact. What remains different is what is DRAWN: the render interpolation
between frames (the Geometry's previous transform is last frame's final one, as with
`ext.render_culling`'s GEOMXF_LASTONLY), and nothing else, since only the last iteration is drawn and
it is faithful. **So with `kPlaceEveryTick = true` the extension would be class A (no sim effect, no
serialized difference at any point a save can happen) and a live toggle**: the registry row would
become `Class::A`, formatVersion 0, no pseudo-mod, no lock, and its description would drop "diverges
from the original". (Done: section 13.) The cost is the saving: `Place` on a
walker misses the rect memo every tick (the cell computation runs, ~5-10 ns), so the variant keeps
roughly the executor dispatch, the trig and the transform stores (~6-9 ns of the ~12-16 predicted).

## 13. Results and the decision (coordinator, 2026-10-13)

Measured on a CLEAN machine, window 1 (before the population can diverge), heavy town at speed 10:

| setting | EntityList, the extension alone | with `render_culling` + `anim_batch` |
|---|---|---|
| `kPlaceEveryTick = false` (class B) | -3% | -4% |
| `kPlaceEveryTick = true` (class A) | -1% | -2% |

- Shadows (COMPPROF=4): `WalkBatchSim` 41M checks / 0 mismatches (the batched step is retail's
  arithmetic); `WalkBatchGrid` with Place every tick 41M / 0 (the grid never lags); `WalkBatchSpan` 0
  (no arrival or pop inside a batched tick).
- The faithful exe with the extension off matches main: EntityList 2.57 / 2.56 ms/frame vs 2.58.
  The register-allocation difference of section 6 costs nothing measurable, so the own-TU branch
  (section 12) is not merged.
- The prediction (section 9: ~4-6% for class B) was high: the class B variant gave 3-4%, about a
  third of the 12% ceiling. `Place` is worth about 2 points of EntityList on its own.

**Decision (the user): ship `ext.walk_batch` as CLASS A with `kPlaceEveryTick = true`.** Exactness
for the sim is worth more than the extra two points. The row is now `Class::A`, formatVersion 0: a
live toggle with no lock, no pseudo-mod, nothing in the save manifest, and no load notice. The
description says what it does change: the direction a walker's body faces is brought up to date once
per frame, on the drawn step. The class letter is the only per-extension input of the save / lock /
notice machinery (`registry.cpp` `IsLockedClass`, `PseudoModsForSave`, `LockForGameWithSet`,
`PseudoModLoadStatus`; `savetail.cpp` writes the active B / C set), so nothing else keys on the row.
`ext_walk_batch_registry_row` now checks: class A, formatVersion 0, `kPlaceEveryTick` true, live
under a game's lock, never a pseudo-mod, and a save manifest naming it refused.

The class B variant stays in the tree (`kPlaceEveryTick = false`) as the measured alternative. Turning
it back on needs the row back to class B / formatVersion 1, the registry test, and this section.
