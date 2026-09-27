#pragma once
// Function declarations for deeply reimplemented MapComponent methods.
// Called from engine/reimpl/mapcomponent.cpp wrappers.

#include "MapComponent.h"
#include <cstdint>

// ============================================================
// MapComponent — tile map coordinates for entities
// Vtable: 0x140163aa0
// Size: 0x38 bytes
// ============================================================

// Draw (vtable slot [6], 0x140124370): Checks building data flag, updates map overlay.
// Gets sibling component via IsDerivedFrom(+0x18, MapDescription typeId).
// If building[0x30] != 0, calls internal map update sub-function.
void MapComponent_Draw(MapComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void MapComponent_InitFuncs();
