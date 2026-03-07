# Phase F: UI Deep Reimplementation — Remaining Work

**Progress: ~110 done, 13 N/A, ~22 remaining.**
Waves 1-4 complete (except items below). Wave 6 already done as Runtime DLL methods.

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

## Outstanding TODO Methods

| Method | Notes |
|--------|-------|
| DebugUI (all slots) | BLOCKED: vtable addresses wrong — Factory/Ctor/Vtable in Panels.h point to SpriteCaptureUI/SelectResourceUI. Need Ghidra investigation to find real DebugUI. |
| ModBrowseUI (remaining) | ~30 lines, small remaining methods |
| UpgradeUI::Serialize | Reimpl wrapper calls unimplemented function — reverted to trampoline. Needs clean impl. |
| UpgradeUI::OnRemove | Same as above — reverted to trampoline. Needs clean impl. |

---

## Passthrough Candidates (low priority)

These work correctly via trampoline or exe fn pointer delegation. Can be upgraded to clean C++ later if needed.

| Method | Reason kept as passthrough |
|--------|---------------------------|
| SaveLoadDialog::Load | Creates UpdateLoop + LoaderLoop (MSVC ABI constructors) |
| OptionsDialog::Startup | Deeply dependent on _singleton_exref relocations |
| TownHallUI::Startup | Unhookable (relative CALL in prologue) |
| TownHallUI::OnCreate | Unhookable (RIP-relative MOV in prologue) |
| TownHallUI::OnRemove | Unhookable (short Jcc in prologue) |
| TradeUI (5 methods) | Serialize/Startup/OnCreate/OnRemove/Update delegated to exe fn pointers (too complex) |
| SpriteCaptureUI::OnCreate | 12 one-off deps (Input, FileSystem, Filename) |
| ModDialog::Slot4 | 428 lines, ImageBuffer/MaterialInstance pipeline |

---

## Workflow Reminder

1. Decompile in Ghidra → rename, annotate
2. Add deps to `engine/funcs/engine_funcs.h/.cpp`
3. Write clean C++ in `code/Game/Dialog/`
4. Update hook wrapper in `engine/reimpl/` to call clean code
5. `mingw32-make deploy` + test

Group by class — do all methods within one class together.
