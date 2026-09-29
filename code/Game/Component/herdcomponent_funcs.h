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

// B4 FORWARD to AnimalComponent::Place (0x140107400): 2 outside callers
// (pasturecomponent_spawn.cpp, tradecomponent_livestock.cpp). `animalComp` is the
// animal's AnimalComponent, `pos` a Math::Vector3.
void HerdComponent_Helper7400(int64_t animalComp, void* pos, float yaw, char pose);

// B4 FORWARD to HerdComponent::Populate (0x140120f50): 1 outside caller
// (populationcomponent.cpp).
void HerdComponent_Populate(int64_t herdComp, int count);

// 0x140121e20, HerdComponent's slot 7 (not installed: HerdComponent::OnRemove is empty).
void HerdComponent_OnRemove(HerdComponent* self);
