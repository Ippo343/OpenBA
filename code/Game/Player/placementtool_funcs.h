#pragma once
// Function declarations for deeply reimplemented PlacementTool methods.
// Called from engine/reimpl/placementtool.cpp wrappers.

#include "PlacementTool.h"
#include <cstdint>

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

// Draw (vtable +0x40, Slot8): Renders placement overlay tiles on the terrain.
// When param3==0: calculates placement bounds via sub_da00, reads rotation from
// IBP at +0x48, then delegates to sub_1380 for terrain validation and tile rendering.
// Always checks dialog controller (+0x10 at +0x80) for existing placement marker;
// if present, renders that existing placement in yellow via sub_1380.
// Original: 0x1400fdcc0
void PlacementTool_Draw_Impl(PlacementTool* self, void* context, char param3);

// Startup (vtable +0x20, Slot4): Initialization — find components, alloc mesh.
// Finds 4 components via GetTypeId+IsDerivedFrom loops, resolves RotationSetting IBP,
// allocates MeshInstance (0x120 bytes), picks random mesh variant, calls SetMesh
// and UpdateHierarchy.
// Original: 0x1400fe580
void PlacementTool_Startup_Impl(PlacementTool* self);
