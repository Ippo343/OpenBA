#pragma once
// Function declarations for deeply reimplemented FireWork methods.
// Called from engine/reimpl/firework.cpp wrappers.

#include "FireWork.h"
#include <cstdint>

// ============================================================
// FireWork — fire suppression work action (0x60 bytes)
// Inherits: Object::Internal -> Work (0x48) -> FireWork
// Vtable: 0x1401612a0 (12 slots)
// ============================================================

// Slot4 (vtable +0x20, 0x1400e9620):
// Fire suppression work assignment — finds fire location, checks
// pathfinding, sets up citizen action queue for firefighting.
// Returns: 0 = no fire found, 1 = at capacity, 2 = work assigned.
// Uses int return type (NOT bool) per bug class 7.
int FireWork_Slot4_Impl(FireWork* self, void* citizenIBP, void* time);

// Slot9 (vtable +0x48, 0x1400e93c0):
// Fire AI validation — traverses BST at +0x50, checks each entity
// still needs fire suppression work, removes invalid entries.
// Returns: 1 if all entries removed (BST empty), 0 otherwise.
// Uses int return type (NOT bool) per bug class 7.
int FireWork_Slot9_Impl(FireWork* self, void* citizenIBP);

// Slot6 (vtable +0x30, 0x1400e9600):
// OnComplete — removes citizen entry from BST at +0x50, returns false (0).
// Uses int return type (NOT bool) per bug class 7.
int FireWork_Slot6_Impl(FireWork* self, void* citizenIBP);

// Slot7 (vtable +0x38, 0x1400e95f0):
// OnCancel — thunk to BST remove by key on +0x50.
// 6-byte thunk in original: ADD RCX,0x50; JMP CitizenAI_impl_5520.
// Returns int per bug class 7.
int FireWork_Slot7_Impl(FireWork* self, void* citizenIBP);

// Slot8 (vtable +0x40, 0x1400e99a0):
// CancelAll/Cleanup — walks BST at +0x50, removes all pending
// actions from each citizen's state machine component.
// Returns int per bug class 7.
int FireWork_Slot8_Impl(FireWork* self);

// Slot10 (vtable +0x50, 0x1400e9a30):
// GetTarget — returns gameWorld->field_b0->field_30.
// Uses hidden return pointer ABI: RCX=self, RDX=retbuf.
// NOT HOOKED — too small (25 bytes, uses RIP-relative addressing).
void* FireWork_Slot10_Impl(FireWork* self, void* retbuf);

// Resolve all function pointers needed by FireWork reimpl.
// Call after ResolveInit(). Returns true if all critical pointers resolved.
bool FireWork_ResolveFuncs();
