// standalone/reg_player_roadcalcpath.cpp
// Registration for the RoadTool COMMIT batch (code/Game/Player/roadtool_calcpath.cpp).
//
// Maps each ported exe helper's Ghidra address to our reimplementation so that
// ResolveAddr(Module::Exe, 0xADDR) returns our function instead of 0 in standalone.
//
// RoadTool::CalcPath itself (0x140104d30) is reached through the RoadTool vtable
// slot 13 (roadtool.cpp), so it does not need an address entry — but the three
// helpers do, since other exe code paths call them by address.

#include "resolve.h"
#include <cstdint>

extern "C" void** RoadTool_FindMapDescription_6990(void* entity, void** out);
extern "C" void  RoadTool_DecalStore_acd90(int64_t mgr, int index, void* decal);
extern "C" void  RoadTool_PlaceRoadSegment_9c960(uint32_t* mapGrid, uint32_t cellIdx,
                                                 uint8_t roadType, uint64_t endCell,
                                                 uint32_t mapDescVal, uint64_t roadDescExtra);

void RegisterPlayerRoadCalcPathBatch() {
    RegisterExeAddr(0x140026990, reinterpret_cast<uintptr_t>(&RoadTool_FindMapDescription_6990));
    RegisterExeAddr(0x1400acd90, reinterpret_cast<uintptr_t>(&RoadTool_DecalStore_acd90));
    RegisterExeAddr(0x14009c960, reinterpret_cast<uintptr_t>(&RoadTool_PlaceRoadSegment_9c960));
}
