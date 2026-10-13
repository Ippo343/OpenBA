# CLEANUP: off-switched perf candidates removed from the tree

Branch `cleanup`, base `7c6dea32d`. Policy (the user's, 2026-10-13): a perf candidate that measured
neutral or worse does not stay in main behind an off switch. Its hand-in keeps the record and its
merge commit stays in history; the code goes. Shipped (ON) work is untouched. Only `src/` and
`tests/` changed; every hand-in and doc mention is as it was.

## 1. What was removed (one commit each, titled `cleanup:`)

| # | Commit | Switch (shipped value) | Removed |
|---|---|---|---|
| 1 | `7b3eb7725` | `XformMemo::kEnabled` (false), merge `9a85e586a` | `src/engine/graphics/xform_memo.h` (218 lines), `tests/unit/xform_memo_test.cpp` (274 lines, 5 tests); the eight sites put back to their pre-XFORM text by reversing the merge's diff (`mesh.cpp` x2, `meshinstance_draw.cpp` x2, `scene.cpp`, `wave27_lane_gfxbig.cpp` the `Scope`, `wave27_lane_gfxpart.cpp` x2, `geometry_draw.cpp`); `stress_harness.cpp`: the include, the `ItemTransform` shadow hook, `g_prof` wiring, `XformMemoReport` / `XformMemoReset` and their two calls (the 47 lines the merge added). The unit sources are globbed, so `tests/unit/sources.cmake` had no line to remove. |
| 2 | `bc8dc5c91` | `GatherPf::kCellHead` (false) | the switch and its comment block in `gather_prefetch.h`; the `if constexpr` cell-head prefetch and its comment in `Scene::AddCulledDrawItems` (`cull.cpp`). kChain / kDispatch / kDispatchMaterials untouched. |
| 3 | `f4319def3` | `kPrefetchCovChar`, `kPrefetchCovTerrainLast`, `kPrefetchCovStateMachine` (all false) | the three switches with their comments; `PrefetchCovCharGrid`, `PrefetchCovCharTerrain`; the char arm at stages i+2 / i+1 (the i+1 action prefetch is back to its pre-PREFETCHCOV one-liner); both StateMachine arms; `WalkCovVptrs::stateMachine` and its set; five includes only they used (`CharacterGrid.h`, `Game/Map.h`, `Game/terrain_sidecar.h`, `Game/WorldGeometry.h`, `StateMachineComponent.h`); `CharacterGrid::RectMemoAt`; `StateMachineComponent`'s `DEFINE_EXE_VTABLE` (no other reader). `kPrefetchCovParticle` (true) and its code unchanged. |
| 4 | `134b8d103` | `kDirectTick` (false) | the switch, `TickOneComponentDirect` and its three `static_assert`s, `<type_traits>`, the unused `dt` local and its capture in `MakeWalkTick`. `MakeWalkTick` is now byte-for-byte its pre-DIRECTTICK text (`8a696926f^1`). |
| 5 | `eafe264aa` | `WalkBatch::kPlaceEveryTick` (true) | the switch and its comment in `walk_batch_rules.h`; the three `if constexpr` guards in `walk_batch.cpp` (the bodies stay, unconditional); the comments naming the false variant in `walk_batch.cpp` / `walk_batch.h`. Unit test: the false-variant run `ext_walk_batch_two_worlds_bit_identical` removed; the `placeEveryTick` parameter dropped from `BatchTick` / `RunTwoWorlds` / `TwoWorlds` (always places); the `CHECK(WalkBatch::kPlaceEveryTick)` line removed; `ext_walk_batch_two_worlds_place_every_tick` still requires grid lag 0 on every tick. |
| 6 | `9de81a14b` | (no switch) the GATHERPF per-object counters | `++g_gatherCells` (per walked cell) and `++g_gatherChainObjects` (per ring object) in the faithful walk; both globals (`cull.cpp`, `gather_prefetch.h`); their reset; the `cells walked/frame`, `chain objects/frame`, `/cell` and `% of chain` fields of the COMPPROF `gather` row. Kept: `visible/frame`, `draw items appended/frame` (`/visible`), `passes/frame`. Reason: the coordinator's A/B read the all-switches-off tree 0.08-0.11 ms/frame slower than the CULLROUTE-only exe; the counts are in GATHERPF.md's Results. |

Not removed, and why:
- **Place's rect memo stays at class scope** (`CharacterGrid::s_rectMemo`, PREFETCHCOV moved it out
  of `Place`). Moving it back into a function-local static would change `Place`'s code (a guard
  variable and an atexit registration). Only the accessor `RectMemoAt` went; the comments now say
  why the memo lives there.
- The PREFETCHCOV introduction comment ("Each is its own switch so the A/B can take them one at a
  time") is left as written; it describes the one remaining arm correctly.

## 2. Object proof (perf flavour, `objdump -dr`)

**The baseline.** main's own `build/cmake-perf` objects date from 16:38, before `7c6dea32d` turned
XFORM off, and still carry the memo's table code (`std::vector<XformMemo::Table::Slot>::_M_fill_assign`
in `mesh.cpp.obj`). They are not main HEAD. So base `7c6dea32d` was checked out in a detached
scratch worktree and built with `tools/build.sh build perf --jobs 4`, the same command as this branch.
The scratch worktree has been removed since.

**Sweep.** `objdump -d` of all 980 objects, base vs this branch: 10 differ, all of them touched TUs.
These are byte-identical: `walk_batch.cpp`, `charmanager.cpp`, `wave9_lane_gfx.cpp`,
`particlecomponent.cpp` and every includer of `CharacterGrid.h` / `gather_prefetch.h`.

**Per TU.** `raw` is the count of differing `objdump -dr` lines; `functions` compares each function
by name with addresses, section offsets and nop padding masked.

| TU | raw | functions | Verdict |
|---|---|---|---|
| `mesh.cpp`, `meshinstance_draw.cpp`, `scene.cpp`, `wave27_lane_gfxbig.cpp`, `wave27_lane_gfxpart.cpp`, `geometry_draw.cpp` | 1801 / 1169 / 1594 / 1894 / 1629 / 2264 vs main | vs main: the XFORM site functions, plus the memo table's static init / `__tcf` teardown gone. **Against the pre-XFORM objects (`9a85e586a^1`, built the same way): 0 raw lines in all six.** | XFORM-off main still routed every site through `XformMemo::World`'s lambda and linked the memo's inline table. The restored text is identical to the pre-XFORM text and compiles to the pre-XFORM objects exactly. The 2-4 line diffs in `Geometry::Attach`, `UpdateAttached` and `ParticleInstance::AddDrawItems` are rip-relative displacements only. |
| `cull.cpp` | 1014 | 1 function, `AddCulledDrawItems`, 914 -> 910 insns | With displacements and branch targets masked, the diff is exactly 6 lines: two `addq $0x1,<.bss>` (`g_gatherCells`, `g_gatherChainObjects`) and their two relocations are gone, and one `mov %r13d,0x40(%rsp)` moved up one slot past where the first add sat. kCellHead was `if constexpr` false, so it compiled to nothing. |
| `entitylist.cpp` | 478 | `UpdateWalkBatch` (1896 = 1896 insns), `Set<ComponentEntity*>::Clear` (166 = 166) | Commit 3 (PREFETCHCOV) alone: **0 lines** against base. All of this is commit 4: removing the unused 16-byte `dt` capture from the walk_batch lambda. Restoring only `WalkVptrs dt{nullptr, nullptr};` plus `[time, charVt, dt]` gives **0 lines** against base. Same instruction count; register allocation and block order differ in `UpdateWalkBatch` (the `ext.walk_batch` instantiation, off by default), and `Set::Clear`'s block order differs. The faithful `EntityList::Update` path is identical. See section 4. |
| `statemachinecomponent.cpp` | 22 | `StateMachineComponent::Vtable()` gone; the ctor's trailing alignment padding differs (`data16 cs nopw` vs 11 `nop`) | Masked: 0 changed functions. |
| `stress_harness.cpp` | 21436 | 47 changed, 4 only-base, 1 only-ours | Intended: the `/xform` row, its shadow hook and the two gather fields are gone. With `Resolve()` smaller, GCC stopped splitting it into `Resolve() [clone .part.0]` (now `Resolve()`), so its callers changed shape: `StressHarness_Enabled` calls `Resolve()` directly instead of inlining its head. The `.data` / `.bss` layout and `__tcf_N` numbering moved, so most of the rest is relocation offsets. Diagnostics only. |

Surviving `gather` row fields: `visible/frame`, `draw items appended/frame (/visible)` and
`passes/frame`. `g_gatherVisible` is still added once per call in `cull.cpp` and `g_gatherDrawItems`
once per append in `wave9_lane_gfx.cpp`, whose object is byte-identical to base.

## 3. Static gate

| Check | Result |
|---|---|
| `tools/build.sh build perf --jobs 4` | exit 0; 183 warnings, the same as base, and no change in any touched file |
| `tools/build.sh build debug --jobs 4` | exit 0 |
| `tools/build.sh unit perf --jobs 4` | exit 0, **1147 tests, 0 failures** (1153 - 5 XFORM - 1 walk_batch false-variant run); 0 warnings in touched files |
| `python tools/abi_audit.py` | OK |
| `tools/acceptance/acceptance.sh` | 0 lines off target |
| `python tools/castmetric.py src` | total 2 |

`grep -rn` over `src tests` for every removed name returns nothing:
`XformMemo|xform_memo|ItemTransform|/xform|kCellHead|kPrefetchCovChar|kPrefetchCovTerrainLast|kPrefetchCovStateMachine|PrefetchCovChar|RectMemoAt|kDirectTick|TickOneComponentDirect|kPlaceEveryTick|g_gatherCells|g_gatherChainObjects`.

The game and the stress town were not run, per the brief.

## 4. Open: the DIRECTTICK capture

Commit 4 restores `MakeWalkTick` to the exact text it had before DIRECTTICK. The cost is that
`UpdateWalkBatch` (ext.walk_batch's walk) is not byte-identical to main: the same 1896 instructions
with different register allocation and block order. The unused `dt` capture was the only thing
holding main's layout. Two ways forward, the coordinator's call:
- **Keep as is (recommended).** The walk_batch walk is back to the object it had before DIRECTTICK
  merged, and the faithful walk is unchanged.
- **Restore the dead capture.** Two lines bring back main's object exactly, but they keep a
  DIRECTTICK leftover in the tree.
