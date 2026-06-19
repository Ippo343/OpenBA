// code/Game/Action/action_pf_roots_road.cpp
// Wave 3 (Action) — ROAD-edit ROOT callers. Faithful 1:1 ports of the four top-level
// road/zone mutation entry points that drive the pathfinding connectivity + terrain
// refresh. NONE is exercised on the autostart path, so a wrong transcription is
// undetectable silent corruption — every offset is taken verbatim from the Ghidra
// decompile. Do NOT "improve" or simplify.
//
// Registration point: RegisterPfRootsRoadBatch() in standalone/reg_pf_roots_road.cpp.
//
// Sub-callees (all called via ResolveAddr(Module::Exe, ...); none duplicated here):
//   0x14009c170 Pf_FloodFillRegion (registered — PF A* batch)
//   0x1400a8c90 fine-cell dirty    (registered — PF road batch)
//   0x1400a6fe0 enqueue cmd2       (registered — PF A* batch)
//   0x1400ab150 region-dirty       (registered — PF road batch)
//   0x14009f3e0 grid->world        (registered — move/CNR wave)
//   0x14001a6e0 BST/AVL delete     (registered — Work BST wave)
//   0x14009f2f0 / 0x14009e0e0      (THIS file — registered in this batch)
//   0x1400af370 road-mesh rebuild  (NOT YET registered — DEFERRED; called via
//                                   ResolveAddr per brief, no-op until its own pass)
//
// g_pGameWorldSingleton + X  ==  (uintptr_t)*g_pGameWorldSingleton + X.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>

typedef unsigned int  uint;
typedef unsigned short ushort;

namespace {

// Exe sub-callees (resolved lazily; all registered except af370 which is deferred).
typedef void  (*fn_FloodFill_t)(int* map, uint32_t cell, uint16_t label, int seed);  // 0x14009c170
typedef void  (*fn_FineCellDirty_t)(int64_t obj, int x, int y);                       // 0x1400a8c90
typedef void  (*fn_EnqueueCmd2_t)(int64_t self, uint32_t a, void* b, void* c);        // 0x1400a6fe0
typedef void  (*fn_RegionDirty_t)(int64_t game, int x, int y);                        // 0x1400ab150
typedef float* (*fn_GridToWorld_t)(int* map, float* out, float* in, char flag, void* p); // 0x14009f3e0
typedef void  (*fn_BstDelete_t)(int64_t* hdr, void* node);                            // 0x14001a6e0
typedef void  (*fn_RoadMeshRebuild_t)(int64_t game, uint* map);                       // 0x1400af370

static fn_FloodFill_t      s_fn_Flood       = nullptr;
static fn_FineCellDirty_t  s_fn_FineDirty   = nullptr;
static fn_EnqueueCmd2_t    s_fn_EnqCmd2     = nullptr;
static fn_RegionDirty_t    s_fn_RegionDirty = nullptr;
static fn_GridToWorld_t    s_fn_GridToWorld = nullptr;
static fn_BstDelete_t      s_fn_BstDelete   = nullptr;
static fn_RoadMeshRebuild_t s_fn_RoadMesh   = nullptr;

// Float constants from the exe .rdata (used by e0e0's tile-centre math).
static const float* s_pDAT_172230 = nullptr;  // 0x140172230
static const float* s_pDAT_172114 = nullptr;  // 0x140172114 (0.5f cell-centre half)

static bool s_init = false;
static void EnsureFns() {
    if (s_init) return;
    s_fn_Flood       = reinterpret_cast<fn_FloodFill_t>(ResolveAddr(Module::Exe, 0x14009c170));
    s_fn_FineDirty   = reinterpret_cast<fn_FineCellDirty_t>(ResolveAddr(Module::Exe, 0x1400a8c90));
    s_fn_EnqCmd2     = reinterpret_cast<fn_EnqueueCmd2_t>(ResolveAddr(Module::Exe, 0x1400a6fe0));
    s_fn_RegionDirty = reinterpret_cast<fn_RegionDirty_t>(ResolveAddr(Module::Exe, 0x1400ab150));
    s_fn_GridToWorld = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_fn_BstDelete   = reinterpret_cast<fn_BstDelete_t>(ResolveAddr(Module::Exe, 0x14001a6e0));
    s_fn_RoadMesh    = reinterpret_cast<fn_RoadMeshRebuild_t>(ResolveAddr(Module::Exe, 0x1400af370));
    s_pDAT_172230    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172230));
    s_pDAT_172114    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    s_init = true;
}

} // namespace

// Forward declares for the two same-file helpers 9c640 calls via ResolveAddr.
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
    EnsureFns();
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
                    if (s_fn_Flood) s_fn_Flood(param_1, static_cast<int>(uVar2), static_cast<uint16_t>(sVar6), param_4);
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
    EnsureFns();
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
    uVar5 = 1u << (bVar1 & 0x1f);
    if ((((uVar5 & 0xe1f) != 0) && ((1u << (bVar4 & 0x1f) & 0x1e0U) != 0)) ||
        (((uVar5 & 0x1e0) != 0) &&
         (uVar5 = 1u << (bVar4 & 0x1f), uVar6 = static_cast<uint64_t>(uVar5), (uVar5 & 0xe1f) != 0))) {
        if (s_fn_FineDirty)
            s_fn_FineDirty(*reinterpret_cast<int64_t*>(param_1 + 0x1c), param_2 % *param_1, param_2 / *param_1);
    }
    if (s_fn_EnqCmd2)
        s_fn_EnqCmd2(*reinterpret_cast<int64_t*>(param_1 + 0x1a), static_cast<uint32_t>(param_2),
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
    EnsureFns();
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
    float co[3];   // co[0]=local_88, co[1]=local_84, co[2]=local_80
    float local_78[80];  // Ghidra `Vector3 local_78 [80]` — preserve size (only [0] used)

    fVar4 = s_pDAT_172230 ? *s_pDAT_172230 : 0.0f;
    fVar3 = s_pDAT_172114 ? *s_pDAT_172114 : 0.0f;
    lVar2 = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x110);
    for (; param_3 <= param_5; param_3 = param_3 + 1) {
        if (param_2 <= param_4) {
            iVar6 = param_2;
            do {
                uVar1 = *param_1;
                iVar5 = uVar1 * param_3 + iVar6;
                fVar7 = static_cast<float>(static_cast<int>(uVar1) >> 1) - fVar3;
                if (fn_Vector3_Ctor)
                    fn_Vector3_Ctor(local_78,
                                    static_cast<float>(iVar5 % static_cast<int>(uVar1)) - fVar7,
                                    static_cast<float>(iVar5 / static_cast<int>(uVar1)) - fVar7, 0.0f);
                if (s_fn_GridToWorld)
                    s_fn_GridToWorld(reinterpret_cast<int*>(param_1), gw, local_78, '\0', nullptr);
                *reinterpret_cast<uint32_t*>(&gw[2]) = 0;  // original: local_90 = 0 (4-byte zero into the z slot)
                fVar7 = static_cast<float>(*reinterpret_cast<int*>(lVar2 + 0xc0) >> 1);
                if (fn_Vector3_Ctor)
                    fn_Vector3_Ctor(co, gw[0] + fVar7, gw[1] + fVar7, 0.0f);
                *reinterpret_cast<short*>(*reinterpret_cast<int64_t*>(lVar2 + 0xd8) +
                    ((static_cast<int64_t>(*reinterpret_cast<int*>(lVar2 + 0xd0)) * static_cast<int64_t>(static_cast<int>(co[1])) +
                      static_cast<int64_t>(static_cast<int>(co[0]))) * 2)) = static_cast<short>(static_cast<int>(fVar4 * 0.0f));
                if (s_fn_RegionDirty)
                    s_fn_RegionDirty(lVar2, static_cast<int>(co[0]), static_cast<int>(co[1]));
                iVar6 = iVar6 + 1;
            } while (iVar6 <= param_4);
        }
    }
    if (s_fn_RoadMesh)
        s_fn_RoadMesh(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x110), param_1);
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
    EnsureFns();
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
                if (s_fn_BstDelete) s_fn_BstDelete(reinterpret_cast<int64_t*>(param_1 + 0x3a), piVar1);
                goto LAB_14009c6d2;
            }
            piVar1 = *reinterpret_cast<int**>(piVar1 + 6);   // right child (+0x18)
            continue;
        }
        piVar1 = *reinterpret_cast<int**>(piVar1 + 4);       // left child (+0x10)
    } while (true);
LAB_14009c6d2:
    if (s_fn_RoadMesh)
        s_fn_RoadMesh(*reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton) + 0x110), param_1);
}
