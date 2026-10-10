#pragma once
// Function declarations for deeply reimplemented PickingComponent methods.

#include "PickingComponent.h"
#include <cstdint>


// Convenience: find `entity`'s PickingComponent and register its mesh (for callers that
// hold an entity, e.g. the player-placement create path). Respects BANISHED_LIVE_PICKMESH.
class ComponentEntity;
void PickingComponent_RegisterMeshForEntity(ComponentEntity* entity);
