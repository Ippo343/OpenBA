#pragma once
// Function declarations for deeply reimplemented TrackerComponent methods.
// No dependency on hooks.h — portable to standalone exe.

#include "TrackerComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140059130): Serialize two paired arrays + trackerId.
void TrackerComponent_Shutdown(TrackerComponent* self, void* stream);

void TrackerComponent_InitFuncs();
