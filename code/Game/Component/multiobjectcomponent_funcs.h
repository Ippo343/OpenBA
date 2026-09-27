#pragma once
// Function declarations for deeply reimplemented MultiObjectComponent methods.

#include "MultiObjectComponent.h"
#include <cstdint>

// OnCreate (vtable slot [7], 0x1401257C0): Mark child entities for removal.
void MultiObjectComponent_OnCreate(MultiObjectComponent* self);

void MultiObjectComponent_InitFuncs();
