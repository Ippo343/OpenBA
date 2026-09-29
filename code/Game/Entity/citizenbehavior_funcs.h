#pragma once
// Function declarations for deeply reimplemented CitizenBehavior methods.
// Called from engine/reimpl/ wrappers (when created) or future standalone exe.
//
// CitizenBehavior is the population manager (GameUpdateLoop+0xF0).
// Profile source path: Entity\CitizenBehavior.cpp
// Size: 0x1C0 bytes, no vtable.

#include "engine/types/CitizenBehavior.h"   // the forwarder: also carries CitizenBehaviorRaw
#include <cstdint>

// ============================================================
// CitizenBehavior methods
// ============================================================

// Update (0x14008c2b0) — main per-frame tick.
// Advances timers, calls CountResources, CheckThresholds, and
// periodically calls CollectStats and ProcessPaths.
// Ghidra name: Entity_impl_c2b0
void CitizenBehavior_Update(CitizenBehaviorRaw* self, int64_t* deltaTime);

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
