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
// flag (param_4): the original, when flag==0 and the cell has a placed object
// (tileGrid[idx]+0x2d > 0), refines the height via Picking::Scene::Intersect. Default cells
// have +0x2d==0, so that path is skipped; we defer Picking and return the heightmap height.
//
// Verified against Runtime/exe decompiles + worldgen.cpp WG_* offsets (2026-05-31).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
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

    int ix = (int)floorf(fx);
    int iz = (int)floorf(fz);
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
    const int ix = (int)floorf(worldX + center);
    const int iz = (int)floorf(worldZ + center);

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

// FUN_14009f3e0 — Map terrain-position query. param_1=Game (unused on the height-only path),
// outPos3=float[3] result, inXY2=float[2+] {worldX, worldZ}, flag, outTri (ignored).
extern "C" float* Reimpl_MapGetTerrainPos(void* /*game*/, float* outPos3, float* inXY2,
                                          char /*flag*/, void* outTri) {
    const float inX = inXY2[0];
    const float inZ = inXY2[1];

    void* worldGeom = nullptr;
    if (g_pGameWorldSingleton && *g_pGameWorldSingleton)
        worldGeom = *(void**)((uint8_t*)*g_pGameWorldSingleton + 0x110);

    const float height = MapQuery_SampleHeight(worldGeom, inX, inZ);

    // outTri (param_5) receives the surface normal — only [2] (slope/flatness) is
    // consumed downstream (the tile classifier), but we fill a true unit normal.
    if (outTri)
        MapQuery_SampleNormal(worldGeom, inX, inZ, (float*)outTri);

    // Picking refine (flag==0 + cell has an object) deferred — default cells skip it.
    outPos3[0] = inX;
    outPos3[1] = inZ;
    outPos3[2] = height;
    return outPos3;
}

#ifdef STANDALONE_EXE
#include "address_registry.h"
extern "C" void RegisterMapQueryAddrs() {
    // fn_MapGetTerrainPos (~100 call sites) resolves via ResolveAddr(0x14009f3e0).
    RegisterExeAddr(0x14009f3e0, reinterpret_cast<uintptr_t>(&Reimpl_MapGetTerrainPos));
}
#endif // STANDALONE_EXE
