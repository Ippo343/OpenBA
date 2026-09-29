#pragma once
// Function declarations for the reimplemented TerraformComponent methods.

#include "TerraformComponent.h"
#include <cstdint>

// The build-complete deformation, dispatched from vtable slot 12 on message id 4.
void TerraformComponent_MeshPass(TerraformComponent* self, bool ceiling);   // 0x140056b50
void TerraformComponent_FootprintPass(TerraformComponent* self);            // 0x140057120

