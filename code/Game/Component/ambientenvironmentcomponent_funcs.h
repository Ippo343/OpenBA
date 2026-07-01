#pragma once
// Function declarations for deeply reimplemented AmbientEnvironmentComponent methods.
// Called from engine/reimpl/ambientenvironmentcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AmbientEnvironmentComponent.h"
#include <cstdint>

// Update (vtable slot [9], 0x140043310): Samples terrain grid for dominant biome,
// blends day/night ambient sounds based on temperature, driving the sibling
// AmbientEmitterComponent through the world Audio::Scene. Faithful reimpl.
// The component Update ABI is (self, Time*); the original reads only `self`.
void AmbientEnvironmentComponent_Update(AmbientEnvironmentComponent* self, int64_t* time);

// Init: resolve all internal function pointers (called once at startup)
void AmbientEnvironmentComponent_InitFuncs();
