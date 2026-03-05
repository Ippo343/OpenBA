# Phase F: UI Deep Reimplementation — Remaining Work

**Progress: ~50 done, 13 N/A, ~79 remaining** across Waves 3-6.
Waves 1-2 complete. All methods below are TODO.

---

## Wave 2 (1 remaining)

| Method | Bytes | Notes |
|--------|-------|-------|
| ModDetailsDialog::Slot4 | 2941 | Populate mod details (complex: 12 element lookups, mod conflict tree) |

---

## Wave 3: Complex Methods (150-300+ Lines)

### 3A: Complex Panel Logic

| Method | Est. Lines | Notes |
|--------|-----------|-------|
| BuildUI::Slot4 | ~250 | Populate build storage table |
| BuildUI::Slot7 | ~180 | Update build progress + materials |
| ProductionUI::Slot4 | ~180 | Setup production list (vector) |
| ProductionUI::Slot7 | ~100 | Update production status |
| ProfessionUI::Slot5 | ~120 | Populate profession table (BST iteration) |
| ProfessionUI::Slot8 | ~80 | Update profession counts |
| ProfessionUI::Slot9 | ~60 | Handle profession changes |
| StorageUI::Serialize | ~100 | Enumerate storage types (ExternalFactory) |
| StorageUI::Startup | ~120 | Build storage contents table |
| StorageUI::Slot9 | ~40 | Notification handler |
| CitizenUI::Slot4 | ~150 | Populate citizen info panel |
| CitizenUI::Slot7 | ~120 | Update citizen stats |
| CitizenUI::Slot9 | ~60 | Handle citizen panel buttons |

### 3B: Complex Dialog Logic

| Method | Est. Lines | Notes |
|--------|-----------|-------|
| MainMenuDialog::Slot4 | ~200 | Init main menu (445 instr) |
| MainMenuDialog::Slot8 | ~100 | Handle menu button clicks |
| AchievementDialog::Slot4 | ~150 | Populate achievement table (372 instr) |
| SaveLoadDialog::Slot4 | ~150 | Populate save/load file list |
| SaveLoadDialog::Slot7 | ~80 | Update save info display |
| SaveLoadDialog::Slot9 | ~60 | Handle save/load/delete actions |
| NewGameDialog::Slot4 | ~120 | Setup new game options |
| NewGameDialog::Slot9 | ~60 | Handle start game |
| OptionsDialog::Slot4 | ~150 | Populate options panel |
| OptionsDialog::Slot9 | ~80 | Apply/save options |
| OptionInputBindDialog::Serialize | ~60 | Setup keybind display |
| OptionInputBindDialog::Startup | ~40 | Start key capture |
| OptionInputBindDialog::OnCreate | ~30 | Init handler |
| OptionInputBindDialog::OnRemove | ~30 | Cleanup handler |
| OptionInputBindDialog::Slot7 | ~40 | Update bind display |
| OptionInputBindDialog::Slot9 | ~30 | Apply binding |

---

## Wave 4: Large Multi-Method Classes

### 4A: Multi-Method Panels

| Method | Est. Lines | Notes |
|--------|-----------|-------|
| TownHallUI::ScalarDtor | ~100 | Complex teardown (3 nested containers + 7 buffers) |
| TownHallUI::Serialize | ~150 | Multi-tab init |
| TownHallUI::Startup | ~200 | Update all tabs |
| TownHallUI::OnCreate | ~80 | Create tab infrastructure |
| TownHallUI::OnRemove | ~60 | Remove tab infrastructure |
| TownHallUI::Update | ~120 | Per-frame tab updates |
| TradeUI::Serialize | ~100 | Init trade panel |
| TradeUI::Draw | ~80 | Custom draw for trade |
| TradeUI::Slot6 | ~60 | Trade update handler |
| TradeUI::Startup | ~120 | Populate trade info |
| TradeUI::OnCreate | ~60 | Init trade monitoring |
| TradeUI::OnRemove | ~40 | Cleanup trade monitoring |
| TradeUI::Update | ~100 | Per-frame trade updates |

### 4B: High-Dep-Count Methods

| Method | Est. Lines | Notes |
|--------|-----------|-------|
| StatusBarUI::Slot7 | ~200 | Main HUD update (607 instr) |
| StatusBarUI::Slot8 | ~80 | Secondary update |
| StatusBarUI::Slot9 | ~60 | Notification handler |
| ToolUI::Slot4 | ~100 | Setup active tool panel |
| ToolUI::Slot7 | ~80 | Update tool display |
| ToolUI::Slot9 | ~40 | Tool panel notifications |
| DebugUI::Slot4 | ~150 | Setup debug overlay |
| DebugUI::Slot7 | ~100 | Update debug display |
| DebugUI::Slot8 | ~40 | Notification handler |
| DebugUI::Slot9 | ~60 | Handle debug toggles |
| ModDialog::Slot4 | ~100 | Populate mod list |
| ModDialog::Slot7 | ~60 | Update mod status |
| ModDialog::Slot9 | ~40 | Handle mod toggles |
| ModBrowseUI (remaining) | ~30 | Remaining methods |

### 4C: ImageBuffer Dependent (unblocked)

| Method | Est. Lines | Notes |
|--------|-----------|-------|
| MapUI::Slot4 | ~150 | Map image construction |
| MapUI::Slot9 | ~80 | Map interaction |
| NoiseUI::Slot4 | ~120 | Noise visualization |
| NoiseUI::Slot9 | ~60 | Noise interaction |
| SpriteCaptureUI::Serialize | ~80 | Init capture |
| SpriteCaptureUI::OnCreate | ~60 | Start capture |

---

## Wave 5: Skipped Destructors (9 total)

Do after game logic for each class is done.

| Method | Complexity | Notes |
|--------|-----------|-------|
| EventNotifyDialog::Dtor | Medium | Vector destructor iterator |
| ModDialog::Dtor | Medium | Array with virtual dtors |
| SaveLoadDialog::Dtor | Medium | 4 params + array cleanup |
| TutorialGoalDialog::Dtor | Low-Med | Extends Object::External |
| TownHallUI::ScalarDtor | High | 3 nested containers + 7 buffers + sub-func |
| ProfessionUI::Dtor | High | BST node removal + 3 buffers |
| ProductionUI::Dtor | Medium | MSVC vector destructor iterator |
| MapUI::Dtor | Medium | Custom sub-function |
| NoiseUI::Dtor | Medium | Custom sub-function |

---

## Wave 6: Dialog/Loop Infrastructure (10 methods)

Loading screens and dialog-as-loop patterns. Shares infra with Engine::MainLoop.

| Method | Est. Lines | Notes |
|--------|-----------|-------|
| DialogLoaderLoop::Ctor | ~40 | Construct loader loop |
| DialogLoaderLoop::Dtor | ~30 | Destroy loader loop |
| DialogLoaderLoop::Update | ~60 | Loading tick |
| DialogLoaderLoop::Frame | ~40 | Frame wrapper |
| DialogLoaderLoop::Draw | ~30 | Render loading screen |
| DialogLoop::Ctor | ~40 | Construct dialog loop |
| DialogLoop::Dtor | ~30 | Destroy dialog loop |
| DialogLoop::Update | ~50 | Dialog tick |
| DialogLoop::Frame | ~40 | Frame wrapper |
| DialogLoop::Draw | ~30 | Render dialog |

---

## Workflow Reminder

1. Decompile in Ghidra → rename, annotate
2. Add deps to `engine/funcs/engine_funcs.h/.cpp`
3. Write clean C++ in `code/Game/Dialog/`
4. Update hook wrapper in `engine/reimpl/` to call clean code
5. `mingw32-make deploy` + test

Group by class — do all methods within one class together.
