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

- [ ] **BuildRequirementUI::Slot4** (Startup) — buildrequirementui.cpp:68 (Dtor done)
- [ ] **ImmigrationUI::Slot9** — immigrationui.cpp:65 (Dtor, Startup, Update done; 6-param function)

## Stage 3 — OptionsDialog Apply/Cancel (3 functions + 4 sub-functions) (opus)

High complexity. Reimplementing OnOk/OnCancel/OnApply and their sub-functions.
Requires resolving ~17 new Runtime DLL exports and 3 new singletons.

- [ ] **ApplyInputSettings** (0x140072360) — reads checkMouseClip/checkSoftwareMouse, writes to Input::Configuration, calls Save/EnableMouseClipping/EnableCursor
- [ ] **ApplyGameSettings** (0x140072420) — reads game sliders/checkboxes/combos, writes to g_pGameSettings, iterates dialogs, sets UI scale. Uses g_pKeyMapSingleton, g_pGameWorldSingleton
- [ ] **ApplyVideoSettings** (0x140071e10) — reads video combos/checkboxes, compares config, calls Video::Device Shutdown/Startup/RecreateDynamic/ApplyChangedSettings
- [ ] **RestoreVideoSettings** (0x1400727d0) — reverses video changes on cancel
- [ ] **OnOk** (0x140072930) — calls Apply* functions + Save + CloseDialog
- [ ] **OnCancel** (0x140072840) — calls ReadSettings + Restore* + CloseDialog
- [ ] **OnApply** (0x1400728e0) — same as OnOk minus CloseDialog

### Runtime DLL exports needed

```
?Save@Configuration@Audio@@QEAAXXZ
?Startup@Configuration@Audio@@QEAA_NXZ
?Save@Configuration@Video@@QEAAXXZ
?Save@Configuration@Input@@QEAAXXZ
?Load@Configuration@Input@@QEAAXXZ
?WriteSettings@Persistence@IO@@QEBAXXZ
?ReadSettings@Persistence@IO@@QEAAXXZ
?EnableMouseClipping@Device@Input@@QEAAX_N@Z
?ResetState@Device@Input@@QEAAXXZ
?ApplyChangedSettings@Device@Video@@QEAA_NXZ
?Startup@Device@Video@@QEAA_NXZ
?RecreateDynamic@Device@Video@@QEAAXXZ
?ReleaseDynamic@Device@Video@@QEAAXXZ
?Shutdown@Device@Video@@QEAAXXZ
?OnVideoError@MainLoop@Engine@@QEAAXXZ
?OnVideoRestart@MainLoop@Engine@@QEAAXXZ
?EnableCursor@UserInterface@UI@@QEAAX_N@Z
```

### Singletons needed

```
?_singleton@Configuration@Input@@0PEAV12@EA
?_singleton@Persistence@IO@@0PEAV12@EA
?_singleton@MainLoop@Engine@@1PEAV12@EA
```

(Video::Device, Audio::Configuration, Video::Configuration, Input::Device already resolved)

## Stage 4 — OptionsDialog Input Binding (2 functions, very high complexity) (opus)

Deepest complexity. Opens sub-dialogs, constructs dialog instances, interacts with Input::KeyMap.
Last to tackle — may need additional dialog class reimplementations first.

- [ ] **PopulateInputBindList** (0x140073580) — Input::KeyMap iteration, UI::Element::Child, UI::TextElement::SetText, FUN_140073490, FUN_140099c20
- [ ] **OnInputBindClick** (0x1400729a0) — opens inputBind/inputBound sub-dialogs, Input::KeyMap::SetInput/ClearInput/GetIdForDeviceInput, Input::Device::ResetState, ExternalFactory, multiple dialog ctors

### Additional Runtime exports needed

```
?ShowDialog@UserInterface@UI@@QEAAHV?$InternalPtr@VDialogInstance@UI@@@Object@@@Z
```

---

## Stage 5 — Miscellaneous exe dependencies in clean code (opus)

Not trampolines, but still read data from the exe address space.

- [ ] **optionsdialog.cpp** — Input::Configuration singleton (0x140151738) → resolve via `?_singleton@Configuration@Input@@0PEAV12@EA`
- [ ] **optionsdialog.cpp** — Float constants (0x1401720c4, 0x140172208) for status icon scale conversion → replace with literal constants
- [ ] **saveloaddialog.cpp** — Float constant (0x14017214c) → replace with literal constant
