#pragma once
// Function declarations for deeply reimplemented PickingComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "PickingComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140128bb0): Serialize IBP at +0x30 with validation.
void PickingComponent_Shutdown(PickingComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x140128b50): Remove picking mesh if registered.
void PickingComponent_OnCreate(PickingComponent* self);

void PickingComponent_InitFuncs();
