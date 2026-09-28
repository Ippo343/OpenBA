#pragma once
// Function declarations for deeply reimplemented StorageDisplayComponent methods.
// Called from engine/reimpl/storagedisplaycomponent.cpp wrappers.

#include "StorageDisplayComponent.h"
#include <cstdint>

// ============================================================
// StorageDisplayComponent — visual display of stored items on buildings
// Vtable: 0x14015a780
// Size: 0x38 bytes
// ============================================================

// Update (vtable slot [9], actual body at 0x1400563e0): Display update logic.
// Queries StorageComponent on entity, iterates slots, proportionally allocates
// display objects across resource types.
void StorageDisplayComponent_Update(StorageDisplayComponent* self);

// Init: resolve all internal function pointers (called once at startup)
