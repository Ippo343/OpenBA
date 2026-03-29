#pragma once
// Function declarations for deeply reimplemented PlacementTool methods.
// Called from engine/reimpl/placementtool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "PlacementTool.h"
#include <cstdint>

// Dtor (vtable +0x00, Slot0): Scalar deleting destructor.
// Resets vtable chain (PlacementTool -> Tool), calls Object::Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x14002dc90
void* PlacementTool_Dtor_Impl(PlacementTool* self, uint32_t flags);

// Startup (vtable +0x20, Slot4): Find 4 description components on entity + setup mesh.
// Finds PlacementDescription (DAT_1401c7468), MapComponent (DAT_1401c7a18),
// CreateComponent (DAT_1401c7a88), RadiusComponent (DAT_1401c4870).
// Also looks up RotationSetting IBP, allocates MeshInstance, sets random mesh variant.
// Currently delegates to trampoline — requires many untyped engine APIs:
//   MeshInstance::MeshInstance, SetMesh, UpdateHierarchy, Random::Rand,
//   Graphics scene management.
// Original: 0x1400fe580

// Shutdown2 (vtable +0x28, Slot5): Cleanup scene objects, mesh, and dialog.
// Removes from Graphics::Scene and radius display, destroys MeshInstance,
// closes dialog via UI::UserInterface::CloseDialog.
// Currently delegates to trampoline — requires unresolved engine APIs:
//   Graphics::Scene::RemoveObject, TradeWork_impl_0d00 (radius display),
//   MeshInstance vtable dtor.
// Original: 0x1400fe4e0

// OnKey (vtable +0x30, Slot6): Main input handler — rotation and placement.
// Handles rotation keys (0x13=CCW, 0x14=CW, 0x15=placement click),
// manages dialog open/close, calls placement helpers.
// Currently delegates to trampoline — complex input/placement logic with many
// sub-functions (PlacementTool_sub_d960, PlacementTool_helper_d650).
// Original: 0x1400fe2a0

// Serialize (vtable +0x38, Slot7): Placement logic with cost calculation.
// Picks cell under cursor, calculates placement validity, updates UI overlay
// with cost string, manages radius display and scene objects.
// Currently delegates to trampoline — requires many unresolved engine APIs:
//   PlacementTool_sub_da00, PlacementTool_helper_d580, Math::Transform,
//   Graphics::Scene, Entity FindDescription.
// Original: 0x1400fdee0

// Draw (vtable +0x40, Slot8): Render placement preview geometry.
// Calculates placement area, renders preview meshes for building footprint,
// draws duplicate placement indicator for repeat mode.
// Currently delegates to trampoline — requires unresolved rendering APIs:
//   PlacementTool_sub_da00, PlacementTool_OnRemove_sub_1380.
// Original: 0x1400fdcc0
