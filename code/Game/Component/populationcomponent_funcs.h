#pragma once
// Function declarations for deeply reimplemented PopulationComponent methods.

#include "PopulationComponent.h"
#include <cstdint>

// Update (vtable slot [9], 0x140054070): Density smoothing + habitability scanning.
void PopulationComponent_Update(PopulationComponent* self, int64_t* pDeltaTime);

void PopulationComponent_InitFuncs();
