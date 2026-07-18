#pragma once
// Function declarations for deeply reimplemented HerdComponent methods.
// Called from engine/reimpl/herdcomponent.cpp wrappers.

#include "HerdComponent.h"
#include <cstdint>

// ============================================================
// HerdComponent -- wild animal herd management
// Vtable: 0x140163680
// Size: 0x60 bytes
// ============================================================

// Update (vtable slot [9], 0x140121bf0): Manages herd grazing/migration.
// Two phases:
//   Phase 0 (grazing): countdown timer at +0x50, check terrain validity,
//     transition to migration when timer expires or invalid terrain.
//   Phase 1 (migrating): check migration done, transition back to grazing
//     with randomized timer.
// On each timer tick (interval from config+0x3C): checks food availability,
// computes target herd size, calls AdjustSize.
void HerdComponent_Update(HerdComponent* self, int64_t* deltaTime);

// Init: resolve all internal function pointers (called once at startup)
void HerdComponent_InitFuncs();
