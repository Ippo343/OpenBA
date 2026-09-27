#pragma once
// Function declarations for deeply reimplemented HerbalistComponent methods.
// Called from engine/reimpl/herbalistcomponent.cpp wrappers.

#include "HerbalistComponent.h"
#include <cstdint>

// ============================================================
// HerbalistComponent -- herbalist building (ResourceProduceComponent subclass)
// Vtable: 0x140159860
// Size: 0x50 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void HerbalistComponent_InitFuncs();
