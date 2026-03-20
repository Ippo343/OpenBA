#pragma once
// Function declarations for deeply reimplemented BuildStructure Work methods.
// Called from engine/reimpl/buildstructure.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "BuildActions.h"
#include <cstdint>

// ============================================================
// BuildStructure Work class — structure construction
// Vtable: 0x1401610a0 (12 slots)
// Size: 0x70 bytes
// ============================================================

// Ctor: Initialize all BuildStructure fields.
// Original: 0x1400e5a90
void* BuildStructure_Ctor(BuildStructure* self);

// ScalarDtor (vtable +0x00): Calls BaseDtor, conditionally frees memory.
// Original: 0x140029680
void* BuildStructure_ScalarDtor(BuildStructure* self, uint32_t flags);

// BaseDtor: Destroys BuildStructure fields, frees arrays and tree.
// Original: 0x1400e5920
// NOT hooked directly (called by ScalarDtor).
void BuildStructure_BaseDtor(BuildStructure* self);

// Serialize (vtable +0x18, 0x1400E5850):
// Serializes Work base, then BuildStructure fields.
void BuildStructure_Serialize(BuildStructure* self, void* stream);

// Execute (vtable +0x20, 0x1400E4FC0):
// Finds unbuilt construction point, validates walkability, sets up actions.
// Returns: 1 = not ready, 2 = work assigned.
// Uses int return type (NOT bool) per bug class 7.
int BuildStructure_Execute(BuildStructure* self, void* citizenPtr);

// OnComplete (vtable +0x30): Called when a work slot finishes.
// Original: 0x1400e4f80. Returns int per bug class 7.
int BuildStructure_OnComplete(BuildStructure* self, void* citizenIBP,
                                void* unused, int slotIndex);

// OnCancel (vtable +0x38): Called when a work slot is cancelled.
// Original: 0x1400e4f40. Returns int per bug class 7.
int BuildStructure_OnCancel(BuildStructure* self, void* citizenIBP,
                              void* unused, int slotIndex);

// Cleanup (vtable +0x40): Walks the binary tree at +0x60, releasing
// entities back to the citizen state machine.
// Original: 0x1400e57a0. Returns int per bug class 7.
int BuildStructure_Cleanup(BuildStructure* self);

// GetPriority (vtable +0x58): Returns true if field_68 > 0.
// Original: 0x140029670 (NOT hookable — only 9 bytes)
bool BuildStructure_GetPriority(BuildStructure* self);

// Shared (vtable +0x28, 0x1400f7540): Always returns false.
// Original is "xor al,al; ret" (3 bytes, unhookable).
// Shared across AutoPickupWork, BuildRoad, BuildStructure, etc.
inline bool BuildStructure_Shared(BuildStructure* /*self*/) { return false; }

// GetTarget (vtable +0x50, 0x1400e5830): Returns world building entity.
// Reads game+0xB0→+0x38 (the world's current build target).
// Uses hidden return pointer ABI (member function returning InternalBasePtr).
// params: RCX=self, RDX=retBuf (InternalBasePtr*)
void* BuildStructure_GetTarget(BuildStructure* self, void* retBuf);

// Validate (vtable +0x48, 0x1400e4be0): Validates work assignments.
// Walks the BST at +0x60, checks if each citizen has a matching CompleteTask
// action in their state machine. Builds lists of citizens to remove and
// slot indices to free. Returns 0 if all validated, 1 if tree is empty.
// Very complex — involves BST traversal, dynamic array management,
// and citizen action inspection.
int BuildStructure_Validate(BuildStructure* self, void* citizenIBP);
