// standalone/reg_pf_roots_route.cpp
// Wave 3 (Action) / Cluster A — routing/storage-selection ROOT callers registration.
//
// Registers the four faithful 1:1 ports in code/Game/Action/action_pf_roots_route.cpp to
// their Ghidra exe addresses. The integrator wires RegisterPfRootsRouteBatch() into
// RegisterCodeFunctions() in standalone/address_registry.cpp.
//
// Closure: ALL FOUR are closure-satisfied (verified from the disassembly CALL targets):
//   0x14009f810 -> a00f0/9d0a0/a5ac0/a8ba0/a5b30 (all reg)
//   0x14012cd80 -> 12c560 + cf10 (QueryCount) + IsDerivedFrom (engine_funcs)
//   0x14012d5d0 -> 9c480 (ZoneRefresh, NOW reg in reg_pf_roots_road) + 123070/122e20/a00f0/
//                  9f810[this file]/132010/004680/9f3e0 (all reg)
//   0x14008d590 -> 12cd80/9f810/12d5d0 [this file] + 87fb0/4edb0 (reg)
//
// (The "9c480 deferred" comment in reg_work_accessor.cpp is STALE — the pathfinding wave
// landed 0x14009c480 in reg_pf_roots_road.cpp, which closes 12d5d0 then 8d590.)
#include "resolve.h"
#include <cstdint>

extern int      Pf_IsDstReachable_f810(int*, uint32_t, uint32_t, char);                 // 0x14009f810
extern uint32_t Pf_StorageCanAccept_cd80(int64_t, int64_t, int);                         // 0x14012cd80
extern int      Pf_FindBestAdjacentCell_d5d0(int64_t, uint32_t, float*, int64_t);         // 0x14012d5d0
extern int64_t* Pf_PickBestDropTarget_8d590(int64_t**, int64_t*, uint32_t, uint64_t, int, char, uint32_t*, void*); // 0x14008d590

void RegisterPfRootsRouteBatch() {
    RegisterExeAddr(0x14009f810, reinterpret_cast<uintptr_t>(&Pf_IsDstReachable_f810));
    RegisterExeAddr(0x14012cd80, reinterpret_cast<uintptr_t>(&Pf_StorageCanAccept_cd80));
    RegisterExeAddr(0x14012d5d0, reinterpret_cast<uintptr_t>(&Pf_FindBestAdjacentCell_d5d0));
    RegisterExeAddr(0x14008d590, reinterpret_cast<uintptr_t>(&Pf_PickBestDropTarget_8d590));
}
