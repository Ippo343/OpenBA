#pragma once
// Function declarations for deeply reimplemented TendArea methods.
// Called from engine/reimpl/tendarea.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "TendArea.h"
#include <cstdint>

// ============================================================
// TendArea -- Area tending work (0x60 bytes)
// Inherits: Object::Internal -> Work (0x48) -> TendArea
// Vtable: 0x140161990 (12 slots)
// ============================================================

// Slot0 (vtable +0x00): ScalarDtor. Calls dtor body, conditional free.
void* TendArea_ScalarDtor_Impl(TendArea* self, uint32_t flags);

// Slot1 (vtable +0x08): VectorDtor. Tiny, returns DAT_1401c6c90.
// Not implemented (returns type info, handled by original).

// Slot2 (vtable +0x10): BaseDtor. Tiny, returns TypeInfo_TendArea.
// Not implemented (returns type info, handled by original).

// Slot3/Serialize (vtable +0x18): Work_Serialize + BST + int field.
void TendArea_Serialize_Impl(TendArea* self, void* stream);

// Slot4 (vtable +0x20): Complex tending assignment logic.
// Returns: 0=continue, 1=done/no work, 2=work assigned.
int TendArea_Slot4_Impl(TendArea* self, void* citizenIBP, uint64_t param3, uint64_t param4);

// Slot5/Execute (vtable +0x28): 3 bytes, returns false. Shared with all work classes.
bool TendArea_Execute_Impl(TendArea* self);

// Slot6 (vtable +0x30): BSTFindRemove on self+0x48, returns false.
bool TendArea_Slot6_Impl(TendArea* self, void* param2);

// Slot7 (vtable +0x38): BSTFindRemove on self+0x48 (tail call thunk).
void TendArea_Slot7_Impl(TendArea* self, void* param2);

// Slot8 (vtable +0x40): CancelAll. Loops BST at +0x48 while count > 0.
void TendArea_Slot8_Impl(TendArea* self);

// Slot9 (vtable +0x48): RemoveInvalid. Iterates BST, removes invalid entries.
bool TendArea_Slot9_Impl(TendArea* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity. Returns entity from sub-object.
void* TendArea_Slot10_Impl(TendArea* self, void* outPtr);

// Slot11 (vtable +0x58): GetPriority/HasWorkers. Returns (workerCount > 0).
bool TendArea_Slot11_Impl(TendArea* self);

// Dtor body (called by ScalarDtor). Sets vtables, frees BST, calls base dtor.
void TendArea_DtorBody(TendArea* self);

// Resolve all function pointers needed by TendArea.
// Called once during hook installation.
bool TendArea_ResolveFuncs();
