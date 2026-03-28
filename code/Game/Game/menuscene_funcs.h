#pragma once
// Function declarations for deeply reimplemented MenuScene methods.
// Called from engine/reimpl/menuscene.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "MenuScene.h"
#include <cstdint>

// Dtor (vtable +0x00): Scalar deleting destructor.
// Restores vtable, destroys sub-objects and arrays, calls External base dtor,
// conditionally frees memory.
void* MenuScene_Dtor(MenuScene* self, uint32_t flags);

// Slot5 (vtable +0x28): Serialize -- read/write all fields to/from stream.
// Delegates to the original function via resolved pointer (201 instructions,
// too many unresolved helper dependencies for full reimpl at this stage).
void MenuScene_Serialize(MenuScene* self, void* stream);

// Slot6 (vtable +0x30): Cleanup -- destroys via vtable dtor(self,0), then re-inits.
void MenuScene_Cleanup(MenuScene* self);

// Init / Slot8 (vtable +0x40): Constructs External base, sets vtable, zeroes all
// MenuScene fields, initializes transform and atmosphere defaults.
// Slot8 is the null-safe variant (returns null if self is null).
void MenuScene_Init(MenuScene* self);
void* MenuScene_Slot8(MenuScene* self);
