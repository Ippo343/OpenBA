#pragma once
// PickingComponent's include for the BuildComponent / DestroyComponent / start-town callers
// of PickingComponent::RefreshMesh (0x140128f50). The entity-level convenience
// `PickingComponent_RegisterMeshForEntity` that lived here had no retail body and lost its
// last caller with row 1261 (D.4 LUT); it is deleted (D.4 RESIDUE, row 1291).

#include "PickingComponent.h"
