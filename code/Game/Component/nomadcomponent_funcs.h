#pragma once
// Function declarations for deeply reimplemented NomadComponent methods.

#include "NomadComponent.h"
#include <cstdint>

// OnCreate (vtable slot [7], 0x14004fd20): Register with game state nomad list.
void NomadComponent_OnCreate(NomadComponent* self);

void NomadComponent_InitFuncs();
