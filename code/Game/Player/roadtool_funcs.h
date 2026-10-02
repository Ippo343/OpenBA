#pragma once
// Declarations of the exe helpers ported alongside RoadTool::CalcPath
// (roadtool_calcpath.cpp). The RoadTool slot bodies are members (RoadTool.h).

#include "RoadTool.h"
#include <cstdint>

// Helpers ported alongside CalcPath (roadtool_calcpath.cpp), registered so the exe
// addresses resolve for any other caller.
class ComponentDescription;   // Component/ComponentDescription.h
class MapDescription;         // Component/MapDescription.h
class WorldGeometry;          // Game/WorldGeometry.h
class Map;                    // Game/Map.h
// The road A* (0x14009ce00) with the map and buffer typed; defined in roadtool.cpp.
int RoadAStar(Map* map, uint32_t mask, uint32_t sc, uint32_t ec, int* pathBuf,
              uint8_t modifier, uint8_t strict);
extern "C" MapDescription** RoadTool_FindMapDescription_6990(const ComponentDescription* entity, MapDescription** out);   // 0x140026990
extern "C" void  RoadTool_DecalStore_acd90(WorldGeometry* mgr, int index, void* decal); // 0x1400acd90
extern "C" void  RoadTool_PlaceRoadSegment_9c960(Map* mapGrid, uint32_t cellIdx,
                                                 uint8_t roadType, uint64_t endCell,
                                                 uint32_t mapDescVal, uint64_t roadDescExtra);
