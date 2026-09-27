// standalone/reg_player_tilerender.cpp
// Player wave (batch "tilerender") — registration for the PlacementTool/RoadTool
// footprint VALIDITY OVERLAY (the green/red tiles under the building ghost).
//
// Registers the exe addresses of the tile-overlay closure (ported 1:1 in
// code/Game/Player/tool_base_tilerender.cpp) so that in standalone mode
// ResolveAddr(Module::Exe, 0xADDR) fires our reimplementation instead of 0.
// This closes the DEFER documented in reg_player_place.cpp for 0x140101380.
//
// Closure (bottom-up):
//   0x1400b10e0  TerrainCorners  (6-corner heightfield sampler)     [here]
//   0x1401008c0  DrawOverlay     (Video::Context tessellate+submit)  [here] -> b10e0
//                                  + f5d0 (reg reg_player_path) + 0f90 (reg reg_pf_road)
//   0x140101380  BuildRecords    (per-cell validate + record build)  [here] -> 08c0 + f5d0
//
// The integrator wires RegisterPlayerTileRenderBatch() into RegisterCodeFunctions().
#include "resolve.h"
#include <cstdint>

extern "C" void PlayerTileRender_TerrainCorners_b10e0(int64_t, int, int, float*);
extern "C" void PlayerTileRender_DrawOverlay_08c0(uint64_t, void*, int64_t*, int64_t*);
extern "C" void PlayerTileRender_BuildRecords_1380(
        uint64_t, void*, int, int, uint32_t, int, uint32_t, int64_t*, int64_t*,
        float, float, float, char, char, int64_t*, int, int, char, int64_t);

void RegisterPlayerTileRenderBatch() {
    RegisterExeAddr(0x1401008c0, reinterpret_cast<uintptr_t>(&PlayerTileRender_DrawOverlay_08c0));
    RegisterExeAddr(0x140101380, reinterpret_cast<uintptr_t>(&PlayerTileRender_BuildRecords_1380));
}
