// code/Game/Component/keystone_a_map.cpp
// Keystone A — map overlay / map-init grid rasterizer cluster.
//
// Faithful 1:1 ports (Ghidra, ABI/offset verified against the disassembly) of the
// MapComponent overlay-update + CreateNaturalResource map-init family. These five
// functions share a "param rotation" switch (0x5a / 0xb4 / 0x10e — 90/180/270 deg)
// driving how a small char stamp ('#','_','.') is sampled into a destination cell
// grid, the same shape as ZoneComponent_sub_d360.
//
//   0x1401241b0  MapComponent_sub_41b0            overlay update entry (vtable)
//   0x14009dc20  MapComponent_overlay_dc20        overlay grid-update worker
//   0x140124750  CreateNaturalResource_impl_4750  map-init entry (vtable)
//   0x14009dee0  CreateNaturalResource_impl_dee0  map-init grid-update worker
//   0x14009e2c0  CreateNaturalResource_helper_e2c0  per-cell terrain classification
//
// int*-arithmetic trap: `param_1` is `int*` (the Map). The grid pointer the original
// reads as `*(longlong*)(param_1 + 0x20)` is BYTE offset 0x80 (0x20 * 4) — confirmed
// by disasm `MOV ... ,[R13 + 0x80]`. `*param_1` is the map width (byte 0). Cell stride
// is 0x30 (LEA RBX,[RAX+RAX*2]; SHL RBX,4).
//
// Sub-callees (all already registered, or this batch):
//   fn_IsDerivedFrom         slot 0x140152a98 (engine_funcs)
//   fn_InternalBasePtr_CheckPtr slot 0x140152ab0 (engine_funcs)
//   fn_InternalBasePtr_Ctor  slot 0x140152a90 (engine_funcs)
//   0x1400a4d30  MapComponent_helper_4d30   (reg_component_visuals)
//   0x1400c3470  NotifyEntityChanged        (reg_action_hunt)
//   0x14009f2f0  SetTileRoadFlag/cost setter(reg_pf_roots_road)
//   0x1400c2ea0  MarkEntityRemoval          (reg_action_hunt)
//   0x140003110  DynArraySetCount push      (reg_action_hunt)
//   0x14009f3e0  GridToWorld terrain query  (mapquery, reg)
//   g_pGameWorldSingleton (+0xe8 / +0xd0 / +0xd8 / +0xf8 / +0x148)
//

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// BANISHED_DC20_TRACE=1 — log dc20's stale-occupant eviction decisions (see the branch).
static bool DC20Trace() {
    static int on = -1;
    if (on < 0) { const char* e = std::getenv("BANISHED_DC20_TRACE");
                  on = (e && e[0] && e[0] != '0') ? 1 : 0; }
    return on == 1;
}

// --- registered exe sub-callees, resolved on first use ----------------------
typedef int      (*fn_e2c0_t)(int* map, int cellIndex);                 // 0x14009e2c0 (this file)
typedef void     (*fn_9f2f0_t)(int* map, int cellIndex, uint32_t val);  // 0x14009f2f0
typedef void     (*fn_a4d30_t)(void* p1, uint32_t p2, int x0, int y0,   // 0x1400a4d30
                               int x1, int y1);
typedef void     (*fn_c3470_t)(void* internal);                         // 0x1400c3470
typedef void     (*fn_c2ea0_t)(int64_t internal);                       // 0x1400c2ea0
typedef void     (*fn_003110_t)(int64_t* tree, int key, void* ibp);     // 0x140003110
typedef float*   (*fn_f3e0_t)(int* map, float* outVec, float* inXY,     // 0x14009f3e0
                              char flag, uint32_t* outMat);

static fn_9f2f0_t  s_9f2f0  = nullptr;
static fn_a4d30_t  s_a4d30  = nullptr;
static fn_c3470_t  s_c3470  = nullptr;
static fn_c2ea0_t  s_c2ea0  = nullptr;
static fn_003110_t s_003110 = nullptr;
static fn_f3e0_t   s_f3e0   = nullptr;
static bool        s_init   = false;

static void EnsureInit() {
    if (s_init) return;
    s_9f2f0  = reinterpret_cast<fn_9f2f0_t >(ResolveAddr(Module::Exe, 0x14009f2f0));
    s_a4d30  = reinterpret_cast<fn_a4d30_t >(ResolveAddr(Module::Exe, 0x1400a4d30));
    s_c3470  = reinterpret_cast<fn_c3470_t >(ResolveAddr(Module::Exe, 0x1400c3470));
    s_c2ea0  = reinterpret_cast<fn_c2ea0_t >(ResolveAddr(Module::Exe, 0x1400c2ea0));
    s_003110 = reinterpret_cast<fn_003110_t>(ResolveAddr(Module::Exe, 0x140003110));
    s_f3e0   = reinterpret_cast<fn_f3e0_t  >(ResolveAddr(Module::Exe, 0x14009f3e0));
    s_init = true;
}

// e2c0 terrain classification thresholds (named DAT floats, exact bit values).
constexpr float kHalf     = 0.5f;    // DAT_140172114 = 0x3f000000
constexpr float kT_Ret8   = -1.4f;   // DAT_140172284 = 0xbfb33333
constexpr float kT_Ret7   = -0.25f;  // DAT_140172274 = 0xbe800000
constexpr float kT_Ret2Hi = 0.75f;   // DAT_140172124 = 0x3f400000
constexpr float kSlopeMin = 0.9f;    // DAT_140172134 = 0x3f666666
constexpr float kT_Ret6   = 2.0f;    // DAT_140172174 = 0x40000000

} // namespace

// ============================================================
// CreateNaturalResource_helper_e2c0 (0x14009e2c0)
// ============================================================
// Classifies the terrain at cell `param_2` of the map. Builds a centred world XY
// from the cell linear index (col = idx % width, row = idx / width, both minus
// width/2 - 0.5), queries terrain height/slope via 0x14009f3e0, and returns a
// small classification byte:
//   height < -1.4              -> 8 (deep water)
//   height < -0.25             -> 7 (shallow water / shore)
//   -0.25 <= h <= 0.75 & slope>=0.9 -> 2 (flat buildable)
//   else height < 2.0          -> 10, else 6
// Returned byte is consumed by callers as `1 << (b & 0x1f) & 0x1e0`.
int CreateNaturalResource_helper_e2c0(int* param_1, int param_2) {
    EnsureInit();

    // Stack layout mirrors the original (e2c0 disasm):
    //   local_18 (RSP+0x50) = out vec3   -> [0],[1] = world XY, local_10 (RSP+0x58)=height
    //   local_38 (RSP+0x30) = in XY      -> local_38=x, local_34=y
    //   local_28 (RSP+0x40) = out "mat"  -> f3e0 writes 3 dwords; local_20 (RSP+0x48)=elem[2]
    // The classifier reads height (local_10) and slope (local_20 == out-mat elem[2]).
    float outVec[3] = {0, 0, 0};       // local_18 / local_10
    float inXY[2];                     // local_38 / local_34
    uint32_t outMat[3] = {0, 0, 0};    // local_28[0..1] + local_20 (init 0.0)

    float half = (float)(*param_1 >> 1) - kHalf;             // local_34 = width/2 - 0.5
    inXY[0] = (float)(param_2 % *param_1) - half;            // local_38 = col - half
    inXY[1] = (float)(param_2 / *param_1) - half;            // local_34 = row - half

    if (s_f3e0)
        s_f3e0(param_1, outVec, inXY, '\x01', outMat);

    float height = outVec[2];                                       // local_10
    float slope  = *reinterpret_cast<float*>(&outMat[2]);           // local_20

    if (height < kT_Ret8) return 8;
    if (height < kT_Ret7) return 7;
    if (kT_Ret7 <= height && height <= kT_Ret2Hi && kSlopeMin <= slope) return 2;
    int ret = 6;
    if (height < kT_Ret6) ret = 10;
    return ret;
}

// ============================================================
// MapComponent_overlay_dc20 (0x14009dc20)
// ============================================================
// Overlay grid-update worker. For each cell in the [param_4..+param_6) x
// [param_5..+param_7) window of the destination grid (param_1, base +0x80, stride
// 0x30): if the cell currently references an entity (cell+8) different from the
// overlay's source IBP (*param_2) and that entity's flags bit 10 is clear, mark it
// for removal and push it to the gameworld removal queue. Then sample the rotated
// char stamp (param_8) at the rotated coordinate; '#'/'_' => set, '.' => leave,
// '_' => value 2. Write the resulting road/cost value via 0x14009f2f0 and store the
// source/null IBP into the cell.
void MapComponent_overlay_dc20(int* param_1, int64_t* param_2, uint8_t param_3,
                               int param_4, int param_5, int param_6, int param_7,
                               int64_t* param_8, int param_9) {
    EnsureInit();

    int iVar18 = 0;
    int iVar4 = param_7;
    if (0 < param_7) {
        do {
            iVar4 = iVar4 + -1;             // descending outer (row)
            int iVar13 = 0;
            int iVar5 = param_6;
            if (0 < param_6) {
                do {
                    iVar5 = iVar5 + -1;     // descending inner (col)
                    int iVar15 = (param_5 + iVar18) * *param_1 + iVar13 + param_4;
                    int64_t lVar14 = (int64_t)iVar15 * 0x30 +
                                     *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(param_1) + 0x80);

                    // Evict any stale entity occupying this cell.
                    if ((*param_2 != 0) && (*reinterpret_cast<int64_t*>(lVar14 + 8) != 0)) {
                        if (fn_InternalBasePtr_CheckPtr)
                            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(lVar14 + 8));
                        int64_t lVar2 = *reinterpret_cast<int64_t*>(lVar14 + 8);
                        if (*param_2 != lVar2) {
                            int64_t lVar3 = *reinterpret_cast<int64_t*>(
                                reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xe8);
                            int64_t local_68 = lVar2;
                            if (lVar2 != 0) {
                                if (fn_InternalBasePtr_CheckPtr)
                                    fn_InternalBasePtr_CheckPtr(&local_68);
                                // [diag] BANISHED_DC20_TRACE — this branch is retail DESTROYING
                                // whatever already occupied the cell (MarkForRemoval + push onto
                                // the world removal queue). User-observed retail behaviour is that
                                // a building placed over trees does NOT remove them, so either this
                                // branch is skipped (bit 0x400 already set) or the whole 41b0 chain
                                // does not run at placement. Log which, with the deciding flags.
                                if (DC20Trace()) {
                                    static int s_ev = 0;
                                    if (++s_ev <= 40) {
                                        uint32_t f40 = *reinterpret_cast<uint32_t*>(local_68 + 0x40);
                                        std::fprintf(stderr, "[dc20] cell=%d occupant=%p flags+0x40=0x%x "
                                                     "removalBit0x400=%d -> %s\n",
                                                     iVar15, reinterpret_cast<void*>(local_68), f40,
                                                     (f40 >> 10) & 1,
                                                     ((f40 >> 10) & 1) ? "SKIP (already marked)"
                                                                       : "EVICT (MarkForRemoval + queue)");
                                        std::fflush(stderr);
                                    }
                                }
                                if ((*reinterpret_cast<uint32_t*>(local_68 + 0x40) >> 10 & 1) == 0) {
                                    if (fn_InternalBasePtr_CheckPtr)
                                        fn_InternalBasePtr_CheckPtr(&local_68);
                                    if (s_c2ea0) s_c2ea0(local_68);
                                    if (s_003110)
                                        s_003110(reinterpret_cast<int64_t*>(lVar3 + 0xd0),
                                                 *reinterpret_cast<int*>(lVar3 + 0xd8) + 1,
                                                 &local_68);
                                }
                            }
                        }
                    }

                    // Sample the rotated char stamp.
                    int64_t lVar2 = *param_8;
                    bool bVar7;
                    if ((lVar2 == 0) || (*reinterpret_cast<int*>(lVar2 - 8) == 0)) {
                        bVar7 = true;
                    } else {
                        bVar7 = false;
                    }
                    bool bVar19 = true;
                    uint8_t bVar16 = param_3;
                    if (!bVar7) {
                        int iVar12 = iVar5;
                        int iVar6 = iVar18;
                        int iVar11, iVar17, iVar8;
                        if ((param_9 == 0x5a) ||
                            ((iVar11 = param_7, iVar17 = param_6, iVar8 = iVar4,
                              param_9 != 0xb4 &&
                              (iVar8 = iVar18, iVar12 = iVar13, iVar6 = iVar4,
                               param_9 == 0x10e)))) {
                            iVar11 = param_6;
                            iVar17 = param_7;
                            iVar8 = iVar12;
                            iVar12 = iVar6;
                        }
                        char cVar1 = *reinterpret_cast<char*>(
                            (((iVar11 - iVar8) - 1) * (iVar17 + 1) + iVar12) + lVar2);
                        if ((cVar1 == '#') || (cVar1 == '_')) {
                            bVar7 = true;
                        } else {
                            bVar7 = false;
                        }
                        bVar19 = cVar1 != '.';
                        if (cVar1 == '_') {
                            bVar16 = 2;
                        }
                    }

                    uint8_t bVar10 = *reinterpret_cast<uint8_t*>(lVar14 + 0x29);
                    if (bVar7) {
                        bVar10 = bVar16;
                    }
                    if (s_9f2f0) s_9f2f0(param_1, iVar15, (uint32_t)bVar10);

                    int64_t local_60;
                    int64_t local_58[3];
                    int64_t* plVar9;
                    if (bVar19) {
                        local_60 = *param_2;
                        plVar9 = &local_60;
                    } else {
                        if (fn_InternalBasePtr_Ctor)
                            fn_InternalBasePtr_Ctor(local_58, nullptr);
                        plVar9 = local_58;
                    }
                    iVar13 = iVar13 + 1;
                    // Wipe census. dc20 writes either the owner entity or a NULL IBP
                    // per cell (bVar19 picks, from the footprint mask). A building whose
                    // mask says "walkable" therefore NULLS the tile -- and if a crop was
                    // already standing there, the crop's tile entry is destroyed and
                    // HarvestArea can never find it again. Count only the destructive
                    // case: a cell that HELD something and is being cleared.
                    {
                        static int s_wt = -1;
                        if (s_wt < 0) { const char* e = std::getenv("BANISHED_CROPFIELD_TRACE");
                                        s_wt = (e && e[0] && e[0] != '0') ? 1 : 0; }
                        int64_t prev = *reinterpret_cast<int64_t*>(lVar14 + 8);
                        static long long s_wipes = 0, s_n = 0;
                        if (prev != 0 && *plVar9 == 0) {
                            ++s_wipes;
                            if (s_wt && ((s_n++ % 10) == 0)) {
                                std::fprintf(stderr, "[TileWipe] dc20 NULLing a cell that held "
                                             "an entity: cell=%d prev=%p wipes=%lld\n",
                                             iVar15, (void*)prev, s_wipes);
                                std::fflush(stderr);
                            }
                        }
                    }
                    *reinterpret_cast<int64_t*>(lVar14 + 8) = *plVar9;
                } while (iVar13 < param_6);
            }
            iVar18 = iVar18 + 1;
        } while (iVar18 < param_7);
    }
}

// ============================================================
// CreateNaturalResource_impl_dee0 (0x14009dee0)
// ============================================================
// Map-init grid-update worker. For each cell in the window: sample the rotated char
// stamp (param_8) — '#'/'_' => set. On a set cell, classify the terrain via
// 0x14009e2c0; if the classification bit is in mask 0x1e0 AND the cell already holds
// an entity that differs from the overlay source IBP (param_2), force class 2; write
// the class via 0x14009f2f0. Always (re)initialise the cell's +8 IBP slot to null.
void CreateNaturalResource_impl_dee0(int* param_1, void* param_2, int param_3,
                                     int param_4, int param_5, int param_6,
                                     int64_t* param_7, int param_8) {
    EnsureInit();

    int iVar16 = 0;
    int iVar3 = param_6;
    if (0 < param_6) {
        do {
            iVar3 = iVar3 + -1;            // descending outer (row)
            int iVar12 = 0;
            int iVar4 = param_5;
            if (0 < param_5) {
                do {
                    iVar4 = iVar4 + -1;    // descending inner (col)
                    int64_t lVar2 = *param_7;
                    int iVar14 = (iVar16 + param_4) * *param_1 + iVar12 + param_3;

                    bool bVar6;
                    if ((lVar2 == 0) || (*reinterpret_cast<int*>(lVar2 - 8) == 0)) {
                        bVar6 = true;
                    } else {
                        bVar6 = false;
                    }
                    if (!bVar6) {
                        int iVar7 = iVar4;
                        int iVar5 = iVar16;
                        int iVar15, iVar8, iVar10;
                        if ((param_8 == 0x5a) ||
                            ((iVar15 = param_5, iVar8 = iVar3, iVar10 = param_6,
                              param_8 != 0xb4 &&
                              (iVar8 = iVar16, iVar7 = iVar12, iVar5 = iVar3,
                               param_8 == 0x10e)))) {
                            iVar15 = param_6;
                            iVar8 = iVar7;
                            iVar10 = param_5;
                            iVar7 = iVar5;
                        }
                        char cVar1 = *reinterpret_cast<char*>(
                            (((iVar10 - iVar8) - 1) * (iVar15 + 1) + iVar7) + lVar2);
                        if ((cVar1 == '#') || (cVar1 == '_')) {
                            bVar6 = true;
                        } else {
                            bVar6 = false;
                        }
                    }

                    int64_t* plVar11 = reinterpret_cast<int64_t*>(
                        (int64_t)iVar14 * 0x30 +
                        *reinterpret_cast<int64_t*>(reinterpret_cast<uint8_t*>(param_1) + 0x80));
                    if (bVar6) {
                        int cls = CreateNaturalResource_helper_e2c0(param_1, iVar14);
                        uint8_t bVar13 = (uint8_t)cls;
                        if (((1 << (bVar13 & 0x1f) & 0x1e0u) != 0) && (*plVar11 != 0)) {
                            if (fn_InternalBasePtr_CheckPtr)
                                fn_InternalBasePtr_CheckPtr(param_2);
                            if (*plVar11 != *reinterpret_cast<int64_t*>(param_2)) {
                                bVar13 = 2;
                            }
                        }
                        if (s_9f2f0) s_9f2f0(param_1, iVar14, (uint32_t)bVar13);
                    }

                    int64_t local_40 = 0;
                    if (fn_InternalBasePtr_Ctor)
                        fn_InternalBasePtr_Ctor(&local_40, nullptr);
                    iVar12 = iVar12 + 1;
                    plVar11[1] = local_40;
                } while (iVar12 < param_5);
            }
            iVar16 = iVar16 + 1;
        } while (iVar16 < param_6);
    }
}

// ============================================================
// MapComponent_sub_41b0 (0x1401241b0)
// ============================================================
// Overlay-update vtable entry. Guarded by the +0x20 bit-20 flag (must be CLEAR).
// Resolves the overlay's AreaDescription (IsDerivedFrom DAT_1401c7a18), optionally
// clears the underlying terrain region via 0x1400a4d30, then runs the overlay
// grid-update (dc20). Finally sets bit-20 and, if the low-16 flag bits changed and
// the IBP at +0x10 is non-null, NotifyEntityChanged (0x1400c3470).
void MapComponent_sub_41b0(int64_t param_1) {
    EnsureInit();

    if ((*reinterpret_cast<uint32_t*>(param_1 + 0x20) >> 0x14 & 1) != 0) return;

    void* obj = *reinterpret_cast<void**>(param_1 + 0x18);
    int* piVar2 = *reinterpret_cast<int**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf8);
    uint32_t typeId = g_TypeId_AreaDescription ? *g_TypeId_AreaDescription : 0;
    bool bVar3 = (fn_IsDerivedFrom && typeId) ? fn_IsDerivedFrom(obj, typeId) : false;
    void* pvVar5 = bVar3 ? obj : nullptr;

    if (*reinterpret_cast<char*>(reinterpret_cast<int64_t>(pvVar5) + 0x32) != '\0') {
        if (s_a4d30) {
            s_a4d30(*reinterpret_cast<void**>(
                        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0x148),
                    *reinterpret_cast<uint32_t*>(reinterpret_cast<int64_t>(pvVar5) + 0x34),
                    (int)*reinterpret_cast<short*>(param_1 + 0x28),
                    (int)*reinterpret_cast<short*>(param_1 + 0x2a),
                    (int)*reinterpret_cast<short*>(param_1 + 0x2c),
                    (int)*reinterpret_cast<short*>(param_1 + 0x2e));
        }
    }

    if (*reinterpret_cast<char*>(reinterpret_cast<int64_t>(pvVar5) + 0x31) == '\0') {
        int64_t local_res8 = *reinterpret_cast<int64_t*>(param_1 + 0x10);
        MapComponent_overlay_dc20(
            piVar2, &local_res8,
            (uint8_t)*reinterpret_cast<uint32_t*>(reinterpret_cast<int64_t>(pvVar5) + 0x20),
            (int)*reinterpret_cast<short*>(param_1 + 0x28),
            (int)*reinterpret_cast<short*>(param_1 + 0x2a),
            (int)*reinterpret_cast<short*>(param_1 + 0x2c),
            (int)*reinterpret_cast<short*>(param_1 + 0x2e),
            reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(pvVar5) + 0x28),
            *reinterpret_cast<int*>(param_1 + 0x30));

        uint32_t uVar1 = *reinterpret_cast<uint32_t*>(param_1 + 0x20);
        uint32_t uVar4 = uVar1 | 0x100000;          // BTS 0x14
        *reinterpret_cast<uint32_t*>(param_1 + 0x20) = uVar4;
        if ((((uVar4 ^ uVar1) & 0xffff) != 0) &&
            (*reinterpret_cast<int64_t*>(param_1 + 0x10) != 0)) {
            if (fn_InternalBasePtr_CheckPtr)
                fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x10));
            if (s_c3470) s_c3470(*reinterpret_cast<void**>(param_1 + 0x10));
        }
    }
}

// ============================================================
// CreateNaturalResource_impl_4750 (0x140124750)
// ============================================================
// Map-init vtable entry. Guarded by the +0x20 bit-20 flag (must be SET). Resolves
// the AreaDescription, runs the map-init grid-update (dee0), then CLEARS bit-20 and,
// if the low-16 flag bits changed and the IBP at +0x10 is non-null, NotifyEntityChanged.
void CreateNaturalResource_impl_4750(int64_t param_1) {
    EnsureInit();

    if ((*reinterpret_cast<uint32_t*>(param_1 + 0x20) >> 0x14 & 1) == 0) return;

    void* obj = *reinterpret_cast<void**>(param_1 + 0x18);
    int* piVar2 = *reinterpret_cast<int**>(
        reinterpret_cast<uint8_t*>(*g_pGameWorldSingleton) + 0xf8);
    uint32_t typeId = g_TypeId_AreaDescription ? *g_TypeId_AreaDescription : 0;
    bool bVar3 = (fn_IsDerivedFrom && typeId) ? fn_IsDerivedFrom(obj, typeId) : false;
    int64_t local_res8 = *reinterpret_cast<int64_t*>(param_1 + 0x10);
    void* pvVar5 = bVar3 ? obj : nullptr;

    CreateNaturalResource_impl_dee0(
        piVar2, &local_res8,
        (int)*reinterpret_cast<short*>(param_1 + 0x28),
        (int)*reinterpret_cast<short*>(param_1 + 0x2a),
        (int)*reinterpret_cast<short*>(param_1 + 0x2c),
        (int)*reinterpret_cast<short*>(param_1 + 0x2e),
        reinterpret_cast<int64_t*>(reinterpret_cast<int64_t>(pvVar5) + 0x28),
        *reinterpret_cast<int*>(param_1 + 0x30));

    uint32_t uVar1 = *reinterpret_cast<uint32_t*>(param_1 + 0x20);
    uint32_t uVar4 = uVar1 & 0xffefffff;            // BTR 0x14
    *reinterpret_cast<uint32_t*>(param_1 + 0x20) = uVar4;
    if ((((uVar4 ^ uVar1) & 0xffff) != 0) &&
        (*reinterpret_cast<int64_t*>(param_1 + 0x10) != 0)) {
        if (fn_InternalBasePtr_CheckPtr)
            fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x10));
        if (s_c3470) s_c3470(*reinterpret_cast<void**>(param_1 + 0x10));
    }
}
