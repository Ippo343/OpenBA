#pragma once
// Function declarations for deeply reimplemented MapGenerator methods.
// Called from engine/reimpl/mapgenerator.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "MapGenerator.h"
#include <cstdint>

// Dtor (vtable +0x00): Destroys all MapGenerator fields, calls External::~External,
// conditionally frees memory if flags & 1.
void* MapGenerator_Dtor(MapGenerator* self, uint32_t flags);

// Cleanup (vtable +0x30, Slot6): Destroys (flags=0) then re-constructs in place.
void MapGenerator_Cleanup(MapGenerator* self);

// Init: Initializes all MapGenerator fields to defaults.
// Mirrors MapGenerator_Ctor (0x1400a3030). Called by Cleanup and Slot8.
void MapGenerator_Init(MapGenerator* self);

// Serialize (vtable +0x28, Slot5): Serializes all fields to/from stream.
void MapGenerator_Serialize(MapGenerator* self, void* stream);

// Slot8 (vtable +0x40): If self is null, return null; otherwise reinit via Init.
void* MapGenerator_Slot8(MapGenerator* self);
