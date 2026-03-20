#pragma once
// Function declarations for deeply reimplemented MarketWork methods.
// Called from engine/reimpl/marketwork.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "MarketWork.h"
#include <cstdint>

// ============================================================
// MarketWork -- Market trading work action (0x58 bytes)
// Inherits: Object::Internal -> Work (0x48) -> MarketWork
// Vtable: 0x140161710 (12 slots)
//
// CRITICAL: MarketWork uses a DIFFERENT BST node layout than
// HuntWork/PastureWork/HerbalistWork:
//   +0x00: key (InternalBasePtr)
//   +0x08: data pointer
//   +0x10: data2
//   +0x18: parent
//   +0x20: left child
//   +0x28: right child
//   Node size = 0x38 bytes
// vs HerbalistWork BST nodes which use +0x08=parent, +0x10=left, +0x18=right
// ============================================================

// Slot4 (vtable +0x20): Main market trading logic (~400+ instructions).
// DISABLED hook (MOV RAX,RSP). Extremely complex -- calls original for now.
// Uses int return type (bug class 7).
int MarketWork_Slot4_Impl(MarketWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): Add entity to BST. DISABLED hook (relative CALL).
// Calls MarketWork_sub_aaa0(self+0x48, param2) then returns false.
bool MarketWork_Slot6_Impl(MarketWork* self, void* param2);

// Slot7 (vtable +0x38): Not a function in Ghidra, skip.
// Too small / not hookable. Implemented for standalone exe only.
// Returns int (bug class 7 -- action executor reads EAX).
int MarketWork_Slot7_Impl(MarketWork* self, void* param2);

// Slot8 (vtable +0x40): CancelAll. DISABLED hook (short Jcc).
// Loops through BST at +0x48 while count (+0x50) > 0, cancels each work item.
// Uses DIFFERENT BST offsets: left child at +0x20 (not +0x10).
// Returns int (bug class 7 -- action executor reads EAX).
int MarketWork_Slot8_Impl(MarketWork* self);

// Slot9 (vtable +0x48): RemoveInvalid. DISABLED hook (MOV RAX,RSP).
// Iterates BST, validates entries, removes invalid ones.
// Uses DIFFERENT BST offsets: left=+0x20, right=+0x28, parent=+0x18.
bool MarketWork_Slot9_Impl(MarketWork* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity. ENABLED hook.
// Returns entity from this->_field10's sub-object at +0x48.
void* MarketWork_Slot10_Impl(MarketWork* self, void* outPtr);

// Serialize (vtable +0x18): Work::Serialize + versioned BST serialization.
// ENABLED hook. Version=1 serializer with v0/v1 migration path.
void MarketWork_Serialize_Impl(MarketWork* self, void* stream);

// Execute (vtable +0x28): 3 bytes, returns false. Too small to hook.
// Implemented for standalone exe only.
bool MarketWork_Execute_Impl(MarketWork* self);

// ScalarDtor body: Sets vtables down chain, frees tree nodes, calls base dtor.
// Uses DIFFERENT tree cleanup (FUN_14002af40) -- frees node data at +0x08.
void MarketWork_DtorBody(MarketWork* self);

// ScalarDtor (0x14002a910): Calls dtor body, conditional free.
void* MarketWork_ScalarDtor_Impl(MarketWork* self, uint32_t flags);

// Slot11 (vtable +0x58): 9 bytes, returns (this->_field50 > 0). Too small to hook.
bool MarketWork_Slot11_Impl(MarketWork* self);

// Resolve all function pointers needed by MarketWork.
// Called once during hook installation.
bool MarketWork_ResolveFuncs();
