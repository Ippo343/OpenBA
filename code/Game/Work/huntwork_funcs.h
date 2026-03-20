#pragma once
// Function declarations for deeply reimplemented HuntWork methods.
// Called from engine/reimpl/huntwork.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "HuntWork.h"
#include <cstdint>

// ============================================================
// HuntWork — Hunting work action (0x58 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HuntWork
// Vtable: 0x140161620 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Hunt target assignment logic (326 instructions).
// Searches for valid hunt target, creates PerformWork to harvest.
// Returns: 0=continue, 1=done, 2=spawn. Uses int return type (bug class 7).
int HuntWork_Slot4_Impl(HuntWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): NOT HOOKABLE (RIP-relative CALL in 20-byte thunk).
// Calls CitizenAI_impl_5520(self+0x48, param2) then returns false.
// Implemented for standalone exe only.
bool HuntWork_Slot6_Impl(HuntWork* self, void* param2);

// Slot7 (vtable +0x38): 9-byte thunk, too small to hook.
// Calls CitizenAI_impl_5520(self+0x48, param2).
// Returns int (bug class 7 -- action executor reads EAX).
int HuntWork_Slot7_Impl(HuntWork* self, void* param2);

// Slot8 (vtable +0x40): CancelAll.
// Loops through tree at +0x48 while count (+0x50) > 0, cancels each work item.
// Returns int (bug class 7 -- action executor reads EAX).
int HuntWork_Slot8_Impl(HuntWork* self);

// Slot9 (vtable +0x48): RemoveInvalid (164 instructions).
// Iterates tree, validates entries, removes invalid ones.
// Hook DISABLED (bug class 3 -- MOV RAX,RSP). Implement for standalone exe.
bool HuntWork_Slot9_Impl(HuntWork* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity.
// Returns entity from this->_field10's sub-object at +0x48.
void* HuntWork_Slot10_Impl(HuntWork* self, void* outPtr);

// Slot11 (vtable +0x58): HasWorkers. 9-byte thunk, too small to hook.
// Returns (this->_field50 > 0).
bool HuntWork_Slot11_Impl(HuntWork* self);

// Slot5/Execute (vtable +0x28): 3 bytes, returns false. Too small to hook.
bool HuntWork_Execute_Impl(HuntWork* self);

// Serialize (vtable +0x18): Work::Serialize + tree-set (no extra int field).
void HuntWork_Serialize_Impl(HuntWork* self, void* stream);

// ScalarDtor body (0x1400EF680): Sets vtables down chain, frees tree nodes, calls base dtor.
void HuntWork_DtorBody(HuntWork* self);

// ScalarDtor (0x14002A700): Calls dtor body, conditional free.
void* HuntWork_ScalarDtor_Impl(HuntWork* self, uint32_t flags);

// Resolve all function pointers needed by HuntWork.
// Called once during hook installation.
bool HuntWork_ResolveFuncs();
