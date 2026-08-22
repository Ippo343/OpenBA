// standalone/reg_keystone_a_zone.cpp
// Keystone A (zone + road enqueue cluster) registration. Maps the exe addresses to
// the faithful 1:1 ports in code/Game/Action/keystone_a_zone.cpp. The integrator
// wires RegisterKeystoneAZoneBatch() into RegisterCodeFunctions() in
// address_registry.cpp.
//
// Closure: every exe sub-callee is registered elsewhere —
//   0x1400a6a30 / 0x1400a68b0 / 0x1400a6b20 / 0x1400a6c10 (reg_pf_astar.cpp),
//   0x1400a6d50 (reg_action_move.cpp), 0x1400a6430 (reg_pf_astar.cpp),
//   plus fn_InternalBasePtr_Ctor (engine_funcs) and the Runtime exports
//   (CriticalSection::Lock/Unlock, Thread::Start, MemoryAllocator::MemAlloc,
//   Application::YieldToThreads) resolved via ResolveExport inside the ports.
#include "resolve.h"
#include <cstdint>

extern int  RoadTool_helper_6e30(int64_t, unsigned int, uint64_t, void*, int64_t, int); // 0x1400a6e30
extern void Pf_EnqueueCmd3_a6f00(int64_t, uint32_t, uint64_t, void*);                    // 0x1400a6f00
extern void ZoneComponent_sub_d360(int*, int64_t*, int, int, unsigned int, int, int64_t*, int); // 0x14009d360
extern void RoadTool_sub_ce00(int64_t, unsigned int, unsigned int, uint64_t, void*, int64_t, uint8_t, uint8_t); // 0x14009ce00

void RegisterKeystoneAZoneBatch() {
    RegisterExeAddr(0x1400a6e30, reinterpret_cast<uintptr_t>(&RoadTool_helper_6e30));
    RegisterExeAddr(0x1400a6f00, reinterpret_cast<uintptr_t>(&Pf_EnqueueCmd3_a6f00));
    RegisterExeAddr(0x14009d360, reinterpret_cast<uintptr_t>(&ZoneComponent_sub_d360));
    RegisterExeAddr(0x14009ce00, reinterpret_cast<uintptr_t>(&RoadTool_sub_ce00));
}
