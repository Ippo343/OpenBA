#pragma once
// Function declarations for deeply reimplemented FishingComponent methods.
// Called from engine/reimpl/fishingcomponent.cpp wrappers.

#include "FishingComponent.h"
#include <cstdint>

// ============================================================
// FishingComponent — fishing dock resource production
// Vtable: 0x140163230
// Size: 0x58 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14011f050): Serialization.
// ResourceProduceComponent::Shutdown (base) + own serializer with
// fishingFlag (bool at +0x50) and fishingRate (float at +0x54).
void FishingComponent_Shutdown(FishingComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void FishingComponent_InitFuncs();
