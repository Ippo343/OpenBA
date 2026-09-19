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

// video/Context.h FIRST: it pulls runtime/types/volume/Volume.h, whose
// global-scope `struct Rectangle` is hidden by <wingdi.h>'s `Rectangle()` once
// <windows.h> has been seen through resolve.h.  ENDGAME B.3.5 caller batch I2.
// NOTE: the Geometry SUBCLASS in this file is an EXE-PHASE class (it names
// Geometry's retail vtable 0x180154e60 and hand-builds its own); it is NOT
// converted. Only the calls it makes INTO Video::Context members are.
#include "video/Context.h"
#include "resolve.h"
#include "engine_funcs.h"
#include "math/Math.h"
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
typedef void  (*fn_GetUVs_t)(void* sheet, int idx, float* u0, float* v0, float* u1, float* v1); // SpriteSheet::GetUVs
// cc!=0 camera-constant-screen-size projection uses the Math:: classes directly
// (Transform3::Inverse / AsMatrix, Matrix::operator*(Matrix|Vector4)).

static fn_Lerp_t              s_Lerp           = nullptr;
static fn_GetUVs_t            s_GetUVs         = nullptr;
static bool                   s_resolved       = false;

static void EnsureDrawFuncs() {
    if (s_resolved) return;
    s_Lerp = reinterpret_cast<fn_Lerp_t>(ResolveExport(
        "?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?AVTransform@Math@@AEBVScalar@4@@Z"));
    s_GetUVs = reinterpret_cast<fn_GetUVs_t>(ResolveExport(
        "?GetUVs@SpriteSheet@UI@@QEBAXHAEAM000@Z"));
    s_resolved = true;
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
    Math::Transform xf;              // Lerp retbuf; .translation is the billboard origin
    bool haveXf = false;
    if (s_Lerp) {
        s_Lerp(g + 0x84, &xf, &t);
        haveXf = true;
        Math::Matrix mat = xf.AsMatrix();
        reinterpret_cast<Video::Context*>(ctx)->SetTransform(0, mat);   // B.3.5 bridge
    }

    // 2) Color = (1,1,1, lerp(geom+0x80, geom+0x7c, t)). geom+0x7c/+0x80 hold the
    //    settings-derived scale written by OnRemove (used here as the alpha).
    float alpha = (kUVScale - t) * *reinterpret_cast<float*>(g + 0x80)
                + t           * *reinterpret_cast<float*>(g + 0x7c);
    float color4[4] = { 1.0f, 1.0f, 1.0f, alpha };
    reinterpret_cast<Video::Context*>(ctx)
        ->SetColor(*reinterpret_cast<const Math::Vector4*>(color4));   // B.3.5 bridge

    // 3) Count active sprites; begin the indexed-vertex batch (n quads).
    int      spriteCount = *reinterpret_cast<int*>(g + 0xc4);
    uint32_t activeMask  = *reinterpret_cast<uint32_t*>(g + 0xc8);
    int n = 0;
    for (int i = 0; i < spriteCount && i < 32; ++i)
        if (activeMask & (1u << i)) ++n;

    if (!reinterpret_cast<Video::Context*>(ctx)
             ->BeginIndexedVertices(n * 4, 0x20, n * 6)) {   // B.3.5 bridge
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
    if (cc != 0 && haveXf) {
        const Math::Matrix& proj =
            *reinterpret_cast<const Math::Matrix*>(reinterpret_cast<uint8_t*>(camera) + 0x94);   // B.3.1 bridge: 4x4 projection
        Math::Transform3 invView =
            reinterpret_cast<const Math::Transform3*>(reinterpret_cast<uint8_t*>(camera) + 0x64)->Inverse();   // B.3.1 bridge
        Math::Matrix invMat = invView.AsMatrix();                        // invView as Matrix
        Math::Matrix vpMat  = invMat * proj;                             // vp = invView * proj

        float P[3] = { xf.translation.x, xf.translation.y, xf.translation.z };   // billboard origin (Transform pos)
        Math::Vector4 v4a(P[0],             P[1],             P[2],             1.0f);
        Math::Vector4 v4b(P[0] + rightU[0], P[1] + rightU[1], P[2] + rightU[2], 1.0f);
        Math::Vector4 clipB = vpMat * v4b;
        Math::Vector4 clipA = vpMat * v4a;
        float wa = clipA.w, wb = clipB.w;
        if (wa != 0.0f && wb != 0.0f) {
            float d = (clipB.x / wb) - (clipA.x / wa);
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

        reinterpret_cast<Video::Context*>(ctx)->AddVertices(4, vtx);   // B.3.5 bridge

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

    reinterpret_cast<Video::Context*>(ctx)->EndIndexedVertices(0);   // B.3.5 bridge


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
        // FIXED by Wave 22 lane GEOM, discharging the addressee tag on
        // ??_7Geometry@Graphics@@6B@ in standalone/reg_mint_promoted.cpp.
        // The `12` here was wrong; the true extent is NINE on both sides,
        // RE-MEASURED THIS LANE (not just re-quoted from Wave 21 lane GFXVT /
        // WAVE21_LANE_GFXVT.md §5.2a) by reading the raw bytes of both shipped
        // binaries directly with pefile:
        //
        //   SOURCE      ??_7Geometry@Graphics@@6B@, Runtime-x64.dll, image
        //               VA 0x180154e60: qwords[0..8] are code pointers into
        //               .text (0x18001____-0x1800a____ range); qword[9] =
        //               0x6675426567616d49 = ASCII "ImageBuf" (of
        //               "ImageBuffer\0Generated..."), confirming slots 9-11
        //               are STRING BYTES, not function pointers, and all are
        //               non-zero so the old `baseVtbl[i]` truthiness test
        //               would have installed three fragments of a C string as
        //               callable slots.
        //   DESTINATION retail's own billboard vtable, Application-x64.exe,
        //               image VA 0x14015f0c0: qwords[0..8] are code pointers
        //               into .text; qword[9] == 0 (a genuine NULL, not a
        //               string byte); qwords[10..18] are a DIFFERENT class's
        //               9-slot table (confirmed by xrefs: 0x14015f0c0 is
        //               stamped by StatusIconComponent_Ctor at 0x14012c0fe,
        //               0x14015f110 by TradeWork_impl_ed50 and
        //               FUN_1400b00e0), then qwords[19..20] are ASCII
        //               ("Terrain..." reversed).
        //
        // So the destination's slot 9 is the TERMINATOR BETWEEN TWO TABLES,
        // not a null slot of the Geometry class itself, and "12 real slots"
        // has no basis in either binary — the fix is `i < 9`.
        //
        // Both classes present at the destination address (billboard and
        // whatever owns the 0x14015f110 table) have exactly nine virtuals, so
        // nothing in the shipped code can dispatch index 9, 10 or 11 through
        // this object's vtable pointer. THIS IS A DERIVED conclusion (from the
        // extents above, from xrefs on the stampers, and from ResolveAddr's
        // registry-first order at runtime/src/graphics/scene.cpp:176), NOT an
        // observed one: there is no attendance counter on
        // StatusIconGeometry_Vtable()'s call sites, so a live run has not
        // been used to confirm slots 9-11 are never invoked. A lane that
        // wants an observation, not a derivation, needs to add one.
        //
        // RESIDUAL (recorded by Wave 21, resolved here): should the
        // now-unreachable tail (i >= 9) hold retail's nullptr or the
        // SIGeom_StubRet0 stub? Slot 9 specifically mirrors a genuine retail
        // NULL (measured above), so this lane installs nullptr for i >= 9
        // rather than a callable stub nobody asked for -- a null pointer in
        // an unreachable slot fails LOUDLY if some future change ever did
        // reach it, matching retail's own exposure there, instead of quietly
        // absorbing the call the way the stub would. Slots 0-8 keep the stub
        // fallback for when baseVtbl itself is null/not yet built, because
        // those ARE real, potentially-dispatched Geometry virtuals (dtor,
        // bounds/type getters) and a null there would crash on ordinary
        // scene culling, not just on a hypothetical stray call.
        if (i < 9)
            s_siGeomVtable[i] = (baseVtbl && baseVtbl[i])
                                     ? baseVtbl[i]
                                     : reinterpret_cast<void*>(&SIGeom_StubRet0);
        else
            s_siGeomVtable[i] = nullptr;
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

