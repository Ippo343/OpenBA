#pragma once
// Function declarations for deeply reimplemented TutorialGoalButton methods.
// Called from engine/reimpl/tutorialgoalbutton.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "TutorialGoals.h"
#include <cstdint>

// ============================================================
// TutorialGoalButton — wait for UI element state
// Vtable: 0x140160800 (10 slots)
// Size: 0x30 bytes
// Extends TutorialGoal -> Object::External
//
// Fields:
//   +0x20: System::String elementPath — UI element path to find
//   +0x28: bool invertCheck — which bit of flags to check
// ============================================================

// Slot0 (vtable +0x00, 0x1400273b0): Scalar deleting destructor.
// Destroys String at +0x20, calls TutorialGoal::Dtor, optionally frees.
void* TutorialGoalButton_Dtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400dee30): GetDescription — returns constant 2.
// Original is "mov eax,2; ret" (6 bytes, unhookable).
inline int TutorialGoalButton_GetDescription(void* /*self*/) { return 2; }

// Slot5 (vtable +0x28, 0x1400deda0): Serialize.
// Calls base TutorialGoal::Serialize (empty), then serializes:
//   - String at +0x20 (elementPath)
//   - bool at +0x28 (invertCheck)
void TutorialGoalButton_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140027350): Cleanup.
// Calls ScalarDtor(self, 0) via vtable, then re-initializes via Init.
void TutorialGoalButton_Cleanup(void* self);

// Slot8 (vtable +0x40, 0x1400dece0): Check — test if UI element meets condition.
// Finds the UI element by elementPath (+0x20) using FUN_1400e1870.
// Then checks element flags at +0x34:
//   - If invertCheck (+0x28): returns ~(flags >> 23) & 1  (bit 23 NOT set)
//   - If !invertCheck: returns (flags >> 22) & 1  (bit 22 set)
// Returns 0 if element not found or has no matching internal object.
int TutorialGoalButton_Check(void* self);

// Slot9 (vtable +0x48, 0x140027300): Init.
// Null-checks self, calls External::External(self),
// sets vtable chain (TutorialGoal -> TutorialGoalButton),
// zeroes elementPath (+0x20) and invertCheck (+0x28).
void* TutorialGoalButton_Init(void* self);
