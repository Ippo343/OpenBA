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

// InitRegion (port of the region/position half of AmbientEnvironmentComponent_
// impl_36a0, exe 0x1400436a0): stamps the biome-sample grid region (comp+0x28
// gridX, +0x2c gridY, +0x30 rw, +0x34 rh), computes the region-center world
// position (comp+0x38 Vector3) and pushes it onto the sibling AmbientEmitter's
// position (emitter+0x28 Vector3). Call once at spawn, BEFORE the entity is
// registered (so the emitter position is set before OnAdded seeds its handles).
// Without this the region stays (0,0,0,0) → biome sampling collapses to biome[0]
// and every emitter sits at the world origin.
void AmbientEnvironmentComponent_InitRegion(AmbientEnvironmentComponent* self,
                                            int gridX, int gridY, int rw, int rh);

// Init: resolve all internal function pointers (called once at startup)
void AmbientEnvironmentComponent_InitFuncs();
