#pragma once
// Function declarations for deeply reimplemented ForesterComponent methods.
// Called from engine/reimpl/forestercomponent.cpp wrappers.

#include "ForesterComponent.h"
#include <cstdint>

// ============================================================
// ForesterComponent — forester resource production
// Vtable: 0x140163460
// Size: 0x58 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void ForesterComponent_InitFuncs();
