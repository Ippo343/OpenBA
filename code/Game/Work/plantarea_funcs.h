#pragma once
// Function declarations for deeply reimplemented PlantArea methods.
// Called from engine/reimpl/plantarea.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "BuildActions.h"
#include <cstdint>

// ============================================================
// PlantArea -- Crop field planting work (0x70 bytes)
// Inherits: Object::Internal -> Work (0x48) -> PlantArea
// Vtable: 0x140161890 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Main planting assignment logic.
// Iterates field cells, finds plantable spots, assigns citizen to plant.
// Returns: 1=done/idle, 2=work assigned. Uses int return type (bug class 7).
int PlantArea_Slot4_Impl(PlantArea* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot5/Execute (vtable +0x28): 3 bytes, returns false. Too small to hook.
bool PlantArea_Execute_Impl(PlantArea* self);

// Slot6/OnComplete (vtable +0x30): Removes BST entry, marks cells valid.
// param4 is packed: low 16 bits = primary cell, high 16 bits = paired cell.
// Returns int (bug class 7 -- action executor reads EAX).
int PlantArea_OnComplete_Impl(PlantArea* self, void* param2, void* param3, uint32_t param4);

// Slot7/OnCancel (vtable +0x38): Marks cells valid, removes paired BST entry.
// param4 is packed: low 16 bits = primary cell, high 16 bits = paired cell.
// Returns int (bug class 7 -- action executor reads EAX).
int PlantArea_OnCancel_Impl(PlantArea* self, void* param2, void* param3, uint32_t param4);

// Slot8/Cleanup (vtable +0x40): CancelAll -- loops BST, cancels each work item.
void PlantArea_Cleanup_Impl(PlantArea* self);

// Slot9/Validate (vtable +0x48): RemoveInvalid -- validates BST entries, removes dead ones.
// Returns bool: true if all valid. Hook may be disabled due to bug class.
bool PlantArea_Validate_Impl(PlantArea* self, void* param2);

// Slot10/GetTarget (vtable +0x50): Returns entity ptr from work's entity ref.
// Hidden return ptr ABI: RCX=self, RDX=retbuf.
void* PlantArea_GetTarget_Impl(PlantArea* self, void* outPtr);

// Slot11/GetPriority (vtable +0x58): 9 bytes, shared. Returns (field_50 > 0).
bool PlantArea_GetPriority_Impl(PlantArea* self);

// Serialize (vtable +0x18): Work::Serialize + BST + bool array + ExternalRef.
void PlantArea_Serialize_Impl(PlantArea* self, void* stream);

// ScalarDtor body: Sets vtables, frees array + BST, calls Work dtor.
void PlantArea_DtorBody(PlantArea* self);

// ScalarDtor (vtable +0x00): Calls dtor body, conditional free.
void* PlantArea_ScalarDtor_Impl(PlantArea* self, uint32_t flags);

// Resolve all function pointers needed by PlantArea.
// Called once during hook installation.
bool PlantArea_ResolveFuncs();
