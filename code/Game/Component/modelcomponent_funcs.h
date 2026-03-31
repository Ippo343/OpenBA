#pragma once
// Function declarations for deeply reimplemented ModelComponent methods.
// Called from engine/reimpl/modelcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "ModelComponent.h"
#include <cstdint>

// ============================================================
// ModelComponent — mesh/model rendering component
// Vtable: 0x140163ba0
// Size: 0x170 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x140124b70): Serializes mesh state and transform.
// Calls Component_Serialize base, then IO::Serializer with variant index (+0x164 short),
// model variant (+0x166 short), Math::Transform::Serialize, scale float (+0x168),
// then if loading and variant >= 0, calls SetMesh. Also serializes MeshInstance,
// and a bool flag for modelId presence, conditionally calling vtable Draw (+0x30).
void ModelComponent_Shutdown(ModelComponent* self, void* stream);

// Serialize (vtable slot [4], 0x140124ac0): Reads building data, sets mesh variant.
// Gets MultiObjectDescription via IsDerivedFrom, reads variant index from description
// +0x30 (with optional random if +0x38 set), sets model variant from +0x34,
// calls MeshInstance::SetMesh if variant >= 0.
void ModelComponent_Serialize(ModelComponent* self);

// Draw (vtable slot [6], 0x140124a30): Adds mesh to Graphics::Scene.
// If variant >= 0 and modelId < 0: sets transform, calls MeshInstance vtable +0x8
// (Update with Time(0)), adds to Scene, stores scene object ID at +0x16C.
void ModelComponent_Draw(ModelComponent* self);

// OnCreate (vtable slot [7], 0x1401249d0): Removes mesh from Scene.
// If variant >= 0 and modelId >= 0: removes from Scene, sets modelId = -1,
// checks owner entity, calls helper to notify entity.
void ModelComponent_OnCreate(ModelComponent* self);

// OnRemove (vtable slot [8], 0x140124960): Updates mesh animation, clears draw flag.
// Calls MeshInstance vtable +0x8 (Update), checks IsAnimating.
// If not animating, clears flag bit 2 (0x04) from +0x20, notifies entity.
// NOTE: Bug class 6 — this vtable slot forwards RDX (Time*) implicitly.
void ModelComponent_OnRemove(ModelComponent* self, void* time);

// Init: resolve all internal function pointers (called once at startup)
void ModelComponent_InitFuncs();
