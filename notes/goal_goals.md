# Goal Module — Reimplementation Goals

**Directory:** `code/Game/Goal/`
**Type headers:** `engine/types/AchievementGoals.h`
**Source files:** None yet (empty directory)

---

## Overview

Goals are achievement/scenario objectives. Each has a vtable with Check/IsMet/HasFailed methods. Driven by AchievementManager::Update (currently disabled in GameUpdateLoop due to null vtable crash).

---

## Classes

| Class | Vtable | Type Header | Reimpl Status | Priority |
|-------|--------|-------------|---------------|----------|
| BuildingGoal | 0x140160300 | AchievementGoals.h | Not started | Medium |
| PopulationGoal | 0x140160370 | AchievementGoals.h | Not started | Medium |
| FinishTutorialGoal | 0x140160540 | AchievementGoals.h | Not started | High (blocks AchievementManager) |
| TimeGoal | 0x140160420 | AchievementGoals.h | Not started | Medium |
| StatGoal | 0x140160478 | AchievementGoals.h | Not started | Medium |
| AchievementGoal6 | 0x140160598 | AchievementGoals.h | Not started | Low |

---

## Blocking Issue

**FinishTutorialGoal** has a null vtable entry at Slot5 that crashes AchievementManager::Update. This is the highest-priority goal class to decompile — fixing it unblocks achievement tracking in the game.

## Profile Source Paths

- `Goal\BuildingGoal.h`, `Goal\PopulationGoal.h`, `Goal\FinishTutorialGoal.h`
- `Goal\StatGoal.h`, `Goal\TimeGoal.h`
- `Goal\FinishTutorialGoal.cpp`, `Goal\StatGoal.cpp`

## Runtime Dependencies

| Function | Source | Notes |
|----------|--------|-------|
| AchievementManager::GetAchievement | Runtime 0x18009e8a0 | ExternalPtr return |
| Achievement::GetTitle/Description/SpriteName | Runtime | String returns |
