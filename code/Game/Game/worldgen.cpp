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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cmath>

// ============================================================
// Reused helpers from sibling reimpl TUs (already registered/defined)
// ============================================================
extern "C" void Reimpl_Array_Resize_Stride02(void* hdr, int n);        // sub_c890  (int16 heightmap @wg+0xd8)
extern "C" void Reimpl_Array_Resize_Stride04(void* hdr, int n);        // sub_3a00  (float heightmap @hf+0x08)
extern "C" void Reimpl_Array_Resize_ArrayOfArrays(void* hdr, int n);   // sub_f0a0  (detail-group-C @wg+0x1e0)
extern "C" void Reimpl_TradeWork_sub_c780(void* self);                 // height-texture upload (+0x148)
extern "C" void Reimpl_TradeWork_sub_c1e0(void* self);                 // base grid mesh (VB+0x100/IB+0x120)
extern "C" void WorldGen_BuildWaterMesh(void* self);                   // flat water overlay mesh (VB+0x160/IB+0x180)
extern "C" void WorldGen_BuildContourWaterMesh(void* self, void* hf);  // b260 contour water mesh (water_contour_mesh.cpp)

// ============================================================
// Heightfield scratch-struct field offsets (verified, blueprint §1.1)
// Base struct ~0xA30 bytes. b0230 stack-allocates it; we heap-allocate
// (large; avoids a giant stack frame) and free at the end.
// ============================================================
static constexpr int HF_GRIDSIZE   = 0x00;   // int  = sizeRow[0]*2  (set by f390)
static constexpr int HF_HEIGHTMAP  = 0x08;   // {float* data, int count, int cap} — (dim+1)^2 floats
static constexpr int HF_PATCHLIST  = 0x18;   // {void* data, int count, int _} — feature patches
static constexpr int HF_DETAILCNTA = 0x20;   // int  count passed to sub_f0a0
static constexpr int HF_LISTB      = 0x28;   // {void*,int,int,int} river points
// +0x38: cd150's radial-stamp list, 0xC elems {x, y, radius}. Named "mountain
// satellites" historically, but that name is BACKWARDS about what they do: the
// e910/ec50 stamp feeds them to eee0 with MAX into maskC (strength 2.0, exp 2.25,
// retail 0x1400ceac2/aca), and the final height is `h = maskB*h*maskA - maskC` —
// maskC is SUBTRACTED. So these carve circular DEPRESSIONS, not peaks. They are
// what produces the round basins dotted across the valley floor.
static constexpr int HF_BASINSTAMPS = 0x38;  // {void*,int,int,int} radial carve stamps
// +0x48: one 0xC record per mountain range {x, y, bandX}. Despite the old name
// "cliff polys", this is the array b260 PHASE 2 reads as the water features
// {x, y, radius} — the tarn disks. Count at +0x50. See water_contour_mesh.cpp.
static constexpr int HF_WATERFEATS  = 0x48;  // {void*,int,int,int} b260 phase-2 tarn features
static constexpr int HF_RANDOM     = 0x58;   // System::Random (~0x9C8 bytes), seeded
static constexpr int HF_TERRAINGEN = 0xa20;  // TerrainGenerator* back-ptr
static constexpr int HF_SIZEROW    = 0xa28;  // int* = gen->sizeTable + sizeIdx*0x44
static constexpr int HF_TOTAL      = 0xa40;  // allocation size (margin past +0xa30)

// ============================================================
// WorldGeometry field offsets (verified, blueprint §1.3)
// ============================================================
static constexpr int WG_DIMX2      = 0xc0;   // int dim*2  (== heightfield gridSize)
static constexpr int WG_DIM        = 0xc4;   // int dim
static constexpr int WG_BASESUBDIV = 0xc8;   // int 32  (per-chunk cell edge)
static constexpr int WG_BASEEDGE   = 0xcc;   // int 33
static constexpr int WG_HEIGHTEDGE = 0xd0;   // int dim*2+1  (= N, int16 heightmap edge)
static constexpr int WG_BASEVERTS  = 0xd4;   // int 33*33
static constexpr int WG_HM16       = 0xd8;   // {short* data,int,int} int16 heightmap (NxN)
static constexpr int WG_MATERIAL1  = 0xf8;   // External* (ground material)
static constexpr int WG_BLENDTEX   = 0x140;  // void* RGBA splat buffer (cf440 result)
static constexpr int WG_MATERIAL2  = 0x150;  // External*
static constexpr int WG_MATERIAL3  = 0x158;  // External*
static constexpr int WG_DETAILGRPC = 0x1e0;  // {void*,int,int} detail-group-C

// Source PathPoint stride for the step-8 feature-record copy. Matches
// PATHPT_STRIDE in worldgen_f390_coastline.cpp (which is static there), and the
// `count * 0x1c` walk in retail TradeWork_impl_c4b0's append loop.
static constexpr int PATHPT_STRIDE_SRC = 0x1c;

// PathPoint field offsets within the 0x1C source stride (water_contour_mesh.cpp:295,
// notes/water_mesh/lake_coverage.md §"+0x14 is a per-path constant"):
//   +0x00 float x, +0x04 float y, ..., +0x14 float halfWidth
// halfWidth is the CHANNEL half-width in tiles. It is written by the path builder
// (FUN_1400cc850 arg a7) identically into every point of a path, so it is a per-path
// constant, not per-point: coastline ring = 4.0 (DAT_140172194), secondary paths =
// 1.5 (DAT_140172160). The RENDERED water band is wider still — fb00 uses
// halfWidth + 2.0 (DAT_140172174) — so "distance from centerline to dry land" is
// halfWidth + 2, not halfWidth.
static constexpr int PATHPT_OFF_HALFWIDTH = 0x14;
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
// TerrainGenerator material-triple offsets (input to fd10).
// Each triple = {System::String name @+0; int cookie @+0x08; External* cache @+0x10}.
// ============================================================
static constexpr int GEN_SIZETABLE = 0x80;   // int* (rows of 0x44 bytes, indexed by sizeIdx)
static constexpr int GEN_MAT1       = 0x38;
static constexpr int GEN_MAT2       = 0x50;
static constexpr int GEN_MAT3       = 0x68;

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

// ============================================================
// Late-bound Runtime resolve: seeded Random ctor.
// (??0Random@System@@QEAA@I@Z — same one gameupdateloop_load.cpp resolves.)
// ============================================================
typedef void* (*fn_Random_Seeded_Ctor_t)(void* self, uint32_t seed);
static fn_Random_Seeded_Ctor_t s_fn_Random_Seeded_Ctor = nullptr;
static bool s_wgResolved = false;
static void wg_EnsureInit() {
    if (s_wgResolved) return;
    s_wgResolved = true;
    s_fn_Random_Seeded_Ctor = (fn_Random_Seeded_Ctor_t)ResolveExport("??0Random@System@@QEAA@I@Z");
    fprintf(stderr, "[worldgen] resolves: RandSeed=%p PerlinCtor=%p PerlinGen=%p RandTol=%p\n",
            (void*)s_fn_Random_Seeded_Ctor, (void*)fn_PerlinNoise_Ctor,
            (void*)fn_PerlinNoise_Generate, (void*)fn_Random_RandTol);
    fflush(stderr);
}

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

// f390 feature/normalization stages (each operates on the heightfield `hf`).
extern "C" void wg_f390_coastline(void* hf);   // cd090 — coastline ring -> patchList
extern "C" void wg_f390_ranges(void* hf);      // cd150 — mountain ranges -> listC
extern "C" void wg_f390_paths(void* hf);       // ccf60 — secondary paths -> patchList
extern "C" void wg_f390_rivers(void* hf);      // cd460 — river network -> listB
extern "C" void wg_f390_stamp(void* hf);       // e910  — feature stamp BUILD (carves height)
extern "C" void wg_f390_erosion(void* hf);     // cc080 — droplet erosion
extern "C" void wg_f390_carve(void* hf);       // ec50  — feature stamp finalize/carve
extern "C" void wg_f390_smooth(void* hf);      // c350  — 3-tap box-blur (called twice)

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
static void wg_BaseHeightmap_e5d0(int32_t* hf) {
    uint8_t* p = (uint8_t*)hf;
    int dim  = *(int32_t*)(p + HF_GRIDSIZE);   // = mapDim*2
    int edge = dim + 1;                          // (dim+1) vertices per side

    // Resize the float heightmap to (dim+1)^2 (sub_3a00 == Stride04).
    Reimpl_Array_Resize_Stride04(p + HF_HEIGHTMAP, edge * edge);
    float* heightmap = *(float**)(p + HF_HEIGHTMAP);
    if (!heightmap) return;

    if (!fn_PerlinNoise_Ctor || !fn_PerlinNoise_Generate || !fn_Random_RandTol) {
        // Without noise we can't generate — leave a flat (zero) heightmap.
        fprintf(stderr, "[worldgen] e5d0: noise/RNG unresolved (PerlinCtor=%p Gen=%p RandTol=%p) "
                "— flat heightmap\n", (void*)fn_PerlinNoise_Ctor,
                (void*)fn_PerlinNoise_Generate, (void*)fn_Random_RandTol);
        fflush(stderr);
        return;
    }

    void* rng = p + HF_RANDOM;
    int s1 = fn_Random_RandTol(rng, 0xffffff);
    int s2 = fn_Random_RandTol(rng, 0xffffff);
    uint8_t pn1[0x100] = {};
    fn_PerlinNoise_Ctor(pn1, kHalf, kOctaves, s2, s1);          // persist 0.5
    int s3 = fn_Random_RandTol(rng, 0xffffff);
    int s4 = fn_Random_RandTol(rng, 0xffffff);
    uint8_t pn2[0x100] = {};
    fn_PerlinNoise_Ctor(pn2, kQuarter, kOctaves, s4, s3);       // persist 0.25

    int half = dim >> 1;
    float distBias = (float)dim * kQuarter + kFalloffBias;      // dim*0.25 + 16

    for (int row = 0; row < edge; ++row) {
        int dz = row - half;
        for (int col = 0; col < edge; ++col) {
            int dx = col - half;
            float dist  = sqrtf((float)(dx * dx + dz * dz));
            float curve = powf(kFalloffBase, distBias - dist);  // 1.1 ^ ((dim*0.25+16) - dist)
            float w     = kOne / (curve + kOne);

            float pn1s = fn_PerlinNoise_Generate(pn1, (float)col * kQuarter, (float)row * kQuarter);
            float base = (pn1s + kOne) * kBaseScale;             // (pn1+1)*8
            float pn2s = fn_PerlinNoise_Generate(pn2, (float)col * kPn2Coord, (float)row * kPn2Coord);
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
static void wg_HeightmapPipeline_f390(int32_t* hf) {
    uint8_t* p = (uint8_t*)hf;
    int32_t* sizeRow = *(int32_t**)(p + HF_SIZEROW);
    *(int32_t*)(p + HF_GRIDSIZE) = (sizeRow ? sizeRow[0] : 0) * 2;   // gridSize = mapDim*2

    wg_BaseHeightmap_e5d0(hf);

    if (kFullPipeline) {
        // Verified f390 order (blueprint §0). g_pGameWorldSingleton+0x24 stage
        // counters (1..5) are progress hints only; omitted (no consumer in standalone).
        wg_f390_coastline(hf);   // cd090  (gworld+0x24 = 2)
        wg_f390_ranges(hf);      // cd150
        wg_f390_paths(hf);       // ccf60
        wg_f390_rivers(hf);      // cd460  (gworld+0x24 = 3)
        wg_f390_stamp(hf);       // e910   — first carve pass
        wg_f390_erosion(hf);     // cc080  (gworld+0x24 = 4)
        wg_f390_carve(hf);       // ec50   (gworld+0x24 = 5) — finalize carve
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
static void* wg_BlendTexture_cf440_stub(int32_t* hf) {
    int dim = *(int32_t*)((uint8_t*)hf + HF_GRIDSIZE);   // = mapDim*2 (== WorldGeom+0xc0)
    if (dim <= 0 || !fn_MemAlloc || !g_pMemAllocSingleton) return nullptr;
    uint64_t n = (uint64_t)dim * (uint64_t)dim;
    void* buf = fn_MemAlloc(g_pMemAllocSingleton, n * 4, 4);
    if (buf) {
        // Neutral mid splat weights (R rock / G grass / B AO, A=0xff). With null
        // materials the shader falls back to a flat surface — visible terrain.
        uint32_t* px = (uint32_t*)buf;
        for (uint64_t i = 0; i < n; ++i) px[i] = 0x80808080u;
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
static void* wg_BlendTexture_cf440(int32_t* hf) {
    int dim = *(int32_t*)((uint8_t*)hf + HF_GRIDSIZE);
    const float* hm = *(const float**)((uint8_t*)hf + HF_HEIGHTMAP);
    void* rng = (uint8_t*)hf + HF_RANDOM;
    if (dim <= 0 || !hm || !fn_MemAlloc || !g_pMemAllocSingleton ||
        !fn_PerlinNoise_Ctor || !fn_PerlinNoise_Generate || !fn_Random_RandTol) {
        fprintf(stderr, "[worldgen] cf440: deps unresolved (hm=%p PerlinCtor=%p Gen=%p "
                "RandTol=%p) — falling back to flat splat\n", (void*)hm,
                (void*)fn_PerlinNoise_Ctor, (void*)fn_PerlinNoise_Generate,
                (void*)fn_Random_RandTol);
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
    int s1 = fn_Random_RandTol(rng, 0xffffff);
    int s2 = fn_Random_RandTol(rng, 0xffffff);
    uint8_t pn[0x100] = {};
    fn_PerlinNoise_Ctor(pn, kCoord, kCfOctaves, s2, s1);

    // Per-cell scratch: dim*dim * {x,y,h,nx,ny,nz} (6 floats / 0x18 bytes).
    float* scratch = (float*)fn_MemAlloc(g_pMemAllocSingleton,
                                         (uint64_t)dim * dim * 0x18, 4);
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

    uint32_t* out = (uint32_t*)fn_MemAlloc(g_pMemAllocSingleton,
                                           (uint64_t)dim * dim * 4, 4);
    if (!out) { fn_MemFreeStatic(scratch); return wg_BlendTexture_cf440_stub(hf); }

    // --- R (rock) + G (grass) channels ---
    for (int row = 0; row < dim; ++row) {
        for (int col = 0; col < dim; ++col) {
            const float* e = scratch + (size_t)(dim * row + col) * 6;
            float height = e[2];
            float nz     = e[5];

            // R: rock scatter.
            uint32_t rock;
            if (height >= kRockHMin) {
                float pnv = fn_PerlinNoise_Generate(pn, (float)col * kCoord,
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

            out[dim * row + col] = ((grass & 0xff) << 8) | (rock & 0xff);
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
            out[dim * row + col] |= aoByte << 16;
        }
    }

    fn_MemFreeStatic(scratch);
    return out;
}

// ============================================================
// fd10 — lazy material resolve (blueprint §5). NULL-safe; trusts Find's return
// and skips the post-Find IsDerivedFrom check (cascade strategy — the standalone
// factory tree is incomplete and IsDerivedFrom NULL-derefs).
//   triple = {System::String name @+0; int cookie @+0x08; External* cache @+0x10}
// Returns the resolved External* (or null -> untextured-but-visible terrain).
// ============================================================
static void* wg_ResolveMaterial_fd10(void* triple) {
    if (!triple || !g_pExternalFactorySingleton) return nullptr;
    void* factory = *g_pExternalFactorySingleton;
    if (!factory || !fn_Filename_CtorString || !fn_ExternalFactory_Find) return nullptr;

    uint8_t fnbuf[0x20] = {};                    // System::Filename (small String wrapper)
    fn_Filename_CtorString(fnbuf, triple);       // String is at +0 of the triple
    void* result = fn_ExternalFactory_Find(factory, fnbuf);
    if (fn_Filename_Dtor) fn_Filename_Dtor(fnbuf);
    return result;
}

// ============================================================
// FreePatchListVector (FUN_1400175d0) + scalar list teardown.
// First-light lists are empty (no feature stages), so these are effectively
// no-ops, but they stay faithful for when the feature stages come online.
// ============================================================
static void wg_FreePatchList(void* hdr) {
    uint8_t* h = (uint8_t*)hdr;
    void* data = *(void**)(h + 0x00);
    int   count = *(int32_t*)(h + 0x08);
    if (data && fn_MemFreeStatic) {
        uint8_t* e = (uint8_t*)data;
        for (int i = 0; i < count; ++i) {
            void* inner = *(void**)(e + (size_t)i * 0x10);   // each elem = {void* recs, int count}
            if (inner) fn_MemFreeStatic(inner);
        }
        fn_MemFreeStatic(data);
    }
    *(void**)(h + 0x00) = nullptr;
    *(int32_t*)(h + 0x08) = 0;
    *(int32_t*)(h + 0x0c) = 0;
}

static void wg_FreeScalarList(void* hdr) {
    uint8_t* h = (uint8_t*)hdr;
    void* data = *(void**)(h + 0x00);
    if (data && fn_MemFreeStatic) fn_MemFreeStatic(data);
    *(void**)(h + 0x00) = nullptr;
    *(int32_t*)(h + 0x08) = 0;
}

// ============================================================
// c4b0 — TradeWork core orchestrator. Verified from TradeWork_impl_c4b0
// @0x1400ac4b0 (blueprint §0). First-light skips b260 (detail mesh).
// ============================================================
static void wg_TradeCore_c4b0(void* wg, int32_t* hf) {
    uint8_t* w = (uint8_t*)wg;
    int N = *(int32_t*)(w + WG_HEIGHTEDGE);     // dim*2+1

    // 1. Resize the int16 heightmap to N*N (sub_c890 == Stride02).
    Reimpl_Array_Resize_Stride02(w + WG_HM16, N * N);

    // 2. Generate the float heightmap.
    wg_HeightmapPipeline_f390(hf);

    // [WATER-DIAG] one-shot dump of the f390 list counts that feed the water
    // mesh (BANISHED_WATER_DIAG=1). patches(+0x20) -> b260 phase1 fb00 strips;
    // features(+0x50) -> b260 phase2 disk-fill; rivers(+0x30) + satellites(+0x40)
    // are not band-meshed. numSecondary = sizeRow[8] (drives ccf60 path count).
    if (getenv("BANISHED_WATER_DIAG")) {
        uint8_t* h = (uint8_t*)hf;
        int32_t* sr = *(int32_t**)(h + HF_SIZEROW);
        fprintf(stderr,
            "[WATER-DIAG] patches(+0x20)=%d rivers(+0x30)=%d satellites(+0x40)=%d "
            "features(+0x50)=%d numSecondary(sizeRow[8])=%d\n",
            *(int*)(h + 0x20), *(int*)(h + 0x30), *(int*)(h + 0x40),
            *(int*)(h + 0x50), sr ? sr[8] : -1);
        // Always-runs underwater check (post-carve float heightmap). If hmin > -0.75
        // then NO Phase-2 tarn disk can ever emit, regardless of feature count —
        // i.e. there is no deep open water to fill, confirming linear-water model.
        float* fmap = *(float**)(h + HF_HEIGHTMAP);
        if (fmap) {
            long under = 0; float hmin = 1e9f, hmax = -1e9f;
            for (int i = 0; i < N * N; ++i) {
                float z = fmap[i];
                if (z < hmin) hmin = z;
                if (z > hmax) hmax = z;
                if (z <= -0.75f) ++under;
            }
            fprintf(stderr, "[WATER-DIAG] heightmap %dx%d: hmin=%.3f hmax=%.3f "
                    "underCells(<=-0.75)=%ld/%d\n", N, N, hmin, hmax, under, N * N);
        }
        // Satellite-sign check: cd150's listC (+0x38, stride 0xC {x,y,radius})
        // entries are meant to become POSITIVE radial bumps (mountain peaks) via
        // the e910/ec50 stamp. If the map instead shows a round PIT at each
        // satellite, the stamp sign/mask is inverted and those pits are what is
        // flooding + reading as dark holes. Report the OUTCOME: sample the carved
        // heightmap at each satellite centre and bucket it.
        if (fmap) {
            uint8_t* lc = (uint8_t*)(h + HF_BASINSTAMPS);
            float* pts = *(float**)lc;
            int  n = *(int*)(h + 0x40);
            int peaks = 0, pits = 0, flat = 0, underw = 0, oob = 0;
            float lo = 1e9f, hi = -1e9f, rawLo = 1e9f, rawHi = -1e9f;
            for (int i = 0; pts && i < n; ++i) {
                float sx = pts[i * 3 + 0], sy = pts[i * 3 + 1];
                if (sx < rawLo) rawLo = sx; if (sx > rawHi) rawHi = sx;
                if (sy < rawLo) rawLo = sy; if (sy > rawHi) rawHi = sy;
                int ci = (int)sx, ri = (int)sy;
                if (ci < 0 || ci >= N || ri < 0 || ri >= N) { ++oob; continue; }
                float z = fmap[(size_t)ri * N + ci];
                if (z < lo) lo = z; if (z > hi) hi = z;
                if (z <= -0.75f) ++underw;
                if (z > 4.0f) ++peaks; else if (z < 0.0f) ++pits; else ++flat;
            }
            fprintf(stderr, "[WATER-DIAG] listC satellites n=%d at-centre height: "
                    "peaks(>4)=%d pits(<0)=%d flat=%d underwater(<=-0.75)=%d "
                    "range=[%.2f,%.2f] rawXY=[%.1f,%.1f] oob=%d N=%d\n",
                    n, peaks, pits, flat, underw,
                    (double)lo, (double)hi, (double)rawLo, (double)rawHi, oob, N);
        }
        // Assumption-free dump: write the carved float heightmap so the terrain can
        // be inspected directly (round craters? where? how many?) without trusting
        // any scratch-list layout. BANISHED_WATER_DIAG only.
        if (fmap) {
            FILE* fh = fopen("C:/GOG Games/Banished/heightmap_dump.raw", "wb");
            if (fh) { fwrite(&N, 4, 1, fh); fwrite(fmap, 4, (size_t)N * N, fh); fclose(fh);
                      fprintf(stderr, "[WATER-DIAG] heightmap dumped: %dx%d floats\n", N, N); }
        }
        fflush(stderr);
    }

    // 3. Quantize float -> int16 (x256 fixed-point) into WorldGeom+0xd8.
    {
        float* fmap = *(float**)((uint8_t*)hf + HF_HEIGHTMAP);
        short* hm16 = *(short**)(w + WG_HM16);
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
    //    rock/grass/AO splat (green grass, rock on slopes/peaks). Set
    //    BANISHED_FLAT_SPLAT=1 to fall back to the old flat-grey stub for A/B.
    const char* flat = getenv("BANISHED_FLAT_SPLAT");
    bool useFlat = flat && flat[0] && flat[0] != '0';
    *(void**)(w + WG_BLENDTEX) = useFlat ? wg_BlendTexture_cf440_stub(hf)
                                         : wg_BlendTexture_cf440(hf);

    // 5. Height-texture upload (+0x148) and 6. base grid mesh (VB+0x100/IB+0x120).
    Reimpl_TradeWork_sub_c780(wg);
    Reimpl_TradeWork_sub_c1e0(wg);

    // 7. b260 water mesh. Two modes:
    //    * BANISHED_WATER_CONTOUR=1 -> the faithful b260 CONTOUR mesh (rivers
    //      flow / lake shore rings; UV follows each water body's centerline).
    //      Builds into VB+0x160/IB+0x180 with per-body ranges at +0x1c0/+0x1d0,
    //      drawn by geometry_draw.cpp modes 0x6000/0x8000. param_2 = hf
    //      (centerlines at hf+0x18, features at hf+0x48).
    //    * default -> the minimal flat WATER overlay mesh at sea level into the
    //      same VB+0x160/IB+0x180 (mode 0xA000). BANISHED_NO_WATER=1 disables.
    {
        const char* contour = getenv("BANISHED_WATER_CONTOUR");
        const char* nw = getenv("BANISHED_NO_WATER");
        bool noWater = (nw && nw[0] && nw[0] != '0');
        if (contour && contour[0] && contour[0] != '0') {
            WorldGen_BuildContourWaterMesh(wg, hf);
        } else if (!noWater) {
            WorldGen_BuildWaterMesh(wg);
        }
    }

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
        const int destCount = *(int32_t*)((uint8_t*)hf + HF_DETAILCNTA);
        Reimpl_Array_Resize_ArrayOfArrays(w + WG_DETAILGRPC, destCount);

        uint8_t* srcBase  = *(uint8_t**)((uint8_t*)hf + HF_PATCHLIST);
        uint8_t* destBase = *(uint8_t**)(w + WG_DETAILGRPC);
        const char* rt = getenv("BANISHED_RIVER_TRACE");
        const bool trace = rt && rt[0] && rt[0] != '0';
        int pathsCopied = 0, pointsCopied = 0;

        s_pathHalfWidthCount = 0;
        for (int i = 0; i < kMaxTrackedPaths; ++i)
            s_pathHalfWidth[i] = s_pathHalfWidthMax[i] = -1.0f;

        for (int i = 0; i < destCount && srcBase && destBase; ++i) {
            uint8_t* srcRec  = srcBase  + (size_t)i * 0x10;
            uint8_t* destRec = destBase + (size_t)i * 0x10;
            uint8_t* srcPts  = *(uint8_t**)(srcRec + 0x00);
            const int srcN   = *(int32_t*)(srcRec + 0x08);
            if (trace)
                fprintf(stderr, "[worldgen::river]   src[%d] pts=%p n=%d\n",
                        i, (void*)srcPts, srcN);
            if (!srcPts || srcN <= 0) continue;

            // Capture halfWidth from the SOURCE before the narrowing copy drops it.
            // Scan every point rather than trusting point 0: the "per-path constant"
            // claim comes from the decomp of the path builder, and if +0x14 were the
            // wrong field this min/max would diverge and say so. Cheap insurance
            // against reading a plausible-looking number out of the wrong offset.
            if (i < kMaxTrackedPaths) {
                float hwMin = *(const float*)(srcPts + PATHPT_OFF_HALFWIDTH);
                float hwMax = hwMin;
                for (int j = 1; j < srcN; ++j) {
                    const float hw = *(const float*)(srcPts + (size_t)j * PATHPT_STRIDE_SRC
                                                     + PATHPT_OFF_HALFWIDTH);
                    if (hw < hwMin) hwMin = hw;
                    if (hw > hwMax) hwMax = hw;
                }
                s_pathHalfWidth[i]    = hwMin;
                s_pathHalfWidthMax[i] = hwMax;
                if (i >= s_pathHalfWidthCount) s_pathHalfWidthCount = i + 1;
            }

            void* buf = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton,
                                                  (uint64_t)srcN * 8, 4) : nullptr;
            if (!buf) continue;
            // dest[j] = src[srcN-1-j] — the reversal from note (1).
            for (int j = 0; j < srcN; ++j)
                *(uint64_t*)((uint8_t*)buf + (size_t)j * 8) =
                    *(uint64_t*)(srcPts + (size_t)(srcN - 1 - j) * PATHPT_STRIDE_SRC);
            *(void**)(destRec + 0x00)  = buf;
            *(int32_t*)(destRec + 0x08) = srcN;
            *(int32_t*)(destRec + 0x0c) = srcN;
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

// ============================================================
// b0230 — TerrainBuild_Dispatcher (top-level worldgen entry).
// Verified from FUN_1400b0230 @0x1400b0230 (blueprint §0).
//   wg        = WorldGeometry*
//   gen       = TerrainGenerator*
//   sizeIdx   = terrain size-table index
//   seed      = map seed
// ============================================================
extern "C" void WorldGen_BuildTerrain(void* wg, void* gen, int sizeIdx, uint32_t seed) {
    wg_EnsureInit();
    if (!wg || !gen) {
        fprintf(stderr, "[worldgen] BuildTerrain: null wg=%p gen=%p — skipping\n", wg, gen);
        fflush(stderr);
        return;
    }

    uint8_t* w = (uint8_t*)wg;
    uint8_t* g = (uint8_t*)gen;

    // Size-table row for this map size: gen->sizeTable + sizeIdx*0x44.
    int32_t* sizeTable = *(int32_t**)(g + GEN_SIZETABLE);
    int32_t* sizeRow   = sizeTable ? (int32_t*)((uint8_t*)sizeTable + (size_t)sizeIdx * 0x44) : nullptr;
    int dim = sizeRow ? sizeRow[0] : 0;

    // Optional reproducible seed (BANISHED_WORLDGEN_SEED=N) — pins the worldgen RNG
    // so water/feature diagnostics are repeatable across runs.
    if (const char* se = getenv("BANISHED_WORLDGEN_SEED")) {
        seed = (uint32_t)strtoul(se, nullptr, 10);
    }

    fprintf(stderr, "[worldgen] BuildTerrain ENTER wg=%p gen=%p sizeIdx=%d seed=%u sizeTable=%p dim=%d\n",
            wg, gen, sizeIdx, seed, (void*)sizeTable, dim);
    fflush(stderr);

    if (dim <= 0) {
        fprintf(stderr, "[worldgen] BuildTerrain: bad dim=%d (sizeTable=%p) — aborting\n",
                dim, (void*)sizeTable);
        fflush(stderr);
        return;
    }

    // WorldGeometry grid dims.
    *(int32_t*)(w + WG_DIMX2)      = dim * 2;
    *(int32_t*)(w + WG_DIM)        = dim;
    *(int32_t*)(w + WG_BASESUBDIV) = 0x20;        // 32
    *(int32_t*)(w + WG_HEIGHTEDGE) = dim * 2 + 1;
    *(int32_t*)(w + WG_BASEEDGE)   = 0x20 + 1;    // 33
    *(int32_t*)(w + WG_BASEVERTS)  = 33 * 33;     // 1089

    // Resolve the 3 ground materials (NULL-safe; untextured if unresolved).
    *(void**)(w + WG_MATERIAL1) = wg_ResolveMaterial_fd10(g + GEN_MAT1);
    *(void**)(w + WG_MATERIAL2) = wg_ResolveMaterial_fd10(g + GEN_MAT2);
    *(void**)(w + WG_MATERIAL3) = wg_ResolveMaterial_fd10(g + GEN_MAT3);

    // Allocate + init the heightfield scratch struct (heap; ~0xA30 bytes).
    void* hfMem = fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, HF_TOTAL, 8) : nullptr;
    if (!hfMem) {
        fprintf(stderr, "[worldgen] BuildTerrain: heightfield alloc failed — aborting\n");
        fflush(stderr);
        return;
    }
    memset(hfMem, 0, HF_TOTAL);
    uint8_t* hf = (uint8_t*)hfMem;
    if (s_fn_Random_Seeded_Ctor) s_fn_Random_Seeded_Ctor(hf + HF_RANDOM, seed);
    *(void**)(hf + HF_TERRAINGEN) = gen;
    *(int32_t**)(hf + HF_SIZEROW) = sizeRow;

    // Build.
    wg_TradeCore_c4b0(wg, (int32_t*)hf);

    // Teardown (reverse of construction; first-light lists are empty no-ops).
    wg_FreeScalarList(hf + HF_WATERFEATS);
    wg_FreeScalarList(hf + HF_BASINSTAMPS);
    wg_FreeScalarList(hf + HF_LISTB);
    wg_FreePatchList (hf + HF_PATCHLIST);
    wg_FreeScalarList(hf + HF_HEIGHTMAP);
    if (fn_MemFreeStatic) fn_MemFreeStatic(hfMem);

    fprintf(stderr, "[worldgen] BuildTerrain EXIT — wg=%p dimX2=%d edge=%d hm16=%p mat1=%p blendTex=%p\n",
            wg, *(int32_t*)(w + WG_DIMX2), *(int32_t*)(w + WG_HEIGHTEDGE),
            *(void**)(w + WG_HM16), *(void**)(w + WG_MATERIAL1), *(void**)(w + WG_BLENDTEX));
    // Water diagnostics: the two water materials (+0x150/+0x158) and the overlay
    // mesh draw-item counts (+0x1c8/+0x1d8). If matWater* are null the water
    // material resource didn't load; if the counts are 0 the b260 overlay mesh
    // wasn't built (b260 is skipped in our worldgen).
    fprintf(stderr, "[worldgen]   waterMatA(+0x150)=%p waterMatB(+0x158)=%p "
            "overlayVB(+0x160)=%p overlayCntA(+0x1c8)=%d overlayCntB(+0x1d8)=%d\n",
            *(void**)(w + 0x150), *(void**)(w + 0x158), *(void**)(w + 0x160),
            *(int32_t*)(w + 0x1c8), *(int32_t*)(w + 0x1d8));
    fflush(stderr);
}
