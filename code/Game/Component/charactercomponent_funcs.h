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

// Init: resolve all internal function pointers (called once at startup)
void CharacterComponent_InitFuncs();
