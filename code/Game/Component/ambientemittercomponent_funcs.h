#pragma once
// Function declarations for deeply reimplemented AmbientEmitterComponent methods.
// Called from engine/reimpl/ambientemittercomponent.cpp wrappers.

#include "AmbientEmitterComponent.h"
#include <cstdint>

// Init: resolve all internal function pointers (called once at startup)
void AmbientEmitterComponent_InitFuncs();
