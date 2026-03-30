#pragma once
// Function declarations for deeply reimplemented FloorComponent methods.
// Called from engine/reimpl/floorcomponent.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "FloorComponent.h"
#include <cstdint>

// ============================================================
// FloorComponent -- floor/ground plane rendering for buildings
// Vtable: 0x140163360
// Size: 0x30 bytes
// ============================================================

// Shutdown (vtable slot [3], 0x14011F560): Serializes base component + floorFlag (bool).
void FloorComponent_Shutdown(FloorComponent* self, void* stream);

// Draw (vtable slot [6], 0x14011F340): Checks IsDerivedFrom on _unk18,
// calls floor mesh placement sub if building flag at +0x28 is nonzero.
void FloorComponent_Draw(FloorComponent* self);

// OnCreate (vtable slot [7], 0x14011F2A0): If floorId >= 0, removes existing
// floor mesh from picking scene, resets floorId to -1 and floorFlag to 0.
void FloorComponent_OnCreate(FloorComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void FloorComponent_InitFuncs();
