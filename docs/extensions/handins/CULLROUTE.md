# CULLROUTE -- every frustum test through the prepared table

Block CULLROUTE, 2026-10-13. Idea 2 of IBS_20261013 section 3. The uncapped faithful IBS pass still showed the
reference bodies on the main thread: `SelectVertices` 2.6% + `RtVolume_Frustum_Contains` 2.3% of ops (~4.9%).
IBS_20261013_S50 (speed 50, everything on) then showed the same bodies at 9.6% through `ext.draw_gather`. This block
routes every remaining caller to FRUSTUMCULL's prepared table and adds a per-site breakdown to the `frustum cull` row.
Faithful perf work in FRUSTUMCULL's style: no registry entry, no settings key, no `BANISHED_*` flag, no `Opt()` field,
no `getenv`, no door. The kill switch is still `FrustumCull::kFastCull` (frustum_cull.h).
**The game was not run by this block.** The coordinator runs the COMPPROF=4 shadow, the row and the A/B.

**Short version.** Four faithful call sites and one extension site tested boxes with the reference body. Each now
builds a `FrustumCullPlanes` table once per frustum, right after the frustum's last writer, and tests every box with
`FrustumCull_Test(table, box, mask, verify)`. That is the existing shadow entry when `verify` (COMPPROF=4) and the
existing inlined fast body otherwise. The reference body, the NaN finding (row 1327) and the fast body's arithmetic are
untouched. FRUSTUMCULL section 1 proves the fast body bit-exact for every (frustum, box, mask), so no answer changes.
After this block nothing in `src/` calls `RtVolume_Frustum_Contains` except `FrustumCull_ContainsVerified` (the shadow)
and `FrustumCull_Contains` when `kFastCull` is false.

## 1. The callers (step 1)

`grep` over `src/` for `RtVolume_Frustum_Contains`, `SelectVertices`, `Contains@Frustum` and the retail address
0x1800f8e80, plus the indirect routes the block named:

- **`Volume.h` / member wrappers.** `Volume::Frustum` has no `Contains` member. `Volume.h:167` only declares
  `RtVolume_Frustum_Contains`. `SelectVertices` is in frustum.cpp's anonymous namespace, so its only caller is the
  reference body. Every `SelectVertices` sample is therefore a reference call.
- **The sun / spot shadow groups.** `SunLight::AddDrawItems` (wave28_lane_partsys.cpp) and `SpotLight::AddDrawItems`
  (lights_vtable.cpp) have no cull of their own. They open draw groups and call `AddGlobalDrawItems` and
  `AddCulledDrawItems`. That makes the scene walk run once per group, which FRUSTUMCULL already routed. It also makes
  every GLOBAL geometry's `AddDrawItems` run once per group, and the terrain is a global (below).
- **`Renderer::EndDrawGroup`, picking, `scene_grid.cpp`, `NodeBoxShadow`.** None of them tests a frustum. The node boxes
  feed the scene walk's tests, which are already routed.
- **The minimap capture** (`Minimap::PaintTerrain`, minimap_paint.cpp:211) builds a stack `Scene` holding the terrain
  and the sun, and calls `renderer.Draw` on it. It reaches the terrain site. It runs on the main thread, in
  `GameUpdateLoop::Draw` step 3, and only while the minimap's terrain image is null: once after a world build or a
  device restart, never per frame.
- **`ext.eye_camera`** is the only other `Renderer::Draw` caller. It draws on the main thread and reaches the same
  sites.
- **`cull.cpp:143`.** The comment said `kFastCull = false` "in frustum.cpp" makes it the reference again. The switch
  lives in frustum_cull.h. It was a stale comment, not a hidden route, and it is fixed.

| site | frustum (space) | mask | boxes per call (large map) | calls per frame | thread |
|---|---|---|---|---|---|
| `Scene::AddCulledDrawItems` (cull.cpp) | `cullFrustum` / `shadowFrustum` (world) | the walk's | ~11k (FRUSTUMCULL row) | 17-21 (the row's passes) | main; **already routed** |
| `WorldGeometry::AddDrawItems`, chunk grid (geometry_draw.cpp) | `localFrustum` (geometry-local) | 0x3F | chunkCount^2 = (1024 / 32)^2 = **1024** | one per draw group with bit 0 in its pass mask, ~P | main |
| same, decals | `localFrustum` | 0x3F | D = live decal slots (`material != null`); one per building with a `DecalComponent` | groups with `shader != 1`, ~1 + S | main |
| same, road batches | `localFrustum` | 0x3F | R = road batches with vertices: at most (512 / 32)^2 = 256 blocks x road materials | ~1 + S | main |
| `WeatherParticleGeometry::Draw` (weather_particle_draw.cpp) | `worldFrustum` (world) | 0x3f | 5 x 5 x 5 = **125** | one per weather draw item, ~1, 0 with no weather | main |
| `ext.draw_gather` `Walk` / `BoxPasses` (draw_gather.cpp) | `f.cull` / `f.shadow` (world) | the walk's | the scene walk's ~11k | 17-21 | main; class A, OFF in faithful |

**The arithmetic.** On a large map, `worldgen.cpp:716` sets `_gridSize = 2 x mapSize = 1024` and `_chunkSize = 32`, so
chunkCount = 32. The terrain is a scene global. `serialize_arrays.cpp:112` sets its `_materialTypeFlags = 1`.
`AddGlobalDrawItems` (and the inline global loop of `Renderer::Draw`) calls its `AddDrawItems` in every draw group
whose pass mask has bit 0. That is every group in the frame: the scene group (0x8007), the sun group (1), and both
spot-light groups (1 and 3). So the chunk loop runs once per group: P is about 19 groups, and 1024 x 19 is about 19.5k
boxes per frame.

`WorldGeometry::AddDrawItems` returns right after the chunk loop when `GetShader() == 1`. So the decals and roads are
tested only in groups with `shader != 1`: the scene group and the second group of each spot light. With P = 2 + 2S (one scene,
one sun, two per spot light), 17-21 passes gives S of about 8, and decals plus roads are tested about 9 times a frame.
With D + R of about 1000 in the heavy spread town, that is about 9k boxes per frame. Weather adds at most 125.

| reference boxes per frame (heavy town, estimate) | |
|---|---|
| terrain chunks | ~19.5k |
| decals + roads | ~9k (D + R ~1000, x ~9 groups) |
| weather tiles | <= 125 |
| **total** | **~29k** |

**Does it reach ~5%?** The fast scene cull is ~18% of ops (`FrustumCull_ContainsFast` 3.6 plus the attributed
intrinsics ~14.5) for ~200k boxes per frame, so about 0.09% of ops per 1k boxes. 4.9% over ~29k boxes is about 0.17%
per 1k boxes, so a reference test costs about 1.9x a fast one. That is consistent with what we know. FRUSTUMCULL found
the fast body about 1.5x cheaper per box on the walk's hierarchical masks. These sites always pass 0x3F, which is the
reference body's worst case: six `SelectVertices` and up to twelve dot products unless an outside plane comes early.
The fast body costs the same whatever the mask. So the arithmetic does reach 5%. The hidden caller is the terrain, run
once per draw group rather than once per frame, and its decal and road loops.

**Not counted, only inferred:** S (the spot lights) comes from the pass count, and D + R from the town's building
count. The new row (section 4) gives both directly.

## 2. Routing and the writer census (step 2)

Every site follows the same pattern, in the same place: right after the frustum's last writer.

```
FrustumCullPlanes planes;                 // 16-aligned stack object
FrustumCull_Prepare(&frustum, &planes);   // after the last write to `frustum`
const bool cullVerify = g_frustumCullVerify != 0;   // read once, COMPPROF=4 only
... r = FrustumCull_Test(&planes, &box, mask, cullVerify);   // per box
FrustumCull_CountSite(site, boxes);       // once per call
```

`FrustumCull_Test` (frustum_cull.h) is `verify ? FrustumCull_ContainsVerified : FrustumCull_Contains`, the same branch
the scene walk's lambda makes. The shadow `FrustumCull` and the `kFastCull` switch therefore cover every new site with
no new hook.

FRUSTUMCULL section 2's argument, per site: the table is a value snapshot, so only writers of the frustum between
`Prepare` and the last test matter. Writers elsewhere in the tree are irrelevant.

| site | frustum | writers in the function | after Prepare |
|---|---|---|---|
| `WorldGeometry::AddDrawItems` | local `::Frustum localFrustum = {}` | two: the `= {}` initialiser and `RtVolume_Frustum_MulTransform3(&worldFrustum, &localFrustum, ...)` as `ret`. Prepare is on the next line. | none. Its address reaches only `Prepare` (const). The loops call `renderer.AddDrawItem(this, ...)`, which cannot name the local. `PerfX::SetWorldFrustum(&worldFrustum)` takes `const void*`, memcpys, and runs before Prepare on the OTHER local. |
| `WeatherParticleGeometry::Draw` | local `::Frustum worldFrustum` | one: `RtVolume_Frustum_MulTransform3(&fr, &worldFrustum, &camXform)`. Prepare is on the next line. | none. The tile loop writes only `tiles[]`, `visibleTiles` and `aabb`. |
| `ext.draw_gather` `ExtCulledGather` | `Frame f`'s `cull` / `shadow` members | one each: the two `RtVolume_Frustum_MulTransform3` (`ret` = `&f.cull`, `&f.shadow`). Both Prepares come after the second. | none. `Walk`, `BoxPasses`, `RetailTest` and `SideTest` take `const Frame&`. The only mutable member is the new box counter. A nested gather (the re-entrant `MaterialInstance::RenderTextures` dispatch) builds its own `Frame` on its own stack. |
| `Scene::AddCulledDrawItems` | unchanged (FRUSTUMCULL) | | |

**Alignment.** `FrustumCullPlanes` is `alignas(16)`, and `_mm_load_ps` needs it. The Win64 ABI keeps RSP 16-aligned at
every call, and GCC honours a 16-byte `alignas` on a local with no realignment code. Only alignments above 16 are a
MinGW problem. It is the same object, from the same compiler, that FRUSTUMCULL's site has used since 2026-10-12. In
draw_gather the tables are members of `Frame`, which makes `Frame` 16-aligned. `Frame` is a stack local, never
heap-allocated and never in a `std::vector`.

**Threads.** All five sites run inside `Renderer::Draw` on the main thread:
- the terrain from the global visits, and once per minimap capture from `GameUpdateLoop::Draw` step 3;
- the weather from the per-item draw;
- the gather from the scene groups and the light groups nested in them.

No worker thread owns a `Video::Context` or calls `Renderer::Draw`. The loader builds the world but does not draw, and
the path workers and audio threads never reach graphics. The tables are stack locals, so thread-safe by construction.
The per-site counters are plain `uint64_t` globals, written on the main thread only, like FRUSTUMCULL's.

**What the objects show** (perf build, `nm -C` on the routed objects): `geometry_draw.cpp.obj`,
`weather_particle_draw.cpp.obj`, `draw_gather.cpp.obj` and `cull.cpp.obj` refer to neither
`RtVolume_Frustum_Contains` nor an out-of-line `FrustumCull_ContainsFast`. The fast body is inlined at every site, and
the reference is reached only through the out-of-line shadow entry in frustum.cpp.

## 3. The shadow (unchanged hook, more sites)

COMPPROF=4 sets `g_frustumCullVerify` (stress_harness.cpp, unchanged). Every routed site reads it once per call, so
every box at every site goes through `FrustumCull_ContainsVerified`. That runs both bodies, reports
`StressHarness_Verify("FrustumCull", fast == ref, (ref << 8) | fast)`, and returns the reference. `[VERIFY] FrustumCull
checks=N mismatches=M` now counts all sites together. **Expected M = 0.** N grows by about 29k per frame over
FRUSTUMCULL's figure.

## 4. The row (step 3, COMPPROF >= 1)

The existing line is kept as is: `frustum cull: boxes tested/frame=... cull passes/frame=...` is the scene walk only, so
it stays comparable with FRUSTUMCULL's numbers. A second line follows it:

```
[COMPPROF]   frustum cull sites: all 229000 boxes/frame;  scene 200000 (19.00 calls)  terrain chunks 19456 (19.00 calls)  terrain decals 8100 (9.00 calls)  terrain roads 900 (9.00 calls)  weather tiles 125 (1.00 calls)  ext.draw_gather 0 (0.00 calls)
```

(The numbers above are this hand-in's estimate, not a measurement.) The line shows boxes and calls per frame for each
site, from `g_frustumCullSiteBoxes` / `g_frustumCullSiteCalls[FrustumCullSite]`. Each site adds its count once per
call; the gather's per-box count is a local or a stack member. Both are reset per window with the existing counters.

What to read from it:
- **terrain chunks / calls** must be exactly 1024 on a large map (576 on medium, 256 on small).
- **calls/frame** for the chunks gives P, and for decals and roads gives 1 + S.
- **ext.draw_gather** is 0 in the faithful profile, and about equal to `scene` with the extension on. The scene line
  then reads 0, because the extension replaces the whole gather.

There is no counter of reference calls, since the reference body is not touched. That the reference is gone shows in
IBS: `SelectVertices` and `RtVolume_Frustum_Contains` should leave the main-thread table in both profiles. The only
way to reach them is now the shadow or `kFastCull = false`.

## 5. Unit test (`tests/unit/frustum_cull_test.cpp`, `frustum_cull_routed_sites`)

- 400 frusta (1 in 5 orthographic), each with a geometry-local copy (a second `MulTransform3`) and one table per frustum.
- The routed sites' own box shapes, all at mask 0x3F:
  - the large map's 32 x 32 chunk grid (z -16..16);
  - 64 flat decal / road boxes (z = 0, integer corners);
  - the 5 x 5 x 5 weather tiles around a snapped point.
- Results: 485,200 tests (culled 462,962, inside 15,126, partial 7,112).
  - `FrustumCull_Test` with verify false equals `RtVolume_Frustum_Contains` on every test, and so does verify true.
  - The shadow reported every test, all ok.
  - `FrustumCull_CountSite` adds boxes and one call per call.

FRUSTUMCULL's 2.26M-case proof of the body itself is unchanged.

## 6. How to measure (coordinator)

1. **Shadow:** `BANISHED_STRESS_COMPPROF=4`, heavy town, once faithful and once with `ext.draw_gather` on. Expect
   `[VERIFY] FrustumCull ... mismatches=0`, plus `DrawGather` / `DrawGatherSide` still 0 in the extension run.
2. **Row:** any COMPPROF >= 1 run prints the `frustum cull sites` line. Check:
   - terrain chunks = 1024 boxes per call;
   - the per-frame totals against section 1's ~19.5k chunks and ~9k decals plus roads (if they differ widely, the
     estimate of S or D + R was wrong, not the routing);
   - `ext.draw_gather` is 0 when faithful.
3. **A/B:** `FrustumCull::kFastCull` true vs false (frustum_cull.h, rebuild perf). Since this block, false turns the
   scene walk AND the new sites back to the reference. For the new sites alone, compare against main at `248b1618f` /
   `23e418cd5`, or cherry-pick a revert of the routing commit.
   - Run conditions: COMPPROF=0; Afterburner, RTSS AND `RTSSHooksLoader64` stopped; a CLEAN machine; interleaved pairs;
     heavy and compact towns at speed 10 uncapped.
   - **A/B column:** `draw_ms_mean` and fps. With `ext.draw_gather` on: the speed-50 everything-on fps / sim iterations
     per frame.
   - **Must not move:** `update_ms_per_iter` (sim ms / iteration) and the resave golden. The cull is presentation-only,
     and the answers are identical.
4. **IBS:** `SelectVertices` and `RtVolume_Frustum_Contains` absent from `main_thread_by_function.txt` in both profiles.

## 7. Prediction

- **Faithful, speed 10 uncapped, heavy town.** ~29k reference tests per frame become fast tests: ~4.9% of main-thread
  ops become ~2.6%, saving **~2.3% of main-thread ops**. In wall time, FRUSTUMCULL's ratio applies: an 11-point op-share
  drop gave -2% draw. So expect **draw ms -0.5 to -1.5%, fps +0.5 to +1.5%**. That is about the noise of one pair, so
  it needs the interleaved pairs. Compact town: smaller, since the chunk count is set by the map, not the town.
- **ext.draw_gather on, speed 50.** 9.6% of main-thread ops ran the reference. The walk's masks are hierarchical, so
  FRUSTUMCULL's ~1.5x per-box ratio applies: **~3-4% of main-thread ops saved**. With ~60% of the main thread in the
  sim at speed 50, that is about **+1-3% sim iterations per frame**. The draw ms column with the extension on should
  drop by roughly the FRUSTUMCULL amount (-2%).
- `update_ms_per_iter`: unchanged in both. The resave golden: unchanged.

## 8. Doubts

- **S and D + R are inferred.** S comes from FRUSTUMCULL's 17-21 passes per frame, and D + R from the town size. If the
  heavy town has fewer spot-light groups, the terrain share shrinks proportionally and the 5% needs another source.
  The row settles it. No other caller exists in `src/` (section 1's grep), so a mismatch would mean wrong counts, not a
  missing site.
- **The weather site runs only with weather on.** It is at most 125 tests per frame, negligible either way.
- **Off x86 (ARM64 Linux):** the fast body is the reference body there (FRUSTUMCULL section 8). The routing then costs
  one `Prepare` per call, 1-3 per site per group, which is noise.
- **`Prepare` cost per call:** one ~40-store loop, at most twice per site per draw group, about 60 per frame. Negligible
  next to 1024 chunk tests.
- **Retail shape.** The game code (geometry_draw.cpp, weather_particle_draw.cpp) now calls an engine helper retail does
  not have. The answers are the reference's bit for bit (FRUSTUMCULL section 1, plus section 5 above on these exact box
  shapes), and the switch restores the reference.
- **ext.draw_gather's DrawGather shadow** compares the side-array walk with a `RetailTest` walk. Both now test through
  the tables. At COMPPROF=4 both take the verified path and return the reference, so the comparison is unchanged. The
  per-site count for the extension doubles at COMPPROF=4 (two walks), which is a diagnostic-only artefact.
- Not run in the game. The first verification is the coordinator's COMPPROF=4 run.

## 9. Files

| file | change |
|---|---|
| `src/engine/volume/frustum_cull.h` | `FrustumCull_Test`, the `FrustumCullSite` enum, `g_frustumCullSiteBoxes` / `Calls`, `FrustumCull_CountSite` |
| `src/engine/volume/frustum.cpp` | the two counter arrays (the reference body unchanged) |
| `src/engine/graphics/cull.cpp` | the scene site also records into the per-site array; the stale `kFastCull` comment fixed |
| `src/game/Game/geometry_draw.cpp` | `localPlanes` after the local frustum's MulTransform3; chunks, decals and roads routed and counted |
| `src/game/Game/weather_particle_draw.cpp` | `worldPlanes` after its MulTransform3; the 125 tiles routed and counted |
| `src/ext/draw_gather/draw_gather.cpp` | `Frame` carries both tables plus verify and count; `Contains` over the tables |
| `src/diag/stress_harness.cpp` | the `frustum cull sites` line and its window reset |
| `tests/unit/frustum_cull_test.cpp` | `frustum_cull_routed_sites` |

## 10. Static gate

Run in the worktree after `git merge main` (main at `06a7c5886`, merge `b4b34ce21`; the merge brought docs and
`perf/uprof` files only), foreground, `--jobs 2`, after the coordinator's `ibs_done`:

| check | result |
|---|---|
| `tools/build.sh build perf --jobs 2` | OK (exit 0); 0 warnings in the touched files (the full compile before the merge) |
| `tools/build.sh build debug --jobs 2` | OK (exit 0); 0 warnings in the touched files |
| `tools/build.sh unit perf --jobs 2` | **1144 tests, 0 failures** (1 new: `frustum_cull_routed_sites`) |
| `python tools/abi_audit.py` | OK (`FrustumCullPlanes` and `Frame` are not pinned layouts) |
| `tools/acceptance/acceptance.sh` | 0 lines off target |
| `python tools/castmetric.py src` | total 2 |
| `nm -C` on the four routed perf objects | no reference to `RtVolume_Frustum_Contains`, no out-of-line `FrustumCull_ContainsFast` |

Not run: the game, the stress town, the COMPPROF=4 shadow, the A/B, IBS (the coordinator's, per the block rules).
