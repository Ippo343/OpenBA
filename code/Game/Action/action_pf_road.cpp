// code/Game/Action/action_pf_road.cpp
// Wave 3 (Action) — pathfinding / road-mesh-rebuild / geometry-wrapper / entity-resolve
// closure batch (the "PF-ROAD" family).
//
// Faithful 1:1 ports of 11 exe helpers (decompiled + disassembly-verified in Ghidra). The big
// one (0x1400af370 road-mesh rebuild) had two index-math details pinned against the disassembly:
// the 3x3 mask -> ae180 arg order and the per-vertex AO-uv overwrite offsets (see its block).
//   0x1400acaa0  Pf_RoadVtxFill_caa0       — fill one road-mesh vertex (uv/height/blend)
//   0x1400acc10  Pf_RoadQuad_cc10          — build the 4 verts of one road-tile quad
//   0x1400af370  Pf_RoadMeshRebuild_f370   — rebuild the whole road mesh (vert/index buffers)
//   0x1400aaa20  Pf_CellQuadRay_aa20       — terrain-cell quad (2-tri) ray test
//   0x1400b0f90  Pf_CellQuadRayWrap_0f90   — wrapper: build a query point, call aa20
//   0x1400ab150  Pf_RegionDirty4_b150      — enqueue up-to-4 coarse cells into the dirty AVL
//   0x1400a8c90  Pf_FineCellDirty_a8c90    — mark a fine cell + its 3x3 neighbourhood dirty
//   0x140122e20  Pf_EntityResolvePos_2e20  — recursive child-entity transform/position resolve
//   0x14004edb0  Pf_PickReachableCell_edb0 — pick a random nearby reachable cell
//   0x140087fb0  Pf_EntityAnchorCell_7fb0  — compute an entity's spawn/anchor cell + worldpos
//
// All sub-callees that are themselves in the broader closure are resolved via
// ResolveAddr(Module::Exe, 0xADDR) — they are registered in sibling batches:
//   1fa80 acc10 ae180 1f8d0 aa810 ae270 a8420 123070 1a4f0  (+ dispatch slots / engine exports).
// In DLL mode they resolve to the original exe code; in standalone they resolve to 0 (and the
// call is skipped) unless registered. This file provides ONLY the callee bodies; the single
// registration point is RegisterPfRoadBatch() in standalone/reg_pf_road.cpp.
//
// Faithfulness is everything: none of this is runtime-exercised on the autostart path, so a
// wrong port is undetectable silent corruption. Each function is a literal transcription of the
// decompile, cross-checked against disassemble_function for index math, member-ABI, the road-mesh
// dynamic buffer offsets/strides, and the implicit-arg traps. Do NOT "improve" or simplify.
//
// This file must NOT depend on hooks.h — it is portable for the standalone exe.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (M-D scoped finder fallback)

namespace {

// ------------------------------------------------------------
// Lazily-resolved exe sub-helpers + .rdata constants (resolved once).
// ------------------------------------------------------------
// Direct exe-code helpers (E8-relative CALLs in the original), all registered in sibling batches.
typedef void  (*fn_RoadVtxFill_t)(int64_t self, int x, int y, short* outVtx, uint8_t blend);     // acaa0 (this file)
typedef void  (*fn_RoadQuad_t)(int64_t self, int x, int y, int64_t outQuad, int dirIdx, uint8_t blend); // cc10 (this file)
typedef void* (*fn_RoadNormal_t)(int64_t self, void* outVec3, int x, int y);                     // ae270
typedef int   (*fn_RoadCornerBlend_t)(uint64_t a1, char c2, char c3, char c4, char c5,
                                      char c6, char c7, char c8, char c9);                        // ae180
typedef void  (*fn_GrowShortDup_t)(int64_t* arr, int newCount, void* src);                       // 1f8d0
typedef void  (*fn_GrowElemDup_t)(int64_t* arr, int newCount, void* src);                        // fa80
typedef void  (*fn_GrowElem_t)(int64_t* arr, uint32_t newCount);                                 // f250
typedef void  (*fn_GrowIntDup_t)(int64_t* arr, int newCount);                                    // 021d0
typedef void  (*fn_GrowShort_t)(int64_t* arr, int newCount);                                     // c890
typedef void  (*fn_GrowElem14_t)(int64_t* arr, int newCount);                                    // ee40

typedef uint64_t (*fn_CellRayTri_t)(uint64_t a1, float* p2, void* p3, float* p4, float* p5,
                                    float* p6, float* p7, float* p8, uint32_t* p9);               // aa810
typedef void* (*fn_CellQuadRay_t)(int64_t self, int x, int y, void* p4, float* p5, float* p6,
                                  float* p7, float* p8, uint32_t* p9);                            // aaa20 (this file)
typedef void  (*fn_AvlInsert_t)(int64_t* tree, void* outNode, uint32_t* key);                    // 1a4f0
typedef void  (*fn_MarkFineDirty_t)(int64_t self, int cell);                                     // a8420
typedef int   (*fn_PosCount_t)(int64_t posComp, int mode);                                       // 123070
typedef void* (*fn_FindCompPos_t)(int64_t entity, void* retIBP);                                 // 002ed0
typedef void* (*fn_ClearPathQueue_t)(int64_t entity, void* retIBP);                              // 003f60
typedef void* (*fn_FindComp4680_t)(int64_t entity, void* retIBP);                                // 004680
typedef void* (*fn_EntityResolvePos_t)(void* self, float* retVec3, int mode, int index,
                                       float* heading);                                          // 122e20 (this file, self-rec)
typedef uint64_t (*fn_ValidateCell_t)(int* mapGrid, uint32_t pathType, uint64_t destCell, int srcCell); // a00f0
typedef int   (*fn_FindNearestCell_t)(int* mapGrid, uint32_t pathType, uint32_t cell);           // 008830
typedef void  (*fn_GridToWorld_t)(int* mapGrid, float* out3, float* in2, char flag, void* ptr);  // 9f3e0 (f3e0)

// Dispatch-slot helpers (CALL qword ptr [0x140xxxxxx] in the original): read the runtime fn-ptr
// out of the .rdata slot (the moveto.cpp / autosavedialog.cpp idiom).
typedef void  (*fn_QuatRotate_t)(void* quat, float* outVec3, const float* inVec3);               // [0x1401525f0] (3-arg — R8 = input vec; Ghidra dropped it)
typedef float (*fn_Vec3Heading_t)(void* vec3);                                                   // [0x1401512d8]
typedef void* (*fn_TransformMul_t)(void* transform, void* retVec3, void* inVec3);                // [0x140151470] member-ABI
typedef void* (*fn_Vec3CtorScalar_t)(void* self, void* scalar);                                  // [0x140151320]
typedef uint32_t (*fn_RandomRand_t)(void* rng, uint32_t range);                                  // [0x1401512e8]

static fn_RoadNormal_t        s_fn_RoadNormal      = nullptr;  // ae270
static fn_RoadCornerBlend_t   s_fn_RoadCornerBlend = nullptr;  // ae180
static fn_GrowShortDup_t      s_fn_GrowShortDup    = nullptr;  // 1f8d0
static fn_GrowElemDup_t       s_fn_GrowElemDup     = nullptr;  // fa80
static fn_GrowElem_t          s_fn_GrowElem        = nullptr;  // f250
static fn_GrowIntDup_t        s_fn_GrowIntDup      = nullptr;  // 021d0
static fn_GrowShort_t         s_fn_GrowShort       = nullptr;  // c890
static fn_GrowElem14_t        s_fn_GrowElem14      = nullptr;  // ee40
static fn_CellRayTri_t        s_fn_CellRayTri      = nullptr;  // aa810
static fn_AvlInsert_t         s_fn_AvlInsert       = nullptr;  // 1a4f0
static fn_MarkFineDirty_t     s_fn_MarkFineDirty   = nullptr;  // a8420
static fn_PosCount_t          s_fn_PosCount        = nullptr;  // 123070
static fn_FindCompPos_t       s_fn_FindCompPos     = nullptr;  // 002ed0
static fn_ClearPathQueue_t    s_fn_ClearPathQueue  = nullptr;  // 003f60
static fn_FindComp4680_t      s_fn_FindComp4680    = nullptr;  // 004680
static fn_EntityResolvePos_t  s_fn_EntityResolvePos= nullptr;  // 122e20
static fn_ValidateCell_t      s_fn_ValidateCell    = nullptr;  // a00f0
static fn_FindNearestCell_t   s_fn_FindNearestCell = nullptr;  // 008830
static fn_GridToWorld_t       s_fn_GridToWorld     = nullptr;  // 9f3e0

static fn_QuatRotate_t        s_fn_QuatRotate      = nullptr;  // [0x1401525f0]
static fn_Vec3Heading_t       s_fn_Vec3Heading     = nullptr;  // [0x1401512d8]
static fn_TransformMul_t      s_fn_TransformMul    = nullptr;  // [0x140151470]
static fn_Vec3CtorScalar_t    s_fn_Vec3CtorScalar  = nullptr;  // [0x140151320]
static fn_RandomRand_t        s_fn_RandomRand      = nullptr;  // [0x1401512e8]

// Type-ID global + float .rdata constants used by the ports below.
static const uint32_t* s_pTid_1c7938  = nullptr;  // DAT_1401c7938 (EntityResolvePos build-comp type)
static const float* s_pUVScale        = nullptr;  // g_fUVScale (DAT_1401720b8 family) — read from exe
static const float* s_pDAT_172214     = nullptr;  // road-uv add bias
static const float* s_pDAT_17214c     = nullptr;  // caa0 height->byte add
static const float* s_pDAT_172264     = nullptr;  // caa0 ao-uv scale (short)
static const float* s_pDAT_1720bc     = nullptr;  // aa20 height scale
static const float* s_pDAT_17226c     = nullptr;  // a810 epsilon (resolved in a810; not needed here)
static const float* s_pDAT_172114     = nullptr;  // 7fb0 half-cell offset
static const float* s_pTypeTid        = nullptr;  // unused placeholder

static bool s_init = false;
static void EnsureInit() {
    if (s_init) return;
    s_fn_RoadNormal      = reinterpret_cast<fn_RoadNormal_t>(ResolveAddr(Module::Exe, 0x1400ae270));
    s_fn_RoadCornerBlend = reinterpret_cast<fn_RoadCornerBlend_t>(ResolveAddr(Module::Exe, 0x1400ae180));
    s_fn_GrowShortDup    = reinterpret_cast<fn_GrowShortDup_t>(ResolveAddr(Module::Exe, 0x14001f8d0));
    s_fn_GrowElemDup     = reinterpret_cast<fn_GrowElemDup_t>(ResolveAddr(Module::Exe, 0x14001fa80));
    s_fn_GrowElem        = reinterpret_cast<fn_GrowElem_t>(ResolveAddr(Module::Exe, 0x14001f250));
    s_fn_GrowIntDup      = reinterpret_cast<fn_GrowIntDup_t>(ResolveAddr(Module::Exe, 0x1400021d0));
    s_fn_GrowShort       = reinterpret_cast<fn_GrowShort_t>(ResolveAddr(Module::Exe, 0x14001c890));
    s_fn_GrowElem14      = reinterpret_cast<fn_GrowElem14_t>(ResolveAddr(Module::Exe, 0x14001ee40));
    s_fn_CellRayTri      = reinterpret_cast<fn_CellRayTri_t>(ResolveAddr(Module::Exe, 0x1400aa810));
    s_fn_AvlInsert       = reinterpret_cast<fn_AvlInsert_t>(ResolveAddr(Module::Exe, 0x14001a4f0));
    s_fn_MarkFineDirty   = reinterpret_cast<fn_MarkFineDirty_t>(ResolveAddr(Module::Exe, 0x1400a8420));
    s_fn_PosCount        = reinterpret_cast<fn_PosCount_t>(ResolveAddr(Module::Exe, 0x140123070));
    s_fn_FindCompPos     = reinterpret_cast<fn_FindCompPos_t>(ResolveAddr(Module::Exe, 0x140002ed0));
    s_fn_ClearPathQueue  = reinterpret_cast<fn_ClearPathQueue_t>(ResolveAddr(Module::Exe, 0x140003f60));
    s_fn_FindComp4680    = reinterpret_cast<fn_FindComp4680_t>(ResolveAddr(Module::Exe, 0x140004680));
    s_fn_EntityResolvePos= reinterpret_cast<fn_EntityResolvePos_t>(ResolveAddr(Module::Exe, 0x140122e20));
    s_fn_ValidateCell    = reinterpret_cast<fn_ValidateCell_t>(ResolveAddr(Module::Exe, 0x1400a00f0));
    s_fn_FindNearestCell = reinterpret_cast<fn_FindNearestCell_t>(ResolveAddr(Module::Exe, 0x140008830));
    s_fn_GridToWorld     = reinterpret_cast<fn_GridToWorld_t>(ResolveAddr(Module::Exe, 0x14009f3e0));

    uintptr_t slot;
    slot = ResolveAddr(Module::Exe, 0x1401525f0); if (slot) s_fn_QuatRotate     = *reinterpret_cast<fn_QuatRotate_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x1401512d8); if (slot) s_fn_Vec3Heading    = *reinterpret_cast<fn_Vec3Heading_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151470); if (slot) s_fn_TransformMul   = *reinterpret_cast<fn_TransformMul_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x140151320); if (slot) s_fn_Vec3CtorScalar = *reinterpret_cast<fn_Vec3CtorScalar_t*>(slot);
    slot = ResolveAddr(Module::Exe, 0x1401512e8); if (slot) s_fn_RandomRand     = *reinterpret_cast<fn_RandomRand_t*>(slot);

    s_pTid_1c7938 = reinterpret_cast<const uint32_t*>(ResolveAddr(Module::Exe, 0x1401c7938));
    s_pUVScale    = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401720b8));  // g_fUVScale
    s_pDAT_172214 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172214));
    s_pDAT_17214c = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
    s_pDAT_172264 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172264));
    s_pDAT_1720bc = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x1401720bc));
    s_pDAT_17226c = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017226c));
    s_pDAT_172114 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172114));
    (void)s_pTypeTid; (void)s_pDAT_17226c;
    s_init = true;
}

static inline uintptr_t GameWorld() { return reinterpret_cast<uintptr_t>(*g_pGameWorldSingleton); }

static inline float fUV()      { return s_pUVScale    ? *s_pUVScale    : 0.0f; }
static inline float f172214()  { return s_pDAT_172214 ? *s_pDAT_172214 : 0.0f; }
static inline float f17214c()  { return s_pDAT_17214c ? *s_pDAT_17214c : 0.0f; }
static inline float f172264()  { return s_pDAT_172264 ? *s_pDAT_172264 : 0.0f; }
// DAT_1401720bc = heightmap int16 -> world-height scale = 0x3b800000 = 1/256. Raw .rdata
// constant, NULL in standalone (ResolveAddr(Exe,.rdata)=0) → without this fallback the
// terrain reads flat (all corner heights 0), so the placement ray only ever "hits" z=0.
static inline float f1720bc()  { return s_pDAT_1720bc ? *s_pDAT_1720bc : 0.00390625f; }
static inline float f172114()  { return s_pDAT_172114 ? *s_pDAT_172114 : 0.5f; }

static inline int ClampCell(int v, int w) {
    if (v < 0) v = 0;
    if (v >= w) v = w - 1;
    return v;
}

} // namespace

// ============================================================
// FUN_1400acaa0 — fill one road-mesh vertex.
// ============================================================
// self (param_1): a TradeWork/road-mesh context. +0xc0 = tileSpan (>>1 = half), +0xd0 =
// heightmap width, +0xd8 = heightmap (short*). Computes the surface normal (e270) into a
// local Vector3, then writes a packed vertex into outVtx (short layout, 0x14 bytes / 10 shorts):
//   [0]=x-half, [1]=y-half, [2]=height(short from clamped heightmap), [3]=blend byte,
//   [6 lo,6 hi]=normal->byte uv pair, [13]=(normal.y+uv)*bias byte, [4],[5]=AO uv shorts.
// Bit-exact transcription (disassembly-verified for the IMUL using +0xd0 width).
void Pf_RoadVtxFill_caa0(int64_t self, int x, int y, short* outVtx, uint8_t blend) {
    EnsureInit();
    int half = *reinterpret_cast<int*>(self + 0xc0) >> 1;

    float normal[3] = {0, 0, 0};   // local_18/local_14/local_10
    if (s_fn_RoadNormal) s_fn_RoadNormal(self, normal, x, y);

    short hSv = static_cast<short>(half);
    outVtx[0] = static_cast<short>(static_cast<short>(x) - hSv);
    outVtx[1] = static_cast<short>(static_cast<short>(y) - hSv);

    // Terrain-normal -> rgba8 encode (disasm 0x1400acb1a..0x1400acba6):
    //   byte = (normal_component + 1.0) * 127.5
    // where 1.0 = DAT_14017214c (XMM3, the +1 UNORM offset) and 127.5 = DAT_140172214
    // (XMM2, the [-1,1]->[0,255] scale). Hardcoded (not via the f17214c()/f172214()
    // accessors) because in standalone ResolveAddr(Exe,.rdata) returns NULL, so those
    // accessors give their 0.0 fallback -> the encode collapses to ~0 -> the decal/road
    // shader decodes a garbage ~(-1,-1,-1) normal -> lighting washes out (decal renders
    // pale/faint instead of the retail dark, properly-lit dirt). The prior port ALSO
    // swapped the two constants (used f17214c()=1.0 as the scale, fUV() as the offset).
    const float addOff = 1.0f;    // DAT_14017214c
    const float encScl = 127.5f;  // DAT_140172214
    int hmW = *reinterpret_cast<int*>(self + 0xd0);

    int cx = (x < 0) ? 0 : x;
    if (hmW <= cx) cx = hmW - 1;
    int cy = (y < 0) ? 0 : y;
    if (hmW <= cy) cy = hmW - 1;

    short* hm = *reinterpret_cast<short**>(self + 0xd8);
    short hVal = hm[static_cast<int64_t>(hmW) * cy + cx];

    // normal encode: byte = (normal + 1.0) * 127.5  (all three components)
    float n2 = normal[2] + addOff;
    float n0 = normal[0] + addOff;
    reinterpret_cast<uint8_t*>(outVtx)[0xf] = 0;                    // *(byte*)(outVtx+0xf) = 0
    reinterpret_cast<uint16_t*>(outVtx)[3] = static_cast<uint16_t>(blend);   // outVtx[3] = (ushort)blend
    outVtx[2] = hVal;                                              // outVtx[2] = height short
    // normal->rgba8 triplet at outVtx bytes 0xc (x), 0xd (y), 0xe (z):
    reinterpret_cast<int8_t*>(outVtx)[0xc] = static_cast<int8_t>(static_cast<int64_t>(n0 * encScl));
    reinterpret_cast<int8_t*>(outVtx)[0xd] = static_cast<int8_t>(static_cast<int64_t>((normal[1] + addOff) * encScl));
    reinterpret_cast<int8_t*>(outVtx)[0xe] = static_cast<int8_t>(static_cast<int64_t>(n2 * encScl));
    // AO uv shorts at outVtx[4], outVtx[5]:
    float span = static_cast<float>(*reinterpret_cast<int*>(self + 0xc0));
    outVtx[4] = static_cast<short>(static_cast<int>((static_cast<float>(x) / span) * f172264()));
    outVtx[5] = static_cast<short>(static_cast<int>((static_cast<float>(y) / span) * f172264()));
}

// ============================================================
// FUN_1400acc10 — build the 4 vertices of one road-tile quad + its 6 indices.
// ============================================================
// outQuad (param_4): a vertex/index struct. Bytes [+0x11..] hold per-vertex blend bytes
// (the local_2c/2b/2a/29 group), shorts at [+2..] hold the per-vertex packed data. The 4
// corners are (x,y),(x+1,y),(x+1,y+1),(x,y+1); each filled via caa0 into a 0x14-byte local
// vertex (local_38..), then scattered into outQuad at stride 0x28 (puVar6 += 10 shorts;
// puVar4 += 0x14 bytes). After the 4th, the 6 index shorts are read from the (DAT_1401c23d0+
// dirIdx*8) table into [+0xc,+0xe,+0x20,+0x22,+0x34,+0x36,+0x48,+0x4a].
void Pf_RoadQuad_cc10(int64_t self, int x, int y, int64_t outQuad, int dirIdx, uint8_t blend) {
    EnsureInit();

    // local vertex scratch (0x14 bytes = 10 shorts), matches caa0's outVtx layout: shorts at
    // [0..5] (local_38..local_2e) and bytes at [0xc..0xf] (local_2c..local_29).
    short scratch[10];
    short* localVtx = scratch;  // caa0 writes a full 0x14-byte vertex here

    uint8_t* puVar4 = reinterpret_cast<uint8_t*>(outQuad + 0x11);   // byte cursor
    uint16_t* puVar6 = reinterpret_cast<uint16_t*>(outQuad + 2);    // short cursor

    int iVar5 = 0;
    while (true) {
        int iVar2 = x;
        int iVar3 = y;
        if (iVar5 == 1)      { iVar2 = x + 1; }
        else if (iVar5 == 2) { iVar3 = y + 1; iVar2 = x + 1; }
        else if (iVar5 == 3) { iVar3 = y + 1; }
        // iVar5 == 0 -> (x,y)
        Pf_RoadVtxFill_caa0(self, iVar2, iVar3, localVtx, blend);   // same TU — call directly

        iVar5++;
        // Scatter the 10-short / 4-byte local vertex into outQuad at the cursors.
        // Mirrors: puVar6[-1]=local_38; puVar4[-1]=local_2c; puVar6[3]=local_30; *puVar6=local_36;
        //          *puVar4=local_2b; puVar6[4]=local_2e; puVar6[1]=local_34; puVar4[1]=local_2a;
        //          puVar6[2]=local_32; puVar4[2]=local_29;
        // local_38=lv[0] local_36=lv[1] local_34=lv[2] local_32=lv[3] local_30=lv[4] local_2e=lv[5]
        // local_2c=lvB[0] local_2b=lvB[1] local_2a=lvB[2] local_29=lvB[3]  (the 4 bytes at lv+0xc)
        short* lv = localVtx;
        uint8_t* lvB = reinterpret_cast<uint8_t*>(localVtx) + 0xc;   // bytes 0xc,0xd,0xe,0xf
        puVar6[-1] = static_cast<uint16_t>(lv[0]);   // local_38
        puVar4[-1] = lvB[0];                         // local_2c
        puVar6[3]  = static_cast<uint16_t>(lv[4]);   // local_30
        puVar6[0]  = static_cast<uint16_t>(lv[1]);   // local_36
        puVar4[0]  = lvB[1];                         // local_2b
        puVar6[4]  = static_cast<uint16_t>(lv[5]);   // local_2e
        puVar6[1]  = static_cast<uint16_t>(lv[2]);   // local_34
        puVar4[1]  = lvB[2];                         // local_2a
        puVar6[2]  = static_cast<uint16_t>(lv[3]);   // local_32
        puVar4[2]  = lvB[3];                         // local_29

        puVar4 += 0x14;
        puVar6 += 10;
        if (3 < iVar5) {
            int64_t lVar1 = static_cast<int64_t>(dirIdx) * 8;
            // Tiled-decal / road mask-atlas rect LUT (DAT_1401c23d0). Each of the 47
            // tile-shape variants (0..46, dirIdx = Pf_TileShapeLut_ae180 result, or 46 =
            // DAT_1401c23cf for the non-tiled foundation default) has 4 uint16 giving the
            // atlas cell corners {t0,t1,t2,t3}; cc10 scatters them into the 4 vertices'
            // texcoord.zw (bytes +0xc/+0xe of each 0x14-stride vertex). The TiledDecal_ps
            // shader samples the 'tiles' mask at texcoord.zw/4096, so a NULL table (the raw
            // .rdata pointer resolves NULL in standalone — reference_standalone_rdata_null_fallback)
            // leaves texcoord.zw = 0 -> mask.y ~ 0 -> the tiled decal alpha collapses to 0
            // -> invisible. Embed the retail bytes (exact, read from 0x1401c23d0) as the
            // fallback so the mask samples the correct per-tile cell. Bytes are little-endian
            // uint16 pairs; casting the byte array to uint16_t* is valid on x64.
            alignas(2) static const uint8_t kZwLutBytes[376] = {
                128,0,128,0,128,1,128,1, 128,2,128,0,128,3,128,1,
                128,4,128,0,128,5,128,1, 128,6,128,0,128,7,128,1,
                128,8,128,0,128,9,128,1, 128,10,128,0,128,11,128,1,
                128,12,128,0,128,13,128,1, 128,14,128,0,128,15,128,1,
                128,0,128,2,128,1,128,3, 128,2,128,2,128,3,128,3,
                128,4,128,2,128,5,128,3, 128,6,128,2,128,7,128,3,
                128,8,128,2,128,9,128,3, 128,10,128,2,128,11,128,3,
                128,12,128,2,128,13,128,3, 128,14,128,2,128,15,128,3,
                128,0,128,4,128,1,128,5, 128,2,128,4,128,3,128,5,
                128,4,128,4,128,5,128,5, 128,6,128,4,128,7,128,5,
                128,8,128,4,128,9,128,5, 128,10,128,4,128,11,128,5,
                128,12,128,4,128,13,128,5, 128,14,128,4,128,15,128,5,
                128,0,128,6,128,1,128,7, 128,2,128,6,128,3,128,7,
                128,4,128,6,128,5,128,7, 128,6,128,6,128,7,128,7,
                128,8,128,6,128,9,128,7, 128,10,128,6,128,11,128,7,
                128,12,128,6,128,13,128,7, 128,14,128,6,128,15,128,7,
                128,0,128,8,128,1,128,9, 128,2,128,8,128,3,128,9,
                128,4,128,8,128,5,128,9, 128,6,128,8,128,7,128,9,
                128,8,128,8,128,9,128,9, 128,10,128,8,128,11,128,9,
                128,12,128,8,128,13,128,9, 128,14,128,8,128,15,128,9,
                128,0,128,10,128,1,128,11, 128,2,128,10,128,3,128,11,
                128,4,128,10,128,5,128,11, 128,6,128,10,128,7,128,11,
                128,8,128,10,128,9,128,11, 128,10,128,10,128,11,128,11,
                128,12,128,10,128,12,128,10,
            };
            const uint16_t* idxTbl = reinterpret_cast<const uint16_t*>(ResolveAddr(Module::Exe, 0x1401c23d0));
            if (!idxTbl) idxTbl = reinterpret_cast<const uint16_t*>(kZwLutBytes);
            if (idxTbl) {
                const uint16_t* t = reinterpret_cast<const uint16_t*>(reinterpret_cast<uintptr_t>(idxTbl) + lVar1);
                // DAT_1401c23d0=t[0], d2=t[1], d4=t[2], d6=t[3]
                *reinterpret_cast<uint16_t*>(outQuad + 0x0c) = t[0];
                *reinterpret_cast<uint16_t*>(outQuad + 0x0e) = t[1];
                *reinterpret_cast<uint16_t*>(outQuad + 0x20) = t[2];
                *reinterpret_cast<uint16_t*>(outQuad + 0x22) = t[1];
                *reinterpret_cast<uint16_t*>(outQuad + 0x34) = t[2];
                *reinterpret_cast<uint16_t*>(outQuad + 0x36) = t[3];
                *reinterpret_cast<uint16_t*>(outQuad + 0x48) = t[0];
                *reinterpret_cast<uint16_t*>(outQuad + 0x4a) = t[3];
            }
            return;
        }
    }
}

// Local forwarder so cc10 can call the caa0 reimpl directly (same translation unit).
static inline void s_fn_RoadVtxFill_caa0_call(int64_t self, int x, int y, short* outVtx, uint8_t blend) {
    Pf_RoadVtxFill_caa0(self, x, y, outVtx, blend);
}

// ============================================================
// FUN_1400aaa20 — terrain-cell quad (2-tri) ray test.
// ============================================================
// Reads the heightmap (self+0xd0 width, +0xd8 short*) for the 4 corners of cell (x,y),
// builds two triangles split by parity of (x+y), and ray-tests each via aa810. On a hit,
// writes the world hit point into param_7 (= ray-origin param_5 + t*dir param_4) and t into
// param_8. Returns whether any tri was hit. Big local Vector3 scratch preserved as in the
// original (a810 keeps its own 64KB-class scratch; here only the small locals are needed).
int Pf_CellQuadRay_aa20(int64_t self, int x, int y, void* p4 /*Vector3* dir*/, float* p5 /*origin*/,
                        float* p6 /*maxT*/, float* p7 /*outHit3*/, float* p8 /*outT*/, uint32_t* p9) {
    EnsureInit();
    int half = *reinterpret_cast<int*>(self + 0xc0) >> 1;
    int hmW = *reinterpret_cast<int*>(self + 0xd0);
    int64_t lW = hmW;
    short* hm = *reinterpret_cast<short**>(self + 0xd8);
    float hscale = f1720bc();
    // Cell-corner world spacing. The original (disasm 0x1400aaaf7) adds *0x14017214c = 1.0
    // to step to the next corner — NOT fUV()=*0x1401720b8=0.001, which collapses the quad
    // to a near-degenerate sliver so aa810's det≈0 and every ray misses. Raw .rdata => null
    // in standalone => 1.0 fallback.
    float uv = 1.0f;
    {
        const float* pUv = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x14017214c));
        if (pUv) uv = *pUv;
    }

    // Heightmap sample at (col,row), clamped to the map — matches the original's
    // per-corner ClampCell of x/x+1 and y/y+1. height = int16 sample * hscale.
    auto H = [&](int col, int row) -> float {
        int cc = ClampCell(col, hmW);
        int cr = ClampCell(row, hmW);
        return static_cast<float>(static_cast<int>(hm[cr * lW + cc])) * hscale;
    };

    float bx = static_cast<float>(x - half);   // base world X (cell corner)
    float by = static_cast<float>(y - half);   // base world Y

    // 🔑 The 4 quad corners as CONTIGUOUS Vector3 {x,y,z}. aa810 reads each &V as a
    // Vector3, so x/y/z MUST be adjacent in memory. The previous port used separate
    // float locals (local_58/54/50 …) and even (void)-cast the corner HEIGHTS, so every
    // triangle passed to aa810 had a garbage/absent z → degenerate → the ray never hit
    // the terrain (root cause of "no ghost / CreateBar stuck top-left"). z = corner height.
    float V00[3] = { bx,      by,      H(x,     y)     };  // (x,   y)
    float V01[3] = { bx + uv, by,      H(x + 1, y)     };  // (x+1, y)
    float V10[3] = { bx,      by + uv, H(x,     y + 1) };  // (x,   y+1)
    float V11[3] = { bx + uv, by + uv, H(x + 1, y + 1) };  // (x+1, y+1)

    float maxTbuf[1] = { *p6 };
    uint64_t maxBits = static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&maxTbuf[0]));
    float outT[1] = { 0.0f };

    // Split the quad into two triangles by (x+y) parity, ray-test each (aa810).
    bool hit;
    if (((x + y) & 1) == 0) {
        uint64_t r = s_fn_CellRayTri ? s_fn_CellRayTri(maxBits, p5, p4, maxTbuf, V00, V01, V10, outT, p9) : 0;
        hit = (static_cast<char>(r) != 0);
        if (hit) maxTbuf[0] = outT[0];
        uint64_t r2 = s_fn_CellRayTri ? s_fn_CellRayTri(maxBits, p5, p4, maxTbuf, V01, V11, V10, outT, p9) : 0;
        if (static_cast<char>(r2) != 0) hit = true;
    } else {
        uint64_t r = s_fn_CellRayTri ? s_fn_CellRayTri(maxBits, p5, p4, maxTbuf, V00, V01, V11, outT, p9) : 0;
        hit = (static_cast<char>(r) != 0);
        if (hit) maxTbuf[0] = outT[0];
        uint64_t r2 = s_fn_CellRayTri ? s_fn_CellRayTri(maxBits, p5, p4, maxTbuf, V00, V11, V10, outT, p9) : 0;
        if (static_cast<char>(r2) != 0) hit = true;
    }

    // outHit = origin + t*dir  (p4 = dir Vector3; p5 = origin).
    float* dir = reinterpret_cast<float*>(p4);
    float t = outT[0];
    p7[0] = t * dir[0] + p5[0];
    p7[1] = t * dir[1] + p5[1];
    p7[2] = t * dir[2] + p5[2];
    *p8 = t;
    return hit ? 1 : 0;
}

// ============================================================
// FUN_1400b0f90 — wrapper: build a query point, call aa20.
// ============================================================
// Builds a downward ray (disassembly-derived, 0x1400b0f90):
//   origin (p5) = (param_2[0], param_2[1], DAT_140172208)   [RSP+0x50..0x58]
//   dir    (p4) = (0, 0, DAT_140172280)                     [RSP+0x60..0x68]
//   maxT   (p6) = DAT_140172228 (single float)
//   p7 = outHit scratch, p8 = outT scratch, p9 = param_3.
// cell = ((param_2 + half) truncated). The aa20 result is discarded (the original's empty
// `if (bVar1) return; return;`).
void Pf_CellQuadRayWrap_0f90(int64_t self, float* param_2, uint32_t* param_3) {
    EnsureInit();
    const float* pD208 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172208));
    const float* pD280 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172280));
    const float* pD228 = reinterpret_cast<const float*>(ResolveAddr(Module::Exe, 0x140172228));

    float origin[3];   // p5 = [RSP+0x50..0x58]
    origin[0] = param_2[0];
    origin[1] = param_2[1];
    origin[2] = pD208 ? *pD208 : 0.0f;     // DAT_140172208

    float dir[3] = {0.0f, 0.0f, 0.0f};     // p4 = [RSP+0x60..0x68]
    dir[2] = pD280 ? *pD280 : 0.0f;        // DAT_140172280

    float maxT = pD228 ? *pD228 : 0.0f;    // p6 = DAT_140172228 (single float)

    float half = static_cast<float>(*reinterpret_cast<int*>(self + 0xc0) >> 1);
    float outHit[3] = {0, 0, 0};           // p7
    float outT = 0.0f;                     // p8 ([RSP+0x78], read back as the result but discarded)

    Pf_CellQuadRay_aa20(self,
                        static_cast<int>(origin[0] + half),
                        static_cast<int>(origin[1] + half),
                        dir, origin, &maxT, outHit, &outT, param_3);
}

// ============================================================
// FUN_1400ab150 — enqueue up-to-4 coarse cells into the dirty AVL.
// ============================================================
// self (param_1): a region graph. +0xc8 = coarse cell size, +0xc0 = grid width, +0xe8 = the
// dirty-cell AVL tree. Given a fine (x,y), if x (resp. y) lands on a coarse boundary and the
// previous coarse column/row exists, insert the 3 adjacent coarse cells; always insert the
// owning coarse cell. Each insert via 1a4f0(tree, &outNode, &key).
void Pf_RegionDirty4_b150(int64_t self, int x, int y) {
    EnsureInit();
    int cs = *reinterpret_cast<int*>(self + 0xc8);     // coarse cell size
    int qx = x / cs;
    int qy = y / cs;
    int cw = *reinterpret_cast<int*>(self + 0xc0) / cs; // coarse grid width

    int64_t* tree = reinterpret_cast<int64_t*>(self + 0xe8);
    uint64_t outNode = 0;     // local_res8
    uint32_t key[2] = {0, 0}; // local_res10

    if ((x % cs == 0) && (-1 < qx - 1)) {
        key[0] = static_cast<uint32_t>(cw * qy - 1 + qx);
        if (s_fn_AvlInsert) s_fn_AvlInsert(tree, &outNode, key);
    }
    if ((y % cs == 0) && (-1 < qy - 1)) {
        key[0] = static_cast<uint32_t>((qy - 1) * cw + qx);
        if (s_fn_AvlInsert) s_fn_AvlInsert(tree, &outNode, key);
    }
    if ((x % cs == 0) && (y % cs == 0) && (-1 < qx - 1) && (-1 < qy - 1)) {
        key[0] = static_cast<uint32_t>((qy - 1) * cw - 1 + qx);
        if (s_fn_AvlInsert) s_fn_AvlInsert(tree, &outNode, key);
    }
    key[0] = static_cast<uint32_t>(cw * qy + qx);
    if (s_fn_AvlInsert) s_fn_AvlInsert(tree, &outNode, key);
}

// ============================================================
// FUN_1400a8c90 — mark a fine cell + its 3x3 neighbourhood dirty.
// ============================================================
// self (param_1): a region/dirty grid. +0x58 cellW, +0x5c cellH, +0x60 gridW, +0x64 gridH,
// +0x78 cell array (stride 0x20: [+0]=col, [+4]=row, [+8]=dirtyFlag byte), +0x88 dirty-list
// (int* ptr), +0x90 count, +0x94 cap. If the owning coarse cell isn't already flagged, flag it,
// append its index to the dirty list (grow-by-3 on a non-empty append), then walk the 3x3 fine
// neighbourhood and call a8420 on each in-bounds fine cell.
void Pf_FineCellDirty_a8c90(int64_t self, int x, int y) {
    EnsureInit();
    int cellW = *reinterpret_cast<int*>(self + 0x58);
    int cellH = *reinterpret_cast<int*>(self + 0x5c);
    int gridW = *reinterpret_cast<int*>(self + 0x60);
    int idx = (y / cellH) * gridW + (x / cellW);
    int64_t lOff = static_cast<int64_t>(idx) * 0x20;
    int64_t cellArr = *reinterpret_cast<int64_t*>(self + 0x78);

    if (*reinterpret_cast<char*>(cellArr + 8 + lOff) != '\0') return;
    *reinterpret_cast<uint8_t*>(*reinterpret_cast<int64_t*>(self + 0x78) + 8 + lOff) = 1;

    // append idx to dirty list (int array): ptr@+0x88, count@+0x90, cap@+0x94
    int oldN = *reinterpret_cast<int*>(self + 0x90);
    int newN = oldN + 1;
    if (newN != oldN) {
        void* oldBuf = *reinterpret_cast<void**>(self + 0x88);
        *reinterpret_cast<int*>(self + 0x90) = newN;
        if (*reinterpret_cast<int*>(self + 0x94) < newN) {
            *reinterpret_cast<int*>(self + 0x94) = newN;
            if (oldN != 0) *reinterpret_cast<int*>(self + 0x94) = newN * 3;
            void* nb = fn_MemAlloc
                ? fn_MemAlloc(g_pMemAllocSingleton,
                              static_cast<uint64_t>(*reinterpret_cast<int*>(self + 0x94)) << 2, 4)
                : nullptr;
            *reinterpret_cast<void**>(self + 0x88) = nb;
        }
        int srcIdx = (oldBuf == *reinterpret_cast<void**>(self + 0x88)) ? oldN : 0;
        for (int64_t i = srcIdx; i < newN; i++) {
            int* dst = reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self + 0x88) + i * 4);
            if (dst) {
                int v = idx;
                if (i < oldN) v = reinterpret_cast<int*>(oldBuf)[i];
                *dst = v;
            }
        }
        if (oldBuf != *reinterpret_cast<void**>(self + 0x88) && fn_MemFreeStatic)
            fn_MemFreeStatic(oldBuf);
    }

    // 3x3 fine neighbourhood: rows (row-1 .. row+1), cols (col-1 .. col+1)
    int baseCol = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self + 0x78) + lOff);     // [+0]
    int rowStart = *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(self + 0x78) + 4 + lOff) - 1; // [+4]-1
    int gridH = *reinterpret_cast<int*>(self + 0x64);
    int fineGW = *reinterpret_cast<int*>(self + 0x60);
    for (int r = 0; r < 3; r++) {
        int rr = rowStart + r;
        if (-1 < rr && rr < gridH) {
            int cc = baseCol - 1;
            for (int c = 0; c < 3; c++) {
                if (-1 < cc && cc < fineGW) {
                    if (s_fn_MarkFineDirty) s_fn_MarkFineDirty(self, rr * fineGW + cc);
                }
                cc++;
            }
        }
    }
}

// ============================================================
// FUN_140122e20 — recursive child-entity transform/position resolve.
// ============================================================
// Member-style hidden-return ABI (disassembly-verified): RCX=self, RDX=retVec3, R8=mode(p3),
// R9=index(p4), [stack]=heading(p5). Returns retVec3.
// self: +0x18 = candidate build component (validated IsDerivedFrom DAT_1401c7938), +0x28 child
// IBP array base, +0x30 child count. The build component holds per-index running counts at
// +((p3+3)*0x10). If that count <= p4, walk children accumulating PosCount(child,p3); recurse
// into the owning child with (p4 - runningCount). Otherwise resolve this entity's origin:
// ClearPathQueue(*(self+0x10), &ibp) → object; optionally Rotate the quaternion at obj+0x148 to
// get heading; then Transform::operator*(obj+0x148 transform, retVec3).
void* Pf_EntityResolvePos_2e20(void* self, float* retVec3, int p3, int p4, float* p5) {
    EnsureInit();
    uintptr_t s = reinterpret_cast<uintptr_t>(self);
    void* buildRaw = *reinterpret_cast<void**>(s + 0x18);

    uint32_t tid = s_pTid_1c7938 ? *s_pTid_1c7938 : 0;
    void* build = nullptr;
    if (fn_IsDerivedFrom && fn_IsDerivedFrom(buildRaw, tid)) build = buildRaw;

    int local_68 = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(build) +
                                           (static_cast<int64_t>(p3) + 3) * 0x10);

    if (local_68 <= p4) {
        int childN = *reinterpret_cast<int*>(s + 0x30);
        int i = 0;
        if (0 < childN) {
            int64_t off = 0;
            do {
                int64_t* node = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(s + 0x28) + off);
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node);
                int n = s_fn_PosCount ? s_fn_PosCount(*node, p3) : 0;
                if (local_68 <= p4 && p4 < n + local_68) {
                    int64_t* node2 = reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(s + 0x28) +
                                                                static_cast<int64_t>(i) * 8);
                    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(node2);
                    if (s_fn_EntityResolvePos)
                        s_fn_EntityResolvePos(reinterpret_cast<void*>(*node2), retVec3, p3, p4 - local_68, p5);
                    return retVec3;
                }
                i++;
                off += 8;
                local_68 += n;
            } while (i < *reinterpret_cast<int*>(s + 0x30));
        }
        int zero = 0;
        if (s_fn_Vec3CtorScalar) s_fn_Vec3CtorScalar(retVec3, &zero);   // Math::Vector3::Vector3(ret,&scalar0)
        return retVec3;
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(s + 0x10));
    uintptr_t ibp = 0;   // local_res8 (object pointer after ClearPathQueue)
    if (s_fn_ClearPathQueue) s_fn_ClearPathQueue(*reinterpret_cast<int64_t*>(s + 0x10), &ibp);
    // [M-D SCOPED FALLBACK] "ClearPathQueue" = the shared ModelComponent finder
    // FUN_140003f60, whose type-id comes from raw .rdata 0x1401c7a50 — NULL in
    // standalone, so the find never matches (the GrowthComponent +0x30 lesson;
    // the shared global must stay unfilled). Self-contained scan of the owning
    // entity; if it still has no ModelComponent, return a zero vec instead of
    // the retail unguarded `null+0x148` transform read.
    if (ibp == 0) {
        static uint32_t s_tidModel = 0;
        if (s_tidModel == 0) s_tidModel = ::Object::GetTypeIdByName("ModelComponent");
        int64_t ownEnt = *reinterpret_cast<int64_t*>(s + 0x10);
        if (ownEnt && s_tidModel) {
            int n = *reinterpret_cast<int*>(ownEnt + 0x38);
            int64_t arr = *reinterpret_cast<int64_t*>(ownEnt + 0x30);
            for (int k = 0; k < n && arr; ++k) {
                void* c = *reinterpret_cast<void**>(arr + (int64_t)k * 8);
                if (!c) continue;
                uintptr_t vt = *reinterpret_cast<uintptr_t*>(c);
                typedef uint32_t (*getTid_t)(void*);
                if ((*reinterpret_cast<getTid_t*>(vt + 0x10))(c) == s_tidModel) {
                    ibp = reinterpret_cast<uintptr_t>(c);
                    break;
                }
            }
        }
        if (ibp == 0) {
            if (p5) *p5 = 0.0f;
            retVec3[0] = retVec3[1] = retVec3[2] = 0.0f;
            return retVec3;
        }
    }

    // Point node for (p3, p4) from the description's pair list — disasm-verified
    // (0x140122ea2..0x140122f42): pair {array,count} stride 0x10 at desc+0x28+p3*0x10;
    // node stride 0x18 = {offset.xyz @0x0, dir.xyz @0xc}. The heading branch rotates
    // node.dir (R8 = arr + 0xc + p4*0x18) and the tail transforms node.offset
    // (R8 = arr + p4*0x18). Both R8 args were Ghidra-dropped in the old port
    // (passed as nullptr -> Runtime read of 0 once interact points went live).
    uint8_t* nodeArr = build
        ? *reinterpret_cast<uint8_t**>(reinterpret_cast<uintptr_t>(build) + 0x28 +
                                       static_cast<size_t>(p3) * 0x10)
        : nullptr;
    uint8_t* node = nodeArr ? nodeArr + static_cast<size_t>(p4) * 0x18 : nullptr;
    static const float kZero3[3] = {0, 0, 0};

    if (p5 != nullptr) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ibp);
        uint32_t xform[8];   // local_50..local_38 = 7 dwords copied from obj+0x148
        for (int k = 0; k < 7; k++)
            xform[k] = *reinterpret_cast<uint32_t*>(ibp + 0x148 + k * 4);
        float dir[4] = {0, 0, 0, 0};   // local_60 (Vector3 out)
        if (s_fn_QuatRotate)
            s_fn_QuatRotate(xform, dir, node ? reinterpret_cast<float*>(node + 0xc) : kZero3);
        float h = s_fn_Vec3Heading ? s_fn_Vec3Heading(dir) : 0.0f;
        *p5 = h;
    }

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&ibp);
    uint32_t xform2[8];
    for (int k = 0; k < 7; k++)
        xform2[k] = *reinterpret_cast<uint32_t*>(ibp + 0x148 + k * 4);
    if (s_fn_TransformMul)
        s_fn_TransformMul(xform2, retVec3,
                          node ? reinterpret_cast<float*>(node) : const_cast<float*>(kZero3));
    return retVec3;
}

// ============================================================
// FUN_14004edb0 — pick a random nearby reachable cell.
// ============================================================
// param_1 = an entity-bearing object (+0x10 IBP). Finds the Position component (002ed0),
// gets its sub-position count (123070), rolls a random index, resolves that sub-position's
// world XY (122e20), converts to a grid cell, then validates reachability (a00f0 with path
// type 0xe1f) — if reachable, returns the nearest reachable cell (008830), else -1.
int Pf_PickReachableCell_edb0(int64_t param_1, uint32_t param_2, float* param_3) {
    EnsureInit();
    int* mapGrid = *reinterpret_cast<int**>(GameWorld() + 0xf8);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(reinterpret_cast<void*>(param_1 + 0x10));
    uintptr_t posIBP = 0;   // local_res8
    if (s_fn_FindCompPos) s_fn_FindCompPos(*reinterpret_cast<int64_t*>(param_1 + 0x10), &posIBP);

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
    uint32_t cnt = s_fn_PosCount ? static_cast<uint32_t>(s_fn_PosCount(posIBP, 0)) : 0;
    uint32_t rnd = s_fn_RandomRand ? s_fn_RandomRand(*reinterpret_cast<void**>(GameWorld() + 0xd8), cnt) : 0;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
    float pos[3] = {0, 0, 0};   // local_28/24/20
    if (s_fn_EntityResolvePos)
        s_fn_EntityResolvePos(reinterpret_cast<void*>(posIBP), pos, 0, static_cast<int>(rnd), nullptr);
    param_3[0] = pos[0];
    param_3[1] = pos[1];
    param_3[2] = pos[2];

    int w = *mapGrid;
    int gy = static_cast<int>(static_cast<float>(w >> 1) + param_3[1]);
    int gx = static_cast<int>(static_cast<float>(w >> 1) + param_3[0]);
    gx = ClampCell(gx, w);
    gy = ClampCell(gy, w);
    uint32_t cell = static_cast<uint32_t>(w * gy + gx);

    uint64_t ok = s_fn_ValidateCell ? s_fn_ValidateCell(mapGrid, 0xe1f, static_cast<uint64_t>(param_2), static_cast<int>(cell)) : 0;
    if (static_cast<char>(ok) == '\0') return -1;
    return s_fn_FindNearestCell ? s_fn_FindNearestCell(mapGrid, param_2, cell) : -1;
}

// ============================================================
// FUN_140087fb0 — compute an entity's spawn/anchor cell + worldpos.
// ============================================================
// param_1 = entity IBP, param_2 = optional out world pos (float[3]). Returns the grid cell
// index (or -1). If the entity has a Position component (002ed0) with >0 sub-positions,
// optionally roll a random sub-index, resolve sub-position 0 (122e20), and map world XY ->
// grid cell. Else fall back to FUN_140004680's component, reading its packed short tile coords
// at +0x28/+0x2a (col/row) and +0x2c/+0x2e (offset halves). If param_2 given and cell>=0,
// fill the world pos via GridToWorld (f3e0).
int Pf_EntityAnchorCell_7fb0(void* param_1IBP, float* param_2) {
    EnsureInit();
    int iVar4 = -1;

    if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1IBP);
    uintptr_t posIBP = 0;   // local_res8
    if (s_fn_FindCompPos) s_fn_FindCompPos(*reinterpret_cast<int64_t*>(param_1IBP), &posIBP);

    bool handled = false;
    if (posIBP != 0) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
        int n = s_fn_PosCount ? s_fn_PosCount(posIBP, 0) : 0;
        if (0 < n) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
            int n2 = s_fn_PosCount ? s_fn_PosCount(posIBP, 0) : 0;
            if (1 < n2) {
                if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
                uint32_t n3 = s_fn_PosCount ? static_cast<uint32_t>(s_fn_PosCount(posIBP, 0)) : 0;
                if (s_fn_RandomRand) s_fn_RandomRand(*reinterpret_cast<void**>(GameWorld() + 0xd8), n3); // result discarded
            }
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&posIBP);
            float pos[3] = {0, 0, 0};   // local_28/24/20
            if (s_fn_EntityResolvePos)
                s_fn_EntityResolvePos(reinterpret_cast<void*>(posIBP), pos, 0, 0, nullptr);
            int w = **reinterpret_cast<int**>(GameWorld() + 0xf8);
            int gx = static_cast<int>(pos[0] + static_cast<float>(w >> 1));
            int gy = static_cast<int>(pos[1] + static_cast<float>(w >> 1));
            gx = ClampCell(gx, w);
            gy = ClampCell(gy, w);
            iVar4 = w * gy + gx;
            handled = true;
        }
    }

    if (!handled) {
        if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(param_1IBP);
        uintptr_t compIBP = 0;   // local_res10
        if (s_fn_FindComp4680) s_fn_FindComp4680(*reinterpret_cast<int64_t*>(param_1IBP), &compIBP);
        if (compIBP != 0) {
            if (fn_InternalBasePtr_CheckPtr) fn_InternalBasePtr_CheckPtr(&compIBP);
            int w = **reinterpret_cast<int**>(GameWorld() + 0xf8);
            int row = static_cast<int>(*reinterpret_cast<int16_t*>(compIBP + 0x2a));
            int offR = static_cast<int>(*reinterpret_cast<int16_t*>(compIBP + 0x2e)) >> 1;
            int col = static_cast<int>(*reinterpret_cast<int16_t*>(compIBP + 0x28));
            int offC = static_cast<int>(*reinterpret_cast<int16_t*>(compIBP + 0x2c)) >> 1;
            iVar4 = (offR + row) * w + offC + col;
        }
    }

    if (param_2 != nullptr && -1 < iVar4) {
        int* mapGrid = *reinterpret_cast<int**>(GameWorld() + 0xf8);
        float half = static_cast<float>(*mapGrid >> 1) - f172114();   // DAT_140172114
        float local[3];
        local[0] = static_cast<float>(iVar4 % *mapGrid) - half;
        local[1] = static_cast<float>(iVar4 / *mapGrid) - half;
        local[2] = 0.0f;
        float wp[3] = {0, 0, 0};   // local_28..local_20
        if (s_fn_GridToWorld) s_fn_GridToWorld(mapGrid, wp, local, '\0', nullptr);
        param_2[0] = wp[0];
        param_2[1] = wp[1];
        param_2[2] = wp[2];
    }
    return iVar4;
}


// ============================================================
// FUN_1400af370 — rebuild the whole road mesh (per-tile verts/indices).
// ============================================================
// self (param_1): a road-mesh builder. param_2: the region/tile source.
//   *(int*)param_2          = gridDim (fine grid dimension).
//   self+0x200              = the per-batch mesh-record array (longlong[2]: ptr@0/count@1) — owned
//                             by the f250/fa80 grow helpers (record stride 0x60).
//   self+0xc0 / +0xc4       = tile span / inset; (span - inset) >> 1 = vtxOff.
//   self+0xc8 (=200 dec)    = coarse-cell pixel size (block dimension).
//   self+0x210              = per-batch source-ptr table (qword per batch).
//   self+0x218              = batch count; +0x220 = per-batch fill-counter array (int); +0x228 = len.
//   *(param_2+0x20)         = the fine-cell grid (stride 0x30): byte +0x2a = batchId, +0x2b = blendId.
//
// Working array local_108 (longlong[2]: ptr@0/count@1) holds `batchCount` records, each 0x60 bytes:
//   +0x00 srcPtr;  vert short-buffer B {ptr@+0x08, cnt@+0x10, cap@+0x14};
//   index short-buffer A {ptr@+0x18, cnt@+0x20, cap@+0x24};  header ints +0x28..+0x34.
//
// Phases: (1) sync the +0x220 counter array length to +0x218 (021d0) then zero it; (2) f250-size
// local_108 to +0x218 records; per coarse block, seed each record header (srcPtr from +0x210,
// min/extent ints) and reset its two buffers (c890/ee40); (3) per fine cell read batchId/blendId,
// build the 3x3 corner-occupancy mask, fold it to a corner blend value (ae180), build the quad
// (cc10), append 8 corner indices to buffer A (the parity-split grow-blocks via AppendShortA),
// and push the 4 verts into buffer B (1f8d0) after overwriting their AO-uv slots (+0x08/+0x0a)
// with the per-corner (fineCol,fineRow)+offset tile coords; (4) per batch with counter>0 flush B
// into the mesh record (fa80); (5) free both buffers per record + local_108.
//
// Index details — BOTH pinned against the disassembly (0x1400af588..0xaff6e):
//   * mask -> ae180 arg order (p2..p9) = mask[3],[5],[1],[7],[0],[2],[6],[8] (slot = rowDelta*3+col,
//     rowDelta/col in {-1,0,1}; center slot[4] forced 1).  Verified at 0xaf61f..0xaf662.
//   * the vertex push overwrites quad vert+0x08 (AO-u) with local_c8[8+i*2]+fineCol and vert+0x0a
//     (AO-v) with local_c8[i*2]+fineRow, i.e. corner (xOff,yOff)+tile.  local_c8 (16 shorts) =
//     {0,0,0,0,1,0,1,0, 0,0,1,0,1,0,0,0}; the quad's AO slots are at vert+0x08/+0x0a per cc10.
//
// AppendShortA: the inline "append one short to buffer A of a record" grow+write block, identical
// across the 8 grow-blocks (they differ only by the value written to a freshly-grown slot). Reads
// count fresh each call; new = old+1; cap rule "cap=new; if new==old+1 && old!=0 cap=new*3".
static void Pf_AppendShortA_f370(uintptr_t recBase, short valueForNew) {
    int* pCount = reinterpret_cast<int*>(recBase + 0x20);
    int* pCap   = reinterpret_cast<int*>(recBase + 0x24);
    void** pPtr = reinterpret_cast<void**>(recBase + 0x18);
    int oldN = *pCount;
    int newN = oldN + 1;
    if (newN == oldN) return;
    void* oldBuf = *pPtr;
    *pCount = newN;
    if (*pCap < newN) {
        *pCap = newN;
        if ((newN == oldN + 1) && (oldN != 0)) *pCap = newN * 3;
        void* nb = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, static_cast<uint64_t>(*pCap) * 2, 2) : nullptr;
        *pPtr = nb;
    }
    int start = (oldBuf == *pPtr) ? oldN : 0;
    for (int64_t j = start; j < newN; j++) {
        short* dst = reinterpret_cast<short*>(reinterpret_cast<uintptr_t>(*pPtr) + j * 2);
        if (dst) {
            short v = valueForNew;
            if (j < oldN) v = reinterpret_cast<short*>(oldBuf)[j];
            *dst = v;
        }
    }
    if (oldBuf != *pPtr && fn_MemFreeStatic) fn_MemFreeStatic(oldBuf);
}

void Pf_RoadMeshRebuild_f370(int64_t param_1, uint32_t* param_2) {
    EnsureInit();

    int64_t* meshRecArr = reinterpret_cast<int64_t*>(param_1 + 0x200);   // local_a8
    if (s_fn_GrowElem) s_fn_GrowElem(meshRecArr, 0);                     // f250(local_a8, 0)

    int vtxOff = (*reinterpret_cast<int*>(param_1 + 0xc0) - *reinterpret_cast<int*>(param_1 + 0xc4)) >> 1; // iVar12

    // Phase 1: counter array length sync (+0x228 vs +0x218) then zero every counter.
    if (*reinterpret_cast<int*>(param_1 + 0x228) != *reinterpret_cast<int*>(param_1 + 0x218)) {
        if (s_fn_GrowIntDup) s_fn_GrowIntDup(reinterpret_cast<int64_t*>(param_1 + 0x220),
                                             *reinterpret_cast<int*>(param_1 + 0x218));
    }
    {
        int cnt = *reinterpret_cast<int*>(param_1 + 0x228);
        for (int i = 0; i < cnt; i++)
            *reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 0x220) + static_cast<int64_t>(i) * 4) = 0;
    }

    // Phase 2: local working batch array, sized to the batch count.
    int64_t local_108[2] = {0, 0};   // {ptr@0, count@1 (int low dword)}
    if (s_fn_GrowElem) s_fn_GrowElem(local_108, *reinterpret_cast<uint32_t*>(param_1 + 0x218));

    int batchCount = static_cast<int>(local_108[1]);          // (int)local_100
    int gridDim = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(param_2));  // MOVSXD R13,[RDI]
    int cellSize = *reinterpret_cast<int*>(param_1 + 0xc8);   // +0xc8 = 200 dec (block dim)

    // local_c8 corner-offset table (16 shorts), used by the per-vertex push.
    static const short kCornerOff[16] = {0,0,0,0,1,0,1,0, 0,0,1,0,1,0,0,0};

    int local_f8 = 0;
    if (0 < gridDim) {
        do {                                   // outer block row (local_f8)
            int local_f4 = 0;
            do {                               // outer block col (local_f4)
                // ---- seed each batch record header for this block ----
                if (0 < batchCount) {
                    int seedColMin = local_f4 + vtxOff;   // iVar26 (R15D = local_f4+vtxOff)
                    int seedRowMin = local_f8 + vtxOff;   // iVar16 (R12D = local_f8+vtxOff)
                    int64_t lVar13 = 0;
                    uintptr_t recBase = static_cast<uintptr_t>(local_108[0]);
                    for (int b = 0; b < batchCount; b++) {
                        *reinterpret_cast<int64_t*>(recBase + 0x00) =
                            *reinterpret_cast<int64_t*>(*reinterpret_cast<int64_t*>(param_1 + 0x210) + lVar13);
                        if (s_fn_GrowShort)  s_fn_GrowShort(reinterpret_cast<int64_t*>(recBase + 0x08), 0);   // c890 (buffer B)
                        if (s_fn_GrowElem14) s_fn_GrowElem14(reinterpret_cast<int64_t*>(recBase + 0x18), 0);  // ee40 (buffer A)
                        *reinterpret_cast<int*>(recBase + 0x28) = seedColMin;
                        *reinterpret_cast<int*>(recBase + 0x2c) = seedRowMin;
                        *reinterpret_cast<int*>(recBase + 0x30) = *reinterpret_cast<int*>(param_1 + 200) + local_f4 + vtxOff;
                        *reinterpret_cast<int*>(recBase + 0x34) = *reinterpret_cast<int*>(param_1 + 200) + local_f8 + vtxOff;
                        recBase += 0x60;
                        lVar13 += 8;
                    }
                }

                // ---- per fine cell in this block ----
                int local_d4 = 0;
                if (0 < cellSize) {
                    do {                       // fine row within block (local_d4)
                        if (0 < cellSize) {
                            int fineRow = local_f8 + local_d4;     // local_dc (R8)
                            int rowBase = fineRow * gridDim;       // local_d0
                            int local_e0 = 0;
                            do {               // fine col within block (local_e0)
                                int fineCol = local_e0 + local_f4; // iVar20 (local_cc)
                                uint64_t cellIdx = static_cast<uint64_t>(rowBase + fineCol);
                                int64_t fineGrid = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(param_2) + 0x20); // *(param_2+0x20)
                                uint8_t batchId = *reinterpret_cast<uint8_t*>(fineGrid + 0x2a + cellIdx * 0x30);
                                uint8_t blendId = *reinterpret_cast<uint8_t*>(fineGrid + 0x2b + cellIdx * 0x30);

                                if (batchId != 0xff) {
                                    if (blendId == 0xff) {
                                        int* ctr = reinterpret_cast<int*>(*reinterpret_cast<int64_t*>(param_1 + 0x220) + static_cast<uint64_t>(batchId) * 4);
                                        *ctr += 1;
                                    }

                                    // ---- 3x3 corner-occupancy mask local_f0[0..8] ----
                                    char local_f0[9];
                                    {
                                        int slot = 0;
                                        int row = fineRow - 1;          // R9D (iVar26)
                                        for (int rd = 0; rd < 3; rd++) {   // rowDelta = -1,0,1 (lVar19 / lVar9=3)
                                            int rowDelta = rd - 1;
                                            int64_t rowOff = static_cast<int64_t>(row) * gridDim; // lVar18
                                            for (int cd = -1; cd < 2; cd++) {   // iVar17 = -1,0,1
                                                if (cd == 0 && rowDelta == 0) {
                                                    local_f0[slot] = 1;
                                                } else {
                                                    int iVar1 = cd + fineCol;
                                                    if (iVar1 < 0 || iVar1 >= gridDim || row < 0 || row >= gridDim) {
                                                        local_f0[slot] = 0;
                                                    } else {
                                                        int64_t fg = *reinterpret_cast<int64_t*>(reinterpret_cast<uintptr_t>(param_2) + 0x20);
                                                        local_f0[slot] = (*reinterpret_cast<uint8_t*>(
                                                            fg + 0x2a + (static_cast<int64_t>(iVar1) + rowOff) * 0x30) != 0xff) ? 1 : 0;
                                                    }
                                                }
                                                slot++;
                                            }
                                            row++;
                                        }
                                    }

                                    int blendVal = s_fn_RoadCornerBlend
                                        ? s_fn_RoadCornerBlend(0,
                                                               local_f0[3], local_f0[5], local_f0[1], local_f0[7],
                                                               local_f0[0], local_f0[2], local_f0[6], local_f0[8])
                                        : 0;

                                    // build the quad (cc10) into the 0x60-byte scratch (local_98 / outQuad).
                                    uint8_t quad[0x60];
                                    Pf_RoadQuad_cc10(param_1, fineCol + vtxOff, fineRow + vtxOff,
                                                     reinterpret_cast<int64_t>(quad), blendVal, blendId);

                                    uint64_t e = batchId;
                                    uintptr_t recBase = static_cast<uintptr_t>(local_108[0]) + e * 0x60;
                                    short sVar21 = static_cast<short>(*reinterpret_cast<uint32_t*>(recBase + 0x10)); // *(...+e*0xc+2)

                                    // ---- push the 8 corner indices (parity split on (rowSeed+vtxOff + colSeed+vtxOff)) ----
                                    if (((fineRow + vtxOff + fineCol + vtxOff) & 1) == 0) {
                                        Pf_AppendShortA_f370(recBase, sVar21);       // +0
                                        Pf_AppendShortA_f370(recBase, sVar21 + 3);   // +3
                                        Pf_AppendShortA_f370(recBase, sVar21 + 1);   // +1
                                        Pf_AppendShortA_f370(recBase, sVar21 + 1);   // +1
                                        Pf_AppendShortA_f370(recBase, sVar21 + 3);   // +3
                                        Pf_AppendShortA_f370(recBase, sVar21 + 2);   // +2
                                    } else {
                                        Pf_AppendShortA_f370(recBase, sVar21);       // +0
                                        Pf_AppendShortA_f370(recBase, sVar21 + 2);   // +2
                                        Pf_AppendShortA_f370(recBase, sVar21 + 1);   // +1
                                        Pf_AppendShortA_f370(recBase, sVar21);       // +0
                                        Pf_AppendShortA_f370(recBase, sVar21 + 3);   // +3
                                        Pf_AppendShortA_f370(recBase, sVar21 + 2);   // +2
                                    }

                                    // ---- push the 4 quad verts into buffer B (1f8d0), overwriting the
                                    //      AO-uv slots (+0x08/+0x0a) with the per-corner tile coords. ----
                                    {
                                        int64_t* bufB = reinterpret_cast<int64_t*>(recBase + 0x08);
                                        for (int vi = 0; vi < 4; vi++) {
                                            short cX = static_cast<short>(kCornerOff[8 + vi * 2] + static_cast<short>(fineCol)); // local_c8[+0x10 + i*4]
                                            short cY = static_cast<short>(kCornerOff[vi * 2]     + static_cast<short>(fineRow)); // local_c8[+0 + i*4]
                                            uint8_t* srcV = quad + static_cast<size_t>(vi) * 0x14;   // puVar14 += 0x14
                                            *reinterpret_cast<short*>(srcV + 0x08) = cX;   // AO-u slot
                                            *reinterpret_cast<short*>(srcV + 0x0a) = cY;   // AO-v slot
                                            int curCount = *reinterpret_cast<int*>(recBase + 0x10);  // (int)plVar15[1]
                                            if (s_fn_GrowShortDup) s_fn_GrowShortDup(bufB, curCount + 1, srcV);
                                        }
                                    }
                                }
                                local_e0++;
                            } while (local_e0 < *reinterpret_cast<int*>(param_1 + 0xc8));
                        }
                        local_d4++;
                    } while (local_d4 < *reinterpret_cast<int*>(param_1 + 0xc8));
                }

                // ---- flush each batch's buffer B into its mesh record (fa80) when counter>0 ----
                if (0 < batchCount) {
                    uintptr_t recBase = static_cast<uintptr_t>(local_108[0]);
                    for (int b = 0; b < batchCount; b++) {
                        if (0 < *reinterpret_cast<int*>(recBase + 0x10)) {
                            if (s_fn_GrowElemDup) s_fn_GrowElemDup(meshRecArr, static_cast<int>(meshRecArr[1]) + 1, reinterpret_cast<void*>(recBase));
                        }
                        recBase += 0x60;
                    }
                }

                local_f4 += *reinterpret_cast<int*>(param_1 + 0xc8);
            } while (local_f4 < gridDim);
            local_f8 += *reinterpret_cast<int*>(param_1 + 0xc8);
        } while (local_f8 < gridDim);
    }

    // Phase 5: free every record's two buffers, then the array.
    {
        int n = static_cast<int>(local_108[1]);
        uintptr_t recBase = static_cast<uintptr_t>(local_108[0]);
        for (int b = 0; b < n; b++) {
            if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(recBase + 0x18));   // buffer A
            *reinterpret_cast<int64_t*>(recBase + 0x18) = 0;
            *reinterpret_cast<int64_t*>(recBase + 0x20) = 0;
            if (fn_MemFreeStatic) fn_MemFreeStatic(*reinterpret_cast<void**>(recBase + 0x08));   // buffer B
            *reinterpret_cast<int64_t*>(recBase + 0x08) = 0;
            *reinterpret_cast<int64_t*>(recBase + 0x10) = 0;
            recBase += 0x60;
        }
        if (fn_MemFreeStatic) fn_MemFreeStatic(reinterpret_cast<void*>(local_108[0]));
    }
}
