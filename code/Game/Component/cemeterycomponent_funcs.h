#pragma once
// Function declarations for deeply reimplemented CemeteryComponent methods.
// Called from engine/reimpl/cemeterycomponent.cpp wrappers.

#include "CemeteryComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x1400475C0): Serializes base + controller stub +
// variable-length array of cemetery plot tile indices.
void CemeteryComponent_Shutdown(CemeteryComponent* self, void* stream);

// BuildPlotGrid (vtable slot [6], 0x140047200): Ghidra calls the slot Draw; it draws
// nothing. It fills self->plots with the grave-plot cell indices, derived from the
// footprint, the rotation, and the entrance child entity.
void CemeteryComponent_BuildPlotGrid(CemeteryComponent* self);

// OnRemove (vtable slot [7], 0x1400471C0): UNregisters the component from the
// TownManager cemetery list at *(GameWorld+0xF0)+0x40. Ghidra labels the slot
// OnCreate; the body is an erase. The matching REGISTER is the state-2 enter.
void CemeteryComponent_OnRemove(CemeteryComponent* self);

// Controller state-machine enters (retail table 0x1400477a0):
//   state 2 "built"    0x140046fc0 -- register + claim the plot cells
//   state 3 "demolish" 0x140046f80 -- unregister
extern "C" char CemeteryComponent_EnterBuilt(void* comp, void* sm);
extern "C" char CemeteryComponent_EnterDemolish(void* comp, void* sm);

// Init: resolve all internal function pointers (called once at startup)
void CemeteryComponent_InitFuncs();
