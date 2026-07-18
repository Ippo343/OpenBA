// standalone/reg_pf_avl.cpp
// Pathfinding/Action closure — AVL/BST node-pool + rotation family registration.
//
// Registers the 10 faithfully-ported exe helpers in code/Game/Action/action_pf_avl.cpp.
// The integrator wires RegisterPfAvlBatch() into RegisterCodeFunctions() in
// address_registry.cpp.
//
// Closure: this batch is self-closing for the rotation dependency chain — the rebalance
// drivers (13480 / a8c0) and inserts (c940 / a4f0) call the rotations (e450/e580 and
// 14c70/14da0), the allocators (cc40/a680), and each other ONLY via ResolveAddr, and ALL
// of those addresses are registered right here. The only external dep is fn_MemAlloc /
// g_pMemAllocSingleton (engine_funcs.h, already satisfied). So registering this whole batch
// at once leaves no half-null edge.
#include "resolve.h"
#include <cstdint>

// Layout B rotations (0x30-node).
extern "C" int64_t   Pf_AvlRotateB_e450(int64_t*, int64_t);        // 0x14000e450
extern "C" int64_t   Pf_AvlRotateB_e580(int64_t*, int64_t);        // 0x14000e580
// Layout A rotations (0x28-node).
extern "C" int64_t   Pf_AvlRotateL_14c70(int64_t*, int64_t);       // 0x140014c70
extern "C" int64_t   Pf_AvlRotateR_14da0(int64_t*, int64_t);       // 0x140014da0
// Rebalance drivers.
extern "C" void      Pf_AvlRebalanceB_13480(int64_t*, int64_t*);   // 0x140013480
extern "C" void      Pf_AvlRebalanceA_a8c0(int64_t*, int64_t*);    // 0x14001a8c0
// Node allocators.
extern "C" uint32_t* Pf_AvlNewNodeA_a680(uint64_t, uint32_t*, uint64_t);  // 0x14001a680
extern "C" uint32_t* Pf_AvlNewNodeB_cc40(uint64_t, uint32_t*, uint64_t);  // 0x14001cc40
// Inserts.
extern "C" uint64_t* Pf_BstInsertB_c940(int64_t*, uint64_t*, uint32_t*);  // 0x14001c940
extern "C" uint64_t* Pf_AvlInsertA_a4f0(int64_t*, uint64_t*, uint32_t*);  // 0x14001a4f0

void RegisterPfAvlBatch() {
    RegisterExeAddr(0x14000e450, reinterpret_cast<uintptr_t>(&Pf_AvlRotateB_e450));
    RegisterExeAddr(0x14000e580, reinterpret_cast<uintptr_t>(&Pf_AvlRotateB_e580));
    RegisterExeAddr(0x140014c70, reinterpret_cast<uintptr_t>(&Pf_AvlRotateL_14c70));
    RegisterExeAddr(0x140014da0, reinterpret_cast<uintptr_t>(&Pf_AvlRotateR_14da0));
    RegisterExeAddr(0x140013480, reinterpret_cast<uintptr_t>(&Pf_AvlRebalanceB_13480));
    RegisterExeAddr(0x14001a8c0, reinterpret_cast<uintptr_t>(&Pf_AvlRebalanceA_a8c0));
    RegisterExeAddr(0x14001a680, reinterpret_cast<uintptr_t>(&Pf_AvlNewNodeA_a680));
    RegisterExeAddr(0x14001cc40, reinterpret_cast<uintptr_t>(&Pf_AvlNewNodeB_cc40));
    RegisterExeAddr(0x14001c940, reinterpret_cast<uintptr_t>(&Pf_BstInsertB_c940));
    RegisterExeAddr(0x14001a4f0, reinterpret_cast<uintptr_t>(&Pf_AvlInsertA_a4f0));
}
