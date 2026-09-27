#pragma once
// Function declarations for deeply reimplemented CemeteryComponent methods.
// Called from engine/reimpl/cemeterycomponent.cpp wrappers.

#include "CemeteryComponent.h"
#include <cstdint>




// Controller state-machine enters (retail table 0x1400477a0):
//   state 2 "built"    0x140046fc0 -- register + claim the plot cells
//   state 3 "demolish" 0x140046f80 -- unregister
extern "C" char CemeteryComponent_EnterBuilt(void* comp, void* sm);
extern "C" char CemeteryComponent_EnterDemolish(void* comp, void* sm);

// Init: resolve all internal function pointers (called once at startup)
void CemeteryComponent_InitFuncs();
