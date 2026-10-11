# RESIDUE hand-in (PORT GAP hunt wave 2, branch `d4-residue`, base `560ddd480`, main merged at `e40044cbc`)

The stand-ins and needs the four wave-1 blocks (SLOTS2A, SLOTS2B, CAI, LUT) recorded, rows
1291-1295, row 41 and row 861 (GAPHUNT_RULES.md section 2, the brief's ten items).

**Static gate only. The game was never run from this worktree.** Every route in section 5 is the
coordinator's, on the merged exe.

## 1. Gate (merged tree)

| check | result |
|---|---|
| `standalone-debug`, `standalone-perf`, `unit` (`-j2`, after `git merge main`) | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1002 / 0 (nothing added) |
| warnings (gate.sh `norm()`, `comm -23` against the main checkout's `runs/warn_baseline_{debug,perf}.txt`, union of every log of the block) | 0 new (debug), 0 new (perf) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK |

The first debug build failed once on a split printf literal (a heredoc ate `\n`); it was fixed before
any commit, and its one warning line is that error.

The commit messages carry `Co-Authored-By: Claude Opus 5.5`. That is the attribution the session's
system reminder gives; it differs from the brief's line (CAI made the same call).

## 2. Commits and B11 verdicts

B11 is `b11pair.py` against `runs/b11base_d4e/perf/obj` (main `1e842b306`), so a TU that a wave-1
block also touched shows that block's DIFFs too. SAME means the code changed but no call, store or
compare did (a dropped `TEST; JZ` reads that way).

| commit | title (short) | B11 |
|---|---|---|
| `eb3dc6042` | behaviour: PickingComponent::RefreshMesh = 0x140128f50 (row 1291) | pickingcomponent.o: DIFF 1 (RefreshMesh) IDENTICAL 16 ONLY-BASE 3 ONLY-TIP 1 (SLOTS2A's Startup) RENAMED-SAME 1 SAME 1 |
| `09bed7566` | behaviour: StateMachineComponent::Startup = 0x14012b6a0, slots [6] / [8] / [9] (row 1291) | statemachinecomponent.o: DIFF 10 IDENTICAL 16 ONLY-BASE 5 (BodyInit, LinkControllersStandalone, ...) RENAMED-SAME 1 SAME 3 |
| `50c0f28d3` | behaviour: AutoPickupComponent::Startup = 0x140044990 (row 1291) | autopickupcomponent.o: DIFF 2 (SLOTS2A's) IDENTICAL 13 RENAMED-SAME 1 SAME 4 (Startup SAME) |
| `4cd413d91` | behaviour: PastureProduce's tail = 0x1401267e0 from 0x140126889 (row 1293) | pasturecomponent_states.o: DIFF 1 (PastureProduce) IDENTICAL 17 RENAMED-SAME 1 SAME 6 |
| `827d0ef75` | behaviour: the worldgen map create = 0x14009ef20, AddWaterAccess = 0x140088b80 (row 1292) | maptilegrid.o: DIFF 1 IDENTICAL 7; wellcomponent.o: IDENTICAL 37 ONLY-TIP 1 RENAMED-SAME 1 |
| `70f2061f4` | behaviour: SendComponentMessage ctor / AbstractImpl without the heal and the trace (row 1294) | sendcomponentmessage.o: DIFF 2 IDENTICAL 16 ONLY-BASE 1 ONLY-TIP 1 RENAMED-SAME 1 |
| `b0fe4cda9` | behaviour: UI::UserInterface::BeginSpriteCapture = Runtime 0x18011f4c0 (row 41) | userinterface.o: IDENTICAL 31 ONLY-TIP 1 |
| `800a49d66` | behaviour: SpriteCaptureUI::OnElementEvent = 0x140079620 -> 0x1400792b0 (row 41) | spritecaptureui.o: IDENTICAL 15 ONLY-TIP 2; ui_dialog_stub_factories.o: IDENTICAL 50 ONLY-BASE 1 RENAMED-IDENTICAL 1 |
| `267a53d32` | port: the GameContainer stand-ins go | game_main.o DIFF 1 IDENTICAL 9 ONLY-TIP 1; menudialogloopt_stubs.o DIFF 2 IDENTICAL 20 ONLY-BASE 1 SAME 1; inputbindings.o IDENTICAL 7 ONLY-BASE 1 SAME 1; dialog_exe_helpers.o IDENTICAL 7 ONLY-BASE 2; mainmenudialog.o DIFF 2 IDENTICAL 34 RENAMED-IDENTICAL 1; newgamedialog.o DIFF 1 IDENTICAL 27 RENAMED-IDENTICAL 1; scenariodialog.o DIFF 1 IDENTICAL 22 |
| `56d43c0a7` | behaviour: BuildComponent::State8Finalize = 0x140107ff0 without the decal event | buildcomponent_state8.o: DIFF 1 IDENTICAL 14 |
| `0f5912fba` | port: CitizenComponent::Update's icon flags call SetFlag 0x14012c3f0 directly | citizencomponent.o: DIFF 1 (five Toggle -> five SetFlag) IDENTICAL 34 RENAMED-IDENTICAL 1; statusiconcomponent.o: IDENTICAL 19 ONLY-BASE 1 RENAMED-SAME 1 SAME 6 |
| `08962c7ab` | behaviour: AmbientEmitterComponent::MoveTo = 0x140042960; the tornado banner | tornadocomponent.o: DIFF 1 (MoveTo) IDENTICAL 28 RENAMED-IDENTICAL 1 |
| `18bc8f044` | behaviour: CitizenAIComponent::Update / Startup / the age-mesh setters as retail | citizenaicomponent.o: DIFF 8 (CAI's included) IDENTICAL 11 RENAMED-IDENTICAL 1 SAME 4 |
| `5116b4b6c` | behaviour: OrchardComponent's state-9 enter = 0x140051070 | orchardcomponent_states.o: DIFF 1 IDENTICAL 38 RENAMED-SAME 1 SAME 9 |
| `ce88e2a43` | docs: retail bug 3 (HappinessComponent::OnMessage) | docs |
| `e40791151` | docs: rows 41, 861, 1291-1295; new rows 1310-1312 | docs |
| `2f6cc9150` | Merge main (DRAW, DRAW-2, ROWS) | one conflict in known_port_issues.md, resolved (main's rows kept, RESIDUE's cells re-applied) |
| (this commit) | docs: this hand-in | docs |

## 3. Items

| # | item | verdict | what |
|---|---|---|---|
| 1a | RefreshMesh 0x140128f50 (row 1291) | PORTED | Read from the disassembly. The userData seat, the six null tests, the singleton test and the `[PickMesh]` lines are gone. |
| 1b | `PickingComponent_RegisterMeshForEntity` (LUT's need) | PORTED (deleted) | It had no caller left. `pickingcomponent_funcs.h` stays, because starttown.cpp (ROWS) includes it. |
| 1c | StateMachineComponent BodyInit / lazy arms / Startup (row 1291) | PORTED | Startup is retail's linker: the first controller's default goes into +0x52 unconditionally. OnAdded / Tick / Update are bare Tick calls. The history was root-caused (section 4). |
| 1d | AutoPickupComponent::Startup's description test (row 1291) | PORTED | Read unguarded. |
| 2 | PastureProduce's tail (row 1293) | PORTED | Ported in full, including the occupant arm (0x1400223c0 = FindComponent<RawMaterialComponent>). `full` is computed once, there is no cap, no test retail lacks, and no print. |
| 3 | The map create 0x14009ef20 (row 1292) | PORTED | The create lives in `maptilegrid.cpp`, not `worldgen.cpp`. It now runs FillZones, the region build and the shore loop, in retail's order. 0x140088b80 had no body and is now `CitizenBehavior::AddWaterAccess`. |
| 3' | Folding Pathfinder_Seed / Region_Seed into one member | LEFT | Not in the brief's item. It lives in pathfinder_seed.cpp / region_seed.cpp, and row 1292 keeps it OPEN. |
| 4 | SendComponentMessage heal and trace (row 1294) | PORTED | Both are dropped. Every sender hands a TypeId() (re-read). |
| 5 | BeginSpriteCapture 0x18011f4c0 + SpriteCaptureUI [9] (row 41) | PORTED | objdump of Runtime-x64.dll gives five stores. [9] and StartCapture 0x1400792b0 were read myself and match SLOTS2B's reading. Both PORT GAP lines are closed. |
| 6a | game_main.cpp boot stand-ins | PORTED (deleted) | RegisterDefaultInputBindings, LoadInputNameTables and StandaloneEnsureSpriteSheetLoaded went at boot. GameContainer_Init overwrote every one of those slots before any reader ran. `LoadInputNameTables` is deleted (caller-less). |
| 6b | `GameContainer_EnsureLists` + its four calls | PORTED (deleted) | Removed from MainMenuDialog OpenLoad / OpenAchievement, NewGameDialog OnOpened (and its wrapper), and ScenarioDialog OnOpened. |
| 6c | The menu loop's EnsureCursorsLoaded / EnsureUISoundsLoaded / StandaloneEnsureSpriteSheetLoaded | PORTED (deleted) | SLOTS2B's own note named them redundant once the container is filled. Each returned at once on a filled slot. |
| 6d | Row 1295 (`~MenuDialogLoopT`'s `_loadBit = -1`) | RULED, kept | The row's exit condition is only half met. The container copies are gone, but game_main.cpp still preloads Dialog/ShiningRock.rsc and Dialog/Loading.rsc on the main thread under bit 0. Those are a flash-avoidance step, not a GameContainer stand-in. The comment and the row now name only them. |
| 6e | `DecalComponent_MarkOwnerBuilt` in State8Finalize | PORTED (dropped) | 0x140107ff0 and the 8->2 action 0x1401075e0 call no decal sender. The start-town copy is ROWS's (row 1311). |
| 7 | Citizen status-icon adapter (SLOTS2A's need) | PORTED | The five sites call `StatusIconComponent::SetFlag`. The adapter, `StatusIcon_Toggle` and the stale comment are gone. |
| 8 | tornadocomponent.cpp:134 banner | PORTED | Now names `FindComponent<AmbientEmitterComponent>`. |
| 9a | CitizenAIComponent::Update harness and guards | PORTED | The lazy Startup retry, the four-pointer bail-out, Decide's storage / citizen tests and the per-call type-cell repoint are gone. Startup is retail's one store. **Kept:** the three force harnesses, each behind its existing Opt() flag (forceWork, forceForesterWork, forceConstruct; default off). |
| 9b | `CitizenComp_SetupAgeMesh`'s two null returns | PORTED | 0x140115110 reads both pointers unguarded. The debug-only `[SetupAgeMesh]` print is gone too. |
| 9c | Orchard state-9 `if (!pos)` | PORTED | Ported as the whole 0x140051070 body: the description / type-id / spacing / stride / slots tests and the `[Orchard9]` traces are gone too. |
| 9d | `MoveTo`'s `&& gw` (it is `AmbientEmitterComponent::MoveTo`, in tornadocomponent.cpp) | PORTED | 0x140042960 reads [0x1401c3dd8] unguarded. |
| 9e | Row 861 | ALREADY THERE | B6 WORK `b33ebb2cb` already passes `context`. Only the status cell was wrong (it read OPEN and FIXED at once). |
| 10 | retail_bugs.md (Happiness) | PORTED | Entry 3, with the addresses re-read (0x14004a824, 0x14004a8da, 0x14004a8e6, 0x14004a907). |

No `PORT GAP` line is left in any file this block touched.

## 4. StateMachineComponent: the history and why the faithful Startup holds

The comments recorded four measured regressions. Each was re-read against today's tree.

1. **Founding houses back in state 1.** A lazy link ran after StartTown stamped +0x52 = 2. Today the
   stamp is a post-RegisterEntity fallback (starttown.cpp ~1690, ROWS's file). Startup runs inside the
   entity ctor, before anyone holds the entity.
2. **The stockpile registered twice after a load.** A lazy link ran on a stream-restored machine.
   Startup never runs on a load: ComponentEntity::Persist builds the components, and Slot11
   (d38bc74e7) relinks them.
3. **The 0xff "nothing pending" rule.** It came from Ticks on an unlinked chain. Every created
   machine is now linked at create.
4. **The zero-default skip, `ownsZero`, and the destroy-out-of-order rule.** These were measured while
   controllers were inert. All 23 controllers answer GetController with their member node.

The templates order their controllers for retail's rule:
- WoodHouse, TownHall and TradingPost list `cleararea` first. Its default 0 is ClearArea's own start
  state: 0 -> {1, 3} -> 2.
- StorageYard lists `absorbresource` first.

A default that no node owns lands on the root's empty state 0, which is retail's RootController.

## 5. Save format, and what to look at

**Resave golden: should NOT move.** None of these bodies runs on load-then-save:
- **Startup** runs at create only.
- **The map create** runs on a new game only.
- **RefreshMesh** writes `pickingId`, which is not serialized.
- **The stand-in deletions** only change who fills the container.

A resave DIFF is a finding. The one caveat is a gate that ticks frames before saving while a pasture's
accumulator is over its threshold: PastureProduce now drops more than 16 piles, and in a different order.

**New-game saves change:**
- The town's +0x130 water-access pairs are now written (they were empty before).
- Pasture output differs over time.
- Created machines may hold a different +0x52 wherever the old lazy linker picked a later controller's
  default.

Routes, one per fix:
1. **StateMachine Startup (`09bed7566`).** Check new game 5694 / 0 and tutorial 5773 / 12.
   - Place a house over trees: the trees are cleared first, then building starts.
   - Place a stockpile.
   - Watch the start town's buildings come up built, and the stockpile accepting goods.
   - A building parked in the wrong state, or a crash in StateMachine::Tick's parent climb, isolates
     to this commit.
2. **RefreshMesh (`eb3dc6042`).** Run `harness_route.sh pick`. Click a building during and after
   construction, a citizen and an animal; each opens its panel.
3. **PastureProduce (`4cd413d91`).** Build a pasture and stock it.
   - Over a few days, produce piles appear on its footprint and the production panel counts them.
   - Fill the town's stock of that good: the piles stop.
4. **Map create (`827d0ef75`).** Start a new game on a map with a lake or river and set a building
   near the shore on fire (the disaster route). Firefighters fetch water from the shore, not only from
   wells. If `worldcmp.sh` reads the town or the cell zones, expect the +0x130 array and the zone ids
   (now filled at create) to differ.
5. **SendComponentMessage (`70f2061f4`).** An orchard planting and harvesting as before; the
   `[SendCompMsg]` lines are gone.
6. **SpriteCaptureUI (`b0fe4cda9`, `800a49d66`).** No vanilla route: nothing opens
   Dialog/SpriteCapture.rsc.
7. **GameContainer stand-ins (`267a53d32`).** Run `sa-run-menu` and check:
   - the arrow cursor, the menu hover / click sounds and the fade;
   - the options input list names;
   - New Game's three combos, Scenario, Achievements, and Load a save.
   A null slot now means Game/GlobalResources.rsc lacks that path. That is retail's behaviour, not a
   guard to add.
8. **State8Finalize (`56d43c0a7`).** Place a house: the footprint decal goes opaque when the clear
   finishes, and stays so at completion.
9. **CitizenAI (`18bc8f044`).** New game: citizens decide and work as before. A crash in Decide or
   the age-mesh setter means a citizen entity reached them without a component retail always gives it.
10. **Orchard state 9 (`5116b4b6c`).** Build an orchard and pick a tree type: the saplings are laid out
    on the grid and planters come. The `[Orchard9]` lines are gone.
11. **MoveTo, status icons (`08962c7ab`, `0f5912fba`).** The tornado loop follows the funnel. Citizen
    icons (hungry, cold, homeless) appear and clear.

## 6. Needs in other blocks' files

- **`gameupdateloop_load.cpp` ~1162-1170 (DRAW).** The new-game `Pathfinder_Seed` / `Region_Seed`
  pair after `MapTileGrid_Build` is now a no-op: the create runs both, and both return early. Its
  comment says the opposite. Row 1312.
- **`gameupdateloop.cpp` ~925-943 (DRAW).** The achievement comment and the fprintf text still name
  `GameContainer_EnsureLists`. Row 1312.
- **`starttown.cpp` ~1726 (ROWS).** It still calls `DecalComponent_MarkOwnerBuilt`. Read how retail's
  start town reaches opaque decals before dropping it. Row 1311.
- **`pathfinder_seed.cpp` / `region_seed.cpp` (no block).** The fold of row 1292.
- **`wellcomponent.cpp` `AddCellEntry` (this block's file, not in the item list).** It keeps an
  `if (!world) return` that retail 0x140088a80 lacks. Recorded as row 1310, not changed.
- **`newgamedialog.cpp` `PopulateGeneratorCombo`.** It keeps a "standalone safety" null test of the
  list. Not in the item list, not changed.

## 7. Rows

**Edited:**
- **41:** FIXED in code; the look is owed.
- **861:** FIXED. The status cell was tidied; B6 had already fixed it.
- **1291, 1293, 1294:** FIXED in code; the look is owed.
- **1292:** PARTLY FIXED. DRAW's load half and RESIDUE's create half are both noted. The fold and the
  new-game pair stay OPEN.
- **1295:** RULED, kept, with the reason narrowed to the two splash preloads.

**Added:**
- **1310:** AddCellEntry's guard.
- **1311:** The start town's decal call (ROWS).
- **1312:** DRAW's stale comments and the no-op pair.

**docs/retail_bugs.md:** entry 3 was added.
