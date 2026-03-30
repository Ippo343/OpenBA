#pragma once
// Function declarations for deeply reimplemented AmbientEmitterComponent methods.
// Called from engine/reimpl/ambientemittercomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AmbientEmitterComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140042f30): Complex serialization with Vector3,
// emitter array, InternalBasePtr entries, and version-gated building data resize.
void AmbientEmitterComponent_Shutdown(AmbientEmitterComponent* self, void* stream);

// Draw (vtable slot [6], 0x140042ba0): Gets building data, resizes emitter list,
// calls audio setup if building has audio flag.
void AmbientEmitterComponent_Draw(AmbientEmitterComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void AmbientEmitterComponent_InitFuncs();
