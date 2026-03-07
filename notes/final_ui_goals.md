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

### Remaining — needs opus agent (complex decompilation)

- [ ] **TownHallUI** (15 funcs) — Init/Update for 6 tabs + graph + profession handlers
  (50-150+ lines each, many unresolved sub-calls)
- [ ] **fn_StorageUI_SortItems** (0x1400213f0) — 100+ line introsort on 0x18-byte structs
- [ ] **fn_SaveLoadDialog_PopulateFileList** (0x1400b6ce0) — 442 lines, complex file list builder
- [ ] **fn_DestroyComponent_StartUpgrade** (0x14011dfa0) — 120+ lines, state machine driver

### Remaining — standard difficulty

**Shared/high-use:**
- [ ] `fn_ResolveExternalByName` (0x14001b1d0) — 8 uses
- [ ] `fn_CitizenUI_UpdateIconDisplay` (0x14006aed0) — 5 uses
- [ ] `fn_CitizenUI_BuildIconArray` (0x14006abf0) — 4 uses
- [ ] `fn_Toolbar_CenterOnEntity` (0x140103040) — 4 uses
- [ ] `fn_FindDialogByName` (0x1400e1600) — 4 uses
- [ ] `fn_GameWorld_GetMapName` (0x140011f90) — 4 uses
- [ ] `fn_TradeUI_RefreshDisplay` (0x1400846b0) — 4 uses
- [ ] `fn_StatusBarUI_StatusIconHandler` (0x14007a120) — 3 uses
- [ ] `fn_ModDialog_RefreshDisplay` (0x14006eec0) — 3 uses
- [ ] `fn_SetEngineNextLoop` (0x140097fb0) — 3 uses
- [ ] `fn_Game_SpawnResources` (0x140091660) — 21 uses (DebugUI only)
- [ ] `fn_Game_TriggerDisasterByType` (0x140060110) — 3 uses (DebugUI only)

**Per-dialog (1-2 uses each):**

CitizenUI: `fn_CitizenUI_Slot9_Original`, `fn_WorldGrid_FindEntity`
EntityUI: `fn_EntityUI_SetEditMode`, `fn_EntityUI_SelectEntity`, `fn_EntityTrackingSet_Remove`,
  `fn_GetDescPage`, `fn_BuildComplete`, `fn_EditTool_SelectEntity`
DestroyUI: `fn_DestroyComponent_CanReclaim/CanRebuild/Reclaim/Rebuild/Destroy`
BuildUI: `fn_WorkplaceComponent_Toggle`, `fn_CountOccupiedBuildTiles`
WorkplaceUI: `fn_WorkplaceUI_AdjustWorkerCount/ToggleWorkplace`,
  `fn_CitizenList_RequestWorker/AssignWorker/RemoveWorker`
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
RadiusUI: `fn_RadiusCalc`, `fn_RadiusDisplay_Remove`
OrchardUI: `fn_OrchardGetGrowth`
TutorialUI: `fn_MenuDialogLoopT_Ctor`
ToolUI: `fn_UIDesc_AddControllers`
FollowDialog: `fn_FindComponent_Character` (if not covered by FindComponent template)

---

## Summary

| Step | Scope | Status |
|------|-------|--------|
| 0-3 | 59 trampoline calls | DONE |
| 4 | ResolveAddr in clean code | DONE |
| 5 | ~120 exe fn_* pointers | ~45 done, ~75 remaining |
