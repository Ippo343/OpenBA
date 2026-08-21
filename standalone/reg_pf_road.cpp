// standalone/reg_pf_road.cpp
// Registration for the PF-ROAD batch (code/Game/Action/action_pf_road.cpp).
//
// Maps each ported exe helper's Ghidra address to our reimplementation so that, in standalone
// mode, ResolveAddr(Module::Exe, 0xADDR) returns our function instead of 0. All 11 PF-ROAD
// addresses are ported + registered here (including 0x1400af370 road-mesh rebuild, whose two
// index-math details — the 3x3 mask -> ae180 arg order and the vertex AO-uv overwrite offsets —
// are now pinned against the disassembly).
//


#include "resolve.h"
#include <cstdint>

// Forward declarations of the ported helpers (defined in code/Game/Action/action_pf_road.cpp).
void  Pf_RoadVtxFill_caa0(int64_t self, int x, int y, short* outVtx, uint8_t blend);
void  Pf_RoadQuad_cc10(int64_t self, int x, int y, int64_t outQuad, int dirIdx, uint8_t blend);
int   Pf_CellQuadRay_aa20(int64_t self, int x, int y, void* p4, float* p5, float* p6,
                          float* p7, float* p8, uint32_t* p9);
float Pf_CellQuadRayWrap_0f90(int64_t self, float* param_2, uint32_t* param_3);
void  Pf_RegionDirty4_b150(int64_t self, int x, int y);
void  Pf_FineCellDirty_a8c90(int64_t self, int x, int y);
void* Pf_EntityResolvePos_2e20(void* self, float* retVec3, int p3, int p4, float* p5);
int   Pf_PickReachableCell_edb0(int64_t param_1, uint32_t param_2, float* param_3);
int   Pf_EntityAnchorCell_7fb0(void* param_1IBP, float* param_2);
void  Pf_RoadMeshRebuild_f370(int64_t param_1, uint32_t* param_2);

void RegisterPfRoadBatch() {
    RegisterExeAddr(0x1400acaa0, reinterpret_cast<uintptr_t>(&Pf_RoadVtxFill_caa0));
    RegisterExeAddr(0x1400acc10, reinterpret_cast<uintptr_t>(&Pf_RoadQuad_cc10));
    RegisterExeAddr(0x1400aaa20, reinterpret_cast<uintptr_t>(&Pf_CellQuadRay_aa20));
    RegisterExeAddr(0x1400b0f90, reinterpret_cast<uintptr_t>(&Pf_CellQuadRayWrap_0f90));
    RegisterExeAddr(0x1400ab150, reinterpret_cast<uintptr_t>(&Pf_RegionDirty4_b150));
    RegisterExeAddr(0x1400a8c90, reinterpret_cast<uintptr_t>(&Pf_FineCellDirty_a8c90));
    RegisterExeAddr(0x140122e20, reinterpret_cast<uintptr_t>(&Pf_EntityResolvePos_2e20));
    RegisterExeAddr(0x14004edb0, reinterpret_cast<uintptr_t>(&Pf_PickReachableCell_edb0));
    RegisterExeAddr(0x140087fb0, reinterpret_cast<uintptr_t>(&Pf_EntityAnchorCell_7fb0));
    RegisterExeAddr(0x1400af370, reinterpret_cast<uintptr_t>(&Pf_RoadMeshRebuild_f370));
}

