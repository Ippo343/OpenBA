#pragma once
// Function declarations for deeply reimplemented ConsumeProduceComponent methods.
// Called from engine/reimpl/consumeproducecomponent.cpp wrappers.
// No dependency on hooks.h — portable to standalone exe.

#include "ConsumeProduceComponent.h"
#include <cstdint>

// ============================================================
// ConsumeProduceComponent — ResourceProduceComponent subclass (0x70 bytes)
// Vtable: 0x140162a80
// Two non-trivial overrides: Shutdown, Serialize
// ============================================================

// Shutdown (vtable slot [3], 0x140118e00): Parent Shutdown + versioned
// serialization of consume fields (resourceId, count, float array, state),
// with version-0 migration for negative resource IDs.
void ConsumeProduceComponent_Shutdown(ConsumeProduceComponent* self, void* stream);

// Serialize (vtable slot [4], 0x140118ef0): Looks up entity's component list,
// finds ConsumeProduceDescription via type check, copies consume slot info
// (count, resourceId), initializes float array to 1.0f per slot.
void ConsumeProduceComponent_Serialize(ConsumeProduceComponent* self);

// Init: resolve all internal function pointers (called once at startup)
void ConsumeProduceComponent_InitFuncs();
