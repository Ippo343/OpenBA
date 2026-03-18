#pragma once
// FireWork function declarations -- clean game logic layer.
// Called from engine/reimpl/firework.cpp hook wrappers.
// Must NOT depend on hooks.h -- portable for standalone exe.

#include <cstdint>

struct FireWork;

// FireWork::Serialize (vtable +0x18, 0x1400e9a50)
// Calls Work::Serialize, then serializes BST at +0x50 and int at +0x48.
void FireWork_Serialize_Impl(FireWork* self, void* stream);

// FireWork::Slot4 (vtable +0x20, 0x1400e9620)
// Fire suppression work logic: finds nearby fire, validates path, assigns work.
// Returns: 0 = no fire found, 1 = retry, 2 = action pushed.
// Uses int return type (NOT bool) per bug class 7.
int FireWork_Slot4_Impl(FireWork* self, void* citizenIBP, void* time);

// FireWork::Slot6 (vtable +0x30, 0x1400e9600)
// Removes citizen from BST at +0x50 (tree remove by key). Returns 0.
void FireWork_Slot6_Impl(FireWork* self, void* citizenIBP);

// FireWork::Slot8 (vtable +0x40, 0x1400e99a0)
// Cleanup: walks BST at +0x50, calls StateMachine_ReleaseTasks on each citizen.
void FireWork_Slot8_Impl(FireWork* self);

// FireWork::Slot9 (vtable +0x48, 0x1400e93c0)
// Validates all assigned citizens: walks BST, removes invalid entries.
// Returns: true if all entries removed (BST empty), false otherwise.
int FireWork_Slot9_Impl(FireWork* self, void* citizenIBP);

// Resolution helper -- resolves all local function pointers.
bool FireWork_ResolveFuncs();
