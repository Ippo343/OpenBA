# PREFETCHCOV -- prefetch coverage II (IDEAS #1, round 2, with SMMEMO on)

Block PREFETCHCOV, 2026-10-13. Candidate #1 of `IDEAS_20261012.md` (§2.1). It was never built
before: the IBS pass demoted it first. `PERFDAY_20261012.md` §5 asks for it to be measured now,
with SMMEMO on, expecting the small end. It is built as faithful perf work in the style of
PREFETCH: hints only, no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()`
field, no `getenv`, no door. **The game was not run by this block**; the coordinator runs
the A/B.

**Short version.** Four `static constexpr bool` switches beside `kWalkPrefetch` in
`src/game/Entity/entitylist.cpp`. Each turns on one `if constexpr` sub-arm of the existing
five-stage pipeline. There is no new stage, and the stride stays 1. **All four ship `false`.**
With all four off, the perf object of `entitylist.cpp` is the shipped variant 2: `objdump -dr`
of it differs from the same TU at `642a56df4` by 0 lines.

| switch | arm | predicted heavy treeWalk (ms/frame) | recommendation |
|---|---|---|---|
| `kPrefetchCovChar` | C: grid slot, rect memo, MoveTo lines, sidecar byte | -0.02 (range -0.05 .. +0.02) | keep if neutral-or-better in both towns |
| `kPrefetchCovTerrainLast` | C-last: cell floorCount line, two heightmap rows, last iteration only | 0.00 +- 0.01 (not resolvable here) | keep if neutral; it exists for the small-L3 case |
| `kPrefetchCovParticle` | P: emitter slots, live emitter instances | -0.01 (range -0.03 .. +0.01) | keep if neutral-or-better in both towns |
| `kPrefetchCovStateMachine` | S: controller node and its parent | +0.01 (range 0 .. +0.03) | **drop whatever the number says** (§4) |

## 1. What is built, per arm, and where

All the code is in `src/game/Entity/entitylist.cpp`, inside `WalkPrefetchStaged` (the S3 =
i+2 and S4 = i+1 lambdas) and in `always_inline` helpers just above it. The vptr compare
picks the arm, as S3 and S4 already do for characters and models. Every helper and lambda is
`always_inline`, because of PREFETCH §4's pure-helper trap. The counts in §7 show the
prefetches survived.

### 1.1 Arm C: character tail (`kPrefetchCovChar`)

| stage | prefetch | read by |
|---|---|---|
| i+2 | `&grid->slots._data[ci]` and `&...[ci].cell` (`grid = GameUpdateLoop::_singleton->m_characterGrid`, `ci = characterIndex`) | `CharacterGrid::Place`: `slot->character`, `slot->cell`. The 0x18 stride makes about a quarter of the entries straddle a line, so both ends are named |
| i+2 | `CharacterGrid::RectMemoAt(ci)` and `&m->b3` | `Place`'s rect memo compare (24-byte entries, same straddle) |
| i+1 | `a + 0x40`, `a + 0x60`, `a + 0x474` beside the existing `a`, `a + 0x1F` | `MoveTo::Execute`: `destination` .. `pathIndex` / `pathCount` (+0x20..+0x67) and `field_0x474` / `speedMultiplier` (+0x474..+0x47B) |
| i+1 | `&TerrainSidecar::g_mirror[cell]`, where `cell` comes from the model translation (`ModelComponent` +0x158) | `MoveTo::Execute`'s `TerrainSidecar::Get(curCell)` on every iteration, and the executor's 0x0b test on the last |

The ideas table names "the +0x40 line" for the action node. MoveTo is `MemAlloc(0x480, 8)`, so
the node is only 8-aligned. At some alignments the lines of `a`, `a + 0x1F` and `a + 0x40`
leave `+0x60..+0x67` uncovered. With `a + 0x60` added, the four points are less than 64 bytes
apart (0, 0x1F, 0x40, 0x60), so their lines are contiguous. The last line reaches at least
`a + 0x67` at every 8-byte alignment. So the arm adds three prefetches here, not two.

**The rect memo move.** `Place`'s memo was `static std::vector<RectMemo> s_memo` inside
`Place`. It is now `CharacterGrid::s_rectMemo`, a private static member defined at file scope
in `src/game/Component/charmanager.cpp`. The public `const` accessor is
`CharacterGrid::RectMemoAt(int)`, inline in `CharacterGrid.h`, which returns null outside the
memo. `Place` binds a reference to it and its body is otherwise unchanged. The entry type
moved from a local struct to `CharacterGrid::RectMemo`, with the same fields. No behaviour
changes. The only differences are the guard-variable check of a function-local static and
destruction order at exit, and nothing runs `Place` at exit. `CharacterGrid`'s layout is
unchanged: a static member takes no bytes, and the `sizeof == 0x60` assert holds.

### 1.2 Arm C-last: terrain on the last iteration (`kPrefetchCovTerrainLast`), and the decision

**The decision asked for (STILLTERRAIN §6 / §8, STILLTERRAIN's memo).** Since STILLTERRAIN, a
still character's last-iteration `GetTerrainPos` is answered from `s_terrainMemo`. That covers
95-97% of the ~880-1,070 still queries per frame in the heavy town (§10 of STILLTERRAIN). Those
characters read neither the heightmap nor the cell's `floorCount` any more. Only the **moving**
characters still do, about 1,000-1,100 per frame. STILLTERRAIN §6 says this arm should then
"only be issued for characters that will miss". **The walk cannot tell the two apart without a
demand load.** It would need the character's speed, which the tick computes, or the memo
entry's x / y bits against the translation, which is a read of the memo line. So:

- **Kept as an arm, but split from C** (its own switch), and gated to the last speed iteration.
  The gate is one load of `Graphics::MeshInstance::_rootOnlyUpdates` per character visit, the
  same byte the tick reads. On the intermediate iterations it issues nothing. Without the
  gate, at speed 10 it would issue nine wasted fills for every useful one.
- About half of its fills go to still characters. Those are wasted, but they are cheap here
  (L3 hits).
- **On this box it cannot be resolved.** It issues 3 prefetches x ~2,000 characters once per
  frame. Even if every line it hides were a serially exposed L3 hit, the ceiling is ~1,100 x
  3 x 11 ns = 0.036 ms/frame. That is the noise on treeWalk (~0.03-0.06). The case for it is
  the small-L3 case (§5), where these are DRAM lines. If the A/B is neutral, the keep rule keeps
  it. If the A/B is negative in both pairs, drop it.

| stage | prefetch | read by |
|---|---|---|
| i+1, last iteration only | `&cells[cell].floorCount` (Map +0x80, 0x30 stride, 7.8 MB) | `GetTerrainPos`'s floor test (`MapQuery_MayReachFloor` on a STILLTERRAIN miss) |
| i+1, last iteration only | `&hm[z0 * N + x0]`, `&hm[z1 * N + x0]` (`WorldGeometry::_heights`, row stride `_fieldD0`) | `MapQuery_SampleHeight`'s four words (x0 / x1 share a line except at a 32-word boundary) |

The cell and height math is `WorldToCell` / `GetTerrainPos` / `MapQuery_SampleHeight`'s:
truncate, then clamp. The prefetch uses the pre-step translation. A step moves the point by
less than one cell almost always, so the line is the one the tick reads.

### 1.3 Arm P: particle (`kPrefetchCovParticle`)

| stage | prefetch | read by |
|---|---|---|
| i+2 | `emitters._data` and `&_data[min(size, 4) - 1]._pad14` | `ParticleComponent::Tick`'s slot loop (`nodeIndex`, `instance`, `sceneId`, `fadeRate`) |
| i+1 | for each of the first 4 slots with `instance && sceneId >= 0`: `inst`, `&inst->_opacity`, `&inst->_resource` | the tick: `_opacity`, `_dirtyFlags`. Then `ParticleSystemInstance::Update` through the vptr: `_resource` / `_boundsPtr` (+0xC0 / +0xC8), and `SetBounds`' `_localBounds` compare (+0x34) |

The ideas table says "the first two lines". The tick's reads sit at +0x00..+0x4B, +0x7C..+0x83
and +0xC0..+0xCF of the 0xE0 object, so the arm names three lines, by field rather than by
offset.

**Not built: `&s_compTbl[slotOf(c)]`.** Block UNSTAMP, running in parallel, replaces that
table probe with a per-component stamp, so the line is moot.

`ParticleComponent::Vtable()` is new: `DEFINE_EXE_VTABLE(ParticleComponent,
"_ZTV17ParticleComponent")` in `particlecomponent.cpp`, as CharacterComponent and ModelComponent
got theirs for PREFETCH. The class is `final`, and the linker checks the symbol.

### 1.4 Arm S: state machine (`kPrefetchCovStateMachine`)

| stage | prefetch | read by (before SMMEMO) |
|---|---|---|
| i+2 | `stateMachine.current` (a controller node embedded in another component) | the run phase's `node->Tick` (node vptr) |
| i+1 | `current->_data08` (the parent), null at the root | the climb `node = node->parent` |

`StateMachineComponent::Vtable()` is new: `DEFINE_EXE_VTABLE(StateMachineComponent,
"_ZTV21StateMachineComponent")` in `statemachinecomponent.cpp`.

**SMMEMO makes this arm pure cost.** SMMEMO §9 measured 6,180 of 6,180 slot-8 ticks per frame
skipped in the heavy town. The hit path reads only the component's +0x50 state word and the
+0x54 memo, which sit in the first 0x80 bytes that S2 already prefetches. It never touches
`current` or the chain. The full tick runs only after a request, a transition or a load, when
the memo declines. That is a handful per frame. So the two lines this arm fetches are read by
almost nothing. The arm also adds one dependent load at i+1 (`current->_data08`, from the line
fetched at i+2). **Prediction: a small regression, probably inside the noise. Recommendation:
drop it whatever the A/B says.** "Neutral" here would only mean "too small to see". The arm
covers no line any tick reads, so the working-set clause of the keep rule (PREFETCH §7: "covers
the misses a smaller cache pays") is false by construction.

## 2. Exactness, per prefetch address

The PREFETCH §3 rules hold in every arm:

- **No architectural effect.** `prefetcht0` never faults (not even on an unmapped address) and
  never changes a value. No value read for a prefetch reaches the sim.
- **Every address is re-derived in its own stage from live fields.** Nothing is carried between
  stages except the TickList entry, which is validated as before. The reads a stage makes are
  plain loads of live objects, which cannot be freed during the walk (frees happen in
  `DrainRemovals`). Those reads are: `characterIndex`, the grid / map / world-geometry pointers
  off `GameUpdateLoop::_singleton`, the array headers, the memo vector's begin / end, the
  model translation, `emitters._data` / `_size`, a slot's `instance` / `sceneId`,
  `stateMachine.current` and the node's `_data08`.
- **Bounds checks** before any address is formed from an index:
  - `0 <= ci < slots._size`
  - `RectMemoAt`: `0 <= idx < s_rectMemo.size()`
  - `cell < g_mirrorCount`, where the mirror is null when not built
  - `cell < cells.GetSize()`
  - the heightmap row / column clamped to `[0, N-1]`, as `MapQuery_SampleHeight` clamps
  - the cell `ix` / `iy` clamped to `[0, dim-1]`, as `WorldToCell` / `GetTerrainPos` clamp
  - the emitter index `< min(_size, 4)`
- **Null tests only on prefetch-only reads.** None of them is on a read the faithful code makes
  unguarded. `_singleton`, `m_characterGrid`, `m_game`, `m_worldGeometry`, `modelComponent`,
  `_heights.data`, `emitters._data`, `instance`, `current` and `_data08` are all tested here
  only so that no prefetch-stage load can dereference null. The ticks are untouched.
- **Blind offsets.**
  - `a + 0x40 / 0x60 / 0x474` on a non-MoveTo action names a line of that object or of the
    next heap block. That costs a wasted fill and cannot fault.
  - `_rootOnlyUpdates` is read as a plain byte, the same one the tick reads.
- **No toggle in the loop.** Every switch is `static constexpr`, and every arm is `if constexpr`.
  The only runtime branch an arm adds is its own vptr compare or live test. With all off, the
  added `else { }` arms are empty, and the object is byte-identical to HEAD (§7).
- **No new doors.** The two vptrs come from `DEFINE_EXE_VTABLE` (the compiler's table, checked
  by the linker). Nothing is registered or resolved by retail name or address.

There is no shadow: a prefetch has no architectural effect. The existing COMPPROF=4 shadows
(TickList, SmTickNoop, StillFastPath, StillTerrain, StillTrig, TerrainSidecar, FrustumCull)
must stay at 0 mismatches in any leg. They cover every tick these arms feed.

## 3. The A/B matrix for the coordinator

**Where to flip:** `src/game/Entity/entitylist.cpp`, the four lines just below
`kWalkPrefetchStride`:

```
static constexpr bool kPrefetchCovChar         = false;   // line 481
static constexpr bool kPrefetchCovTerrainLast  = false;   // line 489
static constexpr bool kPrefetchCovParticle     = false;   // line 495
static constexpr bool kPrefetchCovStateMachine = false;   // line 502
```

Build each leg with `tools/build.sh build perf --jobs 2`. Only `entitylist.cpp` recompiles. Then
**check the prefetch count** with
`objdump -d build/cmake-perf/CMakeFiles/reimpl-x64.dir/src/game/Entity/entitylist.cpp.obj |
grep -c prefetch` against the column below before copying the exe, and record its md5.

| leg | Char | TerrainLast | Particle | StateMachine | prefetch count |
|---|---|---|---|---|---|
| **A0 baseline (shipped variant 2)** | false | false | false | false | 68 |
| C | true | false | false | false | 100 |
| T | false | true | false | false | 84 |
| P | false | false | true | false | 88 |
| S | false | false | false | true | 72 |
| CTP (the likely keep set) | true | true | true | false | 136 |
| ALL | true | true | true | true | 140 |

The counts add up exactly over the arms: C +32, T +16, P +20, S +4. So any other combination is
68 plus the sum of its arms.

- **S is +4, not +8.** GCC cross-jumps the i+1 parent prefetch onto an existing
  `prefetcht0 (%rcx)` in each walk. Both S stages were checked present, by disassembly, in both
  walks (`EntityList::Update` and `UpdateWalkBatch`).
- **Why the arms are multiples of 4.** Each walk carries two loop copies of each stage (the
  cached and the uncached `ForEachWalkCandidate` paths), and there are two walks.

**Protocol** (PREFETCH §6 / §9):

- Faithful profile, every extension off, mods parked.
- Idle machine with `machine load: CLEAN`: RTSS / Afterburner stopped, no compiler, no agent
  building.
- Heavy = `--map large --citizens 1600 --sets 3 --spread --speed 10`. Compact = the default
  town.
- Interleave `A0 C T P S CTP` twice per town. ALL is optional: it should equal CTP + S.
- Read treeWalk ms/frame, EntityList ms/frame and sim ms/iter.

**Keep rule** (PREFETCH §7): keep an arm only if it is neutral-or-better in both towns. A
regression of at least 0.01 ms/iter with the same sign in both pairs is a drop. On treeWalk the
section 6 bar for a real effect is at least 0.06 ms/frame with the same sign in both pairs.

**If the user allows the co-runner** (PREFETCH §6, the cache-pressure process, label `pressure`):
run A0 / T / CTP under it. That is the only way this box can show the case T and C exist for
(§5).

## 4. Predictions (heavy treeWalk, ms/frame; current baseline ~1.88-1.95 with SMMEMO + STILLTERRAIN on)

The frame for every prediction is IBS §3. The sim is not memory-bound on this 96 MB box: grid
place shows ~0.1% load-miss latency, the move tick 0.0%, and the walk 0.05%. PREFETCH's variant
2 already covers the chain's first lines. Expect the small end.

- **C: -0.02 (range -0.05 .. +0.02), i.e. within noise.**
  - Gain: 20,000 character visits per frame (2,000 x 10 iterations). Place's two index-random
    lines and the MoveTo tail are L3 hits at most, mostly overlapped by the out-of-order core.
    Perhaps 2-4 ns hidden per visit, 0.04-0.08 ms/frame gross.
  - Cost: 8 more prefetches and ~8 more loads to resident lines per visit, ~2-3 ns, or
    0.04-0.06 ms/frame.
  - Net: around zero, leaning slightly negative-to-neutral. If it shows anything, it is the
    MoveTo lines. MoveTo is 94% of the executor's calls, and its +0x60 / +0x474 lines are on
    the dependent path.
- **T: 0.00 +- 0.01.** Once per frame, ~2,000 x 3 lines. The ceiling is 0.036 ms/frame even if
  every line were a serial L3 hit (§1.2), so it is not resolvable here.
- **P: -0.01 (range -0.03 .. +0.01).** 7,700 particle visits per frame. The instance lines sit
  behind an indirect call (Update via slot 1), so they are exposed latency in principle. But
  particles are 7.5% incl. of the tree phase, the instances of one building are allocated
  together, and IBS saw no miss latency there.
- **S: +0.01 (range 0 .. +0.03), a cost.** 6,200 SM visits per frame, each with 2 prefetches
  + 2 loads + one compare, feeding lines nothing reads (§1.4). Drop.
- **All on (CTP + S):** the sum, about -0.02 .. +0.01. Expect sim ms/iter unchanged at 0.26 in
  every leg. Compact: the same signs, about half the size.

These follow PREFETCH's own history: its §7 predicted "within noise" and measured -0.07 heavy.
That was the chain's first lines, where the misses were. This round is the tail after them,
and after SMMEMO / STILLPATH / STILLTERRAIN removed most of the work the tail fed. The honest
expectation is that nothing here clears the 0.06 bar on this box.

## 5. The working-set argument (why a neutral arm here can still pay on a 16-32 MB L3)

The user's rule is never to assume the player's L3 is this box's. PREFETCH §5 sizes the walk at
~2.6-5.1 MB per intermediate iteration (~1.1-1.6 KB per character), plus +2.2-4.5 MB on the last
iteration (poses, terrain): **~5-10 MB per frame**. The renderer's per-frame gather (draw lists,
instance buffers for the same 2,000 models) runs between frames and evicts part of it. What these
arms add to the per-iteration set:

| arm | lines it covers | bytes touched per iteration (heavy) | where they live |
|---|---|---|---|
| C | grid slot (48 KB array), rect memo (48 KB), MoveTo +0x40 / +0x60 / +0x474, sidecar mirror (256 KB, ~2,000 lines touched) | ~0.5 MB | index-random over small arrays; the MoveTo tail is one random heap line per character |
| T | cell record line (7.8 MB array, 0x30 stride), two heightmap rows (~0.5 MB int16) | ~0.4 MB, last iteration only | index-random over a 7.8 MB array: no reuse between frames |
| P | emitter slots, ParticleSystemInstance lines (0xE0 objects) | ~0.25-0.4 MB | heap, per building |
| S | controller nodes | (nothing the tick reads) | -- |

- **96 MB X3D (this box):** everything stays L3-resident at any reachable population. Every
  line these arms hide is an ~11 ns L3 hit, mostly overlapped. Neutral is the expected result,
  and it is *not* evidence that the arms are useless.
- **32 MB L3** (5800X / 7700X, i7 / i9 30-36 MB): the intermediate set fits. The last-iteration
  set (T's 7.8 MB cell array is touched once per frame, never reused) mostly misses to DRAM, so T
  pays ~1,000-1,100 moving characters x up to 3 lines at 70-90 ns where exposed. C and P mostly
  hit L3, as here.
- **12-16 MB L3** (i5-12400 / 13400, Ryzen 7 5700U / 7840U): the first iteration after each Draw
  pays DRAM for whatever the render evicted, and the last iteration's set spills. Then:
  - Place's two random lines and the MoveTo tail are DRAM misses on that first iteration, about
    2,000 x 4-5 lines.
  - T's lines are DRAM on the last.
  - IDEAS §2.1's estimate was 0.15-0.18 ms/frame for the terrain lines alone at 12-16 MB. With
    STILLTERRAIN, about half of that population, the moving ones, remains: ~0.07-0.09 ms/frame
    that T can hide.
- **<= 8 MB L3:** every iteration walks from DRAM, and every arm except S hides real 70-90 ns
  misses. One entity per stage (~75 ns per visit) is just enough distance for one miss
  (PREFETCH §9.2).

So the arms C, T and P exist for the smaller caches. On this box the keep rule only asks that
they cost nothing measurable. S is the exception: SMMEMO removed its reads on every cache size.

## 6. Files

| file | change |
|---|---|
| `src/game/Entity/entitylist.cpp` | the four switches and `kPrefetchCovEmitters` (4); `WalkCovVptrs` (particle / state machine vptrs, a struct of its own so `WalkVptrs`, captured by the DIRECTTICK / walk_batch tick lambdas, keeps its size); `PrefetchCovCharGrid`, `PrefetchCovCharTerrain`, `PrefetchCovParticleSlots`, `PrefetchCovParticleInstances` (all `always_inline`); the sub-arms in the S3 / S4 lambdas; `WalkPrefetchStaged` takes `cv`; includes `CharacterGrid.h`, `Game/Map.h`, `Game/terrain_sidecar.h`, `Game/WorldGeometry.h`, `graphics/ParticleInstance.h`, `Component/StateMachineComponent.h` |
| `src/game/Component/CharacterGrid.h` | `CharacterGrid::RectMemo`, `RectMemoAt(int)` (const accessor), private `static std::vector<RectMemo> s_rectMemo`; `#include <vector>` |
| `src/game/Component/charmanager.cpp` | `s_rectMemo`'s definition at file scope; `Place` uses it instead of its function-local static |
| `src/game/Component/particlecomponent.cpp` | `DEFINE_EXE_VTABLE(ParticleComponent, "_ZTV17ParticleComponent")` |
| `src/game/Component/statemachinecomponent.cpp` | `DEFINE_EXE_VTABLE(StateMachineComponent, "_ZTV21StateMachineComponent")` |

## 7. Build results (this block; static gate only)

- **Arms off = the shipped code.** The perf object of `entitylist.cpp` with all four `false`
  was disassembled with relocations (`objdump -dr`) and diffed against the same TU built from
  `642a56df4`'s `entitylist.cpp` in the same tree: **0 lines differ**. Both have 68 prefetch
  instructions.
- **Prefetch counts** (`objdump -d ... entitylist.cpp.obj | grep -c prefetch`, the perf object),
  per combination built:

| combination | count |
|---|---|
| all off | 68 |
| Char | 100 |
| TerrainLast | 84 |
| Char + TerrainLast | 116 |
| Particle | 88 |
| StateMachine | 72 |
| Char + TerrainLast + Particle | 136 |
| all four | 140 |

- `tools/build.sh build perf --jobs 2`: exit 0 in every combination above. There is no warning
  in the touched files. The only `entitylist.cpp` warnings are the pre-existing four
  unused-parameter ones in `ReportTreeCorrupt` and its neighbour, which are also present at HEAD.
- `tools/build.sh build debug --jobs 2`: exit 0 with all four `true` and with all four `false`.
  There is no warning in the touched files.
- `tools/build.sh unit perf --jobs 2`: **1130 tests, 0 failures**. The walk is not in the unit
  build, and no test was added: a prefetch has nothing to assert.
- `python tools/abi_audit.py`: OK.
- `tools/acceptance/acceptance.sh`: 0 lines off target.
- `python tools/castmetric.py src`: **total 2**. No casts were added: every new prefetch address
  is a typed field or element address, and the action-node offsets reuse the existing
  `CAST-OK: crash_walk` pointer `a`.
- **After `git merge main`** (UNSTAMP, `1eb77d007`; it does not touch `entitylist.cpp`), the
  following were all re-run on the merged tree:
  - perf build: exit 0, 68 prefetches with all off and 140 with all on.
  - debug build: exit 0.
  - unit perf: 1130 / 0.
  - abi_audit: OK.
  - acceptance: 0 lines off target.
  - castmetric: 2.
- Not run: the game, the stress town, the resave gate, COMPPROF (the coordinator's).

## 8. Doubts

- **The sidecar byte (C) is probably already cached.** The mirror is 256 KB. With 1 MB of L2
  per core on this box, a good part of it stays in L2 between iterations. The prefetch then
  costs an instruction and buys nothing. It stays in C because the ideas table named it, and
  because on a 512 KB-L2 part (Zen 3) it is a real L3 hit per character.
- **`a + 0x474` straddles at one alignment in eight.** MoveTo is `MemAlloc(0x480, 8)`. At
  `a % 64 == 8`, `field_0x474` (+0x474) and `speedMultiplier` (+0x478) sit on two lines, and the
  arm names only the first. Adding `a + 0x47B` would close the gap at the price of one more
  prefetch on every visit, duplicating the same line in the other seven cases. It was not
  added. The cost of the gap is one missed hint for 1/8 of MoveTo nodes.
- **Fill-buffer pressure** (PREFETCH §9.2's lesson). C adds up to 8 in-flight lines per
  character visit on top of variant 2's 6-14. If C regresses, that is the likely reason. A
  narrower C (only `a + 0x60` / `a + 0x474`, the MoveTo lines) would be the next thing to try.
  It is not built, because the A/B should say first whether C moves at all.
- **P's emitter cap of 4.** Buildings with more emitters get only the first four prefetched.
  The stress town's particle components are chimneys and fires (1-2 emitters), so the cap only
  bounds the loop.
- **T's pre-step position.** A moving character's last-iteration read is at the post-step x / y.
  The cell or heightmap row changes only when the step crosses a cell boundary, a few percent of
  steps at walking speed. In those cases the hint names the neighbouring line and the read
  misses as before.
- **UNSTAMP interaction (merged into this branch from main before the final build).** UNSTAMP
  keeps its frame stamp in `Component::_pad24`, which is inside the component's first 0x80
  bytes. S2 already prefetches those, so the probe line the ideas table wanted to cover is gone,
  and nothing in P needed to change.
