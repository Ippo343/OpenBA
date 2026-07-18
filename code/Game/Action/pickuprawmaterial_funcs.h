#pragma once
// PickupRawMaterial function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/pickuprawmaterial.cpp hook wrappers.

#include <cstdint>

struct PickupRawMaterial;

// PickupRawMaterial::Serialize -- serialize status, entity refs (+0x20, +0x28)
void PickupRawMaterial_Serialize_Impl(PickupRawMaterial* self, void* stream);

// PickupRawMaterial::AbstractImpl -- pick up raw material, transfer to storage, flag entity
// Returns int per bug class 7. RDX = actionStack (bug class 6).
int PickupRawMaterial_AbstractImpl_Impl(PickupRawMaterial* self, void* actionStack);

// PickupRawMaterial::GetTarget -- checks +0x28 direct and +0x20->parent(+0x10)
bool PickupRawMaterial_GetTarget_Impl(PickupRawMaterial* self, void* param);

// Resolution helper
bool PickupRawMaterial_ResolveFuncs();
