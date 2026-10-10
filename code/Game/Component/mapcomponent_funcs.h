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
void MapComponent_Draw(MapComponent* self);   // 0x140124370 B8: stays free -- read, not moved: retail's slot [6] OnAdded body, never installed (the tree's MapComponent::OnAdded is a different body, mapcomponent.cpp); no caller; installing it is behaviour:
