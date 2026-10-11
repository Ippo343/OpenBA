# SLOTS2B hand-in (PORT GAP hunt, branch `d4-slots2b`, base `b9006081e`)

Component slots, dialog teardowns, the inner loader's container init and the menu loop's Update
(GAPHUNT_RULES.md section 2, the SLOTS2B row). Main (`ca6cc314c`) was merged before the final
build, with no conflicts.

**Static gate only. The game was never run from this worktree.** Every route in section 4 is
the coordinator's.

## 1. Gate (merged tree, clean rebuild of both flavours)

| check | result |
|---|---|
| `standalone-perf -j2`, `standalone-debug -j2`, `unit -j2` | EXIT 0, EXIT 0, EXIT 0 |
| unit | 1002 / 0 |
| warnings (gate.sh `norm()`, `comm -23` against the main checkout's `runs/warn_baseline_{debug,perf}.txt`) | 0 new (debug), 0 new (perf) |
| castmetric (`src`) | 2 |
| `tools/abi_audit.py` | OK |
| `b3r_session/acceptance.sh` | 0 lines off target (R14-R16 skipped, they need a step-close log) |

The worktree's SDL submodule was initialised from the main checkout's local clone (no network).

## 2. Commits and B11 verdicts

B11 is `b11pair.py` against `runs/b11base_d4e/perf/obj`.

| commit | title (short) | B11 |
|---|---|---|
| `dcd0e514a` | behaviour: HighlightComponent Tick / OnEvent = 0x14004c850 / 0x14004c870 | highlightcomponent.o: IDENTICAL 16 RENAMED-SAME 1 SAME 2. b11pair counts no call in a bare tail jump. objdump shows Tick = test / js / jmp EdgeMeshInstance::Update and OnEvent = test / je / jmp Repaint, where both were `ret`. |
| `3fda8d0a3` | behaviour: DecalComponent::OnEvent = 0x14011b760 | decalcomponent.o: DIFF 1 (OnEvent) IDENTICAL 21 |
| `5df06143a` | behaviour: DecalComponent::Slot11 = 0x14011b810 (row 1271) | Slot11 = `jmp OnAdded` (objdump) |
| `7d49266b6` | behaviour: Grid_AllEmpty = 0x14009dac0 (row 1272) | clearareacomponent_states.o: DIFF 1 (IsDone, the owner compare gone) IDENTICAL 20 |
| `626c45f38` | behaviour: ~StorageDisplayComponent = 0x1400569b0 (Array<Pile>) | storagedisplaycomponent.o: DIFF 3 (D0 / D1 teardown, OnAdded -> SetSizeCore) ONLY-BASE SetPileCount ONLY-TIP TInit / TAssign / TDestroy<Pile> IDENTICAL 18 |
| `e20fa1741` | behaviour: StorageLocationComponent::OnRemove = 0x14012d270 | storagelocationcomponent.o: DIFF 1 (OnRemove -> ArrayRemove8) IDENTICAL 29 |
| `164464716` | port: ~AmbientEmitterComponent 0x1400016d0 ALREADY THERE | no code change |
| `f6d6fe5db` | behaviour: AmbientEmitter PlaySlot / StartEmitters / SetSlot (row 1246) | ambientemittercomponent.o: DIFF 5 ONLY-BASE GetBuildingData IDENTICAL 16 |
| `6f3f99995` | behaviour: ~MapUI = 0x14006ce90 | mapui.o: DIFF 2 (D1 four slot-1 deletes, D0 -> ~MapUI) IDENTICAL 13 |
| `618070bd2` | behaviour: ~CropUI = 0x1400b2c10 | cropui_promote.o: DIFF 4 (D0 / D1 two MemFree, the ctors' zero stores as qwords) IDENTICAL 16 |
| `f37e797c4` | port: ~BuildRequirementUI 0x140010860 ALREADY THERE (row 441) | buildrequirementui.o: IDENTICAL 16 |
| `b6cc0d6a7` | behaviour: InnerLoaderLoop::Load = 0x140001120 (GameContainer_Init 0x140099db0) | loader_loop_stubs.o: DIFF 1 (Load) ONLY-TIP 5 (GameContainer_Init, four FindInList<T>) IDENTICAL 12 |
| `1d285bacc` | behaviour: MenuDialogLoopT::Update = 0x14009b6e0's block (row 1242) | menudialogloopt_stubs.o: DIFF 1 (Update). menudialogloopt.o: DIFF 1 (UpdateCamera_PhaseA loses the 0xb90 test) |
| `75c40e522` | docs: ~MenuDialogLoopT's `_loadBit = -1` ruled and kept (row 1295) | no code change |
| `2f8cba641` | port: NoiseUI [4] / [9] ALREADY THERE; the stale stub-factory notes go | no code change |
| `7e1c6be3a` | docs: rows 41, 1242, 1246, 1271, 1272 status cells | docs |
| `93cea1ed7` | port: Update's trace literal one line again (debug build) | `1d285bacc`'s heredoc split a printf literal. Perf strips printf, so only debug failed. |
| `e785804eb` | Merge main | none |
| (this commit) | docs: this hand-in | docs |

## 3. Items

| item | verdict | reason |
|---|---|---|
| HighlightComponent Tick 0x14004c850 | PORTED | `if (+0x120 >= 0) edgeMeshInstance.Update(*time)` (slot 1, RDX passed through). Repaint ORs flag bit 2 when it adds the outline, so the tick loop reaches Tick only while the outline is live. |
| HighlightComponent OnEvent 0x14004c870 | PORTED | `if (eventId == 0) Repaint()`. 0x14004c4a0 reads RCX only. |
| DecalComponent OnEvent 0x14011b760 | PORTED | Event 1: if scale != 1.0 and a decal is placed, remove, set 1.0, re-add. Event 5: if placed, remove and re-add. Every 0x1400c2b70 xref was read. Event 1 comes only from ClearArea State2Enter 0x1401177f0 and event 5 only from FinishClear 0x140118270. BuildComponent's completion sends 4. |
| DecalComponent Slot11 0x14011b810 + row 1271 | PORTED | `JMP [vtbl+0x30]`, i.e. OnAdded. EntityList::PostLoad calls slot 11. |
| Row 1272 (Grid_AllEmpty self skip) | PORTED | 0x14009dac0 tests only `occ != 0`. CountRemaining 0x140118330 was read too: it is the one that skips the owner, and the tree already does. |
| StorageDisplayComponent teardown 0x1400569b0 | PORTED | 0x14000c9a0 is ~Array<Pile> and 0x14000ca10 is Array<Pile>::SetSize. `piles` is now a typed `Collection::Array<Pile>`, and Pile carries the element operations. The dtor is member destruction, with no hand-written free loop. |
| StorageLocationComponent removal 0x14012d270 | PORTED | `m_citizenList->field_0x00.Remove(_entity)`, nothing tested |
| AmbientEmitterComponent dtor 0x1400016d0 | ALREADY THERE | The typed `slots` member's ~Array, confirmed by objdump. The stale OnRemove PORT GAP tag also went. |
| Row 1246 (PlaySlot / StartEmitters / SetSlot) | PORTED | Every port-only test is gone, and so are those in OnAdded and Persist, which use the same helper. StartEmitters = loop over PlaySlot, which is retail's inline. |
| MapUI teardown 0x14006ce90 | PORTED | Four `delete`s in retail's order (+0x38, +0x30, +0x48, +0x40), the NoiseUI dtor's shape |
| BuildRequirementUI teardown + row 441 | ALREADY THERE | The typed `labels` / `counts` members. Row 441 FIXED. |
| CropUI teardown 0x1400b2c10 | PORTED | Two `Collection::Array<uint8_t>` members (+0x40, +0x50). No CropUI body reads them, so the element type is unrecoverable, but retail frees with no element destroy. |
| `loader_loop_stubs.cpp:148` 0x140001120 | PORTED | `GameContainer_Init(g_gameContainer, _resourceList)` (0x140099db0, all 561 instructions), then `_loadBit = -1` |
| NoiseUI [4] 0x1400714c0 / [9] 0x1400714a0 | ALREADY THERE | noiseui.cpp's overrides match retail argument for argument. The PORT GAP tags were stale. |
| SpriteCaptureUI [9] 0x140079620 + row 41 | LEFT | Needs `UI::UserInterface::BeginSpriteCapture` 0x18011f4c0. The engine declares it but no file defines it (src/engine/ui/userinterface.cpp, outside SLOTS2B). See section 5. |
| `menudialogloopt_stubs.cpp:278` (PlacementTool note) | RULED | Row 1295, below |
| Row 1242 (menu Update slot) | PORTED | `MenuDialogLoopT_TickAudio` is kept as a documented standalone difference (section 5) |

Every `PORT GAP` line in the SLOTS2B files is closed except the SpriteCaptureUI [9] pair:
`SpriteCaptureUI.h:39` and `ui_dialog_stub_factories.cpp:383`.

## 4. What to look at on the merged exe (one route each)

1. **Highlight Tick.** Select a walking citizen (or a cart, or a deer). The outline should move with
   the model each frame. Before, it stayed where it was drawn.
2. **Highlight OnEvent(0).** Select a building under construction while its builders advance the
   mesh frames. The outline should be rebuilt on the new frame.
3. **Decal OnEvent.** Place a house. The footprint is translucent while the site is cleared, then
   turns fully opaque the moment the clear finishes (ClearArea state 2), not at completion. A
   regrow-terrain clear rebuilds the decal on the new heights.
4. **Decal Slot11.** Load any save (the resave gate's Gate.sav). Every building's dirt / floor
   decal is painted. Before, a loaded town had none.
5. **Row 1272.** Upgrade a house, or demolish one and rebuild it in place. The site clears and
   building starts. If it parks at 0%, something stamps the foundation into its own cells early.
6. **StorageDisplay dtor.** Demolish a full stockpile, then quit. There should be no crash at
   teardown, and the pile meshes are released.
7. **StorageLocation OnRemove.** Mark a stockpile for demolition while haulers are carrying to
   it. They must stop choosing it at once.
8. **Row 1246.** Listen to the biome ambience, the crowd at an occupied house, fire, and the
   tornado. All should be as before. A crash in PlaySlot / SetSlot means a component reached them
   without its description or scene.
9. **MapUI / CropUI dtors.** Open and close the map dialog ten times, then select a crop field and
   close its panel. There should be no crash. MapUI's leak is gone.
10. **GameContainer_Init (boot).** Run `sa-run-menu` and a new game, then check these:
    - the arrow cursor, the menu hover and click sounds, the fade sprite, and the options input
      names list;
    - the Achievements, Scenario and New Game dialogs open.

    Any slot whose path is missing from `Game/GlobalResources.rsc` is now null at boot, exactly as
    in retail. SetAchievements and the two Sprite ctors read their inputs unguarded.
11. **Menu Update.** On the main menu, alt-tab away and back. The menu music pauses and resumes.
    Watch that the menu's UI sounds keep playing after many clicks.

**Save-format consequences.**
- Only DecalComponent's `decalScale` can change in a stream. A site saved after its clear but
  before completion now saves 1.0 where it saved `_initialAlpha`.
- Resaving a loaded file fires no event, so the resave golden should not move. A diff there is a
  finding.
- The StorageLocation removal changes the town manager's first array only after a demolition
  mark.

## 5. Needs in other blocks' files (the coordinator routes them)

- **`src/engine/ui/userinterface.cpp` (no block; ROWS).** Define `UI::UserInterface::BeginSpriteCapture`
  0x18011f4c0. It is five stores, read via the Runtime program:
  `_spriteCapture = capture; _captureViewport = *viewport;` (+0x110, then the four dwords
  +0x118..+0x124).

  SpriteCaptureUI [9] can then become `if (a == 0x12 && b == 1000) StartCapture();`. StartCapture
  is 0x1400792b0, read in full, in this order:
  1. If `anchorX < curX`, swap them; if `anchorY < curY`, swap them.
  2. `p = MemAlloc(0x58, 8)`, then `::new (p) UI::SpriteCapture()` (the exe table 0x14015d550 is
     the class's own copy).
  3. `pendingExternal = p; saveFrameCount = 0`.
  4. `Video::Viewport v{curX, curY, anchorX - curX + 1, anchorY - curY + 1}`.
  5. `_dialog->_ui->BeginSpriteCapture(p, &v)`. Ghidra drops the R8 viewport here.
  6. `captureBox->SetFlagsOnOff(1, 0)`.

  Check that `sizeof(UI::SpriteCapture) == 0x58`.
- **`src/game/Game/game_main.cpp` (no block).** These boot-time stand-ins for 0x140099db0 can go
  once route 10 is clean:
  - `RegisterDefaultInputBindings`
  - `LoadInputNameTables`
  - `StandaloneEnsureSpriteSheetLoaded`

  With them gone, row 1295's `_loadBit = -1` in ~MenuDialogLoopT can go too.
- **`src/game/Dialog/dialog_exe_helpers.cpp` (no block).** `GameContainer_EnsureLists` is now
  redundant. It finds the slots filled. Its once-only SetAchievements runs a second, idempotent
  time.
- **This block's own `menudialogloopt_stubs.cpp`.** `EnsureCursorsLoaded` / `EnsureUISoundsLoaded`
  and Startup's `StandaloneEnsureSpriteSheetLoaded` call return immediately once the container is
  filled. They are left in until route 10 confirms the inner loader fills every slot.
- **`buildcomponent_state8.cpp` / `starttown.cpp` (no block).** `DecalComponent_MarkOwnerBuilt`
  is the tree's, not retail's. It now runs OnEvent(1), which is a no-op whenever the site passed
  ClearArea state 2.
  - Drop the State8Finalize call once route 3 is clean.
  - The start-town call needs a read of how retail's start town gets opaque decals before it goes.
- **Audio owner.** Retail's MainLoop::Frame and in-game loop never call `Audio::Device::Update`.
  The tree's SDL voice path marks a finished channel with bit 2 only, and only Device::Update frees
  it. So where retail reaps channels is open.
  - The menu's port-only device tick is therefore kept, as row 1242 notes.
  - The in-game loop has no such tick at all: check whether in-game one-shots starve after 64.
- **`tests/unit/vis_abi_test.cpp` (touched, outside the row).** The +0x30 check for
  StorageDisplayComponent now reads `piles._size`, the same shape as the StorageComponent line
  above it. The build needed it.

## 6. Rows added or edited

- **Added 1295:** ~MenuDialogLoopT's `_loadBit = -1`, RULED and kept, with its exit condition.
- **Edited:**
  - 1242, 1246, 1271, 1272: PORTED at the static gate, FIXED after the look.
  - 441: FIXED, already there.
  - 41: LEFT, with the blocking body named.
