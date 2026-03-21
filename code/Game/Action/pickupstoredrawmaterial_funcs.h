#pragma once
// PickupStoredRawMaterial function declarations -- clean game logic layer.
// These functions are called from engine/reimpl/pickupstoredrawmaterial.cpp hook wrappers.
// They must NOT depend on hooks.h -- they are portable for standalone exe.

#include <cstdint>

struct PickupStoredRawMaterial;

// PickupStoredRawMaterial::Serialize -- serialize status, entity refs, material list, partial flag
void PickupStoredRawMaterial_Serialize_Impl(PickupStoredRawMaterial* self, void* stream);

// PickupStoredRawMaterial::AbstractImpl -- query storage, subtract, transfer to citizen
// Returns int per bug class 7. RDX = actionStack (bug class 6).
int PickupStoredRawMaterial_AbstractImpl_Impl(PickupStoredRawMaterial* self, void* actionStack);

// PickupStoredRawMaterial::CheckEntity -- undo pickup when entity removed
void PickupStoredRawMaterial_CheckEntity_Impl(PickupStoredRawMaterial* self);

// PickupStoredRawMaterial::GetTarget -- checks +0x28 and +0x20->parent(+0x10)
// NOT HOOKED in passthrough, but included for standalone exe.
bool PickupStoredRawMaterial_GetTarget_Impl(PickupStoredRawMaterial* self, void* param);

// Resolution helper
bool PickupStoredRawMaterial_ResolveFuncs();
