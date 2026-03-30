#pragma once
// Function declarations for deeply reimplemented FruitComponent methods.
// Called from engine/reimpl/fruitcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "FruitComponent.h"
#include <cstdint>

// ============================================================
// FruitComponent -- fruit ripeness simulation
// Vtable: 0x140159450
// Size: 0x38 bytes
// ============================================================

// Update (vtable slot [9], 0x140049de0): Fruit ripening simulation.
// Accumulates growth progress each tick based on temperature sensitivity
// and seasonal growth factor. Uses quadratic temperature curve.
void FruitComponent_Update(FruitComponent* self, int64_t* pDeltaTime);

// Init: resolve all internal function pointers (called once at startup)
void FruitComponent_InitFuncs();
