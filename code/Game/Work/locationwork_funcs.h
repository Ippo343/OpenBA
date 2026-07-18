#pragma once
// Function declarations for deeply reimplemented LocationWork methods.
// Called from engine/reimpl/locationwork.cpp wrappers.

#include "LocationWork.h"
#include <cstdint>

// ============================================================
// LocationWork — Location-based work action (0x50 bytes)
// Inherits: Object::Internal -> Work (0x48) -> LocationWork
// Vtable: 0x1401616a0 (12 slots)
//
// Unlike AutoPickupWork-derived classes (HerbalistWork, ForesterWork),
// LocationWork has a SINGLE InternalBasePtr at +0x48 (not a BST).
// ============================================================

// Slot4 (vtable +0x20): Pathfinding/location work logic.
// Checks citizen storage, finds walkable position near workplace,
// creates WalkTowards action and assigns work.
// Returns: 0=fail, 1=already has target, 2=assigned.
int LocationWork_Slot4_Impl(LocationWork* self, void* citizenIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): Clear target, return false.
// Constructs null InternalBasePtr into +0x48.
bool LocationWork_Slot6_Impl(LocationWork* self);

// Slot7 (vtable +0x38): Clear target.
// Constructs null InternalBasePtr into +0x48.
// Returns int (bug class 7 -- action executor reads EAX).
int LocationWork_Slot7_Impl(LocationWork* self);

// Slot8 (vtable +0x40): Cancel single target's work.
// If target at +0x48 exists, cancels its work via CancelAllWork.
// Returns int (bug class 7 -- action executor reads EAX).
int LocationWork_Slot8_Impl(LocationWork* self);

// Slot9 (vtable +0x48): Validate target.
// Checks if the single target entity is still valid.
// Clears +0x48 if invalid, returns true if valid.
bool LocationWork_Slot9_Impl(LocationWork* self, void* citizenIBP);

// Slot10 (vtable +0x50): Get work entity sub-object.
// Returns entity from self->_field10's sub-object at +0x48.
void* LocationWork_Slot10_Impl(LocationWork* self, void* outPtr);

// Serialize (vtable +0x18): Work::Serialize + Serializer(v0)
//   + InternalBasePtr::Serialize(+0x48) + validate IsDerivedFrom.
void LocationWork_Serialize_Impl(LocationWork* self, void* stream);

// Execute (vtable +0x28): 3 bytes (XOR AL,AL; RET). Returns false.
// Too small to hook; implemented for standalone exe.
bool LocationWork_Execute_Impl(LocationWork* self);

// Slot11 (vtable +0x58): 10 bytes, checks +0x48 != 0. Returns bool.
// Too small to hook; implemented for standalone exe.
bool LocationWork_Slot11_Impl(LocationWork* self);

// ScalarDtor (0x14002a850): Sets vtables, calls Internal dtor, conditional free.
// Too small to hook; implemented for standalone exe.
void* LocationWork_ScalarDtor_Impl(LocationWork* self, uint32_t flags);

// Resolve all function pointers needed by LocationWork.
// Called once during hook installation.
bool LocationWork_ResolveFuncs();
