#pragma once
// Function declarations for deeply reimplemented ClearAreaComponent methods.
// Called from engine/reimpl/clearareacomponent.cpp wrappers.

#include "ClearAreaComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x140117EC0): Component_Serialize + empty serializer +
// serializer with clearState at +0x48.
void ClearAreaComponent_Shutdown(ClearAreaComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void ClearAreaComponent_InitFuncs();
