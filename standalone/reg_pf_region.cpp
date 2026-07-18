// standalone/reg_pf_region.cpp
// Wave 3 (Action) / Cluster A — region-graph build/merge/flood-fill family registration.
//
// Registers the closure-safe ports in code/Game/Action/action_pf_region.cpp to their
// Ghidra exe addresses. The integrator wires RegisterPfRegionBatch() into
// RegisterCodeFunctions() in address_registry.cpp.
//
// Safety: every address registered below is a real function ENTRY and a faithful 1:1 port
// whose entire sub-callee closure is itself satisfiable. The family's sub-callees were all
// registered in earlier batches and the only cross-calls are within THIS batch:
//   0x1400021d0 (Reimpl_Array_Resize_Stride04), 0x14001e210 (reg_pf_pools),
//   0x14001df40 (reg_pf_pools), 0x14001d830 (reg_pf_pools), 0x14001ca60 (reg_work_cellbst),
//   0x14001c940 (reg_pf_avl), 0x140132010 (reg_work_impl2).
//
// NOT REGISTERED (closure NOT satisfied):
//   0x1400a8ba0 Pf_RegionPathLength_8ba0 — calls 0x14001dc00 (BuildStructure_helper_dc00),
//     which is NOT registered and itself calls 0x14001dcd0. Registering a8ba0 while dc00 is
//     null would route the region A* helper to a 0-resolve (silent no-op returning garbage
//     into the 32KB stack path buffer) — strictly worse than the skipped-call no-op. The
//     port exists in action_pf_region.cpp; register it once 0x14001dc00 + 0x14001dcd0 land.
#include "resolve.h"
#include <cstdint>

extern void     Pf_RegionNodeFree_8200(int64_t, int);                        // 0x1400a8200
extern int      Pf_RegionFreeId_8350(int64_t);                               // 0x1400a8350
extern void     Pf_DirtyCoarseCellB_8420(int64_t, int);                      // 0x1400a8420
extern void     Pf_RegionRebuildAdjacency_7480(int64_t, int);                // 0x1400a7480
extern void     Pf_StampRegionIdBlock_7670(int64_t, int);                    // 0x1400a7670
extern void     Pf_AddBorderEdge_7750(int64_t, int, int, int, int, int);     // 0x1400a7750
extern uint64_t Pf_RegionCentroidAndEdges_7950(int64_t, int, int);           // 0x1400a7950
extern void     Pf_FloodFillLabel_7ad0(int64_t, int, int, uint32_t, uint32_t, uint16_t); // 0x1400a7ad0
extern int      Pf_SeedFloodFills_7f20(int64_t, int);                        // 0x1400a7f20
extern void     Pf_RebuildRegionChunk_80a0(int64_t, uint32_t);               // 0x1400a80a0
extern void     Pf_FlushDirtyChunks_8a90(int64_t);                           // 0x1400a8a90
extern void     Pf_RegionPairCacheInsert_5b30(uint64_t*, int, int, uint32_t);// 0x1400a5b30
extern int      Pf_RegionPathLength_8ba0(int*, int, uint32_t, uint64_t);     // 0x1400a8ba0
//   (closure now satisfied: 0x14001dc00/0x14001dcd0 registered in reg_pf_astar.cpp)

void RegisterPfRegionBatch() {
    RegisterExeAddr(0x1400a8200, reinterpret_cast<uintptr_t>(&Pf_RegionNodeFree_8200));
    RegisterExeAddr(0x1400a8350, reinterpret_cast<uintptr_t>(&Pf_RegionFreeId_8350));
    RegisterExeAddr(0x1400a8420, reinterpret_cast<uintptr_t>(&Pf_DirtyCoarseCellB_8420));
    RegisterExeAddr(0x1400a7480, reinterpret_cast<uintptr_t>(&Pf_RegionRebuildAdjacency_7480));
    RegisterExeAddr(0x1400a7670, reinterpret_cast<uintptr_t>(&Pf_StampRegionIdBlock_7670));
    RegisterExeAddr(0x1400a7750, reinterpret_cast<uintptr_t>(&Pf_AddBorderEdge_7750));
    RegisterExeAddr(0x1400a7950, reinterpret_cast<uintptr_t>(&Pf_RegionCentroidAndEdges_7950));
    RegisterExeAddr(0x1400a7ad0, reinterpret_cast<uintptr_t>(&Pf_FloodFillLabel_7ad0));
    RegisterExeAddr(0x1400a7f20, reinterpret_cast<uintptr_t>(&Pf_SeedFloodFills_7f20));
    RegisterExeAddr(0x1400a80a0, reinterpret_cast<uintptr_t>(&Pf_RebuildRegionChunk_80a0));
    RegisterExeAddr(0x1400a8a90, reinterpret_cast<uintptr_t>(&Pf_FlushDirtyChunks_8a90));
    RegisterExeAddr(0x1400a5b30, reinterpret_cast<uintptr_t>(&Pf_RegionPairCacheInsert_5b30));
    RegisterExeAddr(0x1400a8ba0, reinterpret_cast<uintptr_t>(&Pf_RegionPathLength_8ba0));  // dep 1dc00 now reg
}
