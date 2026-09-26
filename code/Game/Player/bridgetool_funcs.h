#pragma once
// Function declarations for deeply reimplemented BridgeTool methods.
// Called from engine/reimpl/bridgetool.cpp wrappers.

#include "BridgeTool.h"
#include <cstdint>

// Startup (vtable +0x20, Slot4): Finds BridgeDescription component on the entity
// at self->entity (+0x20) and stores it at self->bridgeDesc (+0x38).
// Iterates entity's component array, matching by CreateBridgeDescription type ID.
// Original: 0x1400fb7a0
void BridgeTool_Startup(BridgeTool* self);

// CalcCost (vtable +0x70, Slot14): Calculates bridge placement cost.
// Snaps endCell to same row/col as drag start, validates path, returns cost info.
// param_4 = outValid (set to 1), param_5 = outCostB (set to 0),
// param_6 = outCostA (set to 0), param_7 = outLength (set to 1).
// Returns 0 on success, 3 on failure (invalid cell or failed validation).
// Original: 0x1400facf0
uint64_t BridgeTool_CalcCost(BridgeTool* self, int startCell, int endCell,
                              uint32_t* outValid, int* outCostB, int* outCostA, int* outLength);

// CalcPath (vtable +0x68, Slot13): Builds the bridge path between two cells.
// The most complex function (~400 lines decompiled) — creates entity instances
// for each bridge segment using a linked list of segment descriptors, sets their
// position/rotation transforms, and registers with the entity list.
//
// Key operations:
//   1. Snaps end cell via SnapEndCell, validates via ValidatePath
//   2. Builds a doubly-linked list of bridge segment sizes (0/1/2/4/8 cells)
//   3. For each segment: copy-constructs entity from BridgeDescription templates,
//      finds MapComponent/ModelComponent/MultiObjectComponent/PickingComponent,
//      sets cell coordinates, rotation, transform, picking user data
//   4. Registers all segment entities with the entity list
//
// Delegated to original exe function via resolved pointer — full reimplementation
// blocked on typing ~20 unresolved helper functions and the linked list segment
// allocation/management logic.
// Original: 0x1400f9cd0
// Signature: (self, startCell, endCell) -> uint64_t (low byte: 1=success, 0=failure)
uint64_t BridgeTool_CalcPath(BridgeTool* self, int startCell, int endCell);
