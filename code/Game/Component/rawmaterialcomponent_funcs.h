#pragma once
// Function declarations for deeply reimplemented RawMaterialComponent methods.
// Called from engine/reimpl/rawmaterialcomponent.cpp wrappers.

#include "RawMaterialComponent.h"
#include <cstdint>

// ============================================================
// RawMaterialComponent — raw material lifecycle (spawn, decay, collection)
// Vtable: 0x140164170
// Size: 0x40 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x140129330): Versioned serialization of field28, field38, field30.
void RawMaterialComponent_Shutdown(RawMaterialComponent* self, void* stream);

// Serialize (vtable slot [4], 0x1401292c0): Init field28 from building data, register with world.
void RawMaterialComponent_Serialize(RawMaterialComponent* self);

// Draw (vtable slot [6], 0x1401291d0): Create render object and register draw item.
// Gets MaterialDescription component, determines pickup category from flags,
// allocates a 0x60-byte pickup draw item, registers with scene.
void RawMaterialComponent_Draw(RawMaterialComponent* self);

// Update (vtable slot [9], 0x140129130): Accumulate time, check threshold, mark for collection.
void RawMaterialComponent_Update(RawMaterialComponent* self, int64_t* pDeltaTime);

// Init: resolve all internal function pointers (called once at startup)
void RawMaterialComponent_InitFuncs();
