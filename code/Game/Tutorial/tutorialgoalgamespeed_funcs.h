#pragma once
// Function declarations for deeply reimplemented TutorialGoalGameSpeed methods.
// Called from engine/reimpl/tutorialgoalgamespeed.cpp wrappers.

#include "TutorialGoals.h"
#include <cstdint>

// ============================================================
// TutorialGoalGameSpeed — wait for game speed to match target
// Vtable: 0x1401608e0 (10 slots)
// Size: 0x28 bytes
// Extends TutorialGoal -> Object::External
//
// Fields:
//   +0x20: int32_t targetSpeed — target game speed (default 1)
// ============================================================

// Slot0 (vtable +0x00, 0x140027730): Scalar deleting destructor.
// Sets vtable to GameSpeed, then TutorialGoal, calls External::~External, optionally frees.
// No subclass fields to destroy (targetSpeed is a plain int).
void* TutorialGoalGameSpeed_Dtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400df1f0): GetDescription — returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int TutorialGoalGameSpeed_GetDescription(void* /*self*/) { return 1; }

// Slot5 (vtable +0x28, 0x1400df170): Serialize.
// Calls base TutorialGoal::Serialize, then serializes int at +0x20 (targetSpeed).
void TutorialGoalGameSpeed_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x1400276e0): Cleanup.
// Calls ScalarDtor(self, 0) via vtable, then re-initializes.
void TutorialGoalGameSpeed_Cleanup(void* self);

// Slot8 (vtable +0x40, 0x1400df150): Check — test if game speed matches target.
// Reads current game speed from game->updateLoop(+0x120)->speed(+0x34).
// Returns 1 if speed == targetSpeed, 0 otherwise.
// NOTE: Only 18 bytes in original, RIP-relative.
int TutorialGoalGameSpeed_Check(TutorialGoalGameSpeed* self);

// Slot9 (vtable +0x48, 0x140027690): Init.
// Calls External::External, sets vtable chain, sets targetSpeed=1.
void* TutorialGoalGameSpeed_Init(void* self);
