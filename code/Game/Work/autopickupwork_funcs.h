#pragma once
// Function declarations for deeply reimplemented AutoPickupWork methods.
// Called from engine/reimpl/autopickupwork.cpp wrappers.

#include "AutoPickupWork.h"
#include <cstdint>

// Ctor: Calls Object::Internal::Internal, initializes Work base fields,
// sets AutoPickupWork vtable.
// Original: 0x1400E4050
void* AutoPickupWork_Ctor(AutoPickupWork* self);

// ScalarDtor (vtable +0x00): Resets vtable chain (AutoPickupWork -> Work),
// calls Object::Internal::~Internal, conditionally frees memory if flags & 1.
// Original: 0x1400294E0
void* AutoPickupWork_ScalarDtor(AutoPickupWork* self, uint32_t flags);

// Serialize (vtable +0x18): Calls Work::Serialize for base fields, then
// creates an empty serializer block (version 0) for AutoPickupWork extension.
// Original: 0x1400E3F80
void AutoPickupWork_Serialize(AutoPickupWork* self, void* stream);

// Execute (vtable +0x28): Trivial — always returns 0 (false).
// Original: 0x1400F7540
int AutoPickupWork_Execute(AutoPickupWork* self);

// OnRemove (vtable +0x40, Slot8): Frees the item list from the
// AutoPickupComponent on the entity at self+0x10.
// Finds the AutoPickupComponent, then frees its item array (+0x30/+0x38).
// Original: 0x1400E3F30
void AutoPickupWork_OnRemove(AutoPickupWork* self);
