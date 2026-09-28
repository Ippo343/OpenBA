#pragma once
// Function declarations for deeply reimplemented WellComponent methods.
// ENDGAME B.3.8 pilot: `WellComponent` is a real class (Component/WellComponent.h).

#include "Component/WellComponent.h"
#include <cstdint>

// The matching REGISTER half: state-2 "built" enter (0x14005cf20) and the two
// TownManager leaves it drives.
extern "C" char WellComponent_EnterBuilt(void* comp, void* sm);
extern "C" char WellComponent_EnterDemolish(void* comp, void* sm);
void WellComp_RegisterEntity_91cb0(int64_t townMgr, void* ibp);
void WellComp_AddCellEntry_88a80(int64_t townMgr, int x, int y, int value);

