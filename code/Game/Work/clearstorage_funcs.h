#pragma once
// Function declarations for deeply reimplemented ClearStorage methods.
// Called from engine/reimpl/clearstorage.cpp wrappers.

#include "ClearStorage.h"
#include <cstdint>

// Initialize local function pointers for ClearStorage methods.
// Must be called before any ClearStorage_* function is used.
void ClearStorage_InitFuncs();

// ScalarDtor (vtable +0x00, 0x14002a100): Calls base dtor, conditionally frees.
void* ClearStorage_ScalarDtor_Impl(ClearStorage* self, uint32_t flags);

// Slot4/Execute (vtable +0x20, 0x1400e6c50): Complex storage-clearing work execution.
// Iterates storage items, finds a destination to move them, sets up pathfinding.
// Returns 0 (fail), 1 (wait/retry), or 2 (success/state change).
int ClearStorage_Execute(ClearStorage* self, void* citizenAI, void* time);

// Slot6 (vtable +0x30, 0x1400e6c30): BST find-and-remove by key, returns false.
// Calls BSTFindRemove(self+0x58, param2), then returns 0.
int ClearStorage_Slot6_Impl(ClearStorage* self, void* param2);

// Slot7/OnCancel (vtable +0x38, 0x1400e6c20): same BST find-and-remove as slot 6
// (retail is the thunk `ADD RCX,0x58; JMP CitizenAI_impl_5520`).
int ClearStorage_Slot7_Impl(ClearStorage* self, void* param2);

// Slot11/HasActiveTasks (vtable +0x58, 0x14002a0f0): `*(int*)(self+0x60) > 0`.
int ClearStorage_Slot11_Impl(ClearStorage* self);

// Slot8/CancelAll (vtable +0x40, 0x1400e72a0): Iterates BST at +0x58,
// finds each citizen entity and cancels pending work via TaskCancel.
void ClearStorage_CancelAll_Impl(ClearStorage* self);

// Slot9/Validate (vtable +0x48, 0x1400e69f0): Validates pending tasks in the BST.
// Traverses BST at +0x58, checks each entry via Work::CanStartWork. Removes
// invalid entries. Returns true if no entries remain (all cleared).
int ClearStorage_Validate(ClearStorage* self, void* citizenAI);

// Slot10/GetTarget (vtable +0x50, 0x1400e7330): Returns the map entity IBP
// from the game world singleton: gameWorld->+0xb0->+0x30.
void* ClearStorage_GetTarget_Impl(ClearStorage* self, void* retbuf);
