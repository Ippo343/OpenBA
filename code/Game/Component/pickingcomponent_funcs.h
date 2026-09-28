#pragma once
// Function declarations for deeply reimplemented PickingComponent methods.

#include "PickingComponent.h"
#include <cstdint>

// OnAdded (0x140128f50) port: register this component's picking mesh into Picking::Scene
// (Startup +0x30=owner, then AddMesh). Drives mesh-precise picking for static start-town
// buildings, whose real OnAdded/Startup lifecycle never fires in standalone. Idempotent
// enough for a single spawn-time call per entity. `pc` = raw PickingComponent pointer.
void PickingComponent_RegisterMesh(void* pc);

// Convenience: find `entity`'s PickingComponent and register its mesh (for callers that
// hold an entity, e.g. the player-placement create path). Respects BANISHED_LIVE_PICKMESH.
void PickingComponent_RegisterMeshForEntity(void* entity);
