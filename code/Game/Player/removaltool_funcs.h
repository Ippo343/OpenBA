#pragma once
// Function declarations for deeply reimplemented RemovalTool methods.
// Called from engine/reimpl/removaltool.cpp wrappers.
// No dependency on hooks.h or resolve.h -- portable to standalone exe.

#include "RemovalTool.h"
#include <cstdint>

// Dtor (vtable +0x00): Scalar deleting destructor.
// Resets vtable chain (RemovalTool -> DragTool -> Tool), calls Internal::~Internal,
// conditionally frees memory if flags & 1.
// Original: 0x14002e7f0
void* RemovalTool_Dtor(RemovalTool* self, uint32_t flags);

// Startup (vtable +0x20, Slot4): Finds RemovalDescription component on the entity
// at self->descriptionPtr (+0x20) and stores it at self->removalDesc (+0x38).
// Iterates entity's component array, matching by RemovalDescription type ID (DAT_1401c74a0).
// Original: 0x140104bd0
void RemovalTool_Startup(RemovalTool* self);

// CalcCost (vtable +0x70, Slot14): Calculates removal cost/dimensions.
// Converts cell indices to grid coordinates, computes width/height of removal area.
// Checks constraints from the RemovalDescription if present.
// Returns 0 on success, 3 on failure (constrained cells don't match).
// Original: 0x140103ef0
uint64_t RemovalTool_CalcCost(RemovalTool* self, int startCell, int endCell,
                              uint32_t* outValid, int* outCostB, int* outCostA, uint32_t* outLength);

// Draw (vtable +0x40, Slot8): Renders tile overlay for removal preview.
// Phase 1 (constrained): If description entity has constraints, iterates constrained
//   tile range and renders all tiles with default color via fn_TileRender.
// Phase 2 (drag selection): If param3==0, iterates drag selection rectangle,
//   checks each tile for removability (sub_48a0, sub_4770, sub_4670), renders
//   green (0x8000ff00) for removable, red (0xffff0000) for non-removable.
// Original: 0x140104050
void RemovalTool_Draw(RemovalTool* self, void* context, char param3);

// CalcPath (vtable +0x68, Slot13): Creates removal marker entities for each cell
// in the drag selection. Extremely complex (~300 lines): iterates grid cells, checks
// removability via multiple sub-functions, creates ComponentEntity copies, finds
// MapComponent/DestroyComponent on entities, handles road/crop/building removal
// with BST traversals and entity destruction scheduling.
// Resolved as a direct exe function pointer call due to extreme complexity.
// Original: 0x1401035e0
bool RemovalTool_CalcPath(RemovalTool* self, int startCell, int endCell);
