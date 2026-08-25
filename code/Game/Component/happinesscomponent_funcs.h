#pragma once
// Function declarations for deeply reimplemented HappinessComponent methods.
// Called from engine/reimpl/happinesscomponent.cpp wrappers.

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

// ------------------------------------------------------------------
// Controller state machine (retail table 0x14004ac10)
// ------------------------------------------------------------------
// States 2 and 13 "built": enter 0x14004a9f0, per-frame trans38 0x14004a9a0.
// State 3 "demolish": enter 0x14004a940 (== the slot-7 body).
extern "C" bool HappinessComponent_IsActive(void* self);       // 0x14004a550
extern "C" void HappinessComponent_Register(void* self);       // 0x14004a4f0
extern "C" void HappinessComponent_Unregister(void* self);     // 0x14004a4a0
extern "C" char HappinessComponent_EnterBuilt(void* comp, void* sm);
extern "C" char HappinessComponent_Tick38(void* comp, void* sm, void* time, int idx);
extern "C" char HappinessComponent_EnterDemolish(void* comp, void* sm);
