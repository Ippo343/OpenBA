#pragma once
// Function declarations for deeply reimplemented RoadTool methods.
// Called from engine/reimpl/roadtool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "RoadTool.h"
#include <cstdint>

// Dtor (vtable +0x00, Slot0): Scalar deleting destructor.
// Resets vtable chain RoadTool -> DragTool -> Tool, calls Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x14002e8c0
void* RoadTool_Dtor(RoadTool* self, uint32_t flags);

// Startup (vtable +0x20, Slot4): Finds CreateRoadDescription component on entity.
// Iterates entity's component array at self->descriptionPtr (+0x20),
// matching by type ID DAT_1401c74d8. Stores result at self->roadDesc (+0x38).
// Original: 0x1401056e0
void RoadTool_Startup(RoadTool* self);

// Draw (vtable +0x40, Slot8): Renders road preview overlay.
// Calls DragTool::Draw (shared base behavior via BridgeTool_Draw at Slot8),
// then reads KeyMap::GetValue(0, 0x16) to set the modifierFlag at +0x40.
// Always returns 1.
// Original: 0x140105190
// NOTE: This function is kept as trampoline passthrough because it calls
// the base DragTool::Draw which is itself very complex.

// --- Unhookable slots (for standalone exe completeness) ---

// OnKey (vtable +0x30, Slot6): Road-specific drag logic.
// Calls DragTool::OnKey (shared base), then reads KeyMap::GetValue(0, 0x16)
// to set modifierFlag at +0x40. Always returns 1.
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140105690
uint64_t RoadTool_OnKey_Impl(RoadTool* self, char key);

// CalcPath (vtable +0x68, Slot13): Road path calculation.
// Validates cells, checks entity description constraints, calls the internal
// road pathfinding function (RoadTool_sub_ce00), finds CreateRoadDescription
// component, and places road segments along the calculated path.
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140104d30
uint32_t RoadTool_CalcPath_Impl(RoadTool* self, uint32_t startCell, uint32_t endCell);

// CalcCost (vtable +0x70, Slot14): Road cost calculation.
// Validates cells, checks entity description constraints, calls the internal
// road pathfinding function, and returns path length and validity.
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140104fe0
uint64_t RoadTool_CalcCost_Impl(RoadTool* self, uint32_t startCell, uint32_t endCell,
    uint32_t* outValid, int* outCostB, int* outCostA, int* outLength);
