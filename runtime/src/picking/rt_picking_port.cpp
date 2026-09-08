// ============================================================================
// runtime/src/picking/rt_picking_port.cpp — RtPickingPortInit()  Wave 5 lane D
// ============================================================================
//   BANISHED_RT_PICKING=0     leave the whole Picking namespace on the DLL
//   BANISHED_RT_PICKSCENE=0   ... just Picking::Scene + Picking::PickingMesh
//   BANISHED_RT_PICKGRID=0    ... just the spatial-hash internals (Wave 9 lane
//                             PICK). Its own switch on purpose: it is the only
//                             part of this namespace that ALLOCATES, and an
//                             integration run that sees a new failure can halve
//                             the search space with one env var. With it off,
//                             scene.cpp's five by-address forwarders go back to
//                             the DLL exactly as they did before the lane.
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"
#include "scene_grid.h"

extern int RtPickingSceneRegister();  // scene.cpp (Wave 5 lane D)
extern int RtPickingGridRegister();   // scene_grid.cpp (Wave 9 lane PICK)

void RtPickingPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_PICKING")) {
        printf("[RtPort] BANISHED_RT_PICKING=0 -- Picking namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_PICKSCENE")) n += RtPickingSceneRegister();

    int grid = 0;
    if (RtWaveEnabled("BANISHED_RT_PICKGRID")) {
        grid = RtPickingGridRegister();
        // ARM the probe rather than running it: this init runs right after
        // ResolveInit()'s LoadLibrary, where the DLL's MemoryAllocator is
        // constructed but NOT started. See scene_grid.h's "Probe scheduling".
        rtpickgrid::g_probePending = true;
    }
    printf("[RtPort] Picking: %d exports + %d grid addresses registered\n", n, grid);
}
