// standalone/reg_pf_astar.cpp
// Wave 3 (Action) — KEYSTONE batch registration: A* pathfinder core + worker-thread
// command dispatcher/enqueuers + region flood-fill. Maps the exe addresses to the
// faithful 1:1 ports in code/Game/Action/action_pf_astar.cpp. The integrator wires
// RegisterPfAstarBatch() into RegisterCodeFunctions() in address_registry.cpp.
//
// Closure: every exe sub-callee is registered elsewhere —
//   d400/d730/13480/a6030/a5ec0/132010/0021d0/1a9b0/d940 (pf pools/geom/avl/work),
//   plus the four Runtime exports the worker funcs call (resolved via ResolveExport
//   inside the ports, NOT via dispatch slots): CriticalSection::Lock/Unlock,
//   Thread::Start, MemoryAllocator::MemAlloc (static).
#include "resolve.h"
#include <cstdint>
#include "Action/exe_bodies.h"   // Pf_FloodFillRegion_9c170 (0x14009c170), declared from its definition

// Group A — A* search/trace over the grid graph.
extern uint64_t Pf_AstarCore_d040(int*, int, int);                          // 0x14001d040
extern int      Pf_AstarRun_ce00(int*, int, int, void*, int64_t);           // 0x14001ce00
extern void     Pf_AstarRelax_d320(int*, int, uint32_t, uint32_t, uint32_t);// 0x14001d320
extern int64_t* Pf_AvlInsert_d450(int64_t*, int64_t*, uint32_t*);           // 0x14001d450
extern void     Pf_AvlDelete_d550(int64_t*, int64_t*);                      // 0x14001d550

// Group B — A* over the region graph.
extern uint64_t Pf_RegionAstarCore_dcd0(int*, int, uint32_t);               // 0x14001dcd0
extern int      Pf_RegionAstarRun_dc00(int*, int, uint32_t, void*, int64_t);// 0x14001dc00

// Group C — worker-thread command queue.
extern void  Pf_WorkerApplyCmd_a6430(int64_t, uint32_t*, void*, void*);      // 0x1400a6430
extern void  Pf_EnqueueCmd2_a6fe0(int64_t, uint32_t, void*, void*);         // 0x1400a6fe0
extern void  Pf_EnqueueCmd4_a68b0(int64_t, uint8_t, void*, void*);          // 0x1400a68b0
extern void  Pf_EnqueueCmd1_a6a30(int64_t, int, void*, void*);             // 0x1400a6a30
extern void  Pf_EnqueueCmd0_a6b20(int64_t, int, void*, void*);             // 0x1400a6b20
extern int*  Pf_EnqueueTradeRoute_a6c10(int64_t, int*, void*, void*);       // 0x1400a6c10
extern int*  Pf_AllocTradeSlot_a6240(int64_t, int*);                       // 0x1400a6240

void RegisterPfAstarBatch() {
    // Group A
    // Group B
    RegisterExeAddr(0x14001dc00, reinterpret_cast<uintptr_t>(&Pf_RegionAstarRun_dc00));
    // Group C
    RegisterExeAddr(0x1400a6430, reinterpret_cast<uintptr_t>(&Pf_WorkerApplyCmd_a6430));
    RegisterExeAddr(0x1400a6fe0, reinterpret_cast<uintptr_t>(&Pf_EnqueueCmd2_a6fe0));
    RegisterExeAddr(0x1400a68b0, reinterpret_cast<uintptr_t>(&Pf_EnqueueCmd4_a68b0));
    RegisterExeAddr(0x1400a6a30, reinterpret_cast<uintptr_t>(&Pf_EnqueueCmd1_a6a30));
    RegisterExeAddr(0x1400a6b20, reinterpret_cast<uintptr_t>(&Pf_EnqueueCmd0_a6b20));
    RegisterExeAddr(0x1400a6c10, reinterpret_cast<uintptr_t>(&Pf_EnqueueTradeRoute_a6c10));
    RegisterExeAddr(0x14009c170, reinterpret_cast<uintptr_t>(&Pf_FloodFillRegion_9c170));
}
