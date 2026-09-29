#pragma once
// Function declarations for deeply reimplemented PopulationComponent methods.

#include "PopulationComponent.h"
#include <cstdint>

// DynArray resize for the 12-byte density cells (FUN_14000b0d0). Exposed for
// the slot-3 Serialize body in animalwave_serialize.cpp (load-side resize).
void PopulationComponent_ResizeCells(int64_t* arr, int newCount);

