#pragma once
// Function declarations for deeply reimplemented WorkComponent methods.
// Called from engine/reimpl/workcomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "WorkComponent.h"
#include <cstdint>

// ============================================================
// WorkComponent — work ID, flags, and state management
// Vtable: 0x140164be0
// Size: 0x38 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14012e710): Versioned serialization.
// Serializes workId, workState (v>=1), workFlags (v>=2), legacy bool (v<3).
void WorkComponent_Shutdown(WorkComponent* self, void* stream);

// Draw (vtable slot [6], 0x14012e6f0): Compute work capacity.
// Calls FUN_14012e5c0 to compute capacity, stores result at +0x30.
void WorkComponent_Draw(WorkComponent* self);

// OnCreate (vtable slot [7], 0x14012e6c0): Register work slot.
// Checks BST for available slot, registers if found.
void WorkComponent_OnCreate(WorkComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void WorkComponent_InitFuncs();
