#pragma once
// Function declarations for deeply reimplemented TombstoneComponent methods.

#include "TombstoneComponent.h"
#include <cstdint>

// Serialize (vtable slot [4], 0x140057860): Actually OnCreate — initializes
// random tombstone lifetime from entity data.
void TombstoneComponent_Serialize(TombstoneComponent* self);

void TombstoneComponent_InitFuncs();
