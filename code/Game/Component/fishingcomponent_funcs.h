#pragma once
// Function declarations for deeply reimplemented FishingComponent methods.
// Called from engine/reimpl/fishingcomponent.cpp wrappers.

#include "FishingComponent.h"
#include <cstdint>

// ============================================================
// FishingComponent — fishing dock resource production
// Vtable: 0x140163230
// Size: 0x58 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void FishingComponent_InitFuncs();
