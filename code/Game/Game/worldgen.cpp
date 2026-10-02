// code/Game/Game/worldgen.cpp
// ============================================================================
// M5 5b — in-game procedural TERRAIN BUILD (first-light slice).
// ============================================================================
//
// Implements the new-game worldgen chain reached from GameUpdateLoop::Load
// step 31 (project_newgame_path_roadmap):
//
//   FUN_1400b0230  (TerrainBuild_Dispatcher)            -> WorldGen_BuildTerrain
//     -> TradeWork_impl_c4b0 (orchestrator)             -> wg_TradeCore_c4b0
//        -> ConsumeProduceComponent_impl_f390 (pipeline) -> wg_HeightmapPipeline_f390
//           -> ConsumeProduceComponent_impl_e5d0         -> wg_BaseHeightmap_e5d0
//        -> [reuse] sub_c890 / sub_c780 / sub_c1e0 / sub_f0a0   (already reimpl'd)
//        -> FUN_1400cf440 (blend texture)                -> wg_BlendTexture_cf440_stub
//     -> FUN_1400175d0 (FreePatchListVector)            -> wg_FreePatchList
//
// FIRST-LIGHT SCOPE (this slice):
//   * e5d0 base perlin heightmap (radial-falloff valley: low buildable center,
//     mountainous edges — authentic Banished terrain)
//   * quantize float heightmap -> int16 (WorldGeom+0xd8)
//   * stubbed cf440 blend texture (flat splat buffer; real cf440 = next layer)
//   * reuse the already-reimplemented base mesh (sub_c1e0 -> VB+0x100/IB+0x120)
//     + height-texture upload (sub_c780 -> +0x148)
//   * SKIP the detail mesh (b260): the Geometry draw path (geometry_draw.cpp
//     GeomDraw_CollectDraw) gates every detail draw on wg+0x1c8/+0x1d8/+0x1f8/
//     +0x208 counts, which stay 0 when b260 doesn't run -> no detail draw items
//     -> +0x160/+0x180 never touched. Safe.
//
// DEFERRED (later slices; see notes/WORLDGEN_TERRAIN_BLUEPRINT.md):
//   * feature stages cd090/cd150/ccf60/cd460/e910/cc080/ec50 (coastline, rivers,
//     mountain ranges, erosion) + c350 smoothing
//   * real cf440 (rock/grass/AO splat) — surface texturing only, geometry same
//   * b260 detail/cliff mesh
//   * CreateNaturalResource (trees/stone — spawns worker threads)
//   * FUN_1400a3300 start-town + initial-citizen spawn (needs full ECS)
//
// All field offsets verified against Ghidra disassembly via the worldgen recon
// workflow (notes/WORLDGEN_TERRAIN_BLUEPRINT.md, 2026-05-30).

#include <new>
#include "system/String.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "system/Filename.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include "utility/PerlinNoise.h"   // RtUtility_PerlinNoise_Ctor / _Generate
#include "Game/exe_bodies.h"
#include "Game/WorldGeometry.h"
#include "Game/TerrainGenerator.h"
#include "Game/TerrainHeightfield.h"
#include "graphics/Material.h"   // Graphics::MaterialInstance (an Object::External)
#include "collection/Collection.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include "system/Value.h"      // ENDGAME B.3.4b System caller batch (Random/Time/Color)

// ============================================================
// Reused helpers from sibling reimpl TUs (already registered/defined)
// ============================================================
// Game/exe_bodies.h: Reimpl_Array_Resize_Stride02 (sub_c890, int16 heightmap @wg+0xd8),
// Reimpl_Array_Resize_Stride04 (sub_3a00, float heightmap @hf+0x08),
// Reimpl_Array_Resize_ArrayOfArrays (sub_f0a0, detail-group-C @wg+0x1e0),
// WorldGeometry::UploadHeightImage (0x1400ac780, height-texture upload +0x148), WorldGeometry::BuildTerrainMesh (0x1400ac1e0)
// (base grid mesh VB+0x100/IB+0x120).
// WorldGeometry::BuildWaterMesh (b260, water_contour_mesh.cpp) is a member.

// TerrainPathPoint::outerRadius (+0x14, TerrainHeightfield.h) is what this file
// calls the channel half-width (notes/water_mesh/lake_coverage.md §"+0x14 is a
// per-path constant"). halfWidth is the CHANNEL half-width in tiles. It is written by the path builder
// (FUN_1400cc850 arg a7) identically into every point of a path, so it is a per-path
// constant, not per-point: coastline ring = 4.0 (DAT_140172194), secondary paths =
// 1.5 (DAT_140172160). The RENDERED water band is wider still — fb00 uses
// halfWidth + 2.0 (DAT_140172174) — so "distance from centerline to dry land" is
// halfWidth + 2, not halfWidth.
static constexpr float PATHPT_BAND_MARGIN = 2.0f;   // DAT_140172174

// ── halfWidth side table ────────────────────────────────────────────────────────
// The polyline copy into wg+0x1e0 drops all 20 non-(x,y) bytes, faithfully to retail.
// Consumers that legitimately need the channel width (the BUILD_DEMO_TRADINGPOST dock
// placement in starttown.cpp — a dock 1 tile from a CENTERLINE is still under water)
// read it here instead of widening the copy. Indexed by path index, which the copy
// preserves: dest[i] comes from src[i]; only the POINTS within a path are reversed.
static constexpr int kMaxTrackedPaths = 16;
static float s_pathHalfWidth[kMaxTrackedPaths];
static float s_pathHalfWidthMax[kMaxTrackedPaths];   // to verify the "constant" claim
static int   s_pathHalfWidthCount = 0;

// Returns the channel half-width in tiles for feature path `pathIndex`, or -1.0f if
// unknown (index out of range, or worldgen has not run). Callers MUST handle -1
// rather than treating it as a width — a missing value means "no data", and silently
// using 0 would reintroduce the centerline placement this table exists to prevent.
extern "C" float WorldGen_GetPathHalfWidth(int pathIndex) {
    if (pathIndex < 0 || pathIndex >= s_pathHalfWidthCount ||
        pathIndex >= kMaxTrackedPaths)
        return -1.0f;
    return s_pathHalfWidth[pathIndex];
}

// Distance in tiles from a path's centerline to the first dry tile, or -1.0f if the
// half-width is unknown. This is the value a dock placement wants.
extern "C" float WorldGen_GetPathBankDistance(int pathIndex) {
    const float hw = WorldGen_GetPathHalfWidth(pathIndex);
    return (hw < 0.0f) ? -1.0f : hw + PATHPT_BAND_MARGIN;
}

// ============================================================
// .rdata float constants (verified via read_memory; blueprint §2)
// ============================================================
static constexpr float kHalf       = 0.5f;   // 0x140172114  pn1 persistence
static constexpr float kQuarter    = 0.25f;  // 0x140172100  pn2 persistence / falloff bias / +0.25 / pn1 coord
static constexpr float kOctaves    = 5.0f;   // 0x140172198  g_pAgeMultiplier (both noise octaves)
static constexpr float kClampFloor = 2.0f;   // 0x140172174  min height
static constexpr float kOne        = 1.0f;   // 0x14017214c  g_fUVScale
static constexpr float kPn2Coord   = 0.1f;   // 0x1401720e4  pn2 Generate coord scale
static constexpr float kFalloffBase= 1.1f;   // 0x140172150  powf base (radial falloff)
static constexpr float kFalloffBias= 16.0f;  // 0x1401721c0  falloff dist bias
static constexpr float kBaseScale  = 8.0f;   // 0x1401721a8  (pn1+1)*8
static constexpr float kHScale     = 48.0f;  // 0x1401721e4  (pn2+0.25)*48
static constexpr float kQuantize   = 256.0f; // 0x140172230  float->int16 fixed-point scale

// Runtime bodies called by name: the seeded Random ctor ??0Random@System@@QEAA@I@Z ->
// RtSystem_Random_CtorSeed (system/Value.h); Utility::PerlinNoise ctor / Generate ->
// RtUtility_PerlinNoise_Ctor / _Generate (utility/PerlinNoise.h).

// Toggle: full f390 feature pipeline vs first-light (e5d0 only).
// The feature stages are what carve the buildable lowland out of the e5d0 dome:
// e5d0 leaves a ~8-height dome everywhere; the coastline ring + rivers + ranges
// geometry feed the e910/ec50 feature stamps, whose `h = maskB*h*maskA - maskC`
// pinches the carved regions down toward sea level (buildable) while uncarved
// areas stay high (mountains). Reverse-engineered + reimplemented per stage in
// worldgen_f390_{coastline,ranges,rivers,erosion}.cpp (m5-f390 agents, 2026-05-31).
// Random-corruption bug fixed 2026-05-31: coastline_Subdivide_cc510 passed `self`
// (hf base) instead of `self+0x58` to the RNG, shifting the MT window -0x58 and
// clobbering the real Random index. Fixed in worldgen_f390_coastline.cpp.
static constexpr bool kFullPipeline = true;

// The f390 feature / normalization stages (each on the heightfield) are declared
// in TerrainHeightfield.h.

// ============================================================
// e5d0 — base perlin heightmap (the single load-bearing gen function).
// Verified from ConsumeProduceComponent_impl_e5d0 @0x1400ce5d0.
//
//   per cell:  h_out = (1-w)*base + h*w
//     w     = 1 / (curve + 1)
//     curve = powf(1.1, (dim*0.25 + 16) - dist)        [radial falloff]
//     dist  = sqrt(dx^2 + dz^2),  dx=col-dim/2, dz=row-dim/2
//     base  = (pn1 + 1) * 8                            [low, smooth — valley floor]
//     h     = (pn2 + 0.25) * 48 + pn1, clamped >= 2    [high — mountains]
//   pn1: persistence 0.5, octaves 5.0, sampled at (col*0.25, row*0.25)
//   pn2: persistence 0.25, octaves 5.0, sampled at (col*0.1,  row*0.1)
//   -> valley: center (small dist) uses base (low), edge (large dist) uses h (high).
// ============================================================
static void wg_BaseHeightmap_e5d0(TerrainHeightfield* hf) {
    int dim  = hf->gridSize;                     // = mapDim*2
    int edge = dim + 1;                          // (dim+1) vertices per side

    // Resize the float heightmap to (dim+1)^2 (sub_3a00 == Stride04).
    Reimpl_Array_Resize_Stride04(&hf->heightmap, edge * edge);
    float* heightmap = hf->heightmap.data;
    if (!heightmap) return;

    System::Random* rng = &hf->random;
    int s1 = rng->RandTol(0xffffff);
    int s2 = rng->RandTol(0xffffff);
    RtUtility::PerlinNoise pn1 = {};
    RtUtility_PerlinNoise_Ctor(&pn1, kHalf, kOctaves, s2, s1);          // persist 0.5
    int s3 = rng->RandTol(0xffffff);
    int s4 = rng->RandTol(0xffffff);
    RtUtility::PerlinNoise pn2 = {};
    RtUtility_PerlinNoise_Ctor(&pn2, kQuarter, kOctaves, s4, s3);       // persist 0.25

    int half = dim >> 1;
    float distBias = (float)dim * kQuarter + kFalloffBias;      // dim*0.25 + 16

    for (int row = 0; row < edge; ++row) {
        int dz = row - half;
        for (int col = 0; col < edge; ++col) {
            int dx = col - half;
            float dist  = sqrtf((float)(dx * dx + dz * dz));
            float curve = powf(kFalloffBase, distBias - dist);  // 1.1 ^ ((dim*0.25+16) - dist)
            float w     = kOne / (curve + kOne);

            float pn1s = RtUtility_PerlinNoise_Generate(&pn1, (float)col * kQuarter, (float)row * kQuarter);
            float base = (pn1s + kOne) * kBaseScale;             // (pn1+1)*8
            float pn2s = RtUtility_PerlinNoise_Generate(&pn2, (float)col * kPn2Coord, (float)row * kPn2Coord);
            float h    = (pn2s + kQuarter) * kHScale + pn1s;     // (pn2+0.25)*48 + pn1
            if (h < kClampFloor) h = kClampFloor;

            heightmap[row * edge + col] = (kOne - w) * base + h * w;
        }
    }
}

// ============================================================
// f390 — heightmap pipeline. First-light = e5d0 only.
// Verified order (blueprint §0): hf[0]=sizeRow[0]*2; e5d0; [feature stages...]
// ============================================================

// Retail f390 publishes worldgen stage progress by writing 1..5 straight into
// GameUpdateLoop+0x24 (m_loadProgress) through g_pGameWorldSingleton — it never
// goes through the exported SetLoadProgress (0 callers in the whole 2017 build).
// The consumer is the loading screen: GameLoadDialogLoopT::Update (0x140098DA0)
// gates its 2.0s random-tip cycle on nextLoop->m_loadProgress != 0, so these
// writes are what makes the "(Uplifting mountains)" flavor text start cycling.
// Null-checked only because our standalone can run worldgen from harnesses
// before Load sets the singleton; retail's write is unguarded.
static void wg_PublishLoadProgress(int stage) {
    if (GameUpdateLoop::_singleton != nullptr)
        GameUpdateLoop::_singleton->_loadProgress = stage;   // +0x24
}

static void wg_HeightmapPipeline_f390(TerrainHeightfield* hf) {
    const TerrainSizeRow* sizeRow = hf->sizeRow;
    hf->gridSize = (sizeRow ? sizeRow->mapSize : 0) * 2;   // gridSize = mapDim*2

    wg_PublishLoadProgress(1);
    wg_BaseHeightmap_e5d0(hf);

    if (kFullPipeline) {
        // Verified f390 order (blueprint §0), incl. the stage-progress writes.
        wg_PublishLoadProgress(2);
        wg_f390_coastline(hf);   // cd090
        wg_f390_ranges(hf);      // cd150
        wg_f390_paths(hf);       // ccf60
        wg_PublishLoadProgress(3);
        wg_f390_rivers(hf);      // cd460
        wg_f390_stamp(hf);       // e910   — first carve pass
        wg_PublishLoadProgress(4);
        wg_f390_erosion(hf);     // cc080
        wg_PublishLoadProgress(5);
        wg_f390_carve(hf);       // ec50   — finalize carve
        wg_f390_smooth(hf);      // c350
        wg_f390_smooth(hf);      // c350 (second pass)
    }
}

// ============================================================
// cf440 STUB — blend/splat texture. Returns a flat RGBA (dim*dim*4) buffer so
// WorldGeom+0x140 is non-null (sub_c780 uploads it to the +0x148 texture, which
// the base-mesh draw binds). Real cf440 (rock/grass/AO splat) is the next layer;
// it changes only surface texturing, not geometry.
// ============================================================
// The splat buffer is WorldGeometry::_heightBytes: 4 bytes per cell, R rock /
// G grass / B AO / A.
static uint8_t* wg_BlendTexture_cf440_stub(const TerrainHeightfield* hf) {
    int dim = hf->gridSize;   // = mapDim*2 (== WorldGeom+0xc0)
    if (dim <= 0 || !System::MemoryAllocator::_singleton) return nullptr;
    uint64_t n = (uint64_t)dim * (uint64_t)dim;
    uint8_t* buf = static_cast<uint8_t*>(System::MemoryAllocator::MemAlloc(n * 4, 4));
    if (buf) {
        // Neutral mid splat weights (R rock / G grass / B AO, A=0xff). With null
        // materials the shader falls back to a flat surface — visible terrain.
        for (uint64_t i = 0; i < n * 4; ++i) buf[i] = 0x80;
    }
    return buf;
}

// ============================================================
// cf150 — PER-CELL NORMAL via heightmap central differences.
// Verified from FUN_1400cf150 @0x1400cf150.
//
//   normal = normalize( -(h[r][c+1] - h[r][c-1]),     // -dh/dx
//                        -(h[r+1][c] - h[r-1][c]),     // -dh/dy
//                         2.0 )                         // DAT_140172174
//   All neighbour indices are clamped to [0, dim] (heightmap stride = dim+1).
//   The XOR-with-0x80000000 in the decomp is a float negate.
// Writes {nx,ny,nz} into out[0..2].
// ============================================================
static inline void wg_CellNormal_cf150(const float* hm, int dim, int col, int row,
                                        float out[3]) {
    int edge = dim + 1;
    auto clamp = [dim](int v) { return v < 0 ? 0 : (v > dim ? dim : v); };
    int cR   = clamp(row);
    int cC   = clamp(col);
    int colP = clamp(col + 1);
    int colM = clamp(col - 1);
    int rowP = clamp(row + 1);
    int rowM = clamp(row - 1);

    float nx = -(hm[edge * cR + colP] - hm[edge * cR + colM]);   // -dh/dx
    float ny = -(hm[edge * rowP + cC] - hm[edge * rowM + cC]);   // -dh/dy
    float nz = 2.0f;                                             // DAT_140172174
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 0.0f) { nx /= len; ny /= len; nz /= len; }
    out[0] = nx; out[1] = ny; out[2] = nz;
}

// ============================================================
// cf440 — GENERATE BLEND/SPLAT TEXTURE (the real one).
// Verified from FUN_1400cf440 @0x1400cf440. Returns an RGBA (dim×dim) buffer:
//   R (byte0) = ROCK weight   — perlin² scatter (height ≥ -0.5), forced 0xff
//               below -0.5, and ramped up where height > 8.0 (high rock/snow).
//   G (byte1) = GRASS weight  — slope band on the cell normal.z: full (0xff) on
//               slopes (nz ≤ 0.85), feathering to 0 toward flat (nz → 0.999),
//               plus a height ramp where height > 7.0.
//   B (byte2) = AMBIENT-OCCLUSION — 21×21 circular normal kernel: 1 − mean of
//               max(dot(centerNormal, dirToNeighbour), 0), scaled by a 3×3
//               smoothed normal.z.
//   A (byte3) = 0 (matches the original — it never writes byte3).
//
// Per-cell scratch (dim×dim, 6 floats each): {x=col, y=row, h=height, nx,ny,nz}.
// Heights are sampled from the (dim+1)² float heightmap; normals via cf150.
// All thresholds are the verified .rdata constants (read_memory 0x1401720f0..).
// ============================================================
static uint8_t* wg_BlendTexture_cf440(TerrainHeightfield* hf) {
    int dim = hf->gridSize;
    const float* hm = hf->heightmap.data;
    System::Random* rng = &hf->random;
    if (dim <= 0 || !hm || !System::MemoryAllocator::_singleton) {
        fprintf(stderr, "[worldgen] cf440: deps unresolved (hm=%p) "
                "— falling back to flat splat\n", (void*)hm);
        fflush(stderr);
        return wg_BlendTexture_cf440_stub(hf);
    }

    // .rdata constants (verified bytes at the listed addresses).
    const float kCoord     = 0.25f;        // 0x140172100  perlin coord scale / persistence
    const float kCfOctaves = 4.0f;         // 0x140172194
    const float kRRemap    = 0.5f;         // 0x140172114  (pn+1)*0.5
    const float k255       = 255.0f;       // 0x14017222c
    const float kRockHMin  = -0.5f;        // 0x140172278  below => full rock
    const float kHi8       = 8.0f;         // 0x1401721a8  rock high-band start
    const float kHi7       = 7.0f;         // 0x1401721a0  grass high-band start
    const float kBandDiv   = 3.0f;         // 0x140172180  height-band divisor
    const float kGUpper    = 0.998969f;    // 0x140172148  grass slope upper clamp
    const float kGLower    = 0.85f;        // 0x140172130  grass slope lower band
    const float kGBandDiv  = 0.149047f;    // 0x1401720f0  grass slope band divisor

    // Perlin for the rock scatter: persist 0.25, octaves 4.0, two RandTol seeds.
    // (ctor arg order mirrors e5d0: seeds passed (second, first).)
    int s1 = rng->RandTol(0xffffff);
    int s2 = rng->RandTol(0xffffff);
    RtUtility::PerlinNoise pn = {};
    RtUtility_PerlinNoise_Ctor(&pn, kCoord, kCfOctaves, s2, s1);

    // Per-cell scratch: dim*dim * {x,y,h,nx,ny,nz} (6 floats / 0x18 bytes).
    float* scratch = (float*)System::MemoryAllocator::MemAlloc((uint64_t)dim * dim * 0x18, 4);
    if (!scratch) return wg_BlendTexture_cf440_stub(hf);
    int edge = dim + 1;
    for (int row = 0; row < dim; ++row) {
        for (int col = 0; col < dim; ++col) {
            float* e = scratch + (size_t)(dim * row + col) * 6;
            e[0] = (float)col;
            e[1] = (float)row;
            e[2] = hm[edge * row + col];
            wg_CellNormal_cf150(hm, dim, col, row, &e[3]);
        }
    }

    uint8_t* out = static_cast<uint8_t*>(System::MemoryAllocator::MemAlloc((uint64_t)dim * dim * 4, 4));
    if (!out) { System::MemoryAllocator::MemFree(scratch); return wg_BlendTexture_cf440_stub(hf); }

    // --- R (rock) + G (grass) channels ---
    for (int row = 0; row < dim; ++row) {
        for (int col = 0; col < dim; ++col) {
            const float* e = scratch + (size_t)(dim * row + col) * 6;
            float height = e[2];
            float nz     = e[5];

            // R: rock scatter.
            uint32_t rock;
            if (height >= kRockHMin) {
                float pnv = RtUtility_PerlinNoise_Generate(&pn, (float)col * kCoord,
                                                    (float)row * kCoord);
                pnv = pnv * kRRemap + kRRemap;                  // (pn+1)*0.5 -> [0,1]
                rock = (uint32_t)(long long)(pnv * pnv * k255);
            } else {
                rock = 0xff;
            }

            // G: grass slope band on normal.z.
            uint32_t grass;
            if (nz >= kGUpper || nz <= kGLower) {
                grass = (nz <= kGLower) ? 0xff : 0;
            } else {
                grass = (uint32_t)(long long)((1.0f - (nz - kGLower) / kGBandDiv) * k255);
            }

            // Height ramp: rock above 8.0 (max-blend).
            if (height > kHi8) {
                float t = (height - kHi8) / kBandDiv;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                uint8_t v = (uint8_t)(long long)(t * k255);
                uint8_t rb = (uint8_t)rock;
                rock = (rb <= v) ? v : rb;
            }
            // Height ramp: grass above 7.0 (max-blend).
            if (height > kHi7) {
                float t = (height - kHi7) / kBandDiv;
                t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
                uint8_t v = (uint8_t)(long long)(t * k255);
                uint8_t gb = (uint8_t)grass;
                if (gb < v) grass = v;
            }

            uint8_t* px = &out[(size_t)(dim * row + col) * 4];   // the cell's 4 bytes
            px[0] = (uint8_t)(rock & 0xff);    // R
            px[1] = (uint8_t)(grass & 0xff);   // G
            px[2] = 0;                         // B (the AO pass below)
            px[3] = 0;                         // A (never written by retail)
        }
    }

    // --- B (ambient occlusion) channel: 21×21 circular normal kernel ---
    for (int row = 0; row < dim; ++row) {
        for (int col = 0; col < dim; ++col) {
            const float* ce = scratch + (size_t)(dim * row + col) * 6;
            float cx = ce[0], cy = ce[1], ch = ce[2];
            float cnx = ce[3], cny = ce[4], cnz = ce[5];
            float accumDot = 0.0f, count = 0.0f;
            float sumNx = 0.0f, sumNy = 0.0f, sumNz = 0.0f;

            for (int dz = -10; dz < 10; ++dz) {
                int nrow = row + dz;
                if (nrow < 0 || nrow >= dim) continue;
                for (int dx = -10; dx < 11; ++dx) {
                    int ncol = col + dx;
                    if (ncol < 0 || ncol >= dim) continue;
                    if (dx * dx + dz * dz >= 0x65) continue;     // r^2 < 101 circle clip

                    const float* ne = scratch + (size_t)(dim * nrow + ncol) * 6;
                    float dirx = ne[0] - cx, diry = ne[1] - cy, dirz = ne[2] - ch;
                    float len = sqrtf(dirx * dirx + diry * diry + dirz * dirz);
                    if (len > 0.0f) { dirx /= len; diry /= len; dirz /= len; }
                    float dot = cnx * dirx + cny * diry + cnz * dirz;
                    if (dot < 0.0f) dot = 0.0f;                  // Scalar::Max(dot, 0)
                    accumDot += dot;
                    count += 1.0f;

                    if (dx > -2 && dx < 2 && dz > -2 && dz < 2) { // 3x3 smoothed normal
                        sumNx += ne[3]; sumNy += ne[4]; sumNz += ne[5];
                    }
                }
            }

            float sl  = sqrtf(sumNx * sumNx + sumNy * sumNy + sumNz * sumNz);
            float snz = (sl > 0.0f) ? sumNz / sl : 0.0f;
            float avg = (count > 0.0f) ? accumDot / count : 0.0f;
            float ao  = (1.0f - avg) * snz * k255;
            uint32_t aoByte = (uint32_t)(long long)ao & 0xff;
            out[(size_t)(dim * row + col) * 4 + 2] |= (uint8_t)aoByte;   // B
        }
    }

    System::MemoryAllocator::MemFree(scratch);
    return out;
}

// ============================================================
// fd10 — lazy material resolve (blueprint §5). NULL-safe; trusts Find's return
// and skips the post-Find IsDerivedFrom check (cascade strategy — the standalone
// factory tree is incomplete and IsDerivedFrom NULL-derefs).
//   triple = {System::String name @+0; int cookie @+0x08; External* cache @+0x10}
// Returns the resolved External* (or null -> untextured-but-visible terrain).
// ============================================================
static Graphics::MaterialInstance* wg_ResolveMaterial_fd10(void* triple) {
    if (!triple) return nullptr;
    const ::Object::ExternalFactory* factory = ::Object::ExternalFactory::_singleton;
    if (!factory) return nullptr;

    // String is at +0 of the triple. LEFT: TerrainGenerator's three triples are raw
    // String storage (H5: a System::String member would run its ctor / dtor).
    System::Filename fnbuf(*reinterpret_cast<const System::String*>(triple));
    // The tree trusts Find's result (retail checks IsDerivedFrom MaterialInstance in
    // 0x14001fd10; known_port_issues 14).
    return static_cast<Graphics::MaterialInstance*>(
        const_cast<::Object::External*>(factory->Find(fnbuf)));
}

// ============================================================
// FreePatchListVector (FUN_1400175d0) + scalar list teardown.
// First-light lists are empty (no feature stages), so these are effectively
// no-ops, but they stay faithful for when the feature stages come online.
//
// ENDGAME B.3.2 step 2: wg_FreePatchList is `Collection::Array<>::~Array` over
// the `kSpec_WgPathVec` row that worldgen_f390_coastline.cpp defines for this
// same hf+0x18 array (its append is retail 0x140024460). Compared against the
// retail body 0x1400175d0 (re-decompiled 2026-09-13): per element MemFree the
// inner buffer and zero its header, then MemFree the outer block and zero
// data/count/capacity — which is exactly "destroy every element, then
// ArrayDtorImpl". The old transcription skipped zeroing the inner headers of a
// block it was about to free; unobservable. wg_FreeScalarList is NOT converted:
// it leaves +0x0c (capacity) untouched where ~Array zeroes it, and its retail
// body is not pinned.
// ============================================================
// hf->paths is a TerrainPathList (the kSpec_WgPathVec row): its ~Array is that body.
static void wg_FreePatchList(TerrainPathList* paths) {
    paths->~Array();
}

template <class List>
static void wg_FreeScalarList(List* list) {
    void* data = list->data;
    if (data) System::MemoryAllocator::MemFree(data);
    list->data = nullptr;
    list->size = 0;
}
template <class T>
static void wg_FreeScalarList(Collection::Array<T>* list) {
    void* data = list->_data;
    if (data) System::MemoryAllocator::MemFree(data);
    list->_data = nullptr;
    list->_size = 0;
}

// ============================================================
// c4b0 — TradeWork core orchestrator. Verified from TradeWork_impl_c4b0
// @0x1400ac4b0 (blueprint §0). First-light skips b260 (detail mesh).
// ============================================================
void WorldGeometry::BuildFromHeightfield(TerrainHeightfield* hf) {
    int N = _fieldD0;     // dim*2+1

    // 1. Resize the int16 heightmap to N*N (sub_c890 == Stride02).
    Reimpl_Array_Resize_Stride02(&_arrayD8, N * N);

    // 2. Generate the float heightmap.
    wg_HeightmapPipeline_f390(hf);

    // 3. Quantize float -> int16 (x256 fixed-point) into WorldGeom+0xd8.
    {
        float* fmap = hf->heightmap.data;
        int16_t* hm16 = _arrayD8;
        if (fmap && hm16) {
            for (int row = 0; row < N; ++row) {
                for (int col = 0; col < N; ++col) {
                    int idx = N * row + col;
                    hm16[idx] = (short)(int)(fmap[idx] * kQuantize);
                }
            }
        }
    }

    // 4. Blend/splat texture -> WorldGeom+0x140. Real cf440 produces the
    //    rock/grass/AO splat (green grass, rock on slopes/peaks).
    _heightBytes = wg_BlendTexture_cf440(hf);

    // 5. Height-texture upload (+0x148) and 6. base grid mesh (VB+0x100/IB+0x120).
    UploadHeightImage();    // 0x1400ac780
    BuildTerrainMesh();     // 0x1400ac1e0

    // 7. b260 water mesh: the faithful b260 CONTOUR mesh (rivers
    //      flow / lake shore rings; UV follows each water body's centerline).
    //      Builds into VB+0x160/IB+0x180 with per-body ranges at +0x1c0/+0x1d0,
    //      drawn by geometry_draw.cpp modes 0x6000/0x8000. param_2 = hf
    //      (centerlines at hf+0x18, features at hf+0x48).
    BuildWaterMesh(hf);

    // 8. Detail-group-C resize + the FEATURE-RECORD APPEND LOOP.
    //
    // This is what fills worldGeom+0x1e0 — the map's feature polylines (coastline
    // ring, rivers, paths). Consumers: the ambient-emitter pass in starttown.cpp
    // (StartTown_SpawnEnvironment pass 1) and, crucially, retail's merchant boat
    // ROUTE BUILDER 0x1400456e0, which walks path 0 from the map edge to a trading
    // post's dock. Until 2026-07-30 only the resize was ported, so every entry was
    // {ptr=NULL, count=0} and a merchant boat had no river to follow.
    //
    // ⚠ WHY THIS SAT UNDISCOVERED — the comment that used to be here read:
    //     "8. Detail-group-C resize (no-op for first-light: hf detailCountA == 0) +
    //         the feature-record append loop (empty -> skipped)."
    // That was ACCURATE WHEN WRITTEN: detailCountA really was 0, the loop really had
    // nothing to copy, and skipping it really was correct. Then the retail river /
    // coastline generators landed (worldgen_f390_rivers.cpp, _coastline.cpp) and
    // detailCountA became 4 — but the comment never changed, so to every later reader
    // the missing loop looked like a considered decision rather than an omission.
    // The measured symptom was 4 allocated slots all holding {NULL, 0}.
    //
    // The lesson generalises: a comment that states a CURRENT CONDITION ("count is 0
    // here") rather than an INVARIANT silently becomes a lie the moment the condition
    // changes, and it disguises the gap it created. Prefer invariants; when you must
    // state a condition, say what would make it stop being true.
    //
    // Faithful to the tail of retail TradeWork_impl_c4b0 (0x1400ac4b0):
    //   src  = hf+0x18 (HF_PATCHLIST), stride 0x10 = {void* pts, int count, int cap}
    //          — the 0x1C-stride PathPoint vectors that wg_f390_coastline and the
    //          other f390 feature stages append into.
    //   dest = wg+0x1e0, stride 0x10, element size 8 = one (x,y) float pair.
    //
    // THREE THINGS THAT MATTER (each is a silent wrong-behaviour if missed):
    //  1. The copy is REVERSED. Retail walks the source from count-1 down to 0
    //     (`lVar7 = count * 0x1c`, decrementing). Preserving source order does not
    //     fail loudly — it makes the merchant boat traverse the river the WRONG
    //     WAY, which reads as a routing bug and would not be traced back to here.
    //  2. Only the FIRST 8 BYTES of each 0x1C-byte PathPoint are copied — the (x,y)
    //     pair. The remaining 20 bytes (halfWidth etc.) are deliberately dropped.
    //     DO NOT widen this copy to "rescue" halfWidth: retail drops those bytes too,
    //     and wg+0x1e0 is read by retail code that assumes an 8-byte stride. Callers
    //     that need the channel width read it from the SOURCE via the side table
    //     below (WorldGen_GetPathHalfWidth), which leaves the destination
    //     byte-identical to retail.
    //  3. Retail grows the destination incrementally with a *3 growth factor; we
    //     size it exactly once instead. Deliberate deviation: consumers read only
    //     {ptr, count}, never capacity, so the observable result is identical and
    //     this avoids reimplementing the grow-and-copy dance for a known length.
    {
        const int destCount = hf->paths._size;
        Reimpl_Array_Resize_ArrayOfArrays(&contourOutlines, destCount);

        const RawArray<TerrainPathPoint>* srcBase = hf->paths._data;
        RawArray<Math::Vector2>* destBase = contourOutlines.data;
        int pathsCopied = 0, pointsCopied = 0;

        s_pathHalfWidthCount = 0;
        for (int i = 0; i < kMaxTrackedPaths; ++i)
            s_pathHalfWidth[i] = s_pathHalfWidthMax[i] = -1.0f;

        for (int i = 0; i < destCount && srcBase && destBase; ++i) {
            const RawArray<TerrainPathPoint>& srcRec = srcBase[i];
            RawArray<Math::Vector2>& destRec = destBase[i];
            const TerrainPathPoint* srcPts = srcRec.data;
            const int srcN   = srcRec.size;
            if (!srcPts || srcN <= 0) continue;

            // Capture halfWidth from the SOURCE before the narrowing copy drops it.
            // Scan every point rather than trusting point 0: the "per-path constant"
            // claim comes from the decomp of the path builder, and if +0x14 were the
            // wrong field this min/max would diverge and say so. Cheap insurance
            // against reading a plausible-looking number out of the wrong offset.
            if (i < kMaxTrackedPaths) {
                float hwMin = srcPts[0].outerRadius;
                float hwMax = hwMin;
                for (int j = 1; j < srcN; ++j) {
                    const float hw = srcPts[j].outerRadius;
                    if (hw < hwMin) hwMin = hw;
                    if (hw > hwMax) hwMax = hw;
                }
                s_pathHalfWidth[i]    = hwMin;
                s_pathHalfWidthMax[i] = hwMax;
                if (i >= s_pathHalfWidthCount) s_pathHalfWidthCount = i + 1;
            }

            Math::Vector2* buf = static_cast<Math::Vector2*>(System::MemoryAllocator::MemAlloc((uint64_t)srcN * 8, 4));
            if (!buf) continue;
            // dest[j] = src[srcN-1-j] — the reversal from note (1); only (x, y).
            for (int j = 0; j < srcN; ++j) {
                buf[j].x = srcPts[srcN - 1 - j].x;
                buf[j].y = srcPts[srcN - 1 - j].y;
            }
            destRec.data     = buf;
            destRec.size     = srcN;
            destRec.capacity = srcN;
            ++pathsCopied; pointsCopied += srcN;
        }

        // Always print (one line): an empty result here is exactly the silent
        // failure this port exists to remove, so it must never look like success.
        fprintf(stderr, "[worldgen::river] feature polylines: %d/%d path(s) copied, "
                "%d point(s) total%s\n", pathsCopied, destCount, pointsCopied,
                pathsCopied == 0 ? "   <-- EMPTY: no boat route is possible; check "
                                   "that the f390 feature stages populated hf+0x18"
                                 : "");
        // Always print the half-width table, INCLUDING when it is empty — an absent
        // line must mean "this code did not run", never "there was nothing to say".
        // Expected shape: path 0 = 4.00 (the coastline RING, which hugs the map
        // border — it is not a river), later paths = 1.50 (secondary channels).
        if (s_pathHalfWidthCount == 0) {
            fprintf(stderr, "[worldgen::river] halfWidth table: EMPTY (no path "
                    "contributed a width; dock placement has no bank distance)\n");
        } else {
            fprintf(stderr, "[worldgen::river] halfWidth table (%d path(s)):",
                    s_pathHalfWidthCount);
            for (int i = 0; i < s_pathHalfWidthCount; ++i) {
                fprintf(stderr, "  [%d]=%.2f(bank %.2f)", i, s_pathHalfWidth[i],
                        s_pathHalfWidth[i] < 0.0f ? -1.0f
                                                  : s_pathHalfWidth[i] + PATHPT_BAND_MARGIN);
                if (s_pathHalfWidthMax[i] != s_pathHalfWidth[i])
                    fprintf(stderr, "<-- NOT CONSTANT (max %.2f): +0x14 may be the "
                            "wrong field", s_pathHalfWidthMax[i]);
            }
            fprintf(stderr, "\n");
        }
        fflush(stderr);
    }
}

// The tree MemAllocs the heightfield (retail: b0230's stack) with 0x10 bytes past
// its 0xA30 and zeroes all of it.
static constexpr uint64_t kHeightfieldAlloc = 0xa40;
static_assert(kHeightfieldAlloc >= sizeof(TerrainHeightfield), "the allocation covers the record");

// ============================================================
// b0230 — TerrainBuild_Dispatcher (top-level worldgen entry).
// Verified from FUN_1400b0230 @0x1400b0230 (blueprint §0).
//   wg        = WorldGeometry*
//   gen       = TerrainGenerator*
//   sizeIdx   = terrain size-table index
//   seed      = map seed
// ============================================================
void WorldGeometry::BuildTerrain(TerrainGenerator* gen, int sizeIdx, uint32_t seed) {
    // Size-table row for this map size: gen->sizeTable + sizeIdx*0x44.
    TerrainSizeRow* sizeTable = gen->_field80;
    TerrainSizeRow* sizeRow   = sizeTable ? &sizeTable[sizeIdx] : nullptr;
    int dim = sizeRow ? sizeRow->mapSize : 0;

    fprintf(stderr, "[worldgen] BuildTerrain ENTER wg=%p gen=%p sizeIdx=%d seed=%u sizeTable=%p dim=%d\n",
            static_cast<void*>(this), static_cast<void*>(gen), sizeIdx, seed,
            static_cast<void*>(sizeTable), dim);
    fflush(stderr);

    if (dim <= 0) {
        fprintf(stderr, "[worldgen] BuildTerrain: bad dim=%d (sizeTable=%p) — aborting\n",
                dim, static_cast<void*>(sizeTable));
        fflush(stderr);
        return;
    }

    // WorldGeometry grid dims.
    _gridSize         = dim * 2;
    _fieldC4          = dim;
    _chunkSize        = 0x20;          // 32
    _fieldD0          = dim * 2 + 1;
    _fieldCC          = 0x20 + 1;      // 33
    _chunkIndexStride = 33 * 33;       // 1089

    // Resolve the 3 ground materials (NULL-safe; untextured if unresolved).
    _terrainMaterial = wg_ResolveMaterial_fd10(&gen->_field38);
    _waterMaterialA  = wg_ResolveMaterial_fd10(&gen->_field50);
    _waterMaterialB  = wg_ResolveMaterial_fd10(&gen->_field68);

    // Allocate + init the heightfield scratch struct (heap; retail's is b0230's
    // 0xA30-byte stack record; the tree allocates 0x10 bytes of margin past it).
    TerrainHeightfield* hf = static_cast<TerrainHeightfield*>(System::MemoryAllocator::MemAlloc(kHeightfieldAlloc, 8));
    if (!hf) {
        fprintf(stderr, "[worldgen] BuildTerrain: heightfield alloc failed — aborting\n");
        fflush(stderr);
        return;
    }
    memset(static_cast<void*>(hf), 0, kHeightfieldAlloc);
    RtSystem_Random_CtorSeed(&hf->random, seed);
    hf->generator = gen;
    hf->sizeRow   = sizeRow;

    // Build.
    BuildFromHeightfield(hf);

    // Teardown (reverse of construction).
    wg_FreeScalarList(&hf->waterFeatures);
    wg_FreeScalarList(&hf->basinStamps);
    wg_FreeScalarList(&hf->riverPoints);
    wg_FreePatchList (&hf->paths);
    wg_FreeScalarList(&hf->heightmap);
    System::MemoryAllocator::MemFree(hf);

    fprintf(stderr, "[worldgen] BuildTerrain EXIT — wg=%p dimX2=%d edge=%d hm16=%p mat1=%p blendTex=%p\n",
            static_cast<void*>(this), _gridSize, _fieldD0,
            static_cast<void*>(_arrayD8), static_cast<void*>(_terrainMaterial), _heightBytes);
    // Water diagnostics: the two water materials (+0x150/+0x158), the water vertex
    // buffer's size and the water draw-range counts (+0x1c8/+0x1d8).
    fprintf(stderr, "[worldgen]   waterMatA(+0x150)=%p waterMatB(+0x158)=%p "
            "overlayVB(+0x160).size=%d overlayCntA(+0x1c8)=%d overlayCntB(+0x1d8)=%d\n",
            static_cast<void*>(_waterMaterialA), static_cast<void*>(_waterMaterialB),
            _waterVertexBuffer.size, contourRangesA.size, contourRangesB.size);
    fflush(stderr);
}

// B4 FORWARD: 1 outside caller (gameupdateloop_load.cpp, GAME-a: it holds the two
// objects as void* locals copied from GameUpdateLoop's typed members). The null
// test is the old body's; retail's caller tests both before the call.
extern "C" void WorldGen_BuildTerrain(void* wg, void* gen, int sizeIdx, uint32_t seed) {
    if (!wg || !gen) {
        fprintf(stderr, "[worldgen] BuildTerrain: null wg=%p gen=%p — skipping\n", wg, gen);
        fflush(stderr);
        return;
    }
    // LEFT: the two untyped parameters (the old body's C-style casts) until the
    // closing wave rewires the caller to `m_worldGeometry->BuildTerrain(m_terrainConfig, ...)`.
    ((WorldGeometry*)wg)->BuildTerrain((TerrainGenerator*)gen, sizeIdx, seed);
}
