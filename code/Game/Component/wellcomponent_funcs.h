#pragma once
// Function declarations for deeply reimplemented WellComponent methods.
// ENDGAME B.3.8 pilot: `WellComponent` is a real class (Component/WellComponent.h).

#include "Component/WellComponent.h"
#include <cstdint>

// Shutdown (vtable slot [3], 0x14005cfa0): Component_Serialize + two empty serializer blocks.
void WellComponent_Shutdown(WellComponent* self, void* stream);

// OnRemove (vtable slot [7], 0x14005cf60): UNregisters the owning entity from the
// TownManager water-source list. (Ghidra calls this OnCreate; the body is a removal.)
void WellComponent_OnRemove(WellComponent* self);

// The matching REGISTER half: state-2 "built" enter (0x14005cf20) and the two
// TownManager leaves it drives.
extern "C" char WellComponent_EnterBuilt(void* comp, void* sm);
extern "C" char WellComponent_EnterDemolish(void* comp, void* sm);
void WellComp_RegisterEntity_91cb0(int64_t townMgr, void* ibp);
void WellComp_AddCellEntry_88a80(int64_t townMgr, int x, int y, int value);

void WellComponent_InitFuncs();
