#pragma once
// Function declarations for deeply reimplemented HighlightComponent methods.
// Called from engine/reimpl/highlightcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "HighlightComponent.h"
#include <cstdint>

// ============================================================
// HighlightComponent -- selection/highlight outlines
// Vtable: 0x140159990
// Size: 0x128 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14004c880): Component_Serialize + empty serializer.
void HighlightComponent_Shutdown(HighlightComponent* self, void* stream);

// Init: resolve all internal function pointers (called once at startup)
void HighlightComponent_InitFuncs();
