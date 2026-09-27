#pragma once
// Function declarations for deeply reimplemented FruitComponent methods.
// Called from engine/reimpl/fruitcomponent.cpp wrappers.

#include "FruitComponent.h"
#include <cstdint>

// ============================================================
// FruitComponent -- fruit ripeness simulation
// Vtable: 0x140159450
// Size: 0x38 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void FruitComponent_InitFuncs();
