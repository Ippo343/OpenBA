# LUT hand-in (PORT GAP hunt, branch `d4-lut`, base `b9006081e`, main merged at `ca6cc314c`)

The .rdata tables and leftovers of the LUT row (GAPHUNT_RULES.md section 2), Application-x64.exe
only. The three Runtime items of the row were removed from this block by the coordinator.

**Static gate only.** The game was never run from this worktree. Every route below is owed.

## Gate (after `git merge main`, all three targets rebuilt)

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`) | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1002 / 0 (nothing added) |
| warnings (gate.sh `norm()`, `comm -23` against the main checkout's `runs/warn_baseline_{debug,perf}.txt`) | 0 new (debug, full build), 0 new (perf, union of every perf log of the block) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK |
| `b3r_session/acceptance.sh` | 0 line(s) off target |

Setup note: the fresh worktree had no `third_party/SDL`; it was initialised from the main
checkout's local clone (`-c submodule.third_party/SDL.url=<main>/third_party/SDL`, pinned
829a65d), no network fetch.

## Commits

| commit | title (short) | B11 (perf objects vs `b11base_d4e`) |
|---|---|---|
| `4c3ef32c3` | port: tile-shape variant LUT 0x1401c22d0 lifted into `kTileShapeVariantLut` | action_pf_geom IDENTICAL 15 |
| `474032507` | port: road mask-atlas rect LUT 0x1401c23d0 lifted into `kRoadMaskAtlasRect` | action_pf_road IDENTICAL 19 SAME 1 |
| `62d8c7dd2` | behaviour: quarry slope cutoff / corner offset 0x140172130 / 0x140172274, retail's corner order | quarrycomponent_deform DIFF 1 (OnMessage) IDENTICAL 13 RENAMED-IDENTICAL 1; worldgen IDENTICAL 18 |
| `88e65586c` | port: FUN_140022480 = `FindDescription<WorkPlaceDescription>`, the harvest's discarded lookup | action_base_cnr DIFF 1 (RemoveResource) IDENTICAL 14 |
| `3ebefc0a9` | behaviour: the region system build 0x14009e8c0 at `Map::Serialize`'s load tail | gamegrid_serialize DIFF 1 (Map::Serialize) IDENTICAL 9 |
| `78c9449dc` | behaviour: `EditTool::CloseInfoDialog` 0x1400fcc90 on Update's no-rect branch | edittool DIFF 1 (Update) IDENTICAL 28 RENAMED-IDENTICAL 1; edittool_select IDENTICAL 8 |
| `5afd11dae` | docs: the edittool comment's retail addresses (0x1400fcebc / 0x1400fcec8) | comment only |
| `631369793` | behaviour: DebugUI weather combo's audio arm 0x1400609a4-0x140060b4f | debugui DIFF 1 (OnWeatherComboChanged) IDENTICAL 21 ONLY-BASE 1 (inline fprintf, no caller left) RENAMED-SAME 1 SAME 6 |
| `2aba3ee1b` | behaviour: `Map::PlaceRoadCell` 0x14009c960, three tests dropped (row 1240) | roadtool_calcpath IDENTICAL 10 SAME 1 |
| `da93efbaa` | behaviour: `BridgeTool::SnapEndCell` / `ValidatePath`, four tests dropped (row 1180) | bridgetool_validate DIFF 1 (ValidatePath) IDENTICAL 7 SAME 1 |
| `cac85877a` | behaviour: the pick mesh registered once, through RegisterEntity's OnAdded pass (row 1261) | tool_base_tile DIFF 1 (PlacementTool::CreateAt) IDENTICAL 10; bridgetool_calcpath DIFF 1 (CalcPath) IDENTICAL 10 |
| `547e0b93d` | behaviour: the two `Map::StampCellRefs` callers, tests before the stamp dropped (row 1241) | pasturecomponent_states DIFF 1 (PastureProduce) IDENTICAL 23 RENAMED-SAME 1; orchardcomponent_states IDENTICAL 47 RENAMED-IDENTICAL 1 SAME 1 |
| `2eb9ddeb0` | behaviour: `RemoveResource` in retail 0x1401260f0's order, two raw-material tests dropped | action_base_cnr DIFF 1 (RemoveResource) IDENTICAL 14 |
| `60cffeb36` | docs: rows 1240 / 1241 / 1180 / 1261 FIXED (look owed); new rows 1291, 1292 | docs |
| `bc7f9a3e8` | merge main (clean: the coordinator's three Runtime closes) | |
| (this commit) | docs: this hand-in | docs |

`547e0b93d`'s message lost two code spans to shell expansion. The two sentences should read
"stamped only `if (pos)`: gone" and "keeps its own `if (!pos) return 1;`". `60cffeb36` records it.

SAME in b11pair means the code changed but calls, stores and compares did not. A dropped
`TEST reg,reg; JZ` looks like that.

## Items

| # | item | verdict | what was done |
|---|---|---|---|
| 1 | tile-shape variant LUT 0x1401c22d0 | ALREADY THERE | The 256 bytes read; the tree's copy was byte-identical. Consumer 0x1400ae180 builds the same 8-bit mask and reads `MOVSX byte [0x1401c22d0 + mask]`. Every reachable mask maps to 0..46, never -1. Lifted to a file-scope named constexpr. |
| 2 | mask-atlas rect LUT 0x1401c23d0 | ALREADY THERE | 47 rows x 4 uint16 (376 bytes; a pi/2 double follows). Consumer 0x1400acc10 indexes `[0x1401c23d0 + (int64)dirIdx * 8]`. The tree's flat 188-entry copy was word-identical. Lifted to `kRoadMaskAtlasRect[47][4]`. |
| 3 | slope cutoff 0x140172130, corner offset 0x140172274 | PORTED | The bytes are 0x3f59999a = 0.85f and 0xbe800000 = -0.25f, so both literals were exact. `worldgen.cpp:332` spells the same 0.85f exactly, and its comment now carries the bits. The corner order was wrong in the tree and is fixed (nothing visible, see the commit). |
| 4 | `FUN_140022480` | ALREADY THERE | `FindDescription<WorkPlaceDescription>` on the harvesting citizen's description. Retail discards the result (its out slot is overwritten before any read). Spelled as the call. |
| 5 | RegionSystem build 0x14009e8c0 | PORTED (wired) | The body already existed as two port-only halves, `Pathfinder_Seed` and `Region_Seed`. Every arm was checked against the disassembly. Both are now called where retail calls the body, and declared in `Game/exe_bodies.h` with their address ranges. Folding them into one member is row 1292 (not this block's files). |
| 6 | `Entity_impl_cc90` 0x1400fcc90 | PORTED (wired) | The body already existed as `EditTool::CloseInfoDialog`. The site's "on purpose" reason concerned the deleted registry and an old citizen rect failure. The citizen projection fallback now runs before the test. The call is wired. |
| 7 | `debugui.cpp:242 / :271` audio arm | PORTED | Retail's sequence: build the sound entity from the template, store it in `emitters[i]`, call `RegisterEntity` under the template's category, `FindComponent<AmbientEmitterComponent>`, `MoveTo(origin)`, then mute slot 0. The port-only warning print is gone. |
| 8a | row 1240 | FIXED (look owed) | Three tests dropped. |
| 8b | row 1241 | FIXED (look owed) | Pasture: the tests on the town, loop, map and MapComponent dropped; the livestock-type test kept, as retail has it. Orchard: `if (pos)` and the loop test dropped. The tail after the stamp is new row 1291. |
| 8c | row 1180 | FIXED (look owed) | Four tests and the helper's null-loop return dropped. |
| 8d | row 1261 | FIXED (pick route owed) | Both explicit calls dropped. `RefreshMesh` starts with `OnRemove`, so the second call removed the broadcast's mesh and re-added it under a new Picking::Scene id. |
| extra | `RemoveResource` order | PORTED | Found while reading item 4: `CountProducedMaterial` now runs where retail runs it, and two `rawMat` null tests are gone. |

Every `PORT GAP` line in the block's files is closed. The grep over action_pf_geom, action_pf_road,
action_base_cnr, quarrycomponent_deform, pasturecomponent*, gamegrid_serialize, worldgen, edittool,
roadtool_calcpath, bridgetool_validate, tool_* and debugui now finds none. The 12 lines at the start
were geom:201, road:222, cnr:69 / :378, quarry:77 / :81, gamegrid:77 / :307, edittool:368, and
debugui:216 / :242 / :271. Of those, `gamegrid:77` and `debugui:216` were comment mentions.

## Save format and world build

- **Tables (items 1, 2):** byte-identical to what the tree already used. No world or save change.
- **Quarry order:** only during play (Slot15). The set of converted cells is unchanged.
- **Region system (item 5):** load path only. The pathfinder thread, region graph and pair cache
  now exist from `Map::Serialize`'s load tail instead of after `SaveRestore` returns. The entity
  list is restored before the map (master serializer order). Nothing between the map serializer
  and the end of `SaveRestore` writes cells, so the per-cell flags and region ids should come out
  the same. Nothing they produce is saved (cell +0x18 is not serialized). **Resave: expect PASS
  at the current golden. `worldcmp.sh`: expect no difference**, because the new-game path is
  untouched. A resave DIFF here would be a finding, not a golden re-cut.
- **Row 1261:** `pickingId` is not persisted (`PickingComponent::Persist` writes only `userData`),
  so the different Picking::Scene id changes nothing saved.
- **Everything else:** UI and debug only, or dropped tests that never fire on a healthy run.

## Routes for the coordinator (merged exe)

1. **Road (row 1240).** Draw a road of a few cells. Expect the cells to queue (foundation look),
   builders to come, and the road to be built, as before.
2. **Bridge (rows 1180, 1261).** Drag a bridge across the river. Expect the valid and invalid tint
   as before. Place it, then click a built segment: the bridge's panel opens (the pick answers the
   master). Run `harness_route.sh pick` for row 1261.
3. **Building pick (row 1261).** Place a house and click it during and after construction. The
   panel should open and the clickable area should follow the drawn model. Run
   `harness_route.sh pick` on the placement path too.
4. **Info panel close (item 6).** Click a building so its panel opens unpinned, then scroll the
   camera until the building is fully off-screen. Expect the panel to close and the selection
   outline to clear, which is new. With the panel pinned it must stay open. Click a citizen and
   follow it: the panel must stay open while the citizen is on-screen. A citizen panel closing
   while the citizen is visible is a finding: it would mean the projection fallback failed.
5. **Load (item 5).** Load a save, or run the resave gate. `[PfSeed]` and `[RegionSeed]` now print
   during the load, before `[GUL::Load] LOAD-FROM-SAVE done`, and the post-load pair prints
   nothing. Citizens must keep their jobs after a few hundred frames (the `[GridProbe]` canary
   region ids stay non-zero). Paths and work ranges should behave as before.
6. **Quarry (item 3).** Build a quarry and let it produce. Expect the pit to deepen and the dug
   cells to turn to quarried rock, as before.
7. **Pasture / orchard (row 1241).** A pasture with animals producing, and an orchard reaching
   harvest. Expect behaviour as before.
8. **Harvest (`2eb9ddeb0`, item 4).** Woodcutters and gatherers dropping piles. Expect the stock
   counts and piles as before.
9. **DebugUI (item 7).** This needs whatever opens the DebugUI dialog; this block found no tree
   site that opens it. Pick a weather entry with a sound in its weather combo: a sound entity
   appears, held silent at the origin.

## Needs in other blocks' files

- **`pickingcomponent.cpp` (SLOTS2A):** `PickingComponent_RegisterMeshForEntity` has no caller left;
  delete it and its declaration in `pickingcomponent_funcs.h`.
- **`pathfinder_seed.cpp`, `region_seed.cpp`, `gameupdateloop_load.cpp` (ROWS / DRAW):** row 1292.
  Fold the two halves into one `Map` member at 0x14009e8c0 and drop their early returns, null
  returns and prints. Delete the post-load `Pathfinder_Seed` / `Region_Seed` pair in
  `GameUpdateLoop::Load`, which is now a no-op. Also check whether the new-game map create
  (`maptilegrid.cpp`) is missing 0x14009ef20's `FillZones` and the shore-link loop.
- **`orchardcomponent_states.cpp`:** state 9 enter (0x140051070) keeps `if (!pos) return 1;`,
  the same class of guard as row 1241 but outside the row. Also not this block's file.
- **`pasturecomponent_states.cpp`:** row 1291, the production tail (a gameplay body). This file
  is this block's, but the tail is beyond row 1241 and was not ported here.
- **`tornadocomponent.cpp`:** `AmbientEmitterComponent::MoveTo` has `&& gw` on the loop, which
  retail lacks (0x140042960 reads `[0x1401c3dd8]` unguarded).

Files touched outside the LUT file row, all at the coordinator's direct assignment:
`bridgetool_calcpath.cpp` (row 1261) and `orchardcomponent_states.cpp` (row 1241). Shared headers
appended: `Game/exe_bodies.h` (two declarations).

## Rows

- **Edited:** 1240, 1241, 1180, 1261. Status is "FIXED (D.4 LUT, `<commit>`)" with the route owed.
  These become FIXED for real only after the look.
- **Added:** 1291 (PastureProduce's tail, OPEN) and 1292 (the region system's two port-only halves
  plus the new-game create's missing calls, OPEN). Other blocks may have appended rows with the
  same numbers; renumber at merge if so.
