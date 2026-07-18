#pragma once
// Function declarations for deeply reimplemented ImmigrationComponent methods.
// Called from engine/reimpl/immigrationcomponent.cpp wrappers.

#include "ImmigrationComponent.h"
#include <cstdint>

// ============================================================
// ImmigrationComponent -- immigration/nomad arrival events
// Vtable: 0x140159b30
// Size: 0x68 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14004e4b0): Complex serialization with
// entity array loop. Serializes base + empty controller block + field50 (int)
// + entity array at +0x58 with IsDerivedFrom validation + Time at +0x48.
void ImmigrationComponent_Shutdown(ImmigrationComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void ImmigrationComponent_InitFuncs();
