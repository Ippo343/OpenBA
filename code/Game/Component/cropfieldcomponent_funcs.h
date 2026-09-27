#pragma once
// Function declarations for deeply reimplemented CropFieldComponent methods.
// Called from engine/reimpl/cropfieldcomponent.cpp wrappers.

#include "CropFieldComponent.h"
#include <cstdint>

// ============================================================
// CropFieldComponent — crop field entity management
// Vtable: 0x140162ec0
// Size: 0x68 bytes
// ============================================================

// Init: resolve all internal function pointers (called once at startup)
void CropFieldComponent_InitFuncs();
