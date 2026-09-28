// code/Game/Action/action_pf_astar.cpp
// Wave 3 (Action) — KEYSTONE batch: A* pathfinder core + worker-thread command
// queue/dispatcher + region flood-fill. Faithful 1:1 ports of the exe pathfinding
// closure. NONE of this is exercised on the autostart path, so a wrong transcription
// is undetectable silent corruption — every offset below is cross-checked against the
// Ghidra decompile AND disassembly. Do NOT "improve" or simplify.
#include "Action/exe_bodies.h"
//
// Registration point: RegisterPfAstarBatch() in standalone/reg_pf_astar.cpp.
//
// ============================================================================
// A* SEARCH-CONTEXT struct layout (the `param_1` of 1d040 / 1d320 / 1ce00),
// recovered from disassembly. Word offsets are `int*`-relative in the decompile;
// below are BYTE offsets.
//   +0x00 (int)   generation/visit marker (bumped by 1ce00 before each search)
//   +0x04 (int)   sequence counter (monotonic, used as AVL tie-break key)
//   +0x08 (ptr)   open-set NODE ARRAY base   (param_1[2]); node stride 0x18
//   +0x18 (ptr*)  AVL open-set ROOT family   (param_1[6] = AVL header @ +0x18):
//                    AVL header: +0x18 root, +0x1c size(int), +0x28(int) count2,
//                                +0x30 free-list head (param_1[0xc])
//   +0x20 (int)   open-set element count     (param_1[8])
//   +0x44 (int)   freed-back count           (param_1[0x11])
//   +0x48 (int)   recycled count             (param_1[0x12])
//   +0x68 (ptr)   free-list head             (param_1[0xc])  (node recycle chain)
//   +0xd0 (int)   neighbour cost weight base (param_1[0x14] + walkBits index)
//   +0xd4 (byte)  diagonal-allowed flag      (param_1[0x35])
//   +0xd5 (byte)  diagonal-cost mode flag    ((char)param_1+0xd5)
//   +0xd8 (int)   map width / row stride      (param_1[0x36])
//   +0xe0 (ptr)   walkability byte grid       (param_1[0x38])
//   +0x68 region:  region-graph base          (param_1[0x1a]) — for 1dcd0/1dc00
//
// Per-cell OPEN-SET NODE (array @ +0x08, stride 0x18):
//   +0x00 (ptr)  AVL node back-pointer (the open-set BST node for this cell)
//   +0x08 (int)  parent cell index   (-1 = none)
//   +0x0c (int)  closed/visited generation (== ctx+0x00 ⇒ in current search)
//   +0x10 (int)  open-set membership counter (DEC on relax/expand)
//   +0x14 (int)  g-cost (cost-so-far to this cell)
//
// AVL OPEN-SET NODE (allocated by 1d730, freed by 1d550→recycle):
//   +0x00 (cell-index payload word, also free-list link when recycled)
//   +0x04 (int)  seq tie-break key
//   +0x08 (int)  fcost sort key
//   +0x10 (ptr)  parent
//   +0x18 (ptr)  left child
//   +0x20 (ptr)  right child
//   +0x28 (short) balance factor
//   +0x2a (short) subtree height
//
// COMMAND-NODE layout (worker-thread queue; verified against a6fe0/a6a30 stores):
//   +0x00 (int) cmdType   +0x04 (int) arg1   +0x08 (int) arg2   +0x0c (int) arg3
//   +0x10 (ptr) next-link  +0x18 (ptr) prev-link
// Thread/queue struct: +0x08(byte) running-flags, +0x30 CriticalSection,
//   +0x5c(byte) on-worker-thread, +0x60 tail, +0x68 head, +0x70 count.
// ============================================================================

#include <windows.h>
#include "Work/exe_bodies.h"          // WorkImpl_Sqrtf_132010
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Component/coll_bridge.h"
#include "system/Thread.h"     // ENDGAME B.3.4b System callers
#include <cstdint>
#include <cstdio>    // [FLOODDIAG] diagnostic reporting
#include <cstdlib>

// Ghidra decompile uses `uint`/`ushort` for the unsigned grid-index math; alias them
// so the literal transcription below matches the decompile token-for-token.
typedef unsigned int  uint;
typedef unsigned short ushort;

namespace {

// ---- Exe sub-callees in our closure, called by name (B3R). ----
// 1d320 — A* relax/insert (THIS FILE). 1d550 — AVL delete (THIS FILE).
// 1d450 — AVL insert (THIS FILE). 1d040 — A* core (THIS FILE). 1ce00 — run+trace (THIS FILE).
// dcd0 — region A* (THIS FILE). dc00 — region run (THIS FILE).
//   0x14001d400 Pf_BstFreeRecursive_d400    0x14001d730 Pf_NodePoolAlloc30_d730
//   0x140013480 Pf_AvlRebalanceB_13480      0x1400a6030 Pf_MoveHeuristic_6030
//   0x1400a5ec0 Pf_NeighbourTile_5ec0       0x140132010 WorkImpl_Sqrtf_132010
//   0x14001a9b0 Pf_NodePoolAlloc18_a9b0     0x14001d940 Pf_VectorResize18Dup_d940
//   0x140152b60 ?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z -> System::MemoryAllocator::MemAlloc
// 0x1400021d0 (int-array SetSize) is the exe-side Collection::Array<int32>
// monomorphisation -- ENDGAME B.3.2 step 2 converted its one call site below to a
// GameColl::AsArray<GameColl::I32NoInit> member call, so its resolved-pointer stand-in
// is gone. I32NoInit, not I32: 0x1400021d0 copies only the OLD prefix and leaves a grown
// tail as the allocation found it.

} // namespace

// Forward declares} // namespace

// Forward declares — these are members of THIS closure that call each other.
void  Pf_AstarRelax_d320(int* ctx, int cell, uint32_t parent, uint32_t gcost, uint32_t fcost);
void  Pf_AvlDelete_d550(int64_t* avlHdr, int64_t* node);
int64_t* Pf_AvlInsert_d450(int64_t* avlHdr, int64_t* out, uint32_t* key);
uint64_t Pf_AstarCore_d040(int* ctx, int start, int goal);
uint64_t Pf_RegionAstarCore_dcd0(int* ctx, int start, uint32_t goal);
int   Pf_AstarRun_ce00(int* ctx, int start, int goal, void* ctxA, int64_t out);
int*  Pf_AllocTradeSlot_a6240(int64_t self, int* out);
void  Pf_WorkerApplyCmd_a6430(int64_t self, uint32_t* cmd, void* ctxA, void* ctxB);

// ============================================================
// 0x14001d550 — AVL open-set delete + rebalance + node recycle.
// (Ghidra: TradeDescription_impl_d550.) AVL header = avlHdr; node = the BST node
// to unlink. Standard AVL deletion (0/1/2-child), then rebalance via 13480, then
// the deleted node is pushed onto the header's free chain (avlHdr[3] @ +0x18 hdr).
// ============================================================
void Pf_AvlDelete_d550(int64_t* param_1, int64_t* param_2) {
    int64_t lVar1;
    int64_t lVar2;
    int64_t lVar3;
    short sVar4;
    int64_t* plVar5;
    short sVar6;
    short sVar7;
    int64_t local_res8;
    int64_t local_res10[3];

    local_res8 = param_2[2];
    *reinterpret_cast<int*>(param_1 + 1) = static_cast<int>(param_1[1]) + -1;
    lVar1 = param_2[4];
    if (lVar1 == 0) {
        lVar1 = param_2[2];
        lVar2 = param_2[3];
        if (lVar1 == 0) {
            *param_1 = lVar2;
        } else if (*reinterpret_cast<int64_t**>(lVar1 + 0x18) == param_2) {
            *reinterpret_cast<int64_t*>(lVar1 + 0x18) = lVar2;
        } else {
            *reinterpret_cast<int64_t*>(lVar1 + 0x20) = lVar2;
        }
        local_res10[0] = param_2[3];
    } else {
        lVar2 = param_2[3];
        lVar3 = lVar2;
        if (lVar2 != 0) {
            do {
                local_res10[0] = lVar3;
                lVar3 = *reinterpret_cast<int64_t*>(local_res10[0] + 0x20);
            } while (*reinterpret_cast<int64_t*>(local_res10[0] + 0x20) != 0);
            local_res8 = 0;
            if (local_res10[0] != lVar2) {
                lVar1 = *reinterpret_cast<int64_t*>(local_res10[0] + 0x18);
                local_res8 = *reinterpret_cast<int64_t*>(local_res10[0] + 0x10);
                *reinterpret_cast<int64_t*>(local_res8 + 0x20) = lVar1;
                if (lVar1 != 0) {
                    *reinterpret_cast<int64_t*>(lVar1 + 0x10) = local_res8;
                }
                *reinterpret_cast<int64_t*>(local_res10[0] + 0x18) = param_2[3];
                if (*reinterpret_cast<int64_t*>(local_res8 + 0x18) == 0) {
                    sVar7 = 0;
                } else {
                    sVar7 = *reinterpret_cast<short*>(*reinterpret_cast<int64_t*>(local_res8 + 0x18) + 0x2a) + 1;
                }
                if (*reinterpret_cast<int64_t*>(local_res8 + 0x20) == 0) {
                    sVar6 = 0;
                } else {
                    sVar6 = *reinterpret_cast<short*>(*reinterpret_cast<int64_t*>(local_res8 + 0x20) + 0x2a) + 1;
                }
                sVar4 = sVar6;
                if (sVar6 < sVar7) {
                    sVar4 = sVar7;
                }
                *reinterpret_cast<short*>(local_res8 + 0x28) = sVar6 - sVar7;
                *reinterpret_cast<short*>(local_res8 + 0x2a) = sVar4;
            }
            *reinterpret_cast<int64_t*>(local_res10[0] + 0x20) = param_2[4];
            if (*reinterpret_cast<int64_t*>(local_res10[0] + 0x18) != 0) {
                *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(local_res10[0] + 0x18) + 0x10) = local_res10[0];
            }
            if (*reinterpret_cast<int64_t*>(local_res10[0] + 0x20) != 0) {
                *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(local_res10[0] + 0x20) + 0x10) = local_res10[0];
            }
            lVar1 = param_2[2];
            *reinterpret_cast<int64_t*>(local_res10[0] + 0x10) = lVar1;
            if (param_2[2] == 0) {
                *param_1 = local_res10[0];
                *reinterpret_cast<int64_t*>(local_res10[0] + 0x10) = 0;
                local_res10[0] = *param_1;
            } else if (*reinterpret_cast<int64_t**>(lVar1 + 0x18) == param_2) {
                *reinterpret_cast<int64_t*>(lVar1 + 0x18) = local_res10[0];
            } else {
                *reinterpret_cast<int64_t*>(lVar1 + 0x20) = local_res10[0];
            }
            *reinterpret_cast<int*>(reinterpret_cast<char*>(param_1) + 0x2c) =
                *reinterpret_cast<int*>(reinterpret_cast<char*>(param_1) + 0x2c) + -1;
            *reinterpret_cast<int*>(param_1 + 6) = static_cast<int>(param_1[6]) + 1;
            *param_2 = param_1[3];
            param_1[3] = reinterpret_cast<int64_t>(param_2);
            plVar5 = &local_res8;
            if (local_res8 == 0) {
                plVar5 = local_res10;
            }
            Pf_AvlRebalanceB_13480(param_1, plVar5);
            return;
        }
        lVar2 = param_2[2];
        if (lVar2 == 0) {
            *param_1 = lVar1;
            local_res10[0] = param_2[4];
        } else if (*reinterpret_cast<int64_t**>(lVar2 + 0x18) == param_2) {
            *reinterpret_cast<int64_t*>(lVar2 + 0x18) = lVar1;
            local_res10[0] = param_2[4];
        } else {
            *reinterpret_cast<int64_t*>(lVar2 + 0x20) = lVar1;
            local_res10[0] = param_2[4];
        }
    }
    if (local_res10[0] != 0) {
        *reinterpret_cast<int64_t*>(local_res10[0] + 0x10) = param_2[2];
    }
    *reinterpret_cast<int*>(reinterpret_cast<char*>(param_1) + 0x2c) =
        *reinterpret_cast<int*>(reinterpret_cast<char*>(param_1) + 0x2c) + -1;
    *reinterpret_cast<int*>(param_1 + 6) = static_cast<int>(param_1[6]) + 1;
    *param_2 = param_1[3];
    param_1[3] = reinterpret_cast<int64_t>(param_2);
    if (local_res10[0] == 0) {
        Pf_AvlRebalanceB_13480(param_1, &local_res8);
        return;
    }
    Pf_AvlRebalanceB_13480(param_1, local_res10);
    (void)local_res8;
}

// ============================================================
// 0x14001d450 — AVL open-set insert (returns the matching/new node via `out`).
// (Ghidra: BuildStructure_helper_d450.) key = {cell,seq,fcost} (key[2]=fcost sort,
// key[1]=seq tie-break). Walks the tree; on empty subtree allocates via d730 and
// rebalances via 13480; on exact match returns existing.
// ============================================================
int64_t* Pf_AvlInsert_d450(int64_t* param_1, int64_t* param_2, uint32_t* param_3) {
    int iVar1;
    int iVar2;
    int64_t lVar3;
    int64_t lVar4;
    void* puVar5;
    int64_t local_res8;

    if (*param_1 == 0) {
        puVar5 = Pf_NodePoolAlloc30_d730(reinterpret_cast<uintptr_t>(param_1), param_3, 0);   // 0x14001d730
        *reinterpret_cast<int*>(param_1 + 1) = static_cast<int>(param_1[1]) + 1;
        *param_1 = reinterpret_cast<int64_t>(puVar5);
        *param_2 = reinterpret_cast<int64_t>(puVar5);
        return param_2;
    }
    iVar1 = static_cast<int>(param_3[2]);
    lVar4 = *param_1;
    do {
        while (local_res8 = lVar4, lVar3 = local_res8,
               iVar2 = *reinterpret_cast<int*>(local_res8 + 8),
               iVar1 < iVar2 ||
               (iVar1 <= iVar2 && *reinterpret_cast<int*>(local_res8 + 4) < static_cast<int>(param_3[1]))) {
            lVar4 = *reinterpret_cast<int64_t*>(local_res8 + 0x18);
            if (*reinterpret_cast<int64_t*>(local_res8 + 0x18) == 0) {
                puVar5 = Pf_NodePoolAlloc30_d730(reinterpret_cast<uintptr_t>(param_1), param_3, local_res8);   // 0x14001d730
                *reinterpret_cast<void**>(lVar3 + 0x18) = puVar5;
                goto LAB_14001d519;
            }
        }
        if (iVar1 <= iVar2 &&
            (iVar1 < iVar2 || *reinterpret_cast<int*>(local_res8 + 4) <= static_cast<int>(param_3[1]))) {
            *param_2 = local_res8;
            return param_2;
        }
        lVar4 = *reinterpret_cast<int64_t*>(local_res8 + 0x20);
    } while (*reinterpret_cast<int64_t*>(local_res8 + 0x20) != 0);
    puVar5 = Pf_NodePoolAlloc30_d730(reinterpret_cast<uintptr_t>(param_1), param_3, local_res8);   // 0x14001d730
    *reinterpret_cast<void**>(lVar3 + 0x20) = puVar5;
LAB_14001d519:
    Pf_AvlRebalanceB_13480(param_1, &local_res8);
    *reinterpret_cast<int*>(param_1 + 1) = static_cast<int>(param_1[1]) + 1;
    *param_2 = reinterpret_cast<int64_t>(puVar5);
    return param_2;
}

// ============================================================
// 0x14001d320 — A* relax: set the cell node's parent/g-cost, mark closed, then
// insert it into the open-set AVL keyed on f-cost. (Ghidra: ..._sub_d320.)
//   ctx+0x08 = node array (stride 0x18). plVar1 = &node[cell].
//   If node was already in the open set (node+0xc == ctx+0x00), find+delete its old
//   AVL entry first (the inner BST walk → 1d550).
// param_3 = parent cell, param_4 = g-cost, param_5 = f-cost.
// ============================================================
void Pf_AstarRelax_d320(int* param_1, int param_2, uint32_t param_3, uint32_t param_4, uint32_t param_5) {
    int64_t* plVar1;
    int iVar2;
    int iVar3;
    int64_t lVar4;
    int64_t* plVar5;
    int64_t local_res8;
    int local_24;
    // AVL key is a CONTIGUOUS 3-int buffer {cell(+0x00), seq(+0x04), fcost(+0x08)}:
    // d450 reads key[2]=fcost / key[1]=seq and d730 stores all three into the node.
    // The exe uses three consecutive stack slots (local_28/local_24/local_20); porting
    // those as separate C++ locals is the recurring "contiguous-locals gotcha" — GCC is
    // free to reorder/pad them, so &local_28[1]/[2] would read garbage → corrupted key
    // → spurious d450 match → two cells share one AVL node → free-list double-free.
    uint32_t keybuf[3];

    plVar1 = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 2) +
                                        static_cast<int64_t>(param_2) * 0x18);
    if (*reinterpret_cast<int*>(reinterpret_cast<char*>(plVar1) + 0xc) == *param_1) {
        plVar5 = *reinterpret_cast<int64_t**>(param_1 + 6);
        lVar4 = *plVar1;  // AVL node back-pointer for this cell
        if (plVar5 != nullptr) {
            iVar2 = *reinterpret_cast<int*>(lVar4 + 8);  // its fcost key
            do {
                iVar3 = static_cast<int>(plVar5[1]);
                if (iVar2 < iVar3 ||
                    (iVar2 <= iVar3 && *reinterpret_cast<int*>(reinterpret_cast<char*>(plVar5) + 4) < *reinterpret_cast<int*>(lVar4 + 4))) {
                    plVar5 = reinterpret_cast<int64_t*>(plVar5[3]);  // left child (+0x18)
                } else {
                    if (iVar2 <= iVar3 &&
                        (iVar2 < iVar3 || *reinterpret_cast<int*>(reinterpret_cast<char*>(plVar5) + 4) <= *reinterpret_cast<int*>(lVar4 + 4))) {
                        Pf_AvlDelete_d550(reinterpret_cast<int64_t*>(param_1 + 6), plVar5);
                        break;
                    }
                    plVar5 = reinterpret_cast<int64_t*>(plVar5[4]);  // right child (+0x20)
                }
            } while (plVar5 != nullptr);
        }
    }
    iVar2 = *param_1;
    *reinterpret_cast<int*>(plVar1 + 2) = static_cast<int>(plVar1[2]) + -1;  // node+0x10 --
    *reinterpret_cast<uint32_t*>(plVar1 + 1) = param_3;                      // node+0x08 = parent
    *reinterpret_cast<int*>(reinterpret_cast<char*>(plVar1) + 0xc) = iVar2;  // node+0x0c = generation
    *reinterpret_cast<uint32_t*>(reinterpret_cast<char*>(plVar1) + 0x14) = param_4; // node+0x14 = g
    local_24 = param_1[1];
    param_1[1] = local_24 + 1;
    keybuf[0] = static_cast<uint32_t>(param_2);       // cell   (node+0x00)
    keybuf[1] = static_cast<uint32_t>(local_24);      // seq    (node+0x04)
    keybuf[2] = param_5;                              // fcost  (node+0x08)
    plVar5 = Pf_AvlInsert_d450(reinterpret_cast<int64_t*>(param_1 + 6), &local_res8, keybuf);
    *plVar1 = *plVar5;  // store AVL node back-pointer into node+0x00
    (void)local_24;
}

// ============================================================
// 0x14001d040 — A* CORE over the grid graph. (Ghidra: FUN_14001d040.)
//   On entry frees a stale root AVL node (param_1[6][3]/[4] subtrees via d400),
//   seeds the open set with `start`, then loops: pop min-f open node (leftmost AVL),
//   if == goal return 1, else delete from open set, expand 4 or 8 neighbours,
//   compute tentative g (10 ortho / 14 diag * weight) + heuristic h, relax via d320.
// Returns bool-in-low-byte (caller 1ce00 tests (char)ret).
// ============================================================
uint64_t Pf_AstarCore_d040(int* param_1, int param_2, int param_3) {
    int64_t lVar1;
    int* piVar2;
    uint64_t* puVar3;
    int64_t lVar4;
    int64_t lVar5;
    int iVar6;
    int64_t* plVar7;
    int64_t* plVar8;
    int64_t* plVar9;
    int iVar10;
    int iVar11;
    int iVar12;
    uint64_t uVar13;
    uint uVar14;
    uint uVar15;
    uint uVar16;
    uint uVar17;
    int iVar18;
    int iVar19;
    int iVar20;
    int iVar21;

    puVar3 = *reinterpret_cast<uint64_t**>(param_1 + 6);
    if (puVar3 != nullptr) {
        Pf_BstFreeRecursive_d400(reinterpret_cast<uintptr_t>(param_1 + 6), reinterpret_cast<uint64_t*>(puVar3[3]));
        Pf_BstFreeRecursive_d400(reinterpret_cast<uintptr_t>(param_1 + 6), reinterpret_cast<uint64_t*>(puVar3[4]));
        param_1[0x11] = param_1[0x11] + -1;
        param_1[0x12] = param_1[0x12] + 1;
        *puVar3 = *reinterpret_cast<uint64_t*>(param_1 + 0xc);
        *reinterpret_cast<uint64_t**>(param_1 + 0xc) = puVar3;
    }
    param_1[6] = 0;
    param_1[7] = 0;
    param_1[8] = 0;
    param_1[1] = 0;
    iVar10 = Pf_MoveHeuristic_6030(reinterpret_cast<uintptr_t>(param_1), param_2, param_3);   // 0x1400a6030
    Pf_AstarRelax_d320(param_1, param_2, 0xffffffff, 0, iVar10);
    uVar13 = 0;  // d320's RAX-on-exit only ever flows to the masked (&...ff00) return below
    iVar10 = param_1[8];
    do {
        if (iVar10 < 1) {
            return uVar13 & 0xffffffffffffff00ULL;
        }
        plVar9 = *reinterpret_cast<int64_t**>(param_1 + 6);
        plVar8 = *reinterpret_cast<int64_t**>(param_1 + 6);
        while (plVar7 = plVar9, plVar7 != nullptr) {
            uVar13 = 0;
            plVar8 = plVar7;
            plVar9 = reinterpret_cast<int64_t*>(plVar7[3]);  // descend leftmost (+0x18)
        }
        iVar10 = static_cast<int>(*plVar8);  // cell index payload
        if (iVar10 == param_3) {
            return (uVar13 & 0xffffffffffffff00ULL) | 1;
        }
        Pf_AvlDelete_d550(reinterpret_cast<int64_t*>(param_1 + 6), plVar8);
        uVar14 = *reinterpret_cast<uint*>(param_1);
        uVar13 = uVar14;
        lVar1 = static_cast<int64_t>(iVar10) * 0x18;
        lVar4 = *reinterpret_cast<int64_t*>(param_1 + 2);
        piVar2 = reinterpret_cast<int*>(lVar4 + 0xc + lVar1);
        *piVar2 = *piVar2 + -1;
        *reinterpret_cast<uint*>(lVar4 + 0x10 + lVar1) = uVar14;
        iVar18 = 4;
        if (static_cast<char>(param_1[0x35]) != '\0') {
            iVar18 = 8;
        }
        iVar19 = 0;
        if (iVar18 != 0) {
            do {
                iVar11 = Pf_NeighbourTile_5ec0(reinterpret_cast<uintptr_t>(param_1), iVar10, iVar19);   // 0x1400a5ec0
                uVar13 = static_cast<uint64_t>(static_cast<uint32_t>(iVar11));
                if (-1 < iVar11) {
                    iVar21 = param_1[0x36];
                    uVar15 = iVar10 % iVar21 - iVar11 % iVar21;
                    uVar14 = iVar10 / iVar21 - iVar11 / iVar21;
                    uVar16 = static_cast<int>(uVar15) >> 0x1f;
                    uVar17 = static_cast<int>(uVar14) >> 0x1f;
                    uVar13 = static_cast<uint64_t>(*reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x38) + static_cast<int64_t>(iVar11)) & 0x7f);
                    if (((uVar15 ^ uVar16) - uVar16) + ((uVar14 ^ uVar17) - uVar17) == 1) {
                        iVar21 = param_1[static_cast<int>(uVar13) + 0x14] * 10;
                    } else {
                        iVar21 = param_1[static_cast<int>(uVar13) + 0x14] * 0xe;
                    }
                    lVar5 = *reinterpret_cast<int64_t*>(param_1 + 2);
                    iVar21 = *reinterpret_cast<int*>(lVar5 + 0x14 + lVar1) + iVar21;
                    lVar4 = static_cast<int64_t>(iVar11) * 0x18;
                    uVar13 = static_cast<uint64_t>(static_cast<uint>(*param_1));
                    if (*reinterpret_cast<uint*>(lVar4 + 0x10 + lVar5) == static_cast<uint>(*param_1)) {
                        if (*reinterpret_cast<int*>(lVar4 + 0x14 + lVar5) <= iVar21) goto LAB_14001d298;
                        piVar2 = reinterpret_cast<int*>(lVar4 + 0x10 + lVar5);
                        *piVar2 = *piVar2 + -1;
                    }
                    uVar13 = static_cast<uint64_t>(static_cast<uint>(*param_1));
                    if ((*reinterpret_cast<uint*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 0xc + lVar4) != static_cast<uint>(*param_1)) ||
                        (iVar21 < *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 0x14 + lVar4))) {
                        iVar12 = param_1[0x36];
                        uVar15 = iVar11 % iVar12 - param_3 % iVar12;
                        uVar16 = iVar11 / iVar12 - param_3 / iVar12;
                        uVar14 = static_cast<int>(uVar15) >> 0x1f;
                        iVar20 = (uVar15 ^ uVar14) - uVar14;
                        uVar14 = static_cast<int>(uVar16) >> 0x1f;
                        iVar12 = (uVar16 ^ uVar14) - uVar14;
                        if (static_cast<char>(param_1[0x35]) == '\0') {
                            if (*reinterpret_cast<char*>(reinterpret_cast<char*>(param_1) + 0xd5) == '\0') {
                                if (iVar12 < iVar20) {
                                    iVar12 = iVar20;
                                }
                            } else {
                                iVar12 = iVar12 + iVar20;
                            }
                            iVar12 = iVar12 * 10;
                        } else {
                            iVar6 = iVar12;
                            if (iVar20 < iVar12) {
                                iVar6 = iVar20;
                            }
                            iVar12 = (iVar12 + iVar20) * 10 + iVar6 * -6;
                        }
                        Pf_AstarRelax_d320(param_1, iVar11, iVar10, iVar21, iVar12 + iVar21);
                    }
                }
LAB_14001d298:
                iVar19 = iVar19 + 1;
            } while (iVar19 < iVar18);
        }
        iVar10 = param_1[8];
    } while (true);
}

// ============================================================
// 0x14001ce00 — run A* + reconstruct path. (Ghidra: FUN_14001ce00.)
//   Bumps generation (*param_1), runs the core; on success and if out-array given,
//   counts the parent chain length from goal, then walks parent links
//   (node+0x08 @ stride 0x18) writing cell indices into param_5 (cap 0x4000),
//   reversed (so index 0 = start). Returns path length (clamped to 0x4000).
// ============================================================
int Pf_AstarRun_ce00(int* param_1, int param_2, int param_3, void* param_4, int64_t param_5) {
    int64_t lVar1;
    int iVar2;
    uint64_t uVar3;
    int64_t lVar4;
    int iVar5;

    *param_1 = *param_1 + 1;
    uVar3 = Pf_AstarCore_d040(param_1, param_2, param_3);
    if (static_cast<char>(uVar3) == '\0') {
        return 0;
    }
    iVar5 = 0;
    if (param_5 != 0) {
        if (param_3 != -1) {
            iVar2 = param_3;
            do {
                iVar5 = iVar5 + 1;
                iVar2 = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 8 + static_cast<int64_t>(iVar2) * 0x18);
            } while (iVar2 != -1);
        }
        lVar4 = static_cast<int64_t>(iVar5 + -1);
        if (-1 < iVar5 + -1) {
            lVar1 = *reinterpret_cast<int64_t*>(param_1 + 2);
            do {
                if (lVar4 < 0x4000) {
                    *reinterpret_cast<int*>(param_5 + lVar4 * 4) = param_3;
                }
                lVar4 = lVar4 + -1;
                param_3 = *reinterpret_cast<int*>(lVar1 + 8 + static_cast<int64_t>(param_3) * 0x18);
            } while (-1 < lVar4);
        }
        if (0x4000 < iVar5) {
            iVar5 = 0x4000;
        }
        return iVar5;
    }
    if (param_3 != -1) {
        do {
            iVar5 = iVar5 + 1;
            param_3 = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 8 + static_cast<int64_t>(param_3) * 0x18);
        } while (param_3 != -1);
    }
    (void)param_4;
    return iVar5;
}

// ============================================================
// 0x14001dcd0 — A* over the REGION graph (Dijkstra w/ Euclidean h). (Ghidra: ..._dcd0.)
//   Same open-set AVL machinery as 1d040, but the graph is the region adjacency list
//   at param_1[0x1a] (region node stride 0x30: +0x00 x, +0x04 y, +0x10 neighbour
//   array ptr, +0x18 neighbour count). h = sqrtf(dx^2+dy^2) via 132010.
// ============================================================
uint64_t Pf_RegionAstarCore_dcd0(int* param_1, int param_2, uint param_3) {
    int* piVar1;
    uint64_t* puVar2;
    int64_t* plVar3;
    int64_t lVar4;
    int64_t lVar5;
    int64_t* plVar6;
    int64_t* plVar7;
    int iVar8;
    int iVar9;
    uint64_t uVar10;
    uint64_t uVar11;
    int iVar12;
    int iVar13;
    int64_t lVar14;
    int64_t lVar15;
    int iVar16;
    int64_t lVar17;
    uint64_t uVar18;
    int64_t lVar19;
    float fVar20;

    puVar2 = *reinterpret_cast<uint64_t**>(param_1 + 6);
    piVar1 = param_1 + 6;
    uVar18 = static_cast<uint64_t>(static_cast<int>(param_3));
    if (puVar2 != nullptr) {
        Pf_BstFreeRecursive_d400(reinterpret_cast<uintptr_t>(piVar1), reinterpret_cast<uint64_t*>(puVar2[3]));
        Pf_BstFreeRecursive_d400(reinterpret_cast<uintptr_t>(piVar1), reinterpret_cast<uint64_t*>(puVar2[4]));
        param_1[0x11] = param_1[0x11] + -1;
        param_1[0x12] = param_1[0x12] + 1;
        *puVar2 = *reinterpret_cast<uint64_t*>(param_1 + 0xc);
        *reinterpret_cast<uint64_t**>(param_1 + 0xc) = puVar2;
    }
    piVar1[0] = 0;
    piVar1[1] = 0;
    param_1[8] = 0;
    lVar14 = *reinterpret_cast<int64_t*>(param_1 + 0x1a);
    param_1[1] = 0;
    lVar17 = uVar18 * 0x30;
    iVar8 = *reinterpret_cast<int*>(lVar14 + 4 + static_cast<int64_t>(param_2) * 0x30) - *reinterpret_cast<int*>(lVar17 + 4 + lVar14);
    iVar12 = *reinterpret_cast<int*>(lVar14 + static_cast<int64_t>(param_2) * 0x30) - *reinterpret_cast<int*>(lVar17 + lVar14);
    fVar20 = __builtin_sqrtf(static_cast<float>(iVar8 * iVar8 + iVar12 * iVar12));   // PERF: 0x140132010 is our sqrtf; bit-identical inline
    Pf_AstarRelax_d320(param_1, param_2, 0xffffffff, 0, static_cast<int>(fVar20));
    uVar10 = 0;  // d320's RAX-on-exit only ever flows to the masked (&...ff00) return below
    iVar8 = param_1[8];
    do {
        if (iVar8 < 1) {
            return uVar10 & 0xffffffffffffff00ULL;
        }
        plVar3 = *reinterpret_cast<int64_t**>(param_1 + 6);
        plVar7 = plVar3;
        while (plVar6 = plVar3, plVar6 != nullptr) {
            uVar10 = 0;
            plVar7 = plVar6;
            plVar3 = reinterpret_cast<int64_t*>(plVar6[3]);
        }
        iVar8 = static_cast<int>(*plVar7);
        lVar14 = static_cast<int64_t>(iVar8);
        if (iVar8 == static_cast<int>(uVar18)) {
            return (uVar10 & 0xffffffffffffff00ULL) | 1;
        }
        Pf_AvlDelete_d550(reinterpret_cast<int64_t*>(param_1 + 6), plVar7);
        iVar12 = *param_1;
        lVar4 = *reinterpret_cast<int64_t*>(param_1 + 2);
        lVar15 = 0;
        piVar1 = reinterpret_cast<int*>(lVar4 + 0xc + lVar14 * 0x18);
        *piVar1 = *piVar1 + -1;
        *reinterpret_cast<int*>(lVar4 + 0x10 + lVar14 * 0x18) = iVar12;
        uVar11 = static_cast<uint64_t>(*reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 0x1a) + 0x18 + lVar14 * 0x30));
        uVar10 = uVar11;
        if (0 < static_cast<int64_t>(uVar11)) {
            do {
                lVar4 = *reinterpret_cast<int64_t*>(param_1 + 0x1a);
                uVar10 = *reinterpret_cast<uint64_t*>(lVar4 + 0x10 + lVar14 * 0x30);
                iVar12 = *reinterpret_cast<int*>(uVar10 + lVar15 * 4);
                lVar19 = static_cast<int64_t>(iVar12);
                if (-1 < iVar12) {
                    lVar5 = *reinterpret_cast<int64_t*>(param_1 + 2);
                    iVar9 = *reinterpret_cast<int*>(lVar4 + 4 + lVar14 * 0x30) - *reinterpret_cast<int*>(lVar4 + 4 + lVar19 * 0x30);
                    iVar13 = *reinterpret_cast<int*>(lVar4 + lVar14 * 0x30) - *reinterpret_cast<int*>(lVar4 + lVar19 * 0x30);
                    fVar20 = __builtin_sqrtf(static_cast<float>(iVar9 * iVar9 + iVar13 * iVar13));   // PERF: inline, bit-identical
                    iVar9 = static_cast<int>(fVar20) + *reinterpret_cast<int*>(lVar5 + 0x14 + lVar14 * 0x18);
                    uVar10 = static_cast<uint64_t>(static_cast<uint>(*param_1));
                    if (*reinterpret_cast<uint*>(lVar5 + 0x10 + lVar19 * 0x18) == static_cast<uint>(*param_1)) {
                        if (*reinterpret_cast<int*>(lVar5 + 0x14 + lVar19 * 0x18) <= iVar9) goto LAB_14001def2;
                        piVar1 = reinterpret_cast<int*>(lVar5 + 0x10 + lVar19 * 0x18);
                        *piVar1 = *piVar1 + -1;
                    }
                    uVar10 = static_cast<uint64_t>(static_cast<uint>(*param_1));
                    if ((*reinterpret_cast<uint*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 0xc + lVar19 * 0x18) != static_cast<uint>(*param_1)) ||
                        (iVar9 < *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 0x14 + lVar19 * 0x18))) {
                        lVar4 = *reinterpret_cast<int64_t*>(param_1 + 0x1a);
                        iVar13 = *reinterpret_cast<int*>(lVar4 + 4 + lVar19 * 0x30) - *reinterpret_cast<int*>(lVar17 + 4 + lVar4);
                        iVar16 = *reinterpret_cast<int*>(lVar4 + lVar19 * 0x30) - *reinterpret_cast<int*>(lVar17 + lVar4);
                        fVar20 = WorkImpl_Sqrtf_132010(static_cast<float>(iVar13 * iVar13 + iVar16 * iVar16));   // 0x140132010
                        Pf_AstarRelax_d320(param_1, iVar12, iVar8, iVar9, static_cast<int>(fVar20) + iVar9);
                        uVar10 = 0;  // d320 return only used as dead loop accumulator
                    }
                }
LAB_14001def2:
                lVar15 = lVar15 + 1;
            } while (lVar15 < static_cast<int64_t>(uVar11));
            uVar18 = static_cast<uint64_t>(param_3);
        }
        iVar8 = param_1[8];
    } while (true);
}

// ============================================================
// 0x14001dc00 — run region A* + extract region path list. (Ghidra: ..._dc00.)
//   Mirror of 1ce00 but on the region graph; path cap is 0x2000 (not 0x4000).
// ============================================================
int Pf_RegionAstarRun_dc00(int* param_1, int param_2, uint param_3, void* param_4, int64_t param_5) {
    int64_t lVar1;
    uint uVar2;
    uint64_t uVar3;
    int64_t lVar4;
    int iVar5;

    *param_1 = *param_1 + 1;
    uVar3 = Pf_RegionAstarCore_dcd0(param_1, param_2, param_3);
    if (static_cast<char>(uVar3) == '\0') {
        return 0;
    }
    iVar5 = 0;
    if (param_5 != 0) {
        if (param_3 != 0xffffffff) {
            uVar2 = param_3;
            do {
                iVar5 = iVar5 + 1;
                uVar2 = *reinterpret_cast<uint*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 8 + static_cast<int64_t>(static_cast<int>(uVar2)) * 0x18);
            } while (uVar2 != 0xffffffff);
        }
        lVar4 = static_cast<int64_t>(iVar5 + -1);
        if (-1 < iVar5 + -1) {
            lVar1 = *reinterpret_cast<int64_t*>(param_1 + 2);
            do {
                if (lVar4 < 0x2000) {
                    *reinterpret_cast<uint*>(param_5 + lVar4 * 4) = param_3;
                }
                lVar4 = lVar4 + -1;
                param_3 = *reinterpret_cast<uint*>(lVar1 + 8 + static_cast<int64_t>(static_cast<int>(param_3)) * 0x18);
            } while (-1 < lVar4);
        }
        if (0x2000 < iVar5) {
            iVar5 = 0x2000;
        }
        return iVar5;
    }
    if (param_3 != 0xffffffff) {
        do {
            iVar5 = iVar5 + 1;
            param_3 = *reinterpret_cast<uint*>(*reinterpret_cast<int64_t*>(param_1 + 2) + 8 + static_cast<int64_t>(static_cast<int>(param_3)) * 0x18);
        } while (param_3 != 0xffffffff);
    }
    (void)param_4;
    return iVar5;
}

// ============================================================
// WORKER-THREAD COMMAND DISPATCHER + ENQUEUERS
// ============================================================

// 0x1400a6430 — apply a queued command directly (executed on the worker thread).
// (Ghidra: FUN_1400a6430.) switch(cmd[0]) 0..5:
//   0: self+0x148 = cmd[1]
//   1: self+0xc8 + cmd[1]*4 = cmd[2]
//   2: (self+0x158)[cmd[1]] = keep-hi-bit | cmd[2]
//   3: (self+0x158)[cmd[1]] = (cmd[2]?0x80:0) | keep-lo-7bits
//   4: self+0x14c=cmd[1]!=0, +0x14d=cmd[2]!=0, +0x14e=cmd[3]!=0
//   5: run A* path (1ce00) into a 64KB local int buffer, then under CS copy the
//      result into the trade-route result slot list (self+0x160) and flag it ready.
// param_3/param_4 = the (gameworld, time) context forwarded to 1ce00.
// CRITICAL: the 0x10000-byte (16382-int) stack buffer is preserved verbatim.
void Pf_WorkerApplyCmd_a6430(int64_t param_1, uint32_t* param_2, void* param_3, void* param_4) {
    int iVar1;
    int iVar2;
    uint8_t* pbVar3;
    int64_t lVar4;
    int64_t lVar5;
    static uint32_t auStack_10008[16382];  // 64KB — must NOT shrink (see brief)

    switch (param_2[0]) {
    case 0:
        *reinterpret_cast<uint32_t*>(param_1 + 0x148) = param_2[1];
        return;
    case 1:
        *reinterpret_cast<uint32_t*>(param_1 + 200 + static_cast<int64_t>(static_cast<int>(param_2[1])) * 4) = param_2[2];
        return;
    case 2:
        pbVar3 = reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x158) + static_cast<int64_t>(static_cast<int>(param_2[1])));
        *pbVar3 = static_cast<uint8_t>((*pbVar3 & 0x80) | (*reinterpret_cast<uint8_t*>(param_2 + 2)));
        return;
    case 3:
        pbVar3 = reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x158) + static_cast<int64_t>(static_cast<int>(param_2[1])));
        *pbVar3 = static_cast<uint8_t>((static_cast<uint8_t>(-(param_2[2] != 0)) & 0x80U) | (*pbVar3 & 0x7f));
        return;
    case 4:
        iVar2 = param_2[2];
        iVar1 = param_2[1];
        *reinterpret_cast<bool*>(param_1 + 0x14e) = param_2[3] != 0;
        *reinterpret_cast<bool*>(param_1 + 0x14d) = iVar2 != 0;
        *reinterpret_cast<bool*>(param_1 + 0x14c) = iVar1 != 0;
        return;
    case 5:
        iVar2 = Pf_AstarRun_ce00(reinterpret_cast<int*>(param_1 + 0x78), param_2[1], param_2[2],
                                 param_4, reinterpret_cast<int64_t>(auStack_10008));
        // Hold the pathfinder CS (self+0x30) across the ENTIRE result commit — the array
        // resize, the cell write loop, AND the ready-byte store. The original released the
        // lock after the resize and wrote the cells unlocked; that is safe only if no other
        // thread touches the self+0x160 result table meanwhile. It is NOT safe here: the
        // main thread's slot allocator (Pf_AllocTradeSlot_a6240) GROWS/reallocates that same
        // table under the same CS, freeing the old buffer — which the allocator then hands
        // back out as a 0x28 EntityTreeNode. With the write loop unlocked, this worker thread
        // wrote path cells (cell-index values) into that freed-then-reused node, corrupting
        // the post-update entity tree (the intermittent TreeSuccessor/PostUpdate AV). Keeping
        // the lock held for the whole commit makes the worker's write and a6240's grow
        // mutually exclusive. Extra hold time is negligible (the expensive A* already ran
        // outside the lock) and widening a lock can only remove races, never add them.
        reinterpret_cast<System::CriticalSection*>(param_1 + 0x30)->Lock();   // B.3.4b bridge
        // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize. I32NoInit —
        // the grown tail stays as the allocation found it, as retail's helper leaves it
        // (1400a6582). Safe: the cell loop immediately below writes all iVar2 elements
        // before the ready byte at +0x160[slot] lets any reader touch them.
        GameColl::AsArray<GameColl::I32NoInit>(reinterpret_cast<void*>(
                *reinterpret_cast<int64_t*>(param_1 + 0x160) +
                (static_cast<uint64_t>(static_cast<uint>(param_2[3])) * 3 + 1) * 8))
            .SetSize(iVar2);
        lVar4 = 0;
        if (0 < static_cast<int64_t>(iVar2)) {
            do {
                lVar5 = lVar4 + 1;
                *reinterpret_cast<uint32_t*>(*reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x160) + 8 +
                        static_cast<uint64_t>(static_cast<uint>(param_2[3])) * 0x18) + -4 + lVar5 * 4) = auStack_10008[lVar4];
                lVar4 = lVar5;
            } while (lVar5 < iVar2);
        }
        *reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x160) +
                static_cast<uint64_t>(static_cast<uint>(param_2[3])) * 0x18) = 1;
        reinterpret_cast<System::CriticalSection*>(param_1 + 0x30)->Unlock();   // B.3.4b bridge
        return;
    }
    (void)param_3;
}

// ---- Shared enqueue helper: link a freshly-built 0x20-byte command node onto the
// tail of the worker queue under the CS, then start the thread if not running.
// Mirrors the identical tail in a6fe0/a68b0/a6a30/a6b20/a6c10 EXACTLY.
namespace {
static void EnqueueNode(int64_t self, uint32_t* localCmd) {
    reinterpret_cast<System::CriticalSection*>(self + 0x30)->Lock();   // B.3.4b bridge
    uint32_t* node = reinterpret_cast<uint32_t*>(System::MemoryAllocator::MemAlloc(0x20, 8));
    if (node != nullptr) {
        node[0] = localCmd[0];
        node[1] = localCmd[1];
        node[2] = localCmd[2];
        node[3] = localCmd[3];
        *reinterpret_cast<uint64_t*>(node + 4) = 0;  // +0x10 next
        *reinterpret_cast<uint64_t*>(node + 6) = 0;  // +0x18 prev
    }
    if (*reinterpret_cast<int64_t*>(self + 0x60) == 0) {
        *reinterpret_cast<uint32_t**>(self + 0x68) = node;
    } else {
        *reinterpret_cast<uint32_t**>(*reinterpret_cast<int64_t*>(self + 0x60) + 0x10) = node;
        *reinterpret_cast<uint64_t*>(node + 6) = *reinterpret_cast<uint64_t*>(self + 0x60);
    }
    *reinterpret_cast<int*>(self + 0x70) = *reinterpret_cast<int*>(self + 0x70) + 1;
    *reinterpret_cast<uint32_t**>(self + 0x60) = node;
    reinterpret_cast<System::CriticalSection*>(self + 0x30)->Unlock();   // B.3.4b bridge
    if ((*reinterpret_cast<uint8_t*>(self + 8) & 1) == 0) {
        reinterpret_cast<System::Thread*>(self)->Start();   // B.3.4b bridge
    }
}
} // namespace

// 0x1400a6fe0 — enqueue cmd type 2 (set walk-cost byte field). (Ghidra: FUN_1400a6fe0.)
void Pf_EnqueueCmd2_a6fe0(int64_t param_1, uint32_t param_2, void* param_3, void* param_4) {
    uint32_t local[4];
    local[0] = 2;
    local[1] = param_2;
    local[2] = static_cast<uint32_t>(reinterpret_cast<uint64_t>(param_3));
    local[3] = 0;
    if (*reinterpret_cast<uint8_t*>(param_1 + 0x5c) == 0) {
        EnqueueNode(param_1, local);
    } else {
        Pf_WorkerApplyCmd_a6430(param_1, local, param_3, param_4);
    }
}

// 0x1400a68b0 — enqueue cmd type 4 (3 bool flags). (Ghidra: RoadTool_sub_ce00_sub_68b0.)
//   Only enqueues if any of the three byte fields at self+0x1f4/0x1f5/0x1f6 changed.
void Pf_EnqueueCmd4_a68b0(int64_t param_1, uint8_t param_2, void* param_3, void* param_4) {
    uint8_t TVar6 = static_cast<uint8_t>(reinterpret_cast<uint64_t>(param_4));
    uint8_t TVar5 = static_cast<uint8_t>(reinterpret_cast<uint64_t>(param_3));
    uint8_t* pSelf = reinterpret_cast<uint8_t*>(param_1);
    if (pSelf[500] != param_2 || pSelf[0x1f5] != TVar5 || pSelf[0x1f6] != TVar6) {
        pSelf[500] = param_2;
        pSelf[0x1f5] = TVar5;
        pSelf[0x1f6] = TVar6;
        uint32_t local[4];
        local[0] = 4;
        local[1] = static_cast<uint32_t>(param_2 != 0);
        local[2] = static_cast<uint32_t>(TVar5 != 0);
        local[3] = static_cast<uint32_t>(TVar6 != 0);
        if (pSelf[0x5c] == 0) {
            EnqueueNode(param_1, local);
        } else {
            Pf_WorkerApplyCmd_a6430(param_1, local, param_3, param_4);
        }
    }
}

// 0x1400a6a30 — enqueue cmd type 1 (set per-index int field self+0x170[idx]).
// (Ghidra: FUN_1400a6a30.) Only if changed.
void Pf_EnqueueCmd1_a6a30(int64_t param_1, int param_2, void* param_3, void* param_4) {
    int iVar3 = static_cast<int>(reinterpret_cast<uint64_t>(param_3));
    if (*reinterpret_cast<int*>(param_1 + static_cast<int64_t>(param_2) * 4 + 0x170) != iVar3) {
        *reinterpret_cast<int*>(param_1 + static_cast<int64_t>(param_2) * 4 + 0x170) = iVar3;
        uint32_t local[4];
        local[0] = 1;
        local[1] = static_cast<uint32_t>(param_2);
        local[2] = static_cast<uint32_t>(iVar3);
        local[3] = 0;
        if (*reinterpret_cast<uint8_t*>(param_1 + 0x5c) == 0) {
            EnqueueNode(param_1, local);
        } else {
            Pf_WorkerApplyCmd_a6430(param_1, local, param_3, param_4);
        }
    }
}

// 0x1400a6b20 — enqueue cmd type 0 (set self+0x148-equivalent self+0x1f0). (Ghidra: FUN_1400a6b20.)
//   Only if changed. NOTE: the original branches the "on-worker-thread" case FIRST.
void Pf_EnqueueCmd0_a6b20(int64_t param_1, int param_2, void* param_3, void* param_4) {
    if (*reinterpret_cast<int*>(param_1 + 0x1f0) != param_2) {
        *reinterpret_cast<int*>(param_1 + 0x1f0) = param_2;
        uint32_t local[4];
        local[0] = 0;
        local[1] = static_cast<uint32_t>(param_2);
        local[2] = 0;   // *(undefined8*)(puVar1+2)=0 → cmd[2]=cmd[3]=0
        local[3] = 0;
        if (*reinterpret_cast<uint8_t*>(param_1 + 0x5c) != 0) {
            Pf_WorkerApplyCmd_a6430(param_1, local, param_3, param_4);
            return;
        }
        EnqueueNode(param_1, local);
    }
}

// 0x1400a6c10 — enqueue cmd type 5 (trade-route A* request). (Ghidra: TradeComponent_impl_6c10.)
//   First allocates a result slot via a6240 (fills param_2 = {slotId, token}), then
//   enqueues a type-5 command carrying {start=arg1, goal=arg2, slotId=*param_2}.
//   Returns param_2.
int* Pf_EnqueueTradeRoute_a6c10(int64_t param_1, int* param_2, void* param_3, void* param_4) {
    uint32_t uVar5 = static_cast<uint32_t>(reinterpret_cast<uint64_t>(param_4));
    uint32_t uVar4 = static_cast<uint32_t>(reinterpret_cast<uint64_t>(param_3));
    Pf_AllocTradeSlot_a6240(param_1, param_2);
    int iVar1 = *param_2;
    uint32_t local[4];
    local[0] = 5;
    local[1] = uVar4;
    local[2] = uVar5;
    local[3] = static_cast<uint32_t>(iVar1);
    if (*reinterpret_cast<uint8_t*>(param_1 + 0x5c) == 0) {
        EnqueueNode(param_1, local);
    } else {
        Pf_WorkerApplyCmd_a6430(param_1, local, param_3, param_4);
    }
    return param_2;
}

// 0x1400a6240 — allocate a trade-route result slot id (under CS). (Ghidra: TradeComponent_impl_6240.)
//   Scans the slot array (self+0x160, stride 0x18, +0x04 = token) for a free (token==0)
//   slot; if none, grows the array by one via d940 (with a 0x10-byte temp on stack) and
//   uses the new index. Assigns the slot a fresh token (self+0x58, never 0). Returns
//   param_2 = {slotIndex, token}.
int* Pf_AllocTradeSlot_a6240(int64_t param_1, int* param_2) {
    int64_t lVar1;
    int iVar2;
    uint32_t uVar3;
    int* piVar4;
    int iVar5;
    int64_t* plVar6;
    // The original MSVC stack lays out {local_30[4], local_2c, local_28, local_20}
    // as ONE contiguous 0x18-byte slot-element template at rbp-0x30:
    //   +0x00 activeFlag, +0x04 token, +0x08 vec.ptr, +0x10 vec.count, +0x14 vec.cap.
    // d940 reads it as a single struct (vec at template+8). Separate C++ locals are
    // NOT guaranteed contiguous by GCC -> d940 then reads garbage at +8/+0x10 and
    // f9e0 deep-copies a wild {ptr,count} -> crash. Keep it a single buffer.
    uint8_t local_30[0x18];

    iVar5 = 0;
    iVar2 = *reinterpret_cast<int*>(param_1 + 0x168);
    if (0 < iVar2) {
        piVar4 = reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 0x160) + 4);
        do {
            if (*piVar4 == 0) {
                if (-1 < iVar5) goto LAB_1400a62f5;
                break;
            }
            iVar5 = iVar5 + 1;
            piVar4 = piVar4 + 6;  // stride 0x18 (6 ints)
        } while (iVar5 < iVar2);
    }
    reinterpret_cast<System::CriticalSection*>(param_1 + 0x30)->Lock();   // B.3.4b bridge
    for (int i = 0; i < 0x18; ++i) local_30[i] = 0;   // {flag,token,vec.ptr,vec.count,vec.cap} = 0
    Pf_VectorResize18Dup_d940(reinterpret_cast<int64_t*>(param_1 + 0x160),   // 0x14001d940
                         *reinterpret_cast<int*>(param_1 + 0x168) + 1, local_30);
    // MemFree the template's own vec.ptr (template+8) after the resize cloned it (faithful
    // to the original's MemFree(local_28)). It is null here, so this is a harmless no-op.
    System::MemoryAllocator::MemFree(*reinterpret_cast<void**>(local_30 + 8));
    *reinterpret_cast<void**>(local_30 + 8) = nullptr;
    *reinterpret_cast<uint64_t*>(local_30 + 0x10) = 0;
    reinterpret_cast<System::CriticalSection*>(param_1 + 0x30)->Unlock();   // B.3.4b bridge
    iVar5 = iVar2;
LAB_1400a62f5:
    plVar6 = reinterpret_cast<int64_t*>(param_1 + 0x160);
    lVar1 = static_cast<int64_t>(iVar5) * 0x18;
    *reinterpret_cast<uint32_t*>(lVar1 + 4 + *plVar6) = *reinterpret_cast<uint32_t*>(param_1 + 0x58);
    piVar4 = reinterpret_cast<int*>(param_1 + 0x58);
    *piVar4 = *piVar4 + 1;
    uVar3 = *reinterpret_cast<uint32_t*>(param_1 + 0x58);
    if (*piVar4 == 0) {
        uVar3 = 1;
    }
    *reinterpret_cast<uint32_t*>(param_1 + 0x58) = uVar3;
    *reinterpret_cast<uint8_t*>(lVar1 + *plVar6) = 0;
    *param_2 = iVar5;
    param_2[1] = *reinterpret_cast<int*>(lVar1 + 4 + *plVar6);
    return param_2;
}

// ============================================================
// 0x14009c170 — flood-fill connectivity over the road/cell graph. (Ghidra: FUN_14009c170.)
//   BFS over a 4-direction walkable grid (map @ param_1, cell grid base param_1[0x20],
//   stride 0x30; +0x26(short)=mark, +0x29(byte)=terrain-class bits, region label word
//   @ +0x16 (lVar14-10)). Spreads a region label (param_3) across the connected
//   component, run-length sweeping each row span then probing N/S rows. Queue nodes
//   allocated via 1a9b0 (stride 0x18: +0x00 cell, +0x08 next, +0x10 prev), recycled to
//   the pool free-list. Queue head @ param_1[0x2c], tail @ param_1[0x2a], count +0x2e.
//   param_2 = first cell, param_3 = label (short).
// THREE parameters (B3R Leftovers, read in retail): the entry spills R9D to its home slot
// (0x14009c170, [RSP+0x78] after the prologue), but every read of that slot follows a
// write (0x14009c365 / 0x14009c3e8, the neighbour cell handed to 0x14001a9b0 by address),
// and R9D itself is overwritten at 0x14009c26f before any read. So Ghidra's param_4 is
// the local local_res20 below, not an argument; its initial value is never read.
// VERIFIED: index math (iVar13/iVar12 row bounds, the 0xe1fU walkable-class mask) and
// the -10 region-label store offset cross-checked against disassembly.
void Pf_FloodFillRegion_9c170(int* param_1, uint32_t param_2, uint16_t param_3) {
    int64_t* plVar1;
    uint* puVar2;
    uint uVar3;
    int iVar4;
    uint64_t* puVar5;
    int64_t lVar6;
    int iVar7;
    uint uVar8;
    uint64_t uVar9;
    uint uVar10;
    uint8_t* pbVar11;
    int iVar12;
    int iVar13;
    int64_t lVar14;
    uint32_t local_res10[4];
    int local_res20[2];   // written before each read (0x14009c365 / 0x14009c3e8)



    plVar1 = reinterpret_cast<int64_t*>(param_1 + 0x2a);
    local_res10[0] = param_2;
    puVar5 = reinterpret_cast<uint64_t*>(Pf_NodePoolAlloc18_a9b0(reinterpret_cast<uintptr_t>(plVar1), local_res10));   // 0x14001a9b0
    if (*plVar1 == 0) {
        *reinterpret_cast<uint64_t**>(param_1 + 0x2c) = puVar5;
    } else {
        *reinterpret_cast<uint64_t**>(*plVar1 + 8) = puVar5;
        puVar5[2] = static_cast<uint64_t>(*plVar1);
    }
    param_1[0x2e] = param_1[0x2e] + 1;
    *plVar1 = reinterpret_cast<int64_t>(puVar5);
    iVar12 = param_1[0x2e];
    do {
        if (iVar12 < 1) {
            return;
        }
        puVar2 = *reinterpret_cast<uint**>(param_1 + 0x2c);
        uVar8 = *puVar2;
        uVar9 = static_cast<uint64_t>(static_cast<int>(uVar8));
        if (puVar2 == reinterpret_cast<uint*>(*plVar1)) {
            *plVar1 = *reinterpret_cast<int64_t*>(puVar2 + 4);
        }
        *reinterpret_cast<uint64_t*>(param_1 + 0x2c) = *reinterpret_cast<uint64_t*>(puVar2 + 2);
        if (*reinterpret_cast<int64_t*>(puVar2 + 2) != 0) {
            *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(puVar2 + 2) + 0x10) = *reinterpret_cast<uint64_t*>(puVar2 + 4);
        }
        if (*reinterpret_cast<int64_t*>(puVar2 + 4) != 0) {
            *reinterpret_cast<uint64_t*>(*reinterpret_cast<int64_t*>(puVar2 + 4) + 8) = *reinterpret_cast<uint64_t*>(puVar2 + 2);
        }
        puVar2[2] = 0;
        puVar2[3] = 0;
        puVar2[4] = 0;
        puVar2[5] = 0;
        param_1[0x2e] = param_1[0x2e] + -1;
        param_1[0x37] = param_1[0x37] + -1;
        param_1[0x38] = param_1[0x38] + 1;
        *reinterpret_cast<uint64_t*>(puVar2) = *reinterpret_cast<uint64_t*>(param_1 + 0x32);
        *reinterpret_cast<uint**>(param_1 + 0x32) = puVar2;
        iVar12 = *param_1;
        lVar14 = uVar9 * 0x30 + *reinterpret_cast<int64_t*>(param_1 + 0x20);
        iVar4 = static_cast<int>(uVar8) / iVar12;
        iVar13 = iVar12 * iVar4;
        iVar12 = (iVar4 + 1) * iVar12 + -1;
        if ((*reinterpret_cast<short*>(lVar14 + 0x26) == 0) &&
            ((1 << (*reinterpret_cast<uint8_t*>(lVar14 + 0x29) & 0x1f) & 0xe1fU) != 0)) {
            iVar7 = uVar8 - 1;
            uVar10 = uVar8;
            if (iVar13 <= iVar7) {
                pbVar11 = reinterpret_cast<uint8_t*>(lVar14 + -7);
                do {
                    if ((1 << (*pbVar11 & 0x1f) & 0xe1fU) == 0) break;
                    iVar7 = iVar7 + -1;
                    uVar10 = uVar10 - 1;
                    pbVar11 = pbVar11 + -0x30;
                } while (iVar13 <= iVar7);
            }
            iVar7 = uVar8 + 1;
            if (iVar7 <= iVar12) {
                pbVar11 = reinterpret_cast<uint8_t*>(lVar14 + 0x59);
                do {
                    uVar8 = static_cast<uint>(uVar9);
                    if ((1 << (*pbVar11 & 0x1f) & 0xe1fU) == 0) break;
                    iVar7 = iVar7 + 1;
                    uVar8 = uVar8 + 1;
                    uVar9 = static_cast<uint64_t>(uVar8);
                    pbVar11 = pbVar11 + 0x30;
                } while (iVar7 <= iVar12);
            }
            lVar14 = static_cast<int64_t>(static_cast<int>(uVar10));
            if (lVar14 <= static_cast<int>(uVar8)) {
                lVar6 = lVar14 * 0x30;
                lVar14 = (static_cast<int>(uVar8) - lVar14) + 1;
                do {
                    lVar6 = lVar6 + 0x30;
                    *reinterpret_cast<uint16_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x20) + -10 + lVar6) = param_3;
                    lVar14 = lVar14 + -1;
                } while (lVar14 != 0);
            }
            if (iVar13 < static_cast<int>(uVar10 - 1)) {
                uVar10 = uVar10 - 1;
            }
            if (static_cast<int>(uVar8 + 1) < iVar12) {
                uVar8 = uVar8 + 1;
            }
            uVar3 = uVar10;
            if (0 < iVar4) {
                for (; static_cast<int>(uVar3) <= static_cast<int>(uVar8); uVar3 = uVar3 + 1) {
                    local_res20[0] = uVar3 - *param_1;
                    lVar14 = static_cast<int64_t>(local_res20[0]) * 0x30 + *reinterpret_cast<int64_t*>(param_1 + 0x20);
                    if ((*reinterpret_cast<short*>(lVar14 + 0x26) == 0) &&
                        ((1 << (*reinterpret_cast<uint8_t*>(lVar14 + 0x29) & 0x1f) & 0xe1fU) != 0)) {
                        puVar5 = reinterpret_cast<uint64_t*>(Pf_NodePoolAlloc18_a9b0(reinterpret_cast<uintptr_t>(plVar1), reinterpret_cast<uint32_t*>(local_res20)));   // 0x14001a9b0
                        if (*plVar1 == 0) {
                            *reinterpret_cast<uint64_t**>(param_1 + 0x2c) = puVar5;
                        } else {
                            *reinterpret_cast<uint64_t**>(*plVar1 + 8) = puVar5;
                            puVar5[2] = static_cast<uint64_t>(*plVar1);
                        }
                        param_1[0x2e] = param_1[0x2e] + 1;
                        *plVar1 = reinterpret_cast<int64_t>(puVar5);
                    }
                }
            }
            if (iVar4 < *param_1 + -1) {
                for (; static_cast<int>(uVar10) <= static_cast<int>(uVar8); uVar10 = uVar10 + 1) {
                    local_res20[0] = *param_1 + uVar10;
                    lVar14 = static_cast<int64_t>(local_res20[0]) * 0x30 + *reinterpret_cast<int64_t*>(param_1 + 0x20);
                    if ((*reinterpret_cast<short*>(lVar14 + 0x26) == 0) &&
                        ((1 << (*reinterpret_cast<uint8_t*>(lVar14 + 0x29) & 0x1f) & 0xe1fU) != 0)) {
                        puVar5 = reinterpret_cast<uint64_t*>(Pf_NodePoolAlloc18_a9b0(reinterpret_cast<uintptr_t>(plVar1), reinterpret_cast<uint32_t*>(local_res20)));   // 0x14001a9b0
                        if (*plVar1 == 0) {
                            *reinterpret_cast<uint64_t**>(param_1 + 0x2c) = puVar5;
                        } else {
                            *reinterpret_cast<uint64_t**>(*plVar1 + 8) = puVar5;
                            puVar5[2] = static_cast<uint64_t>(*plVar1);
                        }
                        param_1[0x2e] = param_1[0x2e] + 1;
                        *plVar1 = reinterpret_cast<int64_t>(puVar5);
                    }
                }
            }
        }
        iVar12 = param_1[0x2e];
    } while (true);
}
