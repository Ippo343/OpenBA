#pragma once
// Function declarations for deeply reimplemented TombstoneComponent methods.

#include "TombstoneComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1400578E0): Serialize base + Time at +0x28
// with version-gated halving (version < 2).
void TombstoneComponent_Shutdown(TombstoneComponent* self, void* stream);

// Serialize (vtable slot [4], 0x140057860): Actually OnCreate — initializes
// random tombstone lifetime from entity data.
void TombstoneComponent_Serialize(TombstoneComponent* self);

// Update (vtable slot [9], 0x1400577D0): Countdown timer, marks entity
// for removal when timer expires.
void TombstoneComponent_Update(TombstoneComponent* self, int64_t* pDeltaTime);

void TombstoneComponent_InitFuncs();
