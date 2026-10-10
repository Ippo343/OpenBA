# Tutorial Module — Reimplementation Status

**Directory:** `code/Game/Tutorial/` (38 source files)
**Type headers:** `Tutorial.h`, `TutorialGoals.h`, `TutorialImages.h`, `TutorialSetups.h`, `TutorialStep.h`, `TutorialInstance.h`

---

## Overview

The tutorial system is a state machine of TutorialSteps, each with setup actions (camera, dialog, restrict player), goal checks (button click, count, game speed), and image overlays (hotkey icons, toolbar icons, input keys). Driven by GameUpdateLoop step 3g.

**Status:** All 17 tutorial classes fully reimplemented as clean C++. All 57 hooks enabled. No remaining trampolines or delegates — module 100% complete (2026-03-29).

---

## Classes

### Core
| Class | Vtable | Hooks | Reimpl Status |
|-------|--------|-------|---------------|
| Tutorial | 0x140160740 | 4 (Dtor, Serialize, Cleanup, Ctor) | **Done** — all clean C++ |
| TutorialStep | 0x140160e90 | 4 (Dtor, Serialize, Cleanup, Init) | **Done** — all clean C++ |
| TutorialInstance | 0x140160bf8 | 2 (Dtor, Serialize) | **Done** — all clean C++ |

### Goals (4 types)
| Class | Vtable | Hooks | Reimpl Status |
|-------|--------|-------|---------------|
| TutorialGoal (base) | 0x1401607a0 | 2 (Dtor, Serialize) | **Done** — all clean C++ |
| TutorialGoalButton | 0x1401607a0 | 5 | **Done** — all clean C++ |
| TutorialGoalCount | 0x140160800 | 5 | **Done** — all clean C++ (11 count modes) |
| TutorialGoalGameSpeed | 0x1401608e0 | 5 | **Done** — all clean C++ |
| TutorialGoalDialog | 0x140160950 | 5 | **Done** — all clean C++ |

### Setups (5 types)
| Class | Vtable | Hooks | Reimpl Status |
|-------|--------|-------|---------------|
| TutorialSetup (base) | 0x140160c30 | 2 (Dtor, Serialize) | **Done** — all clean C++ |
| TutorialSetupCamera | 0x140160ca0 | 5 | **Done** — camera pos from chunk*cellSize+offset |
| TutorialSetupDialog | 0x140160d20 | 6 | **Done** — all clean C++ |
| TutorialSetupGameSpeed | 0x140160da0 | 5 | **Done** — writes speed to updateLoop+0x34 |
| TutorialSetupRestrictPlayer | 0x140160e20 | 6 | **Done** — tool/camera restrictions via PlayerController |

### Images (4 types)
| Class | Vtable | Hooks | Reimpl Status |
|-------|--------|-------|---------------|
| TutorialImage (base) | 0x140160a40 | 5 (Dtor, Serialize, Cleanup, Ctor, Slot9) | **Done** — all clean C++ |
| TutorialImageHotKey | 0x140160ab0 | 5 | **Done** — all clean C++ |
| TutorialImageInputKey | 0x140160b20 | 5 | **Done** — all clean C++ |
| TutorialImageToolbarIcon | 0x140160b90 | 5 | **Done** — all clean C++ |

### Achievement Goals
| Class | Vtable | Hooks | Reimpl Status |
|-------|--------|-------|---------------|
| FinishTutorialGoal | 0x1401604b8 | 4 | **Done** — all clean C++ |
| FinishTutorialInstance | 0x140160478 | 5 | **Done** — Slot5 re-enabled 2026-03-21 |

### Dialog UI
| Class | Hooks | Reimpl Status |
|-------|-------|---------------|
| TutorialUI | 4 (ScalarDtor, Serialize, Startup, OnRemove) | **Done** — quit-to-menu logic |
| TutorialHighlightDialog | 4 (ScalarDtor, Startup, Update, Load) | **Done** |

---

## Remaining Work

**All items complete as of 2026-03-29.**

### ~~Slot9 (Image display) — still uses original code~~ DONE
All three Slot9 methods (TutorialImageHotKey, TutorialImageInputKey, TutorialImageToolbarIcon) reimplemented as clean C++ in `code/Game/Tutorial/`. Added `fn_Dialog_CreateElement` (dispatch slot 0x1401528c0) and `fn_GetInputKeyName` (exe 0x140099c20) to engine_funcs.

### ~~Tutorial::Serialize — still uses trampoline~~ DONE (was already complete)
Tutorial::Serialize was already reimplemented as clean C++ in `code/Game/Tutorial/tutorial.cpp:194-242`. The reimpl wrapper calls it directly — no trampoline.

### ~~TutorialGoalDialog — not split to _funcs.h~~ DONE
Split to `code/Game/Tutorial/tutorialgoaldialog.cpp` + `tutorialgoaldialog_funcs.h`. The reimpl wrapper now delegates to clean code functions.

---

## Discovered Field Names (from decompilation)

### Tutorial.h
- `+0x20`: setup (ExternalPtr)
- `+0x28`: steps (array pointer)
- `+0x30`: stepCount
- `+0x38`: currentStep (int32, -1 sentinel)

### TutorialStep.h
- `+0x20`: completed (bool)
- `+0x28`: stringTable (ExternalPtr)
- `+0x30/+0x38`: title/desc strings
- `+0x40/+0x50/+0x60`: images/setups/goals DynArrays with counts at +8

### TutorialInstance.h (new)
- 0x38 bytes, extends Object::Internal
- `+0x10`: currentStep (int32, -1 init)
- `+0x18/+0x20`: InternalBasePtrs
- `+0x30`: tutorial def pointer

### TutorialImages.h
- TutorialImage: `+0x20` imageRef, `+0x28` imageWidth (int32), `+0x2C` imageHeight (int32)
- TutorialImageHotKey: `+0x30` String, `+0x38` keyId (int32, -1), `+0x40` ptr
- TutorialImageInputKey: `+0x30` inputId
- TutorialImageToolbarIcon: `+0x30` String, `+0x38` tag, `+0x40` toolbar ptr

### TutorialSetups.h
- TutorialSetupCamera: 4x int32 (cameraX, cameraChunkRow, cameraY, cameraChunkCol)
- TutorialSetupGameSpeed: `+0x20` speed (int32), `+0x24` pause flag (bool)
- TutorialSetupRestrictPlayer: restriction masks, tool/camera ExternalRefs

---

## Profile Source Paths

- `Tutorial\Tutorial.h`, `Tutorial\TutorialInstance.cpp`
- `Tutorial\TutorialSetupDialog.cpp`, `Tutorial\TutorialUI.cpp`
- All goal/setup/image .h files listed in PROFILE_ANALYSIS.md

## Runtime Dependencies

| Function | Source | Status |
|----------|--------|--------|
| UI::DialogInstance | Runtime | Used via dispatch slots |
| Input::Device / KeyMap | Runtime | Needed for Image Slot9 full reimpl |
| Camera control | Runtime | Resolved locally in SetupCamera |
| IO::Serializer | Runtime | Resolved in engine_funcs.h |
| Object::External/Internal | Runtime | Resolved in engine_funcs.h |
