#pragma once
// Function declarations for deeply reimplemented HuntComponent methods.
// Called from engine/reimpl/huntcomponent.cpp wrappers.

#include "HuntComponent.h"
#include <cstdint>

// ============================================================
// HuntComponent — hunt progress timer with terrain speed scaling
// Vtable: 0x140163770
// Size: 0x58 bytes
// ============================================================

// Update (vtable slot [9], 0x140122740): Hunt progress accumulation.
// Reads terrain speed ratio from entity components, scales delta time
// by that ratio, accumulates into hunt progress at +0x50.
void HuntComponent_Update(HuntComponent* self, int64_t deltaTime);

// Init: resolve all internal function pointers (called once at startup)
void HuntComponent_InitFuncs();
