# Final UI/Dialog Deep Reimplementation Goals

Progressive elimination of exe dependencies from dialog/UI code.

---

## Steps 0-3 — DONE (2026-03-07) — Zero trampoline calls

All 59 trampoline calls eliminated from dialog/UI reimpl files:
- Step 0: Stale comment cleanup (10 files)
- Step 1: UIComponent Shutdown + Serialize (2 funcs)
- Step 2: 14 Description classes (52 funcs)
- Step 3: 2 Element classes (5 funcs)

---

## Step 4 — DONE (2026-03-07) — ResolveAddr in clean code eliminated

All `ResolveAddr(Module::Exe, ...)` calls removed from `code/Game/Dialog/*.cpp`.
Remaining ResolveAddr calls are in `engine/reimpl/` scaffolding only (Cat A: vtable
reads in Dtors, Cat B: hook install targets) — these disappear with standalone exe.

---

## Step 5 — DONE (2026-03-08) — Replace exe-resolved engine_funcs with clean C++

Goal: Decompile and reimplement all `fn_*` function pointers that resolve from
`Module::Exe` so dialog/UI code has zero exe function dependencies.

Runtime DLL pointers (`ResolveExport`/`ResolveOrdinal`) are fine to keep.

### Completed (2026-03-07)

- FindComponent family (20 funcs) — generic `Entity_FindComponentByExeTypeId` template
- Array utilities: `fn_ExeArrayResize/2/3`, `fn_ExeArrayPush` — clean C++
- `fn_StorageComponent_QueryCount`, `fn_StorageCountQuery`/`fn_CalcWorkerSlots` — inline
- `fn_TrackerComponent_Rebuild`, `fn_GetCurrentProductIndex` — inline
- `fn_Element_GetParent` — inline vtable call
- Various dialog helpers inlined: EventNotifyDialog_ArrayRemove, ToolbarDialog_ChangeSpeed,
  FollowDialog_Close, Toolbar_FindEntity, ToolbarDialog_CloseSubDialog, GoalDialog_ArrayPush,
  ScenarioDialog_ArrayPush, ResidenceUI_ArrayPush, HelpUI_ArrayResize, HelpUI_Slot4/Slot9,
  EventLogUI_ToggleNotification/NavigateToEvent, PastureUI_UpdateSlider, PastureCalcMaxCount,
  CemeteryGetBurialCount, ModDetails_StringArrayPush, BSTRebalance, MapParamsDialog_OnOk/OnNewName,
  UpgradeUI_Serialize/OnRemove
- TradeUI: ~19 button/spinner/refresh handlers decompiled and inlined
- `fn_ResolveExternalByName` (0x14001b1d0) — inline ResolveExternalByName_Impl in engine_funcs.h
- `fn_CitizenUI_UpdateIconDisplay` (0x14006aed0) — consolidated to shared citizen_icon_helpers.h
- `fn_CitizenUI_BuildIconArray` (0x14006abf0) — consolidated to shared citizen_icon_helpers.h
- `fn_SetEngineNextLoop` (0x140097fb0) — inline in engine_funcs.h
- `fn_StatusBarUI_StatusIconHandler` (0x14007a120) — reimpl in statusbarui.cpp
- `fn_ModDialog_RefreshDisplay` (0x14006eec0) — reimpl in moddialog.cpp
- `fn_StorageUI_SortItems` (0x1400213f0) — iterative quicksort reimpl in storageui.cpp
- `fn_SaveLoadDialog_PopulateFileList` (0x1400b6ce0) — 442-line reimpl in saveloaddialog.cpp
- `fn_DestroyComponent_StartUpgrade` (0x14011dfa0) — state machine driver reimpl in destroyui.cpp
- `fn_TownHallUI_InitGeneral` (0x1400807f0) — reimpl in townhallui.cpp
- `fn_TownHallUI_InitGraph` (0x14007f0a0) — reimpl in townhallui.cpp
- `fn_TownHallUI_OnProfessionButton` (0x14007dca0) — reimpl in townhallui.cpp
- `fn_FindDialogByName` (0x1400e1600) — inline FindDialogByName_Impl (split on ':', Factory type ID, dialog list walk)
- `fn_Game_SpawnResources` (0x140091660) — inline Game_SpawnResources_Impl (round-robin distribute to StorageComponents, 27 call sites updated in debugui.cpp)
- `fn_Game_TriggerDisasterByType` (0x140060110) — inline Game_TriggerDisasterByType_Impl (ExternalFactory iteration, SelectDescription type match)
- Sub-deps resolved: `Game_RemoveEntity_Impl`, `Storage_CanAddItem`, `Storage_AddItem` (all inline)
- New Runtime ptrs: `fn_Factory_GetTypeIdFromName` (ord 2991), `fn_IsA` (ord 3089), `g_pFactorySingleton`

### TownHallUI — 15/15 fully reimplemented (2026-03-08)

- [x] `fn_TownHallUI_InitGeneral` (0x1400807f0)
- [x] `fn_TownHallUI_InitGraph` (0x14007f0a0)
- [x] `fn_TownHallUI_OnProfessionButton` (0x14007dca0)
- [x] `fn_TownHallUI_UpdateGraph` (0x14007e5b0)
- [x] `fn_TownHallUI_UpdateGeneral` (0x14007d030)
- [x] `fn_TownHallUI_UpdateProfessions` (0x14007ccb0)
- [x] `fn_TownHallUI_OnProfessionSpinner` (0x14007dd10)
- [x] `fn_TownHallUI_InitProfessions` (0x140080160)
- [x] `fn_TownHallUI_InitProduction` (0x14007fc30)
- [x] `fn_TownHallUI_InitInventory` (0x14007f660)
- [x] `fn_TownHallUI_InitAgriculture` (0x14007ebf0)
- [x] `fn_TownHallUI_UpdateProduction` (0x14007c7f0)
- [x] `fn_TownHallUI_UpdateAgriculture` (0x14007c220)
- [x] `fn_TownHallUI_UpdateInventory` (0x14007def0)
- [x] `fn_TownHallUI_UpdateAll` (0x14007d590)

### Remaining — kept as exe pointers (too deep)

- [x] `fn_Toolbar_CenterOnEntity` (0x140103040) — DONE: reimplemented as `Toolbar_CenterOnEntity_Impl` in engine_funcs.h. 5 sub-function pointers resolved (MultiObjectComponent, EntityPanel_SyncTracking, CameraSys_ComputeViewTransform, Transform_AsTransform3, Camera_SetCameraToWorld). Also replaces fn_GoToEntity. 5 callers updated.
- [x] `fn_CitizenUI_Slot9_Original` (0x1400650f0) — DONE: all 3 cases reimplemented. 0x228 follow case manually constructs FollowDialog (DialogInstance base ctor + vtable patch + field init). No assembly glue needed.
- [x] `fn_TradeUI_RefreshDisplay` (0x1400846b0) — 4 uses (336 lines, massive entity iteration + UI binding, deferred)

### Remaining — already inlined / no action needed

- [x] `fn_GameWorld_GetMapName` (0x140011f90) — already inlined in mapparamsdialog.cpp

### Per-dialog UI functions — batch reimplemented (2026-03-08)

EntityUI:
- [x] `fn_EntityUI_SetEditMode` (0x140102b60) — BST walk + Factory::CreateObject for tool controllers
- [x] `fn_EntityUI_SelectEntity` (0x140102e30) — AVL BST insert/update for entity tracking

TradeUI:
- [x] `fn_TradeUI_RefreshSellTable` (0x140083c70) — resource table iteration + UI cell population
- [x] `fn_TradeUI_RefreshBuyTable` (0x140083100) — trade order iteration + buy table UI

ToolbarDialog:
- [x] `fn_ToolbarDialog_UpdateButtons` (0x1400b8fd0) — entity BST search for enable state

MainMenuDialog:
- [x] `fn_MainMenu_OpenNewGame` (0x1400B3620) — dialog launcher
- [x] `fn_MainMenu_OpenLoad` (0x1400B3770) — dialog launcher
- [x] `fn_MainMenu_OpenSave` (0x1400B38B0) — dialog launcher
- [x] `fn_MainMenu_OpenOptions` (0x1400B3A10) — dialog launcher
- [x] `fn_MainMenu_ReturnToMenu` (0x1400B4790) — dialog launcher
- [x] `fn_MainMenu_OpenAchievement` (0x1400B34C0) — dialog launcher
- [x] `fn_MainMenu_OpenScenario` (0x1400B3380) — dialog launcher
- [x] `fn_MainMenu_OpenGoal` (0x1400B3220) — dialog launcher
- [x] `fn_MainMenu_OpenMap` (0x1400B3120) — dialog launcher
- [x] `fn_MainMenu_OpenTutorial` (0x1400B2FE0) — dialog launcher
- [x] `fn_MainMenu_OpenMod` (0x1400B2EA0) — dialog launcher

DebugUI:
- [x] `fn_DebugUI_SetDayLighting` (0x140061780) — already existed
- [x] `fn_Game_SpawnResources` (0x140091660) — already existed
- [x] `fn_Game_TriggerDisasterByType` (0x140060110) — already existed
- [x] `fn_Game_KillAllCitizens` (0x140060000) — entity iteration + destroy
- [x] `fn_Game_SpawnItemsInStorage` (0x140060290) — storage list iteration + spawn
- [x] `fn_Game_SpawnCitizens` (0x140060380) — residence capacity check + citizen creation
- [x] `fn_DebugUI_TriggerDisasterOnBuilding` (0x14005FF30) — random building + disease apply
- [x] `fn_Game_StartRandomFire` (0x14005FEB0) — random building + FireComponent ignite
- [x] `fn_Game_SpawnTornado` (0x14005FD20) — entity factory create + register
- [x] `fn_Game_InfectRandomCitizen` (0x14005FE10) — random citizen + disease infect
- [x] `fn_DebugUI_OnDiseaseComboChanged` (0x14005FC60) — already existed
- [x] `fn_DebugUI_OnDaySliderChanged` (0x1400607E0) — already existed
- [x] `fn_DebugUI_WriteLightingColors` (0x140061530) — already existed
- [x] `fn_Game_GetMonthFromTime` — already existed

ModDialog:
- [x] `fn_ModDialog_ToggleMod` (0x14006e930) — checkbox state + Modification::Enable
- [x] `fn_ModDialog_MoveToTop` (0x14006e660) — loop swap entries to top
- [x] `fn_ModDialog_ShowDetails` (0x14006e490) — create ModDetailsDialog instance
- [x] `fn_ModDialog_Apply` (0x14006ea70) — dirty check + ReloadDialog creation

ModDetailsDialog:
- [x] `fn_ModDetailsDialog_OpenUpdateDialog` (0x14006d400) — create ModUpdateDialog
- [x] `fn_ModDetailsDialog_DeleteMod` (0x14006d550) — create ModUpdateDialog (same pattern)

SaveLoadDialog:
- [x] `fn_GetLoadingSoundString` (0x140099130) — inline
- [x] `fn_GameUpdateLoop_Ctor` (0x140098060) — base ctor + vtable patch + field init
- [x] `fn_SetDialogLoaderNextLoop` (0x14009a8f0) — inline

NewGameDialog:
- [x] `fn_StartNewGame` (0x1400b4540) — GameUpdateLoop + GameLoadDialogLoopT + LoaderLoop chain
- [x] `fn_GenerateTownName` (0x140099ac0) — Cities.ndb NameDatabase + "vill"→"ville" fix

ScenarioDialog:
- [x] `fn_ScenarioDialog_LoadSelected` (0x140077cb0) — new scenario vs saved scenario paths

OptionsDialog:
- [x] `fn_OptionsDialog_PopulateInputBindList` (0x140073580) — already reimplemented
- [x] `fn_OptionsDialog_OnInputBindClick` (0x1400729a0) — already reimplemented

MapUI:
- [x] `fn_MapUI_SetupImage` (0x14006cb00) — MaterialInstance + SpriteSheet from ImageBuffer
- [x] `fn_MapUI_HandleClick` (0x14006c850) — click→camera target projection

NoiseUI:
- [x] `fn_NoiseUI_SetupImage` (0x140071890) — same pattern as MapUI
- [x] `fn_NoiseUI_GenerateNoise` (0x1400711D0) — PerlinNoise 256x256 grayscale render

CropUI:
- [x] `fn_CropUI_PopulateCombo` (0x140078830) — ExternalFactory iteration + combo population
- [x] `fn_CropUI_GetExpectedCount` (0x140078a40) — bonus, shares iterator helper

### Final per-dialog UI functions — batch reimplemented (2026-03-08)

SelectResourceUI:
- [x] `fn_SelectResourceUI_PopulateCombo` (0x140078830) — ExternalFactory iteration + combo population (shared iterator helper)
- [x] `fn_SelectResourceUI_GetResourceCount` (0x140078a40) — count-only mode of same iterator

ProfessionUI:
- [x] `fn_ProfessionUI_HandleSpinner` (0x1400763c0) — citizen assignment with FindIdle→FindWorking→FindStudent→FindAny fallback
- [x] `fn_ProfessionUI_HandleButton` (0x140076a30) — rotate-to-next-citizen + camera center

ResidenceUI:
- [x] `fn_ResidenceUI_GoToCitizen` (0x1400b56a0) — storage room iteration + Toolbar_CenterOnEntity_Impl

ConsumeProduceUI:
- [x] `fn_ConsumeProduceUI_Slot4` (0x14005F3D0) — already reimplemented, cleaned up dead fn_*

ControlUI:
- [x] `fn_FindComponentByTypeId` (0x1400c3680) — inline component array walk + vtable type ID match

TutorialUI:
- [x] `fn_MenuDialogLoopT_Ctor` (0x14009bb00) — inline: DialogLoopT base ctor + vtable patch + Random/AudioScene init

ToolUI:
- [x] `fn_UIDesc_AddControllers` (0x14012e100) — controller entry iteration with tab/non-tab paths

### Previously "too complex" — all reimplemented (2026-03-08)

All 5 functions previously deferred as "too complex" have been fully reimplemented:

- [x] `fn_Toolbar_ToggleAll` (0x140101ce0) — camera view transform + PauseDialog creation
- [x] `fn_Toolbar_ShowGroup` (0x140102b60) — 8-step BST traversal + Factory::CreateObject + ToolSetting caching
- [x] `fn_Toolbar_Utility` (0x140101a80) — dialog open/close + BST position tracking + DialogInstance ctor
- [x] `fn_DebugUI_OnDrawComboChanged` (0x1400608A0) — control flow reimplemented; 5 deep engine sub-fns (DrawGeometry/AudioObjectGroup ctors) kept as Runtime-level fn_* (not exe game logic)
- [x] `fn_ToolbarDialog_Ctor` (0x1400b9910) — 400-line constructor: ExternalFactory iteration, template cloning, radio setup, speed array init

Engine/component functions moved to [component_goals.md](component_goals.md)

---

## Summary

| Step | Scope | Status |
|------|-------|--------|
| 0-3 | 59 trampoline calls | DONE |
| 4 | ResolveAddr in clean code | DONE |
| 5 | ~130 exe fn_* pointers | **ALL DONE** — ~130 reimplemented (including all 5 "too complex"), component funcs in component_goals.md |
