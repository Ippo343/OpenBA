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

// Shutdown2 (vtable +0x28, Slot5): Cleanup scene objects, mesh, and dialog.
// Removes scene object from Graphics::Scene, removes radius display highlight,
// destroys MeshInstance, closes dialog via UI::UserInterface::CloseDialog.
// Original: 0x1400fe4e0
void PlacementTool_Shutdown2_Impl(PlacementTool* self);

// OnKey (vtable +0x30, Slot6): Main input handler — rotation and placement.
// Handles rotation keys (0x13=CCW, 0x14=CW), flip/mirror (0x15),
// placement confirmation (key 5). Manages dialog open/close on activation.
// Always returns 1.
// Original: 0x1400fe2a0
uint64_t PlacementTool_OnKey_Impl(PlacementTool* self, char param2);

// Serialize (vtable +0x38, Slot7): Placement preview management.
// Updates the building preview mesh position, validates tile placement,
// manages radius display highlight, and updates the cost dialog.
// Original: 0x1400fdee0
void PlacementTool_Serialize_Impl(PlacementTool* self, void* param2, char param3);
