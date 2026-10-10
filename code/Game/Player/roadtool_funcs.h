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
extern "C" MapDescription** RoadTool_FindMapDescription_6990(const ComponentDescription* entity, MapDescription** out);   // 0x140026990
namespace Object { class External; }
extern "C" void  RoadTool_DecalStore_acd90(WorldGeometry* mgr, int index, Object::External* decal); // 0x1400acd90  // B8: stays free -- not moved: the body returns on a null WorldGeometry (retail does not, row 1124); as a member GCC deletes a `this` test
extern "C" void  RoadTool_PlaceRoadSegment_9c960(Map* mapGrid, uint32_t cellIdx,
                                                 uint8_t roadType, uint64_t endCell,
                                                 uint32_t mapDescVal, ComponentDescription* roadDescExtra);  // B8: stays free -- not moved: the body tests the map for null (retail does not, row 1122); as a member GCC deletes a `this` test
