#pragma once
// GatherRawMaterials function declarations -- clean game logic layer.
// Called from engine/reimpl/gatherrawmaterials.cpp hook wrappers.
// Must NOT depend on hooks.h -- portable for standalone exe.

#include "BuildActions.h"
#include <cstdint>

// ============================================================
// GatherRawMaterials -- raw material gathering work action (0x78 bytes)
// Inherits: Object::Internal -> Work (0x48) -> GatherRawMaterials
// Vtable: 0x140161420 (12 slots)
// ============================================================

// Serialize (vtable slot 3, 0x1400ec560):
// Work::Serialize + 2 int arrays + tree serialization.
void GatherRawMaterials_Serialize_Impl(GatherRawMaterials* self, void* stream);

// Execute (vtable slot 4, 0x1400ebe40):
// Main gather logic -- find resources, path to them, assign citizen.
// Returns: 0=fail, 1=retry, 2=action pushed.
// Uses int return type (NOT bool) per bug class 7.
int GatherRawMaterials_Execute_Impl(GatherRawMaterials* self, void* entityIBP,
                                     uint64_t param3, uint64_t param4);

// OnComplete (vtable slot 6, 0x1400ebdf0):
// Completion handler -- removes from tree, updates build counts.
// Returns int. Uses int return (bug class 7).
int GatherRawMaterials_OnComplete_Impl(GatherRawMaterials* self, void* param2,
                                        void* param3, uint32_t param4);

// OnCancel (vtable slot 7, 0x1400ebdb0):
// Cancel handler -- removes from tree, decrements build counts.
void GatherRawMaterials_OnCancel_Impl(GatherRawMaterials* self, void* param2,
                                       void* param3, uint32_t param4);

// Cleanup (vtable slot 8, 0x1400ec480):
// Release assigned citizens from tree at +0x68.
void GatherRawMaterials_Cleanup_Impl(GatherRawMaterials* self);

// Validate (vtable slot 9, 0x1400ebb50):
// Check assigned tasks are still valid.
// Returns: true (nonzero) if all valid, false if any removed.
// Uses int return type (NOT bool) per bug class 7.
int GatherRawMaterials_Validate_Impl(GatherRawMaterials* self, void* param2);

// GetTarget (vtable slot 10, 0x1400ec510):
// Returns target position from game world singleton.
// Hidden return ptr ABI: RCX=self, RDX=retbuf.
void* GatherRawMaterials_GetTarget_Impl(GatherRawMaterials* self, void* retbuf);

// Resolution helper -- resolve all function pointers. Called once.
bool GatherRawMaterials_ResolveFuncs();
