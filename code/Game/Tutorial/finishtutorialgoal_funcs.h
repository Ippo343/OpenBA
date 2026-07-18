#pragma once
// Function declarations for deeply reimplemented FinishTutorialGoal methods.
// Called from engine/reimpl/finishtutorialgoal.cpp wrappers.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// FinishTutorialGoal — achievement goal for completing tutorial
// Vtable: 0x140160420 (12 slots)
// Size: 0x38 bytes
// ============================================================

// Slot0 (vtable +0x00, 0x140026260): Scalar destructor.
// Sets vtable, calls Goal::~Goal, optionally frees memory.
void* FinishTutorialGoal_Dtor(FinishTutorialGoal* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x1400260e0): GetTypeInfo (8 bytes, unhookable).
// Returns: *(void**)(0x1401c6248)
inline void* FinishTutorialGoal_GetTypeInfo(FinishTutorialGoal* /*self*/) {
    // Reads type info pointer from exe .rdata
    // Resolved at runtime; for standalone exe this would be a static global
    return nullptr; // Placeholder — real impl reads from g_TypeInfo_FinishTutorialGoal
}

// Slot2 (vtable +0x10, 0x1400260d0): GetTypeId (7 bytes, unhookable).
// Returns: *(uint32_t*)(0x1401c6240)
inline uint32_t FinishTutorialGoal_GetTypeId(FinishTutorialGoal* /*self*/) {
    return 0; // Placeholder — real impl reads from g_TypeId_FinishTutorialGoal
}

// Slot3 (vtable +0x18, 0x1400d79d0): Returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int FinishTutorialGoal_Slot3(FinishTutorialGoal* /*self*/) { return 1; }

// Slot4 (vtable +0x20, 0x14003a912): Base class JMP thunk (6b, unhookable).
// Delegates to base Object::External::Compile.

// Slot5 (vtable +0x28, 0x1400d7970): Serialize/Load.
// Calls Goal::Serialize(base), then serializes the tutorial External* at +0x30.
int FinishTutorialGoal_Slot5(FinishTutorialGoal* self, void* stream);

// Slot6 (vtable +0x30, 0x1400261a0): Constructor (called during factory create).
// Calls Goal::Goal() base ctor, sets vtable, zeroes +0x30.
// NOTE: Ghidra names this "Draw" but it's really a copy-ctor / factory init.
FinishTutorialGoal* FinishTutorialGoal_Slot6(FinishTutorialGoal* self);

// Slot7 (vtable +0x38, 0x140001640): Base class JMP thunk (7b, unhookable).

// Slot8 (vtable +0x40, 0x1400d7960): Check goal — always returns 0.
// Original is "xor eax,eax; ret" (3 bytes, unhookable).
inline int FinishTutorialGoal_Slot8(FinishTutorialGoal* /*self*/) { return 0; }

// Slot9 (vtable +0x48, 0x1400261f0): GetInstanceType (12b, unhookable).
// Returns type ID for FinishTutorialInstance.
// Original: mov edx, [TypeInfo_FinishTutorialInstance]; mov [rdx_retval], edx; mov rax, rdx; ret

// Slot10 (vtable +0x50, 0x140026150): CreateInstance — returns new FinishTutorialGoal.
// Calls Goal::Goal(), sets vtable, zeroes +0x30.
FinishTutorialGoal* FinishTutorialGoal_CreateInstance(FinishTutorialGoal* self);

// Slot11 (vtable +0x58, 0x140026200): FinishTutorialInstance scalar dtor.
// SHARED with FinishTutorialInstance::Dtor — same address.
// Destroys FinishTutorialInstance: sets vtable, deletes IBP at +0x20, calls GoalInstance::~GoalInstance.
void* FinishTutorialGoal_Slot11(void* self, uint32_t flags);
