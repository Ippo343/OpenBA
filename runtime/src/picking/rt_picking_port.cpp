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

#include "../../funcs/rt_port.h"

// B3R RT-Core: Scene's and the spatial grid's rows lost their last readers, so
// both register functions went with them. This entry point registers nothing;
// it stays defined only because standalone/resolve_standalone.cpp's
// RuntimePortInit() still calls it (the coordinator removes that call).
void RtPickingPortInit() {
}
