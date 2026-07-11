#pragma once
// Function declarations for deeply reimplemented PickingComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "PickingComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140128bb0): Serialize IBP at +0x30 with validation.
void PickingComponent_Shutdown(PickingComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x140128b50): Remove picking mesh if registered.
void PickingComponent_OnCreate(PickingComponent* self);

void PickingComponent_InitFuncs();

#ifdef STANDALONE_EXE
// OnAdded (0x140128f50) port: register this component's picking mesh into Picking::Scene
// (Startup +0x30=owner, then AddMesh). Drives mesh-precise picking for static start-town
// buildings, whose real OnAdded/Startup lifecycle never fires in standalone. Idempotent
// enough for a single spawn-time call per entity. `pc` = raw PickingComponent pointer.
void PickingComponent_RegisterMesh(void* pc);
#endif
