#pragma once
// Function declarations for deeply reimplemented NaturalResourceComponent methods.

#include "NaturalResourceComponent.h"
#include <cstdint>

// OnRemove (vtable slot [8], 0x140125c40): Clear path queue, check animation,
// modify flags, reassign natural resource ownership.
void NaturalResourceComponent_OnRemove(NaturalResourceComponent* self);
