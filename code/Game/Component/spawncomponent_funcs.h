#pragma once
// Function declarations for deeply reimplemented SpawnComponent methods.

#include "SpawnComponent.h"
#include <cstdint>

// Initial sub-resource seed (ComponentEntity_impl_5cc0, 0x140055cc0): called once
// per scattered natural resource by the worldgen scatter so a new map starts with
// wild food already under the trees. See the definition for why this is the only
// producer of the Gatherer's Hut's accepted resources.
void SpawnComponent_InitialSpawn(SpawnComponent* self);

// Print the DoSpawn attempt/outcome tallies under a caller-supplied tag.
extern "C" void SpawnComponent_ReportDoSpawnCensus(const char* tag);

