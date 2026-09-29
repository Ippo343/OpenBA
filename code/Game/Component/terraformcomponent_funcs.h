#pragma once
// Function declarations for the reimplemented TerraformComponent methods.

#include "TerraformComponent.h"
#include <cstdint>

class WorldGeometry;   // Game/WorldGeometry.h

// 0x1400ab150, the terrain dirty-chunk mark around (x, y). A second port of
// Pf_RegionDirty4_b150 (Action/exe_bodies.h) with the tree's zero-chunk guard;
// QuarryComponent's carve calls this one.
void TerraformComponent_RegionRecalc(WorldGeometry* terrain, int x, int y);   // 0x1400ab150

