#pragma once
// Function declarations for deeply reimplemented HarvestOrchard methods.
// Called from engine/reimpl/harvestorchard.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "HarvestOrchard.h"
#include <cstdint>

// ============================================================
// HarvestOrchard -- Orchard harvest work action (0x68 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestOrchard
// Vtable: 0x140161520 (12 slots)
// Same struct layout as HarvestArea, different harvest logic.
// ============================================================

// Slot4 (vtable +0x20): Main orchard harvest assignment logic (303 instructions).
// Iterates orchard cells, finds harvestable trees, assigns citizen.
// Returns: 1=done/idle, 2=work assigned. Uses int return type (bug class 7).
int HarvestOrchard_Slot4_Impl(HarvestOrchard* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): Insert work item into tree, remove if present.
// Simpler than HarvestArea -- single int key, no paired entry.
// Returns int (bug class 7 -- action executor reads EAX).
int HarvestOrchard_Slot6_Impl(HarvestOrchard* self, void* param2, void* param3, int param4);

// Slot7 (vtable +0x38): Tree node lookup/insertion with validation.
// Returns int (bug class 7 -- action executor reads EAX).
int HarvestOrchard_Slot7_Impl(HarvestOrchard* self, void* param2, void* param3, int param4);

// Slot8 (vtable +0x40): CancelAll -- loops tree, cancels each work item.
// Returns int (bug class 7 -- action executor reads EAX).
int HarvestOrchard_Slot8_Impl(HarvestOrchard* self);

// Slot9 (vtable +0x48): RemoveInvalid -- validates tree entries, removes dead ones.
// Returns int per bug class 7. Hook DISABLED (bug class 2 -- MOV RAX,RSP).
int HarvestOrchard_Slot9_Impl(HarvestOrchard* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity -- returns entity ptr.
void* HarvestOrchard_Slot10_Impl(HarvestOrchard* self, void* outPtr);

// Serialize (vtable +0x18): Work::Serialize + tree-set + array + int fields.
void HarvestOrchard_Serialize_Impl(HarvestOrchard* self, void* stream);

// ScalarDtor: dtor chain, free tree + array, then Work dtor.
void* HarvestOrchard_ScalarDtor_Impl(HarvestOrchard* self, uint32_t flags);

// Resolution helper -- resolve all function pointers. Called once.
bool HarvestOrchard_ResolveFuncs();
