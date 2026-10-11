# B.3 step 8 — block 0, inventory B: dialogs, controllers, loops, goals, tools, tutorial, Game externals

Read-only inventory, 2026-09-26, main @ 1d7866d4 (agent 0B). Scope: `code/Game/{Dialog,Game,Goal,Player,Tutorial}/`,
every pointer array under `standalone/*.cpp|h` (build dirs excluded), their `engine/types/**` PODs, and the four
Element/ElementDescription subclasses (files in `code/Game/Component/`, listed in §0.4 because step 6 did). Starting
points `b36_session/EXE_SUBCLASSES.md` and `b37_session/EXE_SUBCLASSES.md` were re-verified; their line numbers have
moved (step 7) and several facts changed — called out where they matter. No source was edited, nothing was built or
run. Retail truth: Ghidra `Application-x64.exe`, read with three read-only Java scripts (§5); the bridge was switched
back to `Runtime-x64.dll` afterwards. Agent 0C owns the full exe-table dump; my retail slot counts come from my own
dump of the 0B address ranges (method §5) and should agree with 0C's.

Conventions: as `BLOCK0_BRIEF.md`. Per-slot compact codes, one character per slot index 0..N-1:
`o` own body · `b` Runtime base body (filled by name / by address / copied, or inherited by `Apply()` mask) ·
`s` standalone stub that returns a constant or re-implements a base body retail inherits (Return0/ReturnNull/Ret0,
TypeName stubs, the loop `Stub_*` re-implementations) · `n` file-local no-op (StubNop / Nop / MenuDlg_Nop / SD_Nop /
RuntimeThunk<N> print-once / ModDlg_InertSlot / BaseNoOp / purecall stub) · `0` literal nullptr nothing fills.
48-slot tables are shown as the first 14 codes then `…+34n`. QUAL = QUALIFIED `->NS::Cls::member(` calls already
in the table's file (§3 of each earlier inventory; all of this file's classes count). "retail N" = slots in the
exe table, trailing 0 terminator excluded.

## 0. Headline numbers

| what | count | how |
|---|---|---|
| hand-built pointer tables in scope (excluding 2 extern re-declarations) | **119** (Dialog 58, Game 18, Tutorial 18, Player 10, Goal 5, standalone 10) | `arrays.py` + `analyze.py` (§5) |
| of which exe-class tables (excluding the UI_DIALOG_STUB macro body, the SpriteSheet/MaterialInstance mirror, the Particle scratch copy and the EnumStub diagnostic) | 115 | rows |
| `UI_DIALOG_STUB` macro instances (each its own 48-slot table) | 12 (`ui_dialog_stub_factories.cpp:1671-1681`, :798-799, :1194) — 3 DialogController-only (ModBrowseUI, NoiseUI, SpriteCaptureUI), 1 DialogInstance-only (ModUploadDialog), **8 dormant twins** of DialogInstance classes that ALSO have a hand table | `grep -n "^UI_DIALOG_STUB("` |
| exe tables in retail with NO tree table | 13 (§0.3) | Ghidra dump vs tree |
| REGISTER_TYPE (+_FULL) in scope | 95 (+4 Element/Description in code/Game/Component; +12 UI_DIALOG_STUB, + 6 `REGISTER_ABSTRACT_FACTORY` bases in `code/Game/Object/factory_registrations_bulk.cpp:94,142,156,186-188`) | `rtypes.py` |
| RegisterExeAddr of a vtable-range address (0x140153000-0x140162000), repo-wide | 155; in scope files 75 (Dialog 13, Game 14, Goal 5, Player 9, Tutorial 14, standalone 20) | `regs.py` |
| exe-class tables that are **not** RegisterExeAddr'd (directly or through a getter) | **44** of 115 class tables: 33 of the 35 DialogController tables (only ControlUI and TutorialUI are), StatusIconUIConfig, AutoSave/EventNotify/EventPopup/Follow/MapParams/Toolbar dialogs, the ToolUI child DialogInstance, Tornado, TornadoInstance, Pathfinder. Reached through their factory / hand ctor; the only by-address readers are engine_funcs.cpp:1494 (FollowDialog 0x14015be00) and :1329 (ToolbarDialog 0x14015fa00), which get 0 today (§1.11) | `rows.json` `reg` column |
| retail secondary vptrs among 0B classes | **0** (§1.10) | exe stamp scan |
| slots in the 115 class tables, up to the retail length | 1,220: own 748 · base 182 · stub 107 · nop 126 · null 57; plus 1,432 slots PAST the retail end (1,416 nop — the 48-slot padding — 8 stub, 4 own, 4 null) | compact codes, `rows.json` |

### 0.3 Retail exe tables in the 0B address ranges with no tree table

| retail | slots | what (evidence) | tree today |
|---|---|---|---|
| 0x140161f10 | 13 | `Tool` (abstract: 4-9 `_purecall`); stamped by every tool ctor/dtor (`Tool_Ctor*`, `BridgeTool_ScalarDtor` 0x14002d7f0 …) | none; `ResolveAddr(Exe, ToolAddr::Vtable)` at 9 sites (bridgetool.cpp:37, dragplacementtool.cpp:45, dragtool.cpp:35, edittool.cpp:53, fencedtool.cpp:47, pathtool.cpp:35, placementtool.cpp:182, removaltool.cpp:35, roadtool.cpp:43) — **unregistered → 0**, so every tool dtor's base restamp writes null |
| 0x140161c10 | 15 | `DragTool` (4,5,7,8,13 `_purecall`); `DragTool_Ctor` 0x1400fc5a0, `DragTool_ScalarDtor` 0x14002da20 | none; `REGISTER_ABSTRACT_FACTORY(DragTool, "Tool")`; 6 unregistered by-address reads (bridgetool.cpp:36, dragplacementtool.cpp:44, dragtool.cpp:34, fencedtool.cpp:46, removaltool.cpp:34, roadtool.cpp:42) |
| 0x140161ee8 | 4 | `ToolSetting` (RotationSetting ctor stamps it then 0x140160710) | none; abstract factory :188 |
| 0x1401607a0 | 9 | `TutorialGoal` (6,7,8 `_purecall`) | registered to **TutorialGoalDialog's** table (reg_tutorial_wave.cpp:86) |
| 0x140160c30 | 11 | `TutorialSetup` (6-10 `_purecall`) | registered to **TutorialSetupDialog's** table (reg_tutorial_wave.cpp:88) |
| 0x14015d330 | 14 | `SelectResourceUI` (11-13 `_purecall`; Ghidra mislabels it `vtable_CropUI`) | none; abstract factory :94 |
| 0x14015e350 | 4 | exe `Achievement::AchievementInstance` copy (own dtor 0x140019d90) | not registered; gameupdateloop_load.cpp:1621 reads it by address → 0, keeps the Runtime table (b37 §1C) |
| 0x14015e240 | 9 | exe `Graphics::MeshInstance` copy for the SkyMesh (slot 1 thunk = `Graphics::MeshInstance::Update`; stamped in GameUpdateLoop::Load 0x140096b00) | not registered; fencedtool.cpp:799, placementtool.cpp:910 read it by address → 0 |
| 0x14015e370 | 9 (+0x14015e3b8/3c0 `Create`/`Generate`) | exe `Graphics::FrameCopyImageBuffer` copy (thunks to `External::Compile`, `FrameCopyImageBuffer::Create/Generate`); used by TakeScreenshot 0x140095da0 | none |
| 0x14015e290 | 11 | unidentified exe copy of a Runtime class (own dtor 0x140019e40 + 10 import thunks); stamped in GameUpdateLoop::Load and MenuDLT::Load 0x14009a930 | none — **not measured** which class |
| 0x14015ecb0 | 10 | unidentified exe copy (own dtor 0x14001a410 + thunks); stamped in MenuDLT::Load | none — not measured |
| 0x14015c6c0 | 10 | `ModUploadDialog` (Ghidra name `g_ModUpdateDialog_Vtable`) | only the dormant `UI_DIALOG_STUB` table (unregistered; engine_funcs.cpp:1249 reads it by address → 0) |
| 0x14015f5a0 | 10 | bare `UI::DialogInstance` copy used as the ToolUI child dialog | tree table exists (row in §B) but **deliberately unregistered**; engine_funcs.cpp:1336, :1543 read it → 0 |

(0x14015f980, read at engine_funcs.cpp:1342, is inside StorageUI's range and not a table start in my dump — not measured.)

### 0.4 Element / ElementDescription subclasses (files in code/Game/Component, re-verified)

| class | parent literal | table | retail | tree/retail slots | how | note |
|---|---|---|---|---|---|---|
| FrustumOutlineElement | `"Element"` (frustumoutlineelement.cpp:255) | `uintptr_t g_FrustumOutlineElement_Vtable[24]` :51 | 0x14015c2a0 | **24/12** | COPIED at first construction from the table `fn_Element_Ctor` installed (:226, loop :233), then 4/5/6 overridden (:234-236) | copies 24 entries from a 12-slot table — reads 12 past the end |
| CaptureBoxElement | `"Element"` (:40) | none — keeps the Runtime Element table | 0x14015d440 (12) | —/12 | `fn_Element_Ctor` :25 | Element slots 4-6 are `_purecall` (b36) |
| CaptureBoxDescription | `"ElementDescription"` (:123) | `g_CaptureBoxDescription_Vtable[11]` :90, reg :105 | 0x14015d4a0 | **11/10** | hand-built | table one past retail |
| FrustumOutlineDescription | `"ElementDescription"` (:197) | `g_FrustumOutlineDescription_Vtable[11]` :156, reg :177 | 0x14015c300 | **11/10** | hand-built | table one past retail |

---------------------------------------------------------------------------------------------------
## Main table — one row per hand-built table

Columns: class · parent (REGISTER_TYPE literal) · reflected (REGISTER_TYPE site) · table var file:line + fill
mechanism · retail exe vtable + whether/where RegisterExeAddr'd · slots tree/retail · per-slot codes · POD header:line
sizeof + alloc size in the factory · ctor door :line in the table's file (DialogController rows: none) · slot-0 body
→ base-dtor call · QUAL · notes. Multiple inheritance column is omitted because it is empty for every row (§1.10).

### A. Engine loop subclasses (standalone/*_stubs.cpp) — 5 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| GameUpdateLoop | — | no | `standalone/gameupdateloop_stubs.cpp:149` g_GameUpdateLoop_Vtable_Standalone — hand-built array (static initializer) | 0x14015e2f0 — RegisterExeAddr standalone/gameupdateloop_stubs.cpp:166 | 12/12 | `ooooooooooos` | `GameUpdateLoop.h:31` 0x160; alloc 0x160 | `fn_UpdateLoop_Ctor` newgamedialog.cpp:829, saveloaddialog.cpp:765, worldsetup_reload.cpp:76, tutorial_next_scenario.cpp:78; vptr stamped after | GameUpdateLoop_Destroy gameupdateloop.cpp:2702 → file-local door `fn_UpdateLoop_Dtor` :2726 (unqualified door, chains base) | 0 | tree 12 = retail 12. Slot 11 is a standalone stub (`false`). POD `engine/types/GameUpdateLoop.h` 0x160 (m_ fields, §2). |
| LoaderLoop "inner" | — | no | `standalone/loader_loop_stubs.cpp:229` g_innerLoaderLoopVtable — hand-built array (static initializer) | 0x140158870 — RegisterExeAddr standalone/loader_loop_stubs.cpp:360 | 12/12 | `osssssosssos` | `LoaderLoop.h:22` 0x48; alloc 0x48 | `fn_LoaderLoop_Ctor` game_main.cpp:797 | Inner_Destroy:127 → none | 0 | slot 0 frees only (skips ~LoaderLoop). Unnamed in Ghidra (`PTR_FUN_140158870`), stamped only by GameMain. |
| GameLoaderLoop "outer" | — | no | `standalone/loader_loop_stubs.cpp:341` g_outerLoaderLoopVtable — hand-built array (static initializer) | 0x1401588d0 — RegisterExeAddr standalone/loader_loop_stubs.cpp:361 | 12/12 | `osssssssssss` | —; alloc 0x48 | `fn_LoaderLoop_Ctor` mainmenudialog.cpp:278, newgamedialog.cpp:894, saveloaddialog.cpp:835, worldsetup_reload.cpp:134, tutorialui.cpp:134, tutorial_next_scenario.cpp:133 (+ game_main.cpp) | Outer_Destroy:250 → none | 0 | slot 0 frees only. 11 of 12 slots are standalone stubs re-doing the base. |
| MenuDialogLoopT | — | no | `standalone/menudialogloopt_stubs.cpp:782` g_menuDialogLoopVtable — hand-built array (static initializer) | 0x14015ed00 — RegisterExeAddr standalone/menudialogloopt_stubs.cpp:802 | 13/13 | `ooooosossoooo` | —; alloc 0xC00 | inline `MenuDialogLoopT_Ctor` engine_funcs.h:2330 (→ `fn_DialogLoopT_Ctor`) from game_main.cpp:681, mainmenudialog.cpp:251, tutorialui.cpp:96 | Stub_Destroy:242 → none | 4 | slot 0 `Stub_Destroy` frees only: skips ~DialogLoopT, ~Audio::Scene(+0xb30), ~UserInterface. Slot 2 Shutdown now calls `Engine::DialogLoopT::Shutdown` QUALIFIED (b37 §3B carry-over 11 is partly closed — verify the teardown part). |
| GameLoadDialogLoopT (splash) | — | no | `standalone/splash_loop_stubs.cpp:612` g_splashDialogLoopVtable — hand-built array (static initializer) | 0x140158800 — RegisterExeAddr standalone/splash_loop_stubs.cpp:632 | 13/13 | `oooosssssssso` | —; alloc 0xB60 | `fn_DialogLoaderLoopT_Ctor` game_main.cpp:703, mainmenudialog.cpp:261, newgamedialog.cpp:877, saveloaddialog.cpp:812, worldsetup_reload.cpp:116, tutorialui.cpp:110, tutorial_next_scenario.cpp:116 | Stub_Destroy:132 → none | 3 | retail dump reads 14 with the trailing 0 terminator; 13 real. slot 0 frees only (skips ~DialogLoaderLoopT). |

### B. UI::DialogInstance subclasses — 20 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| AutoSaveDialog | "UI::DialogInstance" | yes `code/Game/Dialog/autosavedialog.cpp:284` | `code/Game/Dialog/autosavedialog.cpp:202` autosave_sa::g_vt — hand-built array (static initializer) | 0x14015b120 — NOT registered | 10/10 | `ooononooon` | `ui/Dialogs.h:156` 0xD0; alloc 0xD0 | `fn_DialogInstance_Ctor` :249 | Dtor:213 → fn_DialogInstance_Dtor | 0 | **Registered TWICE**: REGISTER_TYPE autosavedialog.cpp:283 AND `UI_DIALOG_STUB` ui_dialog_stub_factories.cpp:1672 (the only duplicate type name in the tree, §1.9). Not RegisterExeAddr'd. |
| EventNotifyDialog | "UI::DialogInstance" | yes `code/Game/Dialog/eventnotifydialog.cpp:1039` | `code/Game/Dialog/eventnotifydialog.cpp:990` eventnotify_sa::g_vt — hand-built array (static initializer) | 0x14015bca0 — NOT registered | 10/10 | `ooonooooon` | `ui/Dialogs.h:167` 0xF0; alloc 0xF0 | `fn_Time_Ctor_Float` :463; `fn_Time_Ctor_Float` :553 | Dtor:955 → fn_DialogInstance_Dtor | 0 |  |
| EventPopupDialog | "UI::DialogInstance" | yes `code/Game/Dialog/eventpopupdialog.cpp:170` | `code/Game/Dialog/eventpopupdialog.cpp:133` eventpopup_sa::g_vt — hand-built array (static initializer) | 0x14015bd80 — NOT registered | 10/10 | `ooononnnon` | `ui/Dialogs.h:184` 0xC0; alloc 0xC0 | `fn_DialogInstance_Ctor` :153 | Dtor:121 → fn_DialogInstance_Dtor | 0 |  |
| FollowDialog | — | no | `code/Game/Dialog/followdialog.cpp:449` g_FollowDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015be00 — NOT registered | 10/10 | `osoooooooo` | `ui/Dialogs.h:192` 0xD8; alloc — | — | FD_SA_Dtor:398 → fn_DialogInstance_Dtor | 0 | Table NOT registered; `engine_funcs.cpp:1494` reads 0x14015be00 by address → 0. Reflected only via the dormant `UI_DIALOG_STUB` (:1675). |
| LoaderDialog | "UI::DialogInstance" | yes `code/Game/Dialog/loaderdialog.cpp:214` | `code/Game/Dialog/loaderdialog.cpp:117` g_LoaderDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015f300 — RegisterExeAddr code/Game/Dialog/loaderdialog.cpp:186 | 10/10 | `ooononoonn` | `ui/Dialogs.h:214` 0xC8; alloc 0xC8,0xC8 | `fn_DialogInstance_Ctor` :161 | StandaloneStub_Dtor:85 → fn_DialogInstance_Dtor | 0 |  |
| MainMenuDialog | "UI::DialogInstance" | yes `code/Game/Dialog/mainmenudialog.cpp:1314` | `code/Game/Dialog/mainmenudialog.cpp:1061` g_MainMenuDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015f550 — RegisterExeAddr code/Game/Dialog/mainmenudialog.cpp:1287 | 20/10 | `ooonooooononnnnnnnnn` | `ui/Dialogs.h:225` 0xC8; alloc 0xAE8,0x160 | `fn_NewGameDialog_Ctor` :142; `fn_SaveLoadDialog_Ctor` :171 | StandaloneStub_Dtor:821 → fn_DialogInstance_Dtor | 2 | tree 20 vs retail 10 (retail name `g_PauseDialog_Vtable`). Slot 10 also calls the base dtor door. |
| MapParamsDialog | — | no | `code/Game/Dialog/mainmenudialog.cpp:1140` g_MapParamsDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015c180 — NOT registered | 20/10 | `ooonooooononnnnnnnnn` | `ui/Dialogs.h:234` 0xF8; alloc 0xAE8,0x160 | `fn_NewGameDialog_Ctor` :142; `fn_SaveLoadDialog_Ctor` :171 | MP_VDtor:1104 → fn_DialogInstance_Dtor | 2 | tree 20 vs retail 10: slots 10..19 are a standalone "secondary block" (not a retail secondary vptr — retail has none). dormant `UI_DIALOG_STUB` twin (:1677). `mainmenudialog.cpp:1102` is an extern re-declaration of the same array. |
| AchievementDialog | — | no | `code/Game/Dialog/menu_dialog_standalone_vtables.cpp:90` g_AchievementDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015b020 — RegisterExeAddr code/Game/Dialog/menu_dialog_standalone_vtables.cpp:150 | 10/10 | `onononooon` | `ui/Dialogs.h:141` 0xD0; alloc — | — | AD_Dtor:71 → fn_DialogInstance_Dtor | 0 | No secondary vptr: `Dialogs.h:23 SecondaryVtable=0x14015b070` is AttendanceUI's table (§1.1). Also a dormant `UI_DIALOG_STUB` registration (ui_dialog_stub_factories.cpp:1671) — the REFLECTED factory builds the 48-nop stub, the hand path builds this table. |
| GoalDialog | — | no | `code/Game/Dialog/menu_dialog_standalone_vtables.cpp:128` g_GoalDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015beb0 — RegisterExeAddr code/Game/Dialog/menu_dialog_standalone_vtables.cpp:152 | 10/10 | `onononooon` | `ui/Dialogs.h:204` 0xC8; alloc — | — | GD_Dtor:109 → fn_DialogInstance_Dtor | 0 | dormant `UI_DIALOG_STUB` twin (:1676). |
| MessageDialog | "UI::DialogInstance" | yes `code/Game/Dialog/messagedialog.cpp:289` | `code/Game/Dialog/messagedialog.cpp:208` g_MessageDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015c480 — RegisterExeAddr code/Game/Dialog/messagedialog.cpp:276 | 14/14 | `ooonoonnonoooo` | `ui/Dialogs.h:250` 0xC0; alloc sizeof(MessageDialog) | `fn_DialogInstance_Ctor` :241 | MD_Stub_Dtor:158 → fn_DialogInstance_Dtor | 0 | retail 14 (Ghidra name `g_ReloadDialog_Vtable`). Base of OptionInputBindDialog in retail (OIBD ctor stamps 0x14015c480 then 0x14015ce40). |
| ModDialog | "UI::DialogInstance" | yes `code/Game/Dialog/mod_dialog_vtables.cpp:514` | `code/Game/Dialog/mod_dialog_vtables.cpp:99` g_ModDialogVt — filled at init; FillBaseSlots by name (mod_dialog_vtables.cpp) | 0x14015c870 — RegisterExeAddr standalone/reg_mods.cpp:76 via ModDialog_GetVtable() | 10/10 | `obobobbbob` | `ui/Dialogs.h:267` 0xD8; alloc 0xD8,0xD8 | `fn_DialogInstance_Ctor` :322; `fn_SavedState_Ctor` :432 | ModDialog_VDtor:117 → fn_DialogInstance_Dtor | 0 | No secondary vptr: `Dialogs.h:81 SecondaryVtable=0x14015c8c0` is a separately allocated Video::ImageBuffer (stamped at offset 0 of a new object in ModDialog_Serialize 0x14006f150, NoiseUI_Serialize, TradeWork). 1,3,5,6,7,9 filled BY NAME (`FillBaseSlots`). |
| ModDetailsDialog | "UI::DialogInstance" | yes `code/Game/Dialog/mod_dialog_vtables.cpp:515` | `code/Game/Dialog/mod_dialog_vtables.cpp:100` g_ModDetailsVt — filled at init; FillBaseSlots by name (mod_dialog_vtables.cpp) | 0x14015c670 — RegisterExeAddr standalone/reg_mods.cpp:77 via ModDetailsDialog_GetVtable() | 10/10 | `obobobbbob` | `ui/Dialogs.h:258` 0xC8; alloc 0xD8,0xD8 | `fn_DialogInstance_Ctor` :322; `fn_SavedState_Ctor` :432 | ModDetails_VDtor:190 → fn_DialogInstance_Dtor | 0 | same FillBaseSlots as ModDialog. |
| NewGameDialog | "UI::DialogInstance" | yes `code/Game/Dialog/newgamedialog.cpp:1174` | `code/Game/Dialog/newgamedialog.cpp:1074` g_NewGameDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015c920 — RegisterExeAddr code/Game/Dialog/newgamedialog.cpp:1163 | 10/10 | `ooonooooon` | `ui/Dialogs.h:286` 0xAE8; alloc 0x160,sizeof(NewGameDialog) | `fn_UpdateLoop_Ctor` :829; `fn_Time_Ctor_Float` :874 | NewGameStub_Dtor:1033 → fn_DialogInstance_Dtor | 1 |  |
| OptionInputBindDialog | "MessageDialog" | yes `code/Game/Dialog/optioninputbinddialog.cpp:413` | `code/Game/Dialog/optioninputbinddialog.cpp:304` g_OptionInputBindDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015ce40 — RegisterExeAddr code/Game/Dialog/optioninputbinddialog.cpp:394 | 14/14 | `ooonoonoonoooo` | `ui/Dialogs.h:361` 0xC8; alloc sizeof(OptionInputBindDialog) | `fn_DialogInstance_Ctor` :344 | OIBD_Stub_Dtor:246 → fn_DialogInstance_Dtor | 0 | retail parent MessageDialog (stamp chain), tree ctor runs `fn_DialogInstance_Ctor`, not MessageDialog's. |
| OptionsDialog | "UI::DialogInstance" | yes `code/Game/Dialog/optionsdialog.cpp:1819` | `code/Game/Dialog/optionsdialog.cpp:1731` g_OptionsDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015ceb0 — RegisterExeAddr code/Game/Dialog/optionsdialog.cpp:1808 | 10/10 | `ooonoonnon` | `ui/Dialogs.h:314` 0x1C0; alloc 200,0xC0 | `fn_Time_Ctor_Float` :947; `fn_OptionInputBindDialog_Ctor` :1496 | OptionsStub_Dtor:1689 → fn_DialogInstance_Dtor | 0 |  |
| SaveLoadDialog | — | no | `code/Game/Dialog/saveloaddialog.cpp:1278` g_SaveLoadDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015f900 — RegisterExeAddr code/Game/Dialog/saveloaddialog.cpp:1328 | 10/10 | `ooononooon` | `ui/Dialogs.h:373` 0x160; alloc 0x60,0x58 | `fn_MaterialInstance_Ctor` :246; `fn_SpriteSheet_Ctor` :290 | SLDStub_Dtor:1255 → fn_DialogInstance_Dtor | 0 | dormant `UI_DIALOG_STUB` twin (:1680). |
| ScenarioDialog | — | no | `code/Game/Dialog/scenariodialog.cpp:591` g_ScenarioDialog_Vtable_Standalone — hand-built array (static initializer) | 0x14015d2d0 — RegisterExeAddr standalone/reg_nullsweep.cpp:21 via ScenarioDialog_GetStandaloneVtable() | 10/10 | `onononooon` | `ui/Dialogs.h:397` 0xE8; alloc — | `fn_Time_Ctor_Float` :469; `fn_DialogInstance_Ctor` :626 | SD_Dtor:568 → fn_DialogInstance_Dtor | 0 | registered via getter (reg_nullsweep.cpp:21). dormant `UI_DIALOG_STUB` twin (:1679). |
| ToolbarDialog | — | no | `code/Game/Dialog/toolbar_vtable_standalone.cpp:105` g_ToolbarDialog_Vtable_Standalone — hand-built; slots 1,3,7,9 = Runtime virtuals by ResolveExport name | 0x14015fa00 — NOT registered | 10/10 | `obobooobob` | `ui/Dialogs.h:413` 0x108; alloc — | — | ToolbarDialog_ScalarDtor_SA:70 → fn_DialogInstance_Dtor | 0 | Table NOT RegisterExeAddr'd; published through `g_ToolbarDialog_Vtable` (toolbar_vtable_standalone.cpp:147); `engine_funcs.cpp:1329` reads 0x14015fa00 by address → 0. dormant `UI_DIALOG_STUB` twin (:1681). |
| (UI::DialogInstance itself: ToolUI child dialog) | — | no | `code/Game/Player/tool_base_toolui.cpp:118` g_ToolUI_ChildDialog_Vtable_Standalone — COPIED from minted ??_7DialogInstance@UI@@6B@ 0x180156d70 slots 1..9 | — — NOT registered | 10/? | `obbbbbbbbb` | —; alloc 0xB8 | `fn_DialogInstance_Ctor` :228 | ToolUI_Dtor:93 → fn_DialogInstance_Dtor | 0 | a bare DialogInstance with a copied table; 0x14015f5a0 deliberately unregistered (reg_player_toolui.cpp); engine_funcs.cpp:1336/:1543 read it by address. |
| TutorialHighlightDialog | "UI::DialogInstance" | yes `code/Game/Tutorial/tutorial_dialog_vtables.cpp:575` | `code/Game/Tutorial/tutorial_dialog_vtables.cpp:425` tut_highlight::g_vt — filled at init | 0x1401609e0 — RegisterExeAddr standalone/reg_tutorial_wave.cpp:89 via GetTutorialHighlightDialogVtable() | 48/10 | `ooononoonnnnnn…+34n` | `ui/Dialogs.h:448` 0xF0; alloc 0xF0 | `fn_DialogInstance_Ctor` :519; `fn_External_External` :541 | tutorial_dialog_vtables.cpp Dtor → `TutDlg_UnregisterInternal` (guarded ~Internal :126) + free — skips ~External / ~DialogInstance / ~DialogController | 0 | tree 48 vs retail 10. slot 0 skips ~DialogInstance: runs `TutDlg_UnregisterInternal` (= guarded ~Internal, tutorial_dialog_vtables.cpp:126) then frees. |

### C. UI::DialogController subclasses — 35 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| CitizenUI | "UI::DialogController" | yes `code/Game/Dialog/citizenui.cpp:849` | `code/Game/Dialog/citizenui.cpp:731` citizenui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x528 | 0x14015ba00 — NOT registered | 48/11 | `onoboboooobnnn…+34n` | `ui/Panels.h:224` 0xE8; alloc 0xE8 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:740 → none | 0 |  |
| ControlUI | "UI::DialogController" | yes `code/Game/Dialog/controlui.cpp:422` | `code/Game/Dialog/controlui.cpp:261` controlui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015b380 — RegisterExeAddr code/Game/Dialog/controlui.cpp:418 | 48/11 | `onobobbobobnnn…+34n` | `ui/Panels.h:264` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:271 → none | 0 | retail 11 (+0 terminator); its exe slot `ControlUI_Slot12`@0x140010dd0 creates ControlUIConfig (not a table slot). |
| CropUI | "SelectResourceUI" | yes `code/Game/Dialog/cropui_promote.cpp:273` | `code/Game/Dialog/cropui_promote.cpp:130` cropui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x468 | 0x14015d330 — NOT registered | 48/14 | `onobobbooobooo…+34n` | `ui/Panels.h:272` 0x60; alloc 0x60 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:140 → none | 0 | retail 14 slots at 0x14015f270 (kMasks); Ghidra labels 0x14015d330 `vtable_CropUI` but it is the abstract SelectResourceUI (stamped by Crop/Orchard/Pasture ctors and dtors). Apply() only walks slots 0..10; 11..13 own. |
| DebugUI | "DialogController" | yes `code/Game/Dialog/debugui.cpp:1757` | `code/Game/Dialog/debugui.cpp:1631` debugui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x428 | 0x14015b870 — NOT registered | 48/11 | `onobobnooobnnn…+34n` | `ui/Panels.h:287` 0x130; alloc 0x130 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:1640 → none | 0 | REGISTER_TYPE parent spelled `"DialogController"` (no `UI::`). |
| DestroyUI | "UI::DialogController" | yes `code/Game/Dialog/destroyui.cpp:669` | `code/Game/Dialog/destroyui.cpp:536` destroyui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015bb00 — NOT registered | 48/11 | `onobobbooobnnn…+34n` | `ui/Panels.h:338` 0x78; alloc 0x78 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:551 → none | 0 |  |
| EntityUI | "UI::DialogController" | yes `code/Game/Dialog/entityui.cpp:928` | `code/Game/Dialog/entityui.cpp:791` entityui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x420 | 0x14015bb90 — NOT registered | 48/11 | `ononoboooobnnn…+34n` | `ui/Panels.h:354` 0x48; alloc 0x48 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:800 → none | 0 |  |
| EventLogUI | "UI::DialogController" | yes `code/Game/Dialog/eventlogui.cpp:407` | `code/Game/Dialog/eventlogui.cpp:349` eventlogui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x518 | 0x14015bc40 — NOT registered | 48/11 | `onobbonobobnnn…+34n` | `ui/Panels.h:363` 0x80; alloc 0x80 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:358 → none | 0 |  |
| OrchardUI | "SelectResourceUI" | yes `code/Game/Dialog/orchardui.cpp:375` | `code/Game/Dialog/orchardui.cpp:270` orchardui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x468 | 0x14015cf30 — NOT registered | 48/14 | `onobobbooobooo…+34n` | `ui/Panels.h:446` 0x48; alloc 0x48 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:280 → none | 0 | parent SelectResourceUI (retail stamp chain 0x14015d330 → 0x14015cf30). |
| PastureUI | "SelectResourceUI" | yes `code/Game/Dialog/pastureui.cpp:495` | `code/Game/Dialog/pastureui.cpp:384` pastureui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x468 | 0x14015f610 — NOT registered | 48/14 | `onobobbooobooo…+34n` | `ui/Panels.h:467` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:394 → none | 0 | parent SelectResourceUI (stamp chain 0x14015d330 → 0x14015f610). |
| RadiusUI | "UI::DialogController" | yes `code/Game/Dialog/radiusui_promote.cpp:136` | `code/Game/Dialog/radiusui_promote.cpp:53` radiusui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x798 | 0x14015d160 — NOT registered | 48/11 | `onobboobbbbnnn…+34n` | `ui/Panels.h:504` 0x38; alloc 0x38 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:63 → none | 0 |  |
| ResidenceUI | "UI::DialogController" | yes `code/Game/Dialog/residenceui.cpp:657` | `code/Game/Dialog/residenceui.cpp:521` residenceui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x560 | 0x14015f6d0 — NOT registered | 48/11 | `ononobbooobnnn…+34n` | `ui/Panels.h:514` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:536 → none | 0 |  |
| StatusBarUI | "UI::DialogController" | yes `code/Game/Dialog/statusbarui.cpp:887` | `code/Game/Dialog/statusbarui.cpp:670` statusbarui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x518 | 0x14015d810 — NOT registered | 48/11 | `onoboonooobnnn…+34n` | `ui/Panels.h:565` 0x1D0; alloc 0x1D0 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:831 → none | 0 |  |
| StatusIconUI | "UI::DialogController" | yes `code/Game/Dialog/statusiconui.cpp:369` | `code/Game/Dialog/statusiconui.cpp:278` statusiconui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x768 | 0x14015d8a0 — NOT registered | 48/11 | `onobobboobbnnn…+34n` | `ui/Panels.h:615` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:287 → none | 0 |  |
| StorageCapacityUI | "UI::DialogController" | yes `code/Game/Dialog/storagecapacityui.cpp:255` | `code/Game/Dialog/storagecapacityui.cpp:141` storagecapacityui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x760 | 0x14015d950 — NOT registered | 48/11 | `ononobboobbnnn…+34n` | `ui/Panels.h:623` 0x38; alloc 0x38 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:151 → none | 0 |  |
| StorageUI | "UI::DialogController" | yes `code/Game/Dialog/storageui.cpp:715` | `code/Game/Dialog/storageui.cpp:591` storageui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x760 | 0x14015f970 — NOT registered | 48/11 | `ononobboobbnnn…+34n` | `ui/Panels.h:630` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:601 → none | 0 |  |
| TownHallUI | "UI::DialogController" | yes `code/Game/Dialog/townhallui.cpp:2504` | `code/Game/Dialog/townhallui.cpp:2149` townhallui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x68 | 0x14015dce0 — NOT registered | 48/11 | `onobobboooonnn…+34n` | `ui/Panels.h:664` 0x1E8; alloc 0x1E8 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:2399 → none | 0 |  |
| BuildRequirementUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:163` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:121` uistub_BuildRequirementUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x7e0 | 0x14015b1d0 — NOT registered | 48/11 | `ononobbbbbbnnn…+34n` | `ui/Panels.h:192` 0x60; alloc 0x60 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| HotKeyToolTipUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:212` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:178` uistub_HotKeyToolTipUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x7e0 | 0x14015bfc0 — NOT registered | 48/11 | `ononobbbbbbnnn…+34n` | `ui/Panels.h:397` 0x30; alloc 0x30 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| AttendanceUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:321` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:248` uistub_AttendanceUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x768 | 0x14015b070 — NOT registered | 48/11 | `onobobbobbbnnn…+34n` | `ui/Panels.h:180` 0x38; alloc 0x38 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| CemeteryUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:389` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:331` uistub_CemeteryUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x768 | 0x14015b240 — NOT registered | 48/11 | `onobobbobbbnnn…+34n` | `ui/Panels.h:217` 0x38; alloc 0x38 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| ConsumeProduceUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:474` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:404` uistub_ConsumeProduceUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015b2e0 — NOT registered | 48/11 | `onobobbobobnnn…+34n` | `ui/Panels.h:256` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| HelpUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:649` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:540` uistub_HelpUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x548 | 0x14015bf30 — NOT registered | 48/11 | `onobonbobobnnn…+34n` | `ui/Panels.h:380` 0x78; alloc 0x78 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| ImmigrationUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:734` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:672` uistub_ImmigrationUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015c080 — NOT registered | 48/11 | `onobobbobobnnn…+34n` | `ui/Panels.h:404` 0x50; alloc 0x50 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| MapUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:796` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:748` uistub_MapUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x5e0 | 0x14015c350 — NOT registered | 48/11 | `ononobbbbobnnn…+34n` | `ui/Panels.h:414` 0x50; alloc 0x50 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| PageToggleUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:906` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:854` uistub_PageToggleUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x560 | 0x14015cfb0 — NOT registered | 48/11 | `ononobbobobnnn…+34n` | `ui/Panels.h:456` 0x48; alloc 0x48 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| ProductionUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:987` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:933` uistub_ProductionUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x760 | 0x14015d070 — NOT registered | 48/11 | `ononobbobbbnnn…+34n` | `ui/Panels.h:476` 0x50; alloc 0x50 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| ProfessionUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:1102` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:1018` uistub_ProfessionUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x4d8 | 0x14015d0f0 — NOT registered | 48/11 | `onobbobboobnnn…+34n` | `ui/Panels.h:484` 0x78; alloc 0x78 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| ResourceLimitUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:1192` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:1132` uistub_ResourceLimitUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015d1f0 — NOT registered | 48/11 | `onobobbobobnnn…+34n` | `ui/Panels.h:522` 0x40; alloc 0x40 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| TitleDescToolTipUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:1266` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:1240` uistub_TitleDescToolTipUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x7e0 | 0x14015d9f0 — NOT registered | 48/11 | `ononobbbbbbnnn…+34n` | `ui/Panels.h:638` 0x30; alloc 0x30 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| ToolUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:1363` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:1296` uistub_ToolUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x4e0 | 0x14015da70 — NOT registered | 48/11 | `ononobbboobnnn…+34n` | `ui/Panels.h:645` 0x58; alloc 0x58 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| TradeUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:1539` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:1416` uistub_TradeUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x8 | 0x14015de80 — NOT registered | 48/11 | `onobooooooonnn…+34n` | `ui/Panels.h:749` 0x148; alloc 0x148 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| BuildUI | "UI::DialogController" | yes `code/Game/Dialog/ui_dialog_stub_factories.cpp:1649` | `code/Game/Dialog/ui_dialog_stub_factories.cpp:1566` uistub_BuildUI::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015f210 — NOT registered | 48/11 | `onobobbobobnnn…+34n` | `ui/Panels.h:207` 0x50; alloc 0x50 | factory MemAlloc+Zero+stamp — NO DialogController ctor | uistub `Dtor`: restamp + free — no ~DialogController | 0 |  |
| UpgradeUI | "UI::DialogController" | yes `code/Game/Dialog/upgradeui.cpp:327` | `code/Game/Dialog/upgradeui.cpp:209` upgradeui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015df00 — NOT registered | 48/11 | `onobobbnoobnnn…+34n` | `ui/Panels.h:805` 0x38; alloc 0x38 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:223 → none | 0 |  |
| WorkPlaceUI | "UI::DialogController" | yes `code/Game/Dialog/workplaceui.cpp:1117` | `code/Game/Dialog/workplaceui.cpp:554` workplaceui_sa::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x568 | 0x14015e030 — NOT registered | 48/11 | `onobobbooobnnn…+34n` | `ui/Panels.h:812` 0x68; alloc 0x68 | factory MemAlloc+Zero+stamp — NO DialogController ctor | Dtor:564 → none | 0 |  |
| TutorialUI | "UI::DialogController" | yes `code/Game/Tutorial/tutorial_dialog_vtables.cpp:569` | `code/Game/Tutorial/tutorial_dialog_vtables.cpp:149` tut_ui::g_vt — filled at init + Apply() (dialog_controller_vtable.cpp) fills inherited 3..10 by mask 0x560 | 0x140160f30 — RegisterExeAddr standalone/reg_tutorial_wave.cpp:84 via GetTutorialUIVtable() | 48/11 | `ooonobbobobnnn…+34n` | `ui/Panels.h:792` 0x60; alloc 0x60 | factory MemAlloc+Zero+stamp — NO DialogController ctor | tutorial_dialog_vtables.cpp Dtor → `TutDlg_UnregisterInternal` (guarded ~Internal :126) + free — skips ~External / ~DialogInstance / ~DialogController | 0 | slot 3 retail 0x1400E3AA0 not ported (Nop). |

### D. UI::DialogControllerConfig subclasses — 4 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ControlUIConfig | "UI::DialogControllerConfig" | yes `code/Game/Dialog/controluiconfig.cpp:151` | `code/Game/Dialog/controluiconfig.cpp:53` g_vt — hand-built array (static initializer) | 0x14015b3e0 — RegisterExeAddr code/Game/Dialog/controluiconfig.cpp:134 | 9/9 | `osoosooso` | —; alloc 0x28 | `fn_External_External` :68 | Dtor:94 → fn_External_Dtor | 0 |  |
| ResourceLimitUIConfig | "UI::DialogControllerConfig" | yes `code/Game/Dialog/resourcelimituiconfig.cpp:141` | `code/Game/Dialog/resourcelimituiconfig.cpp:59` g_vt — hand-built array (static initializer) | 0x14015d250 — RegisterExeAddr code/Game/Dialog/resourcelimituiconfig.cpp:124 | 9/9 | `osoosooso` | —; alloc 0x28 | `fn_External_External` :63 | Dtor:85 → fn_External_Dtor | 0 |  |
| StatusIconUIConfig | "UI::DialogControllerConfig" | yes `code/Game/Dialog/statusiconuiconfig.cpp:121` | `code/Game/Dialog/statusiconuiconfig.cpp:50` g_StatusIconUIConfig_Vtable — hand-built array (static initializer) | 0x14015d900 — NOT registered | 9/9 | `osoosooso` | —; alloc 0x28 | `fn_External_External` :53 | StatusIconUIConfig_Dtor:74 → fn_External_Dtor | 0 |  |
| UpgradeUIConfig | "UI::DialogControllerConfig" | yes `code/Game/Dialog/upgradeuiconfig.cpp:118` | `code/Game/Dialog/upgradeuiconfig.cpp:42` g_UpgradeUIConfig_Vtable — hand-built array (static initializer) | 0x14015df60 — RegisterExeAddr code/Game/Dialog/upgradeuiconfig.cpp:100 | 9/9 | `osoosooso` | —; alloc 0x28 | `fn_External_External` :45 | UpgradeUIConfig_Dtor:66 → fn_External_Dtor | 0 |  |

### E. Achievement::Goal / GoalInstance / Achievement subclasses — 8 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Scenario | "::Achievement::Achievement" | yes `code/Game/Game/scenario.cpp:291` | `code/Game/Game/scenario.cpp:255` g_Scenario_Vtable — hand-built array (static initializer) | 0x14015f020 — RegisterExeAddr code/Game/Game/scenario.cpp:272 | 13/9 | `o0oo0oo0o0000` | `Scenario.h:27` 0x90; alloc 0x90 | `s_fn_Achievement_Ctor` :197 | Scenario_Dtor:78 → QUALIFIED Achievement::Achievement::~Achievement | 3 | tree 13 vs retail 9: slots 9..12 null over-run; 1,4,7 null (retail own TypeName / Compile thunk / 0x140001640). |
| BuildingGoal | "Achievement::Goal" | yes `code/Game/Goal/buildinggoal.cpp:336` | `code/Game/Goal/buildinggoal.cpp:305` g_BuildingGoal_Vtable — hand-built array (static initializer) | 0x140160300 — RegisterExeAddr code/Game/Goal/buildinggoal.cpp:321 | 11/11 | `o0ooboooobo` | `AchievementGoals.h:20` 0x48; alloc 0x48 | `fn_Goal_BaseCtor` :170 | BuildingGoal_Dtor:58 → fn_BuildingTypeArray_Dtor | 2 | 1 null (TypeName); 4/7/9 null in the array, filled by `Goal_FillInheritedVtableSlots` (buildinggoal.cpp:320). |
| PopulationGoal | "Achievement::Goal" | yes `code/Game/Goal/populationgoal.cpp:203` | `code/Game/Goal/populationgoal.cpp:172` g_PopulationGoal_Vtable — hand-built array (static initializer) | 0x140160370 — RegisterExeAddr code/Game/Goal/populationgoal.cpp:188 | 11/11 | `o0ooboooobo` | `AchievementGoals.h:37` 0x38; alloc 0x38 | `fn_Goal_BaseCtor` :99 | PopulationGoal_Dtor:41 → QUALIFIED Achievement::Goal::~Goal | 2 | as BuildingGoal (:187). |
| StatGoal | "Achievement::Goal" | yes `code/Game/Goal/statgoal.cpp:419` | `code/Game/Goal/statgoal.cpp:384` g_StatGoal_Vtable — hand-built array (static initializer) | 0x1401604c0 — RegisterExeAddr code/Game/Goal/statgoal.cpp:404 | 11/11 | `o0ooboooobo` | `AchievementGoals.h:63` 0x80; alloc sizeof(StatGoal) | `s_fn_GoalCtor` :440 | StatGoal_Dtor:313 → QUALIFIED Achievement::Goal::~Goal | 2 | as BuildingGoal (:403); Ghidra mislabels 0x1401604c0. |
| TimeGoal | "Achievement::Goal" | yes `code/Game/Goal/timegoal.cpp:201` | `code/Game/Goal/timegoal.cpp:168` g_TimeGoal_Vtable — hand-built array (static initializer) | 0x140160540 — RegisterExeAddr code/Game/Goal/timegoal.cpp:186 | 12/11 | `o0oobooooooo` | `AchievementGoals.h:109` 0x38; alloc 0x38 | `s_fn_GoalCtor` :220 | TimeGoal_Dtor:115 → QUALIFIED Achievement::Goal::~Goal/Achievement::GoalInstance::~GoalInstance | 3 | tree 12 vs retail 11 (slot 11 = the instance dtor, retail's next table). |
| TimeGoalInstance | "Achievement::GoalInstance" | yes `code/Game/Goal/timegoalinstance.cpp:262` | `code/Game/Goal/timegoalinstance.cpp:236` g_TimeGoalInstance_Vtable — hand-built array (static initializer) | 0x140160598 — RegisterExeAddr code/Game/Goal/timegoalinstance.cpp:247 | 7/7 | `o0ooooo` | `AchievementGoals.h:158` 0x28; alloc 0x28 | `s_fn_IBPCtor` :112; `s_fn_GoalInstanceCtor` :195 | `TimeGoal_Slot11` (timegoal.cpp) → QUALIFIED Achievement::GoalInstance::~GoalInstance | 2 | slot 0 = `TimeGoal_Slot11` (timegoal.cpp) — QUALIFIED ~GoalInstance. |
| FinishTutorialGoal | "Achievement::Goal" | yes `code/Game/Tutorial/finishtutorialgoal.cpp:190` | `code/Game/Tutorial/finishtutorialgoal.cpp:159` g_FinishTutorialGoal_Vtable — hand-built array (static initializer) | 0x140160420 — RegisterExeAddr code/Game/Tutorial/finishtutorialgoal.cpp:175 | 12/11 | `o0oo0oo0oooo` | `AchievementGoals.h:129` 0x38; alloc 0x38 | `fn_Goal_BaseCtor` :103 | FinishTutorialGoal_Dtor:41 → QUALIFIED Achievement::Goal::~Goal/Achievement::GoalInstance::~GoalInstance | 3 | tree 12 vs retail 11. **Never calls `Goal_FillInheritedVtableSlots`** (finishtutorialgoal.cpp:175 registers raw) → slots 1,4,7 stay null. |
| FinishTutorialInstance | "Achievement::GoalInstance" | yes `code/Game/Tutorial/finishtutorialinstance.cpp:359` | `code/Game/Tutorial/finishtutorialinstance.cpp:331` g_FinishTutorialInstance_Vtable — hand-built array (static initializer) | 0x140160478 — RegisterExeAddr code/Game/Tutorial/finishtutorialinstance.cpp:342 | 7/7 | `o0ooooo` | `AchievementGoals.h:144` 0x28; alloc 0x38,0x28 | `fn_ObjectInternal_Ctor` :119; `s_fn_GoalInstanceCtor` :295 | `FinishTutorialGoal_Slot11` (finishtutorialgoal.cpp) → QUALIFIED Achievement::GoalInstance::~GoalInstance | 1 | slot 0 = `FinishTutorialGoal_Slot11` — QUALIFIED ~GoalInstance. |

### F. Tool family (Object::Internal -> Tool / DragTool / ToolSetting) — 9 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| BridgeTool | "DragTool" | yes `code/Game/Player/bridgetool.cpp:326` | `code/Game/Player/bridgetool.cpp:302` g_BridgeTool_Vtable — hand-built array (static initializer) | 0x140161b00 — RegisterExeAddr code/Game/Player/bridgetool.cpp:320 | 15/15 | `osooososoosoooo` | `BridgeTool.h:34` 0x40; alloc — | — | BridgeTool_Dtor:64 → s_fn_Internal_Dtor | 0 | slot 0 restamps DragTool/Tool tables read by `ResolveAddr(Exe, 0x140161c10/0x140161f10)` — both UNREGISTERED → null stores (+ a NOT IMPLEMENTED log line). |
| DragPlacementTool | "DragTool" | yes `code/Game/Player/dragplacementtool.cpp:740` | `code/Game/Player/dragplacementtool.cpp:716` g_DragPlacementTool_Vtable — hand-built array (static initializer) | 0x140161b90 — RegisterExeAddr code/Game/Player/dragplacementtool.cpp:734 | 15/15 | `osooososoosoooo` | `DragPlacementTool.h:32` 0x48; alloc 0x48 | `s_fn_ComponentEntity_CopyCtor` :645 | DragPlacementTool_Dtor:66 → s_fn_Internal_Dtor | 0 |  |
| EditTool | "Tool" | yes `code/Game/Player/edittool.cpp:1101` | `code/Game/Player/edittool.cpp:895` g_EditTool_Vtable — hand-built array (static initializer) | 0x140161ca0 — RegisterExeAddr code/Game/Player/edittool.cpp:915 | 13/14 | `osoosooossoos` | `EditTool.h:32` 0x38; alloc — | `s_fn_IBPCtor` :459; `s_fn_IBPCtor` :479 | EditTool_Dtor_Impl:79 → s_fn_Internal_Dtor | 0 | **tree 13 vs retail 14**: retail slot 13 = `EditTool_Slot13`@0x14002daf0 missing from the tree table (tool_base_standalone.h:6 claims 13 for every Tool-derived table). |
| FencedTool | "DragTool" | yes `code/Game/Player/fencedtool.cpp:1067` | `code/Game/Player/fencedtool.cpp:1029` g_FencedTool_Vtable — hand-built array (static initializer) | 0x1401605d0 — RegisterExeAddr code/Game/Player/fencedtool.cpp:1047 | 15/15 | `osoooooooosoooo` | `FencedTool.h:34` 0x70; alloc 0x120 | `s_fn_MeshInstance_Ctor` :935 | FencedTool_Dtor:77 → s_fn_Internal_Dtor | 0 |  |
| PathTool | "Tool" | yes `code/Game/Player/pathtool.cpp:494` | `code/Game/Player/pathtool.cpp:468` g_PathTool_Vtable — hand-built array (static initializer) | 0x140160680 — RegisterExeAddr code/Game/Player/pathtool.cpp:488 | 13/14 | `osoosoososooo` | `PathTool.h:24` 0x60; alloc — | — | PathTool_Dtor:114 → s_fn_Internal_Dtor | 0 | **tree 13 vs retail 14**: retail slot 13 = 0x140026b60 missing. |
| PlacementTool | "Tool" | yes `code/Game/Player/placementtool.cpp:1167` | `code/Game/Player/placementtool.cpp:1128` g_PlacementTool_Vtable — hand-built array (static initializer) | 0x140161d20 — RegisterExeAddr code/Game/Player/placementtool.cpp:1144 | 13/13 | `osooooooossoo` | `PlacementTool.h:33` 0x70; alloc 0x120 | `s_fn_IBP_Ctor` :289; `s_fn_IBP_Ctor` :344 | PlacementTool_Dtor_Impl:218 → s_fn_Internal_Dtor | 0 | 13/13. |
| RemovalTool | "DragTool" | yes `code/Game/Player/removaltool.cpp:612` | `code/Game/Player/removaltool.cpp:576` g_RemovalTool_Vtable — hand-built array (static initializer) | 0x140161de0 — RegisterExeAddr code/Game/Player/removaltool.cpp:606 | 15/15 | `osooososoosoooo` | `RemovalTool.h:33` 0x40; alloc — | — | RemovalTool_Dtor:61 → s_fn_Internal_Dtor | 0 |  |
| RoadTool | "DragTool" | yes `code/Game/Player/roadtool.cpp:636` | `code/Game/Player/roadtool.cpp:612` g_RoadTool_Vtable — hand-built array (static initializer) | 0x140161e70 — RegisterExeAddr code/Game/Player/roadtool.cpp:630 | 15/15 | `osooososoosoooo` | `RoadTool.h:24` 0x48; alloc — | — | RoadTool_Dtor:152 → s_fn_Internal_Dtor | 0 |  |
| RotationSetting | "ToolSetting" | yes `code/Game/Player/rotationsetting.cpp:58` | `code/Game/Player/rotationsetting.cpp:35` g_RotationSetting_Vtable — hand-built array (static initializer) | 0x140160710 — RegisterExeAddr code/Game/Player/rotationsetting.cpp:50 | 4/4 | `osoo` | `RotationSetting.h:16` 0x18; alloc — | — | RotationSetting_ScalarDtor:41 → fn_ObjectInternal_Dtor | 0 | parent ToolSetting (abstract, 0x140161ee8, no tree table). |

### G. Tutorial family (Object::External / Internal) — 15 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| Tutorial | "Object::External" | yes `code/Game/Tutorial/tutorial.cpp:248` | `code/Game/Tutorial/tutorial.cpp:225` g_Tutorial_Vtable — hand-built array (static initializer) | 0x140160740 — RegisterExeAddr code/Game/Tutorial/tutorial.cpp:238 | 9/9 | `o0oo0oo0o` | `Tutorial.h:17` 0x40; alloc 0x40 | `fn_External_External` :114; `fn_External_External` :145 | Tutorial_Dtor:64 → fn_External_Dtor | 0 | slots 1,4,7 null (retail: TypeName, External::Compile thunk 0x14003a912, own-dtor helper 0x140001640) — same shape in every Object::External table below. |
| TutorialGoalDialog | "TutorialGoal" | yes `code/Game/Tutorial/tutorial_dialog_vtables.cpp:571` | `code/Game/Tutorial/tutorial_dialog_vtables.cpp:237` tut_goaldlg::g_vt — filled at init | 0x140160870 — RegisterExeAddr standalone/reg_tutorial_wave.cpp:85 (+0x1401607a0 :86) via GetTutorialGoalDialogVtable() | 48/10 | `oooonooooonnnn…+34n` | `ui/Dialogs.h:433` 0x40; alloc 0x40 | `fn_DialogInstance_Ctor` :519; `fn_External_External` :541 | tutorial_dialog_vtables.cpp Dtor → `TutDlg_UnregisterInternal` (guarded ~Internal :126) + free — skips ~External / ~DialogInstance / ~DialogController | 0 | tree 48 vs retail 10. Parent TutorialGoal (Object::External). Factory = alloc+zero, **no External ctor**; slot 0 skips ~External (comment :246-252). 0x1401607a0 (abstract TutorialGoal) registered to THIS table (reg_tutorial_wave.cpp:86). |
| TutorialSetupDialog | "TutorialSetup" | yes `code/Game/Tutorial/tutorial_dialog_vtables.cpp:573` | `code/Game/Tutorial/tutorial_dialog_vtables.cpp:330` tut_setupdlg::g_vt — filled at init | 0x140160d20 — RegisterExeAddr standalone/reg_tutorial_wave.cpp:87 (+0x140160c30 :88) via GetTutorialSetupDialogVtable() | 48/12 | `oooonooooooonn…+34n` | `TutorialSetups.h:89` 0x78; `ui/Dialogs.h:464` 0x78; alloc 0x78 | `fn_DialogInstance_Ctor` :519; `fn_External_External` :541 | tutorial_dialog_vtables.cpp Dtor → `TutDlg_UnregisterInternal` (guarded ~Internal :126) + free — skips ~External / ~DialogInstance / ~DialogController | 0 | tree 48 vs retail 12. Defined twice as a POD (§1.2). Factory alloc+zero, no External ctor on the reflected path; 0x140160c30 (abstract TutorialSetup) registered to THIS table (reg_tutorial_wave.cpp:88). |
| TutorialGoalButton | "TutorialGoal" | yes `code/Game/Tutorial/tutorialgoalbutton.cpp:255` | `code/Game/Tutorial/tutorialgoalbutton.cpp:231` g_TutorialGoalButton_Vtable — hand-built array (static initializer) | 0x140160800 — RegisterExeAddr code/Game/Tutorial/tutorialgoalbutton.cpp:245 | 10/10 | `o0oo0oo0oo` | `TutorialGoals.h:43` 0x30; alloc 0x30 | `fn_External_External` :117; `fn_External_External` :200 | TutorialGoalButton_Dtor:41 → fn_External_Dtor | 0 |  |
| TutorialGoalCount | "TutorialGoal" | yes `code/Game/Tutorial/tutorialgoalcount.cpp:713` | `code/Game/Tutorial/tutorialgoalcount.cpp:689` g_TutorialGoalCount_Vtable — hand-built array (static initializer) | 0x140160950 — RegisterExeAddr code/Game/Tutorial/tutorialgoalcount.cpp:703 | 10/10 | `o0oo0oo0oo` | `TutorialGoals.h:89` 0x50; alloc 0x50 | `fn_External_External` :654 | TutorialGoalCount_Dtor:153 → fn_External_Dtor | 0 |  |
| TutorialGoalGameSpeed | "TutorialGoal" | yes `code/Game/Tutorial/tutorialgoalgamespeed.cpp:194` | `code/Game/Tutorial/tutorialgoalgamespeed.cpp:170` g_TutorialGoalGameSpeed_Vtable — hand-built array (static initializer) | 0x1401608e0 — RegisterExeAddr code/Game/Tutorial/tutorialgoalgamespeed.cpp:184 | 10/10 | `o0oo0oo0oo` | `TutorialGoals.h:66` 0x28; alloc 0x28 | `fn_External_External` :114; `fn_External_External` :140 | TutorialGoalGameSpeed_Dtor:50 → fn_External_Dtor | 0 |  |
| TutorialImage | "Object::External" | yes `code/Game/Tutorial/tutorialimage.cpp:226` | `code/Game/Tutorial/tutorialimage.cpp:202` g_TutorialImage_Vtable — hand-built array (static initializer) | 0x140160a40 — RegisterExeAddr code/Game/Tutorial/tutorialimage.cpp:216 | 10/10 | `o0oo0oo0oo` | `TutorialImages.h:17` 0x30; alloc 0x30 | `fn_External_External` :120; `fn_External_External` :147 | TutorialImage_Dtor:52 → fn_External_Dtor | 0 |  |
| TutorialImageHotKey | "TutorialImage" | yes `code/Game/Tutorial/tutorialimagehotkey.cpp:331` | `code/Game/Tutorial/tutorialimagehotkey.cpp:307` g_TutorialImageHotKey_Vtable — hand-built array (static initializer) | 0x140160ab0 — RegisterExeAddr code/Game/Tutorial/tutorialimagehotkey.cpp:321 | 10/10 | `o0oo0oo0oo` | `TutorialImages.h:37` 0x48; alloc 0x48 | `fn_External_External` :179 | TutorialImageHotKey_Dtor:72 → fn_External_Dtor | 0 |  |
| TutorialImageInputKey | "TutorialImage" | yes `code/Game/Tutorial/tutorialimageinputkey.cpp:308` | `code/Game/Tutorial/tutorialimageinputkey.cpp:284` g_TutorialImageInputKey_Vtable — hand-built array (static initializer) | 0x140160b20 — RegisterExeAddr code/Game/Tutorial/tutorialimageinputkey.cpp:298 | 10/10 | `o0oo0oo0oo` | `TutorialImages.h:57` 0x38; alloc 0x38 | `s_fn_External_Ctor` :143; `s_fn_External_Ctor` :170 | TutorialImageInputKey_Dtor:75 → fn_External_Dtor | 0 |  |
| TutorialImageToolbarIcon | "TutorialImage" | yes `code/Game/Tutorial/tutorialimagetoolbaricon.cpp:366` | `code/Game/Tutorial/tutorialimagetoolbaricon.cpp:342` g_TutorialImageToolbarIcon_Vtable — hand-built array (static initializer) | 0x140160b90 — RegisterExeAddr code/Game/Tutorial/tutorialimagetoolbaricon.cpp:356 | 10/10 | `o0oo0oo0oo` | `TutorialImages.h:80` 0x48; alloc 0x48 | `s_fn_External_Ctor` :179 | TutorialImageToolbarIcon_Dtor:84 → fn_External_Dtor | 0 |  |
| TutorialSetupCamera | "TutorialSetup" | yes `code/Game/Tutorial/tutorialsetupcamera.cpp:229` | `code/Game/Tutorial/tutorialsetupcamera.cpp:203` g_TutorialSetupCamera_Vtable — hand-built array (static initializer) | 0x140160ca0 — RegisterExeAddr code/Game/Tutorial/tutorialsetupcamera.cpp:219 | 12/12 | `o0oo0oo0oooo` | `TutorialSetups.h:21` 0x30; alloc 0x30 | `fn_External_External` :103; `fn_External_External` :168 | TutorialSetupCamera_Dtor:37 → fn_External_Dtor | 0 |  |
| TutorialSetupGameSpeed | "TutorialSetup" | yes `code/Game/Tutorial/tutorialsetupgamespeed.cpp:217` | `code/Game/Tutorial/tutorialsetupgamespeed.cpp:191` g_TutorialSetupGameSpeed_Vtable — hand-built array (static initializer) | 0x140160da0 — RegisterExeAddr code/Game/Tutorial/tutorialsetupgamespeed.cpp:207 | 12/12 | `o0oo0oo0oooo` | `TutorialSetups.h:43` 0x28; alloc 0x28 | `fn_External_External` :130; `fn_External_External` :157 | TutorialSetupGameSpeed_Dtor:70 → fn_External_Dtor | 0 |  |
| TutorialSetupRestrictPlayer | "TutorialSetup" | yes `code/Game/Tutorial/tutorialsetuprestrictplayer.cpp:436` | `code/Game/Tutorial/tutorialsetuprestrictplayer.cpp:410` g_TutorialSetupRestrictPlayer_Vtable — hand-built array (static initializer) | 0x140160e20 — RegisterExeAddr code/Game/Tutorial/tutorialsetuprestrictplayer.cpp:426 | 12/12 | `o0oo0oo0oooo` | `TutorialSetups.h:60` 0x68; alloc 0x68 | `s_fn_FilenameCtor` :200; `s_fn_FilenameCtor` :216 | TutorialSetupRestrictPlayer_Dtor:151 → fn_External_Dtor | 0 |  |
| TutorialStep | "Object::External" | yes `code/Game/Tutorial/tutorialstep.cpp:314` | `code/Game/Tutorial/tutorialstep.cpp:291` g_TutorialStep_Vtable — hand-built array (static initializer) | 0x140160e90 — RegisterExeAddr code/Game/Tutorial/tutorialstep.cpp:304 | 9/9 | `o0oo0oo0o` | `TutorialStep.h:20` 0x70; alloc 0x70 | `s_fn_External_Ctor` :147 | TutorialStep_Dtor :127 → TutorialStep_DtorBody :95 → External::~External (door) | 0 |  |
| TutorialInstance | "Object::Internal" | yes `code/Game/Tutorial/tutorialinstance.cpp:184` | `standalone/reg_w2_lane2.cpp:77` g_TutorialInstance_Vtable_SA — hand-built array (static initializer) | 0x140160bf8 — RegisterExeAddr standalone/reg_w2_lane2.cpp:92 | 4/4 | `o0oo` | `TutorialInstance.h:18` 0x38; alloc — | — | TutorialInstance_Dtor tutorialinstance.cpp:59 → Internal::~Internal (door) | 0 | table lives in standalone/reg_w2_lane2.cpp:77. |

### H. code/Game/Game Object::External subclasses — 11 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| ClimateGenerator | "Object::External" | yes `code/Game/Game/climategenerator.cpp:272` | `code/Game/Game/climategenerator.cpp:239` g_ClimateGenerator_Vtable_Standalone — hand-built array (static initializer) | 0x14015e140 — RegisterExeAddr code/Game/Game/climategenerator.cpp:254 | 11/9 | `osoosoososs` | `ClimateGenerator.h:17` 0x110; alloc sizeof(ClimateGenerator) | `fn_External_External` :156 | ClimateGenerator_Dtor_Trampoline:221 → ClimateGenerator_Dtor | 0 | tree 11 vs retail 9 (2 trailing stubs past the retail end). |
| Disease | "Object::External" | yes `code/Game/Game/disease.cpp:144` | `code/Game/Game/disease.cpp:115` g_Disease_Vtable — hand-built array (static initializer) | 0x14015e190 — RegisterExeAddr code/Game/Game/disease.cpp:128 | 9/9 | `osoososso` | —; alloc 0x48 | `fn_External_External` :57 | Disease_Dtor:93 → fn_External_Dtor | 0 | Ghidra labels 0x14015e190 `PTR_ClimateGenerator_ScalarDtor_…` (mislabel; the table after ClimateGenerator's). |
| MapGenerator | "Object::External" | yes `code/Game/Game/mapgenerator.cpp:564` | `code/Game/Game/mapgenerator.cpp:531` g_MapGenerator_Vtable_Standalone — hand-built array (static initializer) | 0x14015ed80 — RegisterExeAddr code/Game/Game/mapgenerator.cpp:546 | 11/9 | `osoosoososs` | `MapGenerator.h:51` 0x178; alloc sizeof(MapGenerator) | `fn_External_External` :110 | MapGenerator_Dtor_Trampoline:513 → MapGenerator_Dtor | 0 | tree 11 vs retail 9. |
| MenuScene | "Object::External" | yes `code/Game/Game/menuscene.cpp:750` | `code/Game/Game/menuscene.cpp:707` g_MenuScene_Vtable_Standalone — hand-built array (static initializer) | 0x14015ede0 — RegisterExeAddr code/Game/Game/menuscene.cpp:722 | 11/9 | `osooooososs` | `MenuScene.h:23` 0x208; alloc 0x230,sizeof(MenuScene) | `s_fn_External_Ctor` :110; `fn_Array_Serialize_External_GraphicsMesh` :501 | MenuScene_Dtor_Trampoline:695 → MenuScene_Dtor | 0 | tree 11 vs retail 9. |
| MusicList | "Object::External" | yes `code/Game/Game/musiclist.cpp:326` | `code/Game/Game/musiclist.cpp:301` g_MusicList_Vtable — hand-built array (static initializer) | 0x14015ee40 — RegisterExeAddr code/Game/Game/musiclist.cpp:314 | 9/9 | `o0oo0oo0o` | —; alloc — | `fn_External_External` :188 | MusicList_Dtor:255 → fn_External_Dtor | 0 |  |
| NameDatabase | "Object::External" | yes `code/Game/Game/namedatabase.cpp:205` | `code/Game/Game/namedatabase.cpp:176` g_NDB_Vtable — hand-built array (static initializer) | 0x14015eea0 — RegisterExeAddr code/Game/Game/namedatabase.cpp:189 | 9/9 | `osoooooso` | —; alloc — | `fn_External_External` :70 | NameDatabase_Dtor:136 → fn_External_Dtor | 0 |  |
| Profession | "Object::External" | yes `code/Game/Game/profession.cpp:238` | `code/Game/Game/profession.cpp:209` g_Prof_Vtable — hand-built array (static initializer) | 0x14015efa0 — RegisterExeAddr code/Game/Game/profession.cpp:222 | 9/9 | `osoosooso` | `Profession.h:18` 0x60; alloc 0x60 | `fn_External_External` :68 | Profession_Dtor:102 → fn_External_Dtor | 0 |  |
| ProfessionList | "Object::External" | yes `code/Game/Game/professionlist.cpp:306` | `code/Game/Game/professionlist.cpp:272` g_PL_Vtable — hand-built array (static initializer) | 0x14015ef50 — RegisterExeAddr code/Game/Game/professionlist.cpp:285 | 9/9 | `osoosooso` | `ProfessionList.h:18` 0x50; alloc 0x50 | `fn_External_External` :65 | ProfessionList_Dtor:94 → fn_External_Dtor | 0 |  |
| TerrainGenerator | "Object::External" | yes `code/Game/Game/terraingenerator.cpp:424` | `code/Game/Game/terraingenerator.cpp:391` g_TerrainGenerator_Vtable_Standalone — hand-built array (static initializer) | 0x14015f170 — RegisterExeAddr code/Game/Game/terraingenerator.cpp:406 | 11/9 | `osoosoososs` | `TerrainGenerator.h:16` 0x90; alloc sizeof(TerrainGenerator) | `fn_External_External` :301 | TerrainGenerator_Dtor_Trampoline:373 → TerrainGenerator_Dtor | 0 | tree 11 vs retail 9. |
| Toolbar | "Object::External" | yes `code/Game/Game/toolbar.cpp:276` | `code/Game/Game/toolbar.cpp:245` g_Toolbar_Vtable — hand-built array (static initializer) | 0x140160120 — RegisterExeAddr code/Game/Game/toolbar.cpp:258 | 9/9 | `o0oo0oo0o` | `Toolbar.h:27` 0x68; alloc 0x68 | `fn_External_External` :143; `fn_External_External` :179 | Toolbar_Dtor:42 → fn_External_Dtor | 0 |  |
| Weather | "Object::External" | yes `code/Game/Game/weather.cpp:264` | `code/Game/Game/weather.cpp:232` g_Weather_Vtable — hand-built array (static initializer) | 0x140160290 — RegisterExeAddr code/Game/Game/weather.cpp:247 | 9/9 | `oooo0oo0o` | `Weather.h:24` 0x80; alloc sizeof(Weather) | `fn_External_External` :63 | Weather_Dtor:30 → fn_External_Dtor | 0 |  |

### I. Runtime-class exe copies, Graphics subclasses, diagnostics and other standalone arrays — 12 tables

| class | parent (REGISTER_TYPE literal) | reflected? | table var + mechanism | retail vtable / registration | slots tree/retail | per-slot | POD (header:line sizeof) + alloc | ctor path (door :line) | slot-0 → base dtor | QUAL | notes |
|---|---|---|---|---|---|---|---|---|---|---|---|
| (SpriteSheet / MaterialInstance mirror) | — | no | `code/Game/Dialog/thumb_vtables.cpp:44` slots — COPIED slots 1..8 from the first constructed Runtime object (thumb_vtables.cpp Arm) | — — NOT registered | 9/? | `ooooooooo` | —; alloc — | — | own `SheetDtor`/`MatDtor` → copied base slot 0 with flags=0, then free (thumb_vtables.cpp:56-64) | 0 | thumb_vtables.cpp `Mirror g_sheet/g_mat` — exe copies 0x14015c3b0 (SpriteSheet) / 0x14015c400 (MaterialInstance), both UNREGISTERED; engine_funcs.cpp:1914-1915 read them by address → 0. |
| (UI_DIALOG_STUB macro body — 12 instances) | — | no | `code/Game/Dialog/ui_dialog_stub_factories.cpp:74` g_vt — filled at init | — — NOT registered | 48/? | `nnnnooooooonnn…+34n` | —; alloc 0x60,0x30 | — | macro `Dtor`: restamp + free — no base dtor | 0 |  |
| Pathfinder (System::Thread subclass) | — | no | `code/Game/Game/pathfinder_seed.cpp:94` g_pf_vtable — hand-built array (static initializer) | 0x14015ef18 — NOT registered | 2/2 | `on` | —; alloc — | `s_fn_ThreadCtor` :143; `s_fn_CSCtor` :145 | slot 0 = worker run-loop (not a dtor; System::Thread shape) | 0 | retail 0x14015ef18 (2 slots) NOT registered; keystone_a_gamedtor.cpp:321 reads it by address → 0. |
| (exe Graphics::Geometry subclass, serialize_arrays) | — | no | `code/Game/Game/serialize_arrays.cpp:166` g_Geometry_Vtable_Standalone — hand-built array (static initializer) | 0x14015f110 — RegisterExeAddr code/Game/Game/serialize_arrays.cpp:181 | 9/9 | `ooooooooo` | —; alloc — | `fn_Graphics_Geometry_Ctor` :83; `fn_Video_VertexBuffer_Ctor` :107 | GeomDraw_Dtor geometry_draw.cpp:62 — EMPTY body, flags ignored, never frees, no base dtor | 0 |  |
| (Graphics::Particle scratch copy) | — | no | `code/Game/Game/tornado.cpp:126` s_baseVtable — COPIED at first construction from the Runtime object's vtable, then overrides stamped | — — NOT registered | 9/? | `ooooooooo` | —; alloc 0xe0,0x90 | `fn_Graphics_Particle_Ctor` :660; `fn_Time_Ctor_Float` :685 | slot 0 = COPIED Runtime Particle/ParticleInstance slot 0 (base dtor; retail Tornado own 0x140024f00) | 2 | `s_baseVtable` — the copy source for Tornado; not a class. |
| Tornado | "Graphics::Particle" | yes `code/Game/Game/tornado.cpp:702` | `code/Game/Game/tornado.cpp:127` s_tornadoVtable — COPIED at first construction from the Runtime object's vtable, then overrides stamped | 0x140160190 — NOT registered | 10/10 | `oooooooooo` | —; alloc 0xe0,0x90 | `fn_Graphics_Particle_Ctor` :660; `fn_Time_Ctor_Float` :685 | slot 0 = COPIED Runtime Particle/ParticleInstance slot 0 (base dtor; retail Tornado own 0x140024f00) | 2 | Graphics::Particle subclass. **A fifth slot-copy site**: the table is COPIED from the Runtime object's vtable at first construction (9 slots) then overridden (tornado.cpp:100-135). Retail 0x140160190 (10) NOT registered. |
| TornadoInstance | — | no | `code/Game/Game/tornado.cpp:135` s_instVtable — COPIED at first construction from the Runtime object's vtable, then overrides stamped | 0x1401601e0 — NOT registered | 10/10 | `oooooooooo` | —; alloc 0xe0,0x90 | `fn_Graphics_Particle_Ctor` :660; `fn_Time_Ctor_Float` :685 | slot 0 = COPIED Runtime Particle/ParticleInstance slot 0 (base dtor; retail Tornado own 0x140024f00) | 2 | Graphics::ParticleInstance subclass; copied 10 slots + override slot 1 (tornado.cpp). Retail 0x1401601e0 (10) NOT registered. |
| WeatherParticle (exe Graphics::Geometry subclass) | — | no | `code/Game/Game/weather_particles.cpp:447` g_WeatherParticle_Vtable — hand-built array (static initializer) | 0x140160240 — RegisterExeAddr code/Game/Game/weather_particles.cpp:460 | 9/9 | `ooooooooo` | —; alloc 0xf0 | `fn_Graphics_Geometry_Ctor` :135 | WeatherParticle_GeoDtor:153 → s_fnGeoBaseDtor | 0 |  |
| (exe Video::ImageBuffer copy) | — | no | `standalone/address_registry.cpp:1710` g_NoiseUI_ImageBufferVtable_Standalone — hand-built array (static initializer) | 0x14015c8c0 — RegisterExeAddr standalone/address_registry.cpp:1752 | 10/10 | `osssssssso` | —; alloc — | — | slot 0 = `??1ImageBuffer@Video@@UEAA@XZ` (complete dtor in a deleting slot) | 0 | retail 0x14015c8c0 is 10 slots (Ghidra splits slot 9 `Create` into 0x14015c908). Tree slot 0 = `??1ImageBuffer@Video@@UEAA@XZ` — a COMPLETE dtor in a scalar-deleting slot (flags ignored, never frees); slots 1..8 ReturnNull. |
| (diagnostic EnumStub, not an exe class) | — | no | `standalone/factory_enum_stub.cpp:67` g_EnumStub_Vtable — hand-built array (static initializer) | — — NOT registered | 24/? | `o0oonnnnnnnnnn…+10n` | —; alloc 0x40 | — | EnumStub_Dtor:49 → none | 0 | BANISHED_FACTORY_ENUM diagnostic object. |
| TornadoComponent (0A scope; file in standalone/) | — | no | `standalone/reg_tornado.cpp:45` g_TornadoComponent_Vtable — hand-built array (static initializer) | 0x14015aa80 — RegisterExeAddr standalone/reg_tornado.cpp:63 | 16/16 | `nnnoonnnonnnnnnn` | `TornadoComponent.h:50` 0x68; alloc — | — | TornadoComponent_BaseNoOp:38 → none | 0 | component — belongs to 0A's inventory; listed because the array is under standalone/. |
| ControllerComponent (abstract, 0x14015f070) | — | no | `standalone/reg_w2_lane1.cpp:67` g_PurecallVtable — hand-built array (static initializer) | 0x14015f070 — RegisterExeAddr standalone/reg_w2_lane1.cpp:82 | 9/9 | `nnnnnnnnn` | `ControllerComponent.h:32` 0x48; alloc — | — | W2L1_Purecall:58 → none | 0 | the secondary-vptr base of the 0A component MI types (retail component ctors stamp it at [RAX] = this+off). 9 purecall slots. |

### J. Exe classes with NO tree table (need a class but have no hand-built array to retire)

| class | retail | parent | reflected | tree today |
|---|---|---|---|---|
| Tool | 0x140161f10 (13; 4-9 pure) | Object::Internal | `REGISTER_ABSTRACT_FACTORY(Tool, "Object::Internal")` factory_registrations_bulk.cpp:187 | POD `engine/types/Tool.h:26` 0x28; 9 unregistered by-address reads (§0.3) |
| DragTool | 0x140161c10 (15; 4,5,7,8,13 pure) | Tool | abstract factory :186 | POD `Tool.h:38` 0x38; `code/Game/Player/dragtool.cpp` holds its member bodies as free functions |
| ToolSetting | 0x140161ee8 (4) | Object::Internal | abstract factory :188 | none |
| TutorialGoal | 0x1401607a0 (9; 6-8 pure) | Object::External | abstract factory :142 | `code/Game/Tutorial/tutorialgoal.cpp` free functions; address registered to TutorialGoalDialog's table |
| TutorialSetup | 0x140160c30 (11; 6-10 pure) | Object::External | abstract factory :156 | `tutorialsetup.cpp`; address registered to TutorialSetupDialog's table |
| SelectResourceUI | 0x14015d330 (14; 11-13 pure) | UI::DialogController | abstract factory :94 | `selectresourceui.cpp` free functions; kMasks row `dialog_controller_vtable.cpp:98` |
| exe AchievementInstance / MeshInstance / FrameCopyImageBuffer copies, 0x14015e290, 0x14015ecb0 | §0.3 | Runtime classes | no | exe-emitted copies of Runtime tables (own scalar dtor + import thunks); the tree keeps the Runtime table |

---------------------------------------------------------------------------------------------------
## 1. Known defects / oddities, re-verified (and the new ones)

**1.1 `Dialogs.h:23` and `Panels.h:22` both claim 0x14015b070 — RESOLVED: it is AttendanceUI's table.**
`engine/types/ui/Dialogs.h:23` `AchievementDialogAddr::SecondaryVtable = 0x14015b070` and `engine/types/ui/Panels.h:22`
`AttendanceUIAddr::Vtable = 0x14015b070`. Retail stamps 0x14015b070 only at offset 0 of an AttendanceUI:
`Factory_AttendanceUI` 0x1400104b0 ([RBX] @0x1400104eb), `Cleanup_AttendanceUI` 0x140010510, `AttendanceUI_Ctor`
0x14005de50, `AttendanceUI_CopyCtor` 0x14005de80, and 0x140010560 — which Ghidra names `AchievementDialog_Slot10` but
which is slot 0 of 0x14015b070 (AttendanceUI's scalar dtor; the "Slot10/11/12" names are the adjacent-table
mislabel). AchievementDialog's own stamps (0x14015b020) are all at [base]: Factory 0x140010320, Cleanup, ScalarDtor,
CopyCtor, Ctor3, Ctor4, `AchievementDialog_Ctor` 0x1400b34c0 ([RDI] @0x1400b35bc). `SecondaryVtable` has **zero
readers** (`grep -rn SecondaryVtable` → only the two Dialogs.h lines plus two component headers). Dialogs.h:23 is a
wrong label; delete or correct it in the step, nothing depends on it.

**1.2 `::TutorialSetupDialog` defined twice — confirmed, benign layout, live ODR violation.** Lines moved:
`engine/types/ui/Dialogs.h:464` and `engine/types/TutorialSetups.h:89`. Both global namespace, both
`static_assert(sizeof == 0x78)`, identical offsets (+0x00 vtable, +0x08 External blob 0x18, +0x20..+0x70 the same
fields) but different member NAMES and types (`uint64_t data20` vs `void* _field20`, `externalBase` vs `_extBase`).
Include sets are disjoint (transitive closure, `sharedhdr.py`): Dialogs.h reaches 27 .cpp files (incl.
`tutorialsetupdialog.cpp`, `tutorial_dialog_vtables.cpp`), TutorialSetups.h reaches 4 (`tutorialsetup.cpp`,
`tutorialsetupcamera.cpp`, `tutorialsetupgamespeed.cpp`, `tutorialsetuprestrictplayer.cpp`) — no TU sees both, so it
compiles; it is an ODR violation across TUs. When TutorialSetupDialog becomes a class, one POD must go.

**1.3 Goal slot 1 null — confirmed, and it is a FAMILY pattern.** Slot 1 (`TypeName`; retail has an own body, e.g.
BuildingGoal_Slot1) is a literal `nullptr` in all 5 Goal tables, both GoalInstance tables, Scenario, TutorialInstance,
and **15 Object::External tables that also leave slots 4 and 7 null**: MusicList, Toolbar, Tutorial, TutorialStep,
TutorialGoalButton/Count/GameSpeed, TutorialImage/HotKey/InputKey/ToolbarIcon, TutorialSetupCamera/GameSpeed/
RestrictPlayer (Weather leaves 4 and 7). Retail holds there: slot 4 the `Object::External::Compile` import thunk
0x14003a912, slot 7 the exe helper 0x140001640 (`mov rax,[rcx]; xor edx,edx; jmp [rax]` — call own slot 0 with
flags 0). Only the four Goal files repair 4/7/9 at registration (`Goal_FillInheritedVtableSlots`,
goal_exe_helpers.cpp:210-235, called buildinggoal.cpp:320, populationgoal.cpp:187, statgoal.cpp:403, timegoal.cpp:185).
A converted class inherits Compile and the TypeName body, so this set becomes correct by construction — but the
"call own dtor with flags 0" helper (slot 7) has no GCC equivalent (§1.13).

**1.4 FinishTutorialGoal slots 1/4/7 null — confirmed.** `finishtutorialgoal.cpp:175` registers the raw
`g_FinishTutorialGoal_Vtable` (:159) and never calls `Goal_FillInheritedVtableSlots`.

**1.5 Scenario 13 vs retail 9 — confirmed.** `g_Scenario_Vtable[13]` scenario.cpp:255 (was :277), registered :272;
retail 0x14015f020 has 9 slots + a 0 terminator (my dump reads 10 including the terminator). Slots 9-12 null
over-run; 1, 4, 7 null (1.3).

**1.6 Slot-0 bodies that skip their base dtor — five confirmed, 55 tables in all.** The five named ones: MenuDLT
`Stub_Destroy` menudialogloopt_stubs.cpp:242, splash `Stub_Destroy` splash_loop_stubs.cpp:132, `Inner_Destroy`
loader_loop_stubs.cpp:127, `Outer_Destroy` :250, TutorialHighlightDialog `Dtor` tutorial_dialog_vtables.cpp:434.
The complete list in scope (main-table slot-0 column): those 5 + the 35 hand-built DialogController tables (1.8;
TutorialUI's `Dtor` :158 is one of them) + TutorialGoalDialog's and TutorialSetupDialog's `Dtor` (:254, :339 —
`TutDlg_UnregisterInternal` + free, i.e. guarded ~Internal instead of ~External) + the 12 `UI_DIALOG_STUB` instances (restamp + free) +
`GeomDraw_Dtor` geometry_draw.cpp:62 (EMPTY — flags ignored, never frees; retail 0x14001ecc0 walks the SoA arrays).
Opposite oddity: the ImageBuffer copy (address_registry.cpp:1710) puts the COMPLETE dtor `??1ImageBuffer@Video@@UEAA@XZ`
in slot 0 — a deleting-dtor call through it never frees. GameUpdateLoop's slot 0 chains (`fn_UpdateLoop_Dtor`
gameupdateloop.cpp:2726); the Goal family, Scenario and both GoalInstance slot 0s already chain through QUALIFIED
`->Achievement::…::~…()` member calls (B.3.7).

**1.7 TutorialHighlightDialog.** Tree `tut_highlight::g_vt[48]` (tutorial_dialog_vtables.cpp:425, fill :465-470) vs
retail 0x1401609e0 **10** slots. Retail slots 3/5/8/9 are Runtime import thunks (DialogInstance Persist / OnClosed /
OnElementEvent / OnSerializeSettings); the tree has `Nop` there — four inherited behaviours dropped. Two construction
paths: the REGISTER_TYPE factory (:575) is alloc+zero+stamp with **no** DialogInstance ctor; the hand path
`TutWave_HighlightDialogCtor` runs `fn_DialogInstance_Ctor` (:519). Slot 0 skips ~DialogInstance (1.6). The same
"factory skips the base ctor" holds for TutorialUI (0x60), TutorialGoalDialog (0x40, no External ctor) and
TutorialSetupDialog (0x78; only `TutWave_TutorialSetupDialogCtor` runs `fn_External_External` :541). Retail
TutorialGoalDialog / TutorialSetupDialog slot 4 is the External::Compile thunk; the tree has `Nop`.

**1.8 No DialogController subclass runs ~DialogController — confirmed.** `fn_DialogController_Dtor(` has zero call
sites in code/, standalone/, engine/ (grep). All 38 DC tables (35 hand-built + the ModBrowseUI/NoiseUI/SpriteCaptureUI `UI_DIALOG_STUB`s) restamp and free in
slot 0; no DC factory runs a DialogController ctor. After conversion the compiler runs ~DialogController (→
~Internal: factory-list unregister) for all 38 — a behaviour change to decide before the block (B36 §3A already flagged it).

**1.9 NEW — AutoSaveDialog is REGISTER_TYPE'd twice.** `autosavedialog.cpp:283` (the real table `autosave_sa::g_vt`)
and `UI_DIALOG_STUB(AutoSaveDialog, …)` ui_dialog_stub_factories.cpp:1672 (48-nop stub). It is the only duplicate
type name among 311 registrations repo-wide (sweep over REGISTER_TYPE / _FULL / UI_DIALOG_STUB / REGISTER_*_FACTORY).
Which one the Runtime factory keeps depends on static-init order across TUs. The other 8 `UI_DIALOG_STUB`
DialogInstance twins (AchievementDialog, FollowDialog, GoalDialog, MapParamsDialog, ScenarioDialog, SaveLoadDialog,
ToolbarDialog, ModUploadDialog) are the ONLY registration for their name — so the reflected factory for those
names builds the dormant 48-nop object while the game's hand path builds the real table.

**1.10 Multiple inheritance / secondary vptr — none in 0B's classes.** Method: every retail `LEA reg,[table]` →
`MOV qword [x], reg` within 4 instructions, 1,464 pairs (`exe_stamps.tsv`). For the 0B address ranges every store
is to `[base]` (RCX/RBX/RDI/RSI/R14 = the object) — no `[RAX]`-after-`LEA RAX,[this+off]` store, no non-zero
displacement except three stack-argument stores (`[RSP+0x20/0x28]` in 0x14006ca80 and 0x1400992f0). The multi-table
functions are single-inheritance chains (base table then derived): MessageDialog→OptionInputBindDialog,
SelectResourceUI→Crop/Orchard/Pasture, TutorialGoal→4, TutorialImage→3, TutorialSetup→4, Tool→DragTool→5,
Tool→Edit/Path/Placement, ToolSetting→RotationSetting. The detector does see the component MI shape
(`OrchardComponent_Ctor` 0x140051f40 stores 0x14015f070 and 0x14015a130 at [RAX] after its primary at [RBX]) —
those are agent 0A's; `g_PurecallVtable` (standalone/reg_w2_lane1.cpp:67 → 0x14015f070, the `ControllerComponent`
base table) is the one array under standalone/ that belongs to that family. The "AchievementDialog / ModDialog" hint
comes from two wrong header labels (1.1; `Dialogs.h:81` ModDialog `SecondaryVtable = 0x14015c8c0` is a separately
allocated Video::ImageBuffer, stamped at offset 0 of a NEW object in ModDialog_Serialize 0x14006f150 @0x14006faa4,
NoiseUI_Serialize 0x1400714c0, 0x1400ac780).

**1.11 By-address readers of UNREGISTERED tables get 0 today** (`ResolveAddr(Module::Exe, x)` returns 0 and logs
`NOT IMPLEMENTED`, resolve_standalone.cpp:423-431): Tool 0x140161f10 ×9 and DragTool 0x140161c10 ×6 (every tool dtor
restamps null), FollowDialog 0x14015be00 (engine_funcs.cpp:1494), ToolbarDialog 0x14015fa00 (:1329),
0x14015f980 (:1342), ToolUI child 0x14015f5a0 (:1336, :1543), SpriteSheet/MaterialInstance copies 0x14015c3b0/c400
(:1915/:1914), ModUploadDialog 0x14015c6c0 (:1249), SkyMesh MeshInstance 0x14015e240 (fencedtool.cpp:799,
placementtool.cpp:910), AchievementInstance 0x14015e350 (gameupdateloop_load.cpp:1621), Pathfinder 0x14015ef18
(keystone_a_gamedtor.cpp:321). Full reader list §4.

**1.12 Table length vs retail (every mismatch).** Longer than retail: the 35 DialogController tables and the 4
tutorial dialog tables (48 vs 10-14 — padding, all `n`); MainMenuDialog and MapParamsDialog 20 vs 10;
ClimateGenerator / MapGenerator / MenuScene / TerrainGenerator 11 vs 9; Scenario 13 vs 9; TimeGoal and
FinishTutorialGoal 12 vs 11; FrustumOutlineElement 24 vs 12 (copy loop reads past the Runtime table);
Capture/FrustumOutlineDescription 11 vs 10. **Shorter than retail: EditTool and PathTool 13 vs 14** — retail slot 13
(`EditTool_Slot13` 0x14002daf0, PathTool 0x140026b60) is missing, contradicting `tool_base_standalone.h:6` ("13
slots"); a dispatch of slot 13 on either reads the next global. Equal: everything else.

**1.13 Tables filled at init / copied from a Runtime object (every site, for the retirement order):**
(a) `tool_base_toolui.cpp` `ToolUI_InitChildDialogVtable` — slots 1..9 copied from `??_7DialogInstance@UI@@6B@`
0x180156d70 by ADDRESS; (b) `frustumoutlineelement.cpp:226-236` — 24 slots through the constructed object;
(c) `thumb_vtables.cpp:43-96` `Mirror g_sheet/g_mat` — slots 1..8 through the object, slot 0 own deleting dtor that
calls the copied slot 0 with flags 0; (d) `dialog_controller_vtable.cpp` `Apply()` — 8 base bodies (slot 3 by name,
4-10 by DLL address 0x1800cd2b0..250) into 38 registered tables by mask (only slots 0..10; Crop/Orchard/Pasture 11-13
are own); **(e) NEW fifth copy site: `tornado.cpp:100-135`** — `s_tornadoVtable`/`s_instVtable` copied at first
construction from the Runtime Graphics::Particle / ParticleInstance object (9 and 10 slots) then overridden; (f)
by-NAME fills (not copies): `Goal_FillInheritedVtableSlots` (Compile, GetInstanceType), `mod_dialog_vtables.cpp`
`FillBaseSlots` (6 DialogInstance virtuals), `toolbar_vtable_standalone.cpp:130-135` (4), `address_registry.cpp:1728-1753`
ImageBuffer dtor/Create. Hand-inlined "call own slot 0 with flags=0" (the scalar-deleting contract GCC does not
have): `Goal_Slot7_CallOwnDtor` goal_exe_helpers.cpp:203, statgoal.cpp:158, timegoal.cpp:57, scenario.cpp (Cleanup),
thumb_vtables.cpp:56-64, tornado.cpp slot 6/7 (per its banner), menuscene.cpp:200-205 (flags=1 on a child).

**1.14 Abstract exe bases registered onto concrete tables.** 0x1401607a0 (TutorialGoal) → TutorialGoalDialog's
table and 0x140160c30 (TutorialSetup) → TutorialSetupDialog's (reg_tutorial_wave.cpp:78-88 explains: the dialog
dtors store the base vtable transiently). But 10 other tutorial files read those two addresses to restamp THEIR
objects during destruction (tutorialgoalbutton.cpp:52/:121/:204, tutorialgoalcount.cpp:167/:658,
tutorialgoalgamespeed.cpp:58/:118/:144, tutorialsetupcamera.cpp:45/:107/:172, tutorialsetupgamespeed.cpp:78/:134/:161,
tutorialsetuprestrictplayer.cpp:163/:368, tutorialgoal.cpp:34, tutorialsetup.cpp:34) — so a TutorialGoalButton being
destroyed transiently carries TutorialGoalDialog's table. Harmless while nothing dispatches mid-dtor; after conversion
the abstract class's own compiler table replaces both.

---------------------------------------------------------------------------------------------------
## 2. The loop PODs and their `m_` fields

The exe-side loop PODs are the GLOBAL structs `::UpdateLoop` (engine/types/UpdateLoop.h:53, 0x28), `::LoaderLoop`
(engine/types/LoaderLoop.h:22, 0x48), `::DialogLoaderLoopT` (engine/types/DialogLoaderLoopT.h:27, 0x188) — each
banner says it "becomes `using X = RtEngine::X;`" (B7-6). Their `RtEngine::` twins are the POD mirrors in
`runtime/types/engine/UpdateLoop.h:83-238` (`RtEngine::UpdateLoop`, `Loader`, `LoaderLoop`, `DialogLoopT`,
`DialogLoaderLoopT`, `_`-prefixed fields). There is no engine/types DialogLoopT POD (MenuDLT's struct is opaque:
0xC00 blob built by `MenuDialogLoopT_Ctor` engine_funcs.h:2330). `GameUpdateLoop` (engine/types/GameUpdateLoop.h:39,
0x160) is the exe class's own POD and repeats the base fields by name.

Measured (`mfields.py`): a use = `->m_X` / `.m_X` with X a field name declared in the three loop PODs.

| file | uses | fields | struct the object is typed as |
|---|---|---|---|
| code/Game/Game/gameupdateloop.cpp | 44 | m_userInterface 33, m_resourceList 8, m_nextLoop 3 | **`GameUpdateLoop*`** — these are GameUpdateLoop POD fields that share names (GameUpdateLoop.h: m_userInterface +0x98, m_nextLoop +0xC8, m_resourceList +0x08); none is a `::UpdateLoop`/RtEngine field use |
| standalone/splash_loop_stubs.cpp | 28 | m_nextLoop 9, m_dialogExternal 7, m_elapsedTicks 6, m_dialogInstance 3, m_delayTicks 3 | `::DialogLoaderLoopT*` (:154, :264, :546) |
| standalone/loader_loop_stubs.cpp | 10 | m_nextLoop 6, m_loadBit 4 | `::LoaderLoop*` (:138, :168, :259, :285) |
| code/Game/Game/dialogloaderloop.cpp | 2 | m_elapsedTicks 2 | `::DialogLoaderLoopT*` |
| **total** | **84 in 4 files** | | adding `::MainLoop` field names gives 87 in 5 (dialogloaderloop.cpp +2, menudialogloopt.cpp `m_engine` +1) |

NEXT_SESSION's "95 in 7 files" is **not reproduced**: my measure gives 84/4 (loop-POD names) or 87/5 (+MainLoop
names); I could not recover the 7-file set. The rule-sheet owner should re-measure with the grep in §5 rather than
trust either number. Only 40 of the 84 (splash 28 + loader 10 + dialogloaderloop 2) are real RtEngine-twin field uses.

---------------------------------------------------------------------------------------------------
## 3. Shared POD headers (engine/types headers holding more than one top-level struct)

Includers are .cpp files that include the header directly or transitively (`sharedhdr.py`). NEXT_SESSION's
"Dialogs.h 20 classes, Panels.h 38": measured Dialogs.h **22** structs (20 DialogInstance + TutorialGoalDialog and
TutorialSetupDialog, which are Object::External), Panels.h **38** (SelectResourceUI has no struct — the kMasks list
has 39).

| header | structs | structs (sizeof, line) | includer .cpp | includers |
|---|---|---|---|---|
| `AchievementGoals.h` | 7 | `BuildingGoal` 0x48 :20, `PopulationGoal` 0x38 :37, `StatGoal` 0x80 :63, `TimeGoal` 0x38 :109, `FinishTutorialGoal` 0x38 :129, `FinishTutorialInstance` 0x28 :144, `TimeGoalInstance` 0x28 :158 | 7 | Goal/buildinggoal.cpp, Goal/populationgoal.cpp, Goal/statgoal.cpp, Goal/timegoal.cpp, Goal/timegoalinstance.cpp, Tutorial/finishtutorialgoal.cpp, Tutorial/finishtutorialinstance.cpp |
| `AutoPickupWork.h` | 2 | `Work` 0x48 :26, `AutoPickupWork` 0x48 :46 | 2 | Work/autopickupwork.cpp, Work/work_base_standalone.cpp |
| `BuildActions.h` | 8 | `BuildRoad` 0x70 :39, `BuildStructure` 0x70 :98, `ClearArea` 0x78 :156, `ConsumeProduceAction` 0x78 :205, `GatherRawMaterials` 0x78 :253, `PlantArea` 0x70 :304, `PlantOrchard` 0x70 :370, `Pickup` 0x60 :419 | 9 | Work/buildroad.cpp, Work/buildstructure.cpp, Work/cleararea.cpp, Work/consumeproduce.cpp, Work/gatherrawmaterials.cpp, Work/gatherrawmaterials_ctor.cpp, Work/plantarea.cpp, Work/plantorchard.cpp, Work/plantorchard_ctor.cpp |
| `CitizenBehavior.h` | 6 | `ProfessionResourceEntry` 0x20 :38, `ResourceNotifyEntry` 0x10 :62, `PopulationSummary` 0x18 :73, `PathGraph` 0x28 :88, `CitizenBehavior` :103, `CitizenBehaviorRaw` 0x1C0 :179 | 1 | Entity/citizenbehavior.cpp |
| `DiseaseComponent.h` | 2 | `DiseaseComponent` 0x58 :53, `DiseaseDescription` 0x48 :90 | 2 | Component/diseasecomponent.cpp, standalone/disaster_harness.cpp |
| `EntityList.h` | 4 | `IBPArray` 0x10 :26, `EntityTreeNode` 0x28 :38, `UpdateBucket` 0x10 :56, `EntityList` 0xE0 :65 | 4 | Entity/entitylist.cpp, Game/gameupdateloop.cpp, standalone/reg_code_lane5.cpp, standalone/reg_entity.cpp |
| `MapGenerator.h` | 4 | `ExternalRef` 0x18 :24, `DynArrayHeader` 0x10 :34, `BoolIntPair` 8 :43, `MapGenerator` 0x178 :51 | 1 | Game/mapgenerator.cpp |
| `Modification.h` | 2 | `Mod` 0x80 :36, `Modification` 0x28 :85 | 0 |  |
| `Tool.h` | 2 | `Tool` 0x28 :26, `DragTool` 0x38 :38 | 12 | Player/bridgetool.cpp, Player/dragplacementtool.cpp, Player/dragtool.cpp, Player/edittool.cpp, Player/fencedtool.cpp, Player/fencedtool_calcpath.cpp, Player/pathtool.cpp, Player/placementtool.cpp, Player/removaltool.cpp, Player/roadtool.cpp, Player/roadtool_calcpath.cpp, standalone/reg_code_lane6.cpp |
| `TornadoComponent.h` | 2 | `TornadoComponent` 0x68 :50, `TornadoDescriptionFields` :96 | 2 | Component/tornadocomponent.cpp, standalone/reg_tornado.cpp |
| `TutorialGoals.h` | 3 | `TutorialGoalButton` 0x30 :43, `TutorialGoalGameSpeed` 0x28 :66, `TutorialGoalCount` 0x50 :89 | 4 | Tutorial/tutorialgoal.cpp, Tutorial/tutorialgoalbutton.cpp, Tutorial/tutorialgoalcount.cpp, Tutorial/tutorialgoalgamespeed.cpp |
| `TutorialImages.h` | 4 | `TutorialImage` 0x30 :17, `TutorialImageHotKey` 0x48 :37, `TutorialImageInputKey` 0x38 :57, `TutorialImageToolbarIcon` 0x48 :80 | 4 | Tutorial/tutorialimage.cpp, Tutorial/tutorialimagehotkey.cpp, Tutorial/tutorialimageinputkey.cpp, Tutorial/tutorialimagetoolbaricon.cpp |
| `TutorialSetups.h` | 4 | `TutorialSetupCamera` 0x30 :21, `TutorialSetupGameSpeed` 0x28 :43, `TutorialSetupRestrictPlayer` 0x68 :60, `TutorialSetupDialog` 0x78 :89 | 4 | Tutorial/tutorialsetup.cpp, Tutorial/tutorialsetupcamera.cpp, Tutorial/tutorialsetupgamespeed.cpp, Tutorial/tutorialsetuprestrictplayer.cpp |
| `UpdateLoop.h` | 2 | `UpdateLoopVtable` 0x60 :27, `UpdateLoop` 0x28 :53 | 7 | Game/dialogloaderloop.cpp, Game/game_main.cpp, Game/gameupdateloop.cpp, Game/gameupdateloop_load.cpp, standalone/loader_loop_stubs.cpp, standalone/menudialogloopt_stubs.cpp, standalone/splash_loop_stubs.cpp |
| `common.h` | 2 | `Time` 8 :8, `PostCallback` 0x10 :40 | 564 | Action/action_base_cnr.cpp, Action/action_base_complete.cpp, Action/action_base_hunt.cpp, Action/action_base_move.cpp, Action/action_base_movedeps.cpp, Action/action_base_rawmat.cpp, Action/action_base_standalone.cpp, Action/action_child_ctors.cpp, Action/action_child_ctors2.cpp, Action/action_pf_astar.cpp, Action/action_pf_avl.cpp, Action/action_pf_ctors.cpp, Action/action_pf_geom.cpp, Action/action_pf_pools.cpp, Action/action_pf_region.cpp, Action/action_pf_road.cpp, Action/action_pf_roots_drop.cpp, Action/action_pf_roots_misc.cpp, Action/action_pf_roots_road.cpp, Action/action_pf_roots_route.cpp, Action/animatenaturalresource.cpp, Action/completeroad.cpp, Action/completetask.cpp, Action/consumerawmaterial.cpp, Action/createnaturalresource.cpp, Action/createrawmaterial.cpp, Action/dropoffrawmaterial.cpp, Action/huntanimal.cpp, Action/keystone_a_zone.cpp, Action/moveto.cpp, � (+534) |
| `io_serializer.h` | 4 | `Encoding` :42, `Stream` :84, `MemoryStream` :113, `Serializer` 0x10 :156 | 0 |  |
| `ui/Dialogs.h` | 22 | `AchievementDialog` 0xD0 :141, `AutoSaveDialog` 0xD0 :156, `EventNotifyDialog` 0xF0 :167, `EventPopupDialog` 0xC0 :184, `FollowDialog` 0xD8 :192, `GoalDialog` 0xC8 :204, `LoaderDialog` 0xC8 :214, `MainMenuDialog` 0xC8 :225, `MapParamsDialog` 0xF8 :234, `MessageDialog` 0xC0 :250, `ModDetailsDialog` 0xC8 :258, `ModDialog` 0xD8 :267, `ModUploadDialog` 0xB8 :279, `NewGameDialog` 0xAE8 :286, `OptionsDialog` 0x1C0 :314, `OptionInputBindDialog` 0xC8 :361, `SaveLoadDialog` 0x160 :373, `ScenarioDialog` 0xE8 :397, `ToolbarDialog` 0x108 :413, `TutorialGoalDialog` 0x40 :433, `TutorialHighlightDialog` 0xF0 :448, `TutorialSetupDialog` 0x78 :464 | 27 | Dialog/achievementdialog.cpp, Dialog/autosavedialog.cpp, Dialog/eventnotifydialog.cpp, Dialog/eventpopupdialog.cpp, Dialog/followdialog.cpp, Dialog/goaldialog.cpp, Dialog/loaderdialog.cpp, Dialog/mainmenudialog.cpp, Dialog/mapparamsdialog.cpp, Dialog/menu_dialog_standalone_vtables.cpp, Dialog/messagedialog.cpp, Dialog/mod_dialog_vtables.cpp, Dialog/moddetailsdialog.cpp, Dialog/moddialog.cpp, Dialog/newgamedialog.cpp, Dialog/optioninputbinddialog.cpp, Dialog/optionsdialog.cpp, Dialog/saveloaddialog.cpp, Dialog/scenariodialog.cpp, Dialog/toolbar_vtable_standalone.cpp, Dialog/toolbardialog.cpp, Dialog/unreg_lane2_mapparamsdialog_ctor.cpp, Tutorial/tutorial_dialog_vtables.cpp, Tutorial/tutorialhighlightdialog.cpp, Tutorial/tutorialsetupdialog.cpp, standalone/modal_driver.cpp, standalone/reg_autosave.cpp |
| `ui/Panels.h` | 38 | `AttendanceUI` 0x38 :180, `BuildRequirementUI` 0x60 :192, `BuildUI` 0x50 :207, `CemeteryUI` 0x38 :217, `CitizenUI` 0xE8 :224, `ConsumeProduceUI` 0x40 :256, `ControlUI` 0x40 :264, `CropUI` 0x60 :272, `DebugUI` 0x130 :287, `DestroyUI` 0x78 :338, `EntityUI` 0x48 :354, `EventLogUI` 0x80 :363, `HelpUI` 0x78 :380, `HotKeyToolTipUI` 0x30 :397, `ImmigrationUI` 0x50 :404, `MapUI` 0x50 :414, `ModBrowseUI` 0x30 :424, `NoiseUI` 0x70 :431, `OrchardUI` 0x48 :446, `PageToggleUI` 0x48 :456, `PastureUI` 0x40 :467, `ProductionUI` 0x50 :476, `ProfessionUI` 0x78 :484, `RadiusUI` 0x38 :504, `ResidenceUI` 0x40 :514, `ResourceLimitUI` 0x40 :522, `SpriteCaptureUI` 0x70 :530, `StatusBarUI` 0x1D0 :565, `StatusIconUI` 0x40 :615, `StorageCapacityUI` 0x38 :623, `StorageUI` 0x40 :630, `TitleDescToolTipUI` 0x30 :638, `ToolUI` 0x58 :645, `TownHallUI` 0x1E8 :664, `TradeUI` 0x148 :749, `TutorialUI` 0x60 :792, `UpgradeUI` 0x38 :805, `WorkPlaceUI` 0x68 :812 | 44 | Dialog/attendanceui.cpp, Dialog/buildrequirementui.cpp, Dialog/buildui.cpp, Dialog/cemeteryui.cpp, Dialog/citizenui.cpp, Dialog/consumeproduceui.cpp, Dialog/controlui.cpp, Dialog/cropui.cpp, Dialog/cropui_promote.cpp, Dialog/debugui.cpp, Dialog/destroyui.cpp, Dialog/entityui.cpp, Dialog/eventlogui.cpp, Dialog/helpui.cpp, Dialog/immigrationui.cpp, Dialog/mapui.cpp, Dialog/modbrowseui.cpp, Dialog/noiseui.cpp, Dialog/orchardui.cpp, Dialog/pagetoggleui.cpp, Dialog/pastureui.cpp, Dialog/productionui.cpp, Dialog/professionui.cpp, Dialog/radiusui.cpp, Dialog/radiusui_promote.cpp, Dialog/residenceui.cpp, Dialog/resourcelimitui.cpp, Dialog/selectresourceui.cpp, Dialog/spritecaptureui.cpp, Dialog/statusbarui.cpp, � (+14) |

Only `ui/Dialogs.h` (22), `ui/Panels.h` (38), `AchievementGoals.h` (7), `TutorialGoals.h` (3), `TutorialImages.h` (4),
`TutorialSetups.h` (4), `Tool.h` (2), `UpdateLoop.h` (2) and `common.h` hold 0B classes; the rest are listed for
completeness (0A's / helper structs). For the file map: every Dialogs.h / Panels.h class has its own .cpp, but the
two headers are included by 27 / (see row) files each, so a per-class header split is a B6-4.4 closing-pass job.

---------------------------------------------------------------------------------------------------
## 4. Counts per directory, and the RegisterExeAddr seam

| dir | exe classes | hand-built tables | RegisterExeAddr of a vtable-range address (in the dir's files) | REGISTER_TYPE (+UI_DIALOG_STUB) | vtable-pointer loads `*(void***)x` / `*reinterpret_cast<void***>(x)` (files) | QUAL calls already in place | unqualified base-dtor door calls |
|---|---|---|---|---|---|---|---|
| code/Game/Dialog | 61 (56 with a table, 4 UI_DIALOG_STUB-only: ModBrowseUI, NoiseUI, SpriteCaptureUI, ModUploadDialog; SelectResourceUI none) | 58 (56 class tables + the UI_DIALOG_STUB macro body + the thumb mirror) | 13 | 49 (+12) | 104 (36) | 4 | 27 |
| code/Game/Game | 17 (+ the Particle scratch table) | 18 | 14 | 13 | 25 (12) | 12 | 12 |
| code/Game/Goal | 5 | 5 | 5 | 5 | 3 (3) | 11 | 0 |
| code/Game/Player | 12 (9 tools + Tool, DragTool, ToolSetting without tables) + the ToolUI child DialogInstance table | 10 | 9 | 9 | 29 (13) | 0 | 13 |
| code/Game/Tutorial | 20 (18 tables + TutorialGoal, TutorialSetup) | 18 | 14 | 19 | 14 (8) | 4 | 18 |
| standalone/ (top level) | 9 (5 loops, TutorialInstance, ImageBuffer copy, ControllerComponent*, TornadoComponent* — *0A's) | 10 (+ EnumStub diagnostic) | 20 (incl. registrations of other dirs' tables: reg_mods, reg_tutorial_wave, reg_nullsweep, reg_w2_lane*, address_registry) | 0 | 76 (22) | 7 | 0 |
| **total** | **124** | **119** | **75** | **95 (+12)** | **251** | **38** | **70** |

(The `(*(void***)p)[N]` one-expression spelling is rare; the tree loads the vptr into a local first — so the column
counts vptr LOADS. It includes loads on non-self objects; b36 §3B and b37 §3D proved the Dialog/Goal ones that target
`self`.)

**The seam (brief item 5).** An exe table address is reached either (i) only through the hand-built table's own
registration, which dies with the table, or (ii) by address from other code. By-address readers
(`ResolveAddr(Module::Exe, <addr in the 0B ranges>)`, `byaddr.py`) — 68 distinct (address, registered?) pairs,
134 sites:

| retail | registered? | sites | where |
|---|---|---|---|
| 0x140158800 | REG | 1 | engine/funcs/engine_funcs.cpp:2090 |
| 0x1401588d0 | REG | 1 | engine/funcs/engine_funcs.cpp:2091 |
| 0x14015b020 | REG | 1 | engine/funcs/engine_funcs.cpp:1563 |
| 0x14015be00 | UNREG | 1 | engine/funcs/engine_funcs.cpp:1494 |
| 0x14015beb0 | REG | 1 | engine/funcs/engine_funcs.cpp:1565 |
| 0x14015c3b0 | UNREG | 1 | engine/funcs/engine_funcs.cpp:1915 |
| 0x14015c400 | UNREG | 1 | engine/funcs/engine_funcs.cpp:1914 |
| 0x14015c480 | REG | 3 | Dialog/saveloaddialog.cpp:923, Dialog/saveloaddialog.cpp:1104, engine/funcs/engine_funcs.cpp:1742 |
| 0x14015c670 | REG | 1 | engine/funcs/engine_funcs.cpp:1710 |
| 0x14015c6c0 | UNREG | 1 | engine/funcs/engine_funcs.cpp:1249 |
| 0x14015c8c0 | REG | 1 | engine/funcs/engine_funcs.cpp:1943 |
| 0x14015e140 | REG | 1 | Game/climategenerator.cpp:160 |
| 0x14015e190 | REG | 1 | Game/disease.cpp:58 |
| 0x14015e240 | UNREG | 2 | Player/fencedtool.cpp:799, Player/placementtool.cpp:910 |
| 0x14015e2f0 | REG | 2 | Game/gameupdateloop.cpp:2714, engine/funcs/engine_funcs.cpp:2082 |
| 0x14015e350 | UNREG | 1 | Game/gameupdateloop_load.cpp:1621 |
| 0x14015ed00 | REG | 1 | engine/funcs/engine_funcs.cpp:2101 |
| 0x14015ed80 | REG | 1 | Game/mapgenerator.cpp:114 |
| 0x14015ede0 | REG | 2 | Game/menuscene.cpp:115, Game/menuscene.cpp:196 |
| 0x14015ee40 | REG | 2 | Game/musiclist.cpp:189, Game/musiclist.cpp:258 |
| 0x14015eea0 | REG | 2 | Game/namedatabase.cpp:71, Game/namedatabase.cpp:139 |
| 0x14015ef18 | UNREG | 1 | Game/keystone_a_gamedtor.cpp:321 |
| 0x14015ef50 | REG | 1 | Game/professionlist.cpp:70 |
| 0x14015efa0 | REG | 1 | Game/profession.cpp:73 |
| 0x14015f020 | REG | 2 | Game/scenario.cpp:83, Game/scenario.cpp:202 |
| 0x14015f110 | REG | 1 | Game/serialize_arrays.cpp:92 |
| 0x14015f170 | REG | 1 | Game/terraingenerator.cpp:305 |
| 0x14015f550 | REG | 1 | engine/funcs/engine_funcs.cpp:1330 |
| 0x14015f5a0 | UNREG | 2 | engine/funcs/engine_funcs.cpp:1336, engine/funcs/engine_funcs.cpp:1543 |
| 0x14015f980 | UNREG | 1 | engine/funcs/engine_funcs.cpp:1342 |
| 0x14015fa00 | UNREG | 1 | engine/funcs/engine_funcs.cpp:1329 |
| 0x140160120 | REG | 2 | Game/toolbar.cpp:148, Game/toolbar.cpp:184 |
| 0x140160240 | REG | 2 | Game/weather_particles.cpp:138, Game/weather_particles.cpp:158 |
| 0x140160290 | REG | 1 | Game/weather.cpp:67 |
| 0x140160300 | REG | 1 | Goal/buildinggoal.cpp:43 |
| 0x140160370 | REG | 1 | Goal/populationgoal.cpp:33 |
| 0x140160420 | REG | 1 | Tutorial/finishtutorialgoal.cpp:32 |
| 0x140160478 | REG | 2 | Tutorial/finishtutorialgoal.cpp:33, Tutorial/finishtutorialinstance.cpp:301 |
| 0x1401604c0 | REG | 2 | Goal/statgoal.cpp:317, Goal/statgoal.cpp:446 |
| 0x140160540 | REG | 2 | Goal/timegoal.cpp:117, Goal/timegoal.cpp:226 |
| 0x140160598 | REG | 2 | Goal/timegoal.cpp:86, Goal/timegoalinstance.cpp:201 |
| 0x1401605d0 | REG | 1 | Player/fencedtool.cpp:45 |
| 0x140160680 | REG | 1 | Player/pathtool.cpp:34 |
| 0x140160740 | REG | 3 | Tutorial/tutorial.cpp:68, Tutorial/tutorial.cpp:118, Tutorial/tutorial.cpp:149 |
| 0x1401607a0 | REG | 10 | Tutorial/tutorialgoal.cpp:34, Tutorial/tutorialgoalbutton.cpp:52, Tutorial/tutorialgoalbutton.cpp:121, Tutorial/tutorialgoalbutton.cpp:204, Tutorial/tutorialgoalcount.cpp:167, Tutorial/tutorialgoalcount.cpp:658, Tutorial/tutorialgoaldialog.cpp:45, Tutorial/tutorialgoalgamespeed.cpp:58, Tutorial/tutorialgoalgamespeed.cpp:118, Tutorial/tutorialgoalgamespeed.cpp:144 |
| 0x140160800 | REG | 3 | Tutorial/tutorialgoalbutton.cpp:45, Tutorial/tutorialgoalbutton.cpp:123, Tutorial/tutorialgoalbutton.cpp:206 |
| 0x140160870 | REG | 1 | Tutorial/tutorialgoaldialog.cpp:44 |
| 0x1401608e0 | REG | 3 | Tutorial/tutorialgoalgamespeed.cpp:54, Tutorial/tutorialgoalgamespeed.cpp:120, Tutorial/tutorialgoalgamespeed.cpp:146 |
| 0x140160950 | REG | 2 | Tutorial/tutorialgoalcount.cpp:157, Tutorial/tutorialgoalcount.cpp:660 |
| 0x140160a40 | REG | 5 | Tutorial/tutorialimage.cpp:38, Tutorial/tutorialimagehotkey.cpp:83, Tutorial/tutorialimagehotkey.cpp:183, Tutorial/tutorialimageinputkey.cpp:43, Tutorial/tutorialimagetoolbaricon.cpp:45 |
| 0x140160ab0 | REG | 2 | Tutorial/tutorialimagehotkey.cpp:76, Tutorial/tutorialimagehotkey.cpp:189 |
| 0x140160b20 | REG | 1 | Tutorial/tutorialimageinputkey.cpp:42 |
| 0x140160b90 | REG | 1 | Tutorial/tutorialimagetoolbaricon.cpp:44 |
| 0x140160bf8 | REG | 3 | Tutorial/tutorialinstance.cpp:65, Tutorial/tutorialinstance.cpp:166, engine/funcs/engine_funcs.cpp:2464 |
| 0x140160c30 | REG | 10 | Tutorial/tutorialsetup.cpp:34, Tutorial/tutorialsetupcamera.cpp:45, Tutorial/tutorialsetupcamera.cpp:107, Tutorial/tutorialsetupcamera.cpp:172, Tutorial/tutorialsetupdialog.cpp:127, Tutorial/tutorialsetupgamespeed.cpp:78, Tutorial/tutorialsetupgamespeed.cpp:134, Tutorial/tutorialsetupgamespeed.cpp:161, Tutorial/tutorialsetuprestrictplayer.cpp:163, Tutorial/tutorialsetuprestrictplayer.cpp:368 |
| 0x140160ca0 | REG | 3 | Tutorial/tutorialsetupcamera.cpp:41, Tutorial/tutorialsetupcamera.cpp:110, Tutorial/tutorialsetupcamera.cpp:175 |
| 0x140160d20 | REG | 1 | Tutorial/tutorialsetupdialog.cpp:104 |
| 0x140160da0 | REG | 3 | Tutorial/tutorialsetupgamespeed.cpp:74, Tutorial/tutorialsetupgamespeed.cpp:136, Tutorial/tutorialsetupgamespeed.cpp:163 |
| 0x140160e20 | REG | 2 | Tutorial/tutorialsetuprestrictplayer.cpp:155, Tutorial/tutorialsetuprestrictplayer.cpp:371 |
| 0x140160e90 | REG | 2 | Tutorial/tutorialstep.cpp:97, Tutorial/tutorialstep.cpp:150 |
| 0x140161b00 | REG | 1 | Player/bridgetool.cpp:35 |
| 0x140161b90 | REG | 1 | Player/dragplacementtool.cpp:43 |
| 0x140161c10 | UNREG | 6 | Player/bridgetool.cpp:36, Player/dragplacementtool.cpp:44, Player/dragtool.cpp:34, Player/fencedtool.cpp:46, Player/removaltool.cpp:34, Player/roadtool.cpp:42 |
| 0x140161ca0 | REG | 1 | Player/edittool.cpp:52 |
| 0x140161d20 | REG | 1 | Player/placementtool.cpp:181 |
| 0x140161de0 | REG | 1 | Player/removaltool.cpp:33 |
| 0x140161e70 | REG | 1 | Player/roadtool.cpp:41 |
| 0x140161f10 | UNREG | 9 | Player/bridgetool.cpp:37, Player/dragplacementtool.cpp:45, Player/dragtool.cpp:35, Player/edittool.cpp:53, Player/fencedtool.cpp:47, Player/pathtool.cpp:35, Player/placementtool.cpp:182, Player/removaltool.cpp:35, Player/roadtool.cpp:43 |

Reading: most registered-address reads are a class re-reading ITS OWN table to restamp `self` in its dtor or
in-place ctor (Tutorial*, Game externals, Goals, tools) — those die with the conversion (the compiler stamps). The
cross-file readers that need a door or a rewrite are: `engine/funcs/engine_funcs.cpp` (loops :2082/:2090/:2091/:2101,
dialogs :1329/:1330/:1336/:1342/:1494/:1543/:1563/:1565/:1710/:1742/:1914/:1915/:1943, TutorialInstance :2464),
`saveloaddialog.cpp:923/:1104` (MessageDialog), `gameupdateloop.cpp:2714` (GameUpdateLoop), the tutorial subclass files
reading the TutorialGoal / TutorialSetup / TutorialImage base addresses (1.14), and the tool files reading
Tool / DragTool (1.11).

**Threading (brief item 6), noted not measured:** GameUpdateLoop::Load (gameupdateloop_load.cpp, retail 0x140096b00)
runs on the loader thread and constructs the SkyMesh MeshInstance (0x14015e240), the AchievementInstance
(0x14015e350) and 0x14015e290; the menu loop draws concurrently. Function-local `static void* s_vtable =
ResolveAddr(...)` caches (e.g. scenario.cpp, menuscene.cpp:196) are initialised on first use from whichever thread
gets there first.

---------------------------------------------------------------------------------------------------
## 5. How this was measured

Scripts (not in the repo): `C:/Users/USPC/AppData/Local/Temp/claude/b38/0B/`.
- `common.py` — file sets: scope = `code/Game/{Dialog,Game,Goal,Player,Tutorial}/*.{cpp,h}` + `standalone/*.{cpp,h}`
  (top level only — `standalone/build*` never walked); repo = `code/**`, `engine/**`, `standalone/*`. Comments are
  blanked before every regex (`strip_comments`).
- `regs.py` → `regs.tsv`: every `RegisterExeAddr(` (paren-matched, multi-line) whose first argument resolves (literal,
  same-file `NAME = 0x…`, or repo-wide `NS::NAME`) into 0x140153000-0x140162000; plus `{0x1401…, x}` entries. 155 rows.
- `arrays.py` → `arrays.json`: every `void*|uintptr_t|const void*` array declaration in scope; kept if the name matches
  `vt|vtbl|vtable|slots` (121) — the 30 others were read and are buffers.
- `analyze.py` → `analysis.json`: initializer entries, `name[N] = x` and `name[i] = x` fills in the table's namespace
  block, `DLGCTRL_VTABLE_INHERIT`, getters, allocs (`MemAlloc(…, SIZE, 8)` in the namespace block), ctor doors,
  slot-0 body base-dtor calls. `assemble.py` + `genmd.py` → the main table (Apply() masks from
  `dialog_controller_vtable.cpp:72-114`; by-name fills and the loop classifications applied by hand from
  toolbar_vtable_standalone.cpp:130-135, mod_dialog_vtables.cpp FillBaseSlots, goal_exe_helpers.cpp:226-228 and
  b37 §1A).
- `rtypes.py` — REGISTER_TYPE / _FULL sites; duplicate sweep over REGISTER_TYPE / _FULL / UI_DIALOG_STUB /
  REGISTER_ABSTRACT_FACTORY / REGISTER_NULL_FACTORY(_FULL) repo-wide (311 names, 1 duplicate).
- `pods.py`, `sharedhdr.py` — top-level `struct|class X {` in `engine/types/**`, `static_assert(sizeof(X) == …)`,
  transitive include closure (include strings resolved against the including dir, engine/types, engine/funcs,
  code/Game, code, standalone, engine).
- `mfields.py` — §2. `qual.py` — `(->|.)(Achievement|UI|Engine|Object|Graphics|Audio|Save|Picking|Input|System|Video|IO)::Cls::member(`.
  `byaddr.py` — every `ResolveAddr(Module::Exe, x)` resolving into the vtable range, cross-checked against `regs.tsv`.
- Unqualified base-dtor doors: `grep -oE "\b(fn_DialogInstance_Dtor|fn_External_Dtor|s_fn_External_BaseDtor|s_fn_Internal_Dtor|fn_ObjectInternal_Dtor|fn_UpdateLoop_Dtor|s_fnGeoBaseDtor)\s*\("` per dir.
- Ghidra (Application-x64.exe, asserted by name inside each script; switched back to Runtime-x64.dll after):
  `ExeVtDump0B2` — every 8-aligned qword in 0x140153000-0x140162800 that points into an executable block AND has a
  code xref or a `??_7`/vtable label is a table start (296 starts; the exe has no RTTI COLs — a COL scan found 3);
  slots = consecutive code pointers up to the next start, then trimmed at the first 0 (`retail_fixed.tsv`) —
  tables whose tail entry is unreferenced can still merge (0x14015c8c0+0x14015c908, 0x14015e240+0x14015e248; fixed
  by hand above). `ExeVtDump0B3` — per table, the referencing functions and slot targets. `Thunks0B` — import
  names behind the thunks of the unidentified tables. `Stamps0B` → `exe_stamps.tsv` — every `LEA r,[table]`
  followed within 4 instructions by `MOV qword [x], r` (1,464 pairs), which settles 1.1, 1.10 and the stamp chains.
- Not measured: the identity of 0x14015e290 / 0x14015ecb0 / 0x14015f980; per-slot argument shapes; which of the
  unqualified base-dtor doors sit inside slot 0 vs helpers (the slot-0 column gives the slot-0 ones).
