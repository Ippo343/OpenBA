# Tutorial Module — Reimplementation Goals

**Directory:** `code/Game/Tutorial/`
**Type headers:** `Tutorial.h`, `TutorialGoals.h`, `TutorialImages.h`, `TutorialSetups.h`, `TutorialStep.h`
**Source files:** None yet (empty directory)

---

## Overview

The tutorial system is a state machine of TutorialSteps, each with setup actions (camera, dialog, restrict player), goal checks (button click, count, game speed), and image overlays (hotkey icons, toolbar icons, input keys). Driven by GameUpdateLoop step 3g.

---

## Classes

### Core
| Class | Vtable | Reimpl Status | Priority |
|-------|--------|---------------|----------|
| Tutorial | 0x140160740 | Not started | Low |
| TutorialStep | 0x140160e90 | Not started | Low |
| TutorialInstance | - | Not started | Low |

### Goals (4 types)
| Class | Vtable | Notes |
|-------|--------|-------|
| TutorialGoalButton | 0x1401607a0 | Check if player clicked a button |
| TutorialGoalCount | 0x140160800 | Check if count reached |
| TutorialGoalGameSpeed | 0x1401608e0 | Check game speed setting |
| TutorialGoalDialog | 0x140160950 | Check dialog state |

### Setups (5 types)
| Class | Vtable | Notes |
|-------|--------|-------|
| TutorialSetupCamera | 0x140160c30 | Move camera to location |
| TutorialSetupDialog | 0x140160ca0 | Open/close a dialog |
| TutorialSetupGameSpeed | 0x140160da0 | Force game speed |
| TutorialSetupRestrictPlayer | 0x140160e20 | Lock/unlock player tools |
| TutorialSetupDialog2 | 0x140160d20 | Variant |

### Images (4 types)
| Class | Vtable | Notes |
|-------|--------|-------|
| TutorialImage | 0x140160a40 | Base image overlay |
| TutorialImageHotKey | 0x140160ab0 | Show hotkey icon |
| TutorialImageInputKey | 0x140160b20 | Show input key |
| TutorialImageToolbarIcon | 0x140160b90 | Show toolbar icon |

---

## Profile Source Paths

- `Tutorial\Tutorial.h`, `Tutorial\TutorialInstance.cpp`
- `Tutorial\TutorialSetupDialog.cpp`, `Tutorial\TutorialUI.cpp`
- All goal/setup/image .h files listed in PROFILE_ANALYSIS.md

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| UI::DialogInstance | Runtime | Dialog management |
| Input::Device | Runtime | Key state queries |
| Camera control | Runtime | Camera positioning |
