#pragma once
// Declarations for Goal-module exe-helper reimplementations.
// See code/Game/Goal/goal_exe_helpers.cpp.

#include <cstdint>

// FUN_1400265b0 — StatGoal lazy ExternalPtr resolver. void fn(externalRef).
void StatGoal_ResolveExternalPtr(void* externalRef);

// FUN_14001b340 - ExternalPtr-triple array resize (stride 0x18). void fn(arr, count).
void Goal_RefArray_Resize(void* arr, uint32_t newCount);

// ENDGAME B.3.8 block GOAL: `Goal_FillInheritedVtableSlots` (the by-name fill
// of slots 4 / 7 / 9 into the hand-built Goal tables) is gone with those
// tables — the Goal leaves are real classes, and Compile / GetInstanceType are
// inherited from Achievement::Goal, Destruct is each leaf's own member.
