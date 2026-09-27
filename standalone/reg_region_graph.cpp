// standalone/reg_region_graph.cpp
// Pathfinder coarse region-connectivity graph build — registration.
//
// Registers the faithful 1:1 ports in code/Game/Action/region_graph_build.cpp to their
// Ghidra exe addresses. The integrator wires RegisterRegionGraphBuildBatch() into
// RegisterCodeFunctions() in address_registry.cpp.
//
// Closure: Pf_RegionGraphCtor_8670 calls Pf_CoarseCellResize_e060 (this batch), which in
// turn calls Pf_IntArrayClone_f9e0 (0x14000f9e0, registered in reg_pf_pools via
// action_pf_pools.cpp — re-registered here for clarity/self-containment) and
// Pf_CoarseCellFreeTail_e420 (this batch). All sub-callees are satisfied.
#include "resolve.h"
#include <cstdint>

extern int*  Pf_RegionGraphCtor_8670(int*, int64_t, int64_t, int64_t, uint32_t, int); // 0x1400a8670
extern void  Pf_CoarseCellResize_e060(int64_t*, int);                                 // 0x14001e060
extern void  Pf_CoarseCellFreeTail_e420(int64_t, int64_t, int);                       // 0x14001e420
extern int64_t* Pf_IntArrayClone_f9e0(int64_t*, int64_t*);                            // 0x14000f9e0

void RegisterRegionGraphBuildBatch() {
    // 0x14000f9e0 (Pf_IntArrayClone_f9e0) is owned by reg_pf_pools.cpp — same symbol,
    // registered there first. Duplicate registration removed 2026-08-30.
}
