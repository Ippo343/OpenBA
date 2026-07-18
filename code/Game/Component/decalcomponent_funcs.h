#pragma once
// Function declarations for deeply reimplemented DecalComponent methods.
// Called from engine/reimpl/decalcomponent.cpp wrappers.

#include "DecalComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x14011b820): Serializes decalScale + decalState.
void DecalComponent_Shutdown(DecalComponent* self, void* stream);

// Serialize (vtable slot [4], 0x14011b7d0): Reads building data, copies scale.
void DecalComponent_Serialize(DecalComponent* self);

// Draw (vtable slot [6], 0x14011b560): Complex terrain decal rendering (111 instrs).
void DecalComponent_Draw(DecalComponent* self);

// OnCreate (vtable slot [7], 0x14011b510): Removes decal from terrain if decalId >= 0.
void DecalComponent_OnCreate(DecalComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void DecalComponent_InitFuncs();
