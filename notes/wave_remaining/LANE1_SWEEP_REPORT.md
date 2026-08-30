# Lane 1 — Null Function-Pointer Sweep Report

62 exe addresses that our own source resolves via `ResolveAddr(Module::Exe, …)` but
nobody registers in `standalone/address_registry.cpp`. Method: grep every call site of
each address / each `fn_*` variable it is assigned to across the WHOLE main tree
(excluding `.claude/worktrees/*`), verify guard status, and check whether a clean C++
port already exists elsewhere (in which case the raw resolve is vestigial).

Legend: **DEAD** = never called (or superseded by an existing direct port) · **LIVE-GUARDED**
= called behind `if (fn)` · **LIVE-UNGUARDED** = called with no null check (latent
EXEC-of-0) · **NEEDS-PORT** = no reimplementation exists anywhere.

## Fixed this lane (3)

| # | Address | Name | Was | Fix |
|---|---------|------|-----|-----|
| 3 | 0x14000a170 | FindComponent\<ClearAreaComponent\> | LIVE-GUARDED, permanently null | `buildcomponent_state1.cpp` now calls the already-ported `fn_FindComponent_SM` (engine_funcs.h) instead of resolving the raw address |
| 4 | 0x140012a60 | ModDialog array push (stride 0x58) | **LIVE-UNGUARDED** — `moddialog.cpp:479` called it with no null check; adding an installed mod's thumbnail EXEC'd 0 | Wrote a byte-faithful C++ port (`fn_ModDialog_ArrayPush58_CppImpl`, engine_funcs.cpp) modeled on the decompile and the codebase's existing `fn_ExeArrayPush` pattern; wired `fn_ModDialog_ArrayPush` to it |
| 58 | 0x140118270 | ClearAreaComponent finalize (Ghidra: `ModelComponent_impl_8270`) | LIVE-GUARDED, permanently null | `buildcomponent_state1.cpp` now calls the already-ported `ClearArea_Finalize8270` (exported from `clearareacomponent_states.cpp` via `clearareacomponent_funcs.h`) instead of resolving the raw address |

All three compile clean (`mingw32-make standalone-debug`). No new `RegisterExeAddr` calls
were added — both wiring fixes route to existing C++ ports instead, so `standalone/reg_nullsweep.cpp` was not needed for these.

## Full table (62)

| # | Address | Ghidra name | Verdict | Evidence | Existing port | Action |
|---|---|---|---|---|---|---|
| 1 | 0x140002280 | FUN_140002280 | LIVE-GUARDED (4x) | debugui.cpp:314, citizen_assignworkplace.cpp:272/315, firecomponent_ignite.cpp:358/508, tornadocomponent.cpp:994/1006 — all `if (fn)` guarded | `FindSiblingEmitter()` in ambientenvironmentcomponent.cpp solves the same problem via `Object::GetTypeIdByName`, but different signature | NEEDS-PORT. Deliberately deferred already (entity_finders_disaster.cpp:37-40): target type-id global `0x1401c3e60` (AmbientEmitterComponent) is itself unregistered, so porting the finder alone wouldn't unblock it. Not attempted here. |
| 2 | 0x140004920 | CreateFencedDescription_CreateInPlace | DEAD | fn_CreateFencedDescription_Ctor: decl+assign only, no caller | createfenceddescription.cpp wires `CreateFencedDescription_CreateInPlace` directly via `REGISTER_TYPE`, bypassing the exe address | None needed |
| 3 | 0x14000a170 | FUN_14000a170 | **FIXED** | see above | fn_FindComponent_SM | Wired |
| 4 | 0x140012a60 | FUN_140012a60 | **FIXED** (was LIVE-UNGUARDED) | moddialog.cpp:479 | new port | Ported + wired |
| 5 | 0x140017050 | CitizenComponent_helper_09e0_sub_7050 | N/A | No `ResolveAddr(...,0x140017050)` call exists anywhere in the tree | already replaced by inline `fn_CitizenComponent_HasVacancy` (engine_funcs.h) | False positive in sweep list — already fixed by an earlier lane |
| 6 | 0x14001b1d0 | FUN_14001b1d0 | LIVE-GUARDED | buildinggoal.cpp:139-140, 229-230 (`fn_StringHash`) | none | NEEDS-PORT — 49 xrefs in the exe (shared utility); didn't want to guess the hash algorithm (FNV/djb2/CRC) without disasm confirmation |
| 7 | 0x14001b340 | FUN_14001b340 | LIVE-GUARDED | buildinggoal.cpp:116-117 (`fn_BuildingTypeArray_Resize`) | none | NEEDS-PORT |
| 8 | 0x14001fdb0 | TradeWork_helper_fdb0 | LIVE-GUARDED | serialize_struct60b.cpp:131-132 | none | NEEDS-PORT |
| 9 | 0x14001fe60 | TradeWork_helper_fe60 | LIVE-GUARDED | serialize_struct60b.cpp:140-141 | none | NEEDS-PORT |
| 10 | 0x140021790 | ToolbarDialog_helper_1790 | DEAD | toolbardialog.cpp:1198 comment confirms the sort was ported directly (inline stable insertion sort); `fn_ToolbarSort` has zero callers | inline sort in toolbardialog.cpp | None needed |
| 11 | 0x1400328c0 | CreateBridgeDescription_CreateInPlace | DEAD | decl+assign only | createbridgedescription.cpp REGISTER_TYPE | None needed |
| 12 | 0x1400329b0 | CreateDragDescription_CreateInPlace | DEAD | decl+assign only | createdragdescription.cpp REGISTER_TYPE | None needed |
| 13 | 0x14005e120 | thunk_helper_e120 | LIVE-GUARDED | optionsdialog.cpp:894,908 (`fn_UpdateAutoSaveTimer`) | none | NEEDS-PORT |
| 14 | 0x14005e230 | AutoSaveDialog_CopyCtor | LIVE-GUARDED | gameupdateloop.cpp:2198-2201 (`fn_AutoSaveDialog_Ctor`) | none | NEEDS-PORT — small (221 bytes), same shape as EventNotifyDialog's already-ported standalone Ctor; good next target |
| 15 | 0x14005F3D0 | ConsumeProduceUI_Serialize | DEAD | fn_ConsumeProduceUI_Slot4: zero callers | consumeproduceui.cpp implements `ConsumeProduceUI_Slot4` directly, wired into the vtable | None needed |
| 16 | 0x140068ab0 | EventNotifyDialog_CopyCtor | DEAD | fn_EventNotifyDialog_Ctor: zero callers | `EventNotifyDialog_Ctor_Standalone` (eventnotifydialog.cpp) called directly from gameupdateloop.cpp:2163 | None needed |
| 17 | 0x14006bd80 | MapParamsDialog_CopyCtor | Effectively DEAD (unreachable) | Called unguarded from `MainMenuDialog_OpenMap`, but that is only called from `MainMenuDialog_Slot8`, which is never wired into any vtable in standalone — confirmed by grepping for `&MainMenuDialog_Slot8`, zero hits | live "Map" button path is `StandaloneOpenMapParams` (mainmenudialog.cpp:1139), a hand-built alternate ctor that never calls `fn_MapParamsDialog_Ctor` | None needed now; **flag**: if `MainMenuDialog_Slot8` is ever wired live, this becomes a real EXEC-of-0 |
| 18 | 0x14006e9b0 | FUN_14006e9b0 | **LIVE-UNGUARDED** | moddialog.cpp:681, `fn_ModDialog_CloseAndReload((void*)self)` no null check | none | Reporting only — moddialog.cpp is off-limits (sibling lane) |
| 19 | 0x1400786b0 | ScenarioDialog_CopyCtor | Effectively DEAD (unreachable) | Same shape as #17 — called unguarded from `MainMenuDialog_OpenScenario`, itself only reachable from unwired `MainMenuDialog_Slot8` | none | None needed now; same flag as #17 |
| 20 | 0x140078830 | operator_impl_8830 | DEAD | fn_SelectResourceUI_PopulateCombo: zero callers | selectresourceui.cpp:169 reimplements PopulateCombo directly | None needed |
| 21 | 0x140078a40 | FUN_140078a40 | DEAD | fn_SelectResourceUI_GetResourceCount: zero callers | selectresourceui.cpp:199 | None needed |
| 22 | 0x14007ebf0 | FUN_14007ebf0 | DEAD | fn_TownHallUI_InitAgriculture: zero callers | townhallui.cpp:765 | None needed |
| 23 | 0x14007f660 | operator_impl_f660 | DEAD | fn_TownHallUI_InitInventory: zero callers | townhallui.cpp:590 | None needed |
| 24 | 0x14007fc30 | operator_impl_fc30 | DEAD | fn_TownHallUI_InitProduction: zero callers | townhallui.cpp:491 | None needed |
| 25 | 0x140080160 | FUN_140080160 | DEAD | fn_TownHallUI_InitProfessions: zero callers | townhallui.cpp:230 | None needed |
| 26 | 0x1400846b0 | TradeUI_RefreshDisplay | DEAD | zero callers; header comment: "kept for reference / fallback only" | tradeui.cpp full reimplementation (2762+, "Original: 0x1400846b0 (336 lines)") | None needed |
| 27 | 0x1400896a0 | FUN_1400896a0 | DEAD | fn_CitizenBehavior_ShuffleSchools: zero callers | citizenbehavior.cpp:422 | None needed |
| 28 | 0x140089ed0 | Entity_helper_9ed0 | DEAD | fn_CitizenBehavior_CountResources: zero callers | citizenbehavior.cpp:1034 | None needed |
| 29 | 0x14008a730 | FUN_14008a730 | DEAD | fn_CitizenBehavior_CollectStats: call site is even commented OUT (citizenbehavior.cpp:1749-1750) | citizenbehavior.cpp:1480 | None needed |
| 30 | 0x14008c100 | Entity_helper_c100 | DEAD | fn_CitizenBehavior_CheckThresholds: zero callers | citizenbehavior.cpp:1366 | None needed |
| 31 | 0x1400932A0 | Game_GetMonthFromTime | DEAD | fn_Game_GetMonthFromTime: zero callers | debugui.cpp:26 `DebugUI_GetMonthFromTime_Impl` | None needed |
| 32 | 0x140095750 | FUN_140095750 | **LIVE-UNGUARDED** | gameupdateloop.cpp:2568, `fn_EventNotifier_FireWithName(...)` — fires on scenario win/loss, no null check | none | **NEEDS-PORT, highest-priority remaining gap.** Nontrivial: builds a localized `String` via `EventLog_YearString`/`EventLog_SeasonString`/`UI::StringTable::GetText`/`String::FormatIndexed`, pushes into an EventArray, and can set a notification-chime flag. None of those `EventLog_*` helpers are ported yet — this needs its own small wave, not a quick fix. |
| 33 | 0x140097fb0 | SaveLoadDialog_helper_7fb0 | N/A | No resolve call for this address exists anywhere | engine_funcs.h: "REPLACED: was fn_SetEngineNextLoop... Now inline C++" | Already fixed by an earlier lane |
| 34 | 0x140099260 | OptionsDialog_helper_9260 | LIVE-GUARDED | optionsdialog.cpp:919-920, 1187-1188 (`fn_ApplyKeyMapToUI`) | none | NEEDS-PORT |
| 35 | 0x1400992f0 | FUN_1400992f0 | LIVE-GUARDED | optionsdialog.cpp:1239-1240 (`fn_RegisterDefaultInputBindings`) | none | NEEDS-PORT (1983 bytes — large) |
| 36 | 0x140099c20 | FUN_140099c20 | LIVE-GUARDED | tutorialimagehotkey.cpp:283-284, tutorialimageinputkey.cpp:252-253 (`fn_GetInputKeyName`) | none | NEEDS-PORT. Both call sites are in `code/Game/Tutorial/**` — off-limits, reporting only |
| 37 | 0x14009e8c0 | CreateNaturalResource_impl_sub_e8c0 | LIVE-GUARDED | gamegrid_serialize.cpp:107-111 | none | NEEDS-PORT. Extensively pre-documented as deliberately deferred ("large thread/region-pathfinder rebuild... out of scope") — matches the team's own prior scoping call, not attempted |
| 38 | 0x1400a3630 | MenuScene_Slot5 | DEAD | `s_fn_Serialize_Orig` (menuscene.cpp:88): assigned, never read anywhere else | menuscene.cpp reimplements MenuScene::Serialize directly | None needed |
| 39 | 0x1400a40e0 | FUN_1400a40e0 | LIVE-GUARDED | gameupdateloop.cpp:1255 (`fn_AmbientSound_Update`) | none | NEEDS-PORT |
| 40 | 0x1400a4c60 | FUN_1400a4c60 | LIVE-GUARDED | tradecomponent.cpp:326 (`s_tc_GenName`, gender-name generator) | none | NEEDS-PORT — reg_namedb.cpp notes sibling Markov-name helpers are also unported |
| 41 | 0x1400a5d10 | FUN_1400a5d10 | LIVE-GUARDED | gameupdateloop.cpp:1111 (`fn_Game_UpdateTimedEvents`) | none | NEEDS-PORT |
| 42 | 0x1400b4540 | FUN_1400b4540 | **LIVE-UNGUARDED** | scenariodialog.cpp:404 (`fn_StartNewGame`, ScenarioDialog::LoadSelected) | none | NEEDS-PORT (large — starts a whole new game). Currently only reachable if ScenarioDialog becomes constructible (see #19), so latent rather than active today |
| 43 | 0x1400b4790 | FUN_1400b4790 | **LIVE-UNGUARDED** | moddialog.cpp:686 (`fn_MainMenu_Reload`) | none | Reporting only — moddialog.cpp is off-limits |
| 44 | 0x1400b49f0 | FUN_1400b49f0 | **LIVE-UNGUARDED**, DUPLICATE REGISTRATION | Resolved into TWO variables: `fn_LoadScenarioFromSave` (scenariodialog.cpp:462) and `fn_Game_ReloadWithSave` (moddialog.cpp:713, off-limits) — both real, unguarded call sites for the same exe address | none | NEEDS-PORT. **Flag**: when ported, register once and point both variables at the same implementation (verify via Ghidra the two call sites truly invoke the same function/signature before merging) |
| 45 | 0x1400c4090 | EntityList_Update | DEAD | entitylist.cpp header: original hangs on a spinlock in standalone, so a CUSTOM (deliberately non-byte-faithful) reimplementation is used instead; the raw address is intentionally never registered | entitylist.cpp | None needed |
| 46 | 0x1400d6f00 | FUN_1400d6f00 | DEAD | fn_CitizenBehavior_ProcessPaths: zero callers | citizenbehavior.cpp:528 | None needed |
| 47 | 0x1400d70c0 | Entity_impl_70c0 | DEAD | fn_CitizenBehavior_BuildPathGraph: zero callers | citizenbehavior.cpp:698 | None needed |
| 48 | 0x1400e1600 | FUN_1400e1600 | DEAD | Dual-registered to `fn_FindDialogByName` and `fn_TSD_FindByString`, both zero-caller; engine_funcs.h: "NOTE: replaces both..." | `FindDialogByName_Impl` (inline, seen in tutorialsetupdialog.cpp) | None needed |
| 49 | 0x1400e1870 | FUN_1400e1870 | LIVE-GUARDED | tutorialgoalbutton.cpp:148-149, tutorialgoalcount.cpp:569-571 (`fn_FindUIElementByPath`) | none | NEEDS-PORT. Both call sites in `code/Game/Tutorial/**` — off-limits, reporting only |
| 50 | 0x1400f2f00 | Pickup_Serialize | DEAD (misleadingly named) | pickup.cpp has full documentation that this is actually `Pickup::Execute`, byte-faithfully reimplemented and wired directly into the vtable (`Pickup_Slot4_Impl`) | pickup.cpp | None needed — matches CLOSED memory note on this file |
| 51 | 0x1400fc6b0 | BridgeTool_Draw | DEAD (misleadingly named) | Actually `DragTool::OnKey`. dragtool.cpp:271 implements `DragTool_OnKey` directly, wired into BridgeTool/FencedTool/RoadTool vtables (bridgetool.cpp:309); `fn_DragTool_OnKey` has zero callers | dragtool.cpp | None needed |
| 52 | 0x1400fcc90 | EditTool_Deselect_CloseInfoDialog | DEAD, DELIBERATELY | Extensively documented (6+ files) as permanently unregistered — the same exe address doubles as a JMP-thunk target reused elsewhere; edittool.cpp:392 has the full rationale | edittool.cpp / tool_base_render.cpp direct ports | Confirmed intentional — no action |
| 53 | 0x140101a80 | FUN_140101a80 | Split verdict | Dual-registered: `fn_Toolbar_Utility` (DEAD, superseded by `Toolbar_Utility_Impl`, toolbardialog.cpp:601) and `fn_SetFollowBuilding` (**LIVE-UNGUARDED**, tutorialsetupdialog.cpp:369) | Toolbar_Utility half only | NEEDS-PORT for the SetFollowBuilding half; reporting only (Tutorial file, off-limits) |
| 54 | 0x140101ce0 | EntityUI_OpenPauseMenu | DEAD (misleadingly named) | Actually `fn_Toolbar_ToggleAll`. engine_funcs.h: "Kept as fallback only — fn_Toolbar_ToggleAll is no longer called by Toolbar_ToggleAll_Impl" | Toolbar_ToggleAll_Impl | None needed — matches CLOSED memory note (project_ingame_pause_menu_quit_verified.md) about this exact address |
| 55 | 0x140101ee0 | EntityUI_impl_1ee0 | LIVE-GUARDED | gameupdateloop.cpp:2223 (`fn_EntityUI_Startup`) | none | NEEDS-PORT — extensively documented known gap; tool_activation_standalone.cpp already works around its absence |
| 56 | 0x1401026a0 | MainMenuDialog_impl_26a0 | LIVE-GUARDED | gameupdateloop.cpp:1298 (`fn_EntityUI_Update`, really `EntityUI::Update`) | none | NEEDS-PORT — camera_input.cpp already documents and works around its absence |
| 57 | 0x1401027c0 | FUN_1401027c0 | LIVE-GUARDED | gameupdateloop.cpp:1350 (`fn_CalcWindFactor`) | none | NEEDS-PORT |
| 58 | 0x140118270 | ModelComponent_impl_8270 | **FIXED** | see above | ClearArea_Finalize8270 | Wired |
| 59 | 0x14011b560 | DecalComponent_Draw | DEAD (already ported) | decalcomponent.cpp full reimplementation wired into vtable; raw resolve at decalcomponent.cpp:581 unused as a call target | decalcomponent.cpp | None needed |
| 60 | 0x140128f50 | Entity_impl_8f50 | Split verdict | Actually `PickingComponent::OnAdded`. Vtable-dispatched OnAdded is fully ported (pickingcomponent.cpp) but 4 SEPARATE guarded call sites re-invoke the raw address directly as a "refresh picking mesh" operation (buildcomponent.cpp:529, buildcomponent_state8.cpp:330/471/570/594, destroycomponent_state6.cpp — which explicitly logs "pick-mesh rebuild SKIPPED" when null, a documented deviation) | PickingComponent_RegisterMesh (pickingcomponent.cpp) — signature not yet confirmed to match (entity vs. component pointer) | NEEDS-PORT for the 4 refresh call sites — did not wire blind since a param-type mismatch would silently misbehave rather than crash; flag for follow-up with Ghidra signature check |
| 61 | 0x14012bbd0 | StatusIconComponent_OnRemove | DEAD (already ported) | statusiconcomponent.cpp full reimplementation wired directly | statusiconcomponent.cpp | None needed |
| 62 | 0x14012db70 | ToolbarDescription_Ctor | DEAD | fn_ToolbarDescription_cleanup: decl+assign only, zero callers | none found (not required — dtor cleanup for a rarely-destroyed description) | None needed |

## Summary counts

- **Fixed this lane:** 3 (#3, #4, #58) — one of which (#4) was a genuine LIVE-UNGUARDED crash
- **DEAD** (vestigial, real port already exists elsewhere or superseded): 33
- **N/A / already fixed by an earlier lane** (no resolve call remains): 2 (#5, #33)
- **LIVE-GUARDED, NEEDS-PORT** (silently-skipped retail behaviour): 17
- **LIVE-UNGUARDED** (latent EXEC-of-0): 6 — #18, #32, #42, #43, #44, plus the SetFollowBuilding half of #53
- **Effectively dead/unreachable today** but would EXEC-of-0 if a currently-unwired caller (`MainMenuDialog_Slot8`) is ever wired live: #17, #19
- **Deliberately-unregistered by design** (documented, correct as-is): #52
- **Duplicate-registration traps** flagged for whoever ports them: #44 (0x1400b49f0), #53/(Toolbar_Utility+SetFollowBuilding share 0x140101a80)
