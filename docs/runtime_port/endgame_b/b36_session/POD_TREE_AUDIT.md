# B.3 step 6 — duplicate POD tree audit (read-only, 2026-09-25)

Scope: `runtime/types/ui/*.h` (29 headers) vs the engine-side mirrors
`engine/types/ui/{DialogController,DialogInstance,Dialogs,Elements,Panels}.h`,
`engine/types/{Descriptions,DialogLoaderLoopT,DialogLoopT}.h`. Greps scoped to
`code/ engine/ standalone/ runtime/ dx11/ tests/`, excluding `*/build*`, `.claude/worktrees/`.
Header comments are not evidence; `static_assert`s and field declarations are what the tree
believes. "AGREE"/"DISAGREE" below is about what the two headers assert, not about retail.

Two things the brief did not list were found and are load-bearing:

* **A THIRD engine-side dialog tree exists**: `engine/types/Dialogs.h` (not `ui/`), 12 structs
  with the SAME global names as `engine/types/ui/Dialogs.h` and different member names
  (`engine/types/Dialogs.h:15-90`). Zero includers. Pure ODR landmine.
* **A live cross-TU ODR violation**: `::TutorialSetupDialog` is defined twice with different
  members — `engine/types/ui/Dialogs.h:468-487` (`data20…data70`) and
  `engine/types/TutorialSetups.h:90-107` (`_field20…_field70`). Both are compiled into the exe
  (§3.3). No fix applied (read-only).

---

## 1. Runtime `UI::` classes duplicated in the engine tree — field-by-field

### 1.1 `UI::DialogController`

| | engine `engine/types/ui/DialogController.h` | runtime `runtime/types/ui/DialogInstance.h` |
|---|---|---|
| name / namespace | `struct DialogController` global, l.17 | `RtUI::DialogController`, l.184 |
| +0x00 | `void* vtable`, `int32 internalIndex`@8, `uint32 randomId`@0xC, l.18-20 | `RtObject::Internal _base` (0x10), l.185 |
| +0x10 | `uint64 data10` "zeroed", l.21 | `void* _config` (ExternalPtr<DialogControllerConfig>), l.186 |
| +0x18 | `uint64 data18` "zeroed", l.22 | `void* _dialog` (DialogInstance*), l.187 |
| +0x20 | `uint64 data20` "zeroed", l.23 | `void* _container` (InternalPtr<Element>), l.188 |
| +0x28 | `uint64 data28`, l.24 | `void* _dataSource` (Object::Base*), l.189 |
| sizeof | **`static_assert(sizeof==0x30)`**, l.27 | **deliberately NOT asserted**, l.171-173 ("nothing in this lane allocates one") |
| offset asserts | vtable 0, internalIndex 8, data10 0x10, l.28-30 | `_config` 0x10 … `_dataSource` 0x28, l.191-194 |

Offsets AGREE (four qwords at 0x10..0x28). Names: every field differs. Size: engine asserts
0x30, runtime refuses. **The engine tree already holds the evidence the runtime header says it
lacks**: all 38 `Panels.h` subclasses place their first own member at +0x30
(`Panels.h:188,197,211,220,229,…,817`), and MSVC never places derived members inside a base's
tail padding, so `sizeof(UI::DialogController) == 0x30` exactly (base ctor extent 0x30 is the
lower bound, subclass start is the upper bound). Step 6 can add the assert with that citation.

Vtable slot offsets: runtime pins `+0x10 GetTypeId, +0x38 OnUpdateLayout, +0x40 OnUpdate,
+0x48 OnElementEvent` (`DialogInstance.h:196-201`). The engine tree has no slot table in the
header; `code/Game/Dialog/dialog_controller_vtable.h:42,48-49` speaks of "slot 8 `OnUpdate`"
and slots 0/1/2 (slot 8 = +0x40, consistent). Not a disagreement.

Header-comment addresses (NOT evidence, listed as a flag only): engine names ctor 0x1800cd320 /
dtor 0x1800cd2f0 (`DialogController.h:15-16`); runtime cites no DialogController ctor at all.

### 1.2 `UI::DialogInstance`

Engine `engine/types/ui/DialogInstance.h:47-81` (global `struct DialogInstance`) vs runtime
`runtime/types/ui/DialogInstance.h:227-279` (`RtUI::DialogInstance`). Both assert
**sizeof == 0xB8** (`engine …:77`, `runtime …:276`). Every engine field is a placeholder
`dataNN`; the comparison is about widths and the init claims each header makes.

| off | engine (l.) | runtime (l.) | verdict |
|---|---|---|---|
| 0x00-0x0F | vtable/internalIndex/randomId, 48-50 | `RtObject::Internal _base`, 228 | agree |
| 0x10 | `data10` "zeroed", 51 | `_ui` = ctor param 1, 229 | **DISAGREE on init** (a param, not zero) |
| 0x18 | `data18` "zeroed", 52 | `_dialog` = ctor param 2, 230 | **DISAGREE on init** |
| 0x20 | `uint64 data20` "zeroed", 53 | `uint32 _flags` + `uint32 _flags2` (modal result), 231-232 | width split; init agrees for 0x24, 0x20 is seeded from `_dialog->+0x40` |
| 0x28 | `data28` "zeroed", 54 | `_parent` = ctor param 3, 233 | **DISAGREE on init** |
| 0x30 | `data30` "**not initialized by ctor**", 55 | `_dataSource` = **ctor param 4**, 234 | **DISAGREE** — one says unwritten, one says written |
| 0x38/0x40/0x48 | `data38`,`data40`,`uint32 data48`,`_pad4C`, 56-59 | `LinkedList _children` head/tail/count(+pad), 235 | agree (shape incl. the 0x4C hole) |
| 0x50 | `data50` "ptr, cleaned in dtor via vcall", 60 | `_root` (Element*), 236 | agree |
| 0x58 | `data58`, 61 | `_spriteSheet`, 237 | agree |
| 0x60 | `int32 data60` = -1, 62 | `int32 _spriteIndex` (index or -1), 238 | width agrees; engine over-claims "always -1" |
| 0x64 | `_pad64`, 63 | `_hole64` "NOT written", 239 | agree |
| 0x68/0x70 | `data68` array ptr, `data70` "count/capacity", 64-65 | `Array<DialogTimer> _timers`, 240 | agree |
| 0x78/0x80 | `data78` array ptr, `data80` count, 66-67 | `Array<DialogController*> _controllers`, 241 | agree |
| 0x88 | `uint64 data88` "zeroed", 68 | `int32 _upperLeftX`,`int32 _upperLeftY`, 242-243 | width split, bytes agree |
| 0x90 | `int64 data90` = -1, 69 | `int32 _hoverMouseX`=-1, `int32 _hoverMouseY`=-1, 244-245 | width split, bytes agree |
| 0x98 | `uint8 data98` = 0 + `_pad99[7]`, 70-71 | `uint8 _hovering` + `_hole99[7]`, 246-248 | agree |
| 0xA0 | `dataA0` "zeroed", 72 | `_hoverTime` int64, 249 | agree |
| 0xA8 | `dataA8` "zeroed", 73 | `_userData` = **ctor param 5**, 251 | **DISAGREE on init** |
| 0xB0 | **`_padB0[8]` "gap to reach 0xB8"**, 74 | **`int32 _userFlags` = ctor param 6** + `_holeB4`, 252-253 | **DISAGREE** — engine calls a ctor-written field padding |

Offset asserts: engine has vtable/internalIndex/data60/data90 (l.78-81); runtime asserts every
member (l.256-275). Runtime's banner (l.34-36) says 0xC0 is only an upper bound from
`ToolTipDialog::operator new(0xc0)`; the engine tree resolves that too: 20 `Dialogs.h` subclasses
start their own members at +0xB8 (`Dialogs.h:150,161,172,189,197,…,453`), so
`sizeof(UI::DialogInstance) == 0xB8` exactly and `ToolTipDialog` adds one qword.

Header-comment addresses, flagged only: engine ctor 0x180114ca0 / dtor 0x180114d40
(`engine …:45-46`); runtime pins its layout from ctor 0x180114870 (`runtime …:63`) and names a
second ctor overload 0x1801146b0 (`runtime …:299`). 0x180114ca0 appears nowhere in the runtime
header — resolve in Ghidra before any address is carried into the class.

### 1.3 `UI::Element` prefix

Engine `Elements.h:109-110,122-123`: `void* vtable; uint8_t elementBase[0xB8];` = 0xC0.
Runtime `Element.h:125-232`: full layout, `static_assert(sizeof(Element)==0xC0)` (l.232).
AGREE on size; engine has no field knowledge. Sprite at +0xC0 in both engine elements
(`Elements.h:116,128`).

The SAME opaque-prefix pattern exists INSIDE the runtime tree: `ImageElement.h:45-47`
(`vtable` + `elementBase[0xB8]`, bare `struct ImageElement`, sizeof 0xE0 l.68) and
`LabelElement.h:59-60` (bare `struct LabelElement`, sizeof 0x130 l.93) do not embed `UI::Element`
and cannot be `UI::Element` subclasses as written. They are intra-runtime duplicates of the
Element prefix.

### 1.4 `UI::Sprite`

Three definitions, all 0x10, same offsets, different member names:

| definition | fields | sizeof assert |
|---|---|---|
| bare `::Sprite`, `runtime/types/ui/Sprite.h:57-74` | `spriteSheet`@0, `spriteIndex`@8, `pad0C` | l.74 |
| `UI::Sprite`, `runtime/types/ui/ImageElement.h:32-37` | `sheet`@0, `index`@8, `pad0C` | l.37 |
| `RtSave::Sprite`, `runtime/types/save/SaveLoad.h:59-65` | `sheet`@0, `index`@8, `pad0c` | l.65 |

Engine-side embeddings: `Elements.h:111,124` `uint8_t sprite[0x10]` @0xC0 — AGREE.
**`Dialogs.h:379` `uint8_t sprite[0x20]` @0xD0 "UI::Sprite sub-object" — DISAGREES with 0x10.**
`code/Game/Dialog/saveloaddialog.cpp:1305` says "+0xD0 sprite default-ctor, loader/engine ptrs at
+0xE0/+0xE8", so the 0x20 blob is Sprite + two pointers mislabelled as one sub-object.

### 1.5 `UI::ElementDescription` prefix

Engine `Descriptions.h:314-315,329-330`: `void* vtable; uint8_t _elemBase[0x60];` = 0x68.
Runtime `ElementDescription.h:117-171`: full layout, `static_assert(sizeof==0x68)` (l.171).
AGREE on size. Engine has no field knowledge.

### 1.6 `UI::UserInterface` (embedded in the loop templates)

Engine `DialogLoaderLoopT.h:178` `uint8_t m_userInterface[0x128]` @0x60; `DialogLoopT.h:230`
@0x40. Runtime `UserInterface.h:276` `sizeof==0x128`; runtime `engine/UpdateLoop.h:210,236`
`UI::UserInterface _ui` @0x40 / @0x60. AGREE on size and offset.
Engine sub-offset comments (`DialogLoaderLoopT.h:173-177`): "+0x28 pending-open" =
`_addDialogs`@0x28 (agree); "**+0x38 pending-close**" — runtime has `_removedDialogs`@0x18 and
`_toolTipOwner`@0x38 (`UserInterface.h:198,201`): comment DISAGREES; "+0xDC scale" = `_uiScale`
(agree).

### 1.7 Classes with NO engine-side twin

`UI::Dialog` (`Dialog.h:138`), `UI::SpriteSheet`/`SpriteInfo` (`SpriteSheet.h:82,160`),
`UI::String` (`String.h:65`), `UI::StringTable` (`StringTable.h:57,95`), `UI::Font`, `ComboDialog`,
all 10 `DescriptionsBatch1.h` bodies, `GroupDescription`, every `*Element` leaf — grep of
`struct <Name>` across the tree finds exactly one definition each outside `runtime/types/ui`
(§3.5 grep). No engine duplicate.

### 1.8 Intra-runtime disagreements found on the way (not engine-vs-runtime, still step-6 input)

* **`FontGlyph` twice, same name, different semantics**: `RtUI::FontGlyph` `Font.h:77-83`
  (`_preAdvance,_postKern,_postAdvance,_offsetY`) vs `UI::FontGlyph` `SpriteContext.h:310-316`
  (`spriteIndex,x,y,_unread0C`). Both 0x10. These read like two different retail types (glyph
  metrics vs laid-out glyph); when `RtUI` folds into `UI` they collide by name.
* **`LabelElement.h` struct contradicts its own header and `TextElement.h`**: fields
  `textStringC0/textDataC8/fontPtrD0/fieldD8/textColorE0` (`LabelElement.h:62-66`) are the layout
  `LabelElement.h:16-27` marks "WRONG, see below"; `TextElement.h` pins `_params.data`@0xC8,
  `.size`@0xD0, `UI::String _text`@0xD8 (`TextElementOffsets`, `TextElement.h:74-78`). The struct
  was never re-cut to the corrected layout; 20 `static_assert`s (`LabelElement.h:94-112`) pin the
  wrong names at the right offsets.
* `Dialogs.h:23` `AchievementDialogAddr::SecondaryVtable = 0x14015b070` "DialogController
  secondary vtable" is the SAME address `Panels.h:22` gives as `AttendanceUIAddr::Vtable`. Two
  headers, one address, two claims. Either AchievementDialog is multiply-inherited
  (DialogInstance + DialogController) and shares a vtable with AttendanceUI (unlikely), or one
  annotation is wrong. Must be settled in Ghidra before AchievementDialog becomes a single-base
  subclass. `Dialogs.h:81` `ModDialogAddr::SecondaryVtable` "(ImageBuffer secondary interface)" is
  a second multiple-inheritance hint.

---

## 2. Exe-side structs — which Runtime base each embeds (from fields and size, not from comments)

### 2.1 `engine/types/ui/Panels.h` — 38 structs, 38 `*Addr` namespaces (l.20-179; `SelectResourceUI`
is abstract with no struct, l.531-533). **Every struct's first member is `DialogController base`
(0x30) and its first own field is at +0x30** → all 38 are `UI::DialogController` subclasses.

| struct | l. | sizeof | own fields start | notes |
|---|---|---|---|---|
| AttendanceUI | 186 | 0x38 | 0x30 | |
| BuildRequirementUI | 195 | 0x60 | 0x30 | |
| BuildUI | 209 | 0x50 | 0x30 | |
| CemeteryUI | 219 | 0x38 | 0x30 | |
| CitizenUI | 227 | 0xE8 | 0x30 | |
| ConsumeProduceUI | 258 | 0x40 | 0x30 | |
| ControlUI | 266 | 0x40 | 0x30 | |
| CropUI | 276 | 0x60 | 0x30 | extends SelectResourceUI (abstract), l.273-274 |
| DebugUI | 292 | 0x130 | 0x30 | named fields |
| DestroyUI | 341 | 0x78 | 0x30 | |
| EntityUI | 356 | 0x48 | 0x30 | |
| EventLogUI | 366 | 0x80 | 0x30 | |
| HelpUI | 383 | 0x78 | 0x30 | |
| HotKeyToolTipUI | 400 | 0x30 | — | base only |
| ImmigrationUI | 406 | 0x50 | 0x30 | |
| MapUI | 416 | 0x50 | 0x30 | |
| ModBrowseUI | 427 | 0x30 | — | base only |
| NoiseUI | 434 | 0x70 | 0x38 (+0x30 uninit) | |
| OrchardUI | 449 | 0x48 | 0x30 | extends SelectResourceUI, l.447-448 |
| PageToggleUI | 459 | 0x48 | 0x30 | |
| PastureUI | 470 | 0x40 | 0x30 | extends SelectResourceUI, l.468-469 |
| ProductionUI | 479 | 0x50 | 0x30 | 2×0x10 vector items |
| ProfessionUI | 487 | 0x78 | 0x30 | |
| RadiusUI | 507 | 0x38 | 0x30 | |
| ResidenceUI | 516 | 0x40 | 0x30 | |
| ResourceLimitUI | 524 | 0x40 | 0x30 | |
| SpriteCaptureUI | 543 | 0x70 | 0x30 | named fields |
| StatusBarUI | 569 | 0x1D0 | 0x30 | named fields |
| StatusIconUI | 617 | 0x40 | 0x30 | |
| StorageCapacityUI | 625 | 0x38 | 0x30 | |
| StorageUI | 632 | 0x40 | 0x30 | |
| TitleDescToolTipUI | 641 | 0x30 | — | base only |
| ToolUI | 648 | 0x58 | 0x30 | named fields |
| TownHallUI | 677 | 0x1E8 | 0x30 | named fields |
| TradeUI | 752 | 0x148 | 0x30 | |
| TutorialUI | 795 | 0x60 | 0x30 | |
| UpgradeUI | 807 | 0x38 | 0x30 | |
| WorkPlaceUI | 815 | 0x68 | 0x30 | |

Three-level chain to model: `DialogController → SelectResourceUI (abstract) → {CropUI, OrchardUI,
PastureUI}`; all three place `InternalBasePtr` at +0x30 (`Panels.h:278,451,472`), consistent with
that being SelectResourceUI's one member.

### 2.2 `engine/types/ui/Dialogs.h` — 22 `*Addr` namespaces (l.20-140), 22 structs

| struct | l. | sizeof | first member | base |
|---|---|---|---|---|
| AchievementDialog | 148 | 0xD0 | `DialogInstance base` | DialogInstance (+ see §1.8 secondary vtable) |
| AutoSaveDialog | 159 | 0xD0 | `DialogInstance base` | DialogInstance |
| EventNotifyDialog | 170 | 0xF0 | `DialogInstance base` | DialogInstance |
| EventPopupDialog | 187 | 0xC0 | `DialogInstance base` | DialogInstance |
| FollowDialog | 195 | 0xD8 | `DialogInstance base` | DialogInstance |
| GoalDialog | 207 | 0xC8 | `DialogInstance base` | DialogInstance |
| LoaderDialog | 217 | 0xC8 | `DialogInstance base` | DialogInstance |
| MainMenuDialog | 228 | 0xC8 | `DialogInstance base` | DialogInstance |
| MapParamsDialog | 237 | 0xF8 | `DialogInstance base` | DialogInstance |
| MessageDialog | 253 | 0xC0 | `DialogInstance base` | DialogInstance |
| ModDetailsDialog | 261 | 0xC8 | `DialogInstance base` | DialogInstance |
| ModDialog | 270 | 0xD8 | `DialogInstance base` | DialogInstance (secondary vtable hint l.81) |
| ModUploadDialog | 282 | 0xB8 | `DialogInstance base` | DialogInstance, no own fields |
| NewGameDialog | 291 | 0xAE8 | `DialogInstance base` | DialogInstance; `System::Random` blob @0x118 l.305 |
| OptionsDialog | 318 | 0x1C0 | `DialogInstance base` | DialogInstance |
| OptionInputBindDialog | 365 | 0xC8 | `DialogInstance base` + `dataB8` "from MessageDialog" | **MessageDialog** (l.363), i.e. DialogInstance grandchild |
| SaveLoadDialog | 376 | 0x160 | `DialogInstance base` | DialogInstance; `sprite[0x20]` mislabel §1.4 |
| ScenarioDialog | 400 | 0xE8 | `DialogInstance base` | DialogInstance |
| ToolbarDialog | 416 | 0x108 | `DialogInstance base` | DialogInstance |
| **TutorialGoalDialog** | 437 | 0x40 | `void* vtable; uint8_t externalBase[0x18]` | **Object::External**, NOT DialogInstance (l.435) |
| TutorialHighlightDialog | 451 | 0xF0 | `DialogInstance base` | DialogInstance |
| **TutorialSetupDialog** | 468 | 0x78 | `void* vtable; uint8_t externalBase[0x18]` | **Object::External**, NOT DialogInstance (l.466) |

So the file's banner "All 22 Dialog Controller classes that extend UI::DialogInstance" (l.5) is
20; `runtime/types/ui/DialogInstance.h:11` and the UI_INVENTORY §4 count of 22 inherit the same
over-count.

### 2.3 `engine/types/ui/Elements.h`
`CaptureBoxElement` 0xE0 (l.108-117) and `FrustumOutlineElement` 0xD8 (l.121-129): `vtable` +
`elementBase[0xB8]` = `UI::Element` (0xC0), `sprite[0x10]` @0xC0 = `UI::Sprite`, then a tail.
→ `UI::Element` subclasses that embed a `UI::Sprite`. **Zero includers** (§3.1).

### 2.4 `engine/types/Descriptions.h` — 15 structs, 15 `*Addr` namespaces
Only two are UI: `CaptureBoxDescription` 0x78 (l.313-319) and `FrustumOutlineDescription` 0x80
(l.328-335), both `vtable + _elemBase[0x60]` = `UI::ElementDescription` (0x68) + tail. The other 13
(`CreatePathDescription` … `EnvironmentDescription`, l.30-300) are `vtable + _extBase[0x18]` =
`Object::External` (0x20); not UI. `ToolbarDescription` (l.222-236) is 0x68 — the same size as
`UI::ElementDescription` — but is declared External-based; not checked here, flag for the block
that owns toolbars (`code/Game/Component/toolbardescription.cpp`). **Zero includers** (§3.1).

### 2.5 `engine/types/DialogLoaderLoopT.h`, `DialogLoopT.h` — see §5.

---

## 3. Include graph

### 3.1 Who includes each engine-side header (direct)

| header | direct includers |
|---|---|
| `engine/types/ui/DialogController.h` | `engine/types/ui/Panels.h:14`, `code/Game/Dialog/ui_panels.h:6` |
| `engine/types/ui/DialogInstance.h` | `engine/types/ui/Dialogs.h:14` only |
| `engine/types/ui/Dialogs.h` | `code/Game/Dialog/ui_dialogs.h:5`, `code/Game/Dialog/moddialog.cpp:39`, `optioninputbinddialog.cpp:234`, `saveloaddialog.cpp:44`, `standalone/modal_driver.cpp:109` |
| `engine/types/ui/Panels.h` | `code/Game/Dialog/ui_panels.h:5`, `spritecaptureui.cpp:16`, `townhallui.cpp:16` |
| `engine/types/ui/Elements.h` | **none** |
| `engine/types/Descriptions.h` | **none** |
| `engine/types/DialogLoaderLoopT.h` | `code/Game/Game/dialogloaderloop.h:2`, `standalone/splash_loop_stubs.cpp:30` |
| `engine/types/DialogLoopT.h` | **none** (`standalone/menudialogloopt_stubs.cpp:273,322` reads the loop with raw `self+0x30/+0x38` instead) |
| `engine/types/Dialogs.h` (non-ui, third tree) | **none** |
| `engine/types/TutorialSetups.h` | `code/Game/Tutorial/tutorialsetup{,camera,gamespeed,restrictplayer}_funcs.h:5` → `tutorialsetup.cpp:15`, `tutorialsetupcamera.cpp:20`, `tutorialsetupgamespeed.cpp:17`, `tutorialsetuprestrictplayer.cpp:22` |

### 3.2 Transitive fan-out of the two wrapper headers (the real blast radius)

`code/Game/Dialog/ui_dialogs.h` (→ `ui/Dialogs.h` → `ui/DialogInstance.h`): 24 TUs —
`code/Game/Dialog/{achievementdialog,autosavedialog,eventnotifydialog,eventpopupdialog,followdialog,
goaldialog,loaderdialog,mainmenudialog,mapparamsdialog,menu_dialog_standalone_vtables,messagedialog,
moddetailsdialog,moddialog,mod_dialog_vtables,newgamedialog,optioninputbinddialog,optionsdialog,
saveloaddialog,scenariodialog,toolbardialog,toolbar_vtable_standalone,unreg_lane2_mapparamsdialog_ctor}.cpp`,
`code/Game/Tutorial/{tutorialhighlightdialog,tutorialsetupdialog,tutorial_dialog_vtables}.cpp`,
`standalone/reg_autosave.cpp:22`.

`code/Game/Dialog/ui_panels.h` (→ `ui/Panels.h` + `ui/DialogController.h`): 42 TUs — every
`code/Game/Dialog/*ui*.cpp` panel (`attendanceui … workplaceui`, incl. `cropui_promote`,
`radiusui_promote`, `tradeui_checkout`, `selectresourceui`), `code/Game/Tutorial/tutorialui.cpp:9`,
`tutorial_dialog_vtables.cpp:59`, `standalone/disaster_harness.cpp:110`, `standalone/reg_debugmenu.cpp:30`.

`code/Game/Game/dialogloaderloop.h` (→ `DialogLoaderLoopT.h`): `dialogloaderloop.cpp:19`,
`standalone/splash_loop_stubs.cpp:37`.

### 3.3 TUs holding both an engine-side and a runtime-side definition of one class

**None by identical qualified name.** Runtime UI headers are included outside `runtime/` by exactly
one TU, `standalone/mouse_inject.cpp:128-130` (`UserInterface.h`, `Element.h`, `DialogInstance.h`),
and it includes no engine `ui/*` header. `tests/` includes no UI header from either tree.
Engine names are global (`::DialogInstance`), runtime names are `RtUI::`/`UI::`, so today the
two trees never collide by name — they collide by meaning only.

**Live ODR violations that DO exist (same global name, different members, both linked):**

1. `::TutorialSetupDialog` — `engine/types/ui/Dialogs.h:468-487` vs `engine/types/TutorialSetups.h:90-107`.
   Reached from `tutorialsetupdialog.cpp:30`/`tutorial_dialog_vtables.cpp:58` (ui version) and from
   the four `tutorialsetup*.cpp` (TutorialSetups version). No single TU sees both, so it compiles;
   the program is still ill-formed (NDR). Both assert 0x78; member names differ throughout.
2. `engine/types/Dialogs.h` (third tree) duplicates 12 names from `engine/types/ui/Dialogs.h`
   with different members (`base[0xB8]`/`field_B8…`, l.15-90). Zero includers today, so latent.
   Its `LoaderDialog.field_C0` is typed `float` "rotation angle" (l.51) where `ui/Dialogs.h:220`
   has `uint32_t dataC0` — a real content disagreement if anyone ever includes it.

**Runtime-internal same-name pairs** (different namespaces, so legal, but they meet when `RtUI`
folds into `UI`): `Sprite` ×3 (§1.4; `runtime/src/ui/spritecontext_add.cpp` includes `Sprite.h`
AND `SpriteSheet.h` AND `SpriteContext.h`; nothing today includes `ImageElement.h` together with
`Sprite.h` — `widgets_layout.cpp:34` has ImageElement/LabelElement but not Sprite.h;
`element_sprite_debug.cpp:14-15` likewise), `FontGlyph` ×2 (§1.8; `Font.h` and `SpriteContext.h`
share no includer), `Element` in anonymous namespaces of `runtime/src/application/
message_callbacks.cpp:76` and `platform_startup_win.cpp:102` (unrelated 16-byte record; harmless).

### 3.4 Runtime UI headers' own include graph (for the `RtUI`→`UI` fold)

Roots: `Element.h` (includes `String.h`, `Collection.h`), `ElementDescription.h` (includes
`object/External.h`), `DialogInstance.h` (includes `Collection.h`, `object/Internal.h`), `Dialog.h`
(standalone). Leaves on `Element.h`: `ComboElement, DocumentTextElement, EditElement(→NumberEditElement),
NumberElement, PageElement, ProgressElement(+Sprite.h), RadioElement, SliderElement(+Sprite.h),
SpinnerElement, TabElement, TableElement, TextElement`. Leaves on `ElementDescription.h`:
`DescriptionsBatch1.h`, `GroupDescription.h`. Islands: `Font.h`, `SpriteContext.h`,
`SpriteCapture.h`, `SpriteSheet.h`, `Sprite.h`, `String.h`, `StringTable.h`, `UserInterface.h`,
`ImageElement.h`, `LabelElement.h` (the last two include neither `Element.h` nor `Sprite.h`).
`runtime/types/common.h:24` forward-declares a bare `struct SpriteSheet; // Graphics::SpriteSheet`
— the same bare name `SpriteSheet.h:160` defines for `UI::SpriteSheet`.

### 3.5 Definition census (grep `^\s*(struct|class)\s+<Name>` for the UI name set)

`engine/types/ui/*`: 2 + 1 + 22 + 2 + 38 = 65 structs. `engine/types/Descriptions.h`: 15
(`AtmosphereBlock` included). `engine/types/Dialogs.h`: 12. `engine/types/TutorialSetups.h:90`: 1.
`engine/types/{DialogLoaderLoopT,DialogLoopT}.h`: 1 each. `runtime/types/ui/*`: 33 structs.
`runtime/types/engine/UpdateLoop.h:202,225`: `RtEngine::DialogLoopT`, `RtEngine::DialogLoaderLoopT`.
`runtime/types/save/SaveLoad.h:59`: `RtSave::Sprite`.

---

## 4. `engine/types/Descriptions.h` vs the runtime description headers

No `*Description` layout exists in both trees. Runtime defines `ElementDescription` (0x68),
`GroupDescription` (0x78), and the ten in `DescriptionsBatch1.h` (`Border 0xb8, Button 0xa0,
Check 0xa8, ClipRegion 0x78, Combo 0xb8, Container 0x90, DocumentText 0xe8, Edit 0x88, Graph 0x88,
Label 0xb8`, l.67-253). Engine defines two UI descriptions (`CaptureBox`, `FrustumOutline`) that
only PREFIX `ElementDescription` as an opaque 0x68 (`Descriptions.h:315,330`) — AGREE with
`ElementDescription.h:171`. The remaining 13 engine descriptions are `Object::External` tool
descriptions with no runtime counterpart by design.

Runtime gaps step 6 will hit (no struct anywhere in the tree, only `.cpp` bodies including
`ElementDescription.h`): `Image, Layout, List, Number, NumberEdit, Page, Progress, Radio, Ribbon,
Scroll, Slider, Spinner, Tab, Table, Text` descriptions (`runtime/src/ui/*description.cpp`, §3
includer list). `LabelDescription` therefore carries an opaque 0x90 `_textDescriptionBase`
(`DescriptionsBatch1.h:248`) because `TextDescription` has no header. Not a duplicate; a hole.

---

## 5. `DialogLoaderLoopT.h` / `DialogLoopT.h`

What they are: engine-side PODs for `Engine::DialogLoaderLoopT` (0x188: `LoaderLoop` 0x48 +
`m_dialogFilename`@0x48, `m_dialogExternal`@0x50, `m_dialogInstance`@0x58, `m_userInterface[0x128]`@0x60;
`DialogLoaderLoopT.h:20-57`) and `Engine::DialogLoopT` (0x168: `UpdateLoop` 0x28 + the same three
@0x28/0x30/0x38 + UI@0x40; `DialogLoopT.h:23-53`). Both are bare global structs built on the bare
engine `UpdateLoop`/`LoaderLoop` (`engine/types/UpdateLoop.h:52`, `LoaderLoop.h:16`).

Runtime twins: `RtEngine::DialogLoopT` (`runtime/types/engine/UpdateLoop.h:202-215`) and
`RtEngine::DialogLoaderLoopT` (l.225-240) with `RtSystem::String _dialogFilename`,
`void* _dialogExternal`, `void* _dialogInstance`, `UI::UserInterface _ui`. **Offsets and sizes
AGREE on every field** (0x28/0x30/0x38/0x40, 0x168; 0x48/0x50/0x58/0x60, 0x188). Names differ
(`m_` vs `_`), and the runtime types the UI as the real `UI::UserInterface` where the engine keeps
an opaque byte array. The UI sub-offset comment "+0x38 pending-close" is wrong (§1.6).

UI types touched: `UI::DialogInstance*` (as `void*`), `Object::External*`, `UI::UserInterface`.

Users of the engine PODs (field access, not just the name): `standalone/splash_loop_stubs.cpp`
reads/writes `m_dialogInstance` (l.70,80), `m_nextLoop` (147,385,414-417,468), `m_dialogExternal`
(149-202,532,544), `m_elapsedTicks`/`m_delayTicks` (271-486); `code/Game/Game/dialogloaderloop.cpp:24,41,53`
(`m_elapsedTicks`), declared in `dialogloaderloop.h:5`. `code/Game/Tutorial/tutorial_next_scenario.cpp:148`
only names the ctor door. `DialogLoopT.h`: no user. Runtime users: `runtime/src/engine/loops.cpp:135-136`
`using RtEngine::…`, bodies l.462-1281, exports l.1525-1655.

---

## 6. Recommendation per class (step-5 precedent: POD → alias where field-identical; POD → `Rt*`
namespace only where it must survive; delete dead views)

| class | single definition after step 6 | the other tree becomes |
|---|---|---|
| `UI::DialogController` | `runtime/types/ui/DialogController.h` (split out of `DialogInstance.h`, `namespace UI`, ADD `static_assert(sizeof==0x30)` citing the 38 `Panels.h` +0x30 starts) | `engine/types/ui/DialogController.h` → `using DialogController = UI::DialogController;` after the 4 field names are unified; the engine POD's `data10..data28` have zero readers by name (all 38 panels reach base fields via `base.` or raw offsets — verify with a grep of `\.data(10|18|20|28)\b` before deleting) |
| `UI::DialogInstance` | `runtime/types/ui/DialogInstance.h`, `namespace UI`; keep 0xB8, cite the 20 `Dialogs.h` +0xB8 starts as the size proof and drop the "0xC0 upper bound" hedge (l.34-36) | `engine/types/ui/DialogInstance.h` → alias. Its two false claims (0x30 unwritten, 0xB0 padding, §1.2) die with it |
| `UI::Element` | `runtime/types/ui/Element.h` (already `namespace UI`) | `Elements.h` dead (0 includers): delete, or rewrite `CaptureBoxElement`/`FrustumOutlineElement` as `UI::Element` subclasses in `code/Game/…` when that block ports them. Runtime `ImageElement.h:43`/`LabelElement.h:57` prefixes → re-cut as `struct ImageElement : Element` / `LabelElement : TextElement` (LabelElement needs the corrected TextElement layout first, §1.8) |
| `UI::Sprite` | ONE: `runtime/types/ui/Sprite.h` moved into `namespace UI` with the `Sprite.h` names (`spriteSheet`, `spriteIndex`) | delete `UI::Sprite` from `ImageElement.h:26-39` (include `Sprite.h`); `RtSave::Sprite` (`SaveLoad.h:59`) → `using Sprite = UI::Sprite;`; fix `Dialogs.h:379` `sprite[0x20]` → `UI::Sprite sprite; void* loader; void* engine;` per `saveloaddialog.cpp:1305` |
| `UI::ElementDescription` + all `*Description` | `runtime/types/ui/ElementDescription.h`, `DescriptionsBatch1.h`, `GroupDescription.h` → `namespace UI` | `engine/types/Descriptions.h` dead (0 includers): delete the file, or keep only if a block ports `CaptureBoxDescription`/`FrustumOutlineDescription` — then those two become `struct X : UI::ElementDescription` in `code/Game` |
| `UI::UserInterface` | `runtime/types/ui/UserInterface.h` | `DialogLoaderLoopT.h:178`/`DialogLoopT.h:230` byte arrays → `UI::UserInterface _ui` via the alias below |
| `Engine::DialogLoaderLoopT` | `runtime/types/engine/UpdateLoop.h:225` | `engine/types/DialogLoaderLoopT.h` → `using DialogLoaderLoopT = RtEngine::DialogLoaderLoopT;` after renaming the 5 `m_*` reads in `splash_loop_stubs.cpp` and 2 in `dialogloaderloop.cpp` to `_*` (or keep `m_` accessors) — but this pulls `engine/types/{UpdateLoop,LoaderLoop}.h` (bare) into the same question; scope it with the Engine block, not UI |
| `Engine::DialogLoopT` | `runtime/types/engine/UpdateLoop.h:202` | `engine/types/DialogLoopT.h` dead (0 includers): delete. Retarget `menudialogloopt_stubs.cpp:273,322` raw `+0x30/+0x38` onto the class fields |
| exe dialogs (20, `Dialogs.h`) | stay in the engine tree as `struct X : UI::DialogInstance` (`OptionInputBindDialog : MessageDialog`), own fields unchanged | `engine/types/Dialogs.h` (third tree) delete; `TutorialSetups.h:90` `TutorialSetupDialog` and `ui/Dialogs.h:468` → ONE definition (both External-based, 0x78; pick the `TutorialSetups.h` home since that is where its siblings live and drop it from `Dialogs.h`; `TutorialGoalDialog` likewise is External-based and does not belong in a "DialogInstance subclasses" header) |
| exe panels (38, `Panels.h`) | stay in the engine tree as `struct X : UI::DialogController`, with `SelectResourceUI` as a real intermediate for Crop/Orchard/Pasture | `AchievementDialog`'s 0x14015b070 "secondary vtable" (§1.8) must be resolved in Ghidra first: if it is genuine MI, AchievementDialog cannot be a plain subclass |

Ordering constraint from the include graph: `ui_dialogs.h` (24 TUs) and `ui_panels.h` (42 TUs)
are the only doors; changing the two base PODs to aliases touches no include line in `code/Game`.
The `RtUI`→`UI` fold must land BEFORE the aliases, because an alias `using DialogInstance =
UI::DialogInstance` written today would name a type that does not exist (`RtUI::DialogInstance`).

---

## 7. `runtime/types/ui` namespace spelling, verified per header

| spelling | headers (VERIFIED by `grep -nE '^\s*(namespace|struct)'` on each file) |
|---|---|
| `namespace RtUI {` | `DescriptionsBatch1.h:58`, `Dialog.h:76`, `DialogInstance.h:151`, `ElementDescription.h:115`, `Font.h:67`, `GroupDescription.h:43` (6) |
| `namespace UI {` (plain) | `ComboElement.h:36`, `EditElement.h:46`, `Element.h:97`, `NumberEditElement.h:23`, `NumberElement.h:100`, `ProgressElement.h:37`, `RadioElement.h:29`, `SliderElement.h:37` (+ nested `SliderElementOffsets` l.89), `SpinnerElement.h:42`, `SpriteCapture.h:85`, `SpriteContext.h:147` and again `:300`, `UserInterface.h:127` (12) |
| `namespace UI { namespace XOffsets {` only | `DocumentTextElement.h:24,53` (+ a plain `namespace UI` at l.123 for `DocumentItem`), `GraphElement.h:40`, `PageElement.h:28`, `TabElement.h:28`, `TableElement.h:31`, `TextElement.h:61` (6) |
| bare struct, no namespace | `Sprite.h:57`, `SpriteSheet.h:82,160`, `String.h:65`, `LabelElement.h:57`, **and `ImageElement.h:43` (`struct ImageElement` is bare; only its embedded `UI::Sprite` l.26-39 is namespaced — UI_INVENTORY §1.1 lists this file as `namespace UI`, which is half right)** (5 files, 6 structs) |
| bare struct + `StringTableOffsets` sub-namespace | `StringTable.h:57,67,95` (1) |

Blast radius of moving each bare name under `UI::` — TUs with a real (non-comment) use of the
bare or `::`-qualified spelling; `code/Game` and `standalone` have **zero** code uses of any of
them (their hits are comments only: `tradeui.cpp:51,1196,1254,2351,2375`, `storageui.cpp:373,380`,
`saveloaddialog.cpp:199`, `address_registry.cpp:562`):

| bare name | TUs / headers to touch |
|---|---|
| `Sprite` | `runtime/src/ui/{sprite,sprite_debug,spritecapture,spritecapture_class,spritecontext,spritecontext_add,widgets_input}.cpp`; headers `Sprite.h`, `SpriteSheet.h`, `ProgressElement.h` (`::Sprite` ×6), `SliderElement.h` (`::Sprite` ×5), plus the `UI::Sprite` twin in `ImageElement.h` and `RtSave::Sprite` in `SaveLoad.h` (9 TUs, 5 headers) |
| `SpriteSheet` / `SpriteInfo` | `runtime/src/ui/{sprite,spritecontext_add,spritesheet_vtable,spritesheet_new,spritecapture_class,font,tabdescription,sliderdescription,progressdescription,labeldescription,imagedescription,graphdescription,documenttextdescription,borderdescription,element_bodies_b5}.cpp`, `runtime/src/save/saveload_draw.cpp`; headers `SpriteSheet.h`, `Sprite.h`, `ImageElement.h`, `DocumentTextElement.h`, `SpriteContext.h`, `SpriteCapture.h`, `LabelElement.h`, and the conflicting fwd-decl `runtime/types/common.h:24` (16 TUs, 8 headers) |
| `UIString` | `runtime/src/ui/{sprite,element,userinterface_hover}.cpp`; headers `String.h`, `Element.h:169` (`::UIString _toolTip`) (3 TUs, 2 headers). The retail name is `UI::String`; `UIString` was chosen to dodge `System::String`/`RtSystem::String` — moving it to `UI::String` is a rename, not just a namespace move |
| `LabelElement` | `runtime/src/ui/{labelelement,element_sprite_debug}.cpp` (+ `widgets_layout.cpp` includes it, uses only the exported names) (2-3 TUs, 1 header) |
| `ImageElement` (bare) | `runtime/src/ui/imageelement.cpp` (`::ImageElement` ×6), `element_sprite_debug.cpp` (2 TUs, 1 header) |
| `UiStringTable` / `StringTableTextEntry` / `StringTableOffsets` | `runtime/src/ui/{stringtable_factory,stringtable_lifecycle,sprite}.cpp` (3 TUs, 1 header) |

---

## Summary of findings

1. **Both base PODs agree with the runtime on every offset and on sizeof (0x30 / 0xB8).** The
   engine `DialogInstance` POD makes five false initialisation claims (+0x10, +0x18, +0x28, +0xA8
   "zeroed"; +0x30 "not initialised"; +0xB0 "padding") that the runtime header refutes with the
   ctor's parameter list. Names differ on every field of both bases.
2. **The engine tree supplies the two size proofs the runtime header says it lacks**: 38 panels
   start at +0x30, 20 dialogs start at +0xB8.
3. **ODR**: no TU mixes the trees. One live cross-TU ODR violation (`TutorialSetupDialog` ×2) and
   one latent third tree (`engine/types/Dialogs.h`, 12 duplicate names, 0 includers). Runtime has
   `Sprite` ×3 and `FontGlyph` ×2 (the latter with different meanings) that collide once `RtUI` folds
   into `UI`.
4. **Dead engine headers**: `Elements.h`, `Descriptions.h`, `DialogLoopT.h`, `Dialogs.h` (non-ui)
   — zero includers each.
5. **Two of the "22 DialogInstance subclasses" are not** (`TutorialGoalDialog`, `TutorialSetupDialog`
   are `Object::External`); `Dialogs.h:379` mislabels a 0x20 blob as a 0x10 `UI::Sprite`; two
   `SecondaryVtable` annotations (`Dialogs.h:23,81`) hint at multiple inheritance, and the first
   reuses AttendanceUI's vtable address — settle in Ghidra before subclassing.
6. **Ownership**: runtime owns every `UI::` class; engine PODs for the two bases become aliases,
   the four dead headers are deleted, the exe dialogs/panels stay engine-side and become
   `: UI::DialogInstance` / `: UI::DialogController` subclasses. Loop templates belong to the Engine
   block (their bases `UpdateLoop`/`LoaderLoop` are also duplicated bare vs `RtEngine`).
7. **Namespace inventory** confirmed with one correction: `ImageElement.h`'s own struct is bare,
   making 5 bare-struct files, not 4. Moving the bare names is a `runtime/src/ui`-only change
   (≤16 TUs for `SpriteSheet`, ≤9 for `Sprite`, 3 for `UIString`); `code/Game` has no code use.
