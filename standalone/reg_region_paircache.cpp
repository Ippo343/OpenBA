// standalone/reg_region_paircache.cpp
// Registers the region-pair ring-buffer initializer (FUN_14001cca0) so standalone mode
// resolves it to our faithful port. The pair-cache object builder
// (Pf_RegionPairCache_Build) is called directly by the worldgen orchestrator, not via
// ResolveAddr, so it needs no entry here.
//
// The integrator wires RegisterRegionPairCacheBatch() into RegisterCodeFunctions() in
// address_registry.cpp.
#include "resolve.h"
#include <cstdint>

extern void Pf_RegionPairRingResize_cca0(int64_t* param_1);   // 0x14001cca0

void RegisterRegionPairCacheBatch() {
    RegisterExeAddr(0x14001cca0, reinterpret_cast<uintptr_t>(&Pf_RegionPairRingResize_cca0));
}
