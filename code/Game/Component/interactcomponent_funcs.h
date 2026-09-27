#pragma once
// Function declarations for deeply reimplemented InteractComponent methods.
// Called from engine/reimpl/interactcomponent.cpp wrappers.

#include "InteractComponent.h"
#include <cstdint>

// ============================================================
// InteractComponent -- interactive object selection component
// Vtable: 0x1401638b0
// Size: 0x38 bytes
// ============================================================


// Init: resolve all internal function pointers (called once at startup)
void InteractComponent_InitFuncs();
