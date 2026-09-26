#pragma once
// BuildRoad — the file-level helpers of code/Game/Work/buildroad.cpp that are not
// members of the class (Work/BuildRoad.h). ENDGAME B.3.8: the slot bodies, the ctor
// and the dtor are members now.

#include <cstdint>

class BuildRoad;

// The 8-argument ctor 0x1400e4b50 as a by-address door (RoadTool's commit path):
// `new (self) BuildRoad(...)`.
void* BuildRoad_CtorParams_Impl(BuildRoad* self, uint64_t* ownerIbp, uint32_t cellIdx,
                                uint32_t mapDescVal, uint64_t roadDescExtra,
                                uint32_t roadTypeExisting, uint32_t roadState,
                                uint32_t terrainType);
