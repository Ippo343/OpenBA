// code/Game/Game/weather_particle_draw.cpp
//
// Faithful port of the weather-particle Geometry Draw vtable slot (+0x10),
// exe FUN_1400d3120 (Application-x64.exe @ 0x1400d3120).
//
// This is the renderer for the falling precipitation (rain / snow) particle
// field. The particle simulation stores a compact array of particles that
// live in a single box-sized cell (edge = desc.boxSize). To make the weather
// blanket the whole area around the camera without simulating millions of
// particles, Draw *tiles* that one cell over a 5x5x5 grid (offsets -2..+2 on
// each axis, snapped to the box grid around the camera position) and draws the
// same particle field once per visible tile via hardware instancing, one
// Context::SetTransform translation matrix per tile.
//
// ALGORITHM (line-for-line with FUN_1400d3120):
//   1. Copy the camera Transform3 (camera+0x64). Its columns are the billboard
//      basis vectors and its translation (col at +0x24) is the camera position.
//         col0 = T[+0x00..+0x08]   col1 = T[+0x0c..+0x14]
//         col2 = T[+0x18..+0x20]   camPos = T[+0x24..+0x2c]
//   2. Snap the camera position to the box grid:
//         snapped = floor(camPos / boxSize) * boxSize   (per component)
//   3. Frustum-cull the 5x5x5 tile grid. For tile (i,j,k) in [-2..2]^3:
//         center = snapped + boxSize*(i,j,k)
//         aabb   = [center, center + (boxSize,boxSize,boxSize)]
//      Transform the camera frustum by the camera transform and test it against
//      each aabb; if Contains(...) != 0x40 (0x40 == fully outside sentinel) the
//      tile is kept and its cell-center (center + boxSize/2) is appended to the
//      visible-tile list.
//   4. Context::SetColor(1,1,1, geo.intensity) and Context::SetInverseScale from
//      the descriptor near/far fade bounds (desc+0x4c/0x50 and +0x54/0x58).
//   5. Build the per-particle billboard quads into a dynamic staging buffer
//      (4 verts/particle, 0x14 bytes/vert). Billboard axes:
//         - default (desc.velocityStretch == 0): camera-facing, axisU = col2*size/2,
//           axisV = col0*size/2, constant for all particles.
//         - velocityStretch: per-particle, axisV = normalize(vel x col1)*size/2,
//           axisU = normalize(axisV x col1)*size/2, and the trailing edge uses the
//           particle's prevPos so the quad streaks along the velocity.
//      Quad corners:
//         v0 = pos   + axisU - axisV      v1 = pos   + axisU + axisV
//         v2 = altpos- axisU + axisV      v3 = altpos- axisU - axisV
//      Each vertex = { x,y,z, 0.0f, uvColor } with the four corner uv/color
//      dwords 0x00000000, 0x000000ff, 0x0000ffff, 0x0000ff00.
//   6. Instanced submit: batch = min(GetMaxDynamicInstances, visibleTiles).
//      For each batch, SetTransform a translation matrix per tile, then
//      BeginInstancedIndexedVertices, memcpy the vertex block into the context
//      dynamic-vertex cursor (ctx+0x20e0, stride ctx+0x20e8) and the index block
//      into the dynamic-index cursor (ctx+0x20f0), then EndInstancedIndexedVertices.
//
// Struct layout (verified against the 0x1400d3120 disassembly):
//   Geometry `geo`:
//     +0xc0 float intensity   +0xc8 Weather* descriptor
//     +0xd0 index buffer ptr  +0xd8 int indexCount
//     +0xe0 particle array    +0xe8 int particleCount
//   Particle (stride 0x24): +0x00 pos[3]  +0x0c prevPos[3]  +0x18 velocity[3]
//   Weather descriptor (geo+0xc8):
//     +0x40 bool velocityStretch  +0x44 float particleSize  +0x48 float boxSize
//     +0x4c/+0x50 near fade lo/hi  +0x54/+0x58 far fade lo/hi
//   Video::Context cursors: +0x20e0 vtx write ptr, +0x20e8 int vtx stride,
//     +0x20f0 idx write ptr.
//   Camera+0x64 = Transform3 (0x70 bytes).
//
// Constants (bit-exact): g_fUVScale = 1.0f, DAT_140172114 = 0.5f,
//   DAT_1401722c0 = 0x80000000 (float negate via XOR), FUN_140131390 = floorf.
//
// The pure vector math (Vector3 ctor / Cross / Normalize / operator*= / scalar
// broadcast) is inlined in plain C++ — it is bit-identical to the Runtime
// Math::Vector3 helpers the exe calls (Cross computes this x other; the by-value
// Normalize returns this/|this|). Only the operations that touch opaque engine
// state (Transform3 copy, Camera::GetFrustum, Frustum::operator*/Contains,
// Matrix::Matrix, and the six Video::Context methods) are resolved from
// Runtime-x64.dll.

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::Camera and Video::Context
// are real classes. Both headers go AHEAD of "resolve.h" — resolve.h pulls
// <windows.h>, whose <wingdi.h> declares a global `Rectangle` FUNCTION that hides
// `Volume::Rectangle`'s class name inside Volume.h's own static_asserts.
#include "graphics/Camera.h"
#include "WeatherParticleGeometry.h"   // caller batch CB6: Draw is the class's
#include "Weather.h"                   // caller batch CB6: the descriptor's fields
#include <atomic>
#include "video/Context.h"
#include "math/Math.h"
#include "object/External.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "volume/Volume.h"   // RtVolume_Frustum_MulTransform3 / _Contains
#include <new>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace {

// ------------------------------------------------------------------
// Runtime function pointers (lazy, resolved once on first Draw call).
// ------------------------------------------------------------------
// ENDGAME B.3.5 Video/Graphics caller batch: SEVEN doors left this block —
// `?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ` and the six
// `Video::Context` members below, which are now direct member calls:
//     0x18001a8e0  ?SetColor@Context@Video@@QEAAXAEBVVector4@Math@@@Z
//     0x18001a880  Context::SetInverseScale(float,float,float,float)
//     0x1800f21d0  Context::GetMaxDynamicInstances(int,int,int) const
//     0x18001a6b0  ?SetTransform@Context@Video@@QEAAXHAEBVMatrix@Math@@@Z
//     0x1800f2130  Context::BeginInstancedIndexedVertices(int,int,int,int)
//     0x1800f2110  Context::EndInstancedIndexedVertices(int)
// The four Math/Volume doors that stayed then are direct calls now (B3R).
// Called by name (B3R): ??0Transform3@Math@@QEAA@AEBV01@@Z -> the Math::Transform3 copy
// ctor (the registered RtMath_Transform3_CtorCopy is a memcpy of the class), ??D /
// ?Contains@Frustum@Volume@@ -> RtVolume_Frustum_MulTransform3 / _Contains
// (volume/Volume.h), ??0Matrix@Math@@QEAA@AEBVVector4@1@000@Z (Runtime 0x18001e540) ->
// the Math::Matrix column ctor (RtMath_Matrix_CtorColumns assigns Matrix(c0, c1, c2, c3)).

// ------------------------------------------------------------------
// Inlined Vector3 math (bit-identical to the Math::Vector3 Runtime helpers).
// ------------------------------------------------------------------
inline void v3_cross(float out[3], const float a[3], const float b[3]) {
    // this (a) x other (b)
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
inline void v3_normalize(float out[3], const float a[3]) {
    float len = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    if (len > 1e-20f) {
        float inv = 1.0f / len;
        out[0] = a[0] * inv; out[1] = a[1] * inv; out[2] = a[2] * inv;
    } else {
        out[0] = 0.0f; out[1] = 0.0f; out[2] = 0.0f;
    }
}

inline float rd_f(const void* base, int off) {
    return *reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(base) + off);
}

} // namespace

// ============================================================
// WeatherParticleGeometry::Draw — exe FUN_1400d3120 (Geometry Draw vtable +0x10)
//   this      : particle Geometry (0xf0 bytes)
//   videoCtx  : Video::Context
//   cam       : Graphics::Camera
// Caller batch CB6: the body of the old `WeatherParticle_Draw(geo, context,
// camera)` free function (its one caller was this slot, which delegated to it;
// no address binding), moved into the member. The `geo + off` reads became the
// class's named fields; the pointer-null test went with the pointers (the slot
// receives references). The dead one-shot flag is a namespace-scope atomic (B6-5).
namespace { std::atomic<bool> s_weatherDrawWarned{false}; }

void WeatherParticleGeometry::Draw(Video::Context& videoCtx, const Graphics::Camera& cam,
                                   Graphics::ExternalPtrHandle /*material*/,
                                   float /*opacity*/, int /*a*/, int /*b*/, int /*c*/) const {
    // B.3.5: the seven Video/Graphics readiness terms are gone with their doors;
    // B3R: the four Math/Volume ones too.
    if (!System::MemoryAllocator::_singleton) {
        // Cascade-safe: skip the weather draw rather than crash on a null fn ptr.
        s_weatherDrawWarned.store(true);
        return;
    }

    void*    ctx = &videoCtx;
    void*    camera = const_cast<Graphics::Camera*>(&cam);
    if (!_descriptor) return;                             // +0xC8 Weather descriptor
    // The descriptor is a Weather (see WeatherParticle_Config, weather_particles.cpp).
    const Weather* const desc = static_cast<const Weather*>(_descriptor);   // B.3.8 bridge

    const float boxSize      = desc->size;            // Weather +0x48
    const float particleSize = desc->particleSize;    // Weather +0x44
    const bool  velStretch   = (*reinterpret_cast<const uint8_t*>(&desc->velocityStretch) != 0);   // +0x40
    const int   particleCount = static_cast<int32_t>(_particleCountCap);   // +0xE8 count
    const int   indexCount    = static_cast<int32_t>(_indexCount);         // +0xD8
    const float intensity     = _intensity;                                // +0xC0

    // (1) Copy the camera Transform3 (camera+0x64) and pull its basis columns.
    uint8_t camXform[0x70];
    ::new (static_cast<void*>(camXform)) Math::Transform3(
        *reinterpret_cast<const Math::Transform3*>(reinterpret_cast<uint8_t*>(camera) + 0x64));

    const float col0[3] = { rd_f(camXform, 0x00), rd_f(camXform, 0x04), rd_f(camXform, 0x08) };
    const float col1[3] = { rd_f(camXform, 0x0c), rd_f(camXform, 0x10), rd_f(camXform, 0x14) };
    const float col2[3] = { rd_f(camXform, 0x18), rd_f(camXform, 0x1c), rd_f(camXform, 0x20) };
    const float camPos[3] = { rd_f(camXform, 0x24), rd_f(camXform, 0x28), rd_f(camXform, 0x2c) };

    const float boxHalf = boxSize * 0.5f;   // DAT_140172114 = 0.5f

    // (2) Snap camera position to the box grid (floor(camPos/box)*box).
    float snapped[3];
    for (int c = 0; c < 3; ++c)
        snapped[c] = std::floor(camPos[c] / boxSize) * boxSize;

    // (3) Frustum-cull the 5x5x5 tile grid. Visible cell-centers collected here.
    //     Sized generously and zero-initialised: the instanced submit loop can
    //     over-read past the visible count (faithful to the original, which reads
    //     into a large zero-inited stack buffer), and Matrix::Matrix reads a 4th
    //     float one Vector3 past the position pointer.
    const int kTileCap = 384;                 // >= 125 visible + batch over-read + 1
    float tiles[kTileCap * 3];
    std::memset(tiles, 0, sizeof(tiles));
    int visibleTiles = 0;

    // worldFrustum = GetFrustum(camera) * camXform
    // B.3.5: `alignas` added because the retslot door's caller storage is now
    // placement-new'd into as a real `Volume::Frustum` (0x64 bytes of float).
    // The retail door wrote the same bytes here through RDX.
    alignas(16) uint8_t frBuf[0x100];
    uint8_t worldFrustum[0x100];
    // B.3.5 bridge — ?GetFrustum@Camera@Graphics@@QEBA?AVFrustum@Volume@@XZ is a
    // hidden-retslot door (B5-12). `Graphics::Camera::GetFrustum` is NON-VIRTUAL
    // and returns `::Frustum` by value, so the door body moves to the call site:
    // ONE construction, into the SAME caller storage the door was handed.
    void* fr = ::new (static_cast<void*>(frBuf))
        ::Frustum(reinterpret_cast<const Graphics::Camera*>(camera)->GetFrustum());   // B.3.5 bridge
    RtVolume_Frustum_MulTransform3(static_cast<const ::Frustum*>(fr),
                                   reinterpret_cast<::Frustum*>(worldFrustum),
                                   reinterpret_cast<const Math::Transform3*>(camXform));

    for (int i = -2; i < 3; ++i) {            // z (outer)
        for (int j = -2; j < 3; ++j) {        // y (middle)
            for (int k = -2; k < 3; ++k) {    // x (inner)
                float center[3] = {
                    boxSize * (float)k + snapped[0],
                    boxSize * (float)j + snapped[1],
                    boxSize * (float)i + snapped[2],
                };
                // AABB = { center (min), center + box (max) } — 6 floats.
                float aabb[6] = {
                    center[0], center[1], center[2],
                    center[0] + boxSize, center[1] + boxSize, center[2] + boxSize,
                };
                uint32_t r = RtVolume_Frustum_Contains(reinterpret_cast<const ::Frustum*>(worldFrustum),
                                                       reinterpret_cast<const ::Box*>(aabb), 0x3f);
                if (r != 0x40) {
                    if (visibleTiles < kTileCap) {
                        tiles[visibleTiles * 3 + 0] = center[0] + boxHalf;
                        tiles[visibleTiles * 3 + 1] = center[1] + boxHalf;
                        tiles[visibleTiles * 3 + 2] = center[2] + boxHalf;
                    }
                    ++visibleTiles;
                }
            }
        }
    }

    // (4) Per-draw context state.
    float color[4] = { 1.0f, 1.0f, 1.0f, intensity };
    reinterpret_cast<Video::Context*>(ctx)->SetColor(
        *reinterpret_cast<const Math::Vector4*>(color));   // B.3.5 bridge

    const float g_fUVScale = 1.0f;            // 0x14017214c
    reinterpret_cast<Video::Context*>(ctx)->SetInverseScale(   // B.3.5 bridge
        -desc->nearFadeStart,                                     // -near_hi (+0x50, XOR sign flip)
        g_fUVScale / (desc->nearFade - desc->nearFadeStart),      // 1 / (near_lo - near_hi)
        -desc->farFade,                                           // -far_hi (+0x58)
        g_fUVScale / (desc->farFadeStart - desc->farFade));       // 1 / (far_lo - far_hi)

    // (5) Build the per-particle billboard quads (4 verts * 0x14 bytes each).
    const int   kVtxStride = 0x14;
    const int   vtxCount   = particleCount * 4;
    uint8_t*    staging    = nullptr;
    if (vtxCount > 0) {
        staging = reinterpret_cast<uint8_t*>(
            System::MemoryAllocator::MemAlloc(static_cast<uint64_t>(vtxCount) * kVtxStride, 4));
    }

    // Camera-facing billboard axes (default; overwritten per-particle if stretch).
    float axisU[3] = { col2[0] * (particleSize * 0.5f),
                       col2[1] * (particleSize * 0.5f),
                       col2[2] * (particleSize * 0.5f) };
    float axisV[3] = { col0[0] * (particleSize * 0.5f),
                       col0[1] * (particleSize * 0.5f),
                       col0[2] * (particleSize * 0.5f) };

    // Corner uv/color dwords (packed shorts baked per corner).
    static const uint32_t kCornerUV[4] = { 0x00000000u, 0x000000ffu, 0x0000ffffu, 0x0000ff00u };

    if (staging && particleCount > 0) {
        uint8_t* particles = static_cast<uint8_t*>(_particles);   // +0xE0
        uint8_t* out = staging;
        for (int p = 0; p < particleCount; ++p) {
            const float* pbase = reinterpret_cast<const float*>(particles + (size_t)p * 0x24);
            const float* pos   = pbase;                    // +0x00 pos
            const float* alt   = velStretch ? (pbase + 3)  // +0x0c prevPos
                                            : pbase;        // else pos

            if (velStretch) {
                // Velocity-oriented axes: velocity x col1, then that x col1.
                const float* vel = pbase + 6;              // +0x18 velocity
                float perpA[3], perpAn[3], perpB[3], perpBn[3];
                v3_cross(perpA, vel, col1);
                v3_normalize(perpAn, perpA);
                v3_cross(perpB, perpAn, col1);
                v3_normalize(perpBn, perpB);
                for (int c = 0; c < 3; ++c) {
                    axisU[c] = perpBn[c] * (particleSize * 0.5f);   // local_918
                    axisV[c] = perpAn[c] * (particleSize * 0.5f);   // local_928
                }
            }

            float v0[3], v1[3], v2[3], v3[3];
            for (int c = 0; c < 3; ++c) {
                v0[c] = pos[c] + axisU[c] - axisV[c];
                v1[c] = pos[c] + axisU[c] + axisV[c];
                v2[c] = alt[c] - axisU[c] + axisV[c];
                v3[c] = alt[c] - axisU[c] - axisV[c];
            }

            const float* verts[4] = { v0, v1, v2, v3 };
            for (int q = 0; q < 4; ++q) {
                float*    fout = reinterpret_cast<float*>(out);
                uint32_t* uout = reinterpret_cast<uint32_t*>(out);
                fout[0] = verts[q][0];
                fout[1] = verts[q][1];
                fout[2] = verts[q][2];
                fout[3] = 0.0f;
                uout[4] = kCornerUV[q];
                out += kVtxStride;
            }
        }
    }

    // (6) Instanced submit, batched by the max dynamic-instance count.
    int batch = reinterpret_cast<const Video::Context*>(ctx)
                    ->GetMaxDynamicInstances(vtxCount, kVtxStride, indexCount);   // B.3.5 bridge
    if (visibleTiles < batch) batch = visibleTiles;

    if (visibleTiles > 0) {
        const float mcol0[4] = { 1.0f, 0.0f, 0.0f, 0.0f };
        const float mcol1[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
        const float mcol2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };

        int tileBase = 0;
        do {
            if (batch > 0) {
                for (int t = 0; t < batch; ++t) {
                    float matrix[16];
                    // Translation matrix: identity basis + tile cell-center.
                    // Matrix::Matrix reads a 4th float from `pos`; the tiles[]
                    // buffer is padded/zeroed so that read is well-defined.
                    *reinterpret_cast<Math::Matrix*>(matrix) = Math::Matrix(
                        *reinterpret_cast<const Math::Vector4*>(mcol0),
                        *reinterpret_cast<const Math::Vector4*>(mcol1),
                        *reinterpret_cast<const Math::Vector4*>(mcol2),
                        *reinterpret_cast<const Math::Vector4*>(&tiles[(tileBase + t) * 3]));
                    reinterpret_cast<Video::Context*>(ctx)->SetTransform(
                        t, *reinterpret_cast<const Math::Matrix*>(matrix));   // B.3.5 bridge
                }
            }
            if (reinterpret_cast<Video::Context*>(ctx)->BeginInstancedIndexedVertices(
                    vtxCount, kVtxStride, indexCount, batch)) {   // B.3.5 bridge
                int    stride = *reinterpret_cast<int*>(reinterpret_cast<uint8_t*>(ctx) + 0x20e8);
                void*& vtxCur = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ctx) + 0x20e0);
                void*& idxCur = *reinterpret_cast<void**>(reinterpret_cast<uint8_t*>(ctx) + 0x20f0);

                // Copy the whole particle vertex block into the dynamic vertex cursor.
                size_t vtxBytes = static_cast<size_t>(vtxCount) * stride;
                std::memcpy(vtxCur, staging, vtxBytes);
                vtxCur = reinterpret_cast<uint8_t*>(vtxCur) + vtxBytes;

                // Copy the shared particle index buffer into the dynamic index cursor.
                size_t idxBytes = static_cast<size_t>(indexCount) * 2;
                std::memcpy(idxCur, _indices, idxBytes);   // +0xD0
                idxCur = reinterpret_cast<uint8_t*>(idxCur) + idxBytes;

                reinterpret_cast<Video::Context*>(ctx)
                    ->EndInstancedIndexedVertices(0);   // B.3.5 bridge
            }
            tileBase += batch;
        } while (tileBase < visibleTiles);
    }

    if (staging)
        System::MemoryAllocator::MemFree(staging);
}
