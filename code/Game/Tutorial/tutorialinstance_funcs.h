#pragma once
// Function declarations for deeply reimplemented TutorialInstance methods.
// Called from engine/reimpl/tutorialinstance.cpp wrappers.

#include "TutorialInstance.h"
#include <cstdint>

// ============================================================
// TutorialInstance -- tutorial runtime instance
// Vtable: 0x140160bf8 (4 slots)
// Size: 0x38 bytes, extends Object::Internal
// ============================================================

// Slot0 (vtable +0x00, 0x1400281a0): Scalar destructor.
// Sets vtable, calls Internal::~Internal, optionally frees memory.
void* TutorialInstance_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140028110): GetTypeInfo (8b, unhookable).
inline void* TutorialInstance_GetTypeInfo(void* /*self*/) { return nullptr; }

// Slot2 (vtable +0x10, 0x140028100): GetTypeId (7b, unhookable).
inline uint32_t TutorialInstance_GetTypeId(void* /*self*/) { return 0; }

// Slot3 (vtable +0x18, 0x1400e1520): Serialize.
// Serializes ExternalPtr<Tutorial> at +0x30, int currentStep at +0x10.
// On load, adjusts currentStep backwards if steps are not completed.
void TutorialInstance_Serialize(void* self, void* stream);
