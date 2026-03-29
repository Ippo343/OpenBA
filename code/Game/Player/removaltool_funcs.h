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
