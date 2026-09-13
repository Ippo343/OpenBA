// code/Game/Action/action_pf_region.cpp
// Wave 3 (Action) / Cluster A — region-graph build/merge/flood-fill family.
//
// Faithful 1:1 ports of the coarse-region connectivity-graph helpers used by
// long-distance routing. Each Pf_*_xxxx is a literal transcription of the Ghidra
// decompile for the exe function 0x1400aXXXX; none is runtime-exercised on the
// autostart path, so faithfulness is everything (a wrong port is undetectable silent
// corruption, strictly worse than leaving the addr unregistered).
//
// Sub-callees are called via ResolveAddr(Module::Exe, 0xADDR) into a once-resolved
// static fn-ptr; in standalone they resolve to our registered reimpls:
//   0x1400021d0  int-array resize (stride 4)        — REGISTERED (Reimpl_Array_Resize_Stride04)
//   0x14001e210  Vector resize stride 0x30          — REGISTERED (reg_pf_pools)
//   0x14001df40  worklist-node pool alloc           — REGISTERED (reg_pf_pools)
//   0x14001d830  array resize stride 0x18           — REGISTERED (reg_pf_pools)
//   0x14001ca60  cell-BST AVL delete                — REGISTERED (reg_work_cellbst)
//   0x14001c940  BST insert (variant B)             — REGISTERED (reg_pf_avl)
//   0x140132010  sqrtf (NaN/Inf-aware)              — REGISTERED (reg_work_impl2)
//
// CLOSURE NOTE for the lead (a8ba0): 0x1400a8ba0 calls 0x14001dc00
// (BuildStructure_helper_dc00), which is NOT registered and itself calls 0x14001dcd0.
// a8ba0's body is ported below for completeness, but its registration MUST be withheld
// until 0x14001dc00 + 0x14001dcd0 land. The registrar (reg_pf_region.cpp) therefore
// registers all of THIS batch EXCEPT a8ba0 — see the note there.
//
// Region-node + region-graph struct field offsets (reverse-engineered, for verification):
//   regionGraph object = param_1 (the long-distance-router / GameWorld sub-object), with:
//     +0x08  cell-BST root (param_1+8 passed to 1d830/cellbst families)
//     +0x50  -> Map object; *(+0x50)         = mapDimX (grid stride), *(+0x50)+0x20 = cell array base
//     +0x58  int  blockW (coarse cell width in fine cells)
//     +0x5c  int  blockH (coarse cell height)
//     +0x68  region-node array base (stride 0x30): per node
//              +0x00 int centroidX, +0x04 int centroidY, +0x08 int regionId(=-1 free),
//              +0x10 int-array {ptr,count,cap} adjacency list,
//              +0x20 int-array {ptr,count,cap} border-edge cell list, +0x28 count, +0x2c cap
//     +0x70  int  regionNodeCount (high-water id)
//     +0x78  coarse-grid cell array base (stride 0x20): per coarse cell
//              +0x00 int blockX, +0x04 int blockY, +0x08 byte dirtyA, +0x09 byte dirtyB,
//              +0x10 int-array {ptr,count,cap} member-region list, +0x18 count
//     +0x88  int-array {ptr,count,cap} dirty-coarse-cell list A, +0x90 countA
//     +0x98  int-array {ptr,count,cap} dirty-coarse-cell list B, +0xa0 countB
//     +0xa8  int free-list array (region ids), +0xb0 count, +0xb4 cap  (8350/8200 stack)
//     +0xb8  int  region-label grid (stride 4, blockW*blockH per coarse cell)
//     +0xc0  int  region-label grid cell count
//     +0xc8  worklist BST root (a7ad0 flood-fill), +0xd0 head, +0xd8 count,
//              +0xe8 worklist free-list, +0xfc/+0x100 stats
//   region-pair cache (a5b30) = param_1 as undefined8*: [0]=seqId, [2]=ringHead/cap ints,
//     [3]=ring buffer base (stride 0x18), [5]=pair BST root.

#include "engine_funcs.h"
#include "resolve.h"
#include "hw_watch.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// ---- exe sub-callee fn-ptrs (resolved once, all registered in earlier batches) ----
typedef void     (*fn_ArrResize4_t)(int64_t* arr, int newCount);            // 0x1400021d0
typedef void     (*fn_VecResize30_t)(int64_t* vec, uint32_t newCount, uint32_t* fill); // 0x14001e210
typedef void*    (*fn_WorklistAlloc_t)(uintptr_t pool, uint64_t* val);      // 0x14001df40
typedef void     (*fn_ArrResize18_t)(int64_t* arr, int newCount);           // 0x14001d830
typedef void     (*fn_CellBstRemove_t)(uintptr_t* root, void* node);        // 0x14001ca60
typedef uint64_t*(*fn_BstInsertB_t)(int64_t* root, uint64_t* a, uint32_t* b);// 0x14001c940
typedef float    (*fn_Sqrtf_t)(float x);                                    // 0x140132010
typedef int      (*fn_BuildPath_dc00_t)(int* self, int a, uint32_t b, uint64_t c, int64_t outArr); // 0x14001dc00

static fn_ArrResize4_t    s_ArrResize4   = nullptr;
static fn_VecResize30_t   s_VecResize30  = nullptr;
static fn_WorklistAlloc_t s_WorklistAlloc= nullptr;
static fn_ArrResize18_t   s_ArrResize18  = nullptr;
static fn_CellBstRemove_t s_CellBstRemove= nullptr;
static fn_BstInsertB_t    s_BstInsertB   = nullptr;
static fn_Sqrtf_t         s_Sqrtf        = nullptr;
static fn_BuildPath_dc00_t s_BuildPath_dc00 = nullptr;
static bool s_init = false;

static void EnsureFns() {
    if (s_init) return;
    s_ArrResize4    = reinterpret_cast<fn_ArrResize4_t>(ResolveAddr(Module::Exe, 0x1400021d0));
    s_VecResize30   = reinterpret_cast<fn_VecResize30_t>(ResolveAddr(Module::Exe, 0x14001e210));
    s_WorklistAlloc = reinterpret_cast<fn_WorklistAlloc_t>(ResolveAddr(Module::Exe, 0x14001df40));
    s_ArrResize18   = reinterpret_cast<fn_ArrResize18_t>(ResolveAddr(Module::Exe, 0x14001d830));
    s_CellBstRemove = reinterpret_cast<fn_CellBstRemove_t>(ResolveAddr(Module::Exe, 0x14001ca60));
    s_BstInsertB    = reinterpret_cast<fn_BstInsertB_t>(ResolveAddr(Module::Exe, 0x14001c940));
    s_Sqrtf         = reinterpret_cast<fn_Sqrtf_t>(ResolveAddr(Module::Exe, 0x140132010));
    s_BuildPath_dc00= reinterpret_cast<fn_BuildPath_dc00_t>(ResolveAddr(Module::Exe, 0x14001dc00));
    s_init = true;
}

// MemAlloc/MemFree shorthands matching the decompile's
// System::MemoryAllocator::MemAlloc(size, align) / MemFree(ptr) free-function form.
static inline void* MemAlloc(int64_t size, int align) {
    return fn_MemAlloc(g_pMemAllocSingleton, size, align);
}
static inline void MemFree(void* p) { fn_MemFreeStatic(p); }

} // namespace

// ============================================================
// 0x1400a8200 — Pf_RegionNodeFree_8200(graph, nodeIdx)
// ============================================================
// Reset region node `nodeIdx` (stride 0x30 at graph+0x68): id/-1, clear its two int
// vectors (+0x10,+0x20) via 1400021d0(...,0), centroid/blockfields -1, then push nodeIdx
// onto the free-list int-array at graph+0xa8 (count +0xb0, cap +0xb4) with the standard
// exe grow-and-copy. Leaf except 0x1400021d0 (registered).
void Pf_RegionNodeFree_8200(int64_t param_1, int param_2) {
    EnsureFns();
    int64_t lVar7 = (int64_t)param_2 * 0x30;
    *(uint32_t*)(*(int64_t*)(param_1 + 0x68) + 8 + lVar7) = 0xffffffff;
    s_ArrResize4((int64_t*)(*(int64_t*)(param_1 + 0x68) + 0x10 + lVar7), 0);
    s_ArrResize4((int64_t*)(*(int64_t*)(param_1 + 0x68) + 0x20 + lVar7), 0);
    *(uint32_t*)(lVar7 + *(int64_t*)(param_1 + 0x68)) = 0xffffffff;
    *(uint32_t*)(*(int64_t*)(param_1 + 0x68) + 4 + lVar7) = 0xffffffff;
    int iVar2 = *(int*)(param_1 + 0xb0);
    int iVar6 = iVar2 + 1;
    if (iVar6 != iVar2) {
        void* pvVar3 = *(void**)(param_1 + 0xa8);
        *(int*)(param_1 + 0xb0) = iVar6;
        if (*(int*)(param_1 + 0xb4) < iVar6) {
            *(int*)(param_1 + 0xb4) = iVar6;
            if (iVar2 != 0) {
                *(int*)(param_1 + 0xb4) = iVar6 * 3;
            }
            void* pvVar5 = MemAlloc((int64_t)*(int*)(param_1 + 0xb4) << 2, 4);
            *(void**)(param_1 + 0xa8) = pvVar5;
        }
        int iVar4 = 0;
        if (pvVar3 == *(void**)(param_1 + 0xa8)) {
            iVar4 = iVar2;
        }
        for (int64_t lVar7b = (int64_t)iVar4; lVar7b < iVar6; lVar7b = lVar7b + 1) {
            int* piVar1 = (int*)(*(int64_t*)(param_1 + 0xa8) + lVar7b * 4);
            if (piVar1 != (int*)0x0) {
                int v = param_2;
                if (lVar7b < iVar2) {
                    v = *(int*)((int64_t)pvVar3 + lVar7b * 4);
                }
                *piVar1 = v;
            }
        }
        if (pvVar3 != *(void**)(param_1 + 0xa8)) {
            MemFree(pvVar3);
        }
    }
}

// ============================================================
// 0x1400a8350 — Pf_RegionFreeId_8350(graph) -> regionId
// ============================================================
// Pop a region id from the free-list (graph+0xa8 / count +0xb0). If the list is empty
// (count < 1), GROW the region-node array: id = graph+0x70, call 1e210(graph+0x68, id+1,
// &stack tmp-region-template) to size the node array to id+1, free the temp template's two
// vectors, and return the old graph+0x70 value. Otherwise pop the last id (and resize -1
// via 1400021d0). Returns the region id (undefined4 -> int).
//
// Faithfulness: the empty-list branch builds a zeroed 0x20-byte template on the stack
// (local_40[4]=ptrs/sizes pairs at +0x10/+0x20 of the node) and passes &local_40 as the
// fill arg to 1e210; afterwards it MemFrees local_20 then local_30 (the template's two
// vector ptrs). Transcribed literally.
int Pf_RegionFreeId_8350(int64_t param_1) {
    EnsureFns();
    int iVar1 = *(int*)(param_1 + 0xb0);
    uint32_t uVar2;
    if (iVar1 < 1) {
        uVar2 = *(uint32_t*)(param_1 + 0x70);
        // local template: layout matches Ghidra local_40[4]; local_30/_28/_20/_18.
        struct { uint32_t local_40[4]; void* local_30; uint64_t local_28; void* local_20; uint64_t local_18; } t;
        t.local_30 = (void*)0x0; t.local_28 = 0; t.local_20 = (void*)0x0; t.local_18 = 0;
        s_VecResize30((int64_t*)(param_1 + 0x68), *(uint32_t*)(param_1 + 0x70) + 1, t.local_40);
        MemFree(t.local_20); t.local_20 = (void*)0x0; t.local_18 = 0;
        MemFree(t.local_30);
    } else {
        uVar2 = *(uint32_t*)(*(int64_t*)(param_1 + 0xa8) + -4 + (int64_t)iVar1 * 4);
        s_ArrResize4((int64_t*)(param_1 + 0xa8), iVar1 + -1);
    }
    return (int)uVar2;
}

// ============================================================
// 0x1400a8420 — Pf_DirtyCoarseCellB_8420(graph, coarseCellIdx)
// ============================================================
// Mark coarse-grid cell `coarseCellIdx` (stride 0x20 at graph+0x78) dirty-flag-B (byte +9);
// if it was not already set, push the cell index onto dirty-list-B (graph+0x98 / count
// +0xa0 / cap +0xa4) with the standard exe grow-and-copy. Leaf (MemAlloc/MemFree only).
void Pf_DirtyCoarseCellB_8420(int64_t param_1, int param_2) {
    if (*(char*)((int64_t)param_2 * 0x20 + 9 + *(int64_t*)(param_1 + 0x78)) == '\0') {
        *(uint8_t*)((int64_t)param_2 * 0x20 + 9 + *(int64_t*)(param_1 + 0x78)) = 1;
        int iVar2 = *(int*)(param_1 + 0xa0);
        int iVar7 = iVar2 + 1;
        if (iVar7 != iVar2) {
            void* pvVar3 = *(void**)(param_1 + 0x98);
            *(int*)(param_1 + 0xa0) = iVar7;
            if (*(int*)(param_1 + 0xa4) < iVar7) {
                *(int*)(param_1 + 0xa4) = iVar7;
                if (iVar2 != 0) {
                    *(int*)(param_1 + 0xa4) = iVar7 * 3;
                }
                void* pvVar5 = MemAlloc((int64_t)*(int*)(param_1 + 0xa4) << 2, 4);
                *(void**)(param_1 + 0x98) = pvVar5;
            }
            int iVar4 = 0;
            if (pvVar3 == *(void**)(param_1 + 0x98)) {
                iVar4 = iVar2;
            }
            for (int64_t lVar6 = (int64_t)iVar4; lVar6 < iVar7; lVar6 = lVar6 + 1) {
                int* piVar1 = (int*)(*(int64_t*)(param_1 + 0x98) + lVar6 * 4);
                if (piVar1 != (int*)0x0) {
                    int v = param_2;
                    if (lVar6 < iVar2) {
                        v = *(int*)((int64_t)pvVar3 + lVar6 * 4);
                    }
                    *piVar1 = v;
                }
            }
            if (pvVar3 != *(void**)(param_1 + 0x98)) {
                MemFree(pvVar3);
            }
        }
    }
}

// ============================================================
// 0x1400a7480 — Pf_RegionRebuildAdjacency_7480(graph, coarseCellIdx)
// ============================================================
// For each member region of coarse cell `coarseCellIdx` (cell at graph+0x78, member list
// +0x10/+0x18): clear that region node's adjacency list (+0x10 -> 1400021d0(,0)); then for
// each of its border-edge cells (+0x20/+0x28) look up the neighbour region id from the Map
// cell array (*(graph+0x50)+0x80, stride 0x30, +0x18 = regionId), and if >= 0 and not
// already present in the adjacency list, append it (standard exe grow-and-copy at +0x10/
// +0x18/+0x1c). Pure pointer/array logic; calls 1400021d0 (registered). The duplicate
// scan uses the `iVar8 < 0` early-out exactly as decompiled.
void Pf_RegionRebuildAdjacency_7480(int64_t param_1, int param_2) {
    EnsureFns();
    int local_res10 = 0;
    int64_t lVar4 = (int64_t)param_2 * 0x20 + *(int64_t*)(param_1 + 0x78);
    if (0 < *(int*)(lVar4 + 0x18)) {
        int64_t local_res18 = 0;
        do {
            int64_t lVar11 = (int64_t)*(int*)(*(int64_t*)(lVar4 + 0x10) + local_res18) * 0x30 +
                             *(int64_t*)(param_1 + 0x68);
            s_ArrResize4((int64_t*)(lVar11 + 0x10), 0);
            int iVar12 = 0;
            if (0 < *(int*)(lVar11 + 0x28)) {
                int64_t lVar10 = 0;
                do {
                    int iVar1 = *(int*)(*(int64_t*)(*(int64_t*)(param_1 + 0x50) + 0x80) + 0x18 +
                                        (int64_t)*(int*)(lVar10 + *(int64_t*)(lVar11 + 0x20)) * 0x30);
                    if (-1 < iVar1) {
                        int iVar9 = *(int*)(lVar11 + 0x18);
                        int iVar8 = 0;
                        int64_t lVar7 = 0;
                        if (0 < iVar9) {
                            int* piVar5 = *(int**)(lVar11 + 0x10);
                            do {
                                if (*piVar5 == iVar1) {
                                    if (-1 < iVar8) goto LAB_1400a75f8;
                                    break;
                                }
                                lVar7 = lVar7 + 1;
                                iVar8 = iVar8 + 1;
                                piVar5 = piVar5 + 1;
                            } while (lVar7 < iVar9);
                        }
                        iVar8 = *(int*)(lVar11 + 0x18);
                        iVar9 = iVar9 + 1;
                        if (iVar9 != iVar8) {
                            void* pvVar2 = *(void**)(lVar11 + 0x10);
                            *(int*)(lVar11 + 0x18) = iVar9;
                            if (*(int*)(lVar11 + 0x1c) < iVar9) {
                                *(int*)(lVar11 + 0x1c) = iVar9;
                                if ((iVar9 == iVar8 + 1) && (iVar8 != 0)) {
                                    *(int*)(lVar11 + 0x1c) = iVar9 * 3;
                                }
                                void* pvVar6 = MemAlloc((int64_t)*(int*)(lVar11 + 0x1c) << 2, 4);
                                *(void**)(lVar11 + 0x10) = pvVar6;
                            }
                            int iVar3 = 0;
                            if (pvVar2 == *(void**)(lVar11 + 0x10)) {
                                iVar3 = iVar8;
                            }
                            for (int64_t lVar7b = (int64_t)iVar3; lVar7b < iVar9; lVar7b = lVar7b + 1) {
                                int* piVar5 = (int*)(*(int64_t*)(lVar11 + 0x10) + lVar7b * 4);
                                if (piVar5 != (int*)0x0) {
                                    int v = iVar1;
                                    if (lVar7b < iVar8) {
                                        v = *(int*)((int64_t)pvVar2 + lVar7b * 4);
                                    }
                                    *piVar5 = v;
                                }
                            }
                            if (pvVar2 != *(void**)(lVar11 + 0x10)) {
                                MemFree(pvVar2);
                            }
                        }
                    }
LAB_1400a75f8:
                    iVar12 = iVar12 + 1;
                    lVar10 = lVar10 + 4;
                } while (iVar12 < *(int*)(lVar11 + 0x28));
            }
            local_res10 = local_res10 + 1;
            local_res18 = local_res18 + 4;
        } while (local_res10 < *(int*)(lVar4 + 0x18));
    }
}

// ============================================================
// 0x1400a7670 — Pf_StampRegionIdBlock_7670(graph, coarseCellIdx)
// ============================================================
// Write the final region-id for every fine cell of coarse cell `coarseCellIdx` into the
// Map cell array (*(graph+0x50)+0x20, stride 0x30, +0x18 = regionId). The coarse cell
// (graph+0x78, stride 0x20) supplies blockX (+0x00) and a member-region-id remap array
// (+0x04). For each (row 0..blockH, col 0..blockW): read the label grid (graph+0xb8)
// value-1; if <0 write -1, else map through the +0x04 array; destination flat index =
// ((row + blockY*blockH) * mapStride + col) + blockX*blockW. Pure index math, leaf.
//
// NOTE (faithfulness trap): the decompile reuses `iVar4` as both the inner loop bound
// (reloaded from param_1+0x58 each col) and the block stride; transcribed verbatim.
void Pf_StampRegionIdBlock_7670(int64_t param_1, int param_2) {
    int iVar1 = *(int*)(param_1 + 0x5c);
    int iVar4 = *(int*)(param_1 + 0x58);
    int iVar7 = 0;
    int64_t lVar6 = 0;
    int* piVar11 = (int*)((int64_t)param_2 * 0x20 + *(int64_t*)(param_1 + 0x78));
    int iVar2 = piVar11[1];
    int iVar5 = *piVar11 * iVar4;
    if (0 < iVar1) {
        do {
            int iVar9 = 0;
            int64_t lVar10 = 0;
            if (0 < iVar4) {
                do {
                    int iVar3 = (iVar7 + iVar2 * iVar1) * **(int**)(param_1 + 0x50) + iVar9;
                    iVar4 = *(int*)(*(int64_t*)(param_1 + 0xb8) + (iVar4 * lVar6 + lVar10) * 4) + -1;
                    uint32_t uVar8;
                    if (iVar4 < 0) {
                        uVar8 = 0xffffffff;
                    } else {
                        uVar8 = *(uint32_t*)(*(int64_t*)(piVar11 + 4) + (int64_t)iVar4 * 4);
                    }
                    iVar9 = iVar9 + 1;
                    lVar10 = lVar10 + 1;
                    *(uint32_t*)(*(int64_t*)(*(int**)(param_1 + 0x50) + 0x20) + 0x18 +
                                 (int64_t)(iVar3 + iVar5) * 0x30) = uVar8;
                    iVar4 = *(int*)(param_1 + 0x58);
                } while (iVar9 < iVar4);
            }
            iVar7 = iVar7 + 1;
            lVar6 = lVar6 + 1;
        } while (iVar7 < *(int*)(param_1 + 0x5c));
    }
}

// ============================================================
// 0x1400a7750 — Pf_AddBorderEdge_7750(graph, regionNode, blockX, blockY, fineX, fineY)
// ============================================================
// For the 3x3-minus-interior border ring around (fineX-1..fineX+1, fineY-1..fineY+1):
// for each ring cell that is in-bounds and OUTSIDE the coarse block [blockX..blockX+blockW)
// x [blockY..blockY+blockH), append its flat Map index (mapStride*y + x) to region node
// `regionNode`'s border-edge list (node at graph+0x68 stride 0x30, list +0x20/+0x28/+0x2c)
// with the standard grow-and-copy and a duplicate-skip scan. Two nested do/while (outer
// 3 rows via local_64=-1..1, inner 3 cols via lVar14=3 countdown). Leaf (MemAlloc/MemFree).
void Pf_AddBorderEdge_7750(int64_t param_1, int param_2, int param_3, int param_4,
                           int param_5, int param_6) {
    int iVar1 = *(int*)(param_1 + 0x58);
    int iVar2 = *(int*)(param_1 + 0x5c);
    int iVar3 = **(int**)(param_1 + 0x50);
    int local_64 = -1;
    do {
        int64_t lVar14 = 3;
        int iVar13 = param_6 + local_64;
        int iVar15 = param_5 + -1;
        do {
            if (((((-1 < iVar15) && (-1 < iVar13)) && (iVar15 < iVar3)) && (iVar13 < iVar3)) &&
                (((iVar15 < param_3 || (iVar13 < param_4)) ||
                  ((param_3 + -1 + iVar1 < iVar15 || (param_4 + -1 + iVar2 < iVar13)))))) {
                int iVar9 = 0;
                int64_t lVar8 = 0;
                int64_t lVar12 = (int64_t)param_2 * 0x30 + *(int64_t*)(param_1 + 0x68);
                int iVar11 = **(int**)(param_1 + 0x50) * iVar13 + iVar15;
                if (0 < (int64_t)*(int*)(lVar12 + 0x28)) {
                    int* piVar6 = *(int**)(lVar12 + 0x20);
                    do {
                        if (*piVar6 == iVar11) {
                            if (-1 < iVar9) goto LAB_1400a7906;
                            break;
                        }
                        lVar8 = lVar8 + 1;
                        iVar9 = iVar9 + 1;
                        piVar6 = piVar6 + 1;
                    } while (lVar8 < *(int*)(lVar12 + 0x28));
                }
                iVar9 = *(int*)(lVar12 + 0x28);
                int iVar10 = *(int*)(lVar12 + 0x28) + 1;
                if (iVar10 != iVar9) {
                    void* pvVar4 = *(void**)(lVar12 + 0x20);
                    *(int*)(lVar12 + 0x28) = iVar10;
                    if (*(int*)(lVar12 + 0x2c) < iVar10) {
                        *(int*)(lVar12 + 0x2c) = iVar10;
                        if ((iVar10 == iVar9 + 1) && (iVar9 != 0)) {
                            *(int*)(lVar12 + 0x2c) = iVar10 * 3;
                        }
                        void* pvVar7 = MemAlloc((int64_t)*(int*)(lVar12 + 0x2c) << 2, 4);
                        *(void**)(lVar12 + 0x20) = pvVar7;
                    }
                    int iVar5 = 0;
                    if (pvVar4 == *(void**)(lVar12 + 0x20)) {
                        iVar5 = iVar9;
                    }
                    for (lVar8 = (int64_t)iVar5; lVar8 < iVar10; lVar8 = lVar8 + 1) {
                        int* piVar6 = (int*)(*(int64_t*)(lVar12 + 0x20) + lVar8 * 4);
                        if (piVar6 != (int*)0x0) {
                            int v = iVar11;
                            if (lVar8 < iVar9) {
                                v = *(int*)((int64_t)pvVar4 + lVar8 * 4);
                            }
                            *piVar6 = v;
                        }
                    }
                    if (pvVar4 != *(void**)(lVar12 + 0x20)) {
                        MemFree(pvVar4);
                    }
                }
            }
LAB_1400a7906:
            iVar15 = iVar15 + 1;
            lVar14 = lVar14 + -1;
        } while (lVar14 != 0);
        local_64 = local_64 + 1;
        if (1 < local_64) {
            return;
        }
    } while (true);
}

// ============================================================
// 0x1400a7950 — Pf_RegionCentroidAndEdges_7950(graph, coarseCellIdx, labelValue) -> regionId
// ============================================================
// Allocate a fresh region id (a8350), stamp it into the coarse cell member array, then scan
// every fine cell of the coarse block; for each cell whose label grid (graph+0xb8) == the
// flood-fill `labelValue`, accumulate the centroid sums and, if it sits on the block border
// (col/row == 0 or == blockW/H-1), call a7750 to add its outward border edges. Finally write
// the integer-averaged centroid to the region node (+0x00,+0x04) and store the coarse cell
// index at node+0x08 (regionId field reuse: piVar4[2]=param_2). Returns the new region id
// (zero-extended). Calls a8350 + a7750.
//
// Faithfulness traps: a8350 (TradeComponent_impl_8350) returns the new id as undefined4;
// uVar2 (the id) is used both as the region-node selector (uVar12*0x30) and passed to a7750.
// The averages use integer division (iVar10/iVar16). Decompiler's `iVar9` is reloaded from
// +0x5c inside the inner loop; transcribed verbatim. Member-array stamp: piVar4[2]=param_2
// stores the coarse cell index into node+0x08 BEFORE the scan.
uint64_t Pf_RegionCentroidAndEdges_7950(int64_t param_1, int param_2, int param_3) {
    EnsureFns();
    uint32_t uVar2 = (uint32_t)Pf_RegionFreeId_8350(param_1);
    uint64_t uVar12 = (uint64_t)(int)uVar2;
    int iVar10 = 0;
    int iVar11 = 0;
    int* piVar5 = (int*)((int64_t)param_2 * 0x20 + *(int64_t*)(param_1 + 0x78));
    int iVar16 = 0;
    int* piVar4 = (int*)(uVar12 * 0x30 + *(int64_t*)(param_1 + 0x68));
    int iVar15 = 0;
    int64_t lVar7 = 0;
    piVar4[2] = param_2;
    int iVar9 = *(int*)(param_1 + 0x5c);
    int iVar13 = *piVar5 * *(int*)(param_1 + 0x58);
    int iVar14 = piVar5[1] * iVar9;
    if (0 < *(int*)(param_1 + 0x58)) {
        do {
            int iVar6 = 0;
            int64_t lVar8 = 0;
            if (0 < iVar9) {
                do {
                    if (*(int*)(*(int64_t*)(param_1 + 0xb8) +
                                (*(int*)(param_1 + 0x58) * lVar7 + lVar8) * 4) == param_3) {
                        int iVar3 = iVar6 + iVar13;
                        iVar16 = iVar16 + 1;
                        iVar10 = iVar10 + iVar3;
                        int iVar1 = iVar15 + iVar14;
                        iVar11 = iVar11 + iVar1;
                        if ((((lVar8 == 0) || (lVar7 == 0)) || (iVar6 == *(int*)(param_1 + 0x58) + -1)) ||
                            (iVar15 == iVar9 + -1)) {
                            Pf_AddBorderEdge_7750(param_1, uVar2, iVar13, iVar14, iVar3, iVar1);
                        }
                    }
                    uVar12 = (uint64_t)uVar2;
                    iVar9 = *(int*)(param_1 + 0x5c);
                    iVar6 = iVar6 + 1;
                    lVar8 = lVar8 + 1;
                } while (iVar6 < iVar9);
            }
            iVar15 = iVar15 + 1;
            lVar7 = lVar7 + 1;
        } while (iVar15 < *(int*)(param_1 + 0x58));
    }
    *piVar4 = iVar10 / iVar16;
    piVar4[1] = iVar11 / iVar16;
    return uVar12 & 0xffffffff;
}

// ============================================================
// 0x1400a7ad0 — Pf_FloodFillLabel_7ad0(graph, blockX, blockY, fineX, fineY, label)
// ============================================================
// Worklist flood-fill: seed the worklist BST/list (graph+0xc8 root, +0xd0 head, +0xd8
// count, +0xe8 free-list) with the start cell (fineX,fineY) via 1df40, then pop-and-process
// in a do/while(true) loop. For each popped (x,y): if the label grid (graph+0xb8) is 0 and
// the Map cell flags (*(graph+0x50)+0x20, stride 0x30, +0x29 byte) test passable
// (mask 0xe1f), expand the contiguous passable horizontal run [x-... x+...], paint that run
// of label-grid cells with `label`, then enqueue the row above (y-1) and below (y+1) for
// every passable+unlabelled cell in the run. Calls 1df40 (registered) repeatedly to
// allocate worklist nodes; the BST-link bookkeeping (head/prev/next/free-list) is inlined
// exactly as decompiled. Heavy worklist do/while(true) — transcribed literally.
//
// Faithfulness traps: local_res8 packs (x in low32, y in high32). Map flag passability test
// is `(1 << (flagByte & 0x1f) & 0xe1f) != 0`. The run scan walks the cell array by -0x30 /
// +0x30 byte strides reading the flag byte at +0x29 (offsets -7 and +0x59 are flagByte at
// the prev/next cell). The two enqueue loops (row above / row below) re-read param_2/param_3
// from local_res10/local_res18 at each iteration, exactly as the decompiler shows.
void Pf_FloodFillLabel_7ad0(int64_t param_1, int param_2, int param_3,
                            uint32_t param_4, uint32_t param_5, uint16_t param_6) {
    EnsureFns();
    uint64_t* puVar1 = (uint64_t*)(param_1 + 200);   // graph+0xc8 worklist root
    uint64_t local_res8 = ((uint64_t)param_5 << 32) | (uint64_t)param_4;
    int local_res10 = param_2;
    int local_res18 = param_3;
    uint64_t* puVar3 = (uint64_t*)s_WorklistAlloc((uintptr_t)puVar1, &local_res8);
    if (*puVar1 == 0) {
        *(uint64_t**)(param_1 + 0xd0) = puVar3;
    } else {
        *(uint64_t**)(*puVar1 + 8) = puVar3;
        puVar3[2] = *puVar1;
    }
    *(int*)(param_1 + 0xd8) = *(int*)(param_1 + 0xd8) + 1;
    *puVar1 = (uint64_t)puVar3;
    int iVar12 = *(int*)(param_1 + 0xd8);
    do {
        if (iVar12 < 1) {
            return;
        }
        uint64_t* puVar2 = *(uint64_t**)(param_1 + 0xd0);
        local_res8 = *puVar2;
        if (puVar2 == (uint64_t*)*puVar1) {
            *puVar1 = puVar2[2];
        }
        *(uint64_t*)(param_1 + 0xd0) = puVar2[1];
        if (puVar2[1] != 0) {
            *(uint64_t*)(puVar2[1] + 0x10) = puVar2[2];
        }
        if (puVar2[2] != 0) {
            *(uint64_t*)(puVar2[2] + 8) = puVar2[1];
        }
        puVar2[1] = 0;
        puVar2[2] = 0;
        *(int*)(param_1 + 0xd8) = *(int*)(param_1 + 0xd8) + -1;
        *(int*)(param_1 + 0xfc) = *(int*)(param_1 + 0xfc) + -1;
        *(int*)(param_1 + 0x100) = *(int*)(param_1 + 0x100) + 1;
        *puVar2 = *(uint64_t*)(param_1 + 0xe8);
        *(uint64_t**)(param_1 + 0xe8) = puVar2;
        int iVar7 = *(int*)(param_1 + 0x58) + -1;
        uint32_t uVar14 = (uint32_t)local_res8;
        int64_t lVar10 = (int64_t)(int)uVar14;
        iVar12 = (int)(local_res8 >> 0x20);
        if (*(int*)(*(int64_t*)(param_1 + 0xb8) +
                    ((int64_t)*(int*)(param_1 + 0x58) * (int64_t)iVar12 + lVar10) * 4) == 0) {
            int64_t lVar13 = (int64_t)param_3;
            int64_t lVar15 = (int64_t)param_2;
            int64_t lVar11 = (int64_t)**(int**)(param_1 + 0x50);
            int64_t lVar5 = *(int64_t*)(*(int**)(param_1 + 0x50) + 0x20);
            if ((1 << (*(uint8_t*)(lVar5 + 0x29 + ((iVar12 + lVar13) * lVar11 + lVar15 + lVar10) * 0x30) &
                       0x1f) & 0xe1fU) != 0) {
                uint64_t uVar4 = local_res8 & 0xffffffff;
                uint32_t uVar6 = uVar14;
                if (-1 < (int)(uVar14 - 1)) {
                    uint8_t* pbVar9 = (uint8_t*)(lVar5 + -7 + ((iVar12 + lVar13) * lVar11 + lVar15 + lVar10) * 0x30);
                    do {
                        uVar6 = (uint32_t)uVar4;
                        if ((1 << (*pbVar9 & 0x1f) & 0xe1fU) == 0) break;
                        uVar6 = (uint32_t)uVar4 - 1;
                        uVar4 = (uint64_t)uVar6;
                        pbVar9 = pbVar9 + -0x30;
                    } while (0 < (int)uVar6);
                }
                int iVar8 = uVar14 + 1;
                uVar4 = local_res8;
                if (iVar8 <= iVar7) {
                    uint8_t* pbVar9 = (uint8_t*)(lVar5 + 0x59 + ((iVar12 + lVar13) * lVar11 + lVar15 + lVar10) * 0x30);
                    do {
                        if ((1 << (*pbVar9 & 0x1f) & 0xe1fU) == 0) break;
                        iVar8 = iVar8 + 1;
                        uVar4 = (uint64_t)((int)uVar4 + 1);
                        pbVar9 = pbVar9 + 0x30;
                    } while (iVar8 <= iVar7);
                }
                lVar10 = (int64_t)(int)uVar6;
                iVar8 = (int)uVar4;
                if (lVar10 <= iVar8) {
                    do {
                        int64_t lVar5b = (int64_t)*(int*)(param_1 + 0x58) * (int64_t)iVar12 + lVar10;
                        lVar10 = lVar10 + 1;
                        *(uint32_t*)(*(int64_t*)(param_1 + 0xb8) + lVar5b * 4) = (uint32_t)param_6;
                    } while (lVar10 <= iVar8);
                }
                if (-1 < (int)(uVar6 - 1)) {
                    uVar6 = uVar6 - 1;
                }
                if (iVar8 + 1 <= iVar7) {
                    iVar8 = iVar8 + 1;
                }
                if (0 < iVar12) {
                    iVar12 = iVar12 + -1;
                    uVar14 = uVar6;
                    for (lVar10 = (int64_t)(int)uVar6; lVar10 <= iVar8; lVar10 = lVar10 + 1) {
                        if ((*(int*)(*(int64_t*)(param_1 + 0xb8) +
                                     ((int64_t)*(int*)(param_1 + 0x58) * (int64_t)iVar12 + lVar10) * 4) == 0)
                            && ((1 << (*(uint8_t*)(*(int64_t*)(*(int**)(param_1 + 0x50) + 0x20) + 0x29 +
                                       ((iVar12 + lVar13) * (int64_t)**(int**)(param_1 + 0x50) +
                                        (int64_t)local_res10 + lVar10) * 0x30) & 0x1f) & 0xe1fU) != 0)) {
                            uint32_t local_50 = uVar14;
                            int local_4c = iVar12;
                            uint64_t pack = ((uint64_t)(uint32_t)local_4c << 32) | (uint64_t)local_50;
                            puVar3 = (uint64_t*)s_WorklistAlloc((uintptr_t)puVar1, &pack);
                            if (*puVar1 == 0) {
                                *(uint64_t**)(param_1 + 0xd0) = puVar3;
                            } else {
                                *(uint64_t**)(*puVar1 + 8) = puVar3;
                                puVar3[2] = *puVar1;
                            }
                            *(int*)(param_1 + 0xd8) = *(int*)(param_1 + 0xd8) + 1;
                            *puVar1 = (uint64_t)puVar3;
                        }
                        uVar14 = uVar14 + 1;
                    }
                    iVar12 = (int)(local_res8 >> 0x20);
                }
                param_2 = local_res10;
                param_3 = local_res18;
                if (iVar12 < *(int*)(param_1 + 0x5c) + -1) {
                    iVar12 = iVar12 + 1;
                    for (lVar10 = (int64_t)(int)uVar6; ((local_res10 = param_2), (local_res18 = param_3),
                         lVar10 <= iVar8); lVar10 = lVar10 + 1) {
                        if ((*(int*)(*(int64_t*)(param_1 + 0xb8) +
                                     ((int64_t)*(int*)(param_1 + 0x58) * (int64_t)iVar12 + lVar10) * 4) == 0)
                            && ((1 << (*(uint8_t*)(*(int64_t*)(*(int**)(param_1 + 0x50) + 0x20) + 0x29 +
                                       ((iVar12 + lVar13) * (int64_t)**(int**)(param_1 + 0x50) +
                                        (int64_t)param_2 + lVar10) * 0x30) & 0x1f) & 0xe1fU) != 0)) {
                            uint32_t local_48 = uVar6;
                            int local_44 = iVar12;
                            uint64_t pack = ((uint64_t)(uint32_t)local_44 << 32) | (uint64_t)local_48;
                            puVar3 = (uint64_t*)s_WorklistAlloc((uintptr_t)puVar1, &pack);
                            if (*puVar1 == 0) {
                                *(uint64_t**)(param_1 + 0xd0) = puVar3;
                            } else {
                                *(uint64_t**)(*puVar1 + 8) = puVar3;
                                puVar3[2] = *puVar1;
                            }
                            *(int*)(param_1 + 0xd8) = *(int*)(param_1 + 0xd8) + 1;
                            *puVar1 = (uint64_t)puVar3;
                        }
                        uVar6 = uVar6 + 1;
                        param_2 = local_res10;
                        param_3 = local_res18;
                    }
                }
            }
        }
        iVar12 = *(int*)(param_1 + 0xd8);
    } while (true);
}

// ============================================================
// 0x1400a7f20 — Pf_SeedFloodFills_7f20(graph, coarseCellIdx) -> regionCount
// ============================================================
// Clear the label grid (graph+0xb8, graph+0xc0 cells) to 0, then sweep every fine cell of
// the coarse block; for each passable+unlabelled cell, start a new flood-fill (a7ad0) with
// an incrementing label. Returns (label-1) = number of distinct regions found in the block.
// Calls a7ad0. Map flag test identical to a7ad0 (mask 0xe1f on +0x29 byte).
int Pf_SeedFloodFills_7f20(int64_t param_1, int param_2) {
    EnsureFns();
    int iVar5 = 0;
    if (0 < *(int*)(param_1 + 0xc0)) {
        int64_t lVar8 = 0;
        do {
            iVar5 = iVar5 + 1;
            *(uint32_t*)(lVar8 + *(int64_t*)(param_1 + 0xb8)) = 0;
            lVar8 = lVar8 + 4;
        } while (iVar5 < *(int*)(param_1 + 0xc0));
    }
    iVar5 = *(int*)(param_1 + 0x5c);
    int iVar9 = 0;
    int* piVar1 = (int*)((int64_t)param_2 * 0x20 + *(int64_t*)(param_1 + 0x78));
    int64_t lVar8 = 0;
    uint16_t uVar2 = 1;
    int iVar6 = *piVar1 * *(int*)(param_1 + 0x58);
    int iVar7 = piVar1[1] * iVar5;
    if (0 < *(int*)(param_1 + 0x58)) {
        do {
            int iVar4 = 0;
            int64_t lVar3 = 0;
            if (0 < iVar5) {
                do {
                    if (((1 << (*(uint8_t*)(*(int64_t*)(*(int**)(param_1 + 0x50) + 0x20) + 0x29 +
                                ((iVar7 + lVar8) * (int64_t)**(int**)(param_1 + 0x50) +
                                 (int64_t)iVar6 + lVar3) * 0x30) & 0x1f) & 0xe1fU) != 0) &&
                        (*(int*)(*(int64_t*)(param_1 + 0xb8) +
                                 (*(int*)(param_1 + 0x58) * lVar8 + lVar3) * 4) == 0)) {
                        Pf_FloodFillLabel_7ad0(param_1, iVar6, iVar7, iVar4, iVar9, uVar2);
                        uVar2 = uVar2 + 1;
                    }
                    iVar5 = *(int*)(param_1 + 0x5c);
                    iVar4 = iVar4 + 1;
                    lVar3 = lVar3 + 1;
                } while (iVar4 < iVar5);
            }
            iVar9 = iVar9 + 1;
            lVar8 = lVar8 + 1;
        } while (iVar9 < *(int*)(param_1 + 0x58));
    }
    return uVar2 - 1;
}

// ============================================================
// 0x1400a80a0 — Pf_RebuildRegionChunk_80a0(graph, coarseCellIdx)
// ============================================================
// Rebuild the region graph for one coarse cell: free all current member region nodes
// (a8200 each, from member list +0x10/+0x18), clear that list (1400021d0), seed+run the
// flood-fills (a7f20) which yields a region count, then for each region (1..count) compute
// its centroid+border-edges (a7950, returns the new region id) and append that id to the
// coarse cell's member list (+0x10/+0x18/+0x1c, standard grow-and-copy). Finally stamp the
// region ids into the Map cell array (a7670). Calls a8200, a7f20, a7950, a7670, 1400021d0.
void Pf_RebuildRegionChunk_80a0(int64_t param_1, uint32_t param_2) {
    EnsureFns();
    uint64_t uVar13 = (uint64_t)(int)param_2;
    int iVar10 = 0;
    int64_t lVar8 = uVar13 * 0x20 + *(int64_t*)(param_1 + 0x78);
    if (0 < *(int*)(lVar8 + 0x18)) {
        int64_t lVar7 = 0;
        do {
            Pf_RegionNodeFree_8200(param_1, *(int*)(lVar7 + *(int64_t*)(lVar8 + 0x10)));
            iVar10 = iVar10 + 1;
            lVar7 = lVar7 + 4;
        } while (iVar10 < *(int*)(lVar8 + 0x18));
    }
    s_ArrResize4((int64_t*)(lVar8 + 0x10), 0);
    iVar10 = Pf_SeedFloodFills_7f20(param_1, param_2);
    int iVar14 = 0;
    uint32_t uVar12 = param_2;
    if (0 < iVar10) {
        do {
            iVar14 = iVar14 + 1;
            uint64_t uVar5 = Pf_RegionCentroidAndEdges_7950(param_1, (int)uVar13, iVar14);
            int iVar2 = *(int*)(lVar8 + 0x18);
            int iVar11 = iVar2 + 1;
            if (iVar11 != iVar2) {
                void* pvVar3 = *(void**)(lVar8 + 0x10);
                *(int*)(lVar8 + 0x18) = iVar11;
                if (*(int*)(lVar8 + 0x1c) < iVar11) {
                    *(int*)(lVar8 + 0x1c) = iVar11;
                    if (iVar2 != 0) {
                        *(int*)(lVar8 + 0x1c) = iVar11 * 3;
                    }
                    void* pvVar6 = MemAlloc((int64_t)*(int*)(lVar8 + 0x1c) << 2, 4);
                    *(void**)(lVar8 + 0x10) = pvVar6;
                }
                int iVar4 = 0;
                if (pvVar3 == *(void**)(lVar8 + 0x10)) {
                    iVar4 = iVar2;
                }
                for (int64_t lVar7 = (int64_t)iVar4; lVar7 < iVar11; lVar7 = lVar7 + 1) {
                    uint32_t* puVar1 = (uint32_t*)(*(int64_t*)(lVar8 + 0x10) + lVar7 * 4);
                    if (puVar1 != (uint32_t*)0x0) {
                        uint32_t uVar9;
                        if (lVar7 < iVar2) {
                            uVar9 = *(uint32_t*)((int64_t)pvVar3 + lVar7 * 4);
                        } else {
                            uVar9 = (uint32_t)uVar5;
                        }
                        *puVar1 = uVar9;
                    }
                }
                if (pvVar3 != *(void**)(lVar8 + 0x10)) {
                    MemFree(pvVar3);
                }
                uVar13 = (uint64_t)param_2;
            }
            uVar12 = (uint32_t)uVar13;
        } while (iVar14 < iVar10);
    }
    Pf_StampRegionIdBlock_7670(param_1, uVar12);
}

// ============================================================
// 0x1400a8a90 — Pf_FlushDirtyChunks_8a90(graph)
// ============================================================
// Process the two dirty-coarse-cell lists. List A (graph+0x88/+0x90): for each cell call
// a80a0 (full rebuild) and clear its dirty-flag-A (coarse cell +0x08). List B
// (graph+0x98/+0xa0): for each cell call a7480 (adjacency rebuild only) and clear dirty-
// flag-B (+0x09). Then clear both dirty lists (1400021d0(,0)) and call 1d830 on the cell-BST
// at graph+8 with graph+0x70 (region count) to size it. Calls a80a0, a7480, 1400021d0, 1d830.
void Pf_FlushDirtyChunks_8a90(int64_t param_1) {
    EnsureFns();
    if ((0 < *(int*)(param_1 + 0x90)) || (0 < *(int*)(param_1 + 0xa0))) {
        uint64_t uVar5 = 0;
        uint64_t uVar3 = uVar5;
        uint64_t uVar4 = uVar5;
        uint32_t uVar2;
        if (0 < *(int*)(param_1 + 0x90)) {
            do {
                Pf_RebuildRegionChunk_80a0(param_1, *(uint32_t*)(uVar4 + *(int64_t*)(param_1 + 0x88)));
                uVar2 = (int)uVar3 + 1;
                *(uint8_t*)((int64_t)*(int*)(uVar4 + *(int64_t*)(param_1 + 0x88)) * 0x20 + 8 +
                            *(int64_t*)(param_1 + 0x78)) = 0;
                uVar3 = (uint64_t)uVar2;
                uVar4 = uVar4 + 4;
            } while ((int)uVar2 < *(int*)(param_1 + 0x90));
        }
        uVar3 = uVar5;
        if (0 < *(int*)(param_1 + 0xa0)) {
            do {
                Pf_RegionRebuildAdjacency_7480(param_1, *(int*)(uVar5 + *(int64_t*)(param_1 + 0x98)));
                uVar2 = (int)uVar3 + 1;
                int* piVar1 = (int*)(uVar5 + *(int64_t*)(param_1 + 0x98));
                uVar5 = uVar5 + 4;
                *(uint8_t*)((int64_t)*piVar1 * 0x20 + 9 + *(int64_t*)(param_1 + 0x78)) = 0;
                uVar3 = (uint64_t)uVar2;
            } while ((int)uVar2 < *(int*)(param_1 + 0xa0));
        }
        s_ArrResize4((int64_t*)(param_1 + 0x88), 0);
        s_ArrResize4((int64_t*)(param_1 + 0x98), 0);
        s_ArrResize18((int64_t*)(param_1 + 8), *(int*)(param_1 + 0x70));
    }
}

// ============================================================
// 0x1400a8ba0 — Pf_RegionPathLength_8ba0(graph, start, goal, ctx) -> length
// ============================================================
// First flush any dirty region chunks (a8a90) if dirty, then run the A* helper 1dc00 into a
// LARGE local int array local_8008[8190] (32KB-ish stack buffer — declared at full size per
// the brief), which returns the node count of the resulting region path. Sum the Euclidean
// edge lengths between successive region-node centroids (node array at graph+0x68, stride
// 0x30; centroid x=+0x00, y=+0x04) via sqrtf (140132010), accumulating into an int. Returns
// the summed integer length (0 if path < 2 nodes).
//
// FAITHFULNESS / STACK: local_8008 is declared as int[8190] to match the decompile's
// auStack frame exactly (do NOT shrink). __chkstk/alloca_probe is the compiler's stack-probe
// injection and needs no port.
//
// CLOSURE WARNING (lead): 0x14001dc00 (BuildStructure_helper_dc00) is NOT registered (it
// itself calls 0x14001dcd0). This port is complete but a8ba0 must NOT be registered until
// dc00 + dcd0 land — see reg_pf_region.cpp.
int Pf_RegionPathLength_8ba0(int* param_1, int param_2, uint32_t param_3, uint64_t param_4) {
    EnsureFns();
    int local_8008[8190];
    if ((0 < param_1[0x24]) || (0 < param_1[0x28])) {
        Pf_FlushDirtyChunks_8a90((int64_t)param_1);
    }
    int iVar2 = s_BuildPath_dc00(param_1, param_2, param_3, param_4, (int64_t)local_8008);
    int iVar3;
    if (iVar2 < 2) {
        iVar3 = 0;
    } else {
        iVar3 = 0;
        int64_t lVar5 = 0;
        if (0 < iVar2 + -1) {
            int64_t lVar1 = *(int64_t*)(param_1 + 0x1a);
            do {
                int iVar4 = *(int*)(lVar1 + 4 + (int64_t)local_8008[lVar5] * 0x30) -
                            *(int*)(lVar1 + 4 + (int64_t)local_8008[lVar5 + 1] * 0x30);
                int iVar6 = *(int*)(lVar1 + (int64_t)local_8008[lVar5] * 0x30) -
                            *(int*)(lVar1 + (int64_t)local_8008[lVar5 + 1] * 0x30);
                float fVar7 = s_Sqrtf((float)(iVar4 * iVar4 + iVar6 * iVar6));
                lVar5 = lVar5 + 1;
                iVar3 = iVar3 + (int)fVar7;
            } while (lVar5 < iVar2 + -1);
        }
    }
    return iVar3;
}

// ============================================================
// 0x1400a5b30 — Pf_RegionPairCacheInsert_5b30(cache, regionA, regionB, value)
// ============================================================
// Insert/update a region-pair -> value entry in a fixed-size ring buffer + BST cache.
// cache = undefined8*: [5] = pair-BST root, [3] = ring buffer base (stride 0x18:
// +0x00 keyA, +0x04 keyB, +0x08 value, +0x0c.. links), [2] int = ring write head,
// (long+0x14) int = ring tail, [4] int = ring capacity, [0] = seq/prev link.
// First normalize (a,b) so a>=b, look it up in the BST (left=+6 dword-units, right=+8): if
// found, just overwrite its value (+0x08 of its ring slot via slot index piVar4[2]) and
// return. Else advance the ring head (wrap at capacity); if head == tail, evict the tail
// slot from the BST (ca60) and bump tail. Write the new slot, link prev via cache[0], and
// insert into the BST (c940). Calls ca60 (registered) + c940 (registered).
//
// Faithfulness traps: param_1 is undefined8*, but several fields are int (+0x10 = param_1+2
// dword 0 = ring head; +0x14 = (char*)param_1+0x14 = ring tail; +0x20 = param_1+4 = cap).
// The (a,b) normalization (swap so a>=b) is applied THREE times exactly as decompiled
// (lookup key, slot store, c940 insert key). uStack_3c is an uninitialized pad qword the
// decompiler reads into the high half of two CONCAT44 stores; transcribed as a local pad.
void Pf_RegionPairCacheInsert_5b30(uint64_t* param_1, int param_2, int param_3, uint32_t param_4) {
    EnsureFns();
    uint32_t uStack_3c = 0; // decompiler-visible pad (high half of CONCAT44 stores)
    int iVar7 = param_2;
    int iVar8 = param_3;
    if (param_2 < param_3) {
        iVar7 = param_3;
        iVar8 = param_2;
    }
    int* piVar6 = (int*)param_1[5];
    int* piVar4 = piVar6;
    while (piVar4 != (int*)0x0) {
        int iVar2 = *piVar4;
        if (iVar7 < iVar2) {
LAB_1400a5b88:
            piVar4 = *(int**)(piVar4 + 6);
        } else {
            if (iVar7 <= iVar2) {
                if (iVar8 < piVar4[1]) goto LAB_1400a5b88;
                if ((iVar7 <= iVar2) && ((iVar7 < iVar2 || (iVar8 <= piVar4[1])))) {
                    if (piVar4 != (int*)0x0) {
                        *(uint32_t*)(param_1[3] + 8 + (int64_t)piVar4[2] * 0x18) = param_4;
                        return;
                    }
                    break;
                }
            }
            piVar4 = *(int**)(piVar4 + 8);
        }
    }
    iVar7 = *(int*)(param_1 + 2);
    iVar8 = iVar7 + 1;
    if (*(int*)(param_1 + 4) <= iVar7 + 1) {
        iVar8 = 0;
    }
    *(int*)(param_1 + 2) = iVar8;
    if (*(int*)(param_1 + 2) == *(int*)((int64_t)param_1 + 0x14)) {
        piVar4 = (int*)(param_1[3] + (int64_t)*(int*)((int64_t)param_1 + 0x14) * 0x18);
        if (piVar6 != (int*)0x0) {
            int iVar8b = *piVar4;
            do {
                int iVar2 = *piVar6;
                if (iVar8b < iVar2) {
LAB_1400a5c1b:
                    piVar6 = *(int**)(piVar6 + 6);
                } else {
                    if (iVar8b <= iVar2) {
                        int iVar3 = piVar4[1];
                        if (iVar3 < piVar6[1]) goto LAB_1400a5c1b;
                        if ((iVar8b <= iVar2) && ((iVar8b < iVar2 || (iVar3 <= piVar6[1])))) {
                            s_CellBstRemove((uintptr_t*)(param_1 + 5), piVar6);
                            break;
                        }
                    }
                    piVar6 = *(int**)(piVar6 + 8);
                }
            } while (piVar6 != (int*)0x0);
        }
        uint64_t* puVar1 = (uint64_t*)(param_1[3] + (int64_t)*(int*)((int64_t)param_1 + 0x14) * 0x18);
        *puVar1 = 0xffffffffffffffff;
        puVar1[1] = ((uint64_t)uStack_3c << 32) | (uint64_t)0xffffffff;
        puVar1[2] = 0;
        *(int*)((int64_t)param_1 + 0x14) = *(int*)((int64_t)param_1 + 0x14) + 1;
        int iVar8c = *(int*)((int64_t)param_1 + 0x14);
        if (*(int*)(param_1 + 4) <= *(int*)((int64_t)param_1 + 0x14)) {
            iVar8c = 0;
        }
        *(int*)((int64_t)param_1 + 0x14) = iVar8c;
    }
    uint64_t local_48 = ((uint64_t)(uint32_t)param_3 << 32) | (uint64_t)(uint32_t)param_2;
    if (param_2 < param_3) {
        local_48 = ((uint64_t)(uint32_t)param_2 << 32) | (uint64_t)(uint32_t)param_3;
    }
    uint64_t uVar5 = *param_1;
    uint64_t* puVar1 = (uint64_t*)(param_1[3] + (int64_t)iVar7 * 0x18);
    *puVar1 = local_48;
    puVar1[1] = ((uint64_t)uStack_3c << 32) | (uint64_t)param_4;
    puVar1[2] = uVar5;
    int local_res8 = param_2;
    int iStackX_c = param_3;
    if (param_2 < param_3) {
        local_res8 = param_3;
        iStackX_c = param_2;
    }
    // local_res8 / iStackX_c are passed as the BST key pair (a>=b). local_48 is reused as the
    // value-key pair (a,b) for c940; local_40 = iVar7 = the ring slot index.
    struct { int local_res8; int iStackX_c; uint64_t local_48b; int local_40; uint32_t pad; } k;
    k.local_res8 = local_res8;
    k.iStackX_c = iStackX_c;
    k.local_48b = ((uint64_t)(uint32_t)iStackX_c << 32) | (uint64_t)(uint32_t)local_res8;
    k.local_40 = iVar7;
    s_BstInsertB((int64_t*)(param_1 + 5), (uint64_t*)&k.local_res8, (uint32_t*)&k.local_48b);
}
