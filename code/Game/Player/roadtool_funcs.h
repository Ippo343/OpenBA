#pragma once
// Declarations of the exe helpers ported alongside RoadTool::CalcPath
// (roadtool_calcpath.cpp). The RoadTool slot bodies are members (RoadTool.h).

#include "RoadTool.h"
#include <cstdint>

// Helpers ported alongside CalcPath (roadtool_calcpath.cpp), registered so the exe
// addresses resolve for any other caller.
extern "C" void** RoadTool_FindMapDescription_6990(void* entity, void** out);   // 0x140026990
extern "C" void  RoadTool_DecalStore_acd90(int64_t mgr, int index, void* decal); // 0x1400acd90
extern "C" void  RoadTool_PlaceRoadSegment_9c960(uint32_t* mapGrid, uint32_t cellIdx,
                                                 uint8_t roadType, uint64_t endCell,
                                                 uint32_t mapDescVal, uint64_t roadDescExtra);
