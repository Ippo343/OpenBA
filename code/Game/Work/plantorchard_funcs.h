#pragma once
// Function declarations for deeply reimplemented PlantOrchard methods.
// Called from engine/reimpl/plantorchard.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "BuildActions.h"
#include <cstdint>

// ============================================================
// PlantOrchard -- Orchard planting work (0x70 bytes)
// Inherits: Object::Internal -> Work (0x48) -> PlantOrchard
// Vtable: 0x140161910 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Main orchard planting assignment logic.
// Iterates orchard spots, finds plantable ones, assigns citizen to plant.
// Returns: 1=done/idle, 2=work assigned. Uses int return type (bug class 7).
int PlantOrchard_Slot4_Impl(PlantOrchard* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot5/Execute (vtable +0x28): 3 bytes, returns false. Too small to hook.
bool PlantOrchard_Execute_Impl(PlantOrchard* self);

// Slot6/OnComplete (vtable +0x30): Removes BST entry, marks cell available.
void PlantOrchard_OnComplete_Impl(PlantOrchard* self, void* param2, void* param3, int param4);

// Slot7/OnCancel (vtable +0x38): Marks cell available.
void PlantOrchard_OnCancel_Impl(PlantOrchard* self, void* param2, void* param3, int param4);

// Slot8/Cleanup (vtable +0x40): CancelAll -- loops BST, cancels each work item.
void PlantOrchard_Cleanup_Impl(PlantOrchard* self);

// Slot9/Validate (vtable +0x48): RemoveInvalid -- validates BST entries, removes dead ones.
// Returns bool: true if all valid.
bool PlantOrchard_Validate_Impl(PlantOrchard* self, void* param2);

// Slot10/GetTarget (vtable +0x50): Returns entity ptr from work's entity ref.
void* PlantOrchard_GetTarget_Impl(PlantOrchard* self, void* outPtr);

// Slot11/GetPriority (vtable +0x58): 9 bytes, shared. Returns (field_50 > 0).
bool PlantOrchard_GetPriority_Impl(PlantOrchard* self);

// Serialize (vtable +0x18): Work::Serialize + BST + bool array + ExternalRef.
void PlantOrchard_Serialize_Impl(PlantOrchard* self, void* stream);

// ScalarDtor body: Sets vtables, frees array + BST, calls Work dtor.
void PlantOrchard_DtorBody(PlantOrchard* self);

// ScalarDtor (vtable +0x00): Calls dtor body, conditional free.
void* PlantOrchard_ScalarDtor_Impl(PlantOrchard* self, uint32_t flags);

// Resolve all function pointers needed by PlantOrchard.
// Called once during hook installation.
bool PlantOrchard_ResolveFuncs();
