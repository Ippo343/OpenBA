#pragma once
// Function declarations for deeply reimplemented HerbalistWork methods.
// Called from engine/reimpl/herbalistwork.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "HerbalistWork.h"
#include <cstdint>

// ============================================================
// HerbalistWork — Herbalist gathering work action (0x60 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> HerbalistWork
// Vtable: 0x1401615a0 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Main work-assignment logic (457 instructions).
// Finds nearby herbs/natural resources, validates growth, assigns gather tasks.
// Returns: 0=continue, 1=done, 2=spawn. Uses int return type (bug class 7).
int HerbalistWork_Slot4_Impl(HerbalistWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// Slot6 (vtable +0x30): NOT HOOKABLE (RIP-relative CALL in 20-byte thunk).
// Calls CitizenAI_impl_5520(self+0x48, param2) then returns false.
// Implemented for standalone exe only.
bool HerbalistWork_Slot6_Impl(HerbalistWork* self, void* param2);

// Slot7 (vtable +0x38): 9-byte thunk, too small to hook.
// Calls CitizenAI_impl_5520(self+0x48, param2).
// Implemented for standalone exe only.
void HerbalistWork_Slot7_Impl(HerbalistWork* self, void* param2);

// Slot8 (vtable +0x40): CancelAll.
// Loops through tree at +0x48 while count (+0x50) > 0, cancels each work item.
void HerbalistWork_Slot8_Impl(HerbalistWork* self);

// Slot9 (vtable +0x48): RemoveInvalid (164 instructions).
// Iterates tree, validates entries, removes invalid ones.
// Hook DISABLED (bug class 2 — MOV RAX,RSP). Implement for standalone exe.
bool HerbalistWork_Slot9_Impl(HerbalistWork* self, void* param2);

// Slot10 (vtable +0x50): GetWorkEntity.
// Returns entity from this->_field10's sub-object at +0x48.
void* HerbalistWork_Slot10_Impl(HerbalistWork* self, void* outPtr);

// Serialize (vtable +0x18): Work::Serialize + tree-set + int field.
void HerbalistWork_Serialize_Impl(HerbalistWork* self, void* stream);

// ScalarDtor body (0x1400EEC40): Sets vtables down chain, frees tree nodes, calls base dtor.
void HerbalistWork_DtorBody(HerbalistWork* self);

// ScalarDtor (0x14002A640): Calls dtor body, conditional free.
void* HerbalistWork_ScalarDtor_Impl(HerbalistWork* self, uint32_t flags);

// Resolve all function pointers needed by HerbalistWork.
// Called once during hook installation.
bool HerbalistWork_ResolveFuncs();
