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

// Init: resolve all internal function pointers (called once at startup)
void HuntComponent_InitFuncs();
