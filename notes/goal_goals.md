# Goal Module — Reimplementation Status

**Directory:** `code/Game/Goal/`
**Type headers:** `engine/types/AchievementGoals.h`, `engine/types/TutorialGoals.h`
**Source files:** 10 `.cpp` + 10 `_funcs.h` (all 12 classes covered)

---

## Overview

Goals are achievement/scenario objectives. Each has a vtable with Check/IsMet/HasFailed methods. Driven by AchievementManager::Update.

Two hierarchies:
- **Achievement::Goal** → BuildingGoal, PopulationGoal, FinishTutorialGoal, StatGoal, TimeGoal
- **Achievement::GoalInstance** → FinishTutorialInstance, TimeGoalInstance
- **TutorialGoal** (Object::External) → TutorialGoalButton, TutorialGoalCount, TutorialGoalGameSpeed

---

## Achievement Goals

| Class | Vtable | Size | Reimpl Status | Notes |
|-------|--------|------|---------------|-------|
| BuildingGoal | 0x140160300 | 0x48 | **Complete** | All slots. Slot8 re-enabled (clean code bypasses broken trampoline) |
| PopulationGoal | 0x140160370 | 0x38 | **Complete** | All slots. Slot8 re-enabled (clean code bypasses broken trampoline) |
| FinishTutorialGoal | 0x140160420 | 0x38 | **Complete** | All slots. Null Slot5 crash fixed |
| StatGoal | (no vtable const) | 0x80 | **Complete** | All slots. 14 stat fields mapped. Slot10 disabled (near Jcc) but clean code done |
| TimeGoal | 0x140160540 | 0x38 | **Complete** | All slots. 1 year = 3600 seconds |

## Goal Instances

| Class | Vtable | Size | Reimpl Status | Notes |
|-------|--------|------|---------------|-------|
| FinishTutorialInstance | 0x140160478 | 0x28 | **Partial** | Slot3/Slot6 done. Slot4/Slot5 remain trampolines (complex tutorial subsystem deps) |
| TimeGoalInstance | 0x140160598 | 0x28 | **Complete** | All slots. Fixed bug class 6: Slot5 was missing R8=goalList param |

## Tutorial Goals

| Class | Vtable | Size | Reimpl Status | Notes |
|-------|--------|------|---------------|-------|
| TutorialGoal (base) | 0x1401607a0 | abstract | **Complete** | Dtor + Serialize |
| TutorialGoalButton | 0x140160800 | 0x30 | **Complete** | All 5 slots. Check reads UI element flags by path string |
| TutorialGoalCount | 0x140160950 | 0x50 | **Partial** | Dtor/Serialize/Cleanup/Init done. Slot8/Check delegates to original (11-case switch too entangled) |
| TutorialGoalGameSpeed | 0x1401608e0 | 0x28 | **Complete** | All 5 slots. Check compares game speed vs targetSpeed |

---

## Key Findings

### StatGoal Comparison Engine
- `comparisonType` field: even values = compare statA vs fixedValue, odd = compare statA vs statB
- Pairs: 0/1=EQ, 2/3=NE, 4/5=LT, 6/7=LE, 8/9=GT, 10/11=GE
- Stat dispatch (FUN_1400da4e0): 30-case switch reading various game world stats

### TimeGoal Time Scale
- 1 game year = 3600 real seconds (0xe10)
- `requiredYears * 3600` compared against elapsed seconds

### Bug Fixes Applied
- **TimeGoalInstance::Slot5** had implicit R8 forwarding (goalList param) that old passthrough missed (bug class 6)
- **BuildingGoal::Slot8** and **PopulationGoal::Slot8** re-enabled — clean reimpl never calls trampoline, so prologue issues (MOV R11,RSP / RIP-relative) are bypassed

### Remaining Work
- FinishTutorialInstance Slot4/Slot5: complex tutorial subsystem (TutorialInstance allocation, dialog management, step execution)
- TutorialGoalCount Slot8/Check: 11-case switch with 7 exe-internal helpers (~1500 bytes)

---

## Profile Source Paths

- `Goal\BuildingGoal.h`, `Goal\PopulationGoal.h`, `Goal\FinishTutorialGoal.h`
- `Goal\StatGoal.h`, `Goal\TimeGoal.h`
- `Goal\FinishTutorialGoal.cpp`, `Goal\StatGoal.cpp`

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| Achievement::Goal::~Goal | Runtime 0x18009edb0 | Base dtor |
| Achievement::Goal::Goal() | Runtime 0x18009ed50 | Base ctor |
| Achievement::GoalInstance::~GoalInstance | Runtime 0x18009ef40 | Base dtor |
| Achievement::GoalInstance::Persist | Runtime 0x18009ef70 | Serialization |
| Object::InternalBasePtr::Serialize | Runtime export | IBP serialization |
| AchievementManager::GetAchievement | Runtime 0x18009e8a0 | ExternalPtr return |
| Achievement::GetTitle/Description/SpriteName | Runtime | String returns |
| FUN_1400e1870 (FindUIElementByPath) | Exe | Used by TutorialGoalButton::Check |
| FUN_1400da4e0 (stat dispatch) | Exe | 30-case switch for StatGoal |
