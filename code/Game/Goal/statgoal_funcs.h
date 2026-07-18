#pragma once
// Function declarations for deeply reimplemented StatGoal methods.
// Called from engine/reimpl/statgoal.cpp wrappers.

#include "AchievementGoals.h"
#include <cstdint>

// ============================================================
// StatGoal — achievement goal requiring stat comparison
// Vtable: 0x1401604c0 (11 slots)
// Size: 0x80 bytes
// ============================================================

// Slot3 (vtable +0x18, 0x1400daa10): Returns constant 1.
// Original is "mov eax,1; ret" (6 bytes, unhookable).
inline int StatGoal_Slot3(StatGoal* /*self*/) { return 1; }

// Slot4 (vtable +0x20, 0x14003a912): Base class JMP thunk (6 bytes, unhookable).
// Inherited from Achievement::Goal — no override needed.

// Slot5/Load (vtable +0x28, 0x1400da8b0): Serialize goal fields.
// Calls Achievement::Goal::Serialize, then serializes stat-specific fields:
//   +0x30..+0x34: two ints via Serializer vtable +0x98 (serialize enum/int)
//   +0x38: int via vtable +0x98
//   +0x3c: int via Serialize_Int
//   +0x40: bool via Serialize_Bool
//   +0x48: String::Serialize (statA resource name)
//   +0x60: String::Serialize (statB resource name)
//   +0x78: int via vtable +0x98
//   +0x7c: int via Serialize_Int
void StatGoal_Slot5(StatGoal* self, void* stream);

// Slot6/Cleanup (vtable +0x30, 0x140026520): Reset to default state.
// Calls scalar dtor with flags=0 (cleanup without free), then re-constructs.
void StatGoal_Slot6(StatGoal* self);

// Slot7 (vtable +0x38, 0x140001640): Base class stub (7 bytes, unhookable).
// Returns description pointer — inherited from Achievement::Goal.

// Slot8/Check (vtable +0x40, 0x1400da720): Check if stat goal is met.
// Reads statA via ExternalPtr + stat lookup dispatch, optionally reads statB,
// compares using comparisonType switch (EQ/NE/LT/LE/GT/GE).
// Returns: 0 (not met), 1 (met, or not met + requireOtherGoalsMet),
//          2 (met + requireOtherGoalsMet).
int StatGoal_Slot8(StatGoal* self);

// Slot9 (vtable +0x48, 0x14003ab5e): Base class — not a function entry.

// Slot10/CreateInstance (vtable +0x50, 0x140026510): Allocate + construct StatGoal.
// Original: "test rcx,rcx; jnz StatGoal_Ctor; xor eax,eax; ret" (12 bytes).
// DISABLED hook (near Jcc in prologue). Implement for standalone exe.
// param is raw memory pointer; if NULL returns NULL, else calls StatGoal_Ctor.
void* StatGoal_Slot10(void* memory);

// Dtor (vtable +0x00, 0x140026540): Destructor.
// Sets vtable to StatGoal, destructs String fields at +0x48 and +0x60,
// then calls Achievement::Goal::~Goal.
void* StatGoal_Dtor(StatGoal* self, uint32_t flags);

// StatGoal_Ctor (0x1400daa80): Constructor. Used by Slot10 and Slot6.
// Calls Achievement::Goal::Goal, sets vtable, initializes all stat fields to defaults.
void* StatGoal_Ctor_Impl(void* memory);
