#pragma once
// Function declarations for deeply reimplemented RoadTool methods.
// Called from engine/reimpl/roadtool.cpp wrappers.

#include "RoadTool.h"
#include <cstdint>

// Startup (vtable +0x20, Slot4): Finds CreateRoadDescription component on entity.
// Iterates entity's component array at self->descriptionPtr (+0x20),
// matching by type ID DAT_1401c74d8. Stores result at self->roadDesc (+0x38).
// Original: 0x1401056e0
void RoadTool_Startup(RoadTool* self);

// Draw (vtable +0x40, Slot8): Renders road preview overlay.
// Phase 1 (constrained): If description entity has constraints, runs pathfinding
//   (RoadTool_sub_ce00) with mask 0xa17, renders path tiles with default color.
// Phase 2 (drag selection): If param3==0, gets start/end cells, checks terrain
//   validity (mask 0x5e0), tries strict (0xa17) then relaxed (0xff7) pathfinding,
//   color-codes tiles: green (0xff00ff00) for valid road terrain, red (0xffff0000)
//   for invalid/occupied, purple(0xff00ff00) for road-valid.
// Original: 0x140105190
void RoadTool_Draw(RoadTool* self, void* context, char param3);

// --- Unhookable slots (for standalone exe completeness) ---

// OnKey (vtable +0x30, Slot6): Road-specific drag logic.
// Calls DragTool::OnKey (shared base), then reads KeyMap::GetValue(0, 0x16)
// to set modifierFlag at +0x40. Always returns 1.
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140105690
uint64_t RoadTool_OnKey_Impl(RoadTool* self, char key);

// CalcPath (vtable +0x68, Slot13): commit the road run. Validates the endpoints,
// honours the descriptor's fixed-endpoint constraint, runs the road A*, records the
// decal for the road type, queues a BuildRoad work item per cell, and rebuilds the
// road mesh. Returns true when the run was committed — DragTool_OnKey reads this to
// decide between the place sound and the error sound.
// Implemented in roadtool_calcpath.cpp (with its three exe helpers).
// Original: 0x140104d30
extern "C" bool RoadTool_CalcPath_Impl(RoadTool* self, uint32_t startCell, uint32_t endCell);

// Helpers ported alongside CalcPath (roadtool_calcpath.cpp), registered so the exe
// addresses resolve for any other caller.
extern "C" void** RoadTool_FindMapDescription_6990(void* entity, void** out);   // 0x140026990
extern "C" void  RoadTool_DecalStore_acd90(int64_t mgr, int index, void* decal); // 0x1400acd90
extern "C" void  RoadTool_PlaceRoadSegment_9c960(uint32_t* mapGrid, uint32_t cellIdx,
                                                 uint8_t roadType, uint64_t endCell,
                                                 uint32_t mapDescVal, uint64_t roadDescExtra);

// CalcCost (vtable +0x70, Slot14): Road cost calculation.
// Validates cells, checks entity description constraints, calls the internal
// road pathfinding function, and returns path length and validity.
// UNHOOKABLE: CALL rel32 in prologue.
// Original: 0x140104fe0
uint64_t RoadTool_CalcCost_Impl(RoadTool* self, uint32_t startCell, uint32_t endCell,
    uint32_t* outValid, int* outCostB, int* outCostA, int* outLength);
