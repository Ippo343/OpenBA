# Phase F: Remaining UI Passthroughs — Deep Reimplementation Plan

## Overview

Phase F covers **all remaining UI passthrough methods** — approximately **136 methods**
across 42 reimpl files (16 panel-only + 26 dialog files). These were deferred from
Phases D and E due to complexity or many dependencies.

**Prior phases completed:** 102 methods reimplemented (74 panel + 28 dialog)
**Phase F target:** Convert remaining ~136 passthroughs to clean C++

### Blocker Analysis

**No infrastructure blockers remain.** All dependencies (function pointers, globals,
type IDs) are resolved in-situ during each method's decompilation — the same workflow
used in Phases B-E.

**Former blocker (resolved 2026-03-02): Video::ImageBuffer** — affected 6 methods across
3 classes (MapUI, NoiseUI, SpriteCaptureUI). Now unblocked: `runtime/` directory
infrastructure created with binary-compatible type header (`runtime/types/video/ImageBuffer.h`,
0x68 bytes) and 4 function pointers (Ctor, Dtor, CopyToTexture, CopyFromTexture) resolved
in `engine/funcs/engine_funcs.h/.cpp`.

---

## Status Key

| Symbol | Meaning |
|--------|---------|
| [ ] | Not started |
| [~] | In progress |
| [x] | Reimplemented |

---

## Wave 1: Simple Methods (Low Deps, <50 Lines)

Estimated: ~25 methods. These need minimal new function pointers and follow established
patterns (element lookup, simple state read/write, button dispatch).

### Wave 1A: Simple Dialog Handlers

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] MessageDialog::Slot4 | ~30 | fn_DialogInstance_FindElement | Find OK/Cancel/Yes/No buttons, set IDs |
| [ ] EventPopupDialog::Slot4 | ~30 | fn_DialogInstance_FindElement | Populate popup text/image |
| [ ] LoaderDialog::Slot6 | ~15 | Image rotation func | Rotate loading image element |
| [ ] LoaderDialog::Slot7 | ~20 | none (uses existing) | Accumulate rotation angle |
| [ ] AutoSaveDialog::Slot4 | ~20 | fn_DialogInstance_FindElement | Find elements, set IDs |
| [ ] AutoSaveDialog::Slot9 | ~20 | fn_DialogInstance_Close | Button handler → close |
| [ ] TutorialHighlightDialog::Slot9 | ~15 | fn_DialogInstance_Close | Button handler → close |
| [ ] ModUploadDialog (remaining) | ~0 | — | Only dtor exists (already done) |
| [ ] MapParamsDialog::Slot7 | ~30 | spinner/number read funcs | Update map param values |
| [ ] MapParamsDialog::Slot9 | ~25 | spinner/number read funcs | Button dispatch |

### Wave 1B: Simple Panel Methods

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] PastureUI::Slot4 | ~30 | SelectResourceUI base Slot4 | Calls base + finds slider + updates |
| [ ] EntityUI::Slot4 | ~65 | DAT_1401c5788 (1 new global) | Entity info setup |
| [ ] HelpUI::Slot4 | ~40 | page navigation funcs | Find page/button elements, set IDs |
| [ ] HelpUI::Slot9 | ~35 | page navigation funcs | Button click → page change |
| [ ] UpgradeUI::Slot4 | ~30 | fn_FindBuildComponent | Find elements, check upgrade state |
| [ ] UpgradeUI::Slot9 | ~25 | UpgradeComponent funcs | Button click → trigger upgrade |
| [ ] ConsumeProduceUI::Slot4 | ~100 | Sprite resolution funcs | Combo setup with resource sprites |
| [ ] BuildRequirementUI::Slot9 | ~30 | fn_DialogInstance_Close | Button/notification handler |

**Wave 1 Deps (resolved in-situ per method, ~5-8 new function pointers):**
- `DAT_1401c5788` — global (entity-related)
- Image rotation / angle accumulation functions
- SelectResourceUI base Slot4 address
- Sprite resolution helpers (for ConsumeProduceUI)

---

## Wave 2: Medium Complexity (50-150 Lines, Some New Deps)

Estimated: ~35 methods. These involve moderate logic — table population, list iteration,
conditional element construction, or multi-step update handlers.

### Wave 2A: Dialog Setup & Update Methods

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] FollowDialog::Slot4 | ~60 | camera/entity tracking funcs | Setup follow target UI |
| [ ] FollowDialog::Slot7 | ~40 | position read funcs | Update follow position display |
| [ ] FollowDialog::Slot9 | ~30 | camera funcs | Button dispatch (follow/unfollow) |
| [ ] GoalDialog::Slot4 | ~120 | goal list iteration, table setup | Populate goals table (361 instr) |
| [ ] GoalDialog::Slot6 | ~60 | goal progress query | Update goal progress bars |
| [ ] EventNotifyDialog::Slot4 | ~50 | event list access | Setup notification list |
| [ ] EventNotifyDialog::Slot7 | ~40 | event state query | Update notification state |
| [ ] EventNotifyDialog::Slot9 | ~30 | — | Button dispatch (dismiss/goto) |
| [ ] ScenarioDialog::Slot4 | ~120 | scenario list, table | Populate scenario list (348 instr) |
| [ ] ScenarioDialog::Slot7 | ~50 | scenario state query | Update selection state |
| [ ] ScenarioDialog::Slot9 | ~30 | — | Button dispatch (select/back) |
| [ ] ToolbarDialog::Slot7 | ~80 | toolbar button state sync | Update button states from game |
| [ ] ToolbarDialog::Slot9 | ~60 | toolbar command dispatch | Handle toolbar button clicks |
| [ ] ModDetailsDialog::Slot4 | ~50 | mod info query funcs | Populate mod details |
| [ ] ModDetailsDialog::Slot9 | ~25 | — | Button handler |
| [ ] TutorialGoalDialog::Slot4 | ~50 | tutorial goal list | Setup goal display |
| [ ] TutorialGoalDialog::Slot7 | ~40 | goal progress query | Update progress |
| [ ] TutorialGoalDialog::Slot8 | ~30 | — | Notification handler |
| [ ] TutorialGoalDialog::Slot9 | ~25 | — | Button handler |
| [ ] TutorialSetupDialog::Slot4 | ~40 | tutorial config | Setup tutorial options |
| [ ] TutorialSetupDialog::Slot9 | ~25 | — | Button handler |

### Wave 2B: Panel Update & Notification Methods

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] ResidenceUI::Slot4 | ~80 | table setup, storage query | Populate residence info table |
| [ ] ResidenceUI::Slot7 | ~60 | storage iteration | Update resident list/counts |
| [ ] ResidenceUI::Slot9 | ~30 | — | Notification handler |
| [ ] EventLogUI::Slot5 | ~80 | dynamic array access, event data | Event log population |
| [ ] EventLogUI::Slot7 | ~60 | event state query | Update log entries |
| [ ] EventLogUI::Slot9 | ~30 | — | Notification/click handler |
| [ ] WorkplaceUI::Startup | ~80 | multi-component lookup | Initialize workplace panel |
| [ ] WorkplaceUI::OnRemove | ~40 | cleanup funcs | Teardown workplace panel |
| [ ] EntityUI::Slot5 | ~60 | entity info query | Entity detail update |
| [ ] EntityUI::Slot7 | ~80 | entity state query | Full entity info refresh |
| [ ] EntityUI::Slot8 | ~30 | — | Notification handler |
| [ ] EntityUI::Slot9 | ~40 | — | Button/notification dispatch |

**Wave 2 Deps (resolved in-situ per method, ~15-20 new function pointers):**
- Goal/scenario/tutorial list iteration functions
- Table construction helpers (SetSize, SetChild for dynamic rows)
- Event data structure access (global event log)
- Camera/entity tracking functions
- Mod info query functions
- Toolbar button state sync functions

---

## Wave 3: Complex Methods (150-300+ Lines, Many Deps)

Estimated: ~30 methods. Large functions with table construction, array management,
complex state machines, or multi-tab panels.

### Wave 3A: Complex Panel Logic

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] BuildUI::Slot4 | ~250 | storage query, ResourceType iteration, table | Populate build storage table |
| [ ] BuildUI::Slot7 | ~180 | storage count, progress update | Update build progress + materials |
| [ ] ProductionUI::Slot4 | ~180 | dynamic array mgmt, production query | Setup production list (vector) |
| [ ] ProductionUI::Slot7 | ~100 | production state query | Update production status |
| [ ] ProfessionUI::Slot5 | ~120 | BST iteration, table, sprites | Populate profession table |
| [ ] ProfessionUI::Slot8 | ~80 | profession state query | Update profession counts |
| [ ] ProfessionUI::Slot9 | ~60 | profession assignment funcs | Handle profession changes |
| [ ] StorageUI::Serialize | ~100 | ExternalFactory iteration | Enumerate storage types |
| [ ] StorageUI::Startup | ~120 | table construction, storage query | Build storage contents table |
| [ ] StorageUI::Slot9 | ~40 | — | Notification handler |
| [ ] CitizenUI::Slot4 | ~150 | many sub-component lookups | Populate citizen info panel |
| [ ] CitizenUI::Slot7 | ~120 | citizen state query, health/food/etc. | Update citizen stats |
| [ ] CitizenUI::Slot9 | ~60 | citizen action dispatch | Handle citizen panel buttons |

### Wave 3B: Complex Dialog Logic

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] MainMenuDialog::Slot4 | ~200 | button setup, menu state | Init main menu (445 instr) |
| [ ] MainMenuDialog::Slot5 | ~40 | audio unpause | Unpause audio on menu show |
| [ ] MainMenuDialog::Slot8 | ~100 | menu command dispatch | Handle menu button clicks |
| [ ] AchievementDialog::Slot4 | ~150 | achievement list, table | Populate achievement table (372 instr) |
| [ ] AchievementDialog::Slot6 | ~60 | achievement progress query | Refresh achievement state |
| [ ] AchievementDialog::Slot14 | ~40 | — | Secondary handler |
| [ ] AchievementDialog::Slot17 | ~40 | — | Secondary handler |
| [ ] SaveLoadDialog::Slot4 | ~150 | file list, save metadata | Populate save/load file list |
| [ ] SaveLoadDialog::Slot7 | ~80 | save state query | Update save info display |
| [ ] SaveLoadDialog::Slot9 | ~60 | save/load dispatch | Handle save/load/delete actions |
| [ ] NewGameDialog::Slot4 | ~120 | map params, game config | Setup new game options |
| [ ] NewGameDialog::Slot9 | ~60 | game start dispatch | Handle start game |
| [ ] OptionsDialog::Slot4 | ~150 | options read, many controls | Populate options panel |
| [ ] OptionsDialog::Slot9 | ~80 | options write | Apply/save options |
| [ ] OptionInputBindDialog::Serialize | ~60 | key binding read | Setup keybind display |
| [ ] OptionInputBindDialog::Startup | ~40 | key input capture | Start key capture |
| [ ] OptionInputBindDialog::OnCreate | ~30 | — | Init handler |
| [ ] OptionInputBindDialog::OnRemove | ~30 | — | Cleanup handler |
| [ ] OptionInputBindDialog::Slot7 | ~40 | key state query | Update bind display |
| [ ] OptionInputBindDialog::Slot9 | ~30 | key bind write | Apply binding |

**Wave 3 Deps (resolved in-situ per method, ~25-30 new function pointers):**
- ResourceType enumeration / ExternalFactory iteration
- BST (binary search tree) traversal for ProfessionUI
- Dynamic vector management (MSVC-compatible alloc/resize)
- Achievement/goal list access
- Save file metadata query
- Options system read/write
- Audio pause/unpause
- Key binding capture functions

---

## Wave 4: Complex Panel Systems & Large Multi-Method Classes

Estimated: ~25 methods. Large multi-method panels and classes with many cross-cutting
deps. No infrastructure blockers — all function pointers resolved in-situ during
decompilation. Grouped here by effort, not by dependency risk.

### Wave 4A: Multi-Method Panels

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] TownHallUI::ScalarDtor | ~100 | 3 nested container + 7 buffer dtors | Complex teardown |
| [ ] TownHallUI::Serialize | ~150 | many element lookups | Multi-tab init |
| [ ] TownHallUI::Startup | ~200 | population/food/resource queries | Update all tabs |
| [ ] TownHallUI::OnCreate | ~80 | tab setup | Create tab infrastructure |
| [ ] TownHallUI::OnRemove | ~60 | tab teardown | Remove tab infrastructure |
| [ ] TownHallUI::Update | ~120 | game state queries | Per-frame tab updates |
| [ ] TradeUI::Serialize | ~100 | trade list setup | Init trade panel |
| [ ] TradeUI::Draw | ~80 | trade rendering | Custom draw for trade |
| [ ] TradeUI::Slot6 | ~60 | trade state query | Trade update handler |
| [ ] TradeUI::Startup | ~120 | trade post query | Populate trade info |
| [ ] TradeUI::OnCreate | ~60 | trade system hooks | Init trade monitoring |
| [ ] TradeUI::OnRemove | ~40 | trade system unhooks | Cleanup trade monitoring |
| [ ] TradeUI::Update | ~100 | trade state tick | Per-frame trade updates |

### Wave 4B: High-Dep-Count Methods (Many Function Pointers, No Infra Blockers)

These have the most cross-cutting dependencies but all are resolvable via the normal
decompile → resolve → implement workflow. No new subsystem infrastructure needed.

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] StatusBarUI::Slot7 | ~200 | game state queries, globals | Main HUD update (607 instr) |
| [ ] StatusBarUI::Slot8 | ~80 | HUD state funcs | Secondary update |
| [ ] StatusBarUI::Slot9 | ~60 | — | Notification handler |
| [ ] ToolUI::Slot4 | ~100 | tool state query funcs | Setup active tool panel |
| [ ] ToolUI::Slot7 | ~80 | tool state query funcs | Update tool display |
| [ ] ToolUI::Slot9 | ~40 | — | Tool panel notifications |
| [ ] DebugUI::Slot4 | ~150 | debug state, many controls | Setup debug overlay |
| [ ] DebugUI::Slot7 | ~100 | debug state query funcs | Update debug display |
| [ ] DebugUI::Slot8 | ~40 | — | Notification handler |
| [ ] DebugUI::Slot9 | ~60 | debug toggle funcs | Handle debug toggles |
| [ ] ModDialog::Slot4 | ~100 | mod list iteration | Populate mod list |
| [ ] ModDialog::Slot7 | ~60 | mod state query | Update mod status |
| [ ] ModDialog::Slot9 | ~40 | mod enable/disable funcs | Handle mod toggles |
| [ ] ModBrowseUI (remaining) | ~30 | mod browse state funcs | Remaining methods |

### Wave 4C: Video::ImageBuffer Dependent (UNBLOCKED — Infrastructure Complete)

These 6 methods were previously blocked on `Video::ImageBuffer`. The blocker has been
resolved: `runtime/` directory infrastructure is in place, ImageBuffer type header
(`runtime/types/video/ImageBuffer.h`, 0x68-byte binary-compatible struct) is created,
and 4 function pointers (Ctor, Dtor, CopyToTexture, CopyFromTexture) are resolved in
`engine/funcs/engine_funcs.h/.cpp`. These methods can now proceed with normal
decompile → implement workflow.

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] MapUI::Slot4 | ~150 | Video::ImageBuffer, coord math | Map image construction |
| [ ] MapUI::Slot9 | ~80 | ImageBuffer rendering | Map interaction |
| [ ] NoiseUI::Slot4 | ~120 | Video::ImageBuffer, noise gen | Noise visualization |
| [ ] NoiseUI::Slot9 | ~60 | ImageBuffer rendering | Noise interaction |
| [ ] SpriteCaptureUI::Serialize | ~80 | Video capture pipeline | Init capture |
| [ ] SpriteCaptureUI::OnCreate | ~60 | Video capture pipeline | Start capture |

---

## Wave 5: Skipped Destructors (9 Total)

These dtors were skipped in Phases D/E due to non-standard teardown patterns.
They should be attempted after their corresponding game logic methods are done.

### Dialog Dtors

| Method | Complexity | Blocker |
|--------|-----------|---------|
| [ ] EventNotifyDialog::Dtor | Medium | Vector destructor iterator pattern |
| [ ] ModDialog::Dtor | Medium | Array with virtual dtors |
| [ ] SaveLoadDialog::Dtor | Medium | 4 params + array cleanup |
| [ ] TutorialGoalDialog::Dtor | Low-Med | Extends Object::External (non-standard base) |

### Panel Dtors

| Method | Complexity | Blocker |
|--------|-----------|---------|
| [ ] TownHallUI::ScalarDtor | High | 3 nested containers + 7 flat buffers + sub-func |
| [ ] ProfessionUI::Dtor | High | BST node removal (FUN_14000e320) + 3 flat buffers |
| [ ] ProductionUI::Dtor | Medium | MSVC vector destructor iterator |
| [ ] MapUI::Dtor | Medium | Custom sub-function with extra params |
| [ ] NoiseUI::Dtor | Medium | Custom sub-function with extra params |

---

## Wave 6: Dialog/Loop Infrastructure (10 Methods)

These are not typical UI panels/dialogs but MainLoop-derived classes used for
loading screens and dialog-as-loop patterns.

| Method | Est. Lines | Deps Needed | Notes |
|--------|-----------|-------------|-------|
| [ ] DialogLoaderLoop::Ctor | ~40 | MainLoop ctor pattern | Construct loader loop |
| [ ] DialogLoaderLoop::Dtor | ~30 | MainLoop dtor pattern | Destroy loader loop |
| [ ] DialogLoaderLoop::Update | ~60 | loader state machine | Loading tick |
| [ ] DialogLoaderLoop::Frame | ~40 | frame timing | Frame wrapper |
| [ ] DialogLoaderLoop::Draw | ~30 | draw dispatch | Render loading screen |
| [ ] DialogLoop::Ctor | ~40 | MainLoop ctor pattern | Construct dialog loop |
| [ ] DialogLoop::Dtor | ~30 | MainLoop dtor pattern | Destroy dialog loop |
| [ ] DialogLoop::Update | ~50 | dialog state machine | Dialog tick |
| [ ] DialogLoop::Frame | ~40 | frame timing | Frame wrapper |
| [ ] DialogLoop::Draw | ~30 | draw dispatch | Render dialog |

**Dependencies:** MainLoop infrastructure (already partially done from Engine::MainLoop reimpl)

---

## Dependency Resolution Approach

All ~50-60 new function pointers are resolved **in-situ** during each method's
decompilation — the same workflow used in Phases B-E:

1. Decompile method in Ghidra
2. Identify called functions / accessed globals
3. Add to `engine/funcs/engine_funcs.h` (declaration) and `.cpp` (resolution)
4. Write clean C++ in `engine/src/ui/`

No pre-resolution pass is needed. Dependencies are discovered and resolved as part
of the normal decompile-implement cycle.

### Estimated New Function Pointers Per Wave

| Wave | Est. New FPs | Examples |
|------|-------------|---------|
| 1 | 5-8 | Image rotation, SelectResourceUI base, sprite helpers |
| 2 | 15-20 | Goal/scenario lists, camera tracking, event data, toolbar sync |
| 3 | 25-30 | ResourceType enum, BST traversal, vector mgmt, save/options/audio |
| 4A/4B | 10-15 | TownHall tabs, trade system, tool state, debug state |
| 5 | 0 (reuse) | All deps already resolved from game logic waves |
| 6 | ~5 | MainLoop ctor/dtor patterns (partially covered by existing infra) |
| **Total** | **~60** | |

### Video::ImageBuffer (Wave 4C) — RESOLVED

The 6 Wave 4C methods are now **unblocked**. The `runtime/` directory infrastructure
has been created with `Video::ImageBuffer` type header (0x68-byte binary-compatible
struct at `runtime/types/video/ImageBuffer.h`) and 4 function pointers resolved in
`engine/funcs/engine_funcs.h/.cpp` (Ctor, Dtor, CopyToTexture, CopyFromTexture).
Additional function pointers (e.g., Create virtual call) will be added in-situ
during each method's decompilation, same as all other waves.

---

## Recommended Execution Order

1. **Wave 1** — Quick wins, builds momentum. ~25 methods, mostly pattern repetition.
   Each method: decompile in Ghidra → write src/ → update reimpl → test.

2. **Wave 2** — Medium methods, good balance of effort/value. ~35 methods.
   Focus on dialog setup methods first (Slot4 variants), then update handlers (Slot7).

3. **Wave 3** — Complex core. ~30 methods. Tackle in sub-groups:
   - 3A first (panel logic — BuildUI, ProfessionUI, StorageUI, CitizenUI)
   - 3B second (dialog logic — MainMenu, SaveLoad, Options, Achievements)

4. **Wave 5** — Skipped dtors. 9 methods. Attempt after the game logic for each
   class is done (understanding the data structures makes dtors easier).

5. **Wave 4A** — Multi-method panels (TownHallUI, TradeUI). 13 methods.
   These are the largest individual efforts.

6. **Wave 4B** — Infrastructure-dependent (StatusBar, Tool, Debug, Mod). ~14 methods.

7. **Wave 6** — Loop infrastructure (DialogLoaderLoop, DialogLoop). 10 methods.
   These are lower priority since they're loading/transition screens.

8. **Wave 4C** — Video/ImageBuffer dependent (Map, Noise, SpriteCapture). 6 methods.
   Now unblocked — ImageBuffer infrastructure complete (`runtime/types/video/ImageBuffer.h`).

---

## Metrics & Tracking

| Wave | Methods | Est. New FPs | Priority | Status |
|------|---------|-------------|----------|--------|
| Wave 1 | ~25 | 5-8 | HIGH | [ ] Not started |
| Wave 2 | ~35 | 15-20 | HIGH | [ ] Not started |
| Wave 3 | ~30 | 25-30 | MEDIUM | [ ] Not started |
| Wave 4A | ~13 | 10-15 | MEDIUM | [ ] Not started |
| Wave 4B | ~14 | 5-10 | MEDIUM | [ ] Not started |
| Wave 5 | ~9 | 0 (reuse) | LOW | [ ] Not started |
| Wave 6 | ~10 | ~5 | LOW | [ ] Not started |
| **Subtotal** | **~136** | **~60** | | **All unblocked** |
| Wave 4C | 6 | ~4 (resolved) | MEDIUM | [ ] Unblocked (ImageBuffer infra complete) |
| **Grand Total** | **~142** | | | **All unblocked** |

### Completion Criteria

- **Phase F "functionally complete"**: All ~142 methods are [x] (**100% UI coverage**).
  Video::ImageBuffer blocker resolved — all waves are now unblocked.
- **UI module done**: Phase F complete + all prior phases = **~238 methods total**.

---

## Notes

- **In-situ dep resolution**: Every new function pointer is added to `engine_funcs.h/.cpp`
  during the decompilation of the method that needs it. No pre-resolution pass required.
  This is the same workflow that successfully produced 102 methods in Phases B-E.
- **Group by class**: Do all methods within the same class together — shared understanding
  of the struct layout accelerates subsequent methods and reduces re-decompilation.
- **Decompile budget**: ~15-30 min per method (decompile + annotate + write clean C++).
- **Test cadence**: Test after each class is complete, not after each individual method.
- **Skipped dtors**: The 9 Wave 5 dtors become much easier once the corresponding game
  logic methods are done, since you'll understand the data structures by then.
- **Loop classes**: DialogLoaderLoop / DialogLoop share infrastructure with the already-
  reimplemented Engine::MainLoop.
- **Video::ImageBuffer**: Infrastructure resolved (2026-03-02). `runtime/` directory
  created with type header, function pointers, and Makefile integration. All 142
  Phase F methods are now unblocked.
