#pragma once
// Function declarations for deeply reimplemented PastureWork methods.
// Called from engine/reimpl/pasturework.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "PastureWork.h"
#include <cstdint>

// ============================================================
// PastureWork -- Pasture animal work action (0x58 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> PastureWork
// Vtable: 0x1401617a0 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Pasture work assignment logic (509 instructions).
// Complex: checks animal count, resource capacity, remove animals, find empty slots.
// Returns: 0=continue, 1=done, 2=spawn. Uses int return type (bug class 7).
int PastureWork_Slot4_Impl(PastureWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): NOT HOOKABLE (RIP-relative CALL in 20-byte thunk).
// Calls CitizenAI_impl_5520(self+0x48, param2) then returns false.
// Implemented for standalone exe only.
bool PastureWork_Slot6_Impl(PastureWork* self, void* param2);

// Slot7 (vtable +0x38): Tree insertion with validation.
// Unlike other Work classes, PastureWork::Slot7 does more than just BSTFindRemove:
// it also clears the "flagged for removal" bit on the entity.
// Returns int (bug class 7 -- action executor reads EAX).
int PastureWork_Slot7_Impl(PastureWork* self, void* param2, void* param3);

// Slot8 (vtable +0x40): CancelAll.
// Loops through tree at +0x48 while count (+0x50) > 0, cancels each work item.
// Returns int (bug class 7 -- action executor reads EAX).
int PastureWork_Slot8_Impl(PastureWork* self);

// Slot9 (vtable +0x48): RemoveInvalid (164 instructions).
// Iterates tree, validates entries, removes invalid ones.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP). Implement for standalone exe.
bool PastureWork_Slot9_Impl(PastureWork* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity.
// Returns entity from this->_field10's sub-object at +0x48.
void* PastureWork_Slot10_Impl(PastureWork* self, void* outPtr);

// Slot11 (vtable +0x58): HasWorkers. 9-byte thunk, too small to hook.
// Returns (this->_field50 > 0).
bool PastureWork_Slot11_Impl(PastureWork* self);

// Slot5/Execute (vtable +0x28): 3 bytes, returns false. Too small to hook.
bool PastureWork_Execute_Impl(PastureWork* self);

// Serialize (vtable +0x18): Work::Serialize + tree-set (no extra int field).
void PastureWork_Serialize_Impl(PastureWork* self, void* stream);

// ScalarDtor body (0x1400F2CE0): Sets vtables down chain, frees tree nodes, calls base dtor.
void PastureWork_DtorBody(PastureWork* self);

// ScalarDtor (0x14002BF70): Calls dtor body, conditional free.
void* PastureWork_ScalarDtor_Impl(PastureWork* self, uint32_t flags);

// Resolve all function pointers needed by PastureWork.
// Called once during hook installation.
bool PastureWork_ResolveFuncs();
