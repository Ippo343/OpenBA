#pragma once
// Function declarations for deeply reimplemented AnimalComponent methods.
// Called from engine/reimpl/animal.cpp wrappers.
// No dependency on hooks.h -- portable to standalone exe.

#include "AnimalComponent.h"
#include <cstdint>

// ============================================================
// AnimalComponent -- individual animal lifecycle management
// Vtable: 0x140162110
// Size: 0x58 bytes
// ============================================================

// Update (vtable slot [9], 0x140106420): 452 instructions.
// Manages animal aging, death handling, wander timers, behavior update,
// pathfinding with random destination within bounds, terrain validation.
//
// Due to extreme complexity (pathfinding object creation, grid math,
// entity management, multiple sub-function calls), this delegates to
// the original function via ResolveAddr for safety.
// The sub-functions (UpdateBehavior, FindHabitat, SetBounds, SetWanderTimer)
// are documented here for future full reimplementation.
void AnimalComponent_Update(AnimalComponent* self, int64_t* deltaTime);

// Init: resolve all internal function pointers (called once at startup)
void AnimalComponent_InitFuncs();
