// standalone/reg_keystone_a_restree.cpp
// Keystone A — ResidenceComponent residence-record self-balancing BST (AVL) cluster.
// Maps the exe addresses ported in code/Game/Component/keystone_a_restree.cpp to their
// reimplementations. The integrator wires RegisterKeystoneARestreeBatch() into
// RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Closure (bottom-up; every sub-callee registered before/with the caller):
//   0x140015000 / 0x140014ed0  rotation leaves   -> ALREADY registered (reg_player_trade.cpp);
//                                                   NOT re-registered here.
//   0x140014b40                retrace/rebalance -> registered HERE (TradeWork_impl_4b40,
//                                                   defined in tool_base_highlight.cpp; its
//                                                   own closure 15000/14ed0 is registered).
//                                                   Previously only "held" inside
//                                                   reg_player_highlight because that whole
//                                                   batch waited on ace40 (an UNRELATED
//                                                   tessellation sibling, not a 14b40 callee).
//   0x140017460                node alloc        -> 0x140015ec0 + static MemAlloc (slot b60).
//   0x140015ec0                array deep-copy   -> static MemAlloc/MemFree (slots b60/b68).
//   0x1400172f0                AVL insert        -> 0x140017460, 0x140014b40.
//   0x140017230                insert wrapper    -> 0x140015ec0, 0x1400172f0, MemFree (b68).
//
// Every address verified as a real function ENTRY in Ghidra; ports are faithful 1:1
// against the disassembly (offset by offset).
//
// NOTE on the brief's "7460": the brief cited 0x140007460 for the node-alloc, but that is
// a different unrelated function. The REAL node-alloc CALLed by 0x1400172f0 (verified via
// disassemble CALL filter) is 0x140017460 (Ghidra symbol ResidenceComponent_impl_7460).
#include "resolve.h"
#include <cstdint>

// Ports (code/Game/Component/keystone_a_restree.cpp).
extern "C" void*    ResidenceComponent_impl_7230(int64_t* p1, void** p2, uint64_t* p3, int64_t* p4); // 0x140017230
extern "C" int64_t* ResidenceComponent_helper_5ec0(int64_t* p1, int64_t* p2);                        // 0x140015ec0
extern "C" void**   ResidenceComponent_helper_72f0(int64_t* p1, void** p2, uint64_t* p3);            // 0x1400172f0
extern "C" void*    ResidenceComponent_impl_7460(int64_t unused, int64_t* src, int64_t parent);      // 0x140017460

// Shared retrace — 0x140014b40, defined in code/Game/Player/tool_base_highlight.cpp.
extern "C" void TradeWork_impl_4b40(int64_t* p1, int64_t* p2);                                       // 0x140014b40

void RegisterKeystoneARestreeBatch() {
    // Bottom-up: retrace + array-copy + node-alloc, then insert, then the wrapper.
    RegisterExeAddr(0x140014b40, reinterpret_cast<uintptr_t>(&TradeWork_impl_4b40));
    RegisterExeAddr(0x140015ec0, reinterpret_cast<uintptr_t>(&ResidenceComponent_helper_5ec0));
    RegisterExeAddr(0x140017460, reinterpret_cast<uintptr_t>(&ResidenceComponent_impl_7460));
    RegisterExeAddr(0x1400172f0, reinterpret_cast<uintptr_t>(&ResidenceComponent_helper_72f0));
    RegisterExeAddr(0x140017230, reinterpret_cast<uintptr_t>(&ResidenceComponent_impl_7230));
    // NB: 0x140015000 / 0x140014ed0 are intentionally NOT registered here — already done
    // by reg_player_trade.cpp (PlayerTrade_Rot_15000 / _14ed0).
}
