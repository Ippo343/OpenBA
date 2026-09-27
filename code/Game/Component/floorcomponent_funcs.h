#pragma once
// Function declarations for deeply reimplemented FloorComponent methods.
// Called from engine/reimpl/floorcomponent.cpp wrappers.

#include "FloorComponent.h"
#include <cstdint>

// ============================================================
// FloorComponent -- floor/ground plane rendering for buildings
// Vtable: 0x140163360
// Size: 0x30 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void FloorComponent_InitFuncs();
