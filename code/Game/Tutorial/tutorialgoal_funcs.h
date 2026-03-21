#pragma once
// Function declarations for deeply reimplemented TutorialGoal methods.
// Called from engine/reimpl/tutorialgoal.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "TutorialGoals.h"
#include <cstdint>

// ============================================================
// TutorialGoal — abstract base for tutorial goals
// Vtable: 0x1401607a0 (9 slots)
// Extends Object::External
// ============================================================

// Slot0 (vtable +0x00, 0x140027220): Scalar deleting destructor.
// Sets vtable to TutorialGoal, calls External::~External, optionally frees.
void* TutorialGoal_Dtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400dec70): GetDescription — returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int TutorialGoal_GetDescription(void* /*self*/) { return 1; }

// Slot5 (vtable +0x28, 0x1400dec30): Serialize — creates Serializer, immediately destroys.
// Base class serialize does nothing (no fields in TutorialGoal beyond External base).
void TutorialGoal_Serialize(void* self, void* stream);
