// ============================================================================
// runtime/src/picking/rt_picking_port.cpp — RtPickingPortInit()  Wave 5 lane D
// ============================================================================
//   BANISHED_RT_PICKING=0    leave the whole Picking namespace on the DLL
//   BANISHED_RT_PICKSCENE=0  ... just Picking::Scene + Picking::PickingMesh
// ============================================================================

#include <cstdio>
#include "../../funcs/rt_port.h"

extern int RtPickingSceneRegister();  // scene.cpp (lane D)

void RtPickingPortInit() {
    if (!RtWaveEnabled("BANISHED_RT_PICKING")) {
        printf("[RtPort] BANISHED_RT_PICKING=0 -- Picking namespace port disabled\n");
        return;
    }
    int n = 0;
    if (RtWaveEnabled("BANISHED_RT_PICKSCENE")) n += RtPickingSceneRegister();
    printf("[RtPort] Picking: %d exports registered\n", n);
}
