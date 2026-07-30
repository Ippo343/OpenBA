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

// Update (vtable slot [9], 0x140123b50): PRODUCTION timer, not reproduction.
// Accumulates elapsed time into +0x30; every LivestockDescription+0x2c * 300.0f
// seconds it bumps the OWNING PASTURE's produce accumulator at pasture(+0x28)+0x44.
// The pasture's state-2 per-frame handler turns that accumulator into raw-material
// piles (wool / leather / eggs). Reproduction is a separate path: the animal's
// AnimalComponent flag bit 21, consumed by PastureDescription_helper_7370.
void LivestockComponent_Update(LivestockComponent* self, int64_t deltaTime);

// OnAdd (vtable slot [6], 0x140123c70): with no pasture assigned, file into the
// world's free-roaming livestock list (gameState+0x80).
void LivestockComponent_OnAdd(LivestockComponent* self);

// OnRemove (vtable slot [7], 0x140123c00): erase from whichever list owns it.
void LivestockComponent_OnRemove(LivestockComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void LivestockComponent_InitFuncs();
