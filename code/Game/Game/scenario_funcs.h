#pragma once
// Function declarations for deeply reimplemented Scenario methods.
// Called from engine/reimpl/scenario.cpp wrappers.

#include "Scenario.h"
#include <cstdint>

// Dtor (vtable +0x00): Destroys Scenario-specific strings, calls Achievement base dtor,
// conditionally frees memory.
void* Scenario_Dtor(Scenario* self, uint32_t flags);

// Slot3 (vtable +0x18): Returns Achievement base file version OR'd with 0x600.
uint32_t Scenario_Slot3(Scenario* self);

// Slot5 (vtable +0x28): Serializes Achievement base + Scenario fields to/from stream.
void Scenario_Serialize(Scenario* self, void* stream);

// Slot6 (vtable +0x30): Cleanup -- destroys then re-constructs in place.
void Scenario_Cleanup(Scenario* self);

// Slot8 (vtable +0x40): Init -- constructs Achievement base, sets vtable and default fields.
// Also used by Factory (0x14001e9f0) for fresh allocation.
void Scenario_Init(Scenario* self);
