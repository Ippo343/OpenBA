#pragma once
// Function declarations for deeply reimplemented MapComponent methods.
// Called from engine/reimpl/mapcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "MapComponent.h"
#include <cstdint>

// ============================================================
// MapComponent — tile map coordinates for entities
// Vtable: 0x140163aa0
// Size: 0x38 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x1401243e0): Serializes tile coords + extra int.
// Calls Component_Serialize base, then IO::Serializer with 4 uint16 tile coords
// (tileX0, tileY0, tileX1, tileY1) and 1 int32 (extraField).
void MapComponent_Shutdown(MapComponent* self, void* stream);

// Draw (vtable slot [6], 0x140124370): Checks building data flag, updates map overlay.
// Gets sibling component via IsDerivedFrom(+0x18, MapDescription typeId).
// If building[0x30] != 0, calls internal map update sub-function.
void MapComponent_Draw(MapComponent* self);

// OnCreate (vtable slot [7], 0x1401242f0): Initial map setup.
// Gets building data, calls terrain clear helper if building[0x32] != 0,
// then always calls map initialization sub-function.
void MapComponent_OnCreate(MapComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void MapComponent_InitFuncs();
