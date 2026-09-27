#pragma once
// Function declarations for deeply reimplemented WorkComponent methods.
// Called from engine/reimpl/workcomponent.cpp wrappers.

#include "WorkComponent.h"
#include <cstdint>

// ============================================================
// WorkComponent — work ID, flags, and state management
// Vtable: 0x140164be0
// Size: 0x38 bytes
// ============================================================

// Draw (vtable slot [6], 0x14012e6f0): Compute work capacity.
// Calls FUN_14012e5c0 to compute capacity, stores result at +0x30.
void WorkComponent_Draw(WorkComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void WorkComponent_InitFuncs();
