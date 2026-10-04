// code/Game/Action/action_pf_road.cpp
// Wave 3 (Action) — pathfinding / road-mesh-rebuild / geometry-wrapper / entity-resolve
// closure batch (the "PF-ROAD" family).
//
// Faithful 1:1 ports of 11 exe helpers (decompiled + disassembly-verified in Ghidra). The big
// one (0x1400af370 road-mesh rebuild) had two index-math details pinned against the disassembly:
// the 3x3 mask -> ae180 arg order and the per-vertex AO-uv overwrite offsets (see its block).
//   0x1400acaa0  Pf_RoadVtxFill_caa0       — fill one road-mesh vertex (uv/height/blend)
//   0x1400acc10  Pf_RoadQuad_cc10          — build the 4 verts of one road-tile quad
#include "Action/exe_bodies.h"
//   0x1400af370  Pf_RoadMeshRebuild_f370   — rebuild the whole road mesh (vert/index buffers)
//   0x1400aaa20  Pf_CellQuadRay_aa20       — terrain-cell quad (2-tri) ray test
//   0x1400b0f90  Pf_CellQuadRayWrap_0f90   — wrapper: build a query point, call aa20
//   0x1400ab150  Pf_RegionDirty4_b150      — enqueue up-to-4 coarse cells into the dirty AVL
//   0x1400a8c90  Pf_FineCellDirty_a8c90    — mark a fine cell + its 3x3 neighbourhood dirty
//   0x140122e20  Pf_EntityResolvePos_2e20  — recursive child-entity transform/position resolve
//   0x14004edb0  Pf_PickReachableCell_edb0 — pick a random nearby reachable cell
//   0x140087fb0  Pf_EntityAnchorCell_7fb0  — compute an entity's spawn/anchor cell + worldpos
//
// All sub-callees that are themselves in the broader closure are called by name (B3R):
//   1fa80 acc10 ae180 1f8d0 aa810 ae270 a8420 123070 1a4f0  (+ engine members).
// The registration point RegisterPfRoadBatch() (standalone/reg_pf_road.cpp) stays for readers
// other blocks have.
//
// Faithfulness is everything: none of this is runtime-exercised on the autostart path, so a
// wrong port is undetectable silent corruption. Each function is a literal transcription of the
// decompile, cross-checked against disassemble_function for index math, member-ABI, the road-mesh
// dynamic buffer offsets/strides, and the implicit-arg traps. Do NOT "improve" or simplify.
//

#include "object/TypeInfo.h"
#include "Action/PathSearch.h"   // RegionGraph
#include "Game/Map.h"
#include "Game/WorldGeometry.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Work/exe_bodies.h"          // WorkBase_FindComponent_2ed0, WorkBase_ValidateCell_a00f0, WorkBase_CalcDistance_8830
#include "Component/exe_bodies.h"     // FloorComponent_FindResData
#include "Game/exe_bodies.h"          // Reimpl_TradeWork_helper_e270, Reimpl_Struct60B_Resize, Reimpl_MapGetTerrainPos
#include "Game/exe_constants.h"       // kRdata_*
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include <cstring>
#include "math/Math.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "Object/factory_registration.h"   // ::Object::GetTypeIdByName (M-D scoped finder fallback)
#include "Component/coll_bridge.h"
#include "system/Value.h"      // ENDGAME B.3.4b System callers
#include "Component/ModelComponent.h"   // ENDGAME B.3.8 CB3: ModelComponent::TypeId()
#include "Component/InteractDescription.h"
#include "Component/InteractComponent.h"
#include "Component/MapComponent.h"
#include "Entity/ComponentEntity.h"

namespace {

// caa0's packed vertex (0x14 bytes; bytes +0x10..+0x13 are never written).
struct RoadVertexScratch {
    int16_t  x;              // +0x00 x - half
    int16_t  y;              // +0x02 y - half
    int16_t  height;         // +0x04 the clamped heightmap sample
    uint16_t blend;          // +0x06
    int16_t  aoU;            // +0x08
    int16_t  aoV;            // +0x0A
    int8_t   normal[3];      // +0x0C (normal + 1) * 127.5
    uint8_t  normalW;        // +0x0F 0
    uint8_t  _pad10[4];      // +0x10
};
static_assert(sizeof(RoadVertexScratch) == 0x14, "caa0 vertex is 0x14");
static_assert(offsetof(RoadVertexScratch, aoU) == 0x08, "RoadVertexScratch::aoU @ +0x08");
static_assert(offsetof(RoadVertexScratch, normal) == 0x0C, "RoadVertexScratch::normal @ +0x0C");
static_assert(offsetof(RoadVertexScratch, normalW) == 0x0F, "RoadVertexScratch::normalW @ +0x0F");

// The road-mesh vertex (0x14 bytes), as cc10 scatters it and f370 pushes it.
// B.3.2 bridge: FUN_14001ee40 / FUN_14001f8d0 are Collection::Array<T>::SetSize /
// SetSize(n,fill) monomorphised over this 0x14-byte, align-2 element. Neither body's
// new-tail arm touches the tail (ee40 has literally no else branch; f8d0's else
// branch is the 3-arg fill copy), so the element must be TRIVIALLY default
// constructible -- rtcoll::SpecFor<T> then emits init=nullptr, matching retail's
// uninitialised new slot exactly. Its int16 fields give the 0x14001ee40/f8d0
// MemAlloc(.., 2) alignment.
struct RoadVertex {
    int16_t  x;              // +0x00
    int16_t  y;              // +0x02
    int16_t  height;         // +0x04
    uint16_t blend;          // +0x06
    int16_t  aoU;            // +0x08 (f370 overwrites with the corner's tile column)
    int16_t  aoV;            // +0x0A (f370 overwrites with the corner's tile row)
    uint16_t tileU;          // +0x0C mask-atlas corner (DAT_1401c23d0)
    uint16_t tileV;          // +0x0E
    int8_t   normal[3];      // +0x10
    uint8_t  normalW;        // +0x13
};
static_assert(sizeof(RoadVertex) == 0x14, "RoadVertex stride");
static_assert(alignof(RoadVertex) == 2, "RoadVertex align");
static_assert(offsetof(RoadVertex, aoU) == 0x08, "RoadVertex::aoU @ +0x08");
static_assert(offsetof(RoadVertex, tileU) == 0x0C, "RoadVertex::tileU @ +0x0C");
static_assert(offsetof(RoadVertex, normal) == 0x10, "RoadVertex::normal @ +0x10");
static_assert(std::is_trivially_copyable<RoadVertex>::value, "RoadVertex byteCopy");
static_assert(std::is_trivially_destructible<RoadVertex>::value, "RoadVertex destroy-free");
static_assert(std::is_trivially_default_constructible<RoadVertex>::value,
              "RoadVertex must stay uninitialised on the new-tail arm (SpecFor: "
              "init = !is_trivially_default_constructible)");

// The 0x60-byte mesh batch record (0x14001f250 / 0x14001fa80 stride 0x60): f370's working
// array and the WorldGeometry +0x200 array it flushes into.
struct RoadBatch {
    int64_t  source;                   // +0x00 the batch's entry of the +0x210 table
    RawArray<RoadVertex> vertices;     // +0x08 buffer B
    RawArray<int16_t>    indices;      // +0x18 buffer A
    int32_t  slot;                     // +0x28 slot handle (-1 = free, seeded by f250)
    int32_t  minX;                     // +0x2C culling AABB in tile coords
    int32_t  minY;                     // +0x30
    int32_t  maxX;                     // +0x34
    int32_t  maxY;                     // +0x38
    uint8_t  _pad3C[0x24];             // +0x3C
};
static_assert(sizeof(RoadBatch) == 0x60, "RoadBatch stride 0x60");
static_assert(offsetof(RoadBatch, vertices) == 0x08, "RoadBatch::vertices @ +0x08");
static_assert(offsetof(RoadBatch, indices) == 0x18, "RoadBatch::indices @ +0x18");
static_assert(offsetof(RoadBatch, slot) == 0x28, "RoadBatch::slot @ +0x28");
static_assert(offsetof(RoadBatch, minX) == 0x2C, "RoadBatch::minX @ +0x2C");
static_assert(offsetof(RoadBatch, maxY) == 0x38, "RoadBatch::maxY @ +0x38");

// ------------------------------------------------------------
// Exe sub-helpers + .rdata constants, called / read by name (B3R).
// ------------------------------------------------------------
//   ae270 Reimpl_TradeWork_helper_e270 (road normal)    ae180 Pf_TileShapeLut_ae180
//   fa80  Pf_VectorResize60_fa80 (grow + dup element)   f250  Reimpl_Struct60B_Resize
//   aa810 Pf_RayTriIntersect_aa810                       1a4f0 Pf_AvlInsertA_a4f0
//   a8420 Pf_DirtyCoarseCellB_8420                       123070 Pf_RecursiveChildSum_123070
//   002ed0 WorkBase_FindComponent_2ed0                   003f60 ActCNR_FindComponent_3f60
//   004680 FloorComponent_FindResData                    122e20 Pf_EntityResolvePos_2e20 (self-rec)
//   a00f0 WorkBase_ValidateCell_a00f0                    008830 WorkBase_CalcDistance_8830
//   9f3e0 Reimpl_MapGetTerrainPos (f3e0, grid -> world)
// 021d0 (int-array SetSize), c890 (uint16 SetSize) and 1f8d0/ee40 (0x14-byte-element
// SetSize/SetSizeFill) are the exe-side Collection::Array<T> monomorphisations --
// ENDGAME B.3.2 step 2 converted their call sites below to GameColl::AsArray<T>
// member calls, so the resolved-pointer stand-ins for them are gone.
// B.3.1: Math::Quaternion::Rotate [0x1401525f0] (3-arg — R8 = input vec; Ghidra dropped it),
// Math::Vector3::Heading [0x1401512d8], Math::Transform::operator*(Vector3) [0x140151470]
// and the Math::Vector3 splat ctor [0x140151320] are direct members of the real classes.
//
// .rdata constants (Game/exe_constants.h): 0x140172264 (caa0 ao-uv scale), 0x1401720bc
// (aa20 height scale, 1/256), 0x140172114 (7fb0 half-cell offset), 0x14017214c (1.0),
// 0x140172208 / 0x140172280 / 0x140172228 (0f90 query origin z / dir z / maxT).
// (This file also resolved 0x1401720b8, 0x140172214 and 0x14017226c into pointers
// nothing read; those dead reads are gone.)

// The .rdata float constants that are stored as bit patterns.
inline float RdataF(uint32_t bits) { float f; std::memcpy(&f, &bits, sizeof f); return f; }

static inline float f172264()  { return RdataF(kRdata_140172264); }   // 0x140172264
// DAT_1401720bc = heightmap int16 -> world-height scale = 0x3b800000 = 1/256.
static inline float f1720bc()  { return RdataF(kRdata_1401720bc); }   // 0x1401720bc
static inline float f172114()  { return kRdata_140172114; }           // 0x140172114

static inline int ClampCell(int v, int w) {
    if (v < 0) v = 0;
    if (v >= w) v = w - 1;
    return v;
}

} // namespace

// ============================================================
// FUN_1400acaa0 — fill one road-mesh vertex.
// ============================================================
// `geo`: the WorldGeometry. _gridSize = tile span (>>1 = half), _fieldD0 = heightmap width,
// _arrayD8 = the heightmap (int16). Computes the surface normal (e270) into a local Vector3,
// then writes the packed vertex: x-half, y-half, the clamped heightmap sample, the blend,
// the normal as three bytes, the AO uv shorts.
// Bit-exact transcription (disassembly-verified for the IMUL using the heightmap width).
static void FillRoadVertex(WorldGeometry* geo, int x, int y, RoadVertexScratch* outVtx, uint8_t blend) {
    int half = geo->_gridSize >> 1;

    float normal[3] = {0, 0, 0};   // local_18/local_14/local_10
    Reimpl_TradeWork_helper_e270(geo, normal, x, y);   // 0x1400ae270

    short hSv = static_cast<short>(half);
    outVtx->x = static_cast<short>(static_cast<short>(x) - hSv);
    outVtx->y = static_cast<short>(static_cast<short>(y) - hSv);

    // Terrain-normal -> rgba8 encode (disasm 0x1400acb1a..0x1400acba6):
    //   byte = (normal_component + 1.0) * 127.5
    // where 1.0 = DAT_14017214c (XMM3, the +1 UNORM offset) and 127.5 = DAT_140172214
    // (XMM2, the [-1,1]->[0,255] scale). Hardcoded: an earlier port read them through
    // accessors that fell back to 0.0 (the encode collapsed to ~0 -> garbage normal ->
    // washed-out decal lighting) and ALSO swapped the two constants.
    const float addOff = 1.0f;    // DAT_14017214c
    const float encScl = 127.5f;  // DAT_140172214
    int hmW = geo->_fieldD0;

    int cx = (x < 0) ? 0 : x;
    if (hmW <= cx) cx = hmW - 1;
    int cy = (y < 0) ? 0 : y;
    if (hmW <= cy) cy = hmW - 1;

    const int16_t* hm = geo->_arrayD8;
    short hVal = hm[static_cast<int64_t>(hmW) * cy + cx];

    // normal encode: byte = (normal + 1.0) * 127.5  (all three components)
    float n2 = normal[2] + addOff;
    float n0 = normal[0] + addOff;
    outVtx->normalW = 0;
    outVtx->blend = static_cast<uint16_t>(blend);
    outVtx->height = hVal;
    outVtx->normal[0] = static_cast<int8_t>(static_cast<int64_t>(n0 * encScl));
    outVtx->normal[1] = static_cast<int8_t>(static_cast<int64_t>((normal[1] + addOff) * encScl));
    outVtx->normal[2] = static_cast<int8_t>(static_cast<int64_t>(n2 * encScl));
    // AO uv shorts:
    float span = static_cast<float>(geo->_gridSize);
    outVtx->aoU = static_cast<short>(static_cast<int>((static_cast<float>(x) / span) * f172264()));
    outVtx->aoV = static_cast<short>(static_cast<int>((static_cast<float>(y) / span) * f172264()));
}

// ============================================================
// FUN_1400acc10 — build the 4 vertices of one road-tile quad.
// ============================================================
// The 4 corners are (x,y),(x+1,y),(x+1,y+1),(x,y+1); each filled via caa0 into a local
// vertex, then scattered into quad[i] (the normal bytes move from +0x0C to +0x10). After the
// 4th, the corners' mask-atlas coordinates come from the (DAT_1401c23d0 + shape*8) table.
static void BuildRoadQuad(WorldGeometry* geo, int x, int y, RoadVertex* quad, int dirIdx, uint8_t blend) {
    RoadVertexScratch localVtx;   // caa0 writes a full 0x14-byte vertex here (local_38..)

    int iVar5 = 0;
    while (true) {
        int iVar2 = x;
        int iVar3 = y;
        if (iVar5 == 1)      { iVar2 = x + 1; }
        else if (iVar5 == 2) { iVar3 = y + 1; iVar2 = x + 1; }
        else if (iVar5 == 3) { iVar3 = y + 1; }
        // iVar5 == 0 -> (x,y)
        FillRoadVertex(geo, iVar2, iVar3, &localVtx, blend);   // 0x1400acaa0

        // Scatter the local vertex into quad[iVar5], in retail's store order.
        RoadVertex& v = quad[iVar5];
        iVar5++;
        v.x         = localVtx.x;     // local_38
        v.normal[0] = localVtx.normal[0];                   // local_2c
        v.aoU       = localVtx.aoU;                         // local_30
        v.y         = localVtx.y;                           // local_36
        v.normal[1] = localVtx.normal[1];                   // local_2b
        v.aoV       = localVtx.aoV;                         // local_2e
        v.height    = localVtx.height;                      // local_34
        v.normal[2] = localVtx.normal[2];                   // local_2a
        v.blend     = localVtx.blend;                       // local_32
        v.normalW   = localVtx.normalW;                     // local_29

        if (3 < iVar5) {
            // Tiled-decal / road mask-atlas rect LUT (DAT_1401c23d0). Each of the 47
            // tile-shape variants (0..46, dirIdx = Pf_TileShapeLut_ae180 result, or 46 =
            // DAT_1401c23cf for the non-tiled foundation default) has 4 uint16 giving the
            // atlas cell corners {t0,t1,t2,t3}; cc10 scatters them into the 4 vertices'
            // texcoord.zw. The TiledDecal_ps shader samples the 'tiles' mask at
            // texcoord.zw/4096, so a zero table leaves the tiled decal invisible.
            // PORT GAP 0x1401c23d0 tiled-decal mask-atlas rect LUT: not registered; retail
            // reads the .rdata table here. The tree's copy of the retail values (read from
            // 0x1401c23d0) is the code (B3R R4).
            static const uint16_t kZwLut[188] = {
                128,128,384,384,640,128,896,384,
                1152,128,1408,384,1664,128,1920,384,
                2176,128,2432,384,2688,128,2944,384,
                3200,128,3456,384,3712,128,3968,384,
                128,640,384,896,640,640,896,896,
                1152,640,1408,896,1664,640,1920,896,
                2176,640,2432,896,2688,640,2944,896,
                3200,640,3456,896,3712,640,3968,896,
                128,1152,384,1408,640,1152,896,1408,
                1152,1152,1408,1408,1664,1152,1920,1408,
                2176,1152,2432,1408,2688,1152,2944,1408,
                3200,1152,3456,1408,3712,1152,3968,1408,
                128,1664,384,1920,640,1664,896,1920,
                1152,1664,1408,1920,1664,1664,1920,1920,
                2176,1664,2432,1920,2688,1664,2944,1920,
                3200,1664,3456,1920,3712,1664,3968,1920,
                128,2176,384,2432,640,2176,896,2432,
                1152,2176,1408,2432,1664,2176,1920,2432,
                2176,2176,2432,2432,2688,2176,2944,2432,
                3200,2176,3456,2432,3712,2176,3968,2432,
                128,2688,384,2944,640,2688,896,2944,
                1152,2688,1408,2944,1664,2688,1920,2944,
                2176,2688,2432,2944,2688,2688,2944,2944,
                3200,2688,3200,2688,
            };
            const uint16_t* t = &kZwLut[static_cast<int64_t>(dirIdx) * 4];
            // DAT_1401c23d0=t[0], d2=t[1], d4=t[2], d6=t[3]
            quad[0].tileU = t[0];
            quad[0].tileV = t[1];
            quad[1].tileU = t[2];
            quad[1].tileV = t[1];
            quad[2].tileU = t[2];
            quad[2].tileV = t[3];
            quad[3].tileU = t[0];
            quad[3].tileV = t[3];
            return;
        }
    }
}

// ============================================================
// FUN_1400aaa20 — terrain-cell quad (2-tri) ray test.
// ============================================================
// Reads the heightmap (_fieldD0 width, _arrayD8) for the 4 corners of cell (x,y), builds
// two triangles split by parity of (x+y), and ray-tests each via aa810. Writes the world
// hit point (origin + t*dir) into outHit and t into outT. Returns whether any tri was hit.
static int RayTestCell(WorldGeometry* geo, int x, int y, const float* dir, const float* origin,
                const float* maxT, float* outHit, float* outT, float* outNormal) {
    int half = geo->_gridSize >> 1;
    int hmW = geo->_fieldD0;
    int64_t lW = hmW;
    const int16_t* hm = geo->_arrayD8;
    float hscale = f1720bc();
    // Cell-corner world spacing. The original (disasm 0x1400aaaf7) adds *0x14017214c = 1.0
    // to step to the next corner — NOT *0x1401720b8=0.001, which collapses the quad
    // to a near-degenerate sliver so aa810's det≈0 and every ray misses.
    float uv = kRdata_14017214c;   // 0x14017214c

    // Heightmap sample at (col,row), clamped to the map — matches the original's
    // per-corner ClampCell of x/x+1 and y/y+1. height = int16 sample * hscale.
    auto H = [&](int col, int row) -> float {
        int cc = ClampCell(col, hmW);
        int cr = ClampCell(row, hmW);
        return static_cast<float>(static_cast<int>(hm[cr * lW + cc])) * hscale;
    };

    float bx = static_cast<float>(x - half);   // base world X (cell corner)
    float by = static_cast<float>(y - half);   // base world Y

    // The 4 quad corners as CONTIGUOUS Vector3 {x,y,z}: aa810 reads each as a Vector3.
    // z = corner height.
    float V00[3] = { bx,      by,      H(x,     y)     };  // (x,   y)
    float V01[3] = { bx + uv, by,      H(x + 1, y)     };  // (x+1, y)
    float V10[3] = { bx,      by + uv, H(x,     y + 1) };  // (x,   y+1)
    float V11[3] = { bx + uv, by + uv, H(x + 1, y + 1) };  // (x+1, y+1)

    float maxTbuf[1] = { *maxT };
    float tOut[1] = { 0.0f };

    // Split the quad into two triangles by (x+y) parity, ray-test each (aa810). aa810's
    // first argument (RCX) is never read.
    bool hit;
    if (((x + y) & 1) == 0) {
        uint64_t r = Pf_RayTriIntersect_aa810(nullptr, origin, dir, maxTbuf, V00, V01, V10, tOut, outNormal);
        hit = (static_cast<char>(r) != 0);
        if (hit) maxTbuf[0] = tOut[0];
        uint64_t r2 = Pf_RayTriIntersect_aa810(nullptr, origin, dir, maxTbuf, V01, V11, V10, tOut, outNormal);
        if (static_cast<char>(r2) != 0) hit = true;
    } else {
        uint64_t r = Pf_RayTriIntersect_aa810(nullptr, origin, dir, maxTbuf, V00, V01, V11, tOut, outNormal);
        hit = (static_cast<char>(r) != 0);
        if (hit) maxTbuf[0] = tOut[0];
        uint64_t r2 = Pf_RayTriIntersect_aa810(nullptr, origin, dir, maxTbuf, V00, V11, V10, tOut, outNormal);
        if (static_cast<char>(r2) != 0) hit = true;
    }

    // outHit = origin + t*dir.
    float t = tOut[0];
    outHit[0] = t * dir[0] + origin[0];
    outHit[1] = t * dir[1] + origin[1];
    outHit[2] = t * dir[2] + origin[2];
    *outT = t;
    return hit ? 1 : 0;
}

// ============================================================
// FUN_1400b0f90 — the terrain height under a point: a downward ray through aa20.
// ============================================================
// Builds a downward ray (disassembly-derived, 0x1400b0f90):
//   origin = (point[0], point[1], DAT_140172208)   [RSP+0x50..0x58]
//   dir    = (0, 0, DAT_140172280)                 [RSP+0x60..0x68]
//   maxT   = DAT_140172228 (single float)
// cell = ((point + half) truncated).
//
// ⚠ RETURNS A FLOAT — the terrain hit HEIGHT. Ghidra types this `void`; the disassembly:
//     CALL 0x1400aaa20 ; TEST AL,AL ; JZ miss
//     MOVSS XMM0,[RSP+0x78] ; RET      <- hit : return outHit[2]  (Z of the ray hit)
//   miss: XORPS XMM0,XMM0 ; RET        <- miss: return 0.0f
// (PathTool::Draw's worker-route segments read it as each segment's Z; found live
// 2026-08-20.)
static float TerrainHeightUnder(WorldGeometry* geo, const float* point, float* outNormal) {

    float origin[3];   // [RSP+0x50..0x58]
    origin[0] = point[0];
    origin[1] = point[1];
    origin[2] = RdataF(kRdata_140172208);  // DAT_140172208

    float dir[3] = {0.0f, 0.0f, 0.0f};     // [RSP+0x60..0x68]
    dir[2] = kRdata_140172280;             // DAT_140172280

    float maxT = RdataF(kRdata_140172228);  // DAT_140172228 (single float)

    float half = static_cast<float>(geo->_gridSize >> 1);
    // outHit spans [R11-0x18 .. R11-0x10] — three floats, and the third IS the return value.
    // The t out is a separate scratch (retail's local_res10, a float[6]); size both
    // generously so a callee that writes more than we expect cannot clobber a neighbour.
    float outHit[4]  = {0, 0, 0, 0};       // outHit[2] is the returned height
    float scratch[8] = {0, 0, 0, 0, 0, 0, 0, 0};

    int hit = RayTestCell(geo,
                          static_cast<int>(origin[0] + half),
                          static_cast<int>(origin[1] + half),
                          dir, origin, &maxT, outHit, scratch, outNormal);   // 0x1400aaa20
    return hit ? outHit[2] : 0.0f;
}

// ============================================================
// FUN_1400ab150 — enqueue up-to-4 coarse cells into the dirty AVL.
// ============================================================
// WorldGeometry::MarkChunksDirty. _chunkSize = cells per chunk edge, _gridSize = the
// heightmap edge, the dirty-chunk tree at +0xe8. Given a cell (x,y), if x (resp. y) lands on
// a chunk boundary and the previous chunk column/row exists, insert the adjacent chunks;
// always insert the owning chunk. Each insert via 0x14001a4f0(tree, &outNode, &key).
void WorldGeometry::MarkChunksDirty(int x, int y) {
    int cs = _chunkSize;                // cells per chunk edge
    int qx = x / cs;
    int qy = y / cs;
    int cw = _gridSize / cs;            // chunks per row

    // LEFT: the dirty-chunk tree is two raw members (_dirtyTreeRoot / _dirtyTreeCount) of
    // WorldGeometry (block GAME-c); 0x14001a4f0 takes it as one Set header.
    int64_t* tree = reinterpret_cast<int64_t*>(&_dirtyTreeRoot);   // LEFT: Game/WorldGeometry.h _dirtyTreeRoot raw (GAME-2); SIG Pf_AvlInsertA_a4f0
    uint64_t outNode = 0;     // local_res8
    uint32_t key[2] = {0, 0}; // local_res10

    if ((x % cs == 0) && (-1 < qx - 1)) {
        key[0] = static_cast<uint32_t>(cw * qy - 1 + qx);
        Pf_AvlInsertA_a4f0(tree, &outNode, key);   // 0x14001a4f0
    }
    if ((y % cs == 0) && (-1 < qy - 1)) {
        key[0] = static_cast<uint32_t>((qy - 1) * cw + qx);
        Pf_AvlInsertA_a4f0(tree, &outNode, key);   // 0x14001a4f0
    }
    if ((x % cs == 0) && (y % cs == 0) && (-1 < qx - 1) && (-1 < qy - 1)) {
        key[0] = static_cast<uint32_t>((qy - 1) * cw - 1 + qx);
        Pf_AvlInsertA_a4f0(tree, &outNode, key);   // 0x14001a4f0
    }
    key[0] = static_cast<uint32_t>(cw * qy + qx);
    Pf_AvlInsertA_a4f0(tree, &outNode, key);   // 0x14001a4f0
}

// B4 FORWARD: 1 outside caller (Component/terraformcomponent.cpp)
void Pf_RegionDirty4_b150(int64_t self, int x, int y) {
    reinterpret_cast<WorldGeometry*>(self)->MarkChunksDirty(x, y);   // LEFT: SIG Pf_RegionDirty4_b150
}

// ============================================================
// FUN_1400a8c90 — mark a fine cell + its 3x3 neighbourhood dirty.
// ============================================================
// self (param_1): a region/dirty grid. +0x58 cellW, +0x5c cellH, +0x60 gridW, +0x64 gridH,
// +0x78 cell array (stride 0x20: [+0]=col, [+4]=row, [+8]=dirtyFlag byte), +0x88 dirty-list
// (int* ptr), +0x90 count, +0x94 cap. If the owning coarse cell isn't already flagged, flag it,
// append its index to the dirty list (grow-by-3 on a non-empty append), then walk the 3x3 fine
// neighbourhood and call a8420 on each in-bounds fine cell.
void RegionGraph::MarkCellDirty(int x, int y) {
    int cellW = blockWidth;
    int cellH = blockHeight;
    int gridW = blocksWide;
    int idx = (y / cellH) * gridW + (x / cellW);
    int64_t lOff = static_cast<int64_t>(idx);
    const RegionBlock* cellArr = blocks.data;

    if (cellArr[lOff].needsRebuild != '\0') return;
    blocks.data[lOff].needsRebuild = 1;

    // append idx to dirtyRebuild
    int oldN = dirtyRebuild._size;
    int newN = oldN + 1;
    if (newN != oldN) {
        int32_t* oldBuf = dirtyRebuild._data;
        dirtyRebuild._size = newN;
        if (dirtyRebuild._capacity < newN) {
            dirtyRebuild._capacity = newN;
            if (oldN != 0) dirtyRebuild._capacity = newN * 3;
            dirtyRebuild._data = static_cast<int32_t*>(System::MemoryAllocator::MemAlloc(
                              static_cast<uint64_t>(dirtyRebuild._capacity) << 2, 4));
        }
        int srcIdx = (oldBuf == dirtyRebuild._data) ? oldN : 0;
        for (int64_t i = srcIdx; i < newN; i++) {
            int* dst = &dirtyRebuild._data[i];
            if (dst) {
                int v = idx;
                if (i < oldN) v = oldBuf[i];
                *dst = v;
            }
        }
        if (oldBuf != dirtyRebuild._data)
            System::MemoryAllocator::MemFree(oldBuf);
    }

    // 3x3 block neighbourhood: rows (row-1 .. row+1), cols (col-1 .. col+1)
    int baseCol = blocks.data[lOff].x;
    int rowStart = blocks.data[lOff].y - 1;
    int gridH = blocksHigh;
    int fineGW = blocksWide;
    for (int r = 0; r < 3; r++) {
        int rr = rowStart + r;
        if (-1 < rr && rr < gridH) {
            int cc = baseCol - 1;
            for (int c = 0; c < 3; c++) {
                if (-1 < cc && cc < fineGW) {
                    MarkAdjacencyDirty(rr * fineGW + cc);   // 0x1400a8420
                }
                cc++;
            }
        }
    }
}

// B4 FORWARD: 7 outside callers (cemeterycomponent, citizen_lifeevent_handler,
// pasturecomponent, quarrycomponent_deform, terraformcomponent, starttown,
// keystone_a_removalcalcpath)
void Pf_FineCellDirty_a8c90(int64_t self, int x, int y) {
    reinterpret_cast<RegionGraph*>(self)->MarkCellDirty(x, y);   // LEFT: SIG Pf_FineCellDirty_a8c90
}

// ============================================================
// FUN_140122e20 — the world position of an interact node.
// ============================================================
// Member-style hidden-return ABI (disassembly-verified): RCX=self, RDX=retVec3, R8=kind(p3),
// R9=index(p4), [stack]=heading(p5). Returns retVec3.
// The component's InteractDescription holds the node arrays (0 use, 1 build, 2 create). If
// this component's own `kind` count <= index, walk the sub-locations accumulating their
// counts (0x140123070) and recurse into the owning one with (index - running count).
// Otherwise resolve the node through the entity's ModelComponent transform: optionally
// rotate the node's direction to get a heading, then transform the node's offset.
float* InteractNodePosition(const InteractComponent* self, float* retVec3, int p3, int p4, float* p5) {
    Description* buildRaw = self->_description;

    uint32_t tid = InteractDescription::TypeId();
    const InteractDescription* build = nullptr;
    if (::Object::IsDerivedFromNullSafe(buildRaw, tid)) build = static_cast<const InteractDescription*>(buildRaw);

    // [SA-GUARD 2026-07-20] Retail reads the node count unguarded — its data guarantees the
    // description passes the 1c7938 check. Standalone component wiring can miss it (crashed
    // live: ClearStorage_Execute on the upgrade haul-away resolving the house position).
    // Zero-vec fallback, same policy as the ModelComponent fallback below.
    if (!build) {
        // A caller landing here has almost certainly picked the WRONG finder: this
        // wants an InteractComponent (0x140002ed0, type DWORD_1401c7900), whose description
        // IS an InteractDescription. A BuildComponent (0x14000d6c0, DWORD_1401c7008)
        // carries a BuildDescription, a SIBLING of InteractDescription -- the cast can
        // never pass, and the zero vec below silently becomes the map centre.
        // (Cost PathTool's route builder every workplace position; fixed 2026-08-20.)
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[Pf2e20] +0x18 comp failed 1c7938 check "
                         "(self=%p raw=%p tid=0x%x) — zero-vec fallback\n",
                         static_cast<const void*>(self), static_cast<const void*>(buildRaw), tid);
            std::fflush(stderr);
        }
        Math::Vector3 zero(Math::Scalar(0.0f));   // splat 0
        retVec3[0] = zero.x;
        retVec3[1] = zero.y;
        retVec3[2] = zero.z;
        return retVec3;
    }

    const InteractDescription::NodeArray& nodes = (&build->_useNodes)[p3];
    int local_68 = nodes.count;

    if (local_68 <= p4) {
        int childN = self->useLocationCount;
        int i = 0;
        if (0 < childN) {
            do {
                int n = InteractNodeCount(self->useLocations[i], p3);   // 0x140123070
                if (local_68 <= p4 && p4 < n + local_68) {
                    InteractNodePosition(self->useLocations[i], retVec3, p3, p4 - local_68, p5);   // 0x140122e20
                    return retVec3;
                }
                i++;
                local_68 += n;
            } while (i < self->useLocationCount);
        }
        Math::Vector3 zero(Math::Scalar(0.0f));   // Math::Vector3::Vector3(ret,&scalar0)
        retVec3[0] = zero.x;
        retVec3[1] = zero.y;
        retVec3[2] = zero.z;
        return retVec3;
    }

    ModelComponent* model = self->_entity->FindComponent<ModelComponent>();   // 0x140003f60
    // [M-D SCOPED FALLBACK] a self-contained exact-type scan of the owning entity (null
    // slots skipped); if it still has no ModelComponent, return a zero vec instead of the
    // retail unguarded `null+0x148` transform read.
    if (model == nullptr) {
        const uint32_t s_tidModel = ModelComponent::TypeId();   // B.3.8: no lazy cache (B6-5)
        ComponentEntity* ownEnt = self->_entity;
        if (ownEnt && s_tidModel) {
            int n = ownEnt->_componentCount;
            ::Object::InternalPtr<Component>* arr = ownEnt->_components;
            for (int k = 0; k < n && arr; ++k) {
                Component* c = arr[k].Get();
                if (!c) continue;
                if (c->Type() == s_tidModel) {
                    model = static_cast<ModelComponent*>(c);
                    break;
                }
            }
        }
        if (model == nullptr) {
            if (p5) *p5 = 0.0f;
            retVec3[0] = retVec3[1] = retVec3[2] = 0.0f;
            return retVec3;
        }
    }

    // The node (kind, index) — disasm-verified (0x140122ea2..0x140122f42): node stride
    // 0x18 = {offset.xyz @0x0, dir.xyz @0xc}. The heading branch rotates node.direction and
    // the tail transforms node.offset.
    const InteractDescription::Node* nodeArr = nodes.data;
    const InteractDescription::Node* node = nodeArr ? &nodeArr[p4] : nullptr;
    static const Math::Vector3 kZero3(0, 0, 0);

    if (p5 != nullptr) {
        // local_50..local_38 = the 7 dwords of the ModelComponent's Math::Transform
        Math::Transform xform = model->transform;
        // local_60 (Vector3 out) = quaternion.Rotate(node.dir)
        Math::Vector3 dir = xform.rotation.Rotate(node ? node->direction : kZero3);
        float h = dir.Heading();
        *p5 = h;
    }

    Math::Transform xform2 = model->transform;
    Math::Vector3 pos = xform2 * (node ? node->offset : kZero3);
    retVec3[0] = pos.x;
    retVec3[1] = pos.y;
    retVec3[2] = pos.z;
    return retVec3;
}

// ============================================================
// FUN_14004edb0 — pick a random node of a target's interact tree and its reachable cost.
// ============================================================
// `target`: a component on the target entity. Finds the entity's InteractComponent
// (002ed0), gets its node count (123070), rolls a random index, resolves that node's world
// XY (122e20), converts to a grid cell, then validates reachability (a00f0 with path type
// 0xe1f) — if reachable, returns the distance (008830), else -1.
int PickReachableCell(const Component* target, uint32_t param_2, float* param_3) {
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    Map* map = world->m_game;

    InteractComponent* interact = target->_entity->FindComponent<InteractComponent>();   // 0x140002ed0

    uint32_t cnt = static_cast<uint32_t>(InteractNodeCount(interact, 0));   // 0x140123070
    uint32_t rnd = world->m_random->Rand(static_cast<uint32_t>(cnt));

    float pos[3] = {0, 0, 0};   // local_28/24/20
    InteractNodePosition(interact, pos, 0, static_cast<int>(rnd), nullptr);   // 0x140122e20
    param_3[0] = pos[0];
    param_3[1] = pos[1];
    param_3[2] = pos[2];

    int w = map->size;
    int gy = static_cast<int>(static_cast<float>(w >> 1) + param_3[1]);
    int gx = static_cast<int>(static_cast<float>(w >> 1) + param_3[0]);
    gx = ClampCell(gx, w);
    gy = ClampCell(gy, w);
    uint32_t cell = static_cast<uint32_t>(w * gy + gx);

    // LEFT: 0x1400a00f0 / 0x140008830's C declarations (Work/exe_bodies.h, WORK-a's header)
    // take the Map as int*.
    int* mapGrid = reinterpret_cast<int*>(map);   // LEFT: SIG WorkBase_ValidateCell_a00f0 / WorkBase_CalcDistance_8830 (Map as int*)
    uint64_t ok = WorkBase_ValidateCell_a00f0(mapGrid, 0xe1f, param_2, static_cast<int>(cell));   // 0x1400a00f0
    if (static_cast<char>(ok) == '\0') return -1;
    return WorkBase_CalcDistance_8830(mapGrid, param_2, cell);   // 0x140008830
}

// ============================================================
// FUN_140087fb0 — compute an entity's anchor cell + world position.
// ============================================================
// `entityIBP` = the entity's IBP, `param_2` = optional out world pos (float[3]). Returns the
// grid cell index (or -1). If the entity has an InteractComponent (002ed0) with >0 nodes,
// optionally roll a random node index (result discarded), resolve node 0 (122e20), and map
// world XY -> grid cell. Else fall back to the MapComponent footprint (004680): tileX0 /
// tileY0 (col/row) plus half of tileX1 / tileY1. If param_2 given and cell>=0, fill the
// world pos via GridToWorld (f3e0).
int EntityAnchorCell(ComponentEntity* const* entityIBP, float* param_2) {
    int iVar4 = -1;

    ComponentEntity* rawAnchorEntity = *entityIBP;
    ComponentEntity* resolvedAnchorEntity = *entityIBP;
    // DIAGNOSTIC (BUILDCTRL builder-employment crash): retail Entity_impl_7fb0 derefs
    // *param_1 unconditionally, so a null/dead entity here is always a crash — meaning
    // retail never reaches this with one. Our BUILDCTRL employ path does (a just-placed
    // foundation's work IBP, or a citizen's residence pointing at a deconstructed
    // building). Log the identity (raw = pre-CheckPtr value; resolved==0 => CheckPtr
    // nulled a dead weak-ref) and return the legitimate -1 "no anchor cell" sentinel so
    // the caller degrades gracefully instead of reading null+0x38. This is INSTRUMENTATION
    // to locate the upstream null — not the final fix (do not leave a silent guard here).
    if (resolvedAnchorEntity == nullptr) {
        static int s_anchorNullHits = 0;
        if ((s_anchorNullHits++ & 0x3f) == 0) {
            // LEFT: the diagnostic prints the raw pointer as an integer (%llx).
            std::fprintf(stderr, "[EmployCrash] Pf_EntityAnchorCell_7fb0 NULL entity: "
                         "IBPptr=%p raw=0x%llx resolved=0 (hit #%d) -> return -1\n",
                         static_cast<const void*>(entityIBP),
                         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(rawAnchorEntity)),   // CAST-OK: ptr_int
                         s_anchorNullHits);
            std::fflush(stderr);
        }
        return -1;
    }
    GameUpdateLoop* world = GameUpdateLoop::_singleton;
    InteractComponent* interact = resolvedAnchorEntity->FindComponent<InteractComponent>();   // 0x140002ed0

    bool handled = false;
    if (interact != nullptr) {
        int n = InteractNodeCount(interact, 0);   // 0x140123070
        if (0 < n) {
            int n2 = InteractNodeCount(interact, 0);
            if (1 < n2) {
                uint32_t n3 = static_cast<uint32_t>(InteractNodeCount(interact, 0));
                world->m_random->Rand(static_cast<uint32_t>(n3)); // result discarded
            }
            float pos[3] = {0, 0, 0};   // local_28/24/20
            InteractNodePosition(interact, pos, 0, 0, nullptr);   // 0x140122e20
            int w = world->m_game->size;
            int gx = static_cast<int>(pos[0] + static_cast<float>(w >> 1));
            int gy = static_cast<int>(pos[1] + static_cast<float>(w >> 1));
            gx = ClampCell(gx, w);
            gy = ClampCell(gy, w);
            iVar4 = w * gy + gx;
            handled = true;
        }
    }

    if (!handled) {
        const MapComponent* footprint = (*entityIBP)->FindComponent<MapComponent>();   // 0x140004680
        if (footprint != nullptr) {
            int w = world->m_game->size;
            int row = static_cast<int>(footprint->tileY0);
            int offR = static_cast<int>(footprint->tileY1) >> 1;
            int col = static_cast<int>(footprint->tileX0);
            int offC = static_cast<int>(footprint->tileX1) >> 1;
            iVar4 = (offR + row) * w + offC + col;
        }
    }

    if (param_2 != nullptr && -1 < iVar4) {
        Map* map = world->m_game;
        float half = static_cast<float>(map->size >> 1) - f172114();   // DAT_140172114
        float local[3];
        local[0] = static_cast<float>(iVar4 % map->size) - half;
        local[1] = static_cast<float>(iVar4 / map->size) - half;
        local[2] = 0.0f;
        float wp[3] = {0, 0, 0};   // local_28..local_20
        Reimpl_MapGetTerrainPos(map, wp, local, '\0', nullptr);   // 0x14009f3e0
        param_2[0] = wp[0];
        param_2[1] = wp[1];
        param_2[2] = wp[2];
    }
    return iVar4;
}


// ============================================================
// FUN_1400af370 — rebuild the whole road mesh (per-tile verts/indices).
// ============================================================
// `map`: the Map (its size and cells). The WorldGeometry arrays it uses:
//   +0x200 (_arrays[6]) the mesh batches (RoadBatch, stride 0x60) — owned by the f250 / fa80
//                       grow helpers;
//   +0x210 (_arrays[7]) the per-batch source table (a qword per batch; its count is the
//                       batch count);
//   +0x220 (_arrays[8]) the per-batch fill counters (int).
// _gridSize / _fieldC4 = tile span / inset; (span - inset) >> 1 = vtxOff. _chunkSize = the
// block dimension. MapCell::field_0x2A = batchId, field_0x2B = blendId.
//
// Phases: (1) sync the counter array length to the batch count (021d0) then zero it; (2)
// f250-size a local working batch array; per block, seed each batch header (source, culling
// AABB) and reset its two buffers (c890/ee40); (3) per fine cell read batchId/blendId, build
// the 3x3 corner-occupancy mask, fold it to a tile shape (ae180), build the quad (cc10),
// append 6 corner indices to the index buffer (AppendIndex), and push the 4 verts into the
// vertex buffer (1f8d0) after overwriting their AO-uv slots with the per-corner
// (fineCol,fineRow)+offset tile coords; (4) per batch with vertices, append the batch to the
// mesh batches (fa80); (5) free both buffers per batch + the working array.
//
// Index details — BOTH pinned against the disassembly (0x1400af588..0xaff6e):
//   * mask -> ae180 arg order (p2..p9) = mask[3],[5],[1],[7],[0],[2],[6],[8] (slot = rowDelta*3+col,
//     rowDelta/col in {-1,0,1}; center slot[4] forced 1).  Verified at 0xaf61f..0xaf662.
//   * the vertex push overwrites aoU with local_c8[8+i*2]+fineCol and aoV with
//     local_c8[i*2]+fineRow, i.e. corner (xOff,yOff)+tile.  local_c8 (16 shorts) =
//     {0,0,0,0,1,0,1,0, 0,0,1,0,1,0,0,0}.
//
// AppendIndex: the inline "append one short to a batch's index buffer" grow+write block,
// identical across the 8 grow-blocks (they differ only by the value written to a freshly-grown
// slot). Reads count fresh each call; new = old+1; cap rule "cap=new; if new==old+1 && old!=0
// cap=new*3".
static void AppendIndex(RawArray<int16_t>* a, short valueForNew) {
    int oldN = a->size;
    int newN = oldN + 1;
    if (newN == oldN) return;
    int16_t* oldBuf = a->data;
    a->size = newN;
    if (a->capacity < newN) {
        a->capacity = newN;
        if ((newN == oldN + 1) && (oldN != 0)) a->capacity = newN * 3;
        a->data = static_cast<int16_t*>(System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(a->capacity) * 2, 2));
    }
    int start = (oldBuf == a->data) ? oldN : 0;
    for (int64_t j = start; j < newN; j++) {
        int16_t* dst = a->data + j;
        if (dst) {
            short v = valueForNew;
            if (j < oldN) v = oldBuf[j];
            *dst = v;
        }
    }
    if (oldBuf != a->data) System::MemoryAllocator::MemFree(oldBuf);
}

void WorldGeometry::RebuildRoadMesh(Map* map) {
    // LEFT: WorldGeometry (block GAME-c's header) keeps these arrays as raw {data, sizeCap}
    // headers (`_arrays`).
    RawArray<RoadBatch>* meshBatches = reinterpret_cast<RawArray<RoadBatch>*>(&_arrays[6]);    // +0x200 (local_a8)   // LEFT: Game/WorldGeometry.h _arrays raw headers (GAME-2, H5)
    const RawArray<int64_t>* sources = reinterpret_cast<const RawArray<int64_t>*>(&_arrays[7]); // +0x210   // LEFT: Game/WorldGeometry.h _arrays raw headers (GAME-2, H5)
    RawArray<int32_t>* counters = reinterpret_cast<RawArray<int32_t>*>(&_arrays[8]);           // +0x220   // LEFT: Game/WorldGeometry.h _arrays raw headers (GAME-2, H5)
    Reimpl_Struct60B_Resize(meshBatches, 0);                                // f250(local_a8, 0)

    int vtxOff = (_gridSize - _fieldC4) >> 1; // iVar12

    // Phase 1: counter array length sync (vs the batch count) then zero every counter.
    if (counters->size != sources->size) {
        // B.3.2 bridge: FUN_1400021d0 == Collection::Array<int32>::SetSize. I32NoInit —
        // the grown tail is left as the allocation found it, exactly as retail's helper
        // does at 1400af3df. Safe: the counter-zeroing loop right below (retail
        // 1400af400) writes all the elements before anything reads one.
        GameColl::AsArray<GameColl::I32NoInit>(counters).SetSize(sources->size);
    }
    {
        int cnt = counters->size;
        for (int i = 0; i < cnt; i++)
            counters->data[i] = 0;
    }

    // Phase 2: local working batch array, sized to the batch count.
    RawArray<RoadBatch> batches = {nullptr, 0, 0};   // local_108
    Reimpl_Struct60B_Resize(&batches, static_cast<uint32_t>(sources->size));   // 0x14001f250

    int batchCount = batches.size;                            // (int)local_100
    int gridDim = map->size;                                  // MOVSXD R13,[RDI]
    int cellSize = _chunkSize;                                // +0xc8 (block dim)

    // local_c8 corner-offset table (16 shorts), used by the per-vertex push.
    static const short kCornerOff[16] = {0,0,0,0,1,0,1,0, 0,0,1,0,1,0,0,0};

    int local_f8 = 0;
    if (0 < gridDim) {
        do {                                   // outer block row (local_f8)
            int local_f4 = 0;
            do {                               // outer block col (local_f4)
                // ---- seed each batch header for this block ----
                if (0 < batchCount) {
                    int seedColMin = local_f4 + vtxOff;   // iVar26 (R15D = local_f4+vtxOff)
                    int seedRowMin = local_f8 + vtxOff;   // iVar16 (R12D = local_f8+vtxOff)
                    for (int b = 0; b < batchCount; b++) {
                        RoadBatch& rec = batches.data[b];
                        rec.source = sources->data[b];
                        // B.3.2 bridge: FUN_14001c890 == Array<uint16>::SetSize (+0x08);
                        // FUN_14001ee40 == Array<RoadVertex>::SetSize (+0x18). Both shrink
                        // to 0 here, so neither body's new-tail arm is ever exercised.
                        GameColl::AsArray<GameColl::U16>(&rec.vertices).SetSize(0);
                        GameColl::AsArray<RoadVertex>(&rec.indices).SetSize(0);
                        // Culling AABB in tile coords, read by the TerrainGenerator cull pass
                        // (0x1400ae490 -> Volume::Frustum::Contains). `slot` (+0x28, f250
                        // seeds -1) must NOT be written here.
                        rec.minX = seedColMin;
                        rec.minY = seedRowMin;
                        rec.maxX = _chunkSize + local_f4 + vtxOff;
                        rec.maxY = _chunkSize + local_f8 + vtxOff;
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
                                uint8_t batchId = map->cells[cellIdx].field_0x2A;
                                uint8_t blendId = map->cells[cellIdx].field_0x2B;

                                if (batchId != 0xff) {
                                    if (blendId == 0xff) {
                                        counters->data[batchId] += 1;
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
                                                        local_f0[slot] = (map->cells[static_cast<int64_t>(iVar1) + rowOff].field_0x2A != 0xff) ? 1 : 0;
                                                    }
                                                }
                                                slot++;
                                            }
                                            row++;
                                        }
                                    }

                                    int blendVal = Pf_TileShapeLut_ae180(nullptr,   // 0x1400ae180
                                                               local_f0[3], local_f0[5], local_f0[1], local_f0[7],
                                                               local_f0[0], local_f0[2], local_f0[6], local_f0[8]);

                                    // build the quad (cc10) into the local scratch (local_98).
                                    RoadVertex quad[4];
                                    BuildRoadQuad(this, fineCol + vtxOff, fineRow + vtxOff,
                                                  quad, blendVal, blendId);   // 0x1400acc10

                                    RoadBatch& rec = batches.data[batchId];
                                    short sVar21 = static_cast<short>(rec.vertices.size);

                                    // ---- push the 6 corner indices (parity split on (rowSeed+vtxOff + colSeed+vtxOff)) ----
                                    if (((fineRow + vtxOff + fineCol + vtxOff) & 1) == 0) {
                                        AppendIndex(&rec.indices, sVar21);       // +0
                                        AppendIndex(&rec.indices, sVar21 + 3);   // +3
                                        AppendIndex(&rec.indices, sVar21 + 1);   // +1
                                        AppendIndex(&rec.indices, sVar21 + 1);   // +1
                                        AppendIndex(&rec.indices, sVar21 + 3);   // +3
                                        AppendIndex(&rec.indices, sVar21 + 2);   // +2
                                    } else {
                                        AppendIndex(&rec.indices, sVar21);       // +0
                                        AppendIndex(&rec.indices, sVar21 + 2);   // +2
                                        AppendIndex(&rec.indices, sVar21 + 1);   // +1
                                        AppendIndex(&rec.indices, sVar21);       // +0
                                        AppendIndex(&rec.indices, sVar21 + 3);   // +3
                                        AppendIndex(&rec.indices, sVar21 + 2);   // +2
                                    }

                                    // ---- push the 4 quad verts (1f8d0), overwriting their AO-uv
                                    //      slots with the per-corner tile coords. ----
                                    for (int vi = 0; vi < 4; vi++) {
                                        short cX = static_cast<short>(kCornerOff[8 + vi * 2] + static_cast<short>(fineCol)); // local_c8[+0x10 + i*4]
                                        short cY = static_cast<short>(kCornerOff[vi * 2]     + static_cast<short>(fineRow)); // local_c8[+0 + i*4]
                                        quad[vi].aoU = cX;   // AO-u slot
                                        quad[vi].aoV = cY;   // AO-v slot
                                        int curCount = rec.vertices.size;  // (int)plVar15[1]
                                        // B.3.2 bridge: FUN_14001f8d0 == Array<RoadVertex>::
                                        // SetSize(n, fill) (3-arg form) -- appends the vertex as
                                        // the new tail element.
                                        GameColl::AsArray<RoadVertex>(&rec.vertices)
                                            .SetSize(curCount + 1, quad[vi]);
                                    }
                                }
                                local_e0++;
                            } while (local_e0 < _chunkSize);
                        }
                        local_d4++;
                    } while (local_d4 < _chunkSize);
                }

                // ---- append each batch with vertices to the mesh batches (fa80) ----
                if (0 < batchCount) {
                    for (int b = 0; b < batchCount; b++) {
                        RoadBatch& rec = batches.data[b];
                        if (0 < rec.vertices.size) {
                            // LEFT: 0x14001fa80's C declaration (action_pf_pools.cpp; its other
                            // caller is Player/tool_base_highlight.cpp) takes the array and
                            // the element as integers.
                            Pf_VectorResize60_fa80(reinterpret_cast<int64_t*>(meshBatches), meshBatches->size + 1,   // LEFT: SIG Pf_VectorResize60_fa80
                                                   reinterpret_cast<uint64_t*>(&rec));   // 0x14001fa80   // LEFT: SIG Pf_VectorResize60_fa80
                        }
                    }
                }

                local_f4 += _chunkSize;
            } while (local_f4 < gridDim);
            local_f8 += _chunkSize;
        } while (local_f8 < gridDim);
    }

    // Phase 5: free every batch's two buffers, then the array.
    {
        int n = batches.size;
        for (int b = 0; b < n; b++) {
            RoadBatch& rec = batches.data[b];
            System::MemoryAllocator::MemFree(rec.indices.data);    // buffer A
            rec.indices.data = nullptr;
            rec.indices.size = 0;
            rec.indices.capacity = 0;
            System::MemoryAllocator::MemFree(rec.vertices.data);   // buffer B
            rec.vertices.data = nullptr;
            rec.vertices.size = 0;
            rec.vertices.capacity = 0;
        }
        System::MemoryAllocator::MemFree(batches.data);
    }
}

// ===========================================================================
// B4 FORWARD: the C spellings other blocks' files still call, one line each.
// ===========================================================================

// B4 FORWARD: 2 outside callers (Game/gamegrid_serialize.cpp, Player/keystone_a_removalcalcpath.cpp)
void Pf_RoadMeshRebuild_f370(int64_t param_1, uint32_t* param_2) {
    reinterpret_cast<WorldGeometry*>(param_1)->RebuildRoadMesh(reinterpret_cast<Map*>(param_2));   // LEFT: SIG Pf_RoadMeshRebuild_f370
}

// B4 FORWARD: 1 outside caller (Player/tool_base_highlight_tess.cpp)
void Pf_RoadQuad_cc10(int64_t self, int x, int y, int64_t outQuad, int dirIdx, uint8_t blend) {
    BuildRoadQuad(reinterpret_cast<WorldGeometry*>(self), x, y, reinterpret_cast<RoadVertex*>(outQuad), dirIdx, blend);   // LEFT: SIG Pf_RoadQuad_cc10
}

// B4 FORWARD: 2 outside callers (Component/quarrycomponent_deform.cpp, Player/tool_base_ray.cpp)
int Pf_CellQuadRay_aa20(int64_t self, int x, int y, void* p4, float* p5, float* p6, float* p7, float* p8, uint32_t* p9) {
    return RayTestCell(reinterpret_cast<WorldGeometry*>(self), x, y, reinterpret_cast<const float*>(p4),   // LEFT: SIG Pf_CellQuadRay_aa20
                       p5, p6, p7, p8, reinterpret_cast<float*>(p9));   // LEFT: SIG Pf_CellQuadRay_aa20
}

// B4 FORWARD: 1 outside caller (Player/tool_base_tilerender.cpp)
float Pf_CellQuadRayWrap_0f90(int64_t self, float* param_2, uint32_t* param_3) {
    return TerrainHeightUnder(reinterpret_cast<WorldGeometry*>(self), param_2, reinterpret_cast<float*>(param_3));   // LEFT: SIG Pf_CellQuadRayWrap_0f90
}

// B4 FORWARD: outside callers in ~20 files (Component, Dialog, Game, Player, Work)
void* Pf_EntityResolvePos_2e20(void* self, float* retVec3, int p3, int p4, float* p5) {
    return InteractNodePosition(reinterpret_cast<const InteractComponent*>(self), retVec3, p3, p4, p5);   // LEFT: SIG Pf_EntityResolvePos_2e20
}


// B4 FORWARD: outside callers in ~20 files (Component, Entity, Game, Player, Work)
int Pf_EntityAnchorCell_7fb0(void* param_1IBP, float* param_2) {
    return EntityAnchorCell(reinterpret_cast<ComponentEntity* const*>(param_1IBP), param_2);   // LEFT: SIG Pf_EntityAnchorCell_7fb0
}
