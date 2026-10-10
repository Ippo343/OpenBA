#pragma once
// Function declarations for deeply reimplemented DecalComponent methods.
// Called from engine/reimpl/decalcomponent.cpp wrappers.

#include "DecalComponent.h"
#include <cstdint>

// Serialize (vtable slot [4], 0x14011b7d0): Reads building data, copies scale.
void DecalComponent_Serialize(DecalComponent* self);   // 0x14011b7d0 B8: stays free -- read, not moved: retail's slot [4] Startup body, never installed (DecalComponent.h TREE DIVERGENCE: the tree's Startup is a different body); no caller; installing it is behaviour:

// Draw (vtable slot [6], 0x14011b560): a door onto DecalComponent::OnAdded, kept
// for its by-address row (standalone/reg_code_lane6.cpp), B.3.8.
