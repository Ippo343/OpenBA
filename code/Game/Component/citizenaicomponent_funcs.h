#pragma once
// Function declarations for deeply reimplemented CitizenAIComponent methods.
// Called from engine/reimpl/citizenaicomponent.cpp wrappers.

#include "CitizenAIComponent.h"
#include <cstdint>

// ============================================================
// CitizenAIComponent — core citizen AI decision loop
// Vtable: 0x140162470
// Size: 0xD0 bytes
// ============================================================

// Update (vtable slot [9], 0x14010f3e0): Main citizen AI tick.
// 8-phase decision loop:
//   1. Decrement 16 cooldown timers at +0x40..+0xBF
//   2. Increment accumulators at +0xC0 and +0xC8
//   3. Find home storage, query resource count — reset +0xC8 if empty
//   4. Check hunger: citizen health vs config threshold
//   5. Check thirst: hydration vs config threshold
//   6. Housing check
//   7-8. Education/age transitions
// Parameters: self, pDeltaTime (pointer to int64 time value)
void CitizenAIComponent_Update(CitizenAIComponent* self, int64_t* pDeltaTime);
