// code/Game/Action/region_graph_build.cpp
// Pathfinder coarse region-connectivity graph — constructor + coarse-cell vector resize.
//
// Faithful 1:1 ports of three exe functions that build the pathfinder's coarse
#include "Action/exe_bodies.h"
// region-connectivity graph. Each Pf_*_xxxx is a literal transcription of the Ghidra
// decompile for the named exe address; faithfulness is everything (a wrong offset is
// silent corruption, strictly worse than leaving the addr unregistered). The same
// MemAlloc/MemFree free-function convention as code/Game/Action/action_pf_region.cpp.
//
//   0x1400a8670  Pf_RegionGraphCtor_8670    — region graph constructor
//   0x14001e060  Pf_CoarseCellResize_e060   — coarse-cell Vector resize (stride 0x20)
//   0x14001e420  Pf_CoarseCellFreeTail_e420 — MemFree-tail helper (shrink path)
//
// Sub-callees:
//   0x14000f9e0  Pf_IntArrayClone_f9e0  — REGISTERED (code/Game/Action/action_pf_pools.cpp)
//   0x14001e420  Pf_CoarseCellFreeTail_e420 — defined below (this file)
//
// Coarse-cell Vector layout (the {ptr@+0, count@+8(int), cap@+0xc(int)} header at
// param_1+0x1e of the graph object; element stride 0x20): per element
//   +0x00 int blockX, +0x04 int blockY, +0x08 byte dirtyA, +0x09 byte dirtyB,
//   +0x10 int-array {ptr,count,cap} member-region list. e060 deep-clones the +0x10
//   sub-array of each surviving element via f9e0; the shrink path frees the dropped
//   elements' sub-arrays via e420. In the fresh-build path (8670's only call, growing
//   an empty array) NEITHER f9e0 NOR e420 is reached.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

// Sub-callee in code/Game/Action/action_pf_pools.cpp (exe 0x14000f9e0).
extern int64_t* Pf_IntArrayClone_f9e0(int64_t* param_1, int64_t* param_2);

namespace {

// MemAlloc/MemFree shorthands matching the decompile's
// System::MemoryAllocator::MemAlloc(size, align) / MemFree(ptr) free-function form.
static inline void* MemAlloc(int64_t size, int align) {
    return fn_MemAlloc(g_pMemAllocSingleton, size, align);
}
static inline void MemFree(void* p) { fn_MemFreeStatic(p); }

} // namespace

// ============================================================
// 0x14001e420 — Pf_CoarseCellFreeTail_e420(unused, base, count)
// ============================================================
// MemFree the +0x10 sub-array of `count` coarse-cell elements starting at `base`
// (element stride 0x20 -> the qword walk is +4 qwords = 0x20 bytes), zeroing the
// {ptr,count} pair as it goes. param_1 is unused (decompiler-visible). Leaf.
void Pf_CoarseCellFreeTail_e420(int64_t param_1, int64_t param_2, int param_3) {
    (void)param_1;
    uint64_t* puVar1;
    int64_t lVar2;
    if (0 < param_3) {
        lVar2 = (int64_t)param_3;
        puVar1 = (uint64_t*)(param_2 + 0x10);
        do {
            MemFree((void*)*puVar1);
            *puVar1 = 0;
            puVar1[1] = 0;
            puVar1 = puVar1 + 4;
            lVar2 = lVar2 + -1;
        } while (lVar2 != 0);
    }
}

// ============================================================
// 0x14001e060 — Pf_CoarseCellResize_e060(vec, newCount)
// ============================================================
// Resize the coarse-cell Vector (header at param_1: {ptr@+0, count@+8(int),
// cap@+0xc(int)}, element stride 0x20). On grow-with-realloc, allocate cap*0x20 bytes
// and copy/zero-init each element (deep-cloning the +0x10 int sub-array via f9e0 for
// surviving elements, zero-init for new ones); on in-place shrink free the dropped
// elements' sub-arrays via e420; on realloc free the OLD buffer's sub-arrays (inline
// loop, walks old+0x10 stride 4 qwords) then MemFree the old buffer.
void Pf_CoarseCellResize_e060(int64_t* param_1, int param_2) {
    int iVar1;
    void* pvVar2;
    int iVar3;
    void* pvVar4;
    uint64_t* puVar5;
    int64_t* plVar6;
    int64_t* plVar7;
    uint8_t* puVar8;
    int64_t lVar9;
    int64_t lVar10;
    int64_t lVar11;

    lVar11 = (int64_t)param_2;
    iVar1 = (int)param_1[1];
    lVar10 = (int64_t)iVar1;
    if (param_2 != iVar1) {
        pvVar2 = (void*)*param_1;
        *(int*)(param_1 + 1) = param_2;
        plVar6 = param_1;
        if (*(int*)((int64_t)param_1 + 0xc) < param_2) {
            *(int*)((int64_t)param_1 + 0xc) = param_2;
            if ((param_2 == iVar1 + 1) && (iVar1 != 0)) {
                *(int*)((int64_t)param_1 + 0xc) = param_2 * 3;
            }
            plVar6 = (int64_t*)((int64_t)*(int*)((int64_t)param_1 + 0xc) << 5);
            pvVar4 = MemAlloc((int64_t)plVar6, 8);
            *param_1 = (int64_t)pvVar4;
        }
        iVar3 = 0;
        if (pvVar2 == (void*)*param_1) {
            iVar3 = iVar1;
        }
        lVar9 = (int64_t)iVar3;
        if (lVar9 < lVar11) {
            puVar8 = (uint8_t*)((int64_t)pvVar2 + lVar9 * 0x20 + 8);
            do {
                if (lVar9 < lVar10) {
                    plVar7 = (int64_t*)(puVar8 + *param_1 + (-8 - (int64_t)pvVar2));
                    plVar6 = plVar7;
                    if (plVar7 != (int64_t*)0x0) {
                        *(uint32_t*)plVar7 = *(uint32_t*)(puVar8 + -8);
                        *(uint32_t*)((int64_t)plVar7 + 4) = *(uint32_t*)(puVar8 + -4);
                        *(uint8_t*)(plVar7 + 1) = *puVar8;
                        *(uint8_t*)((int64_t)plVar7 + 9) = puVar8[1];
                        plVar6 = plVar7 + 2;
                        *plVar6 = 0;
                        plVar7[3] = 0;
                        Pf_IntArrayClone_f9e0(plVar6, (int64_t*)(puVar8 + 8));
                    }
                } else {
                    puVar5 = (uint64_t*)(puVar8 + *param_1 + (-8 - (int64_t)pvVar2));
                    if (puVar5 != (uint64_t*)0x0) {
                        *puVar5 = 0;
                        *(uint16_t*)(puVar5 + 1) = 0;
                        puVar5[2] = 0;
                        puVar5[3] = 0;
                    }
                }
                lVar9 = lVar9 + 1;
                puVar8 = puVar8 + 0x20;
            } while (lVar9 < lVar11);
        }
        if (pvVar2 == (void*)*param_1) {
            if (param_2 < iVar1) {
                Pf_CoarseCellFreeTail_e420((int64_t)plVar6,
                                           (int64_t)((int64_t)pvVar2 + lVar11 * 0x20),
                                           iVar1 - param_2);
            }
        } else {
            if (0 < iVar1) {
                puVar5 = (uint64_t*)((int64_t)pvVar2 + 0x10);
                do {
                    MemFree((void*)*puVar5);
                    *puVar5 = 0;
                    puVar5[1] = 0;
                    puVar5 = puVar5 + 4;
                    lVar10 = lVar10 + -1;
                } while (lVar10 != 0);
            }
            MemFree(pvVar2);
        }
    }
}

// ============================================================
// 0x1400a8670 — Pf_RegionGraphCtor_8670(graph, map, _, _, blockW, blockH)
// ============================================================
// Zero-init the region-graph object (param_1, treated as int*/undefined4* — param_1[N]
// = +N*4 bytes), store the Map pointer (param_2) at +0x50, blockW (param_5) at +0x60,
// blockH (param_6) at +0x64, then size the coarse-cell Vector (header at param_1+0x1e =
// +0x78) to blockW*blockH coarse cells via e060. The double loop initialises each coarse
// cell's blockX/blockY and dirty flags and appends its index onto two int-arrays (the
// dirty-list staging arrays at param_1+0x22 / +0x26) with the standard exe grow-and-copy.
// Finally grows a fixed 0x40-entry int-array at param_1+0x2e the same way. Returns graph.
int* Pf_RegionGraphCtor_8670(int* param_1, int64_t param_2, int64_t param_3, int64_t param_4,
                             uint32_t param_5, int param_6) {
    (void)param_3;
    (void)param_4;
    int* piVar1;
    int* puVar2;
    int iVar3;
    void* pvVar4;
    void* pvVar5;
    int iVar6;
    int64_t lVar7;
    int64_t lVar8;
    int iVar9;
    uint64_t uVar10;
    int iVar11;
    int64_t* plVar12;
    int64_t lVar13;
    int iVar14;
    int iVar15;
    int64_t local_res10;
    int local_res20;

    *param_1 = 1;
    *(uint64_t*)(param_1 + 2) = 0;
    *(uint64_t*)(param_1 + 4) = 0;
    *(uint64_t*)(param_1 + 6) = 0;
    param_1[8] = 0;
    *(uint64_t*)(param_1 + 10) = 0;
    *(uint64_t*)(param_1 + 0xc) = 0;
    *(uint64_t*)(param_1 + 0xe) = 8;
    *(uint64_t*)(param_1 + 0x10) = 0;
    param_1[0x12] = 0;
    *(uint64_t*)(param_1 + 0x14) = param_2;
    param_1[0x16] = 8;
    param_1[0x17] = 8;
    uVar10 = (uint64_t)(int)param_5;
    param_1[0x18] = param_5;
    param_1[0x19] = param_6;
    *(uint64_t*)(param_1 + 0x1a) = 0;
    *(uint64_t*)(param_1 + 0x1c) = 0;
    *(int64_t*)(param_1 + 0x1e) = 0;
    *(uint64_t*)(param_1 + 0x20) = 0;
    *(uint64_t*)(param_1 + 0x22) = 0;
    *(uint64_t*)(param_1 + 0x24) = 0;
    *(uint64_t*)(param_1 + 0x26) = 0;
    *(uint64_t*)(param_1 + 0x28) = 0;
    *(uint64_t*)(param_1 + 0x2a) = 0;
    *(uint64_t*)(param_1 + 0x2c) = 0;
    *(uint64_t*)(param_1 + 0x2e) = 0;
    *(uint64_t*)(param_1 + 0x30) = 0;
    *(uint64_t*)(param_1 + 0x32) = 0;
    *(uint64_t*)(param_1 + 0x34) = 0;
    param_1[0x36] = 0;
    *(uint64_t*)(param_1 + 0x38) = 0;
    *(uint64_t*)(param_1 + 0x3a) = 0;
    *(uint64_t*)(param_1 + 0x3c) = 8;
    *(uint64_t*)(param_1 + 0x3e) = 0;
    param_1[0x40] = 0;
    Pf_CoarseCellResize_e060((int64_t*)(param_1 + 0x1e), param_5 * param_6);
    local_res20 = 0;
    iVar14 = 0;
    if (0 < param_6) {
        local_res10 = 0;
        lVar8 = uVar10 * 0x20;
        iVar15 = 0;
        do {
            lVar13 = local_res10;
            iVar9 = 0;
            if (0 < (int)uVar10) {
                do {
                    plVar12 = (int64_t*)(param_1 + 0x1e);
                    *(int*)(lVar13 + *plVar12) = iVar9;
                    *(int*)(*plVar12 + 4 + lVar13) = local_res20;
                    *(uint8_t*)(*plVar12 + 9 + lVar13) = 1;
                    *(uint8_t*)(*plVar12 + 8 + lVar13) = 1;
                    iVar11 = param_1[0x24] + 1;
                    iVar3 = param_1[0x24];
                    if (iVar11 != iVar3) {
                        pvVar4 = *(void**)(param_1 + 0x22);
                        param_1[0x24] = iVar11;
                        if ((int)param_1[0x25] < iVar11) {
                            param_1[0x25] = iVar11;
                            if ((iVar11 == iVar3 + 1) && (iVar3 != 0)) {
                                param_1[0x25] = iVar11 * 3;
                            }
                            pvVar5 = MemAlloc((int64_t)(int)param_1[0x25] << 2, 4);
                            *(void**)(param_1 + 0x22) = pvVar5;
                        }
                        iVar6 = iVar14;
                        if (pvVar4 == *(void**)(param_1 + 0x22)) {
                            iVar6 = iVar3;
                        }
                        for (lVar7 = (int64_t)iVar6; lVar7 < iVar11; lVar7 = lVar7 + 1) {
                            piVar1 = (int*)(*(int64_t*)(param_1 + 0x22) + lVar7 * 4);
                            if (piVar1 != (int*)0x0) {
                                if (lVar7 < iVar3) {
                                    iVar6 = *(int*)((int64_t)pvVar4 + lVar7 * 4);
                                } else {
                                    iVar6 = iVar15 + iVar9;
                                }
                                *piVar1 = iVar6;
                            }
                        }
                        if (pvVar4 != *(void**)(param_1 + 0x22)) {
                            MemFree(pvVar4);
                        }
                    }
                    iVar11 = param_1[0x28] + 1;
                    iVar3 = param_1[0x28];
                    if (iVar11 != iVar3) {
                        pvVar4 = *(void**)(param_1 + 0x26);
                        param_1[0x28] = iVar11;
                        if ((int)param_1[0x29] < iVar11) {
                            param_1[0x29] = iVar11;
                            if ((iVar11 == iVar3 + 1) && (iVar3 != 0)) {
                                param_1[0x29] = iVar11 * 3;
                            }
                            pvVar5 = MemAlloc((int64_t)(int)param_1[0x29] << 2, 4);
                            *(void**)(param_1 + 0x26) = pvVar5;
                        }
                        iVar6 = iVar14;
                        if (pvVar4 == *(void**)(param_1 + 0x26)) {
                            iVar6 = iVar3;
                        }
                        for (lVar7 = (int64_t)iVar6; lVar7 < iVar11; lVar7 = lVar7 + 1) {
                            piVar1 = (int*)(*(int64_t*)(param_1 + 0x26) + lVar7 * 4);
                            if (piVar1 != (int*)0x0) {
                                if (lVar7 < iVar3) {
                                    iVar6 = *(int*)((int64_t)pvVar4 + lVar7 * 4);
                                } else {
                                    iVar6 = iVar15 + iVar9;
                                }
                                *piVar1 = iVar6;
                            }
                        }
                        if (pvVar4 != *(void**)(param_1 + 0x26)) {
                            MemFree(pvVar4);
                        }
                    }
                    iVar9 = iVar9 + 1;
                    uVar10 = (uint64_t)param_5;
                    lVar13 = lVar13 + 0x20;
                } while (iVar9 < (int)param_5);
            }
            local_res20 = local_res20 + 1;
            iVar15 = iVar15 + (int)uVar10;
            local_res10 = local_res10 + lVar8;
        } while (local_res20 < param_6);
    }
    iVar15 = param_1[0x30];
    if (iVar15 != 0x40) {
        pvVar4 = *(void**)(param_1 + 0x2e);
        param_1[0x30] = 0x40;
        if ((int)param_1[0x31] < 0x40) {
            param_1[0x31] = 0x40;
            if (iVar15 == 0x3f) {
                param_1[0x31] = 0xc0;
            }
            pvVar5 = MemAlloc((int64_t)(int)param_1[0x31] << 2, 4);
            *(void**)(param_1 + 0x2e) = pvVar5;
        }
        if (pvVar4 == *(void**)(param_1 + 0x2e)) {
            iVar14 = iVar15;
        }
        for (lVar8 = (int64_t)iVar14; lVar8 < 0x40; lVar8 = lVar8 + 1) {
            if ((lVar8 < iVar15) &&
                (puVar2 = (int*)(*(int64_t*)(param_1 + 0x2e) + lVar8 * 4), puVar2 != (int*)0x0)) {
                *puVar2 = *(int*)((int64_t)pvVar4 + lVar8 * 4);
            }
        }
        if (pvVar4 != *(void**)(param_1 + 0x2e)) {
            MemFree(pvVar4);
        }
    }
    return param_1;
}
