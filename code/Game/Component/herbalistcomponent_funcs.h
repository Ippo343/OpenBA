#pragma once
// Function declarations for deeply reimplemented HerbalistComponent methods.
// Called from engine/reimpl/herbalistcomponent.cpp wrappers.

#include "HerbalistComponent.h"
#include <cstdint>

// ============================================================
// HerbalistComponent -- herbalist building (ResourceProduceComponent subclass)
// Vtable: 0x140159860
// Size: 0x50 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14004b200): Calls parent
// ResourceProduceComponent::Shutdown, then creates an empty serializer block.
void HerbalistComponent_Shutdown(HerbalistComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void HerbalistComponent_InitFuncs();
