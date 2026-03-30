#pragma once
// Function declarations for deeply reimplemented AbsorbResourceComponent methods.
// Called from engine/reimpl/absorbresourcecomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AbsorbResourceComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140105c00): Component_Serialize + two empty Serializer blocks.
void AbsorbResourceComponent_Shutdown(AbsorbResourceComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void AbsorbResourceComponent_InitFuncs();
