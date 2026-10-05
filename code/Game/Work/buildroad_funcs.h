#pragma once
// BuildRoad — the file-level helpers of code/Game/Work/buildroad.cpp that are not
// members of the class (Work/BuildRoad.h). ENDGAME B.3.8: the slot bodies, the ctor
// and the dtor are members now.

#include <cstdint>

class BuildRoad;
class ComponentEntity;
class ComponentDescription;

// The 8-argument ctor 0x1400e4b50 as a by-address door (RoadTool's commit path):
// `new (self) BuildRoad(...)`.
BuildRoad* BuildRoad_CtorParams_Impl(BuildRoad* self, ComponentEntity* const* ownerIbp, uint32_t cellIdx,
                                     uint32_t mapDescVal, ComponentDescription* roadDescExtra,
                                     uint32_t roadTypeExisting, uint32_t roadState,
                                     uint32_t terrainType);
