#pragma once
// Function declarations for deeply reimplemented AutoPickupComponent methods.
// Called from engine/reimpl/autopickupcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AutoPickupComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1400449d0): Serializes isEnabled bool + int array.
void AutoPickupComponent_Shutdown(AutoPickupComponent* self, void* stream);

// Serialize (vtable slot [4], 0x140044990): IsDerivedFrom check to init isEnabled.
void AutoPickupComponent_Serialize(AutoPickupComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void AutoPickupComponent_InitFuncs();
