#pragma once
// Function declarations for deeply reimplemented BuildingGoal methods.
// Called from engine/reimpl/buildinggoal.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// BuildingGoal — achievement goal requiring building count
// Vtable: 0x140160300 (11 slots)
// Size: 0x48 bytes
// ============================================================

// Slot3 (vtable +0x18, 0x1400d7790): Returns constant 2.
// Original is "mov eax,2; ret" (6 bytes, unhookable).
inline int BuildingGoal_Slot3(BuildingGoal* /*self*/) { return 2; }

// Slot8 (vtable +0x40, 0x1400d75d0/0x1400d7580): Check goal completion.
// Iterates building type entries in the goal's array (+0x38/+0x40),
// looks up each building type in the world's building BST at game+0xF0+0x160,
// counts how many have been built. Returns:
//   - If _field30 (requireAll): 1 if all matched, 0 otherwise
//   - If !_field30 (requireAny): 1 if none matched, 2 if some matched, 0 otherwise
// NOTE: Hook is DISABLED (MOV R11,RSP prologue — bug class 3).
int BuildingGoal_Slot8(BuildingGoal* self);
