// standalone/reg_player_trade.cpp
// Player wave round 2c (R3-trade) — EditTool trade/track radius registration.
//
// Registers the exe addresses of the EditTool trade-track helper chain (resolved
// from code/Game/Player/edittool.cpp, and reused by tradework.cpp /
// nomadcomponent.cpp / entityui.cpp) to their reimplementations in
// code/Game/Player/tool_base_trade.cpp. The integrator wires
// RegisterPlayerTradeBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp.
//
// Closure is fully self-contained — every sub-callee is registered here (bottom-up),
// and the only external deps are Runtime exports resolved at runtime (no
// registration needed): InternalBasePtr CheckPtr/Ctor, IsDerivedFrom,
// DialogInstance::GetController, and static MemoryAllocator MemAlloc/MemFree.
//
// Safety: every address below was verified as a real function ENTRY in Ghidra
// (get_function_by_address Entry == address) and is a faithful 1:1 port verified
// against the disassembly (AVL balance/rotation direction, node layout, ABI).
//
// NOTE on brief addresses: the brief cited "TradeWork_impl_2e30 (0x140002e30)" and
// "TradeWork_impl_c690 (0x1400c690)", but those are dropped-digit typos. The TRUE
// call targets in 0x1400666b0 (verified via disassemble CALL filter) are
// 0x140102e30 and 0x14002c690 (the Ghidra symbol names "TradeWork_impl_2e30/c690"
// map to those full addresses). 0x140002e30 is mid-function (entry 0x140002e10).
#include "resolve.h"
#include <cstdint>

// AVL rotation leaves (tool_base_trade.cpp)
extern "C" int64_t PlayerTrade_Rot_15000(int64_t*, int64_t);   // 0x140015000 pivot=left
extern "C" int64_t PlayerTrade_Rot_14ed0(int64_t*, int64_t);   // 0x140014ed0 pivot=right
// AVL retrace/rebalance
extern "C" void PlayerTrade_Retrace_25920(int64_t*, int64_t*); // 0x140025920
// AVL node remove
extern "C" void PlayerTrade_BSTRemove_25740(int64_t*, void*);  // 0x140025740
// AVL find+remove by key
extern "C" void PlayerTrade_BSTFindRemove_2c690(int64_t*, void*); // 0x14002c690
// AVL insert/update by key
extern "C" void PlayerTrade_BSTInsert_102e30(int64_t, void*, uint32_t, uint32_t); // 0x140102e30
// EntityUI helper (controller flag + add/remove in world tracking BST)
extern "C" void PlayerTrade_EntityUI_66b0(int64_t);            // 0x1400666b0
// EditTool entry
extern "C" void PlayerTrade_EditTool_66800(int64_t);           // 0x140066800

void RegisterPlayerTradeBatch() {
    // Bottom-up: leaves first, then the structures that call them.
    RegisterExeAddr(0x140015000, reinterpret_cast<uintptr_t>(&PlayerTrade_Rot_15000));
    RegisterExeAddr(0x140014ed0, reinterpret_cast<uintptr_t>(&PlayerTrade_Rot_14ed0));
    RegisterExeAddr(0x140025920, reinterpret_cast<uintptr_t>(&PlayerTrade_Retrace_25920));
    RegisterExeAddr(0x140025740, reinterpret_cast<uintptr_t>(&PlayerTrade_BSTRemove_25740));
    RegisterExeAddr(0x14002c690, reinterpret_cast<uintptr_t>(&PlayerTrade_BSTFindRemove_2c690));
    RegisterExeAddr(0x140102e30, reinterpret_cast<uintptr_t>(&PlayerTrade_BSTInsert_102e30));
    RegisterExeAddr(0x1400666b0, reinterpret_cast<uintptr_t>(&PlayerTrade_EntityUI_66b0));
    RegisterExeAddr(0x140066800, reinterpret_cast<uintptr_t>(&PlayerTrade_EditTool_66800));
}
