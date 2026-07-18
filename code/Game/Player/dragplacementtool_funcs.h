#pragma once
// Function declarations for deeply reimplemented DragPlacementTool methods.
// Called from engine/reimpl/dragplacementtool.cpp wrappers.

#include "DragPlacementTool.h"
#include <cstdint>

// Dtor (vtable +0x00): Scalar deleting destructor.
// Resets vtable chain (DragPlacementTool -> DragTool -> Tool), calls Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x14002d900
void* DragPlacementTool_Dtor(DragPlacementTool* self, uint32_t flags);

// Startup (vtable +0x20, Slot4): Searches entity component list for two description types:
//   CreatePlacementDescription (DAT_1401c7430) -> stored at self->placementDesc (+0x38)
//   AreaDescription (DAT_1401c7a18)            -> stored at self->areaDesc (+0x40)
// Original: 0x1400fc3e0
void DragPlacementTool_Startup(DragPlacementTool* self);

// CalcCost (vtable +0x70, Slot14): Calculates placement cost for a rectangular area.
// Converts start/end cells to grid coordinates, validates size against description
// min/max constraints (+0x28/+0x2C for min, +0x30/+0x34 for max from placementDesc).
// Iterates each cell and checks terrain validity using CharacterComponent_impl_f5d0.
// Returns: 0=success, 1=too small, 2=too big, 3=invalid terrain or overlap
// Original: 0x1400fbae0
uint64_t DragPlacementTool_CalcCost(DragPlacementTool* self, int startCell, int endCell,
                                     uint32_t* outValid, int* outCostA, int* outCostB,
                                     uint32_t* outLength);

// Draw (vtable +0x40, Slot8): Renders colored tile overlays for drag area placement.
// Section 1: If dialog has existing placement at +0x80, renders yellow footprint.
// Section 2: If param3==0, renders current drag rectangle with per-cell terrain
// validation (green=valid, red=invalid/too big).
// Uses CharacterComponent_impl_08c0 for tile rendering and
// CharacterComponent_impl_f5d0 for per-cell terrain validation.
// Original: 0x1400fbe40
void DragPlacementTool_Draw_Impl(DragPlacementTool* self, void* context, char param3);

// CalcPath (vtable +0x68, Slot13): Creates preview placement entity for drag area.
// Validates via CalcCost, clears existing path entities, clones a ComponentEntity
// from description, sets placement coords + rotation on BuildComponent, registers
// entity with EntityList.
// Original: 0x1400fb900
bool DragPlacementTool_CalcPath(DragPlacementTool* self, int startCell, int endCell);
