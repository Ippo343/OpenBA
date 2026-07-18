#pragma once
// Function declarations for deeply reimplemented QuarryComponent methods.

#include "QuarryComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140055230): Same pattern as MineComponent.
void QuarryComponent_Shutdown(QuarryComponent* self, void* stream);

void QuarryComponent_InitFuncs();
