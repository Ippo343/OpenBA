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




// B4 FORWARD to AnimalComponent::UpdateBehavior: 2 outside callers
// (pasturecomponent_spawn.cpp, tradecomponent_livestock.cpp).
void AnimalComponent_UpdateBehavior(AnimalComponent* self);         // 0x140107320
