#pragma once
// Function declarations for deeply reimplemented PickingComponent methods.

#include "PickingComponent.h"
#include <cstdint>


// Convenience: find `entity`'s PickingComponent and register its mesh (for callers that
// hold an entity, e.g. the player-placement create path). Respects BANISHED_LIVE_PICKMESH.
class ComponentEntity;
void PickingComponent_RegisterMeshForEntity(ComponentEntity* entity);  // B8: stays free -- harness / port-only: no retail body (the tree's convenience for the standalone placement paths, bridgetool_calcpath.cpp / tool_base_tile.cpp; retail reaches PickingComponent::RefreshMesh through the component lifecycle)
