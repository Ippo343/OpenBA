#pragma once
// Function declarations for deeply reimplemented CitizenComponent methods.
// Called from engine/reimpl/citizencomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "CitizenComponent.h"
#include <cstdint>

// ============================================================
// CitizenComponent — full citizen state (health, happiness, inventory, etc.)
// Vtable: 0x140162580
// Size: 0x290 bytes
// ============================================================

// Update (vtable slot [9], 0x140112e70): Main citizen lifecycle tick.
// 711 instructions — handles aging, hunger/thirst accumulation, health/death
// checks, food/fuel consumption, workplace assignment, housing, partner
// finding, and child-bearing logic.
// Parameters: self, deltaTime (pointer to Time)
void CitizenComponent_Update(CitizenComponent* self, void* deltaTime);

// ShouldSwitchWorkplace (0x1401109E0): Determines if citizen should switch
// workplace based on job availability and region priority.
// Returns: true if citizen should switch.
bool CitizenComponent_ShouldSwitchWorkplace(CitizenComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void CitizenComponent_InitFuncs();
