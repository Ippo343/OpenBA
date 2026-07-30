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

// OnRemove (vtable slot [7], 0x14012e2b0): if the herd back-ref at +0x28 is
// non-null, wraps self in an InternalBasePtr and ERASES it from that entity's
// component list at +0x38 (FUN_140002cc0 = DynArray8_EraseByValue).
// (Previously misnamed "OnCreate" here; the real OnCreate is 0x14012e440, a
// Stream overload that is not a slot on this vtable.)
void WildAnimalComponent_OnRemove(WildAnimalComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void WildAnimalComponent_InitFuncs();
