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

// helper_7400 (0x140107400): place one spawned animal. `animalComp` is the
// animal's AnimalComponent (type 0x1401c6f98), `pos` a Math::Vector3.
void HerdComponent_Helper7400(int64_t animalComp, void* pos, float yaw, char pose);

// Populate (0x140120f50, Ghidra: PopulationComponent_helper_0f50): seed a fresh
// herd with `count` animals and arm its graze timer.
void HerdComponent_Populate(int64_t herdComp, int count);

// OnAdd (vtable slot 6, 0x140121f40) / OnRemove (slot 7, 0x140121e20).
void HerdComponent_OnAdd(void* self);
void HerdComponent_OnRemove(void* self);

// Startup (vtable slot 4): the ctor tail the inert factory can't express.
void HerdComponent_Startup(void* self);

// Init: resolve all internal function pointers (called once at startup)
void HerdComponent_InitFuncs();
