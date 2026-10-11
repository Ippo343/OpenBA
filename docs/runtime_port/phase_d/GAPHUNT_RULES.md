# The PORT GAP hunt (NEXT_SESSION.md section 2.5) -- rule sheet (written 2026-10-11)

Goal: close every `PORT GAP` line in `src/` (128 at the start, 67 sites over ~40 retail bodies / tables),
the .rdata LUTs, and the 21 `OPEN` rows of `docs/known_port_issues.md` plus the section 2.4 SUSPECTED rows.
Base commit for every block: **`1e842b306`** (main at the start). Kit: `phase_d/` (D4_RULES.md sections 4 / 5
apply verbatim). B11 base: `docs/runtime_port/phase_d/runs/b11base_d4e/perf/obj` (main `1e842b306`'s -O2
objects, 962, cut at the start; `runs/` is gitignored -- read it from the MAIN checkout's absolute path
`C:/code/GIT/untitled_rev_eng_project/docs/runtime_port/phase_d/runs/b11base_d4e/perf/obj`).
Invariants at the start: new game 5694 / 0, tutorial 5773 / 12, typetable `965b527a`, resave 3/3 PASS golden
`9c18dd25f2a8` (23,453,453 bytes), unit 1002 / 0, 0 new warnings, castmetric 2, abi_audit OK.

## 1. What a closed PORT GAP is

A `PORT GAP` comment names a retail body or table that the tree does not run. Closing it means ONE of:

- **PORTED** -- the retail body is read in full in Ghidra (decompile AND disassembly, every slot / arm), written
  as clean C++ in the class's file, declared in its header, called by name where retail calls it. The comment
  is deleted. Commit title `behaviour:` + what becomes visible / audible / saved.
- **ALREADY THERE** -- the body is a template instance or an inline the tree already spells (the 2026-10-11
  `Entity_FindAudioComponent` = `FindComponent<AmbientEmitterComponent>` case). Replace the comment with the
  call; `port:` commit, the proof (addresses, the shape) in the message.
- **RULED** -- retail's body is unreachable in the standalone or is a CRT / platform detail the port has by
  other means (e.g. the CRT `exit(code)`). The comment becomes a one-line `// retail 0x... : <what> -- ruled:
  <why>` (no `PORT GAP` spelling), with a known_port_issues row if any behaviour differs. Rare; argue it.

Never "close" a gap by deleting the comment and keeping the stub. Never add a null guard a faithful body
lacks (CLAUDE.md). A body that now WRITES state the serializer sees moves the resave golden: say so in the
hand-in (the coordinator re-cuts the golden only with the payload / table analysis the gate prints).

## 2. Blocks (wave 1: four building agents at `-j2`; wave 2 after the merges)

| block | branch | owns (files) | items |
|---|---|---|---|
| SLOTS2A | `d4-slots2a` | `src/game/Component/{Animal,AutoPickup,Character,Growth,Picking,UI,StateMachine,StatusIcon,Work}Component*.{h,cpp}` and their impl files (`statusiconcomponent.cpp`, `workcomponent.cpp`, `growthcomponent*.cpp`, ...) | AnimalComponent Slot11 0x140106dd0; AutoPickupComponent [13] 0x140044970 / [14] 0x140044930 and the ctor's `isEnabled = 1`; CharacterComponent OnAdded 0x1401099c0; GrowthComponent OnRemove 0x140120560 / Slot11 0x1401206a0; PickingComponent Startup 0x140128c30 / Slot11 0x140128ba0; UIComponent Startup 0x14012dca0; StateMachineComponent Slot11 0x14012b7d0 (and the root 0x140038630-0x140038800 the .h:78 banner names); StatusIconComponent Slot11 0x14012bf40 (ShowAfterLoad; .h:99) and row 1263 (SetFlag 0x14012c3f0's guard); WorkComponent OnAdded 0x14012e6f0 (the .h:16 banner still says PORT GAP -- verify the body is installed, fix the banner or the body) |
| SLOTS2B | `d4-slots2b` | `src/game/Component/{Highlight,Decal,StorageDisplay,StorageLocation,AmbientEmitter}Component*.{h,cpp}` + impl files, `clearareacomponent_states.cpp`, `src/game/Dialog/{MapUI,BuildRequirementUI,CropUI,NoiseUI,SpriteCaptureUI}*`, `ui_dialog_stub_factories.cpp`, `src/game/Game/loader_loop_stubs.cpp`, `src/game/Game/menudialogloopt*.cpp` | HighlightComponent Tick 0x14004c850 / OnEvent 0x14004c870; DecalComponent Slot11 0x14011b810 / OnEvent 0x14011b760 + rows 1271, 1272; StorageDisplayComponent removal 0x1400569b0; StorageLocationComponent removal hook 0x14012d270; AmbientEmitterComponent's dtor free 0x1400016d0 + row 1246 (PlaySlot's five tests vs 0x140042e10 / 0x140042a10); the dialog teardown helpers MapUI 0x14006ce90, BuildRequirementUI (row 441), CropUI 0x1400b2c10; `loader_loop_stubs.cpp:148` (0x140001120); NoiseUI [4] 0x1400714c0 / [9] 0x1400714a0; SpriteCaptureUI [9] 0x140079620 (row 41); `menudialogloopt_stubs.cpp:278` (the PlacementTool note) + row 1242 (the menu loop's Update slot vs retail's block) |
| CAI | `d4-cai` | `src/game/Component/CitizenAIComponent*`, `citizenai_actionctors.cpp`, `citizenai_handlers.cpp`, `HappinessComponent*`, `src/game/Work/*.cpp` (all), `src/game/Action/action_pf_roots_drop.cpp`, `src/game/Player/dragtool.cpp` | CitizenAIComponent::OnMessage [15] 0x14010f340 and OnAdded [6] 0x14010f880; HappinessComponent [15] 0x14004a780 (OnMessage) + its arm `citizenai_actionctors.cpp:738`; the 21 `CitizenAIComponent::OnMessage` arms (Work/*, action_pf_roots_drop, citizenai_actionctors:153, citizenai_handlers:88) -- read what each arm sends and what retail's OnMessage does with it, then the arms call the body; the `InternalBasePtr::DeletePtr` import slot 0x140151ab8 (6 arms in Work/work_act_*.cpp); `Volume::Box::Box` 0x1401513f8 (dragtool.cpp:205); rows 1262 (WorkBST serialize `citTid &&`), 1181 (PickupRawMaterial ctor), 290 (SortByPriority stability: read 0x14002fe10 and 0x14010dca0; port the introsort if retail's order is observable) |
| LUT | `d4-lut` | `src/game/Action/action_pf_geom.cpp`, `action_pf_road.cpp`, `action_base_cnr.cpp`, `src/game/Component/quarrycomponent_deform.cpp`, `pasturecomponent*.cpp`, `src/game/Game/gamegrid_serialize.cpp`, `worldgen.cpp`, `src/game/Player/{edittool,roadtool_calcpath,bridgetool_validate,tool_*}.cpp`, `src/game/Dialog/debugui.cpp`, `eventnotifydialog.cpp`, `src/engine/system/error.cpp`, `src/engine/io/filesystem_createfile.cpp` | the tile-shape variant LUT 0x1401c22d0 (256 x int8; lift the bytes from .rdata into a `constexpr` table, named); the tiled-decal mask-atlas rect LUT 0x1401c23d0; the slope cutoff 0x140172130 and corner offset 0x140172274 (quarrycomponent_deform.cpp:77 / :81; `worldgen.cpp:332` reads 0x140172130 as 0.85f -- reconcile); `FUN_140022480` (action_base_cnr.cpp:378); the RegionSystem build 0x14009e8c0 (gamegrid_serialize.cpp:307); `Entity_impl_cc90` 0x1400fcc90 (edittool.cpp:368, "not registered on purpose" -- read why, port or rule); `debugui.cpp:242 / :271` (the audio group arm; the finder IS `FindComponent<AmbientEmitterComponent>`, handins/AUDIOCOMP.md); `Dialog::GetReference` 0x1800388d0 (eventnotifydialog.cpp:69); the CRT `exit(code)` 0x18007f8a8 (error.cpp:205); the .rdata prefix 0x1801557e8 (filesystem_createfile.cpp:309); rows 1240 (PlaceRoadCell's three tests), 1241 (StampCellRefs' two callers), 1180 (BridgeTool's four tests), 1261 (the second RefreshMesh after RegisterEntity) |
| DRAW (wave 2) | `d4-draw` | `src/game/Game/gameupdateloop*.cpp`, `src/game/Game/weather*.cpp`, `src/game/Dialog/entityui*.cpp` | `Weather::DrawUpdate` 0x1400938d0 (gameupdateloop.cpp:1375); `EntityUI::Startup` 0x140101ee0 / `Update` 0x1401026a0 (gameupdateloop.cpp:1859 / :1058); rows 754 (WeatherSimulation store order), 1164 (`s_tidCompEnt &&`), 1247 (SerializeAll's tests + the phantom third parameter) |
| ROWS (wave 2) | `d4-rows` | everything the rows name that no wave-1 block owns | 1243 (GameLoadDialogLoopT::Startup's ApplyKeyMapToUI + three UI calls, splash_loop_stubs.cpp), 1260 (fold TownRegistry_RegisterResidence into ResidenceComponent::EnterBuilt 0x140129de0), 1220 + 880 (audio_scene_update.cpp), 1244 (RestoreVideoSettings), 1245 (SeedAgriPool / ResolveTemplate), 1163 (water_contour_mesh RangeResize), 792 (AuditDyingEntity), 796 (goals_serialize EraseCountNode), 820 (ComponentEntity::Persist `_entityFlags`), 810 (SerializeLegacyKeyIntTree), 850 (citizenai_growup position guard), 860 (ModelDescription +0x00 type check), 861 (GatherRawMaterials Time), 1140-1142 (CountProducedMaterial `source &&`, AddWorker / RemoveWorker tests), 1162 (RULED keep 0.0f: fix the row's status cell); the recorded platform rows 1277 / 1278 / 1280 / 1281 / 1283 / 1284 (each: either a fix or the status cell reworded `RECORDED (not a defect)` with the reason, so no `OPEN` is left that nobody owns); 1282 (Linux: read the SDL audio stream close at exit in `audio_sdl.cpp`; the WSL2 re-test is the coordinator's) |

Row 890 (device-lost) stays DEFERRED: nobody touches it or `OnVideoError / OnVideoRestart`.
A block that finds a site it needs in another block's file STOPS at that file and writes the need into its
hand-in (the coordinator routes it); it never edits across the table. Shared headers (`ComponentEntity.h`,
`exe_bodies.h`, `known_port_issues.md`) may be APPENDED by any block; `mergegate.sh` keeps both sides of
`known_port_issues.md`, every other conflict is the coordinator's by hand.

## 3. Per-block recipe (D4_RULES.md section 4, restated)

1. `git log -1` in the worktree must print `1e842b306`. Read CLAUDE.md, this file, D4_RULES.md sections 4 / 5,
   `.claude/rules/type-headers.md`, `handins/AUDIOCOMP.md` and `handins/GAPS.md` (the shape of a good hand-in).
2. Per item: Ghidra first (`decompile_function` AND `disassemble_function` of the body and of every caller
   retail has; `get_function_xrefs`). Note what retail's callers pass and what the body reads / writes, slot by
   slot. Ghidra drops arguments on indirect calls and register-passthrough forwarders and R8 on struct-returning
   statics (MEMORY): trust the disassembly. The tree's existing comments at the site are the previous reader's
   notes, not the proof.
3. Write the body in the class's file; declare it in the header (the override already exists for a slot).
   Static layout: every struct offset the body touches must already be a named member with its `static_assert`;
   add the member if it is missing (type-headers.md).
4. One commit per retail body (`behaviour:`), or per rule / proof (`port:` / `docs:`). Body of the message:
   the retail address(es) read, what changes at the game, the files, the B11 verdict per touched TU
   (`python docs/runtime_port/phase_d/b11pair.py <BASE> <TIP> <MAP-or-/dev/null> <tu>` with BASE the path in
   the header of this file, TIP your worktree's `build/perf/obj`), what was RUN ("static gate only"), what the
   item list said that you did NOT do and why. End with the two attribution lines CLAUDE.md's session gives.
5. Build in the FOREGROUND, `-j2`, 600000 ms timeout, re-invoke if cut, read the EXIT CODE (never a pipe's):
   `mingw32-make standalone-debug -j2`, `mingw32-make standalone-perf -j2`, `mingw32-make unit -j2`. 0 new
   warnings against the main checkout's `runs/warn_baseline_{debug,perf}.txt` (gate.sh's `norm()`: grep
   `warning:`, strip `line:col`, `sort -u`, `comm -23`). Unit 1002 / 0 plus yours. `castmetric.py` 2.
   `python tools/abi_audit.py` OK.
6. Hand in `docs/runtime_port/phase_d/handins/<BLOCK>.md` (never a root HANDIN.md): per commit the verdicts;
   per item PORTED / ALREADY THERE / RULED / LEFT with the reason; the save-format consequences; what the
   coordinator must look at or listen for on the merged exe (a concrete route each: which building, which
   action, which sound, which dialog); needs in other blocks' files; rows added or edited.
7. Merge main before the final build (`git merge main`, keep both sides of appended files, check no conflict
   marker split a brace). Never `git stash`, amend, rebase, push; no sub-agents; never `taskkill /IM`; never
   run the game (the coordinator alone runs it); never `getenv`; no null guard on a faithful deref; no new
   `BANISHED_*` flag, instrument or log line beyond what retail prints.

## 4. The gate (coordinator only)

`phase_d/mergegate.sh <branch> <label> "<title>" ui tools` after the GATE_TABLE row is written; then the
routes the block's hand-in names (disaster route for fire / tornado / Highlight, `harness_route.sh` for the
dialogs, the tutorial for StatusIcon / Happiness, `sa-run-menu` for the splash / menu loop), the resave gate,
a listen / look with the user at the game the same hour. Rows FIXED only after the look.
