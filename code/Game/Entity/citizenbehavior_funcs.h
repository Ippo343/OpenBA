#pragma once
// Function declarations for deeply reimplemented CitizenBehavior methods.
// Called from engine/reimpl/ wrappers (when created) or future standalone exe.
// No dependency on hooks.h — portable to standalone exe.
//
// CitizenBehavior is the population manager (GameUpdateLoop+0xF0).
// Profile source path: Entity\CitizenBehavior.cpp
// Size: 0x1C0 bytes, no vtable.

#include "CitizenBehavior.h"
#include <cstdint>

// ============================================================
// CitizenBehavior methods
// ============================================================

// Constructor (found in GameUpdateLoop::Load, MemAlloc 0x1C0).
// Zeroes all 0x1C0 bytes, then initializes profession resource array
// (21 entries with default max counts), stats sub-arrays, and
// allocates the 0x28-byte PathGraph sub-object.
// Ghidra name: TradeComponent_impl_0a30
CitizenBehaviorRaw* CitizenBehavior_Ctor(CitizenBehaviorRaw* self);

// Update (0x14008c2b0) — main per-frame tick.
// Advances timers, calls CountResources, CheckThresholds, and
// periodically calls CollectStats and ProcessPaths.
// Ghidra name: Entity_impl_c2b0
void CitizenBehavior_Update(CitizenBehaviorRaw* self, int64_t* deltaTime);

// ShuffleSchools (0x1400896a0) — Fisher-Yates shuffle on the schools array.
// Uses the global Random to randomize assignment order.
// Ghidra name: FUN_1400896a0
void CitizenBehavior_ShuffleSchools(CitizenBehaviorRaw* self);

// ShuffleEntities (0x140089470) — shuffle entity list for a profession BST node.
// Looks up profession in BST at +0x100, shuffles the entity array.
// Ghidra name: FUN_140089470
void CitizenBehavior_ShuffleEntities(CitizenBehaviorRaw* self, uint64_t profId);

// FindAny (0x14008c4b0) — find any available citizen.
// Iterates allCitizensPtr, checks age >= adult, not dead, profession matches.
// Assigns housing if found. Returns InternalBasePtr to citizen.
// Ghidra name: CitizenComponent_impl_c4b0
void* CitizenBehavior_FindAny(CitizenBehaviorRaw* self, void* retval, void* profSlot, uint64_t profId);
