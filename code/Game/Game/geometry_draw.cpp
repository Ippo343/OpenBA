// code/Game/Game/geometry_draw.cpp
//
// Real implementations of the 9 vtable slots at exe 0x14015f110 ("derived
// Graphics::Geometry" / TerrainGenerator). Decompiled from the original exe.
//
// Vtable layout (verified via Ghidra read of 0x14015f110):
//   +0x00  GeomDraw_Dtor               (exe 0x14001ECC0)  — see below
//   +0x08  GeomDraw_ChunkUpdate        (exe 0x1400AED40 → JMP 0x1400AAD50)
//   +0x10  GeomDraw_ItemDraw           (exe 0x1400AEA10)  per-item GPU draw
//   +0x18  GeomDraw_CollectDraw        (exe 0x1400AE490)  frustum cull + AddDrawItem
//   +0x20  inherited UpdateAttached    (Runtime export)
//   +0x28  inherited GetSubTransform   (Runtime export, hidden retval)
//   +0x30  inherited GetInstanceTransform (Runtime export, hidden retval)
//   +0x38  inherited SetSharedInstanceData (Runtime export)
//   +0x40  inherited Clean             (Runtime export)
//
// The 9-slot vtable in serialize_arrays.cpp points at the functions defined here.

// ENDGAME B.3.5 batch K1: the runtime type headers come FIRST — they reach
// runtime/types/volume/Volume.h, whose global-scope `struct Rectangle` is hidden by
// <wingdi.h>'s `Rectangle()` once <windows.h> has been seen through resolve.h.
#include "graphics/Geometry.h"
#include "graphics/Camera.h"     // Graphics::Camera, Graphics::Renderer
#include "graphics/Material.h"   // Graphics::MaterialInstance
#include "graphics/InterpolatedState.h"   // ENDGAME B.3.5 closing pass --
                                          // Graphics::InterpolatedState<Transform>
#include "video/Context.h"
#include "engine_funcs.h"
#include "math/Math.h"
#include "resolve.h"
#include "perf_experimental.h"
#include <cstdint>
#include <new>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================
// Exe-resident constants (read once and embedded — these are float
// literals in the original exe .rdata, accessed via PC-relative loads).
// ============================================================
//
// 0x14017228c → bytes 00 00 80 c1 = -16.0f  (chunk Z-min)
// 0x1401721d0 → bytes 00 00 00 42 =  32.0f  (chunk Z-max delta)
//
// In standalone these addresses live in our own PE which doesn't carry the
// original exe's .rdata, so we embed the values as constexpr.
namespace {
constexpr float kChunkZMin   = -16.0f;  // exe 0x14017228c
constexpr float kChunkZRange =  32.0f;  // exe 0x1401721d0
}  // namespace

// ============================================================
// Slot +0x00 — destructor stub
// ============================================================
//
// Original dtor (exe 0x14001ECC0) walks all the SoA arrays at +0x140..+0x228,
// calls MemFree on each, then tail-calls operator delete via slot 0x140152b68
// when the low bit of the flags arg is set. For now we keep the lightweight
// stub: the menu Geometry lives until process exit, leaking is fine. Full
// dtor body is in TODOs for a follow-up session.
extern "C" void* GeomDraw_Dtor(void* self, uint32_t /*flags*/) {
    return self;
}

// ============================================================
// Slot +0x08 — chunk-streaming / heightmap rebuild
// ============================================================
//
// Original is a 5-byte JMP-rel32 thunk (exe 0x1400AED40 → 0x1400AAD50). The
// target walks the binary tree of dirty chunks at self+0xE8, regenerates each
// chunk's vertices via FUN_1400ac880, and uploads them into the master
// VertexBuffer at self+0x100 via Video::VertexBuffer::Set.
//
// Body lives in serialize_heightmap_mesh.cpp next to the related helpers
// (FUN_1400ac880, helper_e270, sub_c1e0) and shares its lazy Runtime
// resolution machinery.
extern "C" void Reimpl_GeometryChunkUpdate(void* self);

extern "C" void GeomDraw_ChunkUpdate(void* self) {
    Reimpl_GeometryChunkUpdate(self);
}

// ============================================================
// Slot +0x18 — collect-phase Draw (frustum cull + queue draw items)
// ============================================================
//
// Decompiled from exe 0x1400AE490 (~300 instructions). Takes (self, unused, renderer)
// where `unused` is a Time* that the original ignores. Walks the heightmap
// chunk grid, frustum-culls each chunk, and calls Renderer::AddDrawItem for
// each visible chunk + the SoA arrays at +0x1C0/+0x1D0/+0x1F0/+0x200.
extern "C" void GeomDraw_CollectDraw(void* self, void* /*unused*/, void* renderer) {
    if (!self || !renderer) return;

    static int s_calls = 0;
    if (s_calls == 0) {
        printf("[GeomDraw_CollectDraw] first call self=%p renderer=%p\n",
               self, renderer);
        fflush(stdout);
    }
    ++s_calls;

    // Cascade: every Runtime primitive needed must be wired. ENDGAME B.3.5 batch K1:
    // the five Video/Graphics terms (GetCullCamera, Geometry::GetTransform,
    // Camera::GetFrustum, Renderer::AddDrawItem, Renderer::GetShader) left this
    // aggregate with their call sites; the two Volume::Frustum terms stay, because
    // `Volume::Frustum` is not a Video/Graphics class and its doors are untouched.
    if (!fn_Frustum_MulTransform3 || !fn_Frustum_Contains) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Geometry::CollectDraw] Volume::Frustum primitives unwired — "
                   "terrain will not render. Frustum=%p\n",
                   (void*)fn_Frustum_Contains);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    auto* selfBytes = reinterpret_cast<uint8_t*>(self);

    int32_t  gridSize  = *reinterpret_cast<int32_t*>(selfBytes + 0xC0);
    int32_t  chunkSize = *reinterpret_cast<int32_t*>(selfBytes + 0xC8);
    int      chunkCount = (chunkSize > 0) ? (gridSize / chunkSize) : 0;

    // 1. Capture cull camera world transform as a Transform3 (camera+0x64
    //    holds the camera-to-world Transform3, 48 bytes).
    const void* camera = &reinterpret_cast<Graphics::Renderer*>(renderer)
                              ->GetCullCamera();   // B.3.5 bridge
    Math::Transform3 cameraTransform3;
    if (camera) {
        cameraTransform3 = *reinterpret_cast<const Math::Transform3*>(
            reinterpret_cast<const uint8_t*>(camera) + 0x64);
    }

    // 2. Compute geometry-space frustum:
    //    frustum_local = (camera_frustum * cameraToWorld * geometry_inverseTransform)
    // B.3.5 bridge — `?GetTransform@Geometry@Graphics@@QEBA?AVTransform@Math@@XZ` is a
    // hidden-retslot door (B5-12) and `Graphics::Geometry::GetTransform` is NON-VIRTUAL,
    // so the door body moves to the call site: exactly ONE construction, into the same
    // caller storage the door was handed.
    const Math::Transform selfTransform =
        reinterpret_cast<const Graphics::Geometry*>(self)->GetTransform();   // B.3.5 bridge
    Math::Transform  selfInvTransform  = selfTransform.Inverse();
    Math::Transform3 selfInvTransform3 = selfInvTransform.AsTransform3();

    // Get camera frustum, then transform it into geometry space.
    // B.3.5: `alignas` added because the retslot door's caller storage is now
    // placement-new'd into as a real `Volume::Frustum` (0x64 bytes of float). The
    // retail door wrote the same bytes here through RDX.
    alignas(16) uint8_t cameraFrustumBuf[256] = {0};   // 256 is a safe upper bound
    uint8_t worldFrustumBuf[256]  = {0};
    uint8_t localFrustumBuf[256]  = {0};
    if (camera) {
        // B.3.5 bridge — `?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ` is a
        // hidden-retslot door; `Graphics::Camera::GetFrustum` is NON-VIRTUAL and returns
        // `::Frustum` by value, so ONE construction into the SAME caller storage.
        ::new (static_cast<void*>(cameraFrustumBuf))
            ::Frustum(reinterpret_cast<const Graphics::Camera*>(camera)
                          ->GetFrustum());   // B.3.5 bridge
    }
    // worldFrustum = cameraFrustum * cameraTransform3
    fn_Frustum_MulTransform3(cameraFrustumBuf, worldFrustumBuf, &cameraTransform3);
    PerfX::SetWorldFrustum(worldFrustumBuf);   // [PerfX] the sim-side off-screen test uses this frame's planes
    // localFrustum = worldFrustum * geometryInverseTransform3
    fn_Frustum_MulTransform3(worldFrustumBuf, localFrustumBuf, &selfInvTransform3);

    // 3. Cached values from self.
    float    uvScale     = g_fUVScale ? *g_fUVScale : 1.0f;
    float    fHalfGrid   = static_cast<float>(gridSize >> 1);
    uint16_t materialKey = *reinterpret_cast<uint16_t*>(selfBytes + 0x28);
    void*    matInst0    = *reinterpret_cast<void**>(selfBytes + 0xF8);
    float    chunkSizeF  = static_cast<float>(chunkSize);

    // 4. Per-chunk frustum cull + AddDrawItem.
    //
    // Chunk box in geometry space:
    //   xmin = j * chunkSize - halfGrid          xmax = xmin + chunkSize
    //   ymin = i * chunkSize - halfGrid          ymax = ymin + chunkSize
    //   zmin = -16.0f                             zmax = -16.0f + 32.0f = 16.0f
    //
    // Box layout passed to Frustum::Contains is 6 floats: {xmin,ymin,zmin, xmax,ymax,zmax}.
    float zMin = kChunkZMin;
    float zMax = kChunkZMin + kChunkZRange;

    for (int i = 0; i < chunkCount; ++i) {
        for (int j = 0; j < chunkCount; ++j) {
            float xmin = static_cast<float>(j) * chunkSizeF - fHalfGrid;
            float ymin = static_cast<float>(i) * chunkSizeF - fHalfGrid;
            float xmax = xmin + chunkSizeF;
            float ymax = ymin + chunkSizeF;

            // Box layout passed to Frustum::Contains: {xmin,ymin,zmin, xmax,ymax,zmax}.
            float box[6] = { xmin, ymin, zMin, xmax, ymax, zMax };

            uint32_t r = fn_Frustum_Contains(localFrustumBuf, box, 0x3F);
            if (r != 0x40) {
                uint16_t key2 = static_cast<uint16_t>((i << 6) | j);
                reinterpret_cast<Graphics::Renderer*>(renderer)->AddDrawItem(
                    reinterpret_cast<const Graphics::Geometry*>(self),
                    reinterpret_cast<const Graphics::MaterialInstance*>(matInst0),
                    materialKey, key2, 0.0f, uvScale, 1, nullptr, 0);   // B.3.5 bridge
            }
        }
    }

    // 5. If the renderer's shader != 1 (i.e., not shadow-only pass), emit
    //    the auxiliary draw items for the SoA arrays.
    int shader = reinterpret_cast<const Graphics::Renderer*>(renderer)
                     ->GetShader();   // B.3.5 bridge
    if (shader == 1) return;

    // --- WATER overlay (standalone): emit the flat sea-level water mesh (built
    //     into VB+0x160/IB+0x180 by WorldGen_BuildWaterMesh). Main pass only
    //     (we already returned on the shadow pass), so water casts no shadow —
    //     matching the original (GetShader()!=1 guards the water draws). Mode
    //     0xA000 routes to the water branch in ItemDraw. Material from +0x150
    //     (or +0x158 via BANISHED_WATER_MAT=1). No-op if the mesh/material
    //     wasn't built (VB data ptr null / material null).
    // The real b260 contour mesh occupies VB+0x160/IB+0x180 and is drawn via the
    // +0x1c8/+0x1d8 range tables (modes 0x6000/0x8000 below); there is no flat
    // 0xA000 water sheet any more.

    int count1C8 = *reinterpret_cast<int32_t*>(selfBytes + 0x1C8);
    void* matInst1 = *reinterpret_cast<void**>(selfBytes + 0x150);
    for (int idx = 0; idx < count1C8; ++idx) {
        uint16_t key1 = static_cast<uint16_t>(idx | 0x3900);
        uint16_t key2 = static_cast<uint16_t>(idx | 0x6000);
        reinterpret_cast<Graphics::Renderer*>(renderer)->AddDrawItem(
            reinterpret_cast<const Graphics::Geometry*>(self),
            reinterpret_cast<const Graphics::MaterialInstance*>(matInst1),
            key1, key2, 0.0f, uvScale, 1, nullptr, 0);   // B.3.5 bridge
    }

    int count1D8 = *reinterpret_cast<int32_t*>(selfBytes + 0x1D8);
    void* matInst2 = *reinterpret_cast<void**>(selfBytes + 0x158);
    for (int idx = 0; idx < count1D8; ++idx) {
        uint16_t key1 = static_cast<uint16_t>(idx | 0x4000);
        uint16_t key2 = static_cast<uint16_t>(idx | 0x8000);
        reinterpret_cast<Graphics::Renderer*>(renderer)->AddDrawItem(
            reinterpret_cast<const Graphics::Geometry*>(self),
            reinterpret_cast<const Graphics::MaterialInstance*>(matInst2),
            key1, key2, 0.0f, uvScale, 1, nullptr, 0);   // B.3.5 bridge
    }

    // 0x1F0 — array of 0x60-byte structs at +0x1F0 (count +0x1F8).
    int count1F8 = *reinterpret_cast<int32_t*>(selfBytes + 0x1F8);
    uint8_t* base1F0 = *reinterpret_cast<uint8_t**>(selfBytes + 0x1F0);
    for (int idx = 0; idx < count1F8; ++idx) {
        uint64_t* elem = reinterpret_cast<uint64_t*>(base1F0 + (size_t)idx * 0x60);
        if (elem[0] == 0) continue;

        // Per-element box: read four ints at offsets +0x2C/+0x30/+0x34 and
        // +offset 0x38/+0x3C (== elem[6]/elem[7] as packed qwords).
        // Layout: elem[6] is the high 8 bytes — but the original walks via
        // (uint8_t*)elem + 0x2C and elem[6]/elem[7]. Let's be explicit:
        const uint8_t* elemBytes = reinterpret_cast<const uint8_t*>(elem);
        float xmax = static_cast<float>(*reinterpret_cast<const int32_t*>(elemBytes + 0x34)) - fHalfGrid;
        float ymax = static_cast<float>(static_cast<int32_t>(elem[7])) - fHalfGrid;
        float xmin = static_cast<float>(*reinterpret_cast<const int32_t*>(elemBytes + 0x2C)) - fHalfGrid;
        float ymin = static_cast<float>(static_cast<int32_t>(elem[6])) - fHalfGrid;
        float box[6] = { xmin, ymin, 0.0f, xmax, ymax, 0.0f };

        uint32_t r = fn_Frustum_Contains(localFrustumBuf, box, 0x3F);
        if (r == 0x40) continue;

        // key1 = 1 normally; 3 if (uint8_t)(elem + 0x5D) != 0.
        uint16_t key1 = (elemBytes[0x5D] != 0) ? 3 : 1;
        uint16_t key2 = static_cast<uint16_t>(idx | 0x2000);
        void* mat = reinterpret_cast<void*>(elem[0]);
        reinterpret_cast<Graphics::Renderer*>(renderer)->AddDrawItem(
            reinterpret_cast<const Graphics::Geometry*>(self),
            reinterpret_cast<const Graphics::MaterialInstance*>(mat),
            key1, key2, 0.0f, uvScale, 1, nullptr, 0);   // B.3.5 bridge
    }

    // 0x200 — same struct layout, count at +0x208. Differs in: filter is
    // (elem[0] != 0) AND ((int)elem[2] != 0), key1 = 2.
    int count208 = *reinterpret_cast<int32_t*>(selfBytes + 0x208);
    uint8_t* base200 = *reinterpret_cast<uint8_t**>(selfBytes + 0x200);
    for (int idx = 0; idx < count208; ++idx) {
        uint64_t* elem = reinterpret_cast<uint64_t*>(base200 + (size_t)idx * 0x60);
        if (elem[0] == 0) continue;
        if (static_cast<int32_t>(elem[2]) == 0) continue;

        const uint8_t* elemBytes = reinterpret_cast<const uint8_t*>(elem);
        float xmax = static_cast<float>(*reinterpret_cast<const int32_t*>(elemBytes + 0x34)) - fHalfGrid;
        float ymax = static_cast<float>(static_cast<int32_t>(elem[7])) - fHalfGrid;
        float xmin = static_cast<float>(*reinterpret_cast<const int32_t*>(elemBytes + 0x2C)) - fHalfGrid;
        float ymin = static_cast<float>(static_cast<int32_t>(elem[6])) - fHalfGrid;
        float box[6] = { xmin, ymin, 0.0f, xmax, ymax, 0.0f };

        uint32_t r = fn_Frustum_Contains(localFrustumBuf, box, 0x3F);
        if (r == 0x40) continue;

        uint16_t key2 = static_cast<uint16_t>(idx | 0x4000);
        void* mat = reinterpret_cast<void*>(elem[0]);
        reinterpret_cast<Graphics::Renderer*>(renderer)->AddDrawItem(
            reinterpret_cast<const Graphics::Geometry*>(self),
            reinterpret_cast<const Graphics::MaterialInstance*>(mat),
            2, key2, 0.0f, uvScale, 1, nullptr, 0);   // B.3.5 bridge
    }
}

// ============================================================
// Slot +0x10 — per-item Draw (called back per AddDrawItem invocation)
// ============================================================
//
// Decompiled from exe 0x1400AEA10 (~170 instructions). Signature:
//   (self, Context*, unused, unused, float param5, uint param6)
// where param5 is the interpolation alpha (Scalar) and param6 is the per-item
// key (the 2nd ushort key passed to AddDrawItem above, packed with extra bits
// in 0xE000 to choose draw mode).
extern "C" void GeomDraw_ItemDraw(void* self, void* context, void* /*p3*/,
                                  void* /*p4*/, float param5, uint32_t param6) {
    if (!self || !context) return;

    static int s_calls = 0;
    if (s_calls == 0) {
        printf("[GeomDraw_ItemDraw] first call self=%p ctx=%p alpha=%f key=0x%x\n",
               self, context, param5, param6);
        fflush(stdout);
    }
    ++s_calls;

    auto* selfBytes = reinterpret_cast<uint8_t*>(self);

    // ENDGAME B.3.5 closing pass: the LAST term of this readiness aggregate is gone
    // with the door it tested. `fn_InterpolatedState_Transform_Lerp` was kept by two
    // batches because "there is no `Graphics::InterpolatedState<T>` class in
    // runtime/types/graphics at all" -- true then, and fixed rather than worked
    // around: `runtime/types/graphics/InterpolatedState.h` declares it, the layout is
    // read from retail three ways, and `Lerp` is ported. The whole early-return and
    // its "context primitives unwired" printf went with the term; nothing else was
    // in the condition.

    // 1. Lerp the InterpolatedState<Transform> at self+0x84 with the alpha.
    // B.3.5 bridge -- ?Lerp@?$InterpolatedState@VTransform@Math@@@Graphics@@QEBA?A
    // VTransform@Math@@AEBVScalar@4@@Z (0x18001aff0). A hidden-retslot door becomes a
    // by-value C++ return: ONE construction, into the same named local the retbuf
    // was. Retail stored the Scalar on its own stack at retbuf+0x28 and passed its
    // address; `param5` is that float and the reference binds to it directly.
    const Math::Transform lerpedTransform =
        reinterpret_cast<const Graphics::InterpolatedState<Math::Transform>*>(
            selfBytes + 0x84)->Lerp(
                *reinterpret_cast<const Math::Scalar*>(&param5));   // B.3.5 bridge

    // 2. Convert to Matrix and SetTransform(0, matrix).
    //    ?AsMatrix@Transform@Math@@QEBA?AVMatrix@2@XZ  retail 0x18001e5f0
    Math::Matrix matrixBuf = lerpedTransform.AsMatrix();
    reinterpret_cast<Video::Context*>(context)
        ->SetTransform(0, matrixBuf);   // B.3.5 bridge

    // 3. SetColor(1,1,1, lerp(uvScaleA, uvScaleB)).
    //    Layout from disasm: local_78/74/70 = 1.0f, local_6c = lerp result.
    float color[4];
    color[0] = 1.0f;
    color[1] = 1.0f;
    color[2] = 1.0f;
    float uvA = *reinterpret_cast<float*>(selfBytes + 0x80);
    float uvB = *reinterpret_cast<float*>(selfBytes + 0x7C);
    float uvScale = g_fUVScale ? *g_fUVScale : 1.0f;
    color[3] = (uvScale - param5) * uvA + param5 * uvB;
    reinterpret_cast<Video::Context*>(context)->SetColor(
        *reinterpret_cast<const Math::Vector4*>(color));   // B.3.5 bridge

    // 4. Branch on the high 3 bits (0xE000) of param6.
    uint32_t key = param6 & 0xFFFF1FFFu;     // local index
    uint32_t mode = param6 & 0xE000u;        // draw-mode selector

    // Helper: indexed draw via per-element vert+idx pointers (used by 0x2000
    // and 0x4000 branches).
    auto IndexedDrawFromElem = [&](uint8_t* elem) {
        int32_t  vertCount = *reinterpret_cast<int32_t*>(elem + 0x10);
        int32_t  vertStride = 0x14;             // hardcoded in original
        int32_t  idxCount  = *reinterpret_cast<int32_t*>(elem + 0x20);
        bool     ok = reinterpret_cast<Video::Context*>(context)
                          ->BeginIndexedVertices(vertCount, vertStride,
                                                 idxCount);   // B.3.5 bridge
        if (!ok) return;

        // Memcpy vert data into context->stagingVertBuf at context+0x20E0,
        // then advance the staging pointer.
        void**    pStageVert = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(context) + 0x20E0);
        int32_t*  pStageVertStride = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(context) + 0x20E8);
        void**    pStageIdx  = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(context) + 0x20F0);

        size_t vertBytes = static_cast<size_t>(vertCount) * static_cast<size_t>(*pStageVertStride);
        std::memcpy(*pStageVert, *reinterpret_cast<void**>(elem + 8), vertBytes);
        *pStageVert = reinterpret_cast<uint8_t*>(*pStageVert) + vertBytes;

        size_t idxBytes = static_cast<size_t>(idxCount) * 2;
        std::memcpy(*pStageIdx, *reinterpret_cast<void**>(elem + 0x18), idxBytes);
        *pStageIdx = reinterpret_cast<uint8_t*>(*pStageIdx) + idxBytes;

        reinterpret_cast<Video::Context*>(context)
            ->EndIndexedVertices(0);   // B.3.5 bridge
    };

    if (mode == 0x2000) {
        // Element from 0x1F0-array; stride 0x60.
        uint8_t* base = *reinterpret_cast<uint8_t**>(selfBytes + 0x1F0);
        if (!base) return;
        IndexedDrawFromElem(base + (size_t)static_cast<int>(key) * 0x60);
        return;
    }
    if (mode == 0x4000) {
        // Element from 0x200-array; stride 0x60.
        uint8_t* base = *reinterpret_cast<uint8_t**>(selfBytes + 0x200);
        if (!base) return;
        IndexedDrawFromElem(base + (size_t)static_cast<int>(key) * 0x60);
        return;
    }

    // Helper: SetVertexBuffer + dirty-IndexBuffer fields, then DrawIndexed using
    // 4-int per-element pulls from a child array.
    auto SetVB_BindIB_DrawIndexedFromArray = [&](void* vb, void* ibField,
                                                 uint8_t* indexedArrayBase) {
        reinterpret_cast<Video::Context*>(context)
            ->SetVertexBuffer(vb, 0, 0, 0x10);   // B.3.5 bridge

        // If context's bound IB != ourIB, dirty the bind.
        void**    pCtxIB        = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(context) + 0x1B8);
        int32_t*  pCtxIBOffset  = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(context) + 0x1C0);
        uint32_t* pCtxFlags     = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(context) + 0x10);
        if (*pCtxIB != ibField || *pCtxIBOffset != 0) {
            *pCtxFlags |= 0x10;
            *pCtxIB = ibField;
            *pCtxIBOffset = 0;
        }
        // Read 4 ints from indexedArrayBase[key*0x10 .. +0xC]; pass as DrawIndexed args.
        int32_t* iarr = reinterpret_cast<int32_t*>(indexedArrayBase + (size_t)static_cast<int>(key) * 0x10);
        int32_t a = iarr[0];
        int32_t b = iarr[1];
        int32_t c = iarr[2];
        int32_t d = iarr[3];
        // DrawIndexed(context, primType=0, b, c, a, d, 1)
        // Original order at LAB_1400aed0e: (param_2, 0, iVar7, iVar8, iVar9, iVar10, 1)
        //   iVar7 = arr[1], iVar8 = arr[2], iVar9 = arr[0], iVar10 = arr[3].
        reinterpret_cast<Video::Context*>(context)
            ->DrawIndexed(0, b, c, a, d, 1);   // B.3.5 bridge
    };

    if (mode == 0x8000) {
        // VB at +0x160, IB at +0x180, indexed array at +0x1D0.
        uint8_t* indexedArr = *reinterpret_cast<uint8_t**>(selfBytes + 0x1D0);
        if (!indexedArr) return;
        SetVB_BindIB_DrawIndexedFromArray(selfBytes + 0x160, selfBytes + 0x180,
                                          indexedArr);
        return;
    }
    if (mode == 0x6000) {
        // VB at +0x160, IB at +0x180, indexed array at +0x1C0.
        uint8_t* indexedArr = *reinterpret_cast<uint8_t**>(selfBytes + 0x1C0);
        if (!indexedArr) return;
        SetVB_BindIB_DrawIndexedFromArray(selfBytes + 0x160, selfBytes + 0x180,
                                          indexedArr);
        return;
    }

    if (mode == 0xA000) {
        // Water overlay chunk — same chunk draw as terrain but from the water
        // VB/IB (+0x160/+0x180) and WITHOUT the height-image bind (the water
        // material binds its own _water_/_reflection_/_frame_ textures).
        reinterpret_cast<Video::Context*>(context)
            ->SetVertexBuffer(selfBytes + 0x160, 0, 0, 0x10);   // B.3.5 bridge

        void**    pCtxIB       = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(context) + 0x1B8);
        int32_t*  pCtxIBOffset = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(context) + 0x1C0);
        uint32_t* pCtxFlags    = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(context) + 0x10);
        void*     ourIB        = selfBytes + 0x180;
        if (*pCtxIB != ourIB || *pCtxIBOffset != 0) {
            *pCtxFlags |= 0x10;
            *pCtxIB = ourIB;
            *pCtxIBOffset = 0;
        }

        int32_t chunkSize = *reinterpret_cast<int32_t*>(selfBytes + 0xC8);
        int32_t scale     = *reinterpret_cast<int32_t*>(selfBytes + 0xD4);
        int32_t gridSize  = *reinterpret_cast<int32_t*>(selfBytes + 0xC0);
        int32_t primCount = chunkSize * chunkSize * 6;
        int32_t chunksPerSide = (chunkSize > 0) ? (gridSize / chunkSize) : 0;
        int32_t row = static_cast<int32_t>((key >> 6) & 0x3F);
        int32_t col = static_cast<int32_t>(key & 0x3F);
        int32_t start = (chunksPerSide * row + col) * scale;
        reinterpret_cast<Video::Context*>(context)
            ->DrawIndexed(0, start, scale, 0, primCount, 1);   // B.3.5 bridge
        return;
    }

    // Default branch: terrain chunk draw.
    //   SetImageBuffer(self+0x148, 6)
    //   SetVertexBuffer(self+0x100, 0, 0, 0x10)
    //   bind IB at self+0x120 (dirty if needed)
    //   compute primCount = chunkSize*chunkSize*6, scaled by self+0xD4
    //   start = ((gridSize/chunkSize)*((key>>6)&0x3F) + (key&0x3F)) * (self+0xD4)
    //   DrawIndexed(context, 0, 0, primCount, 0, start, 1)
    void* imgBuf = *reinterpret_cast<void**>(selfBytes + 0x148);
    reinterpret_cast<Video::Context*>(context)
        ->SetImageBuffer(imgBuf, 6);                              // B.3.5 bridge
    reinterpret_cast<Video::Context*>(context)
        ->SetVertexBuffer(selfBytes + 0x100, 0, 0, 0x10);          // B.3.5 bridge

    void**    pCtxIB        = reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(context) + 0x1B8);
    int32_t*  pCtxIBOffset  = reinterpret_cast<int32_t*>(reinterpret_cast<uint8_t*>(context) + 0x1C0);
    uint32_t* pCtxFlags     = reinterpret_cast<uint32_t*>(reinterpret_cast<uint8_t*>(context) + 0x10);
    void*     ourIB         = selfBytes + 0x120;
    if (*pCtxIB != ourIB || *pCtxIBOffset != 0) {
        *pCtxFlags |= 0x10;
        *pCtxIB = ourIB;
        *pCtxIBOffset = 0;
    }

    int32_t chunkSize = *reinterpret_cast<int32_t*>(selfBytes + 0xC8);
    int32_t scale     = *reinterpret_cast<int32_t*>(selfBytes + 0xD4);
    int32_t gridSize  = *reinterpret_cast<int32_t*>(selfBytes + 0xC0);
    int32_t primCount = chunkSize * chunkSize * 6;
    int32_t chunksPerSide = (chunkSize > 0) ? (gridSize / chunkSize) : 0;
    int32_t row = static_cast<int32_t>((key >> 6) & 0x3F);
    int32_t col = static_cast<int32_t>(key & 0x3F);
    int32_t start = (chunksPerSide * row + col) * scale;

    // Original arg order at LAB_1400aed0e:
    //   DrawIndexed(ctx, 0, iVar7=start, iVar8=scale, iVar9=0, iVar10=primCount, 1)
    reinterpret_cast<Video::Context*>(context)
        ->DrawIndexed(0, start, scale, 0, primCount, 1);   // B.3.5 bridge
}

// ============================================================
// Slots +0x20..+0x40 — inherited Graphics::Geometry base methods
// ============================================================
//
// Each is a thin trampoline that forwards to the corresponding Runtime DLL
// export. The original exe used `FF 25 imm32` IAT thunks; in standalone we
// need real C functions in our PE that the vtable can dispatch through. These
// preserve the calling convention exactly (RCX=this, RDX=hidden_retval where
// applicable, R8/R9/stack=remaining args) since the ABI is identical for
// "trampoline that tail-calls another fn with same signature".

// ENDGAME B.3.5 batch K1. All five call the MEMBER, and all five spell it
// QUALIFIED — `->Graphics::Geometry::X(...)` — which is NOT a stylistic choice.
// Slots 4..8 are VIRTUAL on `Graphics::Geometry`, and these five functions ARE the
// five slots of this file's hand-built derived vtable. An unqualified
// `geom->UpdateAttached()` would dispatch back through that table into this same
// function and recurse until the stack is gone. The qualified form is the non-virtual
// call to Geometry's own body, which is exactly the address the removed
// `fn_Geometry_*` globals held (`?UpdateAttached@Geometry@Graphics@@UEAAXXZ` and
// siblings). The old `if (fn_...)` tests were readiness tests on those pointers and
// go with them; the members always exist.

// +0x20 — Graphics::Geometry::UpdateAttached (no args, no return)
extern "C" void GeomDraw_UpdateAttached(void* self) {
    reinterpret_cast<Graphics::Geometry*>(self)
        ->Graphics::Geometry::UpdateAttached();   // B.3.5 bridge
}

// +0x28 — Graphics::Geometry::GetSubTransform (member fn, hidden retval)
//
// Signature in retail AND in C++: `Math::Transform* GetSubTransform(Math::Transform*
// ret, int node) const` — the hidden return slot is spelled OUT, in retail's order
// (`this`, then the caller's storage), per B5-12 and its COORDINATOR AMENDMENT: a
// by-value C++ return is WRONG for a VIRTUAL here, because GCC's ms_abi passes the
// hidden pointer FIRST and MSVC (and every minted row) passes `this` first.
extern "C" void* GeomDraw_GetSubTransform(void* self, void* retval, int idx) {
    return reinterpret_cast<const Graphics::Geometry*>(self)
        ->Graphics::Geometry::GetSubTransform(
            static_cast<Math::Transform*>(retval), idx);   // B.3.5 bridge
}

// +0x30 — Graphics::Geometry::GetInstanceTransform (member fn, hidden retval)
extern "C" void* GeomDraw_GetInstanceTransform(void* self, void* retval, int idx, float t) {
    return reinterpret_cast<const Graphics::Geometry*>(self)
        ->Graphics::Geometry::GetInstanceTransform(
            static_cast<Math::Matrix*>(retval), idx, t);   // B.3.5 bridge
}

// +0x38 — Graphics::Geometry::SetSharedInstanceData (returns int)
extern "C" int GeomDraw_SetSharedInstanceData(void* self, void* context, int idx, float t) {
    return reinterpret_cast<const Graphics::Geometry*>(self)
        ->Graphics::Geometry::SetSharedInstanceData(
            *reinterpret_cast<Video::Context*>(context), idx, t);   // B.3.5 bridge
}

// +0x40 — Graphics::Geometry::Clean (returns bool)
extern "C" bool GeomDraw_Clean(void* self) {
    return reinterpret_cast<Graphics::Geometry*>(self)
        ->Graphics::Geometry::Clean();   // B.3.5 bridge
}
