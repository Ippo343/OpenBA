#pragma once
// Function declarations for deeply reimplemented AmbientEnvironmentComponent methods.
// Called from engine/reimpl/ambientenvironmentcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AmbientEnvironmentComponent.h"
#include <cstdint>

// Update (vtable slot [9], 0x140043310): Samples terrain grid for dominant biome,
// blends day/night ambient sounds based on time-of-day.
// Very complex (222 instructions), deeply interacts with grid, biome config, audio scene.
// Delegates to original via resolved function pointer.
void AmbientEnvironmentComponent_Update(AmbientEnvironmentComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void AmbientEnvironmentComponent_InitFuncs();
