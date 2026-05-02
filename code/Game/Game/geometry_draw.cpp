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
// In standalone mode the 9-slot vtable in serialize_arrays.cpp points at the
// functions defined here. In DLL mode the original exe code backs the vtable;
// nothing here is exercised. We still compile it in both modes (gated on
// STANDALONE_EXE only at the registry layer) to keep the source unified.

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

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
// Slot +0x08 — chunk-streaming / heightmap rebuild stub
// ============================================================
//
// Original is a 5-byte JMP-rel32 thunk (exe 0x1400AED40 → 0x1400AAD50). The
// target rebuilds vertex buffer regions for any "dirty" heightmap chunks
// queued in self+0xE8. For the menu scene this never gets called (Serialize
// already populated the initial chunks), so a no-op is safe. If terrain stops
// updating dynamically in-game later, this is the place to wire up the real
// body.
extern "C" void GeomDraw_ChunkUpdate(void* /*self*/) {
    // no-op
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

    // Cascade: every Runtime primitive needed must be wired.
    if (!fn_Renderer_GetCullCamera || !fn_Transform3_Copy_Ctor ||
        !fn_Geometry_GetTransform || !fn_Transform_Inverse ||
        !fn_Camera_GetFrustum || !fn_Frustum_MulTransform3 ||
        !fn_Frustum_Contains || !fn_Renderer_AddDrawItem ||
        !fn_Renderer_GetShader || !fn_Vector3_FromFloats_Ctor) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Geometry::CollectDraw] Runtime draw primitives unwired — "
                   "terrain will not render. Renderer=%p AddDrawItem=%p "
                   "Frustum=%p\n",
                   (void*)fn_Renderer_GetCullCamera, (void*)fn_Renderer_AddDrawItem,
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
    void*    camera = fn_Renderer_GetCullCamera(renderer);
    uint8_t  cameraTransform3[48] = {0};
    if (camera) {
        const uint8_t* src = reinterpret_cast<const uint8_t*>(camera) + 0x64;
        fn_Transform3_Copy_Ctor(cameraTransform3, src);
    }

    // 2. Compute geometry-space frustum:
    //    frustum_local = (camera_frustum * cameraToWorld * geometry_inverseTransform)
    uint8_t selfTransform[0x1C]    = {0};   // Math::Transform = 28 bytes
    uint8_t selfInvTransform[0x1C] = {0};
    uint8_t selfInvTransform3[48]  = {0};
    fn_Geometry_GetTransform(self, selfTransform);
    fn_Transform_Inverse(selfTransform, selfInvTransform);
    if (!fn_Transform_AsTransform3) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Geometry::CollectDraw] fn_Transform_AsTransform3 unwired\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }
    fn_Transform_AsTransform3(selfInvTransform, selfInvTransform3);

    // Get camera frustum, then transform it into geometry space.
    uint8_t cameraFrustumBuf[256] = {0};   // Frustum size unknown; 256 is a safe upper bound
    uint8_t worldFrustumBuf[256]  = {0};
    uint8_t localFrustumBuf[256]  = {0};
    if (camera) {
        fn_Camera_GetFrustum(camera, cameraFrustumBuf);
    }
    // worldFrustum = cameraFrustum * cameraTransform3
    fn_Frustum_MulTransform3(cameraFrustumBuf, worldFrustumBuf, cameraTransform3);
    // localFrustum = worldFrustum * geometryInverseTransform3
    fn_Frustum_MulTransform3(worldFrustumBuf, localFrustumBuf, selfInvTransform3);

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

    int dbgVisible = 0;
    int dbgCulled = 0;
    uint32_t dbgFirstFew[8] = {0};
    int dbgFirstFewN = 0;

    for (int i = 0; i < chunkCount; ++i) {
        for (int j = 0; j < chunkCount; ++j) {
            float xmin = static_cast<float>(j) * chunkSizeF - fHalfGrid;
            float ymin = static_cast<float>(i) * chunkSizeF - fHalfGrid;
            float xmax = xmin + chunkSizeF;
            float ymax = ymin + chunkSizeF;

            // Allocate Box on stack — original lays out the 6 floats by
            // building a Vector3 (xmax,ymax,zmax) and gluing xmin/ymin/zmin in.
            float box[6] = { xmin, ymin, zMin, xmax, ymax, zMax };

            uint32_t r = fn_Frustum_Contains(localFrustumBuf, box, 0x3F);
            if (s_calls == 1 && dbgFirstFewN < 8) {
                dbgFirstFew[dbgFirstFewN++] = r;
            }
            if (r != 0x40) {
                ++dbgVisible;
                // Visible (or partial). Queue a draw item.
                uint16_t key2 = static_cast<uint16_t>((i << 6) | j);
                fn_Renderer_AddDrawItem(renderer, self, matInst0, materialKey,
                                        key2, 0.0f, uvScale, 1, nullptr, 0);
            } else {
                ++dbgCulled;
            }
        }
    }

    if (s_calls == 1) {
        // Read camera transform translation (Transform3 layout: 9 floats rotation + 3 floats translation = 48 bytes).
        // Common layout: last 12 bytes = translation. Print first 12 floats so we can see what we got.
        const float* camFloats = reinterpret_cast<const float*>(cameraTransform3);
        const float* invFloats = reinterpret_cast<const float*>(selfInvTransform3);
        printf("[CollectDraw] grid=%d chunkSize=%d chunkCount=%d halfGrid=%.1f matKey=%u mat0=%p\n",
               gridSize, chunkSize, chunkCount, fHalfGrid, materialKey, matInst0);
        printf("[CollectDraw] camera=%p selfXform=%p (Transform 28B), uvScale=%.3f\n",
               camera, self, uvScale);
        // Dump camera struct fields around 0x60..0xC0 — first 24 floats — so we can
        // see whether the world transform lives at a different offset than +0x64.
        if (camera) {
            const float* cam = reinterpret_cast<const float*>(camera);
            printf("[CollectDraw] Camera floats @+0x00..0x60 (24f):\n");
            for (int row = 0; row < 6; ++row) {
                printf("  +0x%02x: %.3f %.3f %.3f %.3f\n",
                       row * 16, cam[row*4+0], cam[row*4+1], cam[row*4+2], cam[row*4+3]);
            }
            printf("[CollectDraw] Camera floats @+0x60..0xC0 (24f):\n");
            for (int row = 0; row < 6; ++row) {
                int off = 0x60 + row * 16;
                const float* p = reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(camera) + off);
                printf("  +0x%02x: %.3f %.3f %.3f %.3f\n", off, p[0], p[1], p[2], p[3]);
            }
        }
        printf("[CollectDraw] cameraT3 (12 floats): %.2f %.2f %.2f / %.2f %.2f %.2f / %.2f %.2f %.2f / %.2f %.2f %.2f\n",
               camFloats[0], camFloats[1], camFloats[2],
               camFloats[3], camFloats[4], camFloats[5],
               camFloats[6], camFloats[7], camFloats[8],
               camFloats[9], camFloats[10], camFloats[11]);
        printf("[CollectDraw] invSelfT3 (12 floats): %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f\n",
               invFloats[0], invFloats[1], invFloats[2],
               invFloats[3], invFloats[4], invFloats[5],
               invFloats[6], invFloats[7], invFloats[8],
               invFloats[9], invFloats[10], invFloats[11]);
        printf("[CollectDraw] terrain cull: visible=%d culled=%d  first8 r-vals: %u %u %u %u %u %u %u %u\n",
               dbgVisible, dbgCulled,
               dbgFirstFew[0], dbgFirstFew[1], dbgFirstFew[2], dbgFirstFew[3],
               dbgFirstFew[4], dbgFirstFew[5], dbgFirstFew[6], dbgFirstFew[7]);
        // Sample box at chunk (0,0) to compare with cull camera position
        float bxmin = -fHalfGrid;
        float bymin = -fHalfGrid;
        printf("[CollectDraw] chunk(0,0) box=(%.1f,%.1f,%.1f .. %.1f,%.1f,%.1f)\n",
               bxmin, bymin, zMin, bxmin + chunkSizeF, bymin + chunkSizeF, zMax);
        fflush(stdout);
    }

    // 5. If the renderer's shader != 1 (i.e., not shadow-only pass), emit
    //    the auxiliary draw items for the SoA arrays.
    int shader = fn_Renderer_GetShader(renderer);
    if (s_calls <= 3) {
        printf("[CollectDraw call#%d] Renderer::GetShader=%d (1=shadow-only-skip-decals)\n",
               s_calls, shader);
        fflush(stdout);
    }
    if (shader == 1) return;

    int count1C8 = *reinterpret_cast<int32_t*>(selfBytes + 0x1C8);
    void* matInst1 = *reinterpret_cast<void**>(selfBytes + 0x150);
    for (int idx = 0; idx < count1C8; ++idx) {
        uint16_t key1 = static_cast<uint16_t>(idx | 0x3900);
        uint16_t key2 = static_cast<uint16_t>(idx | 0x6000);
        fn_Renderer_AddDrawItem(renderer, self, matInst1, key1, key2,
                                0.0f, uvScale, 1, nullptr, 0);
    }

    int count1D8 = *reinterpret_cast<int32_t*>(selfBytes + 0x1D8);
    void* matInst2 = *reinterpret_cast<void**>(selfBytes + 0x158);
    for (int idx = 0; idx < count1D8; ++idx) {
        uint16_t key1 = static_cast<uint16_t>(idx | 0x4000);
        uint16_t key2 = static_cast<uint16_t>(idx | 0x8000);
        fn_Renderer_AddDrawItem(renderer, self, matInst2, key1, key2,
                                0.0f, uvScale, 1, nullptr, 0);
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
        fn_Renderer_AddDrawItem(renderer, self, mat, key1, key2, 0.0f, uvScale,
                                1, nullptr, 0);
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
        fn_Renderer_AddDrawItem(renderer, self, mat, 2, key2, 0.0f, uvScale,
                                1, nullptr, 0);
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

    if (!fn_InterpolatedState_Transform_Lerp || !fn_Transform_AsMatrix ||
        !fn_Context_SetTransform || !fn_Context_SetColor) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[Geometry::ItemDraw] Runtime context primitives unwired — "
                   "geometry won't render this frame\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    // 1. Lerp the InterpolatedState<Transform> at self+0x84 with the alpha.
    //    Original layout: scalar storage on stack at retbuf+0x28; we just
    //    pass &param5 in.
    uint8_t lerpedTransform[0x1C] = {0};
    fn_InterpolatedState_Transform_Lerp(selfBytes + 0x84, lerpedTransform, &param5);

    // 2. Convert to Matrix and SetTransform(0, matrix).
    uint8_t matrixBuf[0x40] = {0};   // Math::Matrix is 64 bytes
    fn_Transform_AsMatrix(lerpedTransform, matrixBuf);
    fn_Context_SetTransform(context, 0, matrixBuf);

    if (s_calls == 1) {
        const float* xfFloats = reinterpret_cast<const float*>(lerpedTransform);
        const float* mxFloats = reinterpret_cast<const float*>(matrixBuf);
        printf("[ItemDraw] lerpedXform 7f: %.3f %.3f %.3f / %.3f %.3f %.3f / %.3f\n",
               xfFloats[0], xfFloats[1], xfFloats[2],
               xfFloats[3], xfFloats[4], xfFloats[5], xfFloats[6]);
        printf("[ItemDraw] matrix row0: %.3f %.3f %.3f %.3f\n",
               mxFloats[0], mxFloats[1], mxFloats[2], mxFloats[3]);
        printf("[ItemDraw] matrix row3: %.3f %.3f %.3f %.3f\n",
               mxFloats[12], mxFloats[13], mxFloats[14], mxFloats[15]);
        int32_t cs = *reinterpret_cast<int32_t*>(selfBytes + 0xC8);
        int32_t sc = *reinterpret_cast<int32_t*>(selfBytes + 0xD4);
        int32_t gs = *reinterpret_cast<int32_t*>(selfBytes + 0xC0);
        printf("[ItemDraw] gridSize=%d chunkSize=%d scale(+0xD4)=%d  → primCount=%d\n",
               gs, cs, sc, cs*cs*6);
        fflush(stdout);
    }

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
    fn_Context_SetColor(context, color);

    // 4. Branch on the high 3 bits (0xE000) of param6.
    uint32_t key = param6 & 0xFFFF1FFFu;     // local index
    uint32_t mode = param6 & 0xE000u;        // draw-mode selector

    // Helper: indexed draw via per-element vert+idx pointers (used by 0x2000
    // and 0x4000 branches).
    auto IndexedDrawFromElem = [&](uint8_t* elem) {
        if (!fn_Context_BeginIndexedVertices || !fn_Context_EndIndexedVertices) return;
        int32_t  vertCount = *reinterpret_cast<int32_t*>(elem + 0x10);
        int32_t  vertStride = 0x14;             // hardcoded in original
        int32_t  idxCount  = *reinterpret_cast<int32_t*>(elem + 0x20);
        bool     ok = fn_Context_BeginIndexedVertices(context, vertCount,
                                                       vertStride, idxCount);
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

        fn_Context_EndIndexedVertices(context, 0);
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
        if (!fn_Context_SetVertexBuffer || !fn_Context_DrawIndexed) return;
        fn_Context_SetVertexBuffer(context, vb, 0, 0, 0x10);

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
        fn_Context_DrawIndexed(context, 0, b, c, a, d, 1);
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

    // Default branch: terrain chunk draw.
    //   SetImageBuffer(self+0x148, 6)
    //   SetVertexBuffer(self+0x100, 0, 0, 0x10)
    //   bind IB at self+0x120 (dirty if needed)
    //   compute primCount = chunkSize*chunkSize*6, scaled by self+0xD4
    //   start = ((gridSize/chunkSize)*((key>>6)&0x3F) + (key&0x3F)) * (self+0xD4)
    //   DrawIndexed(context, 0, 0, primCount, 0, start, 1)
    if (!fn_Context_SetImageBuffer || !fn_Context_SetVertexBuffer ||
        !fn_Context_DrawIndexed) {
        return;
    }
    void* imgBuf = *reinterpret_cast<void**>(selfBytes + 0x148);
    fn_Context_SetImageBuffer(context, imgBuf, 6);
    fn_Context_SetVertexBuffer(context, selfBytes + 0x100, 0, 0, 0x10);

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

    if (s_calls == 1) {
        // Dump the inline VB struct (32B at +0x100) and IB struct (48B at +0x120)
        // first 16 bytes each, to verify they're constructed and populated.
        const uint64_t* vbq = reinterpret_cast<const uint64_t*>(selfBytes + 0x100);
        const uint64_t* ibq = reinterpret_cast<const uint64_t*>(selfBytes + 0x120);
        const uint64_t* imq = reinterpret_cast<const uint64_t*>(imgBuf);
        printf("[ItemDraw default] imgBuf=%p VB=%p IB=%p key=0x%x row=%d col=%d primCount=%d scale=%d start=%d\n",
               imgBuf, (void*)(selfBytes + 0x100), (void*)(selfBytes + 0x120),
               key, row, col, primCount, scale, start);
        printf("[ItemDraw VB]   q0=%016llx q1=%016llx q2=%016llx q3=%016llx\n",
               (unsigned long long)vbq[0], (unsigned long long)vbq[1],
               (unsigned long long)vbq[2], (unsigned long long)vbq[3]);
        printf("[ItemDraw IB]   q0=%016llx q1=%016llx q2=%016llx q3=%016llx q4=%016llx q5=%016llx\n",
               (unsigned long long)ibq[0], (unsigned long long)ibq[1],
               (unsigned long long)ibq[2], (unsigned long long)ibq[3],
               (unsigned long long)ibq[4], (unsigned long long)ibq[5]);
        printf("[ItemDraw img]  q0=%016llx q1=%016llx q2=%016llx q3=%016llx\n",
               (unsigned long long)imq[0], (unsigned long long)imq[1],
               (unsigned long long)imq[2], (unsigned long long)imq[3]);

        // Dereference the VB data pointer (q1) and dump first 8 vertices (128B at stride 16).
        // If terrain mesh was actually uploaded we'd see real position/uv float values.
        // If zeros or garbage, the vertex buffer was never populated.
        const float* vbData = reinterpret_cast<const float*>(vbq[1]);
        if (vbData) {
            printf("[ItemDraw VB data @%p] first 8 verts (16B each):\n", (void*)vbData);
            for (int v = 0; v < 8; ++v) {
                printf("  v%d: %.3f %.3f %.3f %.3f\n",
                       v, vbData[v*4+0], vbData[v*4+1], vbData[v*4+2], vbData[v*4+3]);
            }
        } else {
            printf("[ItemDraw VB data] data pointer NULL — VB is uninitialized!\n");
        }

        // Also dereference IB data pointer (q1 = 0x03401358 in last log)
        const uint16_t* ibData = reinterpret_cast<const uint16_t*>(ibq[1]);
        if (ibData) {
            printf("[ItemDraw IB data @%p] first 18 indices:\n", (const void*)ibData);
            printf("  %u %u %u  %u %u %u  %u %u %u  %u %u %u  %u %u %u  %u %u %u\n",
                   ibData[0], ibData[1], ibData[2],
                   ibData[3], ibData[4], ibData[5],
                   ibData[6], ibData[7], ibData[8],
                   ibData[9], ibData[10], ibData[11],
                   ibData[12], ibData[13], ibData[14],
                   ibData[15], ibData[16], ibData[17]);
        }

        // Material at self+0xF8 — first 16 qwords. Look for a vtable in q0
        // (Runtime DLL ~0x180000000+ or our exe), and shader pointers later.
        void* mat = *reinterpret_cast<void**>(selfBytes + 0xF8);
        if (mat) {
            const uint64_t* mq = reinterpret_cast<const uint64_t*>(mat);
            printf("[ItemDraw Material @%p]:\n", mat);
            for (int i = 0; i < 8; ++i) {
                printf("  q%d: %016llx\n", i, (unsigned long long)mq[i]);
            }
        }
        fflush(stdout);
    }
    // Original arg order at LAB_1400aed0e:
    //   DrawIndexed(ctx, 0, iVar7=start, iVar8=scale, iVar9=0, iVar10=primCount, 1)
    fn_Context_DrawIndexed(context, 0, start, scale, 0, primCount, 1);
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

// +0x20 — Graphics::Geometry::UpdateAttached (no args, no return)
extern "C" void GeomDraw_UpdateAttached(void* self) {
    if (fn_Geometry_UpdateAttached) fn_Geometry_UpdateAttached(self);
}

// +0x28 — Graphics::Geometry::GetSubTransform (member fn, hidden retval)
//
// Signature: Math::Transform GetSubTransform(int idx) const
// Lowered: (this, retval, idx)
extern "C" void* GeomDraw_GetSubTransform(void* self, void* retval, int idx) {
    if (fn_Geometry_GetSubTransform) {
        return fn_Geometry_GetSubTransform(self, retval, idx);
    }
    return retval;
}

// +0x30 — Graphics::Geometry::GetInstanceTransform (member fn, hidden retval)
extern "C" void* GeomDraw_GetInstanceTransform(void* self, void* retval, int idx, float t) {
    if (fn_Geometry_GetInstanceTransform) {
        return fn_Geometry_GetInstanceTransform(self, retval, idx, t);
    }
    return retval;
}

// +0x38 — Graphics::Geometry::SetSharedInstanceData (returns int)
extern "C" int GeomDraw_SetSharedInstanceData(void* self, void* context, int idx, float t) {
    if (fn_Geometry_SetSharedInstanceData) {
        return fn_Geometry_SetSharedInstanceData(self, context, idx, t);
    }
    return 0;
}

// +0x40 — Graphics::Geometry::Clean (returns bool)
extern "C" bool GeomDraw_Clean(void* self) {
    if (fn_Geometry_Clean) return fn_Geometry_Clean(self);
    return false;
}
