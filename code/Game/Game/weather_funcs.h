#pragma once
// Function declarations for deeply reimplemented Weather methods.
// Called from engine/reimpl/weather.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "Weather.h"
#include <cstdint>

// Dtor (vtable +0x00): Calls External::~External, conditionally frees memory.
void* Weather_Dtor(Weather* self, uint32_t flags);

// Cleanup (vtable +0x30, Slot6): Destroys and re-constructs in place (reset).
void Weather_Cleanup(Weather* self);

// Init: Initializes all Weather fields to defaults.
// Called by Cleanup and can replace Weather_Ctor (0x1400d3e50).
void Weather_Init(Weather* self);
