#pragma once
// Function declarations for deeply reimplemented PickingComponent methods.

#include "PickingComponent.h"
#include <cstdint>


// Convenience: find `entity`'s PickingComponent and register its mesh (for callers that
// hold an entity, e.g. the player-placement create path). Respects BANISHED_LIVE_PICKMESH.
class ComponentEntity;
void PickingComponent_RegisterMeshForEntity(ComponentEntity* entity);  // B9: harness / port-only -- read: no retail body; at both callers retail's EntityList::RegisterEntity (0x1400c48a0) broadcast runs PickingComponent::OnAdded (= RefreshMesh 0x140128f50) itself, so the explicit call right after RegisterEntity (tool_base_tile.cpp, bridgetool_calcpath.cpp) is a second RefreshMesh: row 1261
