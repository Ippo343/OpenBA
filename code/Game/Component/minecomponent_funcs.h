#pragma once
// Function declarations for deeply reimplemented MineComponent methods.
// Called from engine/reimpl/minecomponent.cpp wrappers.

#include "MineComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x14004f260): Serializes base component +
// empty serializer block (no component-specific fields).
void MineComponent_Shutdown(MineComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void MineComponent_InitFuncs();
