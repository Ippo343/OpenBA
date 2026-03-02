# UI Deep Reimplementation Plan

## Progress Summary

| Phase | Status | Description |
|-------|--------|-------------|
| A1 | **DONE** | Resolve Runtime UI widget function exports (~35 resolved) |
| A2 | **DONE** | Resolve exe type IDs (~18 resolved) |
| A3 | **DONE** | Resolve game singletons (2 resolved) |
| A4 | **DONE** | Resolve common exe functions (~25 resolved) |
| B | **DONE** | Simple panel game logic (7 methods) |
| C | **DONE** | Complex panel game logic (6 methods) |
| D | **DONE** | Remaining panels — 74 reimpl'd, dtor sweep complete |
| E | **DONE** | 22 dialog classes — 28 reimpl'd (18 dtors + 10 game logic) |
| F | Pending | All remaining complex/skipped UI methods (~141 passthroughs) |

### Phase D Progress (Waves 1-7)

**21 clean source files** in `engine/src/ui/`:

| Source File | Methods | What They Do |
|-------------|---------|-------------|
| titledesctooltipui.cpp | Serialize | Sets title/desc text from data source |
| radiusui.cpp | Slot5, Slot6 | Calculate radius overlay from center tile; remove overlay |
| consumeproduceui.cpp | Slot7, Slot9 | Progress bar update; combo select → component write |
| storagecapacityui.cpp | Serialize, Startup | Find progress element; query storage count, update bar |
| statusiconui.cpp | Serialize, Startup | Cache icon elements; update visibility from bitfield |
| cropui.cpp | Slot4, Slot7, Slot8 | Populate crop combo; enable/disable; repopulate on change |
| destroyui.cpp | Slot4, Slot7, Slot9 | Find elements/set IDs; visibility state machine; dispatch actions |
| immigrationui.cpp | Slot4, Slot7 | Find text/button elements, set IDs; update count + buttons |
| workplaceui.cpp | Serialize | Find worker/job elements, set IDs, check visibility |
| helpui.cpp | Slot7 | Enable/disable prev/next buttons by page index |
| attendanceui.cpp | Slot4, Slot7 | Find progress element, set max; update count |
| cemeteryui.cpp | Slot4, Slot7 | Find progress element, set max; update burial count |
| resourcelimitui.cpp | Slot4, Slot7, Slot9 | Configure spinner min/max; sync value; write back to global |
| pagetoggleui.cpp | Slot4, Slot7, Slot9 | Find checkbox/pages; enable/disable; toggle page 0/4 |
| tutorialui.cpp | Serialize, Startup | Find named elements, hide close; toggle button visibility |
| citizenui.cpp | Slot6 | Close sub-dialog if open, clear reference |
| controlui.cpp | Slot4, Slot7, Slot9 | Find checkbox/sub-component; sync state; toggle on click |
| entityui.cpp | Slot6 | Remove entity from world tracking set |
| buildui.cpp | Slot9 | Toggle workplace enabled state on button click |
| orchardui.cpp | Slot7, Slot11, Slot12 | Update growth bar; set/get crop type on component |
| pastureui.cpp | Slot11, Slot12 | Set/get animal type on component, calc max count |

### Dtor Sweep (Wave 7 — all feasible dtors done)

**23 panel dtors reimplemented** (mechanical set-vtable + cleanup + base-dtor pattern):

| Category | Panels |
|----------|--------|
| Simple (set vtable → base dtor) | AttendanceUI, CemeteryUI, ControlUI, DebugUI, EntityUI, ImmigrationUI, PageToggleUI, ResourceLimitUI, SpriteCaptureUI, ToolUI, TutorialUI, UpgradeUI, WorkPlaceUI |
| Double-vtable (derived → SelectResourceUI base) | OrchardUI, PastureUI |
| Cleanup 1 field | BuildUI (+0x30), ResidenceUI (+0x30), StorageUI (+0x30), HelpUI (+0x68) |
| Cleanup 3 fields | EventLogUI (+0x50/+0x60/+0x70) |
| Cleanup 4 fields | CitizenUI (+0xA0/+0xB0/+0xC0/+0xD0), StatusBarUI (+0x108/+0x118/+0x128/+0x138) |
| Cleanup 10 fields | TradeUI (+0x30 through +0x128) |

**Skipped dtors (5, all complex/custom):**
- TownHallUI: 3 nested container teardowns + 7 flat buffers + sub-function
- ProfessionUI: BST node removal (FUN_14000e320) + 3 flat buffers
- ProductionUI: MSVC vector destructor iterator
- MapUI, NoiseUI: Custom sub-function with extra params

**Total reimplemented methods: 74** (51 game logic + 23 dtors, across 33 reimpl files, 21 src files)

Wave 4 added: ControlUI (Slot4/Slot7/Slot9), EntityUI (Slot6)
Wave 5 added: BuildUI (Slot9), ImmigrationUI (Slot7), RadiusUI (Slot5)
Wave 6 added: OrchardUI (Slot7/Slot11/Slot12), PastureUI (Slot11/Slot12)
Wave 7 added: 23 dtors (13 simple, 2 double-vtable, 8 cleanup)

### Infrastructure Resolved (Phase A)

**Runtime DLL exports (35+):**
- Element: SetFlagsOnOff, SetElementId, SetName, SetToolTip, SetToolTipString, Child, ChildCount, FindElementType, RemoveChildren, AddChild
- DialogInstance: FindElement, GetController, FindElementType, SetFlagsOnOff
- ComboElement: AddItem, SetCurrentItem, GetItemCount, GetCurrentItem, GetItemData, RemoveAllItems
- ProgressElement: SetProgressTextParameter, SetMaximumValue, SetCurrentValue
- TableElement: SetSize, SetChild (2-arg + 3-arg), GetRowCount, GetColumnCount
- NumberEditElement: SetValue, SetMinMax, GetValue
- NumberElement: SetValue (int), SetValue (float)
- SpinnerElement: SetElementId, SetValue, SetMinMax, GetValue, IsActive
- PageElement: GetActivePage, SetActivePage
- ImageElement: SetSprite, SetWidthHeight
- LabelElement: SetSprite
- TextElement: SetText

**Exe type IDs (18):**
- ComponentEntity, ProgressElement (exref), CheckboxElement (exref)
- BuildComponent, ConsumeProduceComponent, RadiusComponent
- BuildRequirementComponent, ResourceComponent, ResourceType
- DialogController, PageToggleUI, MeshInstanceComponent
- DestroyComponent, WorkplaceComponent, StorageLocationComponent
- AttendanceSubComp, ResourceLimitType, WorkplaceJobType

**Game singletons (2):**
- g_pGameWorldSingleton (DAT_1401c3dd8) — World state, sub-fields at +0xB0, +0xF0, +0xF8, +0x100, +0x110
- g_pKeyMapSingleton (DAT_1401c3de8) — Input key mappings

**Common exe functions (30+):**
- FindComponent variants: StateMachine, MeshInstance, Destroy, Build, ConsumeProduce, Resource, Storage, Workplace, Radius, BuildState, Attendance, Cemetery, Orchard, Pasture
- Array ops: ExeArrayResize (x2), ExeArrayPush
- StorageComponent_QueryCount, StorageCountQuery
- RadiusCalc, RadiusDisplay_Remove
- DestroyComponent: CanReclaim, CanRebuild, Reclaim, Rebuild, Destroy
- GetActiveProductIndex, CropUI_PopulateCombo, CropUI_GetExpectedCount
- CemeteryGetBurialCount, SpinnerElement_IsActive
- OrchardGetGrowth, PastureCalcMaxCount, PastureUI_UpdateSlider

---

## Remaining Work

### Phase D: What's Left

**Remaining passthrough game logic (~65 methods across 26 panel files):**
All remaining methods have been assessed and classified as complex (100-250 lines,
many new dependencies, or requiring infrastructure not yet in scope).

**Moderately complex (may be feasible with targeted dep resolution):**
- PastureUI::Slot4 (~30 lines) — calls SelectResourceUI base Slot4, finds slider, updates
- EntityUI::Slot4 (~65 lines) — needs DAT_1401c5788
- HelpUI::Slot4/Slot9 (~100/80 lines) — many new deps

**Complex (100-250 lines, deferred):**
- BuildUI::Slot4 (~250 lines), Slot7 (~180 lines) — storage table with resource types
- ConsumeProduceUI::Slot4 (~100 lines) — combo setup with sprites
- ProductionUI::Slot4 (~180 lines), Slot7 — dynamic array management
- BuildRequirementUI::Slot4 — very complex
- ProfessionUI::Slot5/Slot8/Slot9 — table setup with BST, Sprites
- ResidenceUI::Slot4/Slot7 — table/storage iteration
- WorkplaceUI::Startup/OnRemove — multi-component, many new function pointers
- EventLogUI::Slot5/Slot7 — dynamic arrays, global event data structures
- StorageUI::Serialize/Startup — ExternalFactory iteration, table construction
- CitizenUI::Slot4/Slot7/Slot9 — citizen info, many sub-components
- EntityUI::Slot5/Slot7/Slot9 — entity info panel
- DebugUI::Slot4/Slot7/Slot8/Slot9 — debug panel with many controls

**Skipped (needs infrastructure not in scope):**
- NoiseUI::Slot4/Slot9 — Video::ImageBuffer allocation, noise generation
- MapUI::Slot4/Slot9 — ImageBuffer rendering, coordinate math
- SpriteCaptureUI::Serialize/OnCreate — Video capture pipeline
- StatusBarUI::Draw/Startup/OnRemove — complex rendering, 607 instructions
- ToolUI::Serialize/OnCreate/OnRemove — tool system integration
- TownHallUI — 5 methods, very complex multi-tab panel
- TradeUI — 7 methods, trading system integration

### Phase E: 22 Dialog Classes

Dialog classes use DialogInstance (0xB8 base) instead of DialogController (0x30 base).
Generally more self-contained (menus, load/save, options).

**Infrastructure added:** `fn_DialogInstance_Dtor` (resolved from Runtime-x64.dll `??1DialogInstance@UI@@UEAA@XZ` at 0x180114d40)

**Dtor sweep complete: 17 dialog dtors reimplemented + 1 prior (ModBrowseUI) = 18 total**

| Category | Dialogs |
|----------|---------|
| Simple (set vtable → DialogInstance dtor) | AutoSaveDialog, EventPopupDialog, FollowDialog, LoaderDialog, MainMenuDialog, MapParamsDialog, MessageDialog, ModDetailsDialog, ModUploadDialog, NewGameDialog, OptionsDialog, TutorialHighlightDialog |
| Simple (sets parent MessageDialog vtable) | OptionInputBindDialog |
| Cleanup 1 field | AchievementDialog (+0xC0), GoalDialog (+0xB8), ScenarioDialog (+0xD8) |
| Cleanup 3 fields | ToolbarDialog (+0xF8/+0xD0/+0xC0) |
| Prior (already done) | ModBrowseUI |

**Skipped dtors (4, complex/non-standard):**
- EventNotifyDialog: vector destructor iterator
- ModDialog: array with virtual dtors
- SaveLoadDialog: 4 params + array cleanup
- TutorialGoalDialog: extends Object::External, not DialogInstance

**Game logic methods reimplemented: 10 methods across 5 dialog files**

| Method | What It Does |
|--------|-------------|
| MessageDialog::Slot10 (ButtonOk) | Set result=1, close dialog |
| MessageDialog::Slot11 (ButtonCancel) | Set result=0, close dialog |
| MessageDialog::Slot12 (ButtonYes) | Set result=2, close dialog |
| MessageDialog::Slot13 (ButtonNo) | Set result=3, close dialog |
| AchievementDialog::Slot8 | OnCommand: msg=0x12, id=100 → close |
| GoalDialog::Slot8 | OnCommand: msg=0x12, id=100 → close |
| EventPopupDialog::Slot8 | OnCommand: msg=0x12, id=200 → close |
| LoaderDialog::Slot4 | Find "loadingImage" element, store at +0xB8 |
| MainMenuDialog::Slot10 | Secondary dtor (DialogInstance dtor + free) |
| AchievementDialog::Slot10 | Secondary dtor (set vtable → DialogController dtor + free) |

**Phase E total: 28 methods reimplemented (18 dtors + 10 game logic)**

**Remaining dialog passthroughs moved to Phase F: ~76 methods across 22 dialog files**

---

### Phase F: Remaining Complex/Skipped UI Methods

All remaining UI passthroughs consolidated here for future evaluation.
Estimated ~141 methods total (~76 dialog + ~65 panel).

**Skipped dtors (9 total):**

| Type | Dialog/Panel | Reason |
|------|-------------|--------|
| Dialog | EventNotifyDialog | Vector destructor iterator |
| Dialog | ModDialog | Array with virtual dtors |
| Dialog | SaveLoadDialog | 4 params + array cleanup |
| Dialog | TutorialGoalDialog | Extends Object::External, not DialogInstance |
| Panel | TownHallUI | 3 nested container teardowns + 7 flat buffers + sub-function |
| Panel | ProfessionUI | BST node removal + 3 flat buffers |
| Panel | ProductionUI | MSVC vector destructor iterator |
| Panel | MapUI | Custom sub-function with extra params |
| Panel | NoiseUI | Custom sub-function with extra params |

**Complex dialog game logic (~76 methods):**
- ToolbarDialog: Update (sync buttons), OnEvent (handle clicks)
- ScenarioDialog: Slot4 (populate list, 348 instr), Slot6 (update state)
- AchievementDialog: Slot4 (populate table, 372 instr), Slot6 (refresh), Slot14, Slot17
- GoalDialog: Slot4 (populate goals, 361 instr), Slot6 (progress)
- EventPopupDialog: Slot4 (populate popup)
- LoaderDialog: Slot6 (rotate image), Slot7 (accumulate angle)
- MainMenuDialog: Slot4 (init buttons, 445 instr), Slot5 (unpause audio), Slot8 (menu dispatch)
- MessageDialog: Slot4 (find buttons)
- SaveLoadDialog: all methods
- NewGameDialog: all methods
- OptionsDialog: all methods
- MapParamsDialog: all methods
- ModDialog, ModDetailsDialog, ModUploadDialog: all methods
- EventNotifyDialog, FollowDialog: all methods
- OptionInputBindDialog, TutorialHighlightDialog: all methods
- ModBrowseUI: remaining methods
- TutorialGoalDialog, TutorialSetupDialog: all methods

**Complex panel game logic (~65 methods):**
- BuildUI::Slot4 (~250 lines), Slot7 (~180 lines) — storage table
- ConsumeProduceUI::Slot4 (~100 lines) — combo setup with sprites
- ProductionUI::Slot4/Slot7 — dynamic array management
- BuildRequirementUI::Slot4 — very complex
- ProfessionUI::Slot5/Slot8/Slot9 — table with BST, sprites
- ResidenceUI::Slot4/Slot7 — table/storage iteration
- WorkplaceUI::Startup/OnRemove — multi-component
- EventLogUI::Slot5/Slot7 — dynamic arrays, global data
- StorageUI::Serialize/Startup — ExternalFactory iteration
- CitizenUI::Slot4/Slot7/Slot9 — citizen info, many sub-components
- EntityUI::Slot4/Slot5/Slot7/Slot9 — entity info panel
- DebugUI::Slot4/Slot7/Slot8/Slot9 — debug panel
- NoiseUI::Slot4/Slot9 — ImageBuffer, noise generation
- MapUI::Slot4/Slot9 — ImageBuffer rendering
- SpriteCaptureUI::Serialize/OnCreate — video capture
- StatusBarUI::Draw/Startup/OnRemove — complex rendering
- ToolUI::Serialize/OnCreate/OnRemove — tool system
- TownHallUI — 5 methods, multi-tab
- TradeUI — 7 methods, trading system
- PastureUI::Slot4 — SelectResourceUI base
- EntityUI::Slot4 — needs DAT_1401c5788
- HelpUI::Slot4/Slot9 — many new deps

---

## Architecture Reference

### Two-Layer Design
```
engine/src/ui/         — Clean game logic (21 source files, standalone-ready)
  ui_panels.h          — All function declarations
  <panelname>.cpp      — One file per panel

engine/reimpl/         — Hook wrappers (delegate to src/ or trampoline)
  <panelname>.cpp      — Thin wrappers with g_lastHook breadcrumb
```

### Deep Reimpl Workflow (per function)
1. Decompile in Ghidra → understand logic
2. Check if all deps (function pointers, type IDs) are resolved in engine_funcs
3. If not, add new deps to engine_funcs.h/.cpp
4. Write clean C++ in `engine/src/ui/<panel>.cpp`
5. Add declaration to `engine/src/ui/ui_panels.h`
6. Update `engine/reimpl/<panel>.cpp` to call clean function instead of trampoline
7. Build (`mingw32-make debug`) and test

### Success Metrics
- **Phase A:** DONE — 35+ Runtime exports, 18 type IDs, 2 singletons, 30+ exe functions
- **Phase B:** DONE — 7 game logic methods in `engine/src/ui/`, verified in-game
- **Phase C:** DONE — Complex panels (Destroy, Crop, StatusIcon) have game logic in src
- **Phase D:** DONE — 74 methods (51 game logic + 23 panel dtors)
- **Phase E:** DONE — 28 methods (18 dialog dtors + 10 game logic)
- **Phase F:** Pending — ~141 remaining passthroughs (complex/infrastructure-dependent)
- **Total reimplemented:** 102 methods across A-E
- **End state:** `engine/src/ui/` compiles standalone, all UI logic is clean C++
