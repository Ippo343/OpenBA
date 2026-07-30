#pragma once
// Function declarations for the reimplemented AnimalComponent methods.
// Implementations live in code/Game/Component/animalcomponent.cpp; the exe
// addresses are registered by standalone/reg_animal_update.cpp.

#include "AnimalComponent.h"
#include <cstdint>

// ============================================================
// AnimalComponent -- individual animal lifecycle management
// Vtable: 0x140162110   Size: 0x58 bytes
// ============================================================

// Update (vtable slot [9], 0x140106420): aging + death, the graze latch, the
// age-driven visual scale, and the wander/MoveTo pathing loop.
void AnimalComponent_Update(AnimalComponent* self, int64_t* deltaTime);

// Startup (vtable slot [4], 0x140106d50): rolls the age of death from
// AnimalDescription (+0x28 base, +0x2c variance). Must run before Update, or
// the animal's death age stays 0 and it dies on its first tick.
void AnimalComponent_Startup(AnimalComponent* self);

// OnAdded (vtable slot [6], 0x140106c20): latches the spawn cell, sizes the
// wander box, then runs one Update with a zero delta. No-op unless flag bit 22.
void AnimalComponent_OnAdd(AnimalComponent* self);

// Private helpers of Update (also registered so exe callers reach them).
int  AnimalComponent_FindHabitat(AnimalComponent* self);            // 0x140106020
void AnimalComponent_UpdateBehavior(AnimalComponent* self);         // 0x140107320
void AnimalComponent_SetWanderTimer(AnimalComponent* self,
                                    uint32_t animId, float seconds); // 0x140106250

// Init: resolve all internal function pointers (called once at startup)
void AnimalComponent_InitFuncs();
