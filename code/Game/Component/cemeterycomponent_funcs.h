#pragma once
// Function declarations for deeply reimplemented CemeteryComponent methods.
// Called from engine/reimpl/cemeterycomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "CemeteryComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1400475C0): Serializes base + controller stub +
// variable-length array of cemetery plot tile indices.
void CemeteryComponent_Shutdown(CemeteryComponent* self, void* stream);

// Draw (vtable slot [6], 0x140047200): Complex cemetery grid rendering (233 instrs).
// Builds plot grid based on entity rotation/orientation.
void CemeteryComponent_Draw(CemeteryComponent* self);

// OnCreate (vtable slot [7], 0x1400471C0): Creates InternalBasePtr from owner entity,
// adds it to cemetery list at GameState+0xF0+0x40.
void CemeteryComponent_OnCreate(CemeteryComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void CemeteryComponent_InitFuncs();
