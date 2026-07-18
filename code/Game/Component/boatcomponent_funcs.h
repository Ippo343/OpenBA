#pragma once
// Function declarations for deeply reimplemented BoatComponent methods.
// Called from engine/reimpl/boatcomponent.cpp wrappers.

#include "BoatComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1400465d0): Complex serialization with
// InternalBasePtr refs, Vector3 positions, route waypoint array, dock ID.
void BoatComponent_Shutdown(BoatComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void BoatComponent_InitFuncs();
