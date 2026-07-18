// standalone/reg_player_createbar.cpp
// Player wave (batch "createbar") — CreateBar populate/position registration.
//
// Registers the exe address of operator_impl_fc60 (0x1400ffc60) to its clean
// reimplementation in code/Game/Player/tool_base_createbar.cpp. This is the
// function PlacementTool::Serialize / the Drag tools call every frame to fill +
// position the in-game building-placement "CreateBar" dialog. In standalone the
// exe address is unregistered (ResolveAddr returns 0) so placementtool.cpp's
// s_fn_operator_fc60 stays null and the CreateBar renders empty + stuck top-left;
// this registration wires our reimpl in.
//
// The integrator wires RegisterPlayerCreateBarBatch() into RegisterCodeFunctions()
// in standalone/address_registry.cpp.
//
// Closure (all satisfied — no unregistered exe code in the reimpl's closure):
//   * The fc60 sub-helpers e600 / e420 / e700 / e360 / ebb0 are reimplemented in
//     the SAME translation unit (tool_base_createbar.cpp) and called DIRECTLY as
//     C++ functions — they are NOT resolved by exe address, so they need no
//     RegisterExeAddr entry. (Nothing else in the codebase resolves them by
//     address; fc60 is their only caller.)
//   * CharacterComponent_impl_ec70 (0x1400fec70) and FUN_1400fe960 (0x1400fe960)
//     are already reimplemented + registered (reg_player_screenrect); fc60
//     resolves them via ResolveAddr(Module::Exe, ...).
//   * All remaining callees are Runtime-x64.dll exports (engine_funcs globals +
//     the by-value Math/Volume/String helpers resolved by mangled name inside
//     tool_base_createbar.cpp's EnsureCreateBarExports).
#include "resolve.h"
#include <cstdint>

// operator_impl_fc60 — CreateBar populate + position (main). Signature matches
// placementtool.cpp's fn_operator_fc60_t exactly.
extern "C" void UpdateToolUI_fc60(void* dialogPtr, int W, int H, void* costStringPtr,
                                  char valid, char param_6, char param_7, int param_8,
                                  int startCell, int endCell, void* box, void* transform);

void RegisterPlayerCreateBarBatch() {
    RegisterExeAddr(0x1400ffc60, reinterpret_cast<uintptr_t>(&UpdateToolUI_fc60));
}
