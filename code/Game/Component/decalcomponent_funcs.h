#pragma once
// Function declarations for deeply reimplemented DecalComponent methods.
// Called from engine/reimpl/decalcomponent.cpp wrappers.

#include "DecalComponent.h"
#include <cstdint>

// Serialize (vtable slot [4], 0x14011b7d0): Reads building data, copies scale.
void DecalComponent_Serialize(DecalComponent* self);

// Draw (vtable slot [6], 0x14011b560): a door onto DecalComponent::OnAdded, kept
// for its by-address row (standalone/reg_code_lane6.cpp), B.3.8.

// Init: resolve all internal function pointers (called once at startup)
void DecalComponent_InitFuncs();
