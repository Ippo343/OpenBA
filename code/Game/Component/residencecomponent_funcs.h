#pragma once
// Function declarations for deeply reimplemented ResidenceComponent methods.
// Called from engine/reimpl/residencecomponent.cpp wrappers.

#include "ResidenceComponent.h"
#include <cstdint>

// ============================================================
// ResidenceComponent — ControllerComponent subclass (0x60 bytes)
// Vtable: 0x1401642c0
// Three non-trivial overrides: Shutdown, Serialize, OnCreate
// ============================================================

// OnCreate (vtable slot [7], 0x14012a0e0): Calls helper init function,
// then registers owner entity with game world residence list at worldMgr+0x20.
void ResidenceComponent_OnCreate(ResidenceComponent* self);

