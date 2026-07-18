#pragma once
// Function declarations for deeply reimplemented WellComponent methods.

#include "WellComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x14005cfa0): Component_Serialize + two empty serializer blocks.
void WellComponent_Shutdown(WellComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14005cf60): Registers with population manager.
void WellComponent_OnCreate(WellComponent* self);

void WellComponent_InitFuncs();
