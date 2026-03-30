#pragma once
// Function declarations for deeply reimplemented NomadComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "NomadComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x14004fc40): Serialize base + IBP + Vector3s + int + Time.
void NomadComponent_Shutdown(NomadComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14004fd20): Register with game state nomad list.
void NomadComponent_OnCreate(NomadComponent* self);

void NomadComponent_InitFuncs();
