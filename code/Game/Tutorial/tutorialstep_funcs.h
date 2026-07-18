#pragma once
// Function declarations for deeply reimplemented TutorialStep methods.
// Called from engine/reimpl/tutorialstep.cpp wrappers.

#include "TutorialStep.h"
#include <cstdint>

// ============================================================
// TutorialStep -- tutorial step definition
// Vtable: 0x140160e90 (9 slots)
// Size: 0x70 bytes, extends Object::External
// ============================================================

// Slot0 (vtable +0x00, 0x140028cc0): Scalar destructor.
// Calls TutorialStep_DtorBody, then optionally frees memory.
void* TutorialStep_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140028c40): GetTypeInfo (8b, unhookable).
// Returns: *(void**)(0x1401c67c0)
inline void* TutorialStep_GetTypeInfo(void* /*self*/) { return nullptr; }

// Slot2 (vtable +0x10, 0x140028c30): GetTypeId (7b, unhookable).
// Returns: *(uint32_t*)(0x1401c67b8) -- TypeInfo_TutorialStep
inline uint32_t TutorialStep_GetTypeId(void* /*self*/) { return 0; }

// Slot3 (vtable +0x18, 0x1400e33e0): GetDescription (6b, unhookable).
// Always returns 1.
inline int TutorialStep_GetDescription(void* /*self*/) { return 1; }

// Slot5 (vtable +0x28, 0x1400e32a0): Serialize.
// Serializes all fields: completed bool, stringTable exref, title/desc strings,
// and three dynamic arrays of TutorialImage/TutorialSetup/TutorialGoal externals.
void TutorialStep_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140028ca0): Cleanup.
// Calls scalar dtor with flags=0 (destroy without freeing), then re-initializes via Init.
void TutorialStep_Cleanup(void* self);

// Slot8 (vtable +0x40, 0x140028c90 -> 0x1400e3480): Init/Ctor.
// Calls External base constructor, sets vtable, zeros all fields.
void* TutorialStep_Init(void* self);
