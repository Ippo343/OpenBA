#pragma once
// Function declarations for deeply reimplemented TimeGoal methods.
// Called from engine/reimpl/timegoal.cpp wrappers.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// TimeGoal — achievement goal requiring time elapsed
// Vtable: 0x140160540 (12 slots)
// Size: 0x38 bytes
// ============================================================

// Slot3 (vtable +0x18, 0x1400daba0): Returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int TimeGoal_Slot3(TimeGoal* /*self*/) { return 1; }

// Slot4 (vtable +0x20, 0x14003a912): Base class JMP thunk (6 bytes, unhookable).

// Slot5/Load (vtable +0x28, 0x1400dab20): Serialize goal fields.
// Calls base Goal::Serialize, then serializes requiredYears and requireOtherGoalsMet.
void TimeGoal_Slot5(TimeGoal* self, void* stream);

// Slot6/Cleanup (vtable +0x30, 0x1400267a0): Reset to default state.
// Calls scalar dtor with flags=0 then re-constructs.
void TimeGoal_Slot6(TimeGoal* self);

// Slot7 (vtable +0x38, 0x140001640): Base class stub (7 bytes, unhookable).

// Slot8 (vtable +0x40, 0x1400dab10): Returns 0.
// Original is "xor eax,eax; ret" (3 bytes, unhookable).
inline int TimeGoal_Slot8(TimeGoal* /*self*/) { return 0; }

// Slot9 (vtable +0x48, 0x1400267f0): Returns TypeInfo for TimeGoalInstance.
// Original is "mov eax,[rip+TypeInfo]; mov [rdx],eax; mov rax,rdx; ret" (12 bytes, unhookable).
// This returns the type ID of the GoalInstance class to create.
// Implemented inline since it just reads a global.
inline void* TimeGoal_Slot9(TimeGoal* /*self*/, void* retval) {
    // Read TypeInfo_TimeGoalInstance from 0x1401c6320
    // At runtime, this reads the type ID and stores it at *retval.
    // Stub: the type-id read is not implemented here; returns retval unchanged.
    return retval;
}

// Slot10/CreateInstance (vtable +0x50, 0x140026750): Allocate + construct TimeGoal.
void* TimeGoal_Slot10(void* memory);

// Slot11/SharedDtor (vtable +0x58, 0x140026800): Shared destructor with TimeGoalInstance::Dtor.
// Sets vtable to TimeGoalInstance, calls base GoalInstance::~GoalInstance, optionally frees.
void* TimeGoal_Slot11(void* self, uint32_t flags);

// Dtor (vtable +0x00, 0x140026850): TimeGoal destructor.
// Sets vtable to TimeGoal, calls base Goal::~Goal, optionally frees.
void* TimeGoal_Dtor(TimeGoal* self, uint32_t flags);

// TimeGoal_Ctor_Impl: Construct in place.
void* TimeGoal_Ctor_Impl(void* memory);
