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
namespace Object { class External; }
