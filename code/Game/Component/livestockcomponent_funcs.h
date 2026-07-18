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

// Update (vtable slot [9], 0x140123b50): Reproduction timer.
// Accumulates elapsed time into +0x30. When threshold exceeded
// (configComponent+0x2C * 300.0f), increments birth counter at
// herdComponent(+0x28)+0x44 and wraps.
void LivestockComponent_Update(LivestockComponent* self, int64_t deltaTime);

// Init: resolve all internal function pointers (called once at startup)
void LivestockComponent_InitFuncs();
