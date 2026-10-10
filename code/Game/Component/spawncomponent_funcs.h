#pragma once
// Function declarations for deeply reimplemented SpawnComponent methods.

#include "SpawnComponent.h"
#include <cstdint>


// Print the DoSpawn attempt/outcome tallies under a caller-supplied tag.
extern "C" void SpawnComponent_ReportDoSpawnCensus(const char* tag);  // B8: harness / port-only -- diagnostic print of the DoSpawn tallies, no retail counterpart

