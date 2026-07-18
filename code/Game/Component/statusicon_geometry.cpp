// code/Game/Component/statusicon_geometry.cpp
// Phase B — standalone billboard Geometry draw path for StatusIconComponent.
//
// The StatusIconComponent embeds a Graphics::Geometry sub-object at component+0x28
// whose vtable in the original exe is the billboard subclass PTR_FUN_14015f0c0 (12
// slots). The two draw slots live in the (unmapped-in-standalone) exe:
//   slot 2 (+0x10) Draw(geometry, Context*, camera, ?, lerpT)   = 0x1400a9c10
//   slot 3 (+0x18) AddDrawItems(geometry, scene, renderer)      = 0x1400a9b90
// This file provides faithful C++ ports of both, plus a standalone vtable that COPIES
// the real Runtime base-Geometry vtable (so dtor/bounds/type getters in slots 0,1,4-11
// keep working for scene culling) and overrides only slots 2 and 3.
//
// Everything here is gated by the caller (statusiconcomponent.cpp, env
// BANISHED_LIVE_STATUSICON_DRAW); default builds never construct this vtable.
//
// Ported faithfully from Application-x64.exe (Ghidra), verified against the disassembly.

#include "resolve.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---- .rdata float constants (NULL via ResolveAddr in standalone → use literals) ----
//   g_fUVScale       0x14017214c = 1.0f
//   _DAT_1401720c4   0x1401720c4 = 0.01f
//   DAT_140172114    0x140172114 = 0.5f
static const float kUVScale   = 1.0f;
static const float kScale01   = 0.01f;
static const float kHalf      = 0.5f;

// ---- Resolved Runtime engine functions ----
typedef void* (*fn_Lerp_t)(void* self, void* retbuf, const float* t);              // InterpolatedState<Transform>::Lerp
typedef void* (*fn_TransformAsMatrix_t)(void* self, void* retbuf);                 // Transform::AsMatrix
typedef void  (*fn_CtxSetTransform_t)(void* ctx, int slot, const void* matrix);    // Context::SetTransform
typedef void  (*fn_CtxSetColor_t)(void* ctx, const void* v4);                      // Context::SetColor
typedef bool  (*fn_CtxBeginIV_t)(void* ctx, int vcount, int stride, int icount);   // Context::BeginIndexedVertices
typedef void  (*fn_CtxAddVerts_t)(void* ctx, int count, const void* data);         // Context::AddVertices
typedef void  (*fn_CtxEndIV_t)(void* ctx, int primType);                           // Context::EndIndexedVertices
typedef void  (*fn_GetUVs_t)(void* sheet, int idx, float* u0, float* v0, float* u1, float* v1); // SpriteSheet::GetUVs
// cc!=0 camera-constant-screen-size projection helpers (all const members → hidden retbuf in RDX).
typedef void* (*fn_T3Inverse_t)(void* self, void* retbuf);                   // Transform3::Inverse
typedef void* (*fn_T3AsMatrix_t)(void* self, void* retbuf);                  // Transform3::AsMatrix
typedef void* (*fn_MatMulMat_t)(void* self, void* retbuf, const void* other);// Matrix::operator*(Matrix)
typedef void* (*fn_MatMulV4_t)(void* self, void* retbuf, const void* vec4);  // Matrix::operator*(Vector4)

static fn_Lerp_t              s_Lerp           = nullptr;
static fn_TransformAsMatrix_t s_AsMatrix       = nullptr;
static fn_CtxSetTransform_t   s_SetTransform   = nullptr;
static fn_CtxSetColor_t       s_SetColor       = nullptr;
static fn_CtxBeginIV_t        s_BeginIV        = nullptr;
static fn_CtxAddVerts_t       s_AddVerts       = nullptr;
static fn_CtxEndIV_t          s_EndIV          = nullptr;
static fn_GetUVs_t            s_GetUVs         = nullptr;
static fn_T3Inverse_t         s_T3Inverse      = nullptr;
static fn_T3AsMatrix_t        s_T3AsMatrix     = nullptr;
static fn_MatMulMat_t         s_MatMulMat      = nullptr;
static fn_MatMulV4_t          s_MatMulV4       = nullptr;
static bool                   s_resolved       = false;

static void EnsureDrawFuncs() {
    if (s_resolved) return;
    s_Lerp = reinterpret_cast<fn_Lerp_t>(ResolveExport(
        "?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z"));
    s_AsMatrix = reinterpret_cast<fn_TransformAsMatrix_t>(ResolveExport(
        "?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ"));
    s_SetTransform = reinterpret_cast<fn_CtxSetTransform_t>(ResolveExport(
        "?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z"));
    s_SetColor = reinterpret_cast<fn_CtxSetColor_t>(ResolveExport(
        "?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z"));
    s_BeginIV = reinterpret_cast<fn_CtxBeginIV_t>(ResolveExport(
        "?BeginIndexedVertices@Context@Video@@QEAA_NHHH@Z"));
    s_AddVerts = reinterpret_cast<fn_CtxAddVerts_t>(ResolveExport(
        "?AddVertices@Context@Video@@QEAAXHPEBX@Z"));
    s_EndIV = reinterpret_cast<fn_CtxEndIV_t>(ResolveExport(
        "?EndIndexedVertices@Context@Video@@QEAAXW4PrimitiveType@12@@Z"));
    s_GetUVs = reinterpret_cast<fn_GetUVs_t>(ResolveExport(
        "?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z"));
    s_T3Inverse = reinterpret_cast<fn_T3Inverse_t>(ResolveExport(
        "?Inverse@Transform3@Math@@QEBA?AV12@XZ"));
    s_T3AsMatrix = reinterpret_cast<fn_T3AsMatrix_t>(ResolveExport(
        "?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ"));
    s_MatMulMat = reinterpret_cast<fn_MatMulMat_t>(ResolveExport(
        "??DMatrix@Math@@QEBA?AV01@AEBV01@@Z"));
    s_MatMulV4 = reinterpret_cast<fn_MatMulV4_t>(ResolveExport(
        "??DMatrix@Math@@QEBA?AVVector4@1@AEBV21@@Z"));
    s_resolved = true;
}

static bool s_drawTrace = false;
static bool s_drawTraceInit = false;
static bool DrawTrace() {
    if (!s_drawTraceInit) {
        const char* e = getenv("BANISHED_STATUSICON_TRACE");
        s_drawTrace = e && e[0] && e[0] != '0';
        s_drawTraceInit = true;
    }
    return s_drawTrace;
}

// 32-byte billboard vertex: pos(12) + color(4) + uv(8) + pad(8). Layout matches the
// original's stack-built vertex (BeginIndexedVertices stride = 0x20).
struct SIVertex {
    float    x, y, z;
    uint32_t color;
    float    u, v;
    float    pad0, pad1;
};
static_assert(sizeof(SIVertex) == 32, "SIVertex must be 32 bytes");

// ============================================================
// slot 3 — AddDrawItems  (port of FUN_1400a9b90)
// ============================================================
// Registers the billboard with the renderer if it has any active sprite bits AND
// the game-settings icon-visibility byte (settings+0xc) is set.
static void StatusIconGeom_AddDrawItems(void* geometry, void* /*scene*/, void* renderer) {
    EnsureDrawFuncs();
    uint8_t* g = reinterpret_cast<uint8_t*>(geometry);

    int32_t activeMask = *reinterpret_cast<int32_t*>(g + 0xc8);
    if (activeMask == 0) return;

    void* settings = g_pGameSettingsPtr ? *g_pGameSettingsPtr : nullptr;
    if (!settings || reinterpret_cast<uint8_t*>(settings)[0xc] == 0) return;

    void* sheet = *reinterpret_cast<void**>(g + 0xd0);
    void* material = sheet ? *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(sheet) + 0x30)
                           : nullptr;
    if (fn_Renderer_AddDrawItem && material) {
        // AddDrawItem(renderer, geom, material, 0x8000, 0, 0.0, uvScale, 1, null, 0)
        fn_Renderer_AddDrawItem(renderer, geometry, material,
                                0x8000, 0, 0.0f, kUVScale, 1, nullptr, 0);
    }
}

// ============================================================
// slot 2 — Draw  (port of FUN_1400a9c10)
// ============================================================
// Emits a camera-facing billboard quad per active sprite, stacked vertically in the
// geometry's local frame. param order matches the original vtable Draw:
//   (geometry, Context*, camera, p4, lerpT)
static void StatusIconGeom_Draw(void* geometry, void* ctx, void* camera,
                                void* /*p4*/, float t) {
    EnsureDrawFuncs();
    uint8_t* g = reinterpret_cast<uint8_t*>(geometry);

    // 1) Model transform = Lerp(state, t) -> AsMatrix -> Context::SetTransform(0, M).
    //    Keep the Lerp'd Transform around: its position (+0x10) is the billboard origin
    //    used by the cc!=0 camera-constant-screen-size projection below.
    alignas(16) float xf[8] = {0};   // Math::Transform retbuf (0x1C bytes); pos at xf[4..6]
    bool haveXf = false;
    if (s_Lerp && s_AsMatrix && s_SetTransform) {
        alignas(16) float mat[16];    // Math::Matrix retbuf (64 bytes)
        s_Lerp(g + 0x84, xf, &t);
        haveXf = true;
        void* m = s_AsMatrix(xf, mat);
        s_SetTransform(ctx, 0, m ? m : mat);
    }

    // 2) Color = (1,1,1, lerp(geom+0x80, geom+0x7c, t)). geom+0x7c/+0x80 hold the
    //    settings-derived scale written by OnRemove (used here as the alpha).
    float alpha = (kUVScale - t) * *reinterpret_cast<float*>(g + 0x80)
                + t           * *reinterpret_cast<float*>(g + 0x7c);
    float color4[4] = { 1.0f, 1.0f, 1.0f, alpha };
    if (s_SetColor) s_SetColor(ctx, color4);

    // 3) Count active sprites; begin the indexed-vertex batch (n quads).
    int      spriteCount = *reinterpret_cast<int*>(g + 0xc4);
    uint32_t activeMask  = *reinterpret_cast<uint32_t*>(g + 0xc8);
    int n = 0;
    for (int i = 0; i < spriteCount && i < 32; ++i)
        if (activeMask & (1u << i)) ++n;

    if (!s_BeginIV || !s_BeginIV(ctx, n * 4, 0x20, n * 6)) {
        if (DrawTrace()) {
            static int once = 0;
            if (once++ < 3)
                printf("[StatusIcon::Draw] BeginIndexedVertices FAILED/NULL n=%d (geom=%p)\n",
                       n, geometry);
        }
        return;
    }

    // 4) Camera basis (camera Transform3 at camera+0x64): row0 = right, row2 = up.
    float* cam   = reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(camera) + 0x64);
    float rightU[3] = { cam[0], cam[1], cam[2] };   // row0
    float upU[3]    = { cam[6], cam[7], cam[8] };   // row2

    // 5) Scale. fVar9 = iconSize. When cc!=0 the icon keeps a CONSTANT SCREEN SIZE:
    //    fVar9 /= |ndcB.x - ndcA.x|, where ndcA/ndcB are the NDC-x of the icon origin P
    //    and P+cameraRight, projected through (inverse-view * projection). Faithful port
    //    of the FUN_1400a9c10 cc!=0 block (AsVector3/Vector ctors inlined).
    float    fVar9 = *reinterpret_cast<float*>(g + 0xc0);   // iconSize
    uint8_t  cc    = *reinterpret_cast<uint8_t*>(g + 0xcc);
    if (cc != 0 && haveXf && s_T3Inverse && s_T3AsMatrix && s_MatMulMat && s_MatMulV4) {
        alignas(16) uint8_t invBuf[256];    // Transform3 retbuf (Inverse)
        alignas(16) uint8_t invMat[128];    // Matrix retbuf (AsMatrix)
        alignas(16) uint8_t vpMat[128];     // Matrix retbuf (invView * proj)
        const void* proj = reinterpret_cast<uint8_t*>(camera) + 0x94;   // 4x4 projection
        s_T3Inverse(reinterpret_cast<uint8_t*>(camera) + 0x64, invBuf);  // invView (Transform3)
        s_T3AsMatrix(invBuf, invMat);                                    // invView as Matrix
        s_MatMulMat(invMat, vpMat, proj);                                // vp = invView * proj

        float P[3] = { xf[4], xf[5], xf[6] };                            // billboard origin (Transform pos)
        alignas(16) float v4a[4] = { P[0],            P[1],            P[2],            1.0f };
        alignas(16) float v4b[4] = { P[0] + rightU[0], P[1] + rightU[1], P[2] + rightU[2], 1.0f };
        alignas(16) float clipA[4] = {0}, clipB[4] = {0};
        s_MatMulV4(vpMat, clipB, v4b);
        s_MatMulV4(vpMat, clipA, v4a);
        float wa = clipA[3], wb = clipB[3];
        if (wa != 0.0f && wb != 0.0f) {
            float d = (clipB[0] / wb) - (clipA[0] / wa);
            float ad = d < 0.0f ? -d : d;
            if (ad > 1e-9f) fVar9 = fVar9 / ad;
        }
    }

    void*   settings  = g_pGameSettingsPtr ? *g_pGameSettingsPtr : nullptr;
    int32_t scaleSet  = settings ? reinterpret_cast<int32_t*>(settings)[4] : 0; // settings+0x10
    fVar9 = fVar9 * static_cast<float>(scaleSet) * kScale01;
    float halfsize = fVar9 * kHalf;

    float right[3]   = { rightU[0] * halfsize, rightU[1] * halfsize, rightU[2] * halfsize };
    float up[3]      = { upU[0]    * halfsize, upU[1]    * halfsize, upU[2]    * halfsize };
    float advance[3] = { upU[0]    * fVar9,    upU[1]    * fVar9,    upU[2]    * fVar9 };
    float center[3]  = { 0.0f, 0.0f, halfsize };

    void* sheet = *reinterpret_cast<void**>(g + 0xd0);

    // index pointer lives at ctx+0x20f0 (a short* that advances as we emit indices).
    int   emitted = 0;
    short base    = 0;
    for (int i = 0; i < spriteCount && i < 32; ++i) {
        if (!((activeMask >> i) & 1u)) continue;

        float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
        if (s_GetUVs && sheet) s_GetUVs(sheet, i, &u0, &v0, &u1, &v1);

        SIVertex vtx[4];
        std::memset(vtx, 0, sizeof(vtx));
        // v0 = center - right + up   uv(u0,v0)
        vtx[0].x = center[0] - right[0] + up[0];
        vtx[0].y = center[1] - right[1] + up[1];
        vtx[0].z = center[2] - right[2] + up[2];
        vtx[0].color = 0xffffffffu; vtx[0].u = u0; vtx[0].v = v0;
        // v1 = center + right + up   uv(u1,v0)
        vtx[1].x = center[0] + right[0] + up[0];
        vtx[1].y = center[1] + right[1] + up[1];
        vtx[1].z = center[2] + right[2] + up[2];
        vtx[1].color = 0xffffffffu; vtx[1].u = u1; vtx[1].v = v0;
        // v2 = center + right - up   uv(u1,v1)
        vtx[2].x = center[0] + right[0] - up[0];
        vtx[2].y = center[1] + right[1] - up[1];
        vtx[2].z = center[2] + right[2] - up[2];
        vtx[2].color = 0xffffffffu; vtx[2].u = u1; vtx[2].v = v1;
        // v3 = center - right - up   uv(u0,v1)
        vtx[3].x = center[0] - right[0] - up[0];
        vtx[3].y = center[1] - right[1] - up[1];
        vtx[3].z = center[2] - right[2] - up[2];
        vtx[3].color = 0xffffffffu; vtx[3].u = u0; vtx[3].v = v1;

        if (s_AddVerts) s_AddVerts(ctx, 4, vtx);

        // 6 indices: base, base+1, base+2, base, base+2, base+3.
        short* ip = *reinterpret_cast<short**>(reinterpret_cast<uint8_t*>(ctx) + 0x20f0);
        ip[0] = base;
        ip[1] = static_cast<short>(base + 1);
        ip[2] = static_cast<short>(base + 2);
        ip[3] = base;
        ip[4] = static_cast<short>(base + 2);
        ip[5] = static_cast<short>(base + 3);
        *reinterpret_cast<short**>(reinterpret_cast<uint8_t*>(ctx) + 0x20f0) = ip + 6;

        base = static_cast<short>(base + 4);
        center[0] += advance[0];
        center[1] += advance[1];
        center[2] += advance[2];
        ++emitted;
    }

    if (s_EndIV) s_EndIV(ctx, 0);

    if (DrawTrace()) {
        static int once = 0;
        if (once++ < 3)
            printf("[StatusIcon::Draw] geom=%p quads=%d halfsize=%.3f alpha=%.3f scaleSet=%d sheet=%p\n",
                   geometry, emitted, halfsize, alpha, scaleSet, sheet);
        fflush(stdout);
    }
}

// ============================================================
// Standalone billboard vtable
// ============================================================
// Built by copying the real Runtime base-Geometry vtable (set by ??0Geometry@Graphics
// on the embedded sub-object) and overriding slots 2 (Draw) and 3 (AddDrawItems).
// Base slots (dtor, bounds/type getters) stay valid so scene culling works.
static void* s_siGeomVtable[16];
static bool  s_siGeomVtableBuilt = false;

static void* SIGeom_StubRet0() { return nullptr; }

void StatusIconGeometry_BuildVtable(void** baseVtbl) {
    if (s_siGeomVtableBuilt) return;
    for (int i = 0; i < 16; ++i) {
        // The billboard vtable has 12 real slots; copy what the base provides (slot 9
        // is null in the original) and stub anything we cannot source.
        if (baseVtbl && i < 12 && baseVtbl[i])
            s_siGeomVtable[i] = baseVtbl[i];
        else
            s_siGeomVtable[i] = reinterpret_cast<void*>(&SIGeom_StubRet0);
    }
    s_siGeomVtable[2] = reinterpret_cast<void*>(&StatusIconGeom_Draw);
    s_siGeomVtable[3] = reinterpret_cast<void*>(&StatusIconGeom_AddDrawItems);
    s_siGeomVtableBuilt = true;
}

void** StatusIconGeometry_Vtable() {
    return s_siGeomVtable;
}

bool StatusIconGeometry_VtableBuilt() {
    return s_siGeomVtableBuilt;
}

