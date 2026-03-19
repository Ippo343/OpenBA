#pragma once
// Function declarations for deeply reimplemented ForesterWork methods.
// Called from engine/reimpl/foresterwork.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "ForesterWork.h"
#include <cstdint>

// ============================================================
// ForesterWork — Forester resource gathering work action (0x60 bytes)
// Inherits: Object::Internal -> Work -> AutoPickupWork (0x48) -> ForesterWork
// Vtable: 0x1401613a0 (12 slots)
// ============================================================

// Slot4 (vtable +0x20): Main work-assignment logic (599 instructions).
// Checks auto-pickup assignments, finds nearby trees, checks terrain,
// assigns forester to cut or plant tasks.
// Returns: 0=continue, 1=done, 2=spawn. Uses int return type (bug class 7).
int ForesterWork_Slot4_Impl(ForesterWork* self, void* entityIBP, uint64_t param3, uint64_t param4);

// DtorBody (0x1400EBA00): Sets vtables down chain, frees BST at +0x48.
void ForesterWork_DtorBody(ForesterWork* self);

// ScalarDtor (0x14002A370): Calls dtor body, conditional free.
void* ForesterWork_ScalarDtor_Impl(ForesterWork* self, uint32_t flags);

// Serialize (vtable +0x18, 0x1400EB970):
// Work::Serialize + version block + BST + int serialization.
void ForesterWork_Serialize_Impl(ForesterWork* self, void* stream);

// Slot6 (vtable +0x30, 0x1400EAEA0): NOT HOOKABLE (RIP-relative CALL).
// Removes entity from BST at +0x48, returns false.
// Implemented for standalone exe only.
int ForesterWork_Slot6_Impl(ForesterWork* self, void* param2);

// Slot7 (vtable +0x38, 0x1400EAE90): 9-byte thunk, too small to hook.
// Calls BSTFindRemove on the BST at self+0x48.
// Implemented for standalone exe only.
void ForesterWork_Slot7_Impl(ForesterWork* self, void* param2);

// Slot8 (vtable +0x40, 0x1400EB8A0): CancelAll.
// Loops through tree at +0x48 while count > 0, cancels each work item.
void ForesterWork_Slot8_Impl(ForesterWork* self);

// Slot9 (vtable +0x48, 0x1400EAC60): RemoveInvalid (164 instructions).
// BST traversal + validation + removal of invalid entries.
// Returns: 1 if all valid, 0 if any removed. Uses int return type (bug class 7).
int ForesterWork_Slot9_Impl(ForesterWork* self, void* param2);

// Slot10 (vtable +0x50, 0x1400EB910): GetWorkEntity.
// Returns entity from this->_field10's sub-object at +0x48.
// Hidden return ptr ABI: RCX=self, RDX=outPtr.
void* ForesterWork_Slot10_Impl(ForesterWork* self, void* outPtr);

// Resolve all function pointers needed by ForesterWork.
// Called once during hook installation.
bool ForesterWork_ResolveFuncs();
