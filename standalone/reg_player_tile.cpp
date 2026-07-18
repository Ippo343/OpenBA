// standalone/reg_player_tile.cpp
// Player wave (round 2) — "tile" sub-batch registration.
//
// Registers the exe addresses of shared PlacementTool tile/footprint helpers
// (resolved from code/Game/Player/*.cpp call sites) to their reimplementations in
// code/Game/Player/tool_base_tile.cpp. The integrator wires RegisterPlayerTileBatch()
// into RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and ported 1:1 (disasm-verified).
// The ENTIRE exe-code closure of each is registered (003870 + d580 here; fd650's
// other callees 1018d0/c3720/003f60/004680/c48a0/c2b70/fd960/9f5d0 were all
// registered in prior waves). Runtime deps resolve at runtime (engine_funcs fn_*).
#include "resolve.h"
#include <cstdint>

// 0x140003870 — SetTransformFromYaw
extern void ToolTile_SetTransformFromYaw_3870(uintptr_t self, const void* offset, float yaw);
// 0x14009d580 — footprint tile-validity over a rotated rectangle
extern unsigned long long ToolTile_FootprintValidity_d580(
        int* param_1, int param_2, int param_3, int param_4, int param_5,
        unsigned int param_6, long long* param_7, long long* param_8,
        float param_9, float param_10, float param_11, char param_12,
        long long* param_13, int param_14, long long param_15);
// 0x1400fd650 — placement validate + create placed entity
extern unsigned long long ToolTile_ValidateCreate_fd650(
        long long param_1, int param_2, int param_3, int param_4, int param_5,
        const void* param_6, float param_7);

void RegisterPlayerTileBatch() {
    // bottom-up: leaves (003870, d580) before the composite (fd650)
    RegisterExeAddr(0x140003870, reinterpret_cast<uintptr_t>(&ToolTile_SetTransformFromYaw_3870));
    RegisterExeAddr(0x14009d580, reinterpret_cast<uintptr_t>(&ToolTile_FootprintValidity_d580));
    RegisterExeAddr(0x1400fd650, reinterpret_cast<uintptr_t>(&ToolTile_ValidateCreate_fd650));
}
