#pragma once
// Function declarations for deeply reimplemented PopulationGoal methods.
// Called from engine/reimpl/populationgoal.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// PopulationGoal — achievement goal requiring population count
// Vtable: 0x140160370 (11 slots)
// Size: 0x38 bytes
// ============================================================

// Slot0 (vtable +0x00, 0x140026000): Scalar destructor.
// Sets vtable, calls Goal::~Goal, optionally frees memory.
void* PopulationGoal_Dtor(PopulationGoal* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140025ef0): GetTypeInfo (8b, unhookable).
// Returns: *(void**)(0x1401c6210)
inline void* PopulationGoal_GetTypeInfo(PopulationGoal* /*self*/) {
    return nullptr; // Placeholder
}

// Slot2 (vtable +0x10, 0x140025ee0): GetTypeId (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c6208)
inline uint32_t PopulationGoal_GetTypeId(PopulationGoal* /*self*/) {
    return 0; // Placeholder
}

// Slot3 (vtable +0x18, 0x1400d78e0): Returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int PopulationGoal_Slot3(PopulationGoal* /*self*/) { return 1; }

// Slot4 (vtable +0x20, 0x14003a912): Base class JMP thunk (6b, unhookable).

// Slot5 (vtable +0x28, 0x1400d7870): Serialize/Load.
// Calls Goal::Serialize(base), then serializes targetPop (int at +0x30).
int PopulationGoal_Slot5(PopulationGoal* self, void* stream);

// Slot6 (vtable +0x30, 0x140025fb0): Constructor (factory create).
// Calls Goal::Goal() base ctor, sets vtable, initializes targetPop to 100.
// Returns null if input is null.
PopulationGoal* PopulationGoal_Slot6(PopulationGoal* self);

// Slot7 (vtable +0x38, 0x140001640): Base class JMP thunk (7b, unhookable).

// Slot8 (vtable +0x40, 0x1400d7820): Check goal completion.
// Checks if current population >= targetPop.
// Reads g_pGameWorldSingleton, checks IsRunning, compares world pop at
// game+0xF0+0xA8 against targetPop at self+0x30.
// Returns 1 if goal met (pop >= target), 0 otherwise.
// NOTE: Hook is DISABLED (RIP-relative MOV in prologue — bug class 2).
int PopulationGoal_Slot8(PopulationGoal* self);

// Slot9 (vtable +0x48, 0x14003ab5e): Base class GetInstanceType JMP thunk.

// Slot10 (vtable +0x50, 0x140025f60): CreateInstance — same as Slot6.
PopulationGoal* PopulationGoal_CreateInstance(PopulationGoal* self);
