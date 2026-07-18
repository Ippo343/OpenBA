#pragma once
// Function declarations for deeply reimplemented TerraformComponent methods.

#include "TerraformComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140057500): Serialize Vector3 array.
void TerraformComponent_Shutdown(TerraformComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x140057350): Complex terrain modification.
void TerraformComponent_OnCreate(TerraformComponent* self);

void TerraformComponent_InitFuncs();
