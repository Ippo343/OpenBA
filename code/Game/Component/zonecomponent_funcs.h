#pragma once
// Function declarations for deeply reimplemented ZoneComponent methods.

#include "ZoneComponent.h"
#include <cstdint>

// Draw (vtable slot [6], 0x1401301F0): Zone rendering.
void ZoneComponent_Draw(ZoneComponent* self);

// OnCreate (vtable slot [7], 0x140130080): Zone creation + tree walk.
void ZoneComponent_OnCreate(ZoneComponent* self);
