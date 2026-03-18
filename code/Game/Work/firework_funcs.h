#pragma once
// Function declarations for deeply reimplemented FireWork methods.
// Called from engine/reimpl/firework.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

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

// Resolve all function pointers needed by FireWork reimpl.
// Call after ResolveInit(). Returns true if all critical pointers resolved.
bool FireWork_ResolveFuncs();
