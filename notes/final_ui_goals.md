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

## Step 5 — IN PROGRESS — Replace exe-resolved engine_funcs with clean C++

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

### TownHallUI — 7/15 fully reimplemented, 8 thin wrappers

Fully reimplemented (fn_* removed):
- [x] `fn_TownHallUI_InitGeneral` (0x1400807f0)
- [x] `fn_TownHallUI_InitGraph` (0x14007f0a0)
- [x] `fn_TownHallUI_OnProfessionButton` (0x14007dca0)
- [x] `fn_TownHallUI_UpdateGraph` (0x14007e5b0)
- [x] `fn_TownHallUI_UpdateGeneral` (0x14007d030)
- [x] `fn_TownHallUI_UpdateProfessions` (0x14007ccb0)
- [x] `fn_TownHallUI_OnProfessionSpinner` (0x14007dd10)

Thin wrappers (fn_* kept — too complex, deep BST/component traversal):
- [ ] `fn_TownHallUI_InitProfessions` (0x140080160)
- [ ] `fn_TownHallUI_InitProduction` (0x14007fc30)
- [ ] `fn_TownHallUI_InitInventory` (0x14007f660)
- [ ] `fn_TownHallUI_InitAgriculture` (0x14007ebf0)
- [ ] `fn_TownHallUI_UpdateProduction` (0x14007c7f0)
- [ ] `fn_TownHallUI_UpdateAgriculture` (0x14007c220)
- [ ] `fn_TownHallUI_UpdateInventory` (0x14007def0)
- [ ] `fn_TownHallUI_UpdateAll` (0x14007d590)

### Remaining — kept as exe pointers (too deep)

- [x] `fn_Toolbar_CenterOnEntity` (0x140103040) — DONE: reimplemented as `Toolbar_CenterOnEntity_Impl` in engine_funcs.h. 5 sub-function pointers resolved (MultiObjectComponent, EntityPanel_SyncTracking, CameraSys_ComputeViewTransform, Transform_AsTransform3, Camera_SetCameraToWorld). Also replaces fn_GoToEntity. 5 callers updated.
- [x] `fn_CitizenUI_Slot9_Original` (0x1400650f0) — DONE: all 3 cases reimplemented. 0x228 follow case manually constructs FollowDialog (DialogInstance base ctor + vtable patch + field init). No assembly glue needed.
- [ ] `fn_TradeUI_RefreshDisplay` (0x1400846b0) — 4 uses (336 lines, massive entity iteration + UI binding, deferred)

### Remaining — already inlined / no action needed

- [x] `fn_GameWorld_GetMapName` (0x140011f90) — already inlined in mapparamsdialog.cpp

### Remaining — per-dialog UI functions (1-2 uses each)

EntityUI: `fn_EntityUI_SetEditMode`, `fn_EntityUI_SelectEntity`
WorkplaceUI: `fn_WorkplaceUI_AdjustWorkerCount/ToggleWorkplace`
TradeUI: `fn_TradeUI_RefreshSellTable/RefreshBuyTable/OnOrderSpinner/Update`
ToolbarDialog: `fn_ToolbarDialog_Ctor/UpdateButtons`, `fn_Toolbar_ShowGroup/Utility/ToggleAll`
MainMenuDialog: 11x `fn_MainMenu_Open*` + `fn_MainMenu_ReturnToMenu` (likely one-liners)
DebugUI: 11x `fn_Game_*/fn_DebugUI_*` (debug-only functions)
ModDialog: `fn_ModDialog_ToggleMod/MoveToTop/ShowDetails/Apply`
ModDetailsDialog: `fn_ModDetailsDialog_OpenUpdateDialog/DeleteMod`
SaveLoadDialog: `fn_GetLoadingSoundString`, `fn_GameUpdateLoop_Ctor`,
  `fn_SetDialogLoaderNextLoop`
NewGameDialog: `fn_StartNewGame`, `fn_GenerateTownName`
ScenarioDialog: `fn_ScenarioDialog_LoadSelected`
OptionsDialog: `fn_OptionsDialog_PopulateInputBindList/OnInputBindClick`
MapUI: `fn_MapUI_SetupImage/HandleClick`
NoiseUI: `fn_NoiseUI_SetupImage/GenerateNoise`
CropUI: `fn_CropUI_PopulateCombo/GetExpectedCount`
SelectResourceUI: `fn_SelectResourceUI_PopulateCombo/GetResourceCount`
ProfessionUI: `fn_ProfessionUI_HandleSpinner/HandleButton`
ResidenceUI: `fn_ResidenceUI_GoToCitizen`
ConsumeProduceUI: `fn_ConsumeProduceUI_Slot4`
ControlUI: `fn_FindComponentByTypeId`
TutorialUI: `fn_MenuDialogLoopT_Ctor`
ToolUI: `fn_UIDesc_AddControllers`

Engine/component functions moved to [component_goals.md](component_goals.md)

---

## Summary

| Step | Scope | Status |
|------|-------|--------|
| 0-3 | 59 trampoline calls | DONE |
| 4 | ResolveAddr in clean code | DONE |
| 5 | ~120 exe fn_* pointers | ~67 done, 8 TownHallUI thin wrappers, 3 kept as ptrs, ~35 per-dialog UI + ~16 in component_goals.md |
