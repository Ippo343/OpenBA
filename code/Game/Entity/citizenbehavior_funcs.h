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

// ProcessPaths (0x1400d6f00) — process pending path distance calculations.
// Processes up to 7 pairs per call from the PathGraph pairs array.
// For each pair, computes path cost (or Manhattan fallback) and stores in BST.
// When all pairs are processed, triggers worker reassignment.
// Returns true if paths were processed, false if nothing to do.
// Ghidra name: FUN_1400d6f00
bool CitizenBehavior_ProcessPaths(PathGraph* pg);

// BuildPathGraph (0x1400d70c0) — build/rebuild the path distance graph.
// Collects grid cell IDs for all buildings, clears old data, then generates
// pairs of (entity_cell, building_cell) for each profession's entities.
// Sets pendingIdx to begin processing on next Update call.
// Ghidra name: Entity_impl_70c0
void CitizenBehavior_BuildPathGraph(PathGraph* pg);

// CountResources (0x140089ed0) — count resources across all buildings per profession.
// Iterates buildings, tallies current/previous resource counts into
// ProfessionResourceEntry[21] array at +0x170.
// Ghidra name: Entity_helper_9ed0
void CitizenBehavior_CountResources(CitizenBehaviorRaw* self);

// CheckThresholds (0x14008c100) — check resource shortage thresholds.
// Fires notification events when resources drop below 5% of max.
// Ghidra name: Entity_helper_c100
void CitizenBehavior_CheckThresholds(CitizenBehaviorRaw* self, int64_t* deltaTime);

// CollectStats (0x14008a730) — collect population statistics.
// Iterates citizens, collects alive/dead/educated/working counts,
// computes happiness/health/educated/working percentages, then
// pushes 8 population stats + 63 profession resource values (21x3)
// to stats history arrays at +0x150. Clears per-tick accumulators.
// Ghidra name: FUN_14008a730
void CitizenBehavior_CollectStats(CitizenBehaviorRaw* self);

// FindIdle (0x14008cbf0) — find idle citizen near building for worker assignment.
// Non-trivial return ABI: RCX=this, RDX=hidden_retval, R8+=args.
// Ghidra name: CitizenComponent_impl_sub_cbf0
void* CitizenBehavior_FindIdle(CitizenBehaviorRaw* self, void* retval, void* profSlot, uint64_t profId);

// FindWorking (0x14008ca20) — find working citizen nearest to building.
// Non-trivial return ABI: RCX=this, RDX=hidden_retval, R8+=args.
// Ghidra name: CitizenComponent_impl_sub_ca20
void* CitizenBehavior_FindWorking(CitizenBehaviorRaw* self, void* retval, void* profSlot, uint64_t profId);

// FindStudent (0x14008c5f0) — find student from school with most students.
// Non-trivial return ABI: RCX=this, RDX=hidden_retval, R8+=args.
// Ghidra name: CitizenComponent_impl_c5f0
void* CitizenBehavior_FindStudent(CitizenBehaviorRaw* self, void* retval, void* profSlot, uint64_t profId);
