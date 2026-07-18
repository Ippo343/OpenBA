#pragma once
// Function declarations for deeply reimplemented CharacterComponent methods.
// Called from engine/reimpl/charactercomponent.cpp wrappers.

#include "CharacterComponent.h"
#include <cstdint>

// ============================================================
// CharacterComponent — character animation/rendering
// Vtable: 0x140162370
// Size: 0x80 bytes
// ============================================================

// Update (vtable slot [9], 0x140109480): Checks if character index (data60) >= 0,
// looks up character from array at data48, and calls Character_UpdateAnimation.
// Parameters: self, deltaTime (int64 time value)
void CharacterComponent_Update(CharacterComponent* self, int64_t deltaTime);

// Init: resolve all internal function pointers (called once at startup)
void CharacterComponent_InitFuncs();
