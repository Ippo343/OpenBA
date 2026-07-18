#pragma once
// Function declarations for deeply reimplemented TutorialSetupCamera methods.
// Called from engine/reimpl/tutorialsetupcamera.cpp wrappers.

#include "TutorialSetups.h"
#include <cstdint>

// ============================================================
// TutorialSetupCamera — sets camera position for tutorial steps
// Vtable: 0x140160ca0 (12 slots)
// Extends TutorialSetup (which extends Object::External)
// Size: 0x30
// ============================================================

// Slot0 (vtable +0x00, 0x140028530): Scalar deleting destructor.
void* TutorialSetupCamera_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x1400283f0): GetTypeInfo — 8 bytes, unhookable.
// Returns DAT_1401c66e0 (type info pointer).

// Slot2 (vtable +0x10, 0x1400283e0): GetTypeId — 7 bytes, unhookable.
// Returns TypeInfo_TutorialSetupCamera.

// Slot3 (vtable +0x18, 0x1400e21b0): GetDescription — returns constant 1.
inline int TutorialSetupCamera_GetDescription(void* /*self*/) { return 1; }

// Slot4 (vtable +0x20, 0x14003a912): Serialize thunk — unhookable.

// Slot5 (vtable +0x28, 0x1400e2120): Serialize — serializes 4 int32 camera fields.
void TutorialSetupCamera_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x1400284d0): Cleanup — destroy and reinitialize.
void TutorialSetupCamera_Cleanup(void* self);

// Slot7 (vtable +0x38, 0x140001640): Base dispatch — 8 bytes, unhookable.

// Slot8 (vtable +0x40, 0x1400e20d0): Apply — sets camera position from fields.
void TutorialSetupCamera_Slot8(void* self);

// Slot9 (vtable +0x48, 0x1400e20c0): No-op, returns 0.
inline int TutorialSetupCamera_Slot9(void* /*self*/) { return 0; }

// Slot10 (vtable +0x50, 0x1400e20b0): No-op, returns 0.
inline int TutorialSetupCamera_Slot10(void* /*self*/) { return 0; }

// Slot11 (vtable +0x58, 0x140028470): Ctor/Init — construct from raw memory.
void* TutorialSetupCamera_Ctor(void* self);
