#pragma once
// Function declarations for deeply reimplemented AmbientEnvironmentComponent methods.
// Called from engine/reimpl/ambientenvironmentcomponent.cpp wrappers.

#include "AmbientEnvironmentComponent.h"
#include <cstdint>

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
