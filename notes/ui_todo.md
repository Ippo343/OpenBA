# UI/Dialog Reimplementation TODO

Remaining trampolines and exe dependencies across all dialog/UI reimpl files.
Goal: eliminate all exe calls so code runs entirely from the DLL.

---

## Stage 1 — Simple Passthrough Trampolines (7 functions, 5 files)

Single-function trampolines where the rest of the class is already reimplemented.
Each just needs decompiling in Ghidra and writing clean C++.
No new Runtime exports or singletons expected.

- [x] **TutorialSetupDialog::ScalarDtor** — tutorialsetupdialog.cpp (2026-03-06)
- [x] **TutorialSetupDialog::Shutdown** — tutorialsetupdialog.cpp (2026-03-06)
- [x] **TutorialSetupDialog::OnEvent** — tutorialsetupdialog.cpp (2026-03-06)
- [x] **StatusBarUI::OnRemove** — statusbarui.cpp (2026-03-06)
- [x] **TutorialUI::OnRemove** — tutorialui.cpp (2026-03-06)
- [x] **EventNotifyDialog::Update** — eventnotifydialog.cpp (2026-03-06)
- [x] **FollowDialog::Slot6** — followdialog.cpp (2026-03-06)

## Stage 2 — Startup/Complex Passthrough Trampolines (2 functions, 2 files) (opus)

Slightly more involved — Startup slots with UI element setup, or multi-param functions.
May need a few new element lookups or helper calls.

- [x] **BuildRequirementUI::Slot4** (Startup) — buildrequirementui.cpp (2026-03-07)
- [x] **ImmigrationUI::Slot9** — immigrationui.cpp (2026-03-07)

## Stage 3 — OptionsDialog Apply/Cancel (3 functions + 4 sub-functions) (opus)

High complexity. Reimplementing OnOk/OnCancel/OnApply and their sub-functions.
Requires resolving ~17 new Runtime DLL exports and 3 new singletons.

- [x] **ApplyInputSettings** (0x140072360) — (2026-03-07)
- [x] **ApplyGameSettings** (0x140072420) — (2026-03-07)
- [x] **ApplyVideoSettings** (0x140071e10) — (2026-03-07)
- [x] **RestoreVideoSettings** (0x1400727d0) — (2026-03-07)
- [x] **OnOk** (0x140072930) — (2026-03-07)
- [x] **OnCancel** (0x140072840) — (2026-03-07)
- [x] **OnApply** (0x1400728e0) — (2026-03-07)

All 17 Runtime DLL exports and 3 singletons resolved (2026-03-07).

## Stage 4 — OptionsDialog Input Binding (2 functions, very high complexity) (opus)

Deepest complexity. Opens sub-dialogs, constructs dialog instances, interacts with Input::KeyMap.
Last to tackle — may need additional dialog class reimplementations first.

- [x] **PopulateInputBindList** (0x140073580) — Input::KeyMap iteration, UI::Element::Child, UI::TextElement::SetText, FUN_140073490, FUN_140099c20 (2026-03-07)
- [x] **OnInputBindClick** (0x1400729a0) — opens inputBind/inputBound sub-dialogs, Input::KeyMap::SetInput/ClearInput/GetIdForDeviceInput, Input::Device::ResetState, ExternalFactory, multiple dialog ctors (2026-03-07)

All Runtime exports resolved including ShowDialog (2026-03-07).

---

## Stage 5 — Miscellaneous exe dependencies in clean code (opus)

Not trampolines, but still read data from the exe address space.

- [x] **optionsdialog.cpp** — Input::Configuration singleton (0x140151738) → now uses `g_pInputConfigSingleton` (already resolved via Runtime DLL export) (2026-03-07)
- [x] **optionsdialog.cpp** — Float constants (0x1401720c4=0.01f, 0x140172208=100.0f) for status icon scale conversion → replaced with literal constants (2026-03-07)
- [x] **saveloaddialog.cpp** — Float constant (0x14017214c=1.0f) → replaced with literal constant (2026-03-07)

---

## Stage 6 — Disabled UI/Dialog hooks (35 hooks needing reimpl to enable)

These hooks exist in hooks.cpp but are disabled (`false`) due to prologue issues.
Once each function is fully reimplemented (no trampoline call), the hook can be re-enabled.
Grouped by class.

### Dialog Controllers (12 disabled hooks)

- [ ] **EventNotifyDialog::Load** — MOV RAX/R11,RSP in prologue
- [ ] **SaveLoadDialog::Load** — MOV RAX/R11,RSP in prologue
- [ ] **ToolbarDialog::OnEvent** — MOV RAX/R11,RSP in prologue
- [ ] **AchievementDialog::Slot8** — Short Jcc in prologue
- [ ] **AchievementDialog::Slot14** — RIP-relative in prologue
- [ ] **AutoSaveDialog::Slot6** — RIP-relative in prologue
- [ ] **EventPopupDialog::Slot8** — Short Jcc in prologue
- [ ] **GoalDialog::Slot6** — RIP-relative in prologue
- [ ] **GoalDialog::Slot8** — Short Jcc in prologue
- [ ] **MainMenuDialog::Slot8** — JNZ relative jump at byte +4
- [ ] **ModDetailsDialog::Slot8** — Short Jcc in prologue
- [ ] **ModDialog::Slot4** — MOV RAX/R11,RSP in prologue

### UI Panel Controllers (23 disabled hooks)

- [ ] **AttendanceUI::Slot4** — RIP-relative in prologue
- [ ] **BuildUI::Slot9** — Near Jcc in prologue
- [ ] **CemeteryUI::Slot4** — RIP-relative in prologue
- [ ] **CitizenUI::Slot9** — Short Jcc in prologue
- [ ] **ConsumeProduceUI::Slot9** — Near Jcc in prologue
- [ ] **ControlUI::Slot9** — Short Jcc in prologue
- [ ] **CropUI::Slot9** — Relative JNZ+JZ in 13-byte prologue
- [ ] **DebugUI::Slot8** — RIP-relative in prologue
- [ ] **DestroyUI::Slot9** — Near Jcc in prologue
- [ ] **EntityUI::Slot9** — RIP-relative in prologue
- [ ] **EventLogUI::Slot9** — Short Jcc in prologue
- [ ] **HelpUI::Slot9** — MOV RAX/R11,RSP in prologue
- [ ] **MapUI::Slot4** — RIP-relative in prologue
- [ ] **MapUI::Slot9** — Near Jcc in prologue
- [ ] **NoiseUI::Slot9** — Short Jcc + near Jcc in prologue
- [ ] **OrchardUI::Slot9** — Short Jcc in prologue
- [ ] **PageToggleUI::Slot9** — Short Jcc in prologue
- [ ] **PastureUI::Slot9** — Short Jcc in prologue
- [ ] **ProductionUI::Slot7** — Relative CALL in prologue
- [ ] **ResidenceUI::Slot9** — Short Jcc in prologue
- [ ] **ResourceLimitUI::Slot9** — Near Jcc in prologue
- [ ] **UpgradeUI::OnRemove** — Near Jcc in prologue
- [ ] **WorkPlaceUI::OnRemove** — Near Jcc in prologue

### UI Description/Element (5 disabled hooks)

- [ ] **CaptureBoxDesc::Slot3** — RIP-relative in prologue
- [ ] **FrustumOutlineDesc::Slot3** — RIP-relative in prologue
- [ ] **CaptureBoxElement::OnCreate** — Short Jcc in prologue
- [ ] **FrustumOutlineElement::OnCreate** — Short Jcc in prologue
- [ ] **FrustumOutlineElement::Draw** — MOV RAX/R11,RSP in prologue

---

## Stage 7 — Missing hooks (never added to hook table)

These functions were never added to hooks.cpp because their prologues make even
a disabled hook impossible. Each needs: decompile, reimplement, add Install/Remove
functions to the reimpl file, add hook entry to hooks.cpp.

### OnEvent handlers (5 dialogs) — highest priority

- [ ] **NewGameDialog::OnEvent** (0x140070110) — rel branch in prologue. Handles randomize name/seed, start game, combo changes
- [ ] **SaveLoadDialog::OnEvent** (0x1400b67e0) — rel branch in prologue. Handles save/load/delete button clicks
- [ ] **MapParamsDialog::OnEvent** (0x14006b5f0) — rel branch at byte 4. Handles map parameter changes
- [ ] **EventNotifyDialog::OnEvent** (0x140068680) — no prologue (branch at byte 0). Handles event notification clicks
- [ ] **OptionInputBindDialog::OnEvent** (0x14006d070) — rel branch. Handles key binding input

### TutorialSetupDialog extra slots (3 functions)

- [ ] **TutorialSetupDialog::Slot8** (0x1400e25f0) — rel CALL in prologue
- [ ] **TutorialSetupDialog::Slot9** (0x1400e24c0) — rel CALL in prologue
- [ ] **TutorialSetupDialog::Slot10** (0x1400e2460) — rel CALL in prologue

### TutorialHighlightDialog (2 functions)

- [ ] **TutorialHighlightDialog::Update** (0x1400dffe0) — rel branch at byte 8. Tutorial overlay logic
- [ ] **TutorialHighlightDialog::Load** (0x1400e0000) — 10b MOV+ADD+RET, too short for trampoline

### FollowDialog extra slots (2 functions)

- [ ] **FollowDialog::Slot5** (0x140069320) — 19b, sets flag on followed entity
- [ ] **FollowDialog::Slot8** (0x140069230) — 18b CMP+JNZ+CMP+JZ+RET pattern
