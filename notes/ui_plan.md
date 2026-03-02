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
| D | **IN PROGRESS** | Remaining panels — 19 methods done, ~80+ remain |
| E | Pending | 22 dialog classes |

### Phase D Progress (Waves 1-3)

**16 clean source files** in `engine/src/ui/`:

| Source File | Methods | What They Do |
|-------------|---------|-------------|
| titledesctooltipui.cpp | Serialize | Sets title/desc text from data source |
| radiusui.cpp | Slot6 | Removes radius overlay, resets index |
| consumeproduceui.cpp | Slot7, Slot9 | Progress bar update; combo select → component write |
| storagecapacityui.cpp | Serialize, Startup | Find progress element; query storage count, update bar |
| statusiconui.cpp | Serialize, Startup | Cache icon elements; update visibility from bitfield |
| cropui.cpp | Slot4, Slot7, Slot8 | Populate crop combo; enable/disable; repopulate on change |
| destroyui.cpp | Slot4, Slot7, Slot9 | Find elements/set IDs; visibility state machine; dispatch actions |
| immigrationui.cpp | Slot4 | Find text/button elements, set IDs |
| workplaceui.cpp | Serialize | Find worker/job elements, set IDs, check visibility |
| helpui.cpp | Slot7 | Enable/disable prev/next buttons by page index |
| attendanceui.cpp | Slot4, Slot7 | Find progress element, set max; update count |
| cemeteryui.cpp | Slot4, Slot7 | Find progress element, set max; update burial count |
| resourcelimitui.cpp | Slot4, Slot7, Slot9 | Configure spinner min/max; sync value; write back to global |
| pagetoggleui.cpp | Slot4, Slot7, Slot9 | Find checkbox/pages; enable/disable; toggle page 0/4 |
| tutorialui.cpp | Serialize, Startup | Find named elements, hide close; toggle button visibility |
| citizenui.cpp | Slot6 | Close sub-dialog if open, clear reference |

**Total reimplemented methods across all UI panels: 39** (including 8 dtors from earlier batches)

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

**Common exe functions (25+):**
- FindComponent variants: StateMachine, MeshInstance, Destroy, Build, ConsumeProduce, Resource, Storage, Workplace, Radius, BuildState, Attendance, Cemetery
- Array ops: ExeArrayResize (x2), ExeArrayPush
- StorageComponent_QueryCount, StorageCountQuery
- RadiusCalc, RadiusDisplay_Remove
- DestroyComponent: CanReclaim, CanRebuild, Reclaim, Rebuild, Destroy
- GetActiveProductIndex, CropUI_PopulateCombo, CropUI_GetExpectedCount
- CemeteryGetBurialCount, SpinnerElement_IsActive

---

## Remaining Work

### Phase D: What's Left

The remaining unmigrated panel methods fall into complexity tiers:

**Moderate (need 1-3 new deps each):**
- UpgradeUI::Serialize/OnRemove — needs Sprite construction, ImageElement, DAT_1401c5868
- ControlUI::Slot4/Slot7/Slot9 — needs DAT_1401c4e20, FUN_1400c3680
- ImmigrationUI::Slot7 — needs FindComponent_Immigration, multiple sub-component reads
- BuildUI::Slot9 — needs FUN_140002e10, FUN_14012fc00

**Complex (need 5+ new deps, large functions):**
- ProfessionUI::Slot5/Slot8/Slot9 — table setup with BST, External paths, Sprite construction
- ResidenceUI::Slot4/Slot7 — very complex table/storage iteration
- BuildUI::Slot4/Slot7 — very complex storage table with resource types
- WorkplaceUI::Startup/OnRemove — multi-component, many new function pointers
- ProductionUI::Slot4/Slot7 — dynamic array management, child iteration
- EventLogUI::Slot5/Slot7 — dynamic arrays, global event data structures
- StorageUI::Serialize/Startup — ExternalFactory iteration, table construction
- CitizenUI::Slot4/Slot7/Slot9 — citizen info display, many sub-components
- EntityUI::Slot4-Slot9 — 5 methods, entity info panel
- DebugUI::Slot4/Slot7/Slot8/Slot9 — debug panel with many controls

**Skipped (needs infrastructure not in scope):**
- NoiseUI::Slot4/Slot9 — Video::ImageBuffer allocation, noise generation
- MapUI::Slot4/Slot9 — ImageBuffer rendering, coordinate math
- SpriteCaptureUI — Video capture pipeline
- StatusBarUI — Draw override, complex rendering
- ToolUI — tool system integration
- TownHallUI — 5 methods, very complex multi-tab panel
- TradeUI — 7 methods, trading system integration

### Phase E: 22 Dialog Classes (not started)

Dialog classes use DialogInstance (0xB8 base) instead of DialogController (0x30 base). Generally more self-contained (menus, load/save, options). Key targets:
- AchievementDialog, AutosaveDialog, EventNotifyDialog, EventPopupDialog
- FollowDialog, GoalDialog, LoaderDialog, MainMenuDialog
- MapParamsDialog, MessageDialog, ModDialog, ModBrowseUI, ModDetailsDialog, ModUploadDialog
- NewGameDialog, OptionInputBindDialog, OptionsDialog
- SaveLoadDialog, ScenarioDialog, ToolbarDialog
- TutorialGoalDialog, TutorialHighlightDialog

---

## Architecture Reference

### Two-Layer Design
```
engine/src/ui/         — Clean game logic (16 source files, standalone-ready)
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
- **Phase A:** DONE — 35+ Runtime exports, 18 type IDs, 2 singletons, 25+ exe functions
- **Phase B:** DONE — 7 game logic methods in `engine/src/ui/`, verified in-game
- **Phase C:** DONE — Complex panels (Destroy, Crop, StatusIcon) have game logic in src
- **Phase D target:** All 39 panels have all feasible methods in src (~80+ methods remaining)
- **Phase E target:** All 22 dialog controllers covered
- **End state:** `engine/src/ui/` compiles standalone, all UI logic is clean C++
