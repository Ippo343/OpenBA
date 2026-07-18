#pragma once
// Function declarations for deeply reimplemented ClimateGenerator methods.
// Called from engine/reimpl/climategenerator.cpp wrappers.

#include "ClimateGenerator.h"
#include <cstdint>

// Dtor (vtable +0x00): Destroys String at +0x28, calls External::~External,
// conditionally frees memory.
void* ClimateGenerator_Dtor(ClimateGenerator* self, uint32_t flags);

// Cleanup (vtable +0x30, Slot6): Destroys and re-constructs in place (reset).
void ClimateGenerator_Cleanup(ClimateGenerator* self);

// Serialize (vtable +0x28, Slot5): Serializes all fields via IO::Serializer.
void ClimateGenerator_Serialize(ClimateGenerator* self, void* stream);

// Slot8 (vtable +0x40): Null-check + init helper. If self is null returns null,
// otherwise calls External ctor and sets up vtable/defaults.
void* ClimateGenerator_Slot8(ClimateGenerator* self);

// Init: Initializes all ClimateGenerator fields to defaults.
// Called by Cleanup/Slot8 and replaces ClimateGenerator_Ctor (0x1400930c0).
void ClimateGenerator_Init(ClimateGenerator* self);

// InitFuncs: Resolves function pointers needed by ClimateGenerator methods.
// Must be called once after ResolveInit().
void ClimateGenerator_InitFuncs();
