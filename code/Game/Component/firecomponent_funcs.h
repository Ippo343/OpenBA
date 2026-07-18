#pragma once
// Function declarations for deeply reimplemented FireComponent methods.
// Called from engine/reimpl/firecomponent.cpp wrappers.

#include "FireComponent.h"
#include <cstdint>

// ============================================================
// FireComponent -- building fire simulation
// Vtable: 0x140159380
// Size: 0x48 bytes
// ============================================================

// Update (vtable slot [9], 0x140048fb0): Two-state fire system.
// Active (field30 != 0): countdown + spread + extinguish.
// Inactive (field30 == 0): accumulate time + random fire-start probability.
void FireComponent_Update(FireComponent* self, int64_t* pDeltaTime);

// Init: resolve all internal function pointers (called once at startup)
void FireComponent_InitFuncs();
