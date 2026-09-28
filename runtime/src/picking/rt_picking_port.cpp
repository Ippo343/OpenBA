// ============================================================================
// runtime/src/picking/rt_picking_port.cpp — RtPickingPortInit()  Wave 5 lane D
// ============================================================================
//   RT_PICKING=0     leave the whole Picking namespace on the DLL
//   RT_PICKSCENE=0   ... just Picking::Scene + Picking::PickingMesh
//   RT_PICKGRID=0    ... just the spatial-hash internals (Wave 9 lane
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

extern int RtPickingGridRegister();   // scene_grid.cpp (Wave 9 lane PICK)

void RtPickingPortInit() {
    // B3R RT-Core: RtPickingSceneRegister went with its rows (no reader).
    // [[maybe_unused]]: the perf build strips printf's arguments (B5-11), which
    // leaves `grid` read by nothing there. The registration call stays OUTSIDE
    // the printf for exactly that reason.
    [[maybe_unused]] int grid = RtPickingGridRegister();
    printf("[RtPort] Picking: %d grid addresses registered\n", grid);
}
