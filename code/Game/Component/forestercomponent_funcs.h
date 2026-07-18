#pragma once
// Function declarations for deeply reimplemented ForesterComponent methods.
// Called from engine/reimpl/forestercomponent.cpp wrappers.

#include "ForesterComponent.h"
#include <cstdint>

// ============================================================
// ForesterComponent — forester resource production
// Vtable: 0x140163460
// Size: 0x58 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14011f960): Serialization.
// ResourceProduceComponent::Shutdown (base) + own serializer with
// two bool fields at +0x50 and +0x51 (packed foresterFlags).
void ForesterComponent_Shutdown(ForesterComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void ForesterComponent_InitFuncs();
