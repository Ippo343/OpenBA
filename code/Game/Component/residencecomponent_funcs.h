#pragma once
// Function declarations for deeply reimplemented ResidenceComponent methods.
// Called from engine/reimpl/residencecomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "ResidenceComponent.h"
#include <cstdint>

// ============================================================
// ResidenceComponent — ControllerComponent subclass (0x60 bytes)
// Vtable: 0x1401642c0
// Three non-trivial overrides: Shutdown, Serialize, OnCreate
// ============================================================

// Shutdown (vtable slot [3], 0x14012a170): Component_Serialize + empty block +
// versioned serializer with resident count + array resize + array serialize +
// Time::Serialize at +0x48.
void ResidenceComponent_Shutdown(ResidenceComponent* self, void* stream);

// Serialize (vtable slot [4], 0x14012a120): IsDerivedFrom check on +0x18,
// reads resident capacity from description at +0x20, resizes array at +0x50.
void ResidenceComponent_Serialize(ResidenceComponent* self);

// OnCreate (vtable slot [7], 0x14012a0e0): Calls helper init function,
// then registers owner entity with game world residence list at worldMgr+0x20.
void ResidenceComponent_OnCreate(ResidenceComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void ResidenceComponent_InitFuncs();
