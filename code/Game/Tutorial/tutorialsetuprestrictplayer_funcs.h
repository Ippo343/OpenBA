#pragma once
// Function declarations for deeply reimplemented TutorialSetupRestrictPlayer methods.
// Called from engine/reimpl/tutorialsetuprestrictplayer.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "TutorialSetups.h"
#include <cstdint>

// ============================================================
// TutorialSetupRestrictPlayer — restricts player camera/tools during tutorials
// Vtable: 0x140160e20 (12 slots)
// Extends TutorialSetup (which extends Object::External)
// Size: 0x68
// ============================================================

// Slot0 (vtable +0x00, 0x140028bb0): Scalar deleting destructor.
void* TutorialSetupRestrictPlayer_Dtor(void* self, uint32_t flags);

// Slot1 (vtable +0x08, 0x140028b30): GetTypeInfo — 8 bytes, unhookable.
// Returns DAT_1401c6788 (type info pointer).

// Slot2 (vtable +0x10, 0x140028b20): GetTypeId — 7 bytes, unhookable.
// Returns TypeInfo_TutorialSetupRestrictPlayer.

// Slot3 (vtable +0x18, 0x1400e31b0): GetDescription — returns constant 4.
inline int TutorialSetupRestrictPlayer_GetDescription(void* /*self*/) { return 4; }

// Slot4 (vtable +0x20, 0x14003a912): Serialize thunk — unhookable.

// Slot5 (vtable +0x28, 0x1400e3090): Serialize — serializes restriction fields + external refs.
void TutorialSetupRestrictPlayer_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140028b90): Cleanup — destroy and reinitialize.
void TutorialSetupRestrictPlayer_Cleanup(void* self);

// Slot7 (vtable +0x38, 0x140001640): Base dispatch — 8 bytes, unhookable.

// Slot8 (vtable +0x40, 0x1400e2f20): Apply — applies player restrictions to game state.
void TutorialSetupRestrictPlayer_Slot8(void* self);

// Slot9 (vtable +0x48, 0x1400e2f10): No-op, returns 0.
inline int TutorialSetupRestrictPlayer_Slot9(void* /*self*/) { return 0; }

// Slot10 (vtable +0x50, 0x1400e2e90): Undo — removes player restrictions from game state.
void TutorialSetupRestrictPlayer_Slot10(void* self);

// Slot11 (vtable +0x58, 0x140028b80): Ctor/Init — construct from raw memory.
void* TutorialSetupRestrictPlayer_Ctor(void* self);
