#pragma once
// Function declarations for deeply reimplemented ZoneComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "ZoneComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1401302F0): Serialize + conditional zone registration.
void ZoneComponent_Shutdown(ZoneComponent* self, void* stream);

// Draw (vtable slot [6], 0x1401301F0): Zone rendering.
void ZoneComponent_Draw(ZoneComponent* self);

// OnCreate (vtable slot [7], 0x140130080): Zone creation + tree walk.
void ZoneComponent_OnCreate(ZoneComponent* self);

void ZoneComponent_InitFuncs();
