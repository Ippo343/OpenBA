#pragma once
// Function declarations for deeply reimplemented WellComponent methods.
// ENDGAME B.3.8 pilot: `WellComponent` is a real class (Component/WellComponent.h).

#include "Component/WellComponent.h"
#include <cstdint>

// The matching REGISTER half: state-2 "built" enter (0x14005cf20) and the two
// TownManager leaves it drives.
// B4 FORWARD -> CitizenBehavior::AddCellEntry (Game/citizenlist_serialize.cpp)
void WellComp_AddCellEntry_88a80(int64_t townMgr, int x, int y, int value);   // 0x140088a80

