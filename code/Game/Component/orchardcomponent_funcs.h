#pragma once
// Function declarations for deeply reimplemented OrchardComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "OrchardComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140051de0): Complex entity ref serialization.
void OrchardComponent_Shutdown(OrchardComponent* self, void* stream);

void OrchardComponent_InitFuncs();
