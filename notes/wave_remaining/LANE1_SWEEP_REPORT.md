# Lane 1 — Null Function-Pointer Sweep Report

62 exe addresses that our own source resolves via `ResolveAddr(Module::Exe, …)` but
nobody registers in `standalone/address_registry.cpp`. Method: grep every call site of
each address / each `fn_*` variable it is assigned to across the WHOLE main tree
(excluding `.claude/worktrees/*`), verify guard status, and check whether a clean C++
port already exists elsewhere (in which case the raw resolve is vestigial).

Legend: **DEAD** = never called (or superseded by an existing direct port) · **LIVE-GUARDED**
= called behind `if (fn)` · **LIVE-UNGUARDED** = called with no null check (latent
EXEC-of-0) · **NEEDS-PORT** = no reimplementation exists anywhere.

## Fixed this lane (6)

| # | Address | Name | Was | Fix |
|---|---------|------|-----|-----|
| 3 | 0x14000a170 | FindComponent\<ClearAreaComponent\> | LIVE-GUARDED, permanently null | `buildcomponent_state1.cpp` now calls the already-ported `fn_FindComponent_SM` (engine_funcs.h) instead of resolving the raw address |
| 4 | 0x140012a60 | ModDialog array push (stride 0x58) | **LIVE-UNGUARDED** — `moddialog.cpp:479` called it with no null check; adding an installed mod's thumbnail EXEC'd 0 | Wrote a byte-faithful C++ port (`fn_ModDialog_ArrayPush58_CppImpl`, engine_funcs.cpp) modeled on the decompile and the codebase's existing `fn_ExeArrayPush` pattern; wired `fn_ModDialog_ArrayPush` to it |
| 32 | 0x140095750 | EventLog AddEvent-with-name | **LIVE-UNGUARDED**, and the report's own "blocked on four unported string helpers" note was FALSE — every helper was already in `eventlog.cpp` | Ported as `EventLog_AddEventWithName`. Retail ships THREE byte-identical writers (0x140095580 / 0x140095750 / 0x140094e00) differing only in FormatIndexed arg count, so all three now share one core. That refactor exposed a second defect: the existing `EventLog_AddEvent` port was **missing retail's "drop to 1x speed on a critical event" tail** (types 0xb/0xc/0x1e/0x1f + settings byte +0x17) — it had only ever been ported into the AddCitizenEvent copy. Also fixed a leak of the achievement-title String in `GameUpdateLoop::OnScenarioComplete`. |
| 6 | 0x14001b1d0 | ExternalPtr lazy resolve (mis-named `fn_StringHash`) | LIVE-GUARDED, permanently null — and BOTH halves of the deferral were false: it is not a hash, and it was already ported | Wired `buildinggoal.cpp` to the existing `ResolveExternalByName_Impl`. This was the gate on the whole scenario-goal feature: `BuildingGoal`'s world-building BST is keyed on the RESOLVED description pointer at entry+0x10 (the source called it a "hash key"), so while the resolve no-op'd every goal looked up key 0, matched nothing, and could never be met. Fixing it also removed a latent stack smash — the evaluator passed an 8-byte buffer to a function that writes 0x18 bytes; retail copies the whole triple. |
| 7 | 0x14001b340 | ExternalPtr-triple array resize | LIVE-GUARDED, permanently null | Ported as `Goal_RefArray_Resize` + registered. Without it the load-leg resize no-op'd, leaving the entry array empty and every subsequent entry write going through a null base. |
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
| 6 | 0x14001b1d0 | FUN_14001b1d0 | **FIXED** | buildinggoal.cpp:139-140, 229-230 (`fn_StringHash`) | `ResolveExternalByName_Impl` (engine_funcs.h) | Wired. **Two false premises in the original entry.** (a) It is NOT a string hash — it is a lazy `ExternalPtr<T>::Resolve` (generation check vs ExternalFactory+8, `Find` by Filename, `IsDerivedFrom`, cache ptr at +0x10 / generation at +0x08), so "didn't want to guess FNV/djb2/CRC" was chasing something that does not exist. (b) It was **already ported** and is called inline by starttown / founding_spawn / tutorial\_\* — buildinggoal.cpp was the only file still resolving the raw address. Wiring-only fix, same as #3 and #58. |
| 7 | 0x14001b340 | FUN_14001b340 | **FIXED** | buildinggoal.cpp:116-117 (`fn_BuildingTypeArray_Resize`) | `Goal_RefArray_Resize` (goal_exe_helpers.cpp) | Ported + registered (reg_goal.cpp). ExternalPtr-triple array resize, stride 0x18 — the same primitive as `EventArray_Grow`. New entries MUST be seeded with generation **-1**: 0 is a valid factory generation, so a 0 seed makes the resolver above take its "cache still valid" early-out and the entry silently never resolves. |
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
| 32 | 0x140095750 | FUN_140095750 | **FIXED** (was LIVE-UNGUARDED) | gameupdateloop.cpp:2568, `fn_EventNotifier_FireWithName(...)` — fires on scenario win/loss, no null check | `EventLog_AddEventWithName` (eventlog.cpp) | Ported + registered (reg_citizen_25_tick.cpp). **The blocker in the original entry was a false premise**: it claimed the `EventLog_*` string helpers were unported, but `YearString`/`SeasonString`/`IconIndex`/`TileIndex`/`EventArray_Grow` have all been in `code/Game/Game/eventlog.cpp` since the tornado lane. Retail 0x140095750 is 0x140095580 `AddEvent` with ONE extra `FormatIndexed` arg — nothing else differs. |
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

## 2026-08-30 full audit — one agent per remaining LIVE-GUARDED row

All 16 remaining LIVE-GUARDED rows plus both reachable LIVE-UNGUARDED rows were
re-audited against retail. **The table's verdicts were wrong more often than right.**

| # | Addr | Audited verdict | Outcome |
|---|---|---|---|
| 1 | 0x140002280 | Blocker false, BUT blocked anyway | Ported, **deliberately unregistered**. Registering it CRASHED (READ of 0x10 in a710): AmbientEmitterComponent::OnAdded (0x140042e10) is a no-op stub so component+0x38 is never allocated. Register only after that is ported. |
| 8, 9 | 0x14001fdb0/fe60 | **False positive** | Dispatch-or-fallback: the inline `else` branch IS the port. Nothing broken, no action. |
| 13 | 0x14005e120 | Already ported, blocked on #14 | Ghidra's "thunk" name is wrong; the CALLER is the thunk. Needs 2 dispatch slots too. |
| 14 | 0x14005e230 | **DEAD; real fix is LARGE** | Behind hardcoded `kCreateGameDialogs=false`. Naive registration would crash — the ctor stamps retail vtable 0x14015b120, never mapped in standalone. Needs the eventnotify_sa hand-built-vtable pattern. |
| 34 | 0x140099260 | **FIXED** | ApplyKeyMapToUI. GameUpdateLoop::Startup reached it via a null pointer on every world boot: in-game UI sounds, fade sprite and UI scale never applied. |
| 35 | 0x1400992f0 | **FIXED** (1 line) | "1983 bytes, large" = 50 mechanical calls, already ported and running every boot. Only Options>Cancel was dead. |
| 36 | 0x140099c20 | NEEDS-PORT-SMALL, unverifiable | Tutorial key names render blank. Logic already ported TWICE elsewhere — dedupe rather than add a fourth copy. Autostart cannot reach the tutorial. |
| 37 | 0x14009e8c0 | **False premise — DO NOT implement** | Superseded by Region_Seed + Pathfinder_Seed (both default-on). Wiring it would double-build the region graph and spawn a second pathfinder thread over restored state. |
| 39 | 0x1400a40e0 | NEEDS-PORT-LARGE | Actually `MusicPlayer::Update`; background music never plays. ⚠ Also: GameUpdateLoop.h+0x80 is mislabelled `m_ambientSoundData` — retail stores the MusicPlayer struct there (verified in Load at 0x14009732e). +0x88's true identity is UNVERIFIED. |
| 40 | 0x1400a4c60 | **FIXED** | Merchant names were blank. Took TWO fixes: this address AND the unregistered String::operator= slot 0x1401514c0 behind it — the copy into the name array never happened at all. |
| 41 | 0x1400a5d10 | NEEDS-PORT-LARGE + **crash on wire** | Game+0x78 is always null (ctor zeroes it, no seed exists) and the body starts `*param_1 += *param_2`. Needs the queue's allocation and push side first. |
| 49 | 0x1400e1870 | NEEDS-PORT-LARGE | 4 callers, not 2. Kills the tutorial's "grey out everything but the target" effect. Needs 2 unported helpers + 1 new Runtime resolve. Unverifiable via autostart. |
| 55 | 0x140101ee0 | **False premise** | EntityUI::Startup's only boot-time effect (the 8-entry tool-mode tree) is already fully replicated by tool_activation_standalone.cpp. |
| 56 | 0x1401026a0 | **False premise — DO NOT implement** | EntityUI::Update already ported dispersed; registering it would double-drive camera/hotkeys/tool-tick every frame. |
| 57 | 0x1401027c0 | **FIXED** | Not a wind factor — camera-to-focus distance, returning a FLOAT in XMM0 that our typedef declared uint32_t. It was the only null leg of the guard, so Audio::Scene::Update never ran at all. |
| 42 | 0x1400b4540 | **FIXED** (was EXEC-of-0) | Already ported inline in NewGameDialog; extracted with retail's 10-arg ABI so ScenarioDialog shares it. |
| 53b | 0x140101a80 | **FIXED** (was EXEC-of-0) | Same address described two ways in our tree; it is the toolbar utility toggler, `fn_SetFollowBuilding` is a misnomer. |
| 18, 43, 44 | — | Already closed by e18fa16 | The report was stale; reg_mods.cpp registered all three. |

### What this says about the table

Of 18 rows audited, only **6** were straightforward gaps. Three would have caused
**active harm** if implemented as written (#1 crash, #37 double-build over restored
state, #56 double-drive) and two more (#14, #41) would have crashed on wiring.
Two (#8, #9) were not defects at all.

Recurring causes, in order of frequency:
1. **Our own `fn_*` name is a misnomer** — #37, #39, #53b, #57, #13, #14, #56, #8.
   Always grep the retail ADDRESS, never the name.
2. **Already ported under another name**, needing only wiring — #35, #40, #42, #53b.
3. **Dispatch-or-fallback**: an unregistered address is sometimes DELIBERATE
   because an inline `else` branch is the real port — #8, #9.
4. **Arming a function reveals its unmet dependencies** — #1, #14, #41.
   A build is not evidence; only a run is.

## Summary counts

- **Fixed:** 6 (#3, #4, #58 in the original lane; #6, #7, #32 in the 2026-08-30 follow-up)
- **DEAD** (vestigial, real port already exists elsewhere or superseded): 33
- **N/A / already fixed by an earlier lane** (no resolve call remains): 2 (#5, #33)
- **LIVE-GUARDED, NEEDS-PORT** (silently-skipped retail behaviour): 15 (was 17; #6 and #7 closed)
- **LIVE-UNGUARDED** (latent EXEC-of-0): 5 — #18, #42, #43, #44, plus the SetFollowBuilding half of #53 (#32 closed)

> ⚠ **Three of the four deferrals audited on 2026-08-30 rested on false premises**
> (#32's "blocked on four unported string helpers"; #6's "it's a hash of unknown
> algorithm" AND "needs porting" — it was neither). Before honouring any NEEDS-PORT
> note in this table, re-check its premise against retail and against the tree: grep
> the retail ADDRESS, not the `fn_*` name our source gave it, since two of the three
> were misnamed. See [[reference_stub_comment_outlived_the_port_that_replaced_it]].
- **Effectively dead/unreachable today** but would EXEC-of-0 if a currently-unwired caller (`MainMenuDialog_Slot8`) is ever wired live: #17, #19
- **Deliberately-unregistered by design** (documented, correct as-is): #52
- **Duplicate-registration traps** flagged for whoever ports them: #44 (0x1400b49f0), #53/(Toolbar_Utility+SetFollowBuilding share 0x140101a80)

## 2026-08-30 follow-up — the six named "still open" rows

Re-checked every row the previous pass left open. **Four of the six premises were wrong
again**, in the same four ways the section above catalogues.

| # | Addr | Prior note | Outcome |
|---|---|---|---|
| 39 | 0x1400a40e0 | NEEDS-PORT-LARGE; "+0x88's true identity is UNVERIFIED" | **Header settled + a second blocker found.** Still unported, but no longer mislabelled. |
| 41 | 0x1400a5d10 | "NEEDS-PORT-LARGE + crash on wire; Game+0x78 always null, no seed exists" | **FALSE PREMISE — now PORTED, REGISTERED and RUNTIME-VERIFIED.** |
| 42 | 0x1400b4540 | ScenarioDialog leg unproven, "needs one manual click" | **REAL BUG FOUND AND FIXED** without a click — settled against retail's own caller. |
| 36 | 0x140099c20 | NEEDS-PORT-SMALL, "dedupe rather than add a fourth copy" | **FALSE PREMISE — no port needed at all. WIRED.** |
| 13/14 | 0x14005e120 / 0x14005e230 | AutoSaveDialog | Premise **holds**. Confirmed blocked; exact shape of the work now known. |
| 49 | 0x1400e1870 | NEEDS-PORT-LARGE, unverifiable | Not attempted this pass. |

### 39 — GameUpdateLoop +0x80 / +0x88 (the header question)

Settled **instruction-by-instruction** against retail Load (0x140096B00), not from the
plate comment — which is itself wrong here.

- **+0x80 IS the MusicPlayer** (`param_1[0x10]`): `MemAlloc(0x18,4)` seeded
  `{currentTrackId=-1, Find("Game/MusicTracks.rsc"), 0}`. Was named `m_ambientSoundData`.
- **+0x88 is NOT a MusicPlayer and NOT the "SkyMesh Transform"** the plate comment claims
  (`param_1[0x11]`): `MemAlloc(0x60,8)` + ctor 0x1400182b0. It is a **loose Morton
  (Z-order) quadtree over characters** — 8 levels, level-base table
  {0,1,5,0x15,0x55,0x155,0x555,0x1555} at +0x40, 21845 cells at +0x30 (seeded -1) with a
  parallel count array at +0x50. 0x140018660 re-buckets one entity (level from its bounds,
  Morton code from its position — the 0x33333333/0x55555555 bit-spread). Our own tree
  already agreed under three different names (`charMgr`, `gridPtr`, `bridgeMgr`) and
  already has `CharManager_Seed()` for it.

The rename **exposed a latent collision**: `fn_MusicPlayer_Dtor` (0x140018220) was not a
music dtor at all — it frees exactly the five arrays at +0x10/+0x20/+0x30/+0x40/+0x50 of
the quadtree. It is now `fn_CharacterGrid_Dtor`. (Shutdown's behaviour is unchanged: the
old code already passed +0x88 to it, just under the wrong name.)

`0x1400a40e0` itself is confirmed `MusicPlayer::Update`: it early-outs on
`Audio::Scene::IsEffectPlaying(gw+0x78, currentTrackId)`, then selects a track whose
[monthLo..monthHi] and [moodLo..moodHi] ranges bracket the current month (DayCounter at
+0x128) and the town mood averaged over the citizen list (+0xF0).

**Second blocker, previously unrecorded:** our Load port leaves the MusicTrackList at
musicPlayer+0x08 **null** ("Find deferred"). 0x1400a40e0 reads
`*(int*)(musicPlayer[+0x08] + 0x28)` as the track count with no null check, so wiring the
port before doing that `Find` would be an immediate null deref. Recorded at the deferral
site in `gameupdateloop_load.cpp`.

### 41 — 0x1400a5d10 is the REGION-PAIR CACHE EVICTOR (CLOSED)

Every part of the prior note was wrong. It is not a timed-event queue, `Game+0x78` is not
null, and there is no crash on wiring.

- **What it is:** the staleness sweep for the pathfinder's **region-pair distance cache**.
  The 0x38-byte object at Game+0x78 is built by `Region_Seed` → `Pf_RegionPairCache_Build`
  (our port of the tail of 0x14009e8c0), which is **`BANISHED_LIVE_REGIONMAP`, default-ON**.
  Proven at runtime: `[RegionSeed] … pairCache=0x…` is non-null on every boot.
- **Why #37 and #41 are the same function.** The prior pass ruled #37 (0x14009e8c0) "DO NOT
  implement — superseded by Region_Seed + Pathfinder_Seed". That is right about its *head*
  (region graph +0x70, pathfinder thread +0x68) but **misses its tail**, which is the only
  thing that allocates Game+0x78.
- **The bite:** `cache[0]` is the cache's clock, and *nothing else in the binary advances
  it*. `Pf_RegionPairCacheInsert_5b30` (already ported and registered) stamps each new entry
  with the then-current `cache[0]`. With this function unported the clock sat at 0 forever,
  every entry was stamped 0, and **no entry could ever age out** — cached region-to-region
  distances survived until 131072 further inserts pushed them out, i.e. effectively forever,
  across bridges/roads/terrain edits that re-split regions.
- **A second, independent defect fixed alongside it:** `Pf_RegionPairCache_Build` seeded the
  retention window at cache+0x08 to 0 as a documented "safe" deviation. It is not inert — it
  is the value this evictor compares against, so 0 would evict every entry on the first
  tick. Retail seeds `Time::Time(DAT_140172210)`, and DAT_140172210 = **120.0f**.
- **Verified at runtime** (`BANISHED_DIAG_PAIRCACHE=1`): clock advances 16666µs/tick,
  `window=120000000`µs = exactly 120 s, and at call 7202 (clock 120028532) 3 of 5 entries
  aged out while the 2 newest were retained — so the window is compared per-entry, not as a
  blanket flush. No crash.

Ported as `Pf_RegionPairCacheTick_5d10` (region_paircache.cpp), registered in
reg_pf_region.cpp. Its one dependency (the AVL delete 0x14001ca60) was already registered.

### 42 — the ScenarioDialog leg had a REAL argument-marshalling bug

Settled statically against retail's own `ScenarioDialog::LoadSelected` (0x140077cb0), which
is stronger than the manual click for this question — the click would have shown a broken
world rather than the cause.

`StartNewGame_Impl` was lifted out "with retail's own 10-argument ABI", but its parameters
6 and 8 were **swapped versus retail**:

- Retail 0x1400b4540 passes args 6/7/8 straight into the GameUpdateLoop ctor's params
  9/10/11, which store them at **+0x40 / +0x48 / +0x50 = mapGen / terrainGen / climateGen**.
- Both retail callers pass the generators in that order (NewGameDialog at 0x14007050d;
  ScenarioDialog reads scenario +0x70/+0x78/+0x80 into args 6/7/8).
- Our impl declared them `(climateGen, terrainGen, mapGen)` and assigned +0x40 = arg8.

NewGameDialog was **unaffected** — it passed our order, so +0x40 still received the real
MapGenerator, which is why the autostart-proven leg looked healthy. But ScenarioDialog was
written from retail's assembly and passes retail's order, so through the newly-registered
address it would have put the **ClimateGenerator into +0x40** (camera placement / town spawn
pools) and the **MapGenerator into +0x50** (Environment/Weather).

Fixed by moving `StartNewGame_Impl` to retail's ABI and flipping NewGameDialog's call to
match; ScenarioDialog needed no change to its call, only to three misleading local names
(its +0x70 is mapGen and its +0x80 is climateGen, not the reverse). Regression-checked:
`mapGen(loop+0x40)` still resolves and `mapGen+0x3c(townSize)=13` is unchanged.

**Still needs the manual click** to exercise the rest of ScenarioDialog::LoadSelected
(dialog construction, the saved-scenario branch). The argument marshalling itself no longer
depends on that.

### 36 — 0x140099c20 needed NO port (CLOSED, WIRED)

The note said "already ported TWICE elsewhere — dedupe rather than add a fourth copy".
Stronger than that: `BuildKeyName` in `hotkeytooltipui.cpp` is labelled, in the source,
**"Port of FUN_140099c20"** — the exact address. And retail's `param_1` is the KeyMap
container whose +0xD0 is the keyNames StringTable, which is precisely the
`*g_pKeyMapSingleton` both tutorial call sites were already passing. So the substitution is
exact, not approximate.

Exported it as `Game::Hotkey_BuildKeyName` and pointed both tutorial sites at it. Tutorial
key names were rendering blank purely because the raw address was unregistered. Builds and
links; the tutorial still cannot be *reached* by autostart, so the visual result is
unverified — but the call is provably the same one retail makes.

### 13 / 14 — AutoSaveDialog: premise CONFIRMED, work now specified

The one open whose prior note survived scrutiny.

- **#14 (0x14005e230)** is `AutoSaveDialog::Ctor(self, ui, dialogRes, parentIBP)`: base
  DialogInstance ctor, stamp `&vtable_AutoSaveDialog`, then `FindElement("savingImage")` →
  +0xB8 and `FindElement("animatedImage")` → +0xC0, +0xC8 = 0. Small. The blocker is real:
  it stamps a retail vtable with no standalone counterpart, so it needs the
  `EventNotifyDialog_Ctor_Standalone` hand-built-vtable treatment.
- **#13 (0x14005e120)** is `AutoSaveDialog::RestartAutoSaveTimer(dialog)`: `StopTimer(0)`,
  index `g_pGameSettingsPtr[1]` (the Options autosave combo, 0..7) into a table of 8 float
  constants, then `StartTimer(0, interval * DAT_1401721f0)`.
- **#13 is confirmed blocked on #14**, and the reason is exact: `optionsdialog.cpp:936`
  walks the open dialogs looking for one of type AutoSaveDialog and only then calls e120.
  With `kCreateGameDialogs = false` (gameupdateloop.cpp:2041) no such dialog is ever
  constructed, so the loop matches nothing. **Porting e120 first would be a null test.**
