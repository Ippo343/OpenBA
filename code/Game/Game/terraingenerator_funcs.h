#pragma once
// Function declarations for deeply reimplemented TerrainGenerator methods.
// Called from engine/reimpl/terraingenerator.cpp wrappers.

#include "TerrainGenerator.h"
#include <cstdint>

// Dtor (vtable +0x00): Frees terrain data array, destroys 4 Strings,
// calls External::~External, conditionally frees memory.
void* TerrainGenerator_Dtor(TerrainGenerator* self, uint32_t flags);

// Cleanup (vtable +0x30, Slot6): Destroys and re-constructs in place (reset).
void TerrainGenerator_Cleanup(TerrainGenerator* self);

// Serialize (vtable +0x28, Slot5): Serializes all fields via IO::Serializer.
// Complex: includes ExternalPtr, 3 filename strings, and a dynamic terrain data array.
void TerrainGenerator_Serialize(TerrainGenerator* self, void* stream);

// Slot8 (vtable +0x40): Null-check + init helper. If self is null returns null,
// otherwise calls External ctor and sets up vtable/defaults.
void* TerrainGenerator_Slot8(TerrainGenerator* self);

// Init: Initializes all TerrainGenerator fields to defaults.
// Called by Cleanup/Slot8 and replaces TerrainGenerator_Ctor (0x1400b1810).
void TerrainGenerator_Init(TerrainGenerator* self);

// InitFuncs: Resolves function pointers needed by TerrainGenerator methods.
// Must be called once after ResolveInit().
void TerrainGenerator_InitFuncs();
