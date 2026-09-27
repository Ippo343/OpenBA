#pragma once
// Function declarations for deeply reimplemented LivestockComponent methods.
// Called from engine/reimpl/livestockcomponent.cpp wrappers.

#include "LivestockComponent.h"
#include <cstdint>

// ============================================================
// LivestockComponent — livestock reproduction timer
// Vtable: 0x1401639b0
// Size: 0x38 bytes
// ============================================================

// OnAdd (vtable slot [6], 0x140123c70): with no pasture assigned, file into the
// world's free-roaming livestock list (gameState+0x80).
void LivestockComponent_OnAdd(LivestockComponent* self);

// OnRemove (vtable slot [7], 0x140123c00): erase from whichever list owns it.
void LivestockComponent_OnRemove(LivestockComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void LivestockComponent_InitFuncs();
