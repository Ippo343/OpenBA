# B.3 step 6 — exe-phase subclasses of Runtime `UI::` classes (hand-built vtables)

Read-only enumeration, 2026-09-25, main @ dc647869. Every claim carries file:line.
Scope: `code/Game/**`, `standalone/**`, `engine/**`. Excluded: build dirs, worktrees, `runtime/`
(the runtime's own struct-embedding subclasses are listed only in §6 where they decide by-value).

Conventions used below
- "slot N" = vtable index N (byte offset 8N).
- "own" = the file installs its own body; "base" = the slot holds (or is filled with) the Runtime
  base virtual; "nop" = file-local variadic no-op (`StubNop`/`Nop`/`MenuDlg_Nop`/`SD_Nop`/
  `ModDlg_InertSlot`/`RuntimeThunk<N>` print-once stub); "null" = literal `nullptr` until an init
  routine fills it.
- Base ctor column says how the base sub-object is constructed at the standalone construction site.

---------------------------------------------------------------------------------------------------
## 1. Complete list of hand-built-vtable subclasses of a Runtime UI class

### 1A. `UI::DialogInstance` subclasses — 20 classes, 21 tables (MapParamsDialog has one table,
### MainMenuDialog one, both in mainmenudialog.cpp; ToolUI child dialog is a 21st table for the
### BASE class itself)

DialogInstance slot map used by every 10-slot table (retail; toolbar_vtable_standalone.cpp:17-27,
tool_base_toolui.cpp:31-43): 0 ScalarDtor · 1 TypeName · 2 Type/GetTypeId · 3 Persist · 4 OnOpened
(Setup/Startup) · 5 OnClosed (Shutdown) · 6 OnUpdateLayout · 7 OnUpdate · 8 OnElementEvent ·
9 OnSerializeSettings. Files name 6/7 inconsistently ("Draw/Update", "Update/Load") — the INDEX is
what matters.

| Class | Table var | File:line | Slots | Retail vtable | Alloc size | Own slots | Base/nop slots | Base ctor | Dtor calls base? |
|---|---|---|---|---|---|---|---|---|---|
| AchievementDialog | `g_AchievementDialog_Vtable_Standalone` | menu_dialog_standalone_vtables.cpp:90 | 10 | 0x14015b020 (:150) | 0xD0 (Dialogs.h:154; built by MainMenuDialog opener) | 0,2,4,6,7,8 | 1,3,5,9 nop (`MenuDlg_Nop`) | `fn_DialogInstance_Ctor` in the MainMenuDialog opener (mainmenudialog.cpp:308/353) | yes `fn_DialogInstance_Dtor` :74 |
| AutoSaveDialog | `autosave_sa::g_vt` | autosavedialog.cpp:201, Init :228-241 | 10 | 0x14015b120 (:185) | 0xD0 (Dialogs.h:165) | 0,1,2,4,6,7,8 | 3,5,9 nop | `fn_DialogInstance_Ctor` :248 | yes :216 |
| EventNotifyDialog | `eventnotify_sa::g_vt` | eventnotifydialog.cpp:989, Init :991-1003 | 10 | 0x14015bca0 (Dialogs.h:34) | 0xF0 (:1030) | 0,1,2,4,5,6,7,8 | 3,9 nop | `fn_DialogInstance_Ctor` :1015 | yes :968 |
| EventPopupDialog | `eventpopup_sa::g_vt` | eventpopupdialog.cpp:125, Init :127-135 | 10 | 0x14015bd80 (:90) | 0xC0 (:155) | 0,1,2,4,8 | 3,5,6,7,9 nop (for-loop fill :128) | `fn_DialogInstance_Ctor` :145 | yes :116 |
| FollowDialog | `g_FollowDialog_Vtable_Standalone` | followdialog.cpp:440 | 10 | 0x14015be00 (citizenui.cpp:39) | 0xD8 (citizenui.cpp:651) | 0,1,2,4,5,6,7,8 | 3,9 nop | `fn_DialogInstance_Ctor` citizenui.cpp:657 | yes :392 |
| GoalDialog | `g_GoalDialog_Vtable_Standalone` | menu_dialog_standalone_vtables.cpp:128 | 10 | 0x14015beb0 (:152) | 0xC8 (Dialogs.h:212) | 0,2,4,6,7,8 | 1,3,5,9 nop | `fn_DialogInstance_Ctor` via MainMenuDialog opener | yes :112 |
| LoaderDialog | `g_LoaderDialog_Vtable_Standalone` | loaderdialog.cpp:113 | 10 | 0x14015f300 (:182) | 0xC8 (:170) | 0,1,2,4,6,7 | 3,5,8,9 `RuntimeThunk<N>` print-once nop | `fn_DialogInstance_Ctor` :157 | yes :84 |
| MainMenuDialog | `g_MainMenuDialog_Vtable_Standalone` | mainmenudialog.cpp:1074 | **20** (10 DI + 10 "secondary" block) | 0x14015f550 (:1300) | 0xC8 (Dialogs.h:233) | 0,1,2,4,5,6,7,8,10 | 3,9,11-19 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :1254 | yes :819; slot 10 also calls it :1069 |
| MapParamsDialog | `g_MapParamsDialog_Vtable_Standalone` | mainmenudialog.cpp:1153 | 20 | 0x14015c180 (:1108) | 0xF8 (:1185) | 0,2,4,5,6,7,8,10 (7 = shared `StandaloneImpl_Slot7_OnUpdate`) | 1 shared TypeName stub, 3,9,11-19 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :1192, unreg_lane2_mapparamsdialog_ctor.cpp:85 | yes :1120 |
| MessageDialog | `g_MessageDialog_Vtable_Standalone` | messagedialog.cpp:204 | **14** (10 + OnOk/OnCancel/OnYes/OnNo) | 0x14015c480 (:272) | sizeof(MessageDialog)=0xC0 (:251) | 0,1,2,4,5,8,10,11,12,13 | 3,6,7,9 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :237 | yes :157 |
| ModDetailsDialog | `g_ModDetailsVt` | mod_dialog_vtables.cpp:97, Init :244-247 | 10 | 0x14015c670 (reg_mods.cpp:77) | 0xC8 (:496) | 0,2,4,8 | 1,3,5,6,7,9 **filled from Runtime by name** (`FillBaseSlots` :216-230, `ResolveExport(kSym_*)`) else `ModDlg_InertSlot` | `fn_DialogInstance_Ctor` moddetailsdialog.cpp (1 site) | yes :191 |
| ModDialog | `g_ModDialogVt` | mod_dialog_vtables.cpp:96, Init :239-242 | 10 | 0x14015c870 (reg_mods.cpp:76) | 0xD8 (:366/:480) | 0,2,4,8 | 1,3,5,6,7,9 same by-name fill | `fn_DialogInstance_Ctor` :319 | yes :143 |
| NewGameDialog | `g_NewGameDialog_Vtable_Standalone` | newgamedialog.cpp:1071 | 10 | 0x14015c920 (:1160) | 0xAE8 (Dialogs.h:310) | 0,1,2,4,5,6,8; **7 = wrapper that calls `fn_DialogInstance_OnUpdate`** (:1058-1069) | 3,9 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :1105 | yes :1028 |
| OptionInputBindDialog | `g_OptionInputBindDialog_Vtable_Standalone` | optioninputbinddialog.cpp:299 | 14 (extends MessageDialog) | 0x14015ce40 (:389) | 0xC8 (Dialogs.h:371) | 0,1,2,4,5,7,8,10-13 | 3,6,9 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :339 (NOT MessageDialog's ctor) | yes :244 |
| OptionsDialog | `g_OptionsDialog_Vtable_Standalone` | optionsdialog.cpp:1725 | 10 | 0x14015ceb0 (:1802) | sizeof(OptionsDialog)=0x1C0 (:1778) | 0,1,2,4,5,8 | 3,6,7,9 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :1757 | yes :1686 |
| SaveLoadDialog | `g_SaveLoadDialog_Vtable_Standalone` | saveloaddialog.cpp:1283 | 10 | 0x14015f900 (:1333) | 0x160 (Dialogs.h:395) | 0,1,2,4,6,7,8 | 3,5,9 `RuntimeThunk<N>` | `fn_DialogInstance_Ctor` :1312 | yes :1263 |
| ScenarioDialog | `g_ScenarioDialog_Vtable_Standalone` | scenariodialog.cpp:584 | 10 | 0x14015d2d0 (reg_nullsweep.cpp:21) | 0xE8 (Dialogs.h:410) | 0,2,4,6,7,8 | 1,3,5,9 `SD_Nop` | `fn_DialogInstance_Ctor` :619 | yes :568 |
| ToolbarDialog | `g_ToolbarDialog_Vtable_Standalone` | toolbar_vtable_standalone.cpp:105 | 10 | 0x14015fa00 (:4; NOT RegisterExeAddr'd — published through `g_ToolbarDialog_Vtable` :147, read by toolbardialog.cpp:1077) | 0x108 (Dialogs.h:430) | 0,2,4,5,6,8 | **1,3,7,9 = raw Runtime base virtuals resolved BY NAME** (`ResolveExport(kSym_*)` :130-135) | `fn_DialogInstance_Ctor` toolbardialog.cpp (3 sites) | yes :78 |
| TutorialHighlightDialog | `tut_highlight::g_vt` | tutorial_dialog_vtables.cpp:427, Init :465-474 | **48** (`kSlots` :135, `FillNops`) | 0x1401609E0 (reg_tutorial_wave.cpp:89) | 0xF0 (:480) | 0,1,2,4,6,7 | 3,5,8,9,10-47 `Nop` | `fn_DialogInstance_Ctor` :522 (`TutWave_HighlightDialogCtor`) | :436-440 — resets vtable + `TutDlg_UnregisterInternal` + free; **does NOT call `fn_DialogInstance_Dtor`** (note :85-127) |
| ModUploadDialog | `uistub_ModUploadDialog::g_vt` | ui_dialog_stub_factories.cpp:1692 (macro :72-108) | 48 | 0x14015c6c0 (moduploaddialog.cpp:4; read but never registered, engine_funcs.cpp:1858) | 0xB8 | 0,2 | all others `StubNop` | **none** (alloc + zero + vtable only, macro :92-106) | no (macro :80-85) |
| *(base itself)* ToolUI child dialog `UI::DialogInstance` | `g_ToolUI_ChildDialog_Vtable_Standalone` | tool_base_toolui.cpp:118 | 10 | 0x14015f5a0 (deliberately NOT registered, reg_player_toolui.cpp:19-27) | 0xB8 (:220) | 0 only | **1..9 COPIED from Runtime `??_7DialogInstance@UI@@6B@` 0x180156d70** (:142-146) | `fn_DialogInstance_Ctor` :227 | yes :105 |

Also DialogInstance-based but with only a dormant `UI_DIALOG_STUB` table (48 nops + own Dtor/GetTypeId,
no base ctor) that a real table SHADOWS when the class is built by hand: AchievementDialog,
AutoSaveDialog, FollowDialog, GoalDialog, MapParamsDialog, ScenarioDialog, SaveLoadDialog,
ToolbarDialog (ui_dialog_stub_factories.cpp:1685-1695). These are the by-name factory path; the
tables above are the by-hand path. Both exist today.

NOT DialogInstance-based although listed among the "22" in `engine/types/ui/Dialogs.h`:
TutorialGoalDialog (Object::External, Dialogs.h:435; table `tut_goaldlg::g_vt`
tutorial_dialog_vtables.cpp:239, registered at 0x140160870 AND 0x1401607A0 reg_tutorial_wave.cpp:85-86)
and TutorialSetupDialog (External, Dialogs.h:466; `tut_setupdlg::g_vt` :332, 0x140160D20 + 0x140160C30
:87-88, base ctor `fn_External_External` :544). Listed for completeness; they are `Object::External`
subclasses and outside UI's blast radius except that they hand-build `TutorialHighlightDialog`.

### 1B. `UI::DialogController` subclasses — 39 in `kMasks` (dialog_controller_vtable.cpp:76-115),
### 38 with a standalone table, 1 (SelectResourceUI) with none

Common shape for every promoted file: `constexpr int kSlots = 48; static void* g_vt[kSlots];`
for-loop fill with `StubNop`, own slots stamped, then `DLGCTRL_VTABLE_INHERIT(g_vt, kSlots, "Cls",
&StubNop)` (destroyui.cpp:539-650 is the reference copy). `UIDialogControllerVtable::Apply()`
(dialog_controller_vtable.cpp:150-201) later overwrites slots 3..10 that (a) retail inherits per
the mask and (b) still hold that file's stub, with the Runtime bodies: slot 3 by NAME
(`?Persist@Internal@Object@@`), slots 4-10 by ADDRESS `ResolveAddr(Module::Runtime, 0x1800cd2b0/2a0/
290/280/270/260/250)` (:44-51) — which resolves to the PORTED bodies because ResolveAddr consults the
port registry first (:22-28). Slots 0/1/2 are never filled (:53-56).

**No DialogController subclass runs the base ctor** (grep for `DialogController_Ctor`/0x1800cd320
finds only the comment engine/types/ui/DialogController.h:15) and **no subclass Dtor calls
`fn_DialogController_Dtor`** (zero callers outside engine_funcs.*). Every factory is alloc + zero +
vtable; AddController fills +0x10..+0x28 afterwards (DialogInstance.h:157-170).

Alloc size = `fn_MemAlloc(g_pMemAllocSingleton, SIZE, 8)` at the cited Factory line.

| Class | Mask (retail inherits) | Retail vtable | Table / file:line | Own slots (stamped) | Alloc | Notes |
|---|---|---|---|---|---|---|
| AttendanceUI | 0x768 | 0x14015b070 | `uistub_AttendanceUI::g_vt` ui_dialog_stub_factories.cpp:248, wire :315 | 0,2,4,7 | 0x38 :318 | |
| BuildRequirementUI | 0x7e0 | 0x14015b1d0 | `uistub_BuildRequirementUI::g_vt` :121, wire :158 | 0,2,4 | 0x60 :160 | |
| BuildUI | 0x568 | 0x14015f210 | `uistub_BuildUI::g_vt` :1580, wire :1648-1650 | 0,2,4,7,9 | 0x50 :1655 | |
| CemeteryUI | 0x768 | 0x14015b240 | `uistub_CemeteryUI::g_vt` :331, wire :383 | 0,2,4,7 | 0x38 :386 | |
| CitizenUI | 0x528 | 0x14015ba00 | `citizenui_sa::g_vt` citizenui.cpp:729, Init :820-826 | 0,2,4,6,7,8,9 | 0xE8 :839 | slot 8 OnUpdate is a standalone-only override (kept by Apply) |
| ConsumeProduceUI | 0x568 | 0x14015b2e0 | `uistub_ConsumeProduceUI::g_vt` :404, wire :467-469 | 0,2,4,7,9 | 0x40 :471 | |
| ControlUI | 0x568 | 0x14015b380 | `controlui_sa::g_vt` controlui.cpp:262, Init :381-385 | 0,2,4,7,9 | 0x40 :403 | |
| CropUI | 0x468 | 0x14015f270 (14 slots) | `cropui_sa::g_vt` cropui_promote.cpp:127, Init :230-238 | 0,2,4,7,8,9,11,12,13 | 0x60 :255 | parent "SelectResourceUI" :270 |
| DebugUI | 0x428 | 0x14015b870 | `debugui_sa::g_vt` debugui.cpp:1636, Init :1725-1731 | 0,2,4,7,8,9 | 0x130 :1747 | REGISTER_TYPE parent spelled `"DialogController"` not `"UI::DialogController"` :1762 |
| DestroyUI | 0x568 | 0x14015bb00 | `destroyui_sa::g_vt` destroyui.cpp:539, Init :635-643 | 0,2,4,7,8,9 | 0x78 :659 | |
| EntityUI | 0x420 | 0x14015bb90 | `entityui_sa::g_vt` entityui.cpp:785, Init :895-901 | 0,2,4,6,7,8,9 | 0x48 :914 | |
| EventLogUI | 0x518 | 0x14015bc40 | `eventlogui_sa::g_vt` eventlogui.cpp:353, Init :385-390 | 0,2,5,7,(8),9 | 0x80 :403 | |
| HelpUI | 0x548 | 0x14015bf30 | `uistub_HelpUI::g_vt` :540, wire :642-644 | 0,2,4,7,9 | 0x78 :651 | |
| HotKeyToolTipUI | 0x7e0 | 0x14015bfc0 | `uistub_HotKeyToolTipUI::g_vt` :178, wire :207 | 0,2,4 | 0x30 :209 | |
| ImmigrationUI | 0x568 | 0x14015c080 | `uistub_ImmigrationUI::g_vt` :677, wire :734 | 0,2,4,7,9 | 0x50 :736 | |
| MapUI | 0x5e0 | 0x14015c350 | `uistub_MapUI::g_vt` :753, wire :796 | 0,2,4,9 | 0x50 :798 | |
| ModBrowseUI | 0x7f0 | 0x14015c500 | `uistub_ModBrowseUI::g_vt` macro :803, Register :816 | 0,2 | 0x30 | dormant stub |
| NoiseUI | 0x5e0 | 0x14015ca80 | `uistub_NoiseUI::g_vt` macro :804, Register :818 | 0,2 | 0x70 | dormant stub |
| OrchardUI | 0x468 | 0x14015cf30 (14) | `orchardui_sa::g_vt` orchardui.cpp:268, Init :333-341 | 0,2,4,7,8,9,11,12,13 | 0x48 :358 | parent "SelectResourceUI" :373 |
| PageToggleUI | 0x560 | 0x14015cfb0 | `uistub_PageToggleUI::g_vt` :859, wire :909 | 0,2,4,7,9 | 0x48 :911 | |
| PastureUI | 0x468 | 0x14015f610 (14) | `pastureui_sa::g_vt` pastureui.cpp:382, Init :453-461 | 0,2,4,7,8,9,11,12,13 | 0x40 :478 | parent "SelectResourceUI" :493 |
| ProductionUI | 0x760 | 0x14015d070 | `uistub_ProductionUI::g_vt` :941, wire :991 | 0,2,4,7 | 0x50 :993 | |
| ProfessionUI | 0x4d8 | 0x14015d0f0 | `uistub_ProfessionUI::g_vt` :1027, wire :1104-1106 | 0,2,5,8,9 | 0x78 :1108 | |
| RadiusUI | 0x798 | 0x14015d160 (11) | `radiusui_sa::g_vt` radiusui_promote.cpp:53, Init :94-97 | 0,2,5,6 | 0x38 :121 | |
| ResidenceUI | 0x560 | 0x14015f6d0 | `residenceui_sa::g_vt` residenceui.cpp:515, Init :615-620 | 0,2,4,7,8,9 | 0x40 :636 | |
| ResourceLimitUI | 0x568 | 0x14015d1f0 | `uistub_ResourceLimitUI::g_vt` :1141, wire :1196 | 0,2,4,7,9 | 0x40 :1198 | |
| **SelectResourceUI** | 0x468 | 0x14015d330 | **no standalone table** — abstract base (Panels.h:531-533, orchardui.cpp:239); no REGISTER_TYPE anywhere; children name it as parent | — | — | its shared methods live in selectresourceui.cpp as free functions |
| SpriteCaptureUI | 0x4e0 | 0x14015d4f0 | `uistub_SpriteCaptureUI::g_vt` macro :1203, Register :1209 | 0,2 | 0x70 | dormant stub |
| StatusBarUI | 0x518 | 0x14015d810 | `statusbarui_sa::g_vt` statusbarui.cpp:663, Init :847-853 | 0,2,4,5,7,8,9 | 0x1D0 :869 | slot 4 standalone-only override |
| StatusIconUI | 0x768 | 0x14015d8a0 | `statusiconui_sa::g_vt` statusiconui.cpp:275, Init :341-345 | 0,2,4,7,8 | 0x40 :358 | |
| StorageCapacityUI | 0x760 | 0x14015d950 | `storagecapacityui_sa::g_vt` storagecapacityui.cpp:138, Init :217-221 | 0,2,4,7,8 | 0x38 :237 | |
| StorageUI | 0x760 | 0x14015f970 | `storageui_sa::g_vt` storageui.cpp:578, Init :667-671 | 0,2,4,7,8 | 0x40 :687 | |
| TitleDescToolTipUI | 0x7e0 | 0x14015d9f0 | `uistub_TitleDescToolTipUI::g_vt` :1249, wire :1270 | 0,2,4 | 0x30 :1272 | |
| ToolUI | 0x4e0 | 0x14015da70 | `uistub_ToolUI::g_vt` :1305, wire :1366-1368 | 0,2,4,8,9 | 0x58 :1370 | |
| TownHallUI | 0x068 | 0x14015dce0 | `townhallui_sa::g_vt` townhallui.cpp:2132, Init :2437-2443 | 0,2,4,7,8,9,10 | 0x1E8 :2459 | |
| TradeUI | 0x008 | 0x14015de80 | `uistub_TradeUI::g_vt` :1426, wire :1542-1548 | 0,2,4,5,6,7,8,9,10 | 0x148 :1550 | |
| TutorialUI | 0x560 | 0x140160f30 (11) | `tut_ui::g_vt` tutorial_dialog_vtables.cpp:149, Init :203-209, INHERIT :220 (stub is `Nop`) | 0,1,2,4,7,9 | 0x60 :225 | slot 3 (0x1400E3AA0) NOT PORTED :30 |
| UpgradeUI | 0x568 | 0x14015df00 (11) | `upgradeui_sa::g_vt` upgradeui.cpp:203, Init :286-290 | 0,2,4,8,9 | 0x38 :306 | |
| WorkPlaceUI | 0x568 | 0x14015e030 | `workplaceui_sa::g_vt` workplaceui.cpp:558, Init :1078-1090 | 0,2,4,7,8,9 | 0x68 :1106 | |

Also patching these SAME tables at run time (opt-in, `BANISHED_DLGCTRL_ROUTE`):
standalone/dlgctrl_route.cpp:232-284 walks every registered type whose parent is
"UI::DialogController", constructs one, finds the majority stub pointer in the first 24 slots and
overwrites slots 5/6/10 with counting thunks onto `0x1800cd2a0/290/250`. Any GCC-generated base
table breaks its "majority stub" heuristic; it is an instrument, listed so it is not forgotten.

### 1C. `UI::DialogControllerConfig` subclasses (base = Object::External + nothing; the four files
### call `fn_External_External`, never a UI ctor) — 4 classes, 9-slot tables

| Class | Table | File:line | Retail vtable | Alloc | Base ctor / dtor |
|---|---|---|---|---|---|
| ControlUIConfig | `g_vt[9]` | controluiconfig.cpp:53, VtblInit :119-127 | 0x14015b3e0 (:50) | 0x28 :139 | `fn_External_External` :68 / `fn_External_Dtor` :96 |
| ResourceLimitUIConfig | `g_vt[9]` | resourcelimituiconfig.cpp:59, :109-117 | 0x14015d250 (:56) | 0x28 :129 | :63 / :87 |
| StatusIconUIConfig | `g_StatusIconUIConfig_Vtable[9]` | statusiconuiconfig.cpp:50, :95-103 | 0x14015d900 (:6) | 0x28 :109 | :53 / :76 |
| UpgradeUIConfig | `g_UpgradeUIConfig_Vtable[9]` | upgradeuiconfig.cpp:42, :87-95 | 0x14015df60 (:40) | 0x28 :106 | :45 / :68 |

All nine slots are own bodies (Dtor, ReturnNull, GetTypeId, GetVersion, Return0, Serialize,
Reconstruct, Return0, CreateInPlace). Pins `DialogControllerConfig` = External's 9-slot layout with
NO added virtuals and no added fields (0x20 base, fields at +0x20/+0x24).

### 1D. `UI::Element` subclasses — 2 classes, 1 table

| Class | Table | File:line | Retail vtable | Alloc | How built |
|---|---|---|---|---|---|
| FrustumOutlineElement | `g_FrustumOutlineElement_Vtable[24]` (uintptr_t) | frustumoutlineelement.cpp:47, fill :248-258 | 0x14015c2a0 (Elements.h:21) | 0xD8 :272 | `fn_Element_Ctor` :246 installs the RUNTIME Element vtable; the first construction COPIES its 24 slots into the array and overrides 4 ComputeSize / 5 ComputeLayout / 6 Draw (:252-256); every object is then restamped :260-261. `fn_Sprite_DefaultCtor2` at +0xC0 :263. |
| CaptureBoxElement | **none** — keeps the Runtime `UI::Element` base vtable | captureboxelement.cpp:21-29 | 0x14015d440 (Elements.h:16) never rebuilt (:12-15) | 0xE0 :35 | `fn_Element_Ctor` :23, `fn_Sprite_DefaultCtor2` :25. Divergence recorded :13: GetTypeId reports Element. Note UI::Element is ABSTRACT (frustumoutlineelement.cpp:33-39: slots 4/5/6 are `_purecall`), so a CaptureBoxElement laid out by a group would hit purecall today. |

`ToolbarElement` is NOT a class: `g_TypeId_ToolbarElement` is the type id of `UI::CheckElement`
(toolbar_vtable_standalone.cpp:34, reg_element_typeids.cpp:92).

### 1E. `UI::ElementDescription` subclasses — 2 classes, 11-slot tables

| Class | Table | File:line | Retail vtable | Alloc | Base ctor | Base calls |
|---|---|---|---|---|---|---|
| CaptureBoxDescription | `g_CaptureBoxDescription_Vtable[11]` | captureboxdescription.cpp:87-99, reg :102 | 0x14015d4a0 (:5) | 0x78 :108 | `fn_ElementDescription_Ctor` :32 | Serialize→`fn_ElementDescription_Serialize` :46; GetVersion→`fn_ElementDescription_GetFileVersion` :77; Dtor→`fn_ElementDescription_Dtor` :59; slot 8 CreateElement = ReturnNull |
| FrustumOutlineDescription | `g_FrustumOutlineDescription_Vtable[11]` | frustumoutlinedescription.cpp:147-159, reg :170 | 0x14015c300 (:5) | 0x80 :180 | `fn_ElementDescription_Ctor` :34 | Serialize :49, GetFileVersion :82, Dtor :64, CreateElement→`fn_ElementDescription_CreateInstance` :121 |

Own slots: 0,2,3,5,8(Frustum only),9; slots 1,4,6,7,10 ReturnNull/Return0 stubs — retail inherits
those from ElementDescription/External (the files call them "thunk"). `standalone/reg_element_typeids.cpp`
registers no ElementDescription SUBCLASS: it registers the exe's 21 type-id dispatch slots for the
RUNTIME element classes (kElementTypeSlots :85-110) by `ResolveExport("?_typeInfo@X@UI@@…")`.

### 1F. `UI::SpriteSheet` — no exe subclass, but an exe COPY of the base table
thumb_vtables.cpp:39-99: `Mirror g_sheet` (9 slots) — slots 1..8 copied from the vtable the Runtime
ctor installed on the first constructed object (`Arm` :69-77), slot 0 = own deleting dtor that
calls the copied base slot 0 with flags=0 then frees (:56-61). Stamped by
`ThumbVtable_StampSpriteSheet` from mapui.cpp:115, moddialog.cpp:449, noiseui.cpp:117,
saveloaddialog.cpp:274. Mirrors retail's exe-side duplicate 0x14015c3b0 (:10). (Same file mirrors
Graphics::MaterialInstance 0x14015c400 — not UI.)

---------------------------------------------------------------------------------------------------
## 2. Reconcile the "22 Dialog Controllers in the exe" banner

The banner is in `engine/types/ui/DialogInstance.h:5` and `engine/types/ui/Dialogs.h:5` (the
task prompt said runtime/types/ui/DialogInstance.h; that file has no such line). Dialogs.h lists
exactly 22 `*Addr` namespaces (:22-139) and 22 structs (:148-486), of which **20 embed
`DialogInstance base`** (Dialogs.h:149…452) and **2 are Object::External** (TutorialGoalDialog :435,
TutorialSetupDialog :466). So "22 Dialog Controllers" = 22 exe dialog CLASSES in the header, 20 real
DialogInstance subclasses. The word "Controller" there is the exe's, not `UI::DialogController`.

By my count in the exe tree today:
- **DialogInstance subclasses: 20** classes (the 20 rows of §1A excluding the base-class ToolUI table),
  with **21 hand-built tables** (+ ToolUI child = 22 tables that pin DialogInstance's layout), plus 8
  dormant duplicate `UI_DIALOG_STUB` tables for the same classes.
- **DialogController subclasses: 39** in `kMasks` (dialog_controller_vtable.cpp:76-115),
  **38 tables** (SelectResourceUI has none). `engine/types/ui/DialogController.h:5` says "39 Panel
  Controllers", consistent. `dialog_controller_vtable.h:18` says "38 subclasses that have a standalone
  table" — also consistent.
- DialogControllerConfig subclasses: 4. Element: 2. ElementDescription: 2. SpriteSheet: 0 (1 mirror).

---------------------------------------------------------------------------------------------------
## 3. Slot bodies that call a base member through a door today (must be QUALIFIED when converted)

Only calls to a base VIRTUAL from inside a slot body are subject to the rule (an unqualified
`OnUpdate()` on a converted subclass would re-enter the subclass's own slot). Calls to NON-virtual
base members (`FindElement`, `GetRoot`, `GetController`, `SetFlagsOnOff`, `StartTimer`, …) are not
affected by dispatch, listed here only as the door inventory.

### 3A. Virtual base members called by door — the qualified-call sites

| File | Sites | Base member(s) | Door |
|---|---|---|---|
| mainmenudialog.cpp | 5 | `DialogInstance::~DialogInstance` ×3 (:819 slot 0, :1069 slot 10, :1120 MapParams slot 0), `DialogInstance::OnUpdateLayout` (:915, slot 6), `DialogInstance::OnUpdate` (:936, slot 7 — shared by MapParams :1162) | `fn_DialogInstance_Dtor/_OnUpdateLayout/_OnUpdate` |
| newgamedialog.cpp | 2 | `~DialogInstance` (:1028), `OnUpdate` (:1062 slot 7) | `fn_DialogInstance_Dtor`, `fn_DialogInstance_OnUpdate` |
| moddialog.cpp | 1 | `DialogInstance::OnElementEvent` (:818, inside the ported Slot8 body that `g_ModDialogVt[8]` wraps via mod_dialog_vtables.cpp:180) — **8-arg door; Ghidra-dropped-args hazard already noted tutorial_dialog_vtables.cpp:461** | `fn_DialogInstance_OnElementEvent` |
| mod_dialog_vtables.cpp | 2 | `~DialogInstance` (:143, :191) | `fn_DialogInstance_Dtor` |
| menu_dialog_standalone_vtables.cpp | 2 | `~DialogInstance` (:74, :112) | `fn_DialogInstance_Dtor` |
| autosavedialog.cpp, eventnotifydialog.cpp, eventpopupdialog.cpp, followdialog.cpp, loaderdialog.cpp, messagedialog.cpp, optioninputbinddialog.cpp, optionsdialog.cpp, saveloaddialog.cpp, scenariodialog.cpp, toolbar_vtable_standalone.cpp, tool_base_toolui.cpp | 1 each (12) | `~DialogInstance` in slot 0 | `fn_DialogInstance_Dtor` (autosavedialog:216, eventnotify:968, eventpopup:116, follow:392, loader:84, message:157, optioninputbind:244, options:1686, saveload:1263, scenario:568, toolbar_vtable:78, tool_base_toolui:105) |
| **Total** | **24** door calls onto DialogInstance virtuals: 20 × dtor, 2 × OnUpdate, 1 × OnUpdateLayout, 1 × OnElementEvent | | |

Base virtuals installed RAW (function pointer copied into the table, no C++ call — become inherited
slots automatically after conversion, nothing to qualify):
- toolbar_vtable_standalone.cpp:130-135 slots 1,3,7,9 by `ResolveExport("?TypeName/Persist/OnUpdate/OnSerializeSettings@DialogInstance@UI@@…")`.
- mod_dialog_vtables.cpp:216-230 slots 1,3,5,6,7,9 by the same names (+ `?OnClosed@`, `?OnUpdateLayout@`).
- tool_base_toolui.cpp:142-146 slots 1..9 by copying 0x180156d70.
- dialog_controller_vtable.cpp:44-51 `Apply()` fills DialogController slots 3..10 into 38 tables (166 slots per its own audit, .h:18).
- DialogController subclasses: `fn_DialogController_Dtor` has **0** callers — no DC slot 0 chains to the base dtor (destroyui.cpp:554-559 pattern: reset vtable, free). After conversion the compiler will run `~DialogController` for them — a behaviour CHANGE (base dtor 0x1800cd2f0 runs `~Internal`, which unregisters from the factory list) to plan for, not a qualification.
- TutorialHighlightDialog Dtor skips `~DialogInstance` and calls `fn_ObjectInternal_Dtor` directly (tutorial_dialog_vtables.cpp:126-127, :436-440) — same class of change.

### 3B. Hand-inlined vtable dispatch `(*(void***)p)[N]` onto a UI object inside these files
Narrow pattern (`(*(void***)x)[N]`) in the 20 DI files: saveloaddialog.cpp:187, :381 (slot 8 Clone on a
template object — not a UI base), moddialog.cpp:1, followdialog.cpp:1. **None targets a DialogInstance
/DialogController virtual on `self`.** The exe tree reaches UI virtuals through the `fn_*` doors or
`kControllerVtbl*` offsets in runtime code, not by inlined slot arithmetic. (A broad `+ 0xNN))(`
regex matched 23/18/15 lines in saveloaddialog/toolbardialog/tradeui — those are exe .rdata dispatch
SLOTS, i.e. non-virtual imports, not vtables.)

### 3C. Non-virtual base members reached by door from slot bodies (no qualification needed; listed
### because they become ordinary member calls and the door typedefs die)
Counts of `fn_DialogInstance_*`/`fn_DialogController_*`/`fn_Element_*`/`fn_SpriteSheet_*`/
`fn_ElementDescription_*` tokens per file (any member): tradeui.cpp 57, mainmenudialog.cpp 39,
saveloaddialog.cpp 34, townhallui.cpp 30, destroyui.cpp 27, optionsdialog.cpp 26, moddialog.cpp 23,
tutorial_wave.cpp 22, toolbardialog.cpp 22, eventnotifydialog.cpp 20, workplaceui.cpp 17,
professionui.cpp 17, statusbarui.cpp 14, tool_base_createbar.cpp 13, toolui.cpp 13,
newgamedialog.cpp 13, mapparamsdialog.cpp 13, residenceui.cpp 12, … (77 files total, full list
from `grep -rc` in the run log). Members named: DialogInstance::{Ctor, FindElement, FindElementType,
GetController, GetRoot, SetFlagsOnOff, SetUpperLeft, AddController(6-arg), AddControllerTab,
StartTimer, StopTimer (autosavedialog.cpp:47-49 by name)}; DialogController::GetContainer;
Element::{GetUserData, SetUserData, SetFlagsOnOff, SetElementId, SetName, SetToolTip,
SetToolTipString, Child, ChildCount, FindElement, FindElementType, RemoveChildren, AddChild, Hide,
SetPadding, GetPaddedSize, SetNeedsSizeCalc, Ctor, ComputeLayout(virtual, frustumoutlineelement.cpp:80
— base call from an OVERRIDE of the same slot: **must be qualified** `Element::ComputeLayout`),
Draw(virtual, :191 — same: **qualify** `Element::Draw`), GetAlignedPosition (by name :84)};
SpriteSheet::{Ctor, AddSpriteInfo, SetSpriteInfo, GetSpriteIndexFromName, GetSpriteCount (by name
statusiconcomponent.cpp:75)}; ElementDescription::{Ctor, Dtor, Serialize(virtual — CaptureBox/
FrustumOutline Serialize slot calls base Serialize: **qualify**), GetFileVersion(virtual — same:
**qualify**), CreateInstance}.

So the full qualified-call list = §3A's 24 DialogInstance sites + frustumoutlineelement.cpp:80,:191
+ captureboxdescription.cpp:46,:77 + frustumoutlinedescription.cpp:49,:82 = **30 sites**.

---------------------------------------------------------------------------------------------------
## 4. Exe-side code that READS or COPIES a Runtime UI vtable's slots

| Site | What it copies | Breaks how under a GCC-generated base table |
|---|---|---|
| tool_base_toolui.cpp:142-146 `ToolUI_InitChildDialogVtable` | slots 1..9 of `??_7DialogInstance@UI@@6B@` (0x180156d70) into `g_ToolUI_ChildDialog_Vtable_Standalone`; slot 0 stays own | Resolves by ADDRESS. A GCC vtable is not at 0x180156d70; `ResolveAddr(Module::Runtime, …)` must map that address to the new table's slot-0 address (the address-point, i.e. `&vtable + 16` past offset-to-top/RTTI), or the copy reads garbage. Slot identity 1..9 is preserved only if the member order in the C++ class is exactly TypeName, Type, Persist, OnOpened, OnClosed, OnUpdateLayout, OnUpdate, OnElementEvent, OnSerializeSettings after the dtor. GCC emits TWO dtor slots (complete + deleting) where MSVC has one scalar-deleting slot 0 → every index shifts by one unless the class is laid out for the MSVC ABI. |
| frustumoutlineelement.cpp:248-258 | 24 slots of the table `fn_Element_Ctor` installed (i.e. `??_7Element@UI@@6B@` 0x180156160, or the promoted copy) | Reads THROUGH the constructed object, so it follows whatever ctor stamps — safe w.r.t. address, but overrides slots 4/5/6 by index: same dtor-slot-count hazard; also copies 24 entries from a 12-slot table (reg_mint_promoted.cpp:424 says slots=12) — reads past the end of a GCC table into whatever follows. |
| thumb_vtables.cpp:69-77 `Arm` | slots 1..8 of the vtable on the first constructed `UI::SpriteSheet` (`??_7SpriteSheet@UI@@6B@`, 9 slots), slot 0 called with flags=0 as "run ~T only" | Through the object, address-safe. Assumes slot 0 is a scalar-deleting dtor taking `(self, flags)` — GCC's slot 0 is the complete dtor `(self)` and slot 1 the deleting dtor; calling it with an extra arg is harmless, but `flags=0` no longer means "don't free" — the deleting variant WOULD free. Must be rewritten as `sheet->~SpriteSheet()`. |
| dialog_controller_vtable.cpp:44-51, :156-165 `Apply()` | Not a copy from a table: resolves the 7 DC bodies by DLL address `0x1800cd2b0..250` and Persist by name, writes them into 38 exe tables at fixed indices 3..10 | Address resolution goes through the port registry (comment :22-28) so it survives as long as those 7 registrations stay; the slot INDICES 3..10 are MSVC's. Under GCC the exe tables stay hand-built (they are exe-phase), so the indices in the exe tables are whatever the exe tables say — but the bodies must remain addressable by those 7 addresses or by name. |
| dlgctrl_route.cpp:232-284 | Reads slots 0..23 of each DC table to find the majority stub | Instrument only; disabled by default. |
| toolbar_vtable_standalone.cpp:130-135, mod_dialog_vtables.cpp:216-230 | Not table reads: `ResolveExport` of 4/6 DialogInstance virtuals BY MANGLED NAME | Survive as long as those names keep resolving to the (ported) bodies. |
| engine_funcs.cpp:2024, :2300 | `ResolveAddr(Module::Exe, 0x14015f5a0)` into `g_ToolUI_ChildDialog_Vtable` — an EXE table, not Runtime | unaffected |

Also relevant: `runtime/src/object/deleting_dtor.cpp:351-373` says `??_7ToolTipDialog@UI@@6B@`'s
slots 1..9 are compared BY ADDRESS against 0x180156d70's — same coupling inside runtime/.

---------------------------------------------------------------------------------------------------
## 5. Element / ElementDescription subclasses the exe defines
Covered in §1D/§1E: Elements = {FrustumOutlineElement (table), CaptureBoxElement (no table)};
ElementDescriptions = {CaptureBoxDescription, FrustumOutlineDescription}. REGISTER_TYPE parents are
the SHORT names "Element"/"ElementDescription" (captureboxelement.cpp:38, frustumoutlineelement.cpp:274,
captureboxdescription.cpp:120, frustumoutlinedescription.cpp:188), whereas dialog classes register
under "UI::DialogInstance"/"UI::DialogController" — the type tree spelling is inconsistent
(debugui.cpp:1762 uses bare "DialogController"). `engine/types/ui/Elements.h:5` says "2 Element
classes registered in the exe", matches. `standalone/reg_element_typeids.cpp` registers ZERO
subclasses — 21 Runtime element type-id slots only (:85-110).

---------------------------------------------------------------------------------------------------
## 6. Verdict table — may the Runtime base be ABSTRACT?

"By value" means a C++ member/local of the class type (not a byte blob, not `_base` embedding in a
struct that will BECOME the derived class). Exe-tree evidence first, runtime/ evidence second.

| Runtime class | Held by value anywhere? | Exe subclass construct the base as a C++ object? | Verdict |
|---|---|---|---|
| `UI::DialogInstance` | exe: **no** — every exe struct in engine/types/ui/Dialogs.h embeds the POD `DialogInstance base` (Dialogs.h:149…452) which becomes inheritance; standalone/mouse_inject.cpp:392,429,512,730 holds `RtUI::DialogInstance*` pointers only. runtime: `ComboDialog::_base` (runtime/types/ui/DialogInstance.h:320) embedding → inheritance. BUT the ToolUI child dialog **constructs a bare `UI::DialogInstance` object** (tool_base_toolui.cpp:220-231: MemAlloc(0xB8) + base ctor + restamp with a table that adds no behaviour) and retail's `UserInterface::BeginToolTip` allocates a `ToolTipDialog` (runtime DialogInstance.h:27-33). | Yes for ToolUI (the object IS a DialogInstance whose vtable is swapped to a no-behaviour copy) | **CONCRETE** (protected ctor + friend not required: exe code constructs it through `fn_DialogInstance_Ctor` today and will `new UI::DialogInstance(...)` after). Slot 0 may not be `= 0`. The 20 dtor-only exe subclass tables (§1A) become trivial derived classes. |
| `UI::DialogController` | exe: no by-value; Panels.h embeds POD `DialogController base` ×39 (Panels.h:187…816). runtime: none (runtime DialogInstance.h:172-175 "SIZE IS DELIBERATELY NOT ASSERTED … nothing allocates one"). | **No** — no exe site runs the base ctor (§1B) and `AddController` always constructs a DERIVED class via the factory (DialogInstance.h:157-162). | **MAY BE ABSTRACT** (pure slot 1 `TypeName` or slot 2 `Type` is the natural `= 0`; slot 0 dtor cannot be pure). But the 38 hand-built tables + `Apply()` need the 7 inherited bodies (OnCreated…OnSerializeSettings) to stay real, exported, addressable functions — abstract ≠ empty. |
| `UI::DialogControllerConfig` | exe: no; 4 subclasses call `fn_External_External` and build 9-slot tables (§1C). runtime: only `ExternalPtr<DialogControllerConfig>` (Dialog.h:117). | No — never constructed bare. | **MAY BE ABSTRACT**; it adds no virtuals to External, so "abstract" would mean re-purifying an External slot; simplest is concrete-with-no-members. |
| `UI::Element` | exe: no by-value (`uint8_t elementBase[0xB8]` blobs, Elements.h:29/42). runtime: `Element base` embedding in ComboElement/EditElement/NumberElement/ProgressElement/RadioElement/SliderElement/SpinnerElement (runtime/types/ui/*.h) → inheritance. `runtime/src/application/message_callbacks.cpp:199 Element elem;` is a DIFFERENT `Element` (the callback Set's node payload), not UI::Element. | CaptureBoxElement constructs a bare Element (captureboxelement.cpp:23) and KEEPS its vtable — but retail's Element is already abstract (`_purecall` in slots 4/5/6, frustumoutlineelement.cpp:33-39; reg_mint_promoted.cpp:424 "purecall= 1"), so that object is a latent R6025 today. | **ABSTRACT already** (retail fact). Keep it abstract; CaptureBoxElement must get its real table (0x14015d440) or the conversion makes the purecall a compile error, which is the correct outcome. |
| `UI::ElementDescription` | exe: no by-value (`uint8_t _elemBase[0x60]` blobs, engine/types/Descriptions.h:315/330). runtime: `_base` embedding ×8 (DescriptionsBatch1.h:68…218, GroupDescription.h:46) → inheritance. | No — both exe subclasses run the base ctor then stamp their own 11-slot table (§1E). | **MAY BE ABSTRACT** (CreateElement slot 8 is the natural pure slot — CaptureBoxDescription stubs it ReturnNull, so making it pure forces that file to implement it). |
| `UI::SpriteSheet` | exe: **held by hand-allocated 0x58 blobs only** (thumb_vtables.cpp:5-6, four dialogs); runtime: `runtime/types/save/SaveLoad.h:210,322` hold `Sprite` by value (UI::Sprite, not SpriteSheet). | thumb_vtables.cpp constructs a bare SpriteSheet via the Runtime ctor then restamps a COPY of its own table (§1F) — a concrete object. | **CONCRETE** (retail: 9 slots, 0 purecall, reg_mint_promoted.cpp:335). |
| `UI::Sprite`, `UI::SpriteContext`, `UI::String`, `UI::UserInterface` (not in the ask, but they decide by-value) | `UI::UserInterface` by value ×2 in runtime/types/engine/UpdateLoop.h:207,230 and as `uint8_t m_userInterface[0x128]` in engine/types/DialogLoopT.h:44 / DialogLoaderLoopT.h:49; `Sprite` by value SaveLoad.h:210,322 and in-place `fn_Sprite_DefaultCtor` saveloaddialog.cpp:1319, `fn_Sprite_DefaultCtor2` at +0xC0 in both Elements; `UI::SpriteContext sc` on the stack runtime/src/save/saveload_draw.cpp:734; `uint8_t uiStr[24]` UI::String scratch gameupdateloop.cpp:2756. | — | All four **CONCRETE**, complete types (they are stack/aggregate members). |

Bottom line for the ask: **only DialogController, DialogControllerConfig and ElementDescription may
go abstract; DialogInstance and SpriteSheet must stay concrete (bare instances are constructed by
the exe tree); Element is abstract already.**

---------------------------------------------------------------------------------------------------
## 7. Counts (for the reply)
- DialogInstance subclasses in exe tree: **20** (+ 1 base-class table for the ToolUI child dialog; 22 tables total; 8 further dormant `UI_DIALOG_STUB` duplicates). Header banner "22" counts 2 External classes.
- DialogController subclasses: **39** masks / **38** tables (SelectResourceUI none). All 38 skip the base ctor and base dtor.
- DialogControllerConfig subclasses: 4 (9-slot tables, External ctor).
- Element subclasses: 2 (1 table, 1 keeps the abstract base table). ElementDescription subclasses: 2 (11-slot tables).
- Exe sites that copy Runtime UI vtable slots: 3 (tool_base_toolui 0x180156d70 slots 1-9; frustumoutlineelement 24 slots through the object; thumb_vtables SpriteSheet slots 1-8 through the object) + 1 that resolves 7 DC bodies by DLL address into 38 tables (dialog_controller_vtable.cpp).
- Qualified-call sites after conversion: **30** (24 DialogInstance virtuals — 20 dtor, 2 OnUpdate, 1 OnUpdateLayout, 1 OnElementEvent — plus Element::ComputeLayout/Draw ×2 and ElementDescription::Serialize/GetFileVersion ×4).
- Behaviour changes conversion will introduce on its own: 38 DC subclasses and TutorialHighlightDialog will start running the base dtor they skip today.
