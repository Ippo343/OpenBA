// code/Game/Game/mapquery.cpp
// Terrain height / world-position query — the keystone "Map::GetTerrainPos" utility.
//
// Reimplements FUN_14009f3e0 (CharacterComponent_impl_f3e0), exposed project-wide as
// fn_MapGetTerrainPos. This single function is referenced by ~100 call sites (camera
// placement, tile classification, every tool/work/component that maps a world XY to a
// terrain point) and is exe-only, so standalone needs a real body for it.
//
// The original is a 3-function chain:
//   f3e0  → 08c0_sub_0f90 → QuarryComponent_helper_aa20 → ..._sub_a810
// aa20 reads the int16 heightmap (WorldGeom+0xd8, row stride WorldGeom+0xd0, value scaled
// by 1/256 — the inverse of worldgen's ×256 quantize), takes the 4 quad corners around the
// sample point, and a810 ray-casts a vertical ray against the quad's two triangles to get the
// interpolated height + surface normal. For a vertical ray against a height-field quad the
// ray-triangle intersection height is exactly the planar interpolation of the corners, so we
// compute it directly (bilinear over the quad) rather than transcribe the mangled 3-fn ray cast.
// Output Vector3 = {worldX, worldZ, height} (matching f3e0's field order).
//
// flag (param_4): when flag==0 AND the cell carries a placed floor (cellRec+0x2d > 0),
// the original refines the height by vertically ray-casting the map's Picking::Scene and
// taking the hit when it is ABOVE the terrain. That branch IS the "walk on the deck"
// mechanism for docks and bridges: a citizen's world Z is recomputed every frame by
// CharacterComponent's move tick (0x1401094c0) as `max(f3e0(pos).z, desc+0x44)`, so
// whatever Z the work action resolved is discarded on the next tick and the standing
// height is entirely whatever this function returns. With the refine missing, a fisherman
// on a dock stood on the riverbed under it. Implemented below, gated on BANISHED_LIVE_FLOOR
// (the producer side — FloorComponent registering the deck mesh — shares that gate);
// default-on since 2026-08-29, so the env var is a kill-switch (=0), not an opt-in.
//
// Verified against Runtime/exe decompiles + worldgen.cpp WG_* offsets (2026-05-31);
// picking-refine branch disassembly-verified at 0x14009f4c6..0x14009f582 (2026-08-29).

#include "engine_funcs.h"
#include "Game/GameUpdateLoop.h"   // caller batch CB6: the world's named fields
#include "resolve.h"
#include "picking/Scene.h"     // ENDGAME B.3.7 callers (K2)
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>

// WorldGeometry offsets (mirror worldgen.cpp WG_*).
static constexpr int WG_DIMX2      = 0xc0;   // int dim*2 (== heightfield gridSize, e.g. 512)
static constexpr int WG_HEIGHTEDGE = 0xd0;   // int N = dim*2+1 (int16 heightmap row stride, 513)
static constexpr int WG_HM16       = 0xd8;   // {short* data, int, int} int16 heightmap (N×N)

static constexpr float kHeightScale = 1.0f / 256.0f;   // DAT_1401720bc (reverses worldgen ×256)

// Sample interpolated terrain height at a world (x, z) — the net effect of aa20+a810.
// worldX/worldZ are in logical world units; the heightmap is indexed at fx = worldX + center,
// center = (WorldGeom+0xc0)>>1, exactly as 08c0_sub_0f90 forms the tile coords.
float MapQuery_SampleHeight(void* worldGeomVoid, float worldX, float worldZ) {
    uint8_t* wg = (uint8_t*)worldGeomVoid;
    if (!wg) return 0.0f;
    const int N = *(int32_t*)(wg + WG_HEIGHTEDGE);
    const short* hm = *(const short**)(wg + WG_HM16);
    if (!hm || N <= 0) return 0.0f;

    const float center = (float)(*(int32_t*)(wg + WG_DIMX2) >> 1);
    const float fx = worldX + center;
    const float fz = worldZ + center;

    // Inline floor: MinGW's floorf is a libcall (2 per sample, ~1000 samples per tick at
    // 800 citizens — the move tick calls this every tick). Truncate-then-fix is exact.
    int ix = (int)fx; if ((float)ix > fx) --ix;
    int iz = (int)fz; if ((float)iz > fz) --iz;
    const float tx = fx - (float)ix;
    const float tz = fz - (float)iz;

    auto clampN = [N](int v) -> int { return v < 0 ? 0 : (v >= N ? N - 1 : v); };
    const int x0 = clampN(ix),     z0 = clampN(iz);
    const int x1 = clampN(ix + 1), z1 = clampN(iz + 1);

    const float h00 = (float)hm[z0 * N + x0] * kHeightScale;
    const float h10 = (float)hm[z0 * N + x1] * kHeightScale;
    const float h01 = (float)hm[z1 * N + x0] * kHeightScale;
    const float h11 = (float)hm[z1 * N + x1] * kHeightScale;

    const float hTop = h00 + (h10 - h00) * tx;
    const float hBot = h01 + (h11 - h01) * tx;
    return hTop + (hBot - hTop) * tz;
}

// Unit surface normal at world (x, z), written into out3 in the engine's
// (x, z, height) component order — so out3[2] is the vertical/flatness term.
//
// This is the net effect of sub_a810, which builds the sampled triangle's two
// edge vectors, takes Cross(edge1, edge2), and Normalize()s it into the outTri
// out-param. The tile classifier (CreateNaturalResource_helper_e2c0) reads only
// outTri[2] (>= 0.9 == buildable). We compute the normal of the
// {(x,z),(x+1,z),(x,z+1)} triangle via forward differences over one texel
// (uv = g_fUVScale = 1.0, 0x14017214c) — exactly one of the two triangles a810
// evaluates: normal = normalize(-dhx*uv, -dhz*uv, uv^2), giving
// out3[2] = uv / sqrt(dhx^2 + dhz^2 + uv^2) (1.0 on flat ground).
void MapQuery_SampleNormal(void* worldGeomVoid, float worldX, float worldZ, float out3[3]) {
    out3[0] = 0.0f; out3[1] = 0.0f; out3[2] = 1.0f;   // default = flat / up
    uint8_t* wg = (uint8_t*)worldGeomVoid;
    if (!wg) return;
    const int N = *(int32_t*)(wg + WG_HEIGHTEDGE);
    const short* hm = *(const short**)(wg + WG_HM16);
    if (!hm || N <= 0) return;

    const float center = (float)(*(int32_t*)(wg + WG_DIMX2) >> 1);
    const float fx = worldX + center, fz = worldZ + center;
    int ix = (int)fx; if ((float)ix > fx) --ix;
    int iz = (int)fz; if ((float)iz > fz) --iz;

    auto clampN = [N](int v) -> int { return v < 0 ? 0 : (v >= N ? N - 1 : v); };
    const int x0 = clampN(ix),     z0 = clampN(iz);
    const int x1 = clampN(ix + 1), z1 = clampN(iz + 1);

    const float h00 = (float)hm[z0 * N + x0] * kHeightScale;
    const float h10 = (float)hm[z0 * N + x1] * kHeightScale;
    const float h01 = (float)hm[z1 * N + x0] * kHeightScale;

    const float uv  = 1.0f;          // g_fUVScale (0x14017214c)
    const float nx  = -(h10 - h00) * uv;
    const float nz  = -(h01 - h00) * uv;
    const float nh  = uv * uv;
    const float inv = 1.0f / sqrtf(nx * nx + nz * nz + nh * nh);
    out3[0] = nx * inv;
    out3[1] = nz * inv;
    out3[2] = nh * inv;              // vertical (slope) term read by the classifier
}

// BANISHED_FLOOR_TRACE is gone (ENDGAME C.2). The symbol stays only until
// code/Game/Component/floorcomponent.cpp drops its call sites; it is dead.

// ---------------------------------------------------------------------------
// Picking refine — the flag==0 + cellRec+0x2d>0 branch of the original
// ---------------------------------------------------------------------------
// Disassembly-verified argument build (0x14009f4f4..0x14009f534):
//   RCX = param_1 + 8                    — the map's OWN Picking::Scene (NOT gw+0x70)
//   RDX = &{inX, inZ, DAT_1401721e8}     — ray start,  DAT_1401721e8 = 50.0f
//   R8  = &{0, 0, DAT_140172280}         — ray dir,    DAT_140172280 = -1.0f (straight down)
//   R9  = &DAT_140172208                 — max dist,   DAT_140172208 = 100.0f
//   [RSP+0x20] = &hitPos (Vector3), [RSP+0x28] = &hitNormal (Vector3)
// Accepted only when the call returns >= 0 AND hitPos.z > the heightmap height
// (0x14009f53a JS / 0x14009f543 COMISS+JBE) — so a floor BELOW the terrain never wins.
static constexpr float kRayStartZ  = 50.0f;    // DAT_1401721e8
static constexpr float kRayDirZ    = -1.0f;    // DAT_140172280
static constexpr float kRayMaxDist = 100.0f;   // DAT_140172208


// The map grid the refine indexes. Retail reads param_1 directly and every retail call
// site passes *(gw+0xf8); our ported call sites are not all provably doing so, and a wrong
// param_1 would read a bogus dim and a bogus cell-array pointer. So resolve the canonical
// grid from the singleton and only refine when param_1 agrees (or is null).
static int* CanonicalMapGrid(void* game) {
    if (!g_pGameWorldSingleton || !*g_pGameWorldSingleton) return nullptr;
    int* canonical = static_cast<int*>(reinterpret_cast<GameUpdateLoop*>(*g_pGameWorldSingleton)->m_game);   // B.3.8 bridge
    if (!canonical) return nullptr;
    if (game && game != (void*)canonical) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[MapQuery] param_1=%p != map grid %p — skipping the "
                                 "picking refine for this call site.\n", game, (void*)canonical);
        }
        return nullptr;
    }
    return canonical;
}

// FUN_14009f3e0 — Map terrain-position query. param_1 = the map grid (int*, *(gw+0xf8)),
// outPos3=float[3] result, inXY2=float[2+] {worldX, worldZ}, flag, outTri = out normal.
extern "C" float* Reimpl_MapGetTerrainPos(void* game, float* outPos3, float* inXY2,
                                          char flag, void* outTri) {
    const float inX = inXY2[0];
    const float inZ = inXY2[1];

    void* worldGeom = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        worldGeom = reinterpret_cast<GameUpdateLoop*>(*g_pGameWorldSingleton)->m_worldGeometry;   // B.3.8 bridge

    const float height = MapQuery_SampleHeight(worldGeom, inX, inZ);

    // outTri (param_5) receives the surface normal — only [2] (slope/flatness) is
    // consumed downstream (the tile classifier), but we fill a true unit normal.
    if (outTri)
        MapQuery_SampleNormal(worldGeom, inX, inZ, (float*)outTri);

    if (flag == 0) {
        int* mapGrid = CanonicalMapGrid(game);
        const uintptr_t cells = mapGrid ? *(uintptr_t*)((uintptr_t)mapGrid + 0x80) : 0;
        const int dim = mapGrid ? mapGrid[0] : 0;
        if (cells && dim > 0) {
            int ix = (int)(inX + (float)(dim >> 1));
            int iy = (int)(inZ + (float)(dim >> 1));
            if (ix < 0)    ix = 0;
            if (ix >= dim) ix = dim - 1;
            if (iy < 0)    iy = 0;
            if (iy >= dim) iy = dim - 1;
            // Signed byte compared > 0, exactly as 0x14009f4e9 (CMP .. ; JLE).
            const int8_t floorCount =
                *(int8_t*)(cells + 0x2d + (int64_t)((int64_t)dim * iy + ix) * 0x30);
            if (floorCount > 0) {
                {
                    const float rayStart[3] = { inX, inZ, kRayStartZ };
                    const float rayDir[3]   = { 0.0f, 0.0f, kRayDirZ };
                    const float maxDist     = kRayMaxDist;
                    float hitPos[3]    = { 0.0f, 0.0f, 0.0f };
                    float hitNormal[3] = { 0.0f, 0.0f, 0.0f };
                    // Picking::Scene::Intersect (import slot 0x140152428); the Scene is
                    // embedded at mapGrid+8.
                    const int hit = reinterpret_cast<const Picking::Scene*>(   // B.3.7 bridge
                        (uintptr_t)mapGrid + 8)->Intersect(
                        *reinterpret_cast<const Vector3*>(rayStart), *reinterpret_cast<const Vector3*>(rayDir),
                        *reinterpret_cast<const Math::Scalar*>(&maxDist),
                        *reinterpret_cast<Vector3*>(hitPos), *reinterpret_cast<Vector3*>(hitNormal));
                    if (hit >= 0 && hitPos[2] > height) {
                        if (outTri) {
                            ((float*)outTri)[0] = hitNormal[0];
                            ((float*)outTri)[1] = hitNormal[1];
                            ((float*)outTri)[2] = hitNormal[2];
                        }
                        outPos3[0] = hitPos[0];
                        outPos3[1] = hitPos[1];
                        outPos3[2] = hitPos[2];
                        return outPos3;
                    }
                }
            }
        }
    }

    outPos3[0] = inX;
    outPos3[1] = inZ;
    outPos3[2] = height;
    return outPos3;
}

#include "address_registry.h"
extern "C" void RegisterMapQueryAddrs() {
    // fn_MapGetTerrainPos (~100 call sites) resolves via ResolveAddr(0x14009f3e0).
    RegisterExeAddr(0x14009f3e0, reinterpret_cast<uintptr_t>(&Reimpl_MapGetTerrainPos));
}
