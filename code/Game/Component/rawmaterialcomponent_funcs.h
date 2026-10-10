#pragma once
// Function declarations for deeply reimplemented RawMaterialComponent methods.
// Called from engine/reimpl/rawmaterialcomponent.cpp wrappers.

#include "RawMaterialComponent.h"
#include <cstdint>

// ============================================================
// RawMaterialComponent — raw material lifecycle (spawn, decay, collection)
// Vtable: 0x140164170
// Size: 0x40 bytes
// ============================================================

// Update (vtable slot [9], 0x140129130): Accumulate time, check threshold, mark for collection.
void RawMaterialComponent_Update(RawMaterialComponent* self, int64_t* pDeltaTime);   // 0x140129130 B8: stays free -- read, not moved: retail's slot [9] Update body, never installed (the tree's RawMaterialComponent::Update is the inert PORT GAP stub); no caller; installing it is behaviour:

// Init: resolve all internal function pointers (called once at startup)
