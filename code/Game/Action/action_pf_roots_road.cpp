// code/Game/Action/action_pf_roots_road.cpp
// Wave 3 (Action) — ROAD-edit ROOT callers. Faithful 1:1 ports of the four top-level
// road/zone mutation entry points that drive the pathfinding connectivity + terrain
// refresh. NONE is exercised on the autostart path, so a wrong transcription is
// undetectable silent corruption — every offset is taken verbatim from the Ghidra
// decompile. Do NOT "improve" or simplify.
#include "Action/exe_bodies.h"
//
// Registration point: RegisterPfRootsRoadBatch() in standalone/reg_pf_roots_road.cpp.
//
// Sub-callees (called by name, B3R; none duplicated here):
//   0x14009c170 Pf_FloodFillRegion_9c170 (PF A* batch)
//   0x1400a8c90 Pf_FineCellDirty_a8c90   (PF road batch)
//   0x1400a6fe0 Pf_EnqueueCmd2_a6fe0     (PF A* batch)
//   0x1400ab150 Pf_RegionDirty4_b150     (PF road batch)
//   0x14009f3e0 Reimpl_MapGetTerrainPos  (grid->world)
//   0x14001a6e0 WorkBST_BSTRemove_a6e0   (BST/AVL delete, Work BST wave)
//   0x14009f2f0 / 0x14009e0e0            (THIS file)
//   0x1400af370 Pf_RoadMeshRebuild_f370  (road-mesh rebuild, PF road batch)
//
// g_pGameWorldSingleton + X  ==  (uintptr_t)*g_pGameWorldSingleton + X.

#include "engine_funcs.h"
#include "perf_terrain.h"
#include "Work/exe_bodies.h"       // WorkBST_BSTRemove_a6e0
#include "Game/exe_bodies.h"       // Reimpl_MapGetTerrainPos
#include "Game/exe_constants.h"    // kRdata_*
#include <cstring>
#include "math/Math.h"
#include <cstdint>

typedef unsigned int  uint;
typedef unsigned short ushort;

namespace {

// Float constants (Game/exe_constants.h) used by e0e0's tile-centre math:
// 0x140172230 (256.0f, a bit pattern) and 0x140172114 (0.5f cell-centre half).
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

} // namespace

// Forward declares} // namespace

// Forward declares for the two same-file helpers 9c640 calls.
void Pf_SetTileRoadFlag_9f2f0(int* param_1, int param_2, uint param_3);
void Pf_RefreshRegionHeight_9e0e0(uint* param_1, int param_2, int param_3, int param_4, int param_5);

// ============================================================
// 0x14009c480 — ZoneRefresh. (Ghidra: FUN_14009c480.)
//   If the "dirty" flag at map+0xa2 is set: clear it, zero every cell's +0x26 region
//   marker (cell grid @map+0x20, stride 0x30, count map+0x22), then walk the cells and
//   for each walkable (terrain-class bit in 0xe1f) cell whose marker is still 0, launch
//   a flood-fill (9c170) labelling that connected component with an incrementing region
//   id (sVar6 starting at 1).
// param_4 = the seed/scratch passed through to 9c170.
// ============================================================
void Pf_ZoneRefresh_9c480(int* param_1, void* param_2, void* param_3, int param_4) {
    uint uVar1;
    uint64_t uVar2;
    uint64_t uVar3;
    int64_t lVar4;
    uint64_t uVar5;
    short sVar6;

    if (*reinterpret_cast<char*>(reinterpret_cast<char*>(param_1) + 0xa2) != '\0') {
        uVar5 = 0;
        *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(param_1) + 0xa2) = 0;
        uVar2 = uVar5;
        uVar3 = uVar5;
        if (0 < param_1[0x22]) {
            do {
                uVar1 = static_cast<int>(uVar2) + 1;
                *reinterpret_cast<uint16_t*>(uVar3 + 0x26 + *reinterpret_cast<int64_t*>(param_1 + 0x20)) = 0;
                uVar2 = static_cast<uint64_t>(uVar1);
                uVar3 = uVar3 + 0x30;
            } while (static_cast<int>(uVar1) < param_1[0x22]);
        }
        lVar4 = static_cast<int64_t>(param_1[0x22]);
        sVar6 = 1;
        uVar2 = uVar5;
        if (0 < lVar4) {
            do {
                if (((1 << (*reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x20) + 0x29 + uVar5) & 0x1f) & 0xe1fU) != 0) &&
                    (*reinterpret_cast<short*>(*reinterpret_cast<int64_t*>(param_1 + 0x20) + 0x26 + uVar5) == 0)) {
                    Pf_FloodFillRegion_9c170(param_1, static_cast<uint32_t>(static_cast<int>(uVar2)), static_cast<uint16_t>(sVar6), param_4);   // 0x14009c170
                    sVar6 = sVar6 + 1;
                }
                uVar5 = uVar5 + 0x30;
                lVar4 = lVar4 + -1;
                uVar2 = static_cast<uint64_t>(static_cast<int>(uVar2) + 1);
            } while (lVar4 != 0);
        }
    }
    (void)param_2; (void)param_3;
}

// ============================================================
// 0x14009f2f0 — set a tile's road/path class flag + dirty/enqueue. (Ghidra: FUN_14009f2f0.)
//   cell = map+0x20 + param_2*0x30. Old class byte @cell+0x29. Computes whether the
//   road<->path transition (bit-set test against masks 0xe1f / 0x1e0 / 0x1e0 / 0xe1f)
//   should OR the global map+0xa2 dirty flag. Writes the new class byte (param_3 low).
//   If the transition crosses the road/path boundary, calls the fine-cell dirty
//   (a8c90) with (subgrid obj @map+0x1c, col=param_2%width, row=param_2/width) and sets
//   uVar6 (the road-bit flag) accordingly. Finally enqueues a type-2 walk-cost command
//   (a6fe0) on the worker thread @map+0x1a carrying (param_2, param_3, uVar6).
// ============================================================
void Pf_SetTileRoadFlag_9f2f0(int* param_1, int param_2, uint param_3) {
    uint8_t bVar1;
    uint8_t bVar2;
    int64_t lVar3;
    uint8_t bVar4;
    uint uVar5;
    uint64_t uVar6;

    uVar6 = 1;
    lVar3 = static_cast<int64_t>(param_2) * 0x30 + *reinterpret_cast<int64_t*>(param_1 + 0x20);
    bVar1 = *reinterpret_cast<uint8_t*>(lVar3 + 0x29);
    uVar5 = 1u << (bVar1 & 0x1f);
    bVar4 = static_cast<uint8_t>(param_3);
    if ((((uVar5 & 0xe1f) == 0) || ((1u << (bVar4 & 0x1f) & 0x1e0U) == 0)) &&
        (((uVar5 & 0x1e0) == 0) || ((1u << (bVar4 & 0x1f) & 0xe1fU) == 0))) {
        bVar2 = 0;
    } else {
        bVar2 = 1;
    }
    *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(param_1) + 0xa2) =
        *reinterpret_cast<uint8_t*>(reinterpret_cast<char*>(param_1) + 0xa2) | bVar2;
    *reinterpret_cast<uint8_t*>(lVar3 + 0x29) = bVar4;
    PerfX::TerrainNoteWrite(reinterpret_cast<const uint8_t*>(lVar3));   // [PerfX] mirror
    uVar5 = 1u << (bVar1 & 0x1f);
    if ((((uVar5 & 0xe1f) != 0) && ((1u << (bVar4 & 0x1f) & 0x1e0U) != 0)) ||
        (((uVar5 & 0x1e0) != 0) &&
         (uVar5 = 1u << (bVar4 & 0x1f), uVar6 = static_cast<uint64_t>(uVar5), (uVar5 & 0xe1f) != 0))) {
        Pf_FineCellDirty_a8c90(   // 0x1400a8c90
            *reinterpret_cast<int64_t*>(param_1 + 0x1c), param_2 % *param_1, param_2 / *param_1);
    }
    Pf_EnqueueCmd2_a6fe0(   // 0x1400a6fe0
        *reinterpret_cast<int64_t*>(param_1 + 0x1a), static_cast<uint32_t>(param_2),
                     reinterpret_cast<void*>(static_cast<uint64_t>(param_3)), reinterpret_cast<void*>(uVar6));
}

// ============================================================
// 0x14009e0e0 — clear region heightmap/dirty over a tile rect + refresh. (Ghidra:
//   CitizenAI_impl_e0e0.) For each tile in the [param_3..param_5] x [param_2..param_4]
//   rect: build a local Vector3 at the tile centre, grid->world it (f3e0), offset by
//   the coarse half-cell, zero the coarse heightmap word for that coarse cell, and mark
//   the coarse region dirty (ab150). Then rebuild the road mesh (af370).
//   game = *(g_pGameWorldSingleton + 0x110).  fVar4 = DAT_172230, fVar3 = DAT_172114.
// TRAP: local_78 is a 0x50-byte (80 Vector3-element-typed) Ghidra buffer; only the
//   first Vector3 is used. f3e0 writes its 2-float result into local_98/local_94;
//   the coarse offset uses local_88/local_84 (the SECOND Vector3 built after f3e0).
// ============================================================
void Pf_RefreshRegionHeight_9e0e0(uint* param_1, int param_2, int param_3, int param_4, int param_5) {
    uint uVar1;
    int64_t lVar2;
    float fVar3;
    float fVar4;
    int iVar5;
    int iVar6;
    float fVar7;
    // Vector3 output slots must be CONTIGUOUS — the engine ctor/GridToWorld write 3
    // consecutive floats through a single pointer, which only works if the 3 stack
    // floats are adjacent (the Ghidra locals local_98/94/90 and local_88/84/80 were
    // contiguous stack slots; separate named C++ locals are not guaranteed adjacent
    // at -O0). gw = GridToWorld out {98,94,90}; co = coarse Vector3 {88,84,80}.
    float gw[3];   // gw[0]=local_98, gw[1]=local_94, gw[2]=local_90
    Math::Vector3 co;   // co.x=local_88, co.y=local_84, co.z=local_80
    Math::Vector3 local_78;  // Ghidra `Vector3 local_78 [80]` — only [0] was ever used (B.3.1: one Vector3)

    fVar4 = RdataF(kRdata_140172230);   // 0x140172230
    fVar3 = kRdata_140172114;           // 0x140172114
    lVar2 = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x110);
    for (; param_3 <= param_5; param_3 = param_3 + 1) {
        if (param_2 <= param_4) {
            iVar6 = param_2;
            do {
                uVar1 = *param_1;
                iVar5 = uVar1 * param_3 + iVar6;
                fVar7 = static_cast<float>(static_cast<int>(uVar1) >> 1) - fVar3;
                local_78 = Math::Vector3(
                                    static_cast<float>(iVar5 % static_cast<int>(uVar1)) - fVar7,
                                    static_cast<float>(iVar5 / static_cast<int>(uVar1)) - fVar7, 0.0f);
                Reimpl_MapGetTerrainPos(reinterpret_cast<int*>(param_1), gw, reinterpret_cast<float*>(&local_78), '\0', nullptr);   // B.3.1 bridge
                *reinterpret_cast<uint32_t*>(&gw[2]) = 0;  // original: local_90 = 0 (4-byte zero into the z slot)
                fVar7 = static_cast<float>(*reinterpret_cast<int*>(lVar2 + 0xc0) >> 1);
                co = Math::Vector3(gw[0] + fVar7, gw[1] + fVar7, 0.0f);
                *reinterpret_cast<short*>(*reinterpret_cast<int64_t*>(lVar2 + 0xd8) +
                    ((static_cast<int64_t>(*reinterpret_cast<int*>(lVar2 + 0xd0)) * static_cast<int64_t>(static_cast<int>(co.y)) +
                      static_cast<int64_t>(static_cast<int>(co.x))) * 2)) = static_cast<short>(static_cast<int>(fVar4 * 0.0f));
                Pf_RegionDirty4_b150(lVar2, static_cast<int>(co.x), static_cast<int>(co.y));   // 0x1400ab150
                iVar6 = iVar6 + 1;
            } while (iVar6 <= param_4);
        }
    }
    Pf_RoadMeshRebuild_f370(   // 0x1400af370
        *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x110), param_1);
}

// ============================================================
// 0x14009c640 — finalize a road tile edit. (Ghidra: CompleteRoad_helper_c640.)
//   cell = map+0x20 + param_2*0x30.
//   If param_4 == 2 (removal): decrement cell+0x28, set cell+0x2a = 0x00ff (16-bit), set class
//     to 2 via 9f2f0.
//   Else: write cell+0x2b = param_3 (orientation), set class to (param_4 & 0xff) via
//     9f2f0, then refresh the surrounding region height (9e0e0) over the 1-tile rect
//     [row..row+1] x [col..col+1].
//   Then walk the BST at map+0x3a (key = cell index, node stride 0x18: left @+0x10,
//     right @+0x18) and if param_2 is present, delete it (1a6e0).
//   Finally rebuild the road mesh (af370).
// ============================================================

void Pf_FinalizeRoadTile_9c640(uint* param_1, int param_2, uint8_t param_3, uint param_4) {
    int* piVar1;
    int iVar2;
    int iVar3;
    int64_t lVar4;

    lVar4 = static_cast<int64_t>(param_2) * 0x30 + *reinterpret_cast<int64_t*>(param_1 + 0x20);
    if (param_4 == 2) {
        *reinterpret_cast<char*>(lVar4 + 0x28) = *reinterpret_cast<char*>(lVar4 + 0x28) + -1;
        *reinterpret_cast<uint16_t*>(lVar4 + 0x2a) = 0xff;  // disasm: MOV word ptr,0xff → 0x00ff
        Pf_SetTileRoadFlag_9f2f0(reinterpret_cast<int*>(param_1), param_2, 2);
    } else {
        *reinterpret_cast<uint8_t*>(lVar4 + 0x2b) = param_3;
        Pf_SetTileRoadFlag_9f2f0(reinterpret_cast<int*>(param_1), param_2, param_4 & 0xff);
        iVar3 = param_2 / static_cast<int>(*param_1);
        iVar2 = param_2 % static_cast<int>(*param_1);
        Pf_RefreshRegionHeight_9e0e0(param_1, iVar2, iVar3, iVar2 + 1, iVar3 + 1);
    }
    piVar1 = *reinterpret_cast<int**>(param_1 + 0x3a);
    do {
        if (piVar1 == nullptr) {
            goto LAB_14009c6d2;
        }
        if (*piVar1 <= param_2) {
            if (param_2 <= *piVar1) {
                WorkBST_BSTRemove_a6e0(reinterpret_cast<uintptr_t*>(param_1 + 0x3a), piVar1);   // 0x14001a6e0
                goto LAB_14009c6d2;
            }
            piVar1 = *reinterpret_cast<int**>(piVar1 + 6);   // right child (+0x18)
            continue;
        }
        piVar1 = *reinterpret_cast<int**>(piVar1 + 4);       // left child (+0x10)
    } while (true);
LAB_14009c6d2:
    Pf_RoadMeshRebuild_f370(   // 0x1400af370
        *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x110), param_1);
}
