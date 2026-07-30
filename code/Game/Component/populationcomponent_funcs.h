#pragma once
// Function declarations for deeply reimplemented PopulationComponent methods.

#include "PopulationComponent.h"
#include <cstdint>

// Update (vtable slot [9], 0x140054070): Density smoothing + habitability scanning.
void PopulationComponent_Update(PopulationComponent* self, int64_t* pDeltaTime);

// Startup (vtable slot [4], 0x140054220 — Ghidra mislabels it *_Serialize):
// body init (description, habitability grid) AND the wild-herd creation pass.
void PopulationComponent_Startup(PopulationComponent* self);

void PopulationComponent_InitFuncs();
