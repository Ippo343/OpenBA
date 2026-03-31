#pragma once
// Function declarations for deeply reimplemented StorageLocationComponent methods.
// Called from engine/reimpl/storagelocationcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "StorageLocationComponent.h"
#include <cstdint>

// ============================================================
// StorageLocationComponent — registers entity as storage location
// Vtable: 0x140164880
// Size: 0x48 bytes (ControllerComponent, no extra fields)
// ============================================================

// Shutdown (vtable slot [3], 0x14012d2a0): Component_Serialize + two empty Serializer blocks.
void StorageLocationComponent_Shutdown(StorageLocationComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14012d270): Register entity with game world storage location list.
void StorageLocationComponent_OnCreate(StorageLocationComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void StorageLocationComponent_InitFuncs();
