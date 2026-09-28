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




// Private helpers of Update (also registered so exe callers reach them).
int  AnimalComponent_FindHabitat(AnimalComponent* self);            // 0x140106020
void AnimalComponent_UpdateBehavior(AnimalComponent* self);         // 0x140107320
void AnimalComponent_SetWanderTimer(AnimalComponent* self,
                                    uint32_t animId, float seconds); // 0x140106250

