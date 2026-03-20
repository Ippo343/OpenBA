#pragma once
// Function declarations for deeply reimplemented ClearArea methods.
// Called from engine/reimpl/cleararea.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "BuildActions.h"
#include <cstdint>

// ============================================================
// ClearArea — area clearing work action (0x78 bytes)
// Inherits: Object::Internal -> Work (0x48) -> ClearArea
// Vtable: 0x140161110 (12 slots)
// ============================================================

// Ctor: Initialize all fields to defaults.
void* ClearArea_Ctor(ClearArea* self);

// ScalarDtor (vtable +0x00): Calls BaseDtor then conditionally frees memory.
void* ClearArea_ScalarDtor(ClearArea* self, uint32_t flags);

// BaseDtor: Resets vtables, frees tree nodes, calls ~Internal.
void ClearArea_BaseDtor(ClearArea* self);

// Serialize (vtable +0x18): Serializes Work base + ClearArea fields.
void ClearArea_Serialize(ClearArea* self, void* stream);

// Execute (vtable +0x20): Complex execution with tree searches, map queries.
// Returns: 0=continue, 1=done, 2=spawn. Uses int return type (bug class 7).
int ClearArea_Execute(void* self, void* actionStack, void* time);

// OnComplete (vtable +0x30): Removes completed tree entry.
// Returns int per bug class 7.
int ClearArea_OnComplete(ClearArea* self, void* citizen, void* treeEntry, int cellIndex);

// OnCancel (vtable +0x38): Forwards to Cleanup.
// Returns int per bug class 7.
int ClearArea_OnCancel(ClearArea* self, void* citizen, void* treeEntry);

// Cleanup (vtable +0x40): Releases all assigned citizens.
// Returns int per bug class 7.
int ClearArea_Cleanup(ClearArea* self);

// Validate (vtable +0x48, 0x1400e5b10): Validates entity and cell BSTs.
// Walks entity tree at +0x48 and cell tree at +0x58, checking each
// entry with FUN_1400f7100. Invalid entity entries are removed.
// If any cell tree entry is invalid, returns 0 immediately.
// Returns: 1 if all valid or trees empty, 0 if cell tree has invalid entry.
// NOT HOOKED — bug class 3 (RSP capture), standalone exe only.
int ClearArea_Validate(ClearArea* self, void* citizenIBP);

// GetTarget (vtable +0x50): Returns map entity from gameWorld.
void* ClearArea_GetTarget(ClearArea* self, void* retbuf);

// GetPriority (vtable +0x58): Returns true if tree count > 0.
int ClearArea_GetPriority(ClearArea* self);
