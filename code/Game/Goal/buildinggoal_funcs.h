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

// Slot0 (vtable +0x00, 0x140025e80): Scalar destructor.
// Sets vtable, destroys building type array at +0x38, calls Goal::~Goal,
// optionally frees memory.
void* BuildingGoal_Dtor(BuildingGoal* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140025d70): GetTypeInfo (8b, unhookable).
// Returns: *(void**)(0x1401c61d8)
inline void* BuildingGoal_GetTypeInfo(BuildingGoal* /*self*/) {
    return nullptr; // Placeholder
}

// Slot2 (vtable +0x10, 0x140025d60): GetTypeId (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c61d0)
inline uint32_t BuildingGoal_GetTypeId(BuildingGoal* /*self*/) {
    return 0; // Placeholder
}

// Slot3 (vtable +0x18, 0x1400d7790): Returns constant 2.
// Original is "mov eax,2; ret" (6 bytes, unhookable).
inline int BuildingGoal_Slot3(BuildingGoal* /*self*/) { return 2; }

// Slot4 (vtable +0x20, 0x14003a912): Base class JMP thunk (6b, unhookable).

// Slot5 (vtable +0x28, 0x1400d76a0): Serialize/Load.
// Calls Goal::Serialize(base), then serializes requireAll (bool at +0x30),
// entry count, and the building type array entries (string + hash).
int BuildingGoal_Slot5(BuildingGoal* self, void* stream);

// Slot6 (vtable +0x30, 0x140025e30): Constructor (factory create).
// Calls Goal::Goal() base ctor, sets vtable, initializes fields:
//   +0x30 = 1 (requireAll), +0x38 = 0 (array ptr), +0x40 = 0 (count).
// Returns null if input is null.
BuildingGoal* BuildingGoal_Slot6(BuildingGoal* self);

// Slot7 (vtable +0x38, 0x140001640): Base class JMP thunk (7b, unhookable).

// Slot8 (vtable +0x40, 0x1400d75d0/0x1400d7580): Check goal completion.
// Iterates building type entries in the goal's array (+0x38/+0x40),
// looks up each building type in the world's building BST at game+0xF0+0x160,
// counts how many have been built. Returns:
//   - If _field30 (requireAll): 1 if all matched, 0 otherwise
//   - If !_field30 (requireAny): 1 if none matched, 2 if some matched, 0 otherwise
// NOTE: Hook is DISABLED (MOV R11,RSP prologue — bug class 3).
int BuildingGoal_Slot8(BuildingGoal* self);

// Slot9 (vtable +0x48, 0x14003ab5e): Base class GetInstanceType JMP thunk.

// Slot10 (vtable +0x50, 0x140025de0): CreateInstance — same as Slot6.
BuildingGoal* BuildingGoal_CreateInstance(BuildingGoal* self);
