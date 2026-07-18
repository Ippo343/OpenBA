#pragma once
// Function declarations for deeply reimplemented HarvestArea methods.
// Called from engine/reimpl/harvestarea.cpp wrappers.

#include "HarvestArea.h"
#include <cstdint>

// ============================================================
// HarvestArea -- Crop/field harvest work action (0x68 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HarvestArea
// Vtable: 0x1401614a0 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Main crop harvest assignment logic (363 instructions).
// Iterates field cells, finds harvestable crops, assigns citizen to harvest.
// Returns: 1=done/idle, 2=work assigned. Uses int return type (bug class 7).
int HarvestArea_Slot4_Impl(HarvestArea* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): Insert work item into tree, remove if already present.
// Extracts low/high 16-bit from param4 packed value.
// Returns int (bug class 7 -- action executor reads EAX).
int HarvestArea_Slot6_Impl(HarvestArea* self, void* param2, void* param3, uint32_t param4);

// Slot7 (vtable +0x38): Tree node lookup/insertion with validation.
// Sets validity flag and removes stale paired entry from tree.
// Returns int (bug class 7 -- action executor reads EAX).
int HarvestArea_Slot7_Impl(HarvestArea* self, void* param2, void* param3, uint32_t param4);

// Slot8 (vtable +0x40): CancelAll -- loops tree, cancels each work item.
// Returns int (bug class 7 -- action executor reads EAX).
int HarvestArea_Slot8_Impl(HarvestArea* self);

// Slot9 (vtable +0x48): RemoveInvalid -- validates tree entries, removes dead ones.
// Returns int per bug class 7. Hook DISABLED (bug class 2 -- MOV RAX,RSP).
// Still implemented for standalone exe.
int HarvestArea_Slot9_Impl(HarvestArea* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity -- returns entity ptr from work's entity ref.
// Hidden return ptr ABI: RCX=self, RDX=retbuf.
void* HarvestArea_Slot10_Impl(HarvestArea* self, void* outPtr);

// Serialize (vtable +0x18): Work::Serialize + tree-set + array + int fields.
void HarvestArea_Serialize_Impl(HarvestArea* self, void* stream);

// ScalarDtor: dtor body sets vtables down chain, frees tree + array, then Work dtor.
// If (flags & 1), frees memory.
void* HarvestArea_ScalarDtor_Impl(HarvestArea* self, uint32_t flags);

// Resolution helper -- resolve all function pointers. Called once.
bool HarvestArea_ResolveFuncs();
