#pragma once
// Function declarations for the reimplemented TerraformComponent methods.

#include "TerraformComponent.h"
#include <cstdint>

// Shutdown / Serialize (vtable slot 3, 0x140057500): persist the Vector3 undo record.
void TerraformComponent_Shutdown(TerraformComponent* self, void* stream);

// OnRemove (vtable slot 7, 0x140057350): replay the undo record — restore the original
// terrain heights and put the recorded map cells back to terrain type 10. Ghidra labels
// this one OnCreate; see the body comment for why that is the off-by-one mislabel.
void TerraformComponent_OnRemove(TerraformComponent* self);

// The build-complete deformation, dispatched from vtable slot 12 on message id 4.
void TerraformComponent_MeshPass(TerraformComponent* self, bool ceiling);   // 0x140056b50
void TerraformComponent_FootprintPass(TerraformComponent* self);            // 0x140057120
extern "C" void TerraformComponent_Slot12(void* self, int msgId);           // 0x140057310

void TerraformComponent_InitFuncs();
