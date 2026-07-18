#pragma once
// Function declarations for deeply reimplemented SpawnComponent methods.

#include "SpawnComponent.h"
#include <cstdint>

// Update (vtable slot [9], 0x140055ad0): Periodic entity spawning.
void SpawnComponent_Update(SpawnComponent* self, int64_t* pDeltaTime);

void SpawnComponent_InitFuncs();
