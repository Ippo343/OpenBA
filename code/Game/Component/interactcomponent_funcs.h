#pragma once
// Function declarations for deeply reimplemented InteractComponent methods.
// Called from engine/reimpl/interactcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "InteractComponent.h"
#include <cstdint>

// ============================================================
// InteractComponent -- interactive object selection component
// Vtable: 0x1401638b0
// Size: 0x38 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x140123340): Serializes Component base +
// entity array. Reads count from +0x30, on load resizes array at +0x28,
// serializes each InternalBasePtr element, validates with IsDerivedFrom.
void InteractComponent_Shutdown(InteractComponent* self, void* stream);

// Draw (vtable slot [6], 0x140123110): Rebuilds the interact entity list
// from scene. Gets multi-object children, iterates component lists,
// finds matching InteractComponents, builds dynamic array at +0x28/+0x30.
void InteractComponent_Draw(InteractComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void InteractComponent_InitFuncs();
