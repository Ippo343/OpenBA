#pragma once
// Function declarations for deeply reimplemented HappinessComponent methods.
// Called from engine/reimpl/happinesscomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "HappinessComponent.h"
#include <cstdint>

// ============================================================
// HappinessComponent -- citizen happiness tracking
// Vtable: 0x1401595d0
// Size: 0x50 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14004aa80): Serializes base component,
// empty controller block, then happinessFlag (bool at +0x28).
void HappinessComponent_Shutdown(HappinessComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14004aa20): If happinessFlag != 0,
// registers owner with game state happiness list, sets index to -1,
// then clears flag.
void HappinessComponent_OnCreate(HappinessComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void HappinessComponent_InitFuncs();
