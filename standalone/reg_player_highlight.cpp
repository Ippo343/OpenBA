// standalone/reg_player_highlight.cpp
// Player wave 5 (TIER-2 geometry) — radius-highlight + terrain-decal tessellation.
//
// The HighlightAdd (0x140055430) closure is 8 functions:
//   55430 HighlightAdd, b0d80 slot-manager, and the AVL BST family 1f6b0/1fc00/4b40/
//   1f850/14b00 — all hand-ported in code/Game/Player/tool_base_highlight.cpp (shares
//   rotations 15000/14ed0 with reg_player_trade, and fa80/1f8d0/ee40/acc10/ae180 with
//   the pf waves) — PLUS the large tile tessellation ace40 (0x1400ace40), now integrated
//   in code/Game/Player/tool_base_highlight_tess.cpp.
//
// UNBLOCKED (was HELD on ace40 verification): ace40 is integrated with its Transform2/
// Vector2 dispatch slots (0x140152640/648/650/658, 0x140151e58) registered in
// address_registry.cpp RegisterDispatchSlots, and its .rdata float constants embedded
// as retail values (SA-safe). This batch is required by BANISHED_LIVE_DECAL
// (DecalComponent::Draw -> b0d80 -> ace40 paints the building ground decal, e.g. the
// StorageYard brown foundation) and enables the PlacementTool radius overlay.
//
// Registered bottom-up: 14b00, 1f850, 4b40, 1fc00, 1f6b0, ace40, b0d80, 55430.

#include "resolve.h"
#include <cstdint>

// tool_base_highlight.cpp (AVL BST family + slot-manager + HighlightAdd)
extern "C" void    TradeWork_helper_14b00(int64_t ctx, void* node);
extern "C" void    TradeWork_impl_4b40(int64_t* root, int64_t* startNode);
extern "C" int Player_HighlightAdd_55430(int64_t self, int centerCell);
// tool_base_highlight_tess.cpp (tile tessellation)
extern "C" void TradeWork_impl_ace40(int64_t param_1, int param_2, char param_3,
                                     int param_4, int param_5);

void RegisterPlayerHighlightBatch() {
    RegisterExeAddr(0x140014b00, reinterpret_cast<uintptr_t>(&TradeWork_helper_14b00));
    RegisterExeAddr(0x140014b40, reinterpret_cast<uintptr_t>(&TradeWork_impl_4b40));
    RegisterExeAddr(0x1400ace40, reinterpret_cast<uintptr_t>(&TradeWork_impl_ace40));
    RegisterExeAddr(0x140055430, reinterpret_cast<uintptr_t>(&Player_HighlightAdd_55430));
}
