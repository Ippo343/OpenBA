// standalone/reg_pf_geom.cpp
// PF porter wave — registration of the nine geometry / heuristic / spatial leaf
// sub-helpers ported in code/Game/Action/action_pf_geom.cpp.
//
// The integrator wires RegisterPfGeomBatch() into RegisterCodeFunctions() in
// standalone/address_registry.cpp. Each address is a real function ENTRY and a
// faithful 1:1 port whose sub-callee closure is satisfied (only Math::Vector3
// exports, System::MemoryAllocator/Random exports, and pure index/bit math).
#include "resolve.h"
#include <cstdint>

extern uint32_t Pf_RegionPairLookup_5ac0(uintptr_t, int, int);          // 0x1400a5ac0
extern int      Pf_NeighbourTile_5ec0(uintptr_t, int, uint32_t);        // 0x1400a5ec0
extern int      Pf_MoveHeuristic_6030(uintptr_t, int, int);             // 0x1400a6030
extern int      Pf_RayTriIntersect_aa810(void*, const float*, const float*,
                                          const float*, const float*, const float*,
                                          const float*, float*, float*);   // 0x1400aa810
extern int      Pf_TileShapeLut_ae180(void*, char, char, char, char,
                                       char, char, char, char);          // 0x1400ae180
extern int      Pf_RegionSpiral_9d0a0(int*, int);                       // 0x14009d0a0
extern void     Pf_QuadtreeSubdivide_18880(uintptr_t);                  // 0x140018880
extern uintptr_t Pf_QuadtreeInit_18dd0(uintptr_t, float*, uint32_t, uint32_t); // 0x140018dd0
extern int      Pf_PastureTargetPick_53ca0(uintptr_t, int);             // 0x140053ca0

void RegisterPfGeomBatch() {
    RegisterExeAddr(0x1400a5ac0, reinterpret_cast<uintptr_t>(&Pf_RegionPairLookup_5ac0));
    RegisterExeAddr(0x1400a5ec0, reinterpret_cast<uintptr_t>(&Pf_NeighbourTile_5ec0));
    RegisterExeAddr(0x1400a6030, reinterpret_cast<uintptr_t>(&Pf_MoveHeuristic_6030));
    RegisterExeAddr(0x1400aa810, reinterpret_cast<uintptr_t>(&Pf_RayTriIntersect_aa810));
    RegisterExeAddr(0x1400ae180, reinterpret_cast<uintptr_t>(&Pf_TileShapeLut_ae180));
    RegisterExeAddr(0x14009d0a0, reinterpret_cast<uintptr_t>(&Pf_RegionSpiral_9d0a0));
    RegisterExeAddr(0x140018880, reinterpret_cast<uintptr_t>(&Pf_QuadtreeSubdivide_18880));
    RegisterExeAddr(0x140018dd0, reinterpret_cast<uintptr_t>(&Pf_QuadtreeInit_18dd0));
    RegisterExeAddr(0x140053ca0, reinterpret_cast<uintptr_t>(&Pf_PastureTargetPick_53ca0));
}
