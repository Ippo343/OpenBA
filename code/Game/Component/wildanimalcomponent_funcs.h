#pragma once
// Function declarations for deeply reimplemented WildAnimalComponent methods.
// Called from engine/reimpl/wildanimalcomponent.cpp wrappers.

#include "WildAnimalComponent.h"
#include <cstdint>

// ============================================================
// WildAnimalComponent -- wild animal entity reference component
// Vtable: 0x140164af0
// Size: 0x30 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14012e300): Serializes Component base +
// InternalBasePtr at +0x28. After deserialization, validates with
// IsDerivedFrom(DAT_1401c7820) and nulls out if type check fails.
void WildAnimalComponent_Shutdown(WildAnimalComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14012e2b0): If data28 (entity ref at +0x28)
// is non-null, creates InternalBasePtr from owner, CheckPtr on +0x28 entity,
// then adds owner to entity's component list at +0x38 via FUN_140002cc0.
void WildAnimalComponent_OnCreate(WildAnimalComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void WildAnimalComponent_InitFuncs();
