#pragma once
// Function declarations for deeply reimplemented NaturalResourceComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "NaturalResourceComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140125cf0): Serialize resourceScale + two bool flags.
void NaturalResourceComponent_Shutdown(NaturalResourceComponent* self, void* stream);

// OnRemove (vtable slot [8], 0x140125c40): Clear path queue, check animation,
// modify flags, reassign natural resource ownership.
void NaturalResourceComponent_OnRemove(NaturalResourceComponent* self);

void NaturalResourceComponent_InitFuncs();
