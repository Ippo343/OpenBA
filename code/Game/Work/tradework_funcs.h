#pragma once
// Function declarations for deeply reimplemented TradeWork methods.
// Called from engine/reimpl/tradework.cpp wrappers.

#include "TradeWork.h"
#include <cstdint>

// ============================================================
// TradeWork -- Trade post work (0x58 bytes)
// Inherits: Object::Internal -> Work (0x48) -> TradeWork
// Vtable: 0x140161a10 (12 slots)
// ============================================================

// Slot0 (vtable +0x00): ScalarDtor. Calls dtor body, conditional free.
void* TradeWork_ScalarDtor_Impl(TradeWork* self, uint32_t flags);

// Slot1 (vtable +0x08): VectorDtor. Tiny, returns DAT_1401c6cc8.
// Not implemented (returns type info, handled by original).

// Slot2 (vtable +0x10): BaseDtor. Tiny, returns TypeInfo_TradeWork.
// Not implemented (returns type info, handled by original).

// Slot3/Serialize (vtable +0x18): Work_Serialize + BST.
void TradeWork_Serialize_Impl(TradeWork* self, void* stream);

// Slot4 (vtable +0x20): Complex trade assignment logic.
// Returns: 0=continue, 1=done/no work, 2=work assigned.
int TradeWork_Slot4_Impl(TradeWork* self, void* citizenIBP, uint64_t param3, uint64_t param4);

// Slot5/Execute (vtable +0x28): 3 bytes, returns false. Shared with all work classes.
bool TradeWork_Execute_Impl(TradeWork* self);

// Slot6 (vtable +0x30): BSTFindRemove on self+0x48, returns false.
bool TradeWork_Slot6_Impl(TradeWork* self, void* param2);

// Slot7 (vtable +0x38): BSTFindRemove on self+0x48 (tail call thunk).
// Returns int (bug class 7 -- action executor reads EAX).
int TradeWork_Slot7_Impl(TradeWork* self, void* param2);

// Slot8 (vtable +0x40): CancelAll. Loops BST at +0x48 while count > 0.
// Returns int (bug class 7 -- action executor reads EAX).
int TradeWork_Slot8_Impl(TradeWork* self);

// Slot9 (vtable +0x48): RemoveInvalid. Iterates BST, removes invalid entries.
bool TradeWork_Slot9_Impl(TradeWork* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity. Returns entity from sub-object.
void* TradeWork_Slot10_Impl(TradeWork* self, void* outPtr);

// Slot11 (vtable +0x58): GetPriority/HasWorkers. Returns (workerCount > 0).
bool TradeWork_Slot11_Impl(TradeWork* self);

// Dtor body (called by ScalarDtor). Sets vtables, frees BST, calls base dtor.
void TradeWork_DtorBody(TradeWork* self);

// Resolve all function pointers needed by TradeWork.
// Called once during hook installation.
bool TradeWork_ResolveFuncs();
