#pragma once
// Function declarations for deeply reimplemented HealerComponent methods.
// Called from engine/reimpl/healercomponent.cpp wrappers.

#include "HealerComponent.h"
#include <cstdint>

// ============================================================
// HealerComponent -- healer building registration
// Vtable: 0x140159730
// Size: 0x48 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14004aeb0): Component_Serialize + two empty
// serializer blocks (component section + controller section).
void HealerComponent_Shutdown(HealerComponent* self, void* stream);

// OnCreate (vtable slot [7], 0x14004ae70): Registers owning entity with the
// healer list in the population manager at (manager+0xF0)+0x70.
void HealerComponent_OnCreate(HealerComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void HealerComponent_InitFuncs();
