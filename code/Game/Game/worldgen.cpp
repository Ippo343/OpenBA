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
#include <cmath>

// ============================================================
// Reused helpers from sibling reimpl TUs (already registered/defined)
// ============================================================
extern "C" void Reimpl_Array_Resize_Stride02(void* hdr, int n);        // sub_c890  (int16 heightmap @wg+0xd8)
extern "C" void Reimpl_Array_Resize_Stride04(void* hdr, int n);        // sub_3a00  (float heightmap @hf+0x08)
extern "C" void Reimpl_Array_Resize_ArrayOfArrays(void* hdr, int n);   // sub_f0a0  (detail-group-C @wg+0x1e0)
extern "C" void Reimpl_TradeWork_sub_c780(void* self);                 // height-texture upload (+0x148)
extern "C" void Reimpl_TradeWork_sub_c1e0(void* self);                 // base grid mesh (VB+0x100/IB+0x120)

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
static constexpr int HF_LISTC      = 0x38;   // {void*,int,int,int} mountain satellites
static constexpr int HF_DETAILRECS = 0x48;   // {void*,int,int,int} cliff polys
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

// Toggle: full f390 feature pipeline vs first-light (e5d0 only). Feature stages
// (paths/rivers/erosion/smoothing) land in later slices.
static constexpr bool kFullPipeline = false;

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
        // DEFERRED later slices: cd090/cd150/ccf60 (paths/ranges),
        // cd460/e910 (rivers/stamp), cc080 (erosion), ec50/c350x2 (finalize/smooth).
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

    // 4. Blend/splat texture (stubbed) -> WorldGeom+0x140.
    *(void**)(w + WG_BLENDTEX) = wg_BlendTexture_cf440_stub(hf);

    // 5. Height-texture upload (+0x148) and 6. base grid mesh (VB+0x100/IB+0x120).
    Reimpl_TradeWork_sub_c780(wg);
    Reimpl_TradeWork_sub_c1e0(wg);

    // 7. b260 (detail mesh) — SKIPPED for first-light (detail draws are
    //    count-guarded; counts stay 0 -> no detail draw items issued).

    // 8. Detail-group-C resize (no-op for first-light: hf detailCountA == 0) +
    //    the feature-record append loop (empty -> skipped).
    Reimpl_Array_Resize_ArrayOfArrays(w + WG_DETAILGRPC, *(int32_t*)((uint8_t*)hf + HF_DETAILCNTA));
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
    wg_FreeScalarList(hf + HF_DETAILRECS);
    wg_FreeScalarList(hf + HF_LISTC);
    wg_FreeScalarList(hf + HF_LISTB);
    wg_FreePatchList (hf + HF_PATCHLIST);
    wg_FreeScalarList(hf + HF_HEIGHTMAP);
    if (fn_MemFreeStatic) fn_MemFreeStatic(hfMem);

    fprintf(stderr, "[worldgen] BuildTerrain EXIT — wg=%p dimX2=%d edge=%d hm16=%p mat1=%p blendTex=%p\n",
            wg, *(int32_t*)(w + WG_DIMX2), *(int32_t*)(w + WG_HEIGHTEDGE),
            *(void**)(w + WG_HM16), *(void**)(w + WG_MATERIAL1), *(void**)(w + WG_BLENDTEX));
    fflush(stderr);
}
