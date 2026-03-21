#pragma once
// Function declarations for deeply reimplemented FinishTutorialInstance methods.
// Called from engine/reimpl/finishtutorialinstance.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// FinishTutorialInstance — goal instance for tutorial completion
// Vtable: 0x140160478 (7 slots)
// Size: 0x28 bytes
// ============================================================

// Slot0 (vtable +0x00, 0x140026200): Scalar destructor.
// SHARED with FinishTutorialGoal::Slot11 — implemented in finishtutorialgoal.cpp.
// Declared here for reference:
// void* FinishTutorialInstance_Dtor(FinishTutorialInstance* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140026060): GetTypeInfo (8b, unhookable).
// Returns: *(void**)(0x1401c6280)
inline void* FinishTutorialInstance_GetTypeInfo(FinishTutorialInstance* /*self*/) {
    return nullptr; // Placeholder
}

// Slot2 (vtable +0x10, 0x140026050): GetTypeId (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c6278)
inline uint32_t FinishTutorialInstance_GetTypeId(FinishTutorialInstance* /*self*/) {
    return 0; // Placeholder
}

// Slot3 (vtable +0x18, 0x1400d7bf0): Serialize/Persist.
// Calls GoalInstance::Persist(base), creates Serializer, serializes IBP at +0x20,
// then type-checks the deserialized object against TutorialInstance.
void FinishTutorialInstance_Slot3(FinishTutorialInstance* self, void* stream);

// Slot4 (vtable +0x20, 0x1400d7a90): Startup/Load.
// Allocates TutorialInstance (0x38 bytes), initializes IBP fields,
// copies tutorial definition from parent FinishTutorialGoal, registers
// tutorial dialog via tree insert, sets game world flags.
void FinishTutorialInstance_Slot4(FinishTutorialInstance* self, void* p2, void* p3);

// Slot5 (vtable +0x28, 0x1400d7a50): Execute/Update step.
// Checks IBP at +0x20, delegates to tutorial step execution helper
// (FinishTutorialInstance_helper_1db0). R8/R9 are implicitly forwarded.
void FinishTutorialInstance_Slot5(FinishTutorialInstance* self, void* p2, void* p3, void* p4);

// Slot6 (vtable +0x30, 0x1400d7bb0): Check/Shutdown.
// Checks IBP at +0x20, reads tutorial step count vs current step.
// Returns whether current step >= total steps in tutorial.
int FinishTutorialInstance_Slot6(FinishTutorialInstance* self);
