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

// Shutdown (vtable slot [3], 0x140056970): Component_Serialize + one empty Serializer block.
void StorageDisplayComponent_Shutdown(StorageDisplayComponent* self, void* stream);

// Draw (vtable slot [6], 0x140056730): Grid-based rendering of display objects.
// Computes positions from building dimensions, creates transforms, sets geometry.
void StorageDisplayComponent_Draw(StorageDisplayComponent* self);

// OnCreate (vtable slot [7], 0x1400566b0): Remove display objects from scene.
// Iterates display objects array, removes each from graphics scene if valid.
void StorageDisplayComponent_OnCreate(StorageDisplayComponent* self);

// Update (vtable slot [9], actual body at 0x1400563e0): Display update logic.
// Queries StorageComponent on entity, iterates slots, proportionally allocates
// display objects across resource types.
void StorageDisplayComponent_Update(StorageDisplayComponent* self);

// OnUpdate2 (vtable slot [11], 0x140056940): Draw + Update combined refresh.
// Calls vtable Draw then Update.
void StorageDisplayComponent_OnUpdate2(StorageDisplayComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void StorageDisplayComponent_InitFuncs();
