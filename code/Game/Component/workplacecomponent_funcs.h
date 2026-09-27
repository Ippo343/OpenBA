#pragma once
// Function declarations for deeply reimplemented WorkPlaceComponent methods.
// Called from engine/reimpl/workplacecomponent.cpp wrappers.

#include "WorkPlaceComponent.h"
#include <cstdint>

// ============================================================
// WorkPlaceComponent — worker assignment management
// Vtable: 0x140164d30
// Size: 0x68 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void WorkPlaceComponent_InitFuncs();
