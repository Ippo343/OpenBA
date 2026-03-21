#pragma once
// Function declarations for deeply reimplemented TutorialSetupGameSpeed methods.
// Called from engine/reimpl/tutorialsetupgamespeed.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "TutorialSetups.h"
#include <cstdint>

// ============================================================
// TutorialSetupGameSpeed -- set game speed for tutorial
// Vtable: 0x140160da0 (12 slots)
// Size: 0x28 bytes
// Extends TutorialSetup -> Object::External
//
// Fields:
//   +0x20: int32_t speed      -- game speed to set (default 1)
//   +0x24: uint8_t _field24   -- if nonzero, also pause (set +0x30 to 0)
// ============================================================

// Slot0 (vtable +0x00, 0x140028ac0): Scalar deleting destructor.
// Sets vtable chain, calls External::~External, optionally frees.
void* TutorialSetupGameSpeed_Dtor(void* self, uint32_t flags);

// Slot3 (vtable +0x18, 0x1400e2df0): GetDescription -- returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int TutorialSetupGameSpeed_GetDescription(void* /*self*/) { return 1; }

// Slot5 (vtable +0x28, 0x1400e2d90): Serialize.
// Creates serializer, serializes speed (int) and _field24 (bool), destroys serializer.
void TutorialSetupGameSpeed_Serialize(void* self, void* stream);

// Slot6 (vtable +0x30, 0x140028a70): Cleanup.
// Calls ScalarDtor(self, 0) via vtable, re-initializes via External::External,
// sets vtable chain, resets speed=1 and _field24=0.
void TutorialSetupGameSpeed_Cleanup(void* self);

// Slot8 (vtable +0x40, 0x1400e2d60): Apply -- set game speed from self->speed.
// Writes speed to game->updateLoop(+0x120)->speed(+0x34).
// If _field24 is set, also writes 0 to +0x30 (pauses game).
// NOTE: RIP-relative MOV in original, uses vtable patch.
void TutorialSetupGameSpeed_Slot8(void* self);

// Slot9 (vtable +0x48, 0x1400e2d50): returns 0 (void, no-op).
// Original is "ret" (3 bytes, unhookable).
inline void TutorialSetupGameSpeed_Slot9(void* /*self*/) { }

// Slot10 (vtable +0x50, 0x1400e2d40): returns 0 (void, no-op).
// Original is "ret" (3 bytes, unhookable).
inline void TutorialSetupGameSpeed_Slot10(void* /*self*/) { }

// Slot11 (vtable +0x58, 0x140028a20): Ctor/Init.
// Calls External::External, sets vtable chain, sets speed=1, _field24=0.
void* TutorialSetupGameSpeed_Ctor(void* self);
