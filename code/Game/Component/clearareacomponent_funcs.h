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

// ModelComponent_impl_8270 (0x140118270) port — state-1 finalize body shared with
// BuildComponent_State1Enter's construction-site model reset (see clearareacomponent_states.cpp).
void ClearArea_Finalize8270(void* comp);
