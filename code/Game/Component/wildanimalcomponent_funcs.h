#pragma once
// Function declarations for the reimplemented WildAnimalComponent methods.
// Implementations live in code/Game/Component/wildanimalcomponent.cpp.

#include "WildAnimalComponent.h"
#include <cstdint>

// ============================================================
// WildAnimalComponent -- a wild animal's back-reference to its herd
// Vtable: 0x140164af0
// Size: 0x30 bytes
// ============================================================

// Serialize (vtable slot [3], 0x14012e300): serializes the Component base plus
// the InternalBasePtr at +0x28, then validates the loaded pointer with
// IsDerivedFrom(DAT_1401c7820 = HerdComponent) and nulls it if the check fails.
// (Previously misnamed "Shutdown" here.)
void WildAnimalComponent_Serialize(WildAnimalComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void WildAnimalComponent_InitFuncs();
