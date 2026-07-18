#pragma once
// Function declarations for deeply reimplemented TutorialSetup methods.
// Called from engine/reimpl/tutorialsetup.cpp wrappers.

#include "TutorialSetups.h"
#include <cstdint>

// ============================================================
// TutorialSetup — abstract base for tutorial setup actions
// Vtable: 0x140160c30 (11 slots)
// Extends Object::External
// ============================================================

// Slot0 (vtable +0x00, 0x140028390): Scalar deleting destructor.
// Sets vtable to TutorialSetup, calls External::~External, optionally frees.
void* TutorialSetup_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140028380): GetTypeInfo — 8 bytes, unhookable.
// Slot2 (vtable +0x10, 0x140028370): GetTypeId — 7 bytes, unhookable.

// Slot3 (vtable +0x18, 0x1400e2040): GetDescription — returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int TutorialSetup_GetDescription(void* /*self*/) { return 1; }

// Slot4 (vtable +0x20, 0x14003a912): Serialize thunk — unhookable.

// Slot5 (vtable +0x28, 0x1400e2000): Serialize — creates Serializer, immediately destroys.
// Base class serialize does nothing (no fields in TutorialSetup beyond External base).
void TutorialSetup_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30): Cleanup — pure virtual (no-op in base, subclasses override).
// Slot7 (vtable +0x38): Slot7 — pure virtual (no-op in base).
// Slot8 (vtable +0x40): Apply — pure virtual (no-op in base).
// Slot9 (vtable +0x48): Undo — pure virtual (no-op in base).
// Slot10 (vtable +0x50): Slot10 — pure virtual (no-op in base).
