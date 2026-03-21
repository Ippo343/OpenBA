#pragma once
// Function declarations for deeply reimplemented TimeGoalInstance methods.
// Called from engine/reimpl/timegoalinstance.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// TimeGoalInstance — goal instance tracking elapsed time
// Vtable: 0x140160598 (7 slots)
// Size: 0x28 bytes
// ============================================================

// Dtor (vtable +0x00, 0x140026800): SHARED with TimeGoal::Slot11.
// See timegoal_funcs.h / timegoal.cpp — TimeGoal_Slot11.

// GetTypeInfo (vtable +0x08, 0x140026660): 8 bytes, unhookable.
// GetTypeId (vtable +0x10, 0x140026650): 7 bytes, unhookable.

// Slot3/Serialize (vtable +0x18, 0x1400dad30): Serialize instance state.
// Calls base GoalInstance::Persist, then serializes elapsed time via Time::Serialize.
void TimeGoalInstance_Slot3(TimeGoalInstance* self, void* stream);

// Slot4 (vtable +0x20, 0x1400daca0): Returns 0 via "ret 0x0" (3 bytes, unhookable).
// This appears to be a "RET 0" instruction (C2 00 00).
inline int TimeGoalInstance_Slot4(TimeGoalInstance* /*self*/) { return 0; }

// Slot5/Startup (vtable +0x28, 0x1400dac30): Update goal instance.
// Accumulates elapsed time, checks if other goals are met (via AreGoalsMetExcept),
// resets time to zero if not. Then calls base GoalInstance::Update.
// Parameters: self, time (Time*), goalList (GoalList*)
void TimeGoalInstance_Slot5(TimeGoalInstance* self, void* time, void* goalList);

// Slot6/Shutdown (vtable +0x30, 0x1400dacb0): Check if time goal is complete.
// Casts the goal at +0x18 to TimeGoal (via IsDerivedFrom), reads requiredYears,
// converts elapsed time to seconds, compares against requiredYears * 3600.
// Returns: 1 if elapsed >= required, 0 otherwise.
int TimeGoalInstance_Slot6(TimeGoalInstance* self);
