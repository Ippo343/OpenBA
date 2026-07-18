#pragma once
// Function declarations for deeply reimplemented ClearResourceComponent methods.
// Called from engine/reimpl/clearresourcecomponent.cpp wrappers.

#include "ClearResourceComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1401185E0): Component_Serialize + two empty serializer blocks.
void ClearResourceComponent_Shutdown(ClearResourceComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void ClearResourceComponent_InitFuncs();
