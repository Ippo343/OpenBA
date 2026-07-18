// code/Game/Player/tool_base_tilerender.cpp
// Player wave — "tilerender" sub-batch. The PlacementTool (and RoadTool) footprint
// VALIDITY OVERLAY: the green/red (and yellow "existing") tiles drawn on the terrain
// under the building ghost. This is the last piece of the placement UX — it shows
// whether the current spot is buildable AND which way the building's entry faces.
//
// Closure (resolved via ResolveAddr(Module::Exe, 0xADDR) from placementtool.cpp's
// Draw slot):
//   0x140101380  sub_1380  — build the per-cell tile-record array (col,row,variant,
//                            color,angle) validating each cell via f5d0, then hand it
//                            to 08c0 to tessellate + submit.  [ported here]
//   0x1401008c0  08c0 (CharacterComponent_impl_08c0) — Video::Context overlay render:
//                            FindOrLoad "Terrain/Selection.rsc" material, then for each
//                            record emit 6 verts (terrain-conformed quad, rotated-atlas
//                            UV, per-record RGBA color).  [ported here]
//   0x1400b10e0  b10e0     — sample a tile's 6 terrain corners (18 floats) from the
//                            heightfield with the correct even/odd triangulation. [ported here]
//   0x14009f5d0  f5d0      — single-cell terrain/occupancy validity test. ALREADY
//                            registered (reg_player_path.cpp → PlayerPath_CharComp_f5d0);
//                            resolved via ResolveAddr here.
//   0x1400b0f90  0f90      — vertical-down-ray terrain height sampler. ALREADY registered
//                            (reg_pf_road.cpp → Pf_CellQuadRayWrap_0f90). Only used by
//                            08c0's FIRST loop (road-segment quads), which is DEAD on the
//                            building-placement path (param_4 record count is always 0
//                            from sub_1380), so its void-return registration is harmless
//                            here. Resolved via ResolveAddr.
//
// Each function is a 1:1 port of the Ghidra decompile + disassembly. Registration is in
// standalone/reg_player_tilerender.cpp (RegisterPlayerTileRenderBatch), wired into
// RegisterCodeFunctions().

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace {

// ---- Bit-exact .rdata float constants (raw .rdata constants are NULL via
// ResolveAddr in standalone — reference_standalone_rdata_null_fallback — so inline
// the verified values). ----------------------------------------------------------
static inline float C_half()    { return 0.5f;         }  // DAT_140172114
static inline float C_uvScale() { return 0.125f;       }  // DAT_1401720ec (0x3e000000)
static inline float C_deg2rad() { return 0.017453292f; }  // DAT_1401720cc (0x3c8efa35)
static inline float C_hScale()  { return 0.00390625f;  }  // DAT_1401720bc (0x3b800000 = 1/256)

// ---- Runtime-x64.dll exports used by the overlay render (resolved lazily). --------
// Names recovered from the exe dispatch-slot name-table (0x1401c027e..).
typedef void* (*fn_V3Ctor3_t)(void* self, float x, float y, float z);   // ??0Vector3@Math@@QEAA@MMM@Z (slot 0x140152a30)
typedef void  (*fn_MatSetup_t)(void* mat, void* ctx, int flag);         // ?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z (slot 0x140152920)
typedef void* (*fn_MatIdentity_t)(void* retbuf);                        // ?Identity@Matrix@Math@@SA?AV12@XZ (static, retbuf) (slot 0x140152928)
typedef void  (*fn_CtxSetTransform_t)(void* ctx, int slot, void* matrix);// ?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z (slot 0x1401525e0)
typedef bool  (*fn_CtxBeginVerts_t)(void* ctx, int count, int stride);  // ?BeginVertices@Context@Video@@QEAA_NHH@Z (slot 0x140152930)
typedef void  (*fn_CtxAddVertex_t)(void* ctx, const void* vtx);         // ?AddVertex@Context@Video@@QEAAXPEBX@Z (slot 0x140152948)
typedef void  (*fn_CtxEndVerts_t)(void* ctx, int primType);             // ?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z (slot 0x140152938)

static fn_V3Ctor3_t        s_v3ctor3     = nullptr;
static fn_MatSetup_t       s_matSetup    = nullptr;
static fn_MatIdentity_t    s_matIdentity = nullptr;
static fn_CtxSetTransform_t s_ctxSetXform = nullptr;
static fn_CtxBeginVerts_t  s_ctxBegin    = nullptr;
static fn_CtxAddVertex_t   s_ctxAddVtx   = nullptr;
static fn_CtxEndVerts_t    s_ctxEnd      = nullptr;

// The record array is owned/freed by sub_1380. Uses the proven engine_funcs
// allocator pair (fn_MemAlloc(singleton,size,align) + fn_MemFreeStatic(ptr)) — same
// heap the rest of the reimpl uses; the raw static-MemAlloc export doesn't resolve
// reliably in standalone.

// Overlay Z handling (standalone). DEFAULT = faithful: the grid conforms exactly to the
// collision terrain (verified: b10e0 == the game's own aa20 terrain ray), which renders
// correctly and is visible. On this map the placed building anchors at Z=0 (sub_da00
// forces pos.z=0) and sits ~0.06 below the rendered ground, so the faithful grid can read
// as "above the (sunk) model" — a model-Z / terrain-baseline issue, NOT an overlay bug.
//
// EXPERIMENTAL (opt-in) base-anchor: if BANISHED_TOOL_ZLIFT is SET, the grid is re-anchored
// to the footprint-centre (model base) plane + that lift (world units). This is a knob for
// a future session to reconcile the model-sink; it is OFF by default so the committed build
// renders the visible, correct terrain-conforming overlay.
static bool  OverlayAnchorOn() { static int on = -1; if (on < 0) { const char* e = std::getenv("BANISHED_TOOL_ZLIFT"); on = (e && e[0]) ? 1 : 0; } return on != 0; }
static float OverlayZLift()     { static float v = 1e30f; if (v > 1e29f) { const char* e = std::getenv("BANISHED_TOOL_ZLIFT"); v = (e && e[0]) ? (float)std::atof(e) : 0.0f; } return v; }

static bool s_init = false;
static void EnsureFns() {
    if (s_init) return;
    auto slot = [](uintptr_t a) -> void* {
        uintptr_t s = ResolveAddr(Module::Exe, a);
        return s ? *reinterpret_cast<void**>(s) : nullptr;
    };
    s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(slot(0x140152a30));
    if (!s_v3ctor3) s_v3ctor3 = reinterpret_cast<fn_V3Ctor3_t>(ResolveExport("??0Vector3@Math@@QEAA@MMM@Z"));
    s_matSetup = reinterpret_cast<fn_MatSetup_t>(slot(0x140152920));
    if (!s_matSetup) s_matSetup = reinterpret_cast<fn_MatSetup_t>(ResolveExport("?Setup@MaterialInstance@Graphics@@QEBAXAEAVContext@Video@@H@Z"));
    s_matIdentity = reinterpret_cast<fn_MatIdentity_t>(slot(0x140152928));
    if (!s_matIdentity) s_matIdentity = reinterpret_cast<fn_MatIdentity_t>(ResolveExport("?Identity@Matrix@Math@@SA?AV12@XZ"));
    s_ctxSetXform = reinterpret_cast<fn_CtxSetTransform_t>(slot(0x1401525e0));
    if (!s_ctxSetXform) s_ctxSetXform = reinterpret_cast<fn_CtxSetTransform_t>(ResolveExport("?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z"));
    s_ctxBegin = reinterpret_cast<fn_CtxBeginVerts_t>(slot(0x140152930));
    if (!s_ctxBegin) s_ctxBegin = reinterpret_cast<fn_CtxBeginVerts_t>(ResolveExport("?BeginVertices@Context@Video@@QEAA_NHH@Z"));
    s_ctxAddVtx = reinterpret_cast<fn_CtxAddVertex_t>(slot(0x140152948));
    if (!s_ctxAddVtx) s_ctxAddVtx = reinterpret_cast<fn_CtxAddVertex_t>(ResolveExport("?AddVertex@Context@Video@@QEAAXPEBX@Z"));
    s_ctxEnd = reinterpret_cast<fn_CtxEndVerts_t>(slot(0x140152938));
    if (!s_ctxEnd) s_ctxEnd = reinterpret_cast<fn_CtxEndVerts_t>(ResolveExport("?EndVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z"));
    s_init = true;
}

// f5d0 (0x14009f5d0, registered) — single-cell validity test.
typedef bool (*fn_f5d0_t)(int* grid, int col, int row, uint32_t code, float f5, float f6,
                          float f7, char c8, char c9, int64_t p10);
static fn_f5d0_t s_f5d0 = nullptr;
// 0f90 (0x1400b0f90, registered as void — only used by 08c0's dead first loop).
typedef float (*fn_0f90_t)(int64_t heightfield, float* posXY, void* extra);
static fn_0f90_t s_0f90 = nullptr;
// f3e0 (0x14009f3e0, registered) — grid->world.
typedef float* (*fn_f3e0_t)(int* grid, float* out, float* in, char flag, void* extra);
static fn_f3e0_t s_f3e0 = nullptr;

static uintptr_t GameWorld() {
    return reinterpret_cast<uintptr_t>(g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr);
}

} // namespace

// ============================================================
// 0x1400b10e0 — sample a tile's 6 terrain corners (18 floats)
// ============================================================
// Reads the two triangles (6 verts) of the terrain quad at grid (col,row) from the
// heightfield, choosing the even/odd triangulation by (col+row) parity. Each corner is
// (worldX, worldY, height) where worldX/Y are cell coords recentred on the map and
// height = (int16)heightmap[hdim*clamp(row) + clamp(col)] * (1/256).
//   param_1+0xc0 = grid width, +0xc4 = ?, +0xd0 = heightmap dimension (hdim),
//   +0xd8 = int16 heightmap data pointer.
// 1:1 port of the Ghidra decompile (clamp bound hdim is constant, so both branches use
// the same sampler; corner (posX,posY,col,row) tuples transcribed per branch).
extern "C" void PlayerTileRender_TerrainCorners_b10e0(int64_t param_1, int col, int row,
                                                      float* out /*18 floats*/) {
    EnsureFns();
    const float hScale = C_hScale();
    int base = (*reinterpret_cast<int*>(param_1 + 0xc0) - *reinterpret_cast<int*>(param_1 + 0xc4)) >> 1;
    int c0 = base + col;          // iVar11
    int r0 = base + row;          // iVar3
    int c1 = c0 + 1;              // iVar10
    int r1 = r0 + 1;              // iVar8
    float center = (float)(*reinterpret_cast<int*>(param_1 + 0xc0) >> 1);
    int hdim = *reinterpret_cast<int*>(param_1 + 0xd0);
    const int16_t* hmap = *reinterpret_cast<int16_t**>(param_1 + 0xd8);

    float pC0 = (float)c0 - center;   // fVar15 (world X at c0)
    float pR0 = (float)r0 - center;   // fVar13 (world Y at r0)
    float pC1 = (float)c1 - center;   // fVar14
    float pR1 = (float)r1 - center;   // fVar12

    auto sampleH = [&](int cc, int rr) -> float {
        if (cc < 0) cc = 0; else if (cc >= hdim) cc = hdim - 1;
        if (rr < 0) rr = 0; else if (rr >= hdim) rr = hdim - 1;
        return (float)(int)hmap[(int64_t)hdim * rr + cc] * hScale;
    };
    auto put = [&](int i, float x, float y, int cc, int rr) {
        float z = sampleH(cc, rr);
        if (s_v3ctor3) {
            float v[4];
            s_v3ctor3(v, x, y, z);
            out[i * 3 + 0] = v[0]; out[i * 3 + 1] = v[1]; out[i * 3 + 2] = v[2];
        } else {
            out[i * 3 + 0] = x; out[i * 3 + 1] = y; out[i * 3 + 2] = z;
        }
    };

    if (((c0 + r0) & 1) == 0) {
        // EVEN triangulation
        put(0, pC0, pR0, c0, r0);
        put(1, pC0, pR1, c0, r1);
        put(2, pC1, pR0, c1, r0);
        put(3, pC1, pR0, c1, r0);
        put(4, pC0, pR1, c0, r1);
        put(5, pC1, pR1, c1, r1);
    } else {
        // ODD triangulation
        put(0, pC0, pR0, c0, r0);
        put(1, pC1, pR1, c1, r1);
        put(2, pC1, pR0, c1, r0);
        put(3, pC0, pR0, c0, r0);
        put(4, pC0, pR1, c0, r1);
        put(5, pC1, pR1, c1, r1);
    }
}

// ============================================================
// 0x1401008c0 — Video::Context terrain overlay render
// ============================================================
// Loads the "Terrain/Selection.rsc" MaterialInstance, sets an identity transform, opens
// a 0x18-byte-stride vertex stream, and for each tile record emits 6 verts:
//   position = terrain corner (b10e0), uv = a per-vertex atlas template rotated by the
//   record angle and offset by the record's texture-variant column, color = record RGBA.
// param_3 = the placement tile records (5 ints each = col,row,variant,color,angle).
// param_4 = an optional road-segment record set — ALWAYS EMPTY on the building-placement
// path (sub_1380 passes {0,0}); its first loop is faithfully ported but dead here.
// 1:1 port of the Ghidra decompile + disassembly (vertex layout pinned against
// 0x140100fc0 / 0x140101130).
extern "C" void PlayerTileRender_DrawOverlay_08c0(uint64_t /*param_1*/, void* ctx,
                                                  int64_t* param_3, int64_t* param_4) {
    EnsureFns();
    const float half = C_half();
    const float uvScale = C_uvScale();     // fVar3 = 0.125 (XMM13)
    const float deg2rad = C_deg2rad();

    uintptr_t gw = GameWorld();
    int64_t heightfield = *reinterpret_cast<int64_t*>(gw + 0x110);   // local_1e8
    int* grid = *reinterpret_cast<int**>(gw + 0xf8);                 // local_1e0

    if (!s_f3e0) s_f3e0 = reinterpret_cast<fn_f3e0_t>(ResolveAddr(Module::Exe, 0x14009f3e0));
    if (!s_0f90) s_0f90 = reinterpret_cast<fn_0f90_t>(ResolveAddr(Module::Exe, 0x1400b0f90));

    // --- FindOrLoad "Terrain/Selection.rsc" -> MaterialInstance, validated against
    //     the BaseTypeInfo* at .rdata slot 0x140151350 (Graphics::MaterialInstance). ---
    void* material = nullptr;
    {
        uint8_t fname[32];   // System::Filename is 8-16 bytes; oversized for safety.
        std::memset(fname, 0, sizeof(fname));
        if (fn_Filename_CtorCStr) fn_Filename_CtorCStr(fname, "Terrain/Selection.rsc");
        void* factory = (g_pExternalFactorySingleton && *g_pExternalFactorySingleton)
                            ? *g_pExternalFactorySingleton : nullptr;
        void* obj = (factory && fn_ExternalFactory_FindOrLoad && fn_Filename_CtorCStr)
                        ? fn_ExternalFactory_FindOrLoad(factory, fname) : nullptr;
        // Exe: material=obj unless IsDerivedFrom(MaterialInstance) is false. In standalone
        // the type-id slot 0x140151350 may be unfilled → can't verify → TRUST the load
        // (IsDerivedFromOrTrust pattern) rather than dropping a valid material.
        material = obj;
        if (obj && fn_IsDerivedFrom && g_ppTypeInfo_MaterialInstance && *g_ppTypeInfo_MaterialInstance) {
            if (!fn_IsDerivedFrom(obj, **g_ppTypeInfo_MaterialInstance)) material = nullptr;
        }
        if (fn_Filename_Dtor) fn_Filename_Dtor(fname);
    }

    if (material && s_matSetup) s_matSetup(material, ctx, 0);
    if (s_matIdentity && s_ctxSetXform) {
        uint8_t matrix[64];
        std::memset(matrix, 0, sizeof(matrix));
        s_matIdentity(matrix);
        s_ctxSetXform(ctx, 0, matrix);
    }

    // Guard: never iterate a record set whose base pointer is null (a failed alloc in
    // the builder would otherwise leave count>0 with a null base -> null-record deref).
    int count3 = (param_3 && param_3[0]) ? (int)param_3[1] : 0;
    int count4 = (param_4 && param_4[0]) ? (int)param_4[1] : 0;
    { static int s_on = -1;
      if (s_on < 0) { const char* e = std::getenv("BANISHED_TOOL_DIAG"); s_on = (e && e[0] && e[0] != '0') ? 1 : 0; }
      if (s_on) { static int n = 0; if (n++ < 5)
        std::fprintf(stderr, "[overlay] ctx=%p count3=%d count4=%d material=%p begin=%p add=%p end=%p b10e0hf=%p\n",
                     ctx, count3, count4, material, (void*)s_ctxBegin, (void*)s_ctxAddVtx, (void*)s_ctxEnd,
                     (void*)heightfield), std::fflush(stderr); } }
    bool began = s_ctxBegin ? s_ctxBegin(ctx, (count3 + count4) * 6, 0x18) : false;
    if (!began) return;

    // Even/odd tile UV templates (6 (u,v) pairs each), verbatim from the decompile.
    static const float TMPL_ODD[12]  = {0,0, 1,1, 1,0, 0,0, 0,1, 1,1};   // local_2b0
    // local_1c8: verts 3,4 are (1,0),(0,1) — NOT (0,0),(1,1). Getting these wrong makes an
    // EVEN tile's 2nd triangle map to a degenerate UV (v4==v5==(1,1)) → the arrow texel
    // smears along the diagonal = the stray "|\/" sliver on even-parity entry tiles.
    static const float TMPL_EVEN[12] = {0,0, 0,1, 1,0, 1,0, 0,1, 1,1};   // local_1c8

    // --- Loop 1: road-segment records (param_4). DEAD on placement path (count4==0). ---
    for (int rIdx = 0; rIdx < count4; rIdx++) {
        const int32_t* rec = reinterpret_cast<const int32_t*>(*param_4 + (int64_t)rIdx * 0x20);
        float variantCol = (float)rec[2] * uvScale;                 // local_364
        float angle = (float)rec[4] * deg2rad;                      // rec+0x10
        float sinA = std::sin(angle);   // FUN_140131c90 = sin (verified small-angle series ≈ x)
        float cosA = std::cos(angle);   // helper_1090 = cos (verified series ≈ 1 - x²/2)
        int w = grid[0];
        float rc = (float)(w >> 1) - half;
        // corner A (rec+0,+4)
        int idxA = rec[1] * w + rec[0];
        float inA[3] = { (float)(idxA % w) - rc, (float)(idxA / w) - rc, 0.0f };
        float wA[3] = {0,0,0}; if (s_f3e0) { float* r = s_f3e0(grid, wA, inA, 0, nullptr); wA[0]=r[0]; wA[1]=r[1]; }
        float pA3[3] = { wA[0], wA[1], 50.0f };
        float hA = s_0f90 ? s_0f90(heightfield, pA3, nullptr) : 0.0f;
        float A[3] = { wA[0], wA[1], hA };
        // corner B (rec+0x14,+0x18)
        int idxB = rec[6] * w + rec[5];
        float inB[3] = { (float)(idxB % w) - rc, (float)(idxB / w) - rc, 0.0f };
        float wB[3] = {0,0,0}; if (s_f3e0) { float* r = s_f3e0(grid, wB, inB, 0, nullptr); wB[0]=r[0]; wB[1]=r[1]; }
        float pB3[3] = { wB[0], wB[1], 50.0f };
        float hB = s_0f90 ? s_0f90(heightfield, pB3, nullptr) : 0.0f;
        float B[3] = { wB[0], wB[1], hB };
        // dir = normalize(B - A); perp offset = (-dir.y, dir.x, 0) * width (rec+0x1c)
        float dir[3] = { B[0]-A[0], B[1]-A[1], B[2]-A[2] };
        { float len = std::sqrt(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
          if (len > 1e-6f) { dir[0]/=len; dir[1]/=len; dir[2]/=len; } }
        float wdt; std::memcpy(&wdt, &rec[7], 4);                    // rec+0x1c (float)
        float off[3] = { -dir[1]*wdt, dir[0]*wdt, 0.0f };
        // 6 quad corners: A+off, B-off, A-off, A+off, B+off, B-off
        float quad[6][3] = {
            { A[0]+off[0], A[1]+off[1], A[2]+off[2] },
            { B[0]-off[0], B[1]-off[1], B[2]-off[2] },
            { A[0]-off[0], A[1]-off[1], A[2]-off[2] },
            { A[0]+off[0], A[1]+off[1], A[2]+off[2] },
            { B[0]+off[0], B[1]+off[1], B[2]+off[2] },
            { B[0]-off[0], B[1]-off[1], B[2]-off[2] },
        };
        for (int k = 0; k < 6; k++) {
            float ut = TMPL_ODD[k * 2];
            float vt = TMPL_ODD[k * 2 + 1];
            float vtx[6];
            vtx[0] = quad[k][0]; vtx[1] = quad[k][1]; vtx[2] = quad[k][2];
            vtx[3] = ((vt - half) * sinA + (ut - half) * cosA + half) * uvScale + variantCol;
            vtx[4] = ((vt - half) * cosA - (ut - half) * sinA) + half;
            std::memcpy(&vtx[5], &rec[3], 4);                        // color = rec+0xc
            if (s_ctxAddVtx) s_ctxAddVtx(ctx, vtx);
        }
    }

    // Standalone-only base anchor. The overlay faithfully conforms to the collision
    // heightmap (verified: b10e0 == the game's own aa20 terrain ray), but the placed
    // building anchors at Z=0 (sub_da00 forces pos.z=0) and the standalone's rendered
    // ground sits at ~0 on flat terrain while the collision heightmap carries a small
    // non-zero baseline — so a pure terrain-conforming grid floats that baseline above
    // the model. Anchor the grid to the footprint-centre terrain height so it sits flush
    // with the model base, KEEPING per-cell slope (far tiles still follow real relief).
    // The original skips this (byte-faithful — there the two heights agree).
    float baselineZ = 0.0f;
    if (OverlayAnchorOn() && count3 > 0) {
        const int32_t* mrec = reinterpret_cast<const int32_t*>(*param_3 + (int64_t)(count3 / 2) * 0x14);
        float mc[18];
        PlayerTileRender_TerrainCorners_b10e0(heightfield, mrec[0], mrec[1], mc);
        baselineZ = mc[2];
    }

    // --- Loop 2: placement tile records (param_3). THE LIVE overlay path. ---
    for (int rIdx = 0; rIdx < count3; rIdx++) {
        const int32_t* rec = reinterpret_cast<const int32_t*>(*param_3 + (int64_t)rIdx * 0x14);
        float corners[18];
        PlayerTileRender_TerrainCorners_b10e0(heightfield, rec[0], rec[1], corners);
        const float* tmpl = (((rec[0] + rec[1]) & 1) == 0) ? TMPL_EVEN : TMPL_ODD;
        float variantCol = (float)rec[2] * uvScale;                 // XMM8
        float angle = (float)rec[4] * deg2rad;                      // rec+0x10 * deg2rad
        float sinA = std::sin(angle);   // FUN_140131c90 = sin (was mislabeled cos → arrows misrotated)
        float cosA = std::cos(angle);   // helper_1090 = cos
        for (int k = 0; k < 6; k++) {
            float ut = tmpl[k * 2];
            float vt = tmpl[k * 2 + 1];
            float vtx[6];
            vtx[0] = corners[k * 3 + 0];
            vtx[1] = corners[k * 3 + 1];
            vtx[2] = corners[k * 3 + 2];
            if (OverlayAnchorOn()) vtx[2] += OverlayZLift() - baselineZ;  // opt-in base-anchor (see helper)
            vtx[3] = ((vt - half) * sinA + (ut - half) * cosA + half) * uvScale + variantCol;
            vtx[4] = ((vt - half) * cosA - (ut - half) * sinA) + half;
            std::memcpy(&vtx[5], &rec[3], 4);                        // color = rec+0xc
            if (s_ctxAddVtx) s_ctxAddVtx(ctx, vtx);
        }
    }

    if (s_ctxEnd) s_ctxEnd(ctx, 0);
}

// ============================================================
// 0x140101380 — build the per-cell tile-record array + draw
// ============================================================
// Walks the (param_3..param_5)/(param_4..param_6) footprint rectangle, and for each cell:
//   - computes a rotation-remapped index into two char maps (param_15 road map,
//     param_9 code map indexed by param_8),
//   - runs f5d0 to test validity for that cell,
//   - picks the tile color: green (0xff00ff00) if valid, red (0xffff0000) if invalid or
//     forceRed (param_13), yellow (0xffffff00) if isExisting (param_18),
//   - picks the texture-variant (0/1/2) from the road flag + param_17 sign,
//   - appends a 5-int record (col,row,variant,color,angle=param_16+param_17).
// Then hands the record array to 08c0 and frees it.
// 1:1 port of the Ghidra decompile (0x140101380); the rotation index switch (0x5a/0xb4/
// 0x10e via param_16) and the growth loop (×3 realloc) are reproduced exactly.
extern "C" void PlayerTileRender_BuildRecords_1380(
        uint64_t param_1, void* ctx, int param_3, int param_4, uint32_t param_5, int param_6,
        uint32_t param_7, int64_t* param_8, int64_t* param_9, float param_10, float param_11,
        float param_12, char param_13, char param_14, int64_t* param_15, int param_16,
        int param_17, char param_18, int64_t param_19) {
    EnsureFns();
    (void)param_1;   // 08c0's param_1 (unused; the exe reads the grid from the singleton)
    if (!s_f5d0) s_f5d0 = reinterpret_cast<fn_f5d0_t>(ResolveAddr(Module::Exe, 0x14009f5d0));

    int* grid = *reinterpret_cast<int**>(GameWorld() + 0xf8);   // local_b8

    // Growing record array (0x14-byte / 5-int records), owned+freed here.
    int32_t* arr = nullptr;      // pvVar6 / local_c8
    int32_t  count = 0;          // element count (low32 of local_c0)
    int32_t  cap = 0;            // pvVar11 (capacity)

    int rowEnd = param_6;        // last row (param_4..param_6 inclusive)
    int colEnd = (int)param_5;   // last col (param_3..param_5)
    int iVar21 = ((int)param_5 - param_3) + 1;   // width  (for rotation remap)
    int iVar13 = (param_6 - param_4) + 1;        // height

    if (param_4 <= rowEnd) {
        int iVar12 = param_6 - param_4;          // (height-1) countdown seed
        for (int drow = param_4; drow <= rowEnd; drow++, iVar12--) {
            if (param_3 > colEnd) continue;
            int angleSum = param_16 + param_17;  // local_a8
            int iVar20 = (int)param_5 - param_3; // col countdown seed
            for (int dcol = param_3; dcol <= colEnd; dcol++, iVar20--) {
                bool isRoad = false;
                // --- rotation-remapped flat index into the char maps ---
                // Four cases from the decompile's short-circuit switch (param_16 = the
                // orientation code 0x5a=90 / 0xb4=180 / 0x10e=270 / else=identity):
                //   iVar15 = column term, iVar5 = row term, (iVar14,iVar7) = strides.
                //   flatIdx = (iVar14 + 1) * ((iVar7 - iVar5) - 1) + iVar15.
                int relRow = drow - param_4;     // local_a4 + local_d8
                int relCol = dcol - param_3;     // local_a0 + iVar24
                int colCd  = iVar20;             // colEnd - dcol (col countdown)
                int rowCd  = iVar12;             // param_6 - drow (row countdown)
                int iVar15, iVar5, iVar7, iVar14;
                if (param_16 == 0x5a) {          // 90°
                    iVar15 = relRow; iVar5 = colCd;  iVar7 = iVar21; iVar14 = iVar13;
                } else if (param_16 == 0xb4) {   // 180°
                    iVar15 = colCd;  iVar5 = rowCd;  iVar7 = iVar13; iVar14 = iVar21;
                } else if (param_16 == 0x10e) {  // 270°
                    iVar15 = rowCd;  iVar5 = relCol; iVar7 = iVar21; iVar14 = iVar13;
                } else {                          // identity
                    iVar15 = relCol; iVar5 = relRow; iVar7 = iVar13; iVar14 = iVar21;
                }
                int flatIdx = (iVar14 + 1) * ((iVar7 - iVar5) + -1) + iVar15;

                int64_t roadMap = *param_15;
                if (roadMap != 0 && *reinterpret_cast<int*>(roadMap - 8) != 0) {
                    isRoad = *reinterpret_cast<char*>(flatIdx + roadMap) == '.';
                }
                uint32_t code = param_7;
                if ((int)param_8[1] > 0) {
                    int64_t codeMap = *param_9;
                    if (codeMap != 0 && *reinterpret_cast<int*>(codeMap - 8) != 0) {
                        code = *reinterpret_cast<uint32_t*>(
                            *param_8 - 0xc0 + (int64_t)(*reinterpret_cast<char*>(flatIdx + codeMap)) * 4);
                    }
                }

                bool ok = s_f5d0 && s_f5d0(grid, dcol, drow, code, param_10, param_11, param_12,
                                           param_14, isRoad ? 1 : 0, param_19);

                uint32_t color;
                if (param_13 != '\0' || !ok) color = 0xffff0000u;   // red
                else                          color = 0xff00ff00u;   // green
                if (param_18 != '\0')         color = 0xffffff00u;   // yellow (existing)

                int variant = 0;
                if (isRoad) variant = ((-1 < param_17) ? 1 : 0) + 1;

                // --- append the 5-int record (grow ×3 like the original) ---
                int32_t newCount = count + 1;
                if (newCount > cap) {
                    int32_t newCap = (count != 0) ? newCount * 3 : newCount;
                    int32_t* nb = reinterpret_cast<int32_t*>(
                        fn_MemAlloc ? fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 0x14, 4) : nullptr);
                    if (nb && arr) {
                        for (int i = 0; i < count; i++) {
                            nb[i*5+0]=arr[i*5+0]; nb[i*5+1]=arr[i*5+1]; nb[i*5+2]=arr[i*5+2];
                            nb[i*5+3]=arr[i*5+3]; nb[i*5+4]=arr[i*5+4];
                        }
                    }
                    if (arr && arr != nb) fn_MemFreeStatic(arr);
                    arr = nb;
                    cap = newCap;
                }
                // Only advance count if the record actually got stored — otherwise we'd
                // hand 08c0 a null base with count>0 (null-record deref crash).
                if (arr) {
                    int32_t* rec = arr + (int64_t)count * 5;
                    rec[0] = dcol; rec[1] = drow; rec[2] = variant;
                    std::memcpy(&rec[3], &color, 4); rec[4] = angleSum;
                    count = newCount;
                }
            }
        }
    }

    // Hand the records to 08c0 (param_4 = empty), then free.
    int64_t empty[2] = {0, 0};
    int64_t p3[2]; p3[0] = reinterpret_cast<int64_t>(arr); p3[1] = count;
    PlayerTileRender_DrawOverlay_08c0(param_5, ctx, p3, empty);
    if (arr) fn_MemFreeStatic(arr);
}
