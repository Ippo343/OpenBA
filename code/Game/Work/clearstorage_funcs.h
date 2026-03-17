#pragma once
// Function declarations for deeply reimplemented ClearStorage methods.
// Called from engine/reimpl/clearstorage.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "ClearStorage.h"
#include <cstdint>

// Slot4/Execute (vtable +0x20, 0x1400e6c50): Complex storage-clearing work execution.
// Iterates storage items, finds a destination to move them, sets up pathfinding.
// Returns 0 (fail), 1 (wait/retry), or 2 (success/state change).
int ClearStorage_Execute(ClearStorage* self, void* citizenAI, void* time);

// Slot9/Validate (vtable +0x48, 0x1400e69f0): Validates pending tasks in the BST.
// Traverses BST at +0x58, checks each entry via Work::CanStartWork. Removes
// invalid entries. Returns true if no entries remain (all cleared).
int ClearStorage_Validate(ClearStorage* self, void* citizenAI);
