// code/Game/Game/minimap_paint.cpp
// GUI Keystone F — minimap TERRAIN PAINTER (render-to-texture).
//
// Port of exe FUN_1400a5140 (Ghidra label ModDialog_impl_5140), the minimap's
// "ImageCapture::DrawCheck". The retail game calls it lazily from GameWorld::Draw
// (FUN_140096010) the first frame the minimap's terrain GPU buffer (minimap+0x08)
// is still null:
//
//     if (*(minimap+0x08) == 0) {
//         Scene::RemoveGlobalObject(mainScene, sunLight);
//         Scene::RemoveGlobalObject(mainScene, worldGeom);
//         ImageCapture::DrawCheck(minimap, ctx, worldGeom, sunLight);   // <-- this
//         Scene::AddGlobalObject(mainScene, sunLight);
//         Scene::AddGlobalObject(mainScene, worldGeom);
//     }
//
// It renders the terrain geometry with a top-down orthographic camera into an
// off-screen RenderBuffer, reads the rendered color back into the minimap's CPU
// terrain pixel array (minimap+0x18) via ImageBuffer::CopyFromTexture, then
// creates the display GPU buffers (minimap+0x08/+0x10) and uploads both pixel
// arrays — the CreateAndUpload tail is shared with code/Game/Game/minimap_upload.cpp.
//
// In standalone the exe function is unavailable (ResolveAddr(Exe,0x1400a5140)==NULL),
// so GameUpdateLoop::Draw's paint block was skipped and the minimap stayed blank.
// This clean port lets it run: gameupdateloop.cpp wires fn_ImageCapture_DrawCheck to
// Minimap_PaintTerrain when the exe address doesn't resolve (standalone).
//
// Minimap struct (== GameWorld+0x148):
//   +0x00 byte  dirty flag
//   +0x04 int   width (grid edge, e.g. 256)
//   +0x08 ImageBuffer* terrain GPU texture   (display; built by the tail)
//   +0x10 ImageBuffer* overlay GPU texture   (display; built by the tail)
//   +0x18 uint32*      terrain raw pixel array (width*width ARGB)  <-- painted here
//   +0x28 uint32*      overlay raw pixel array (width*width ARGB)  (stays 0 here)
//
// Ortho camera (byte-faithful to FUN_1400a5140):
//   half = width*0.5;  ortho frustum near=0 far=512 box[-half..half]x[half..-half]
//   yaw=0 pitch=-90deg (straight down), world position (0,0,100).
//   fog start=1000 end=1001 color(1,1,1,1); clear color 0xff000000 then 0xffff00ff.

// ENDGAME B.3.5 Video/Graphics caller batch: Graphics::{Camera,Scene},
// Video::{Context,RenderBuffer,ImageBuffer} are real classes. These headers go
// AHEAD of "resolve.h" — resolve.h pulls <windows.h>, whose <wingdi.h> declares
// a global `Rectangle` FUNCTION that hides `Volume::Rectangle`'s class name
// inside Volume.h's own static_asserts (Camera.h and Geometry.h include Volume.h).
#include "graphics/Camera.h"
#include "graphics/Scene.h"
#include "video/Context.h"       // also brings Video::{Device,RenderBuffer,Viewport}
#include "video/ImageBuffer.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "math/Math.h"
#include "options.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "video/Device.h"   // ENDGAME B.3.9 SS2: Video::Configuration::_singleton
#include "Game/exe_bodies.h"

// Shared with minimap_upload.cpp — creates the display GPU buffers at minimap
// +0x08/+0x10 (ports FUN_1400a4e80) and uploads +0x18/+0x28 into them.
extern "C" void Minimap_CreateAndUpload(void* mapObj);

namespace {

// --- New Runtime primitives (not in engine_funcs; self-resolved once) ---
//
// ENDGAME B.3.5 Video/Graphics caller batch: TWELVE doors left this file, and
// the caches, their typedefs, their resolves, their `s_ok` terms and their
// diagnostic `%p`s went with them:
//   ?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z
//   ?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z
//   ?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z
//   ?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z
//   ?Clear@Context@Video@@QEAAXIIMH@Z
//   ?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z
//   ?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z
//   ?Reset@Context@Video@@QEAAXXZ
//   ?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z
//   fn_Video_Context_SetFog / fn_Graphics_Scene_AddGlobalObject /
//   fn_Graphics_Scene_Clean  (engine_funcs globals — the GLOBALS stay, only
//   their call sites here converted), plus the hand-inlined ImageBuffer vtable
//   +0x48 `Create` slot in MakeRenderTargetBuffer.
//
// What is STILL a door, and why:
//   s_RenderBuffer_Ctor / s_RenderBuffer_Dtor / s_Renderer_Dtor / s_Scene_Dtor
//   and fn_Graphics_{Camera,Renderer,Scene}_Ctor  — CTOR/DTOR doors (B4b-2,
//       B35_RULES section 11: no Video/Graphics class has a real ctor or dtor).
//   s_Frustum_Ortho_Ctor  — a Volume::Frustum constructor, not a member of any
//       Video/Graphics class.
//   (`fn_ImageBuffer_Ctor` went at ENDGAME B.3.9 EXA: `::new (p) Video::ImageBuffer()`.)
typedef void* (*fn_RenderBuffer_Ctor_t)(void* self);
typedef void  (*fn_RenderBuffer_Dtor_t)(void* self);
typedef void* (*fn_Frustum_Ortho_Ctor_t)(void* self, bool ortho, const void* near_,
                                          const void* far_, const void* s3,
                                          const void* s4, const void* s5, const void* s6);
typedef void  (*fn_Renderer_Dtor_t)(void* self);
typedef void  (*fn_Scene_Dtor_t)(void* self);

fn_RenderBuffer_Ctor_t         s_RenderBuffer_Ctor      = nullptr;
fn_RenderBuffer_Dtor_t         s_RenderBuffer_Dtor      = nullptr;
fn_Frustum_Ortho_Ctor_t        s_Frustum_Ortho_Ctor     = nullptr;
fn_Renderer_Dtor_t             s_Renderer_Dtor          = nullptr;
fn_Scene_Dtor_t                s_Scene_Dtor             = nullptr;

bool s_resolved = false;
bool s_ok       = false;

bool ResolveFuncs() {
    if (s_resolved) return s_ok;
    s_resolved = true;

    s_RenderBuffer_Ctor    = (fn_RenderBuffer_Ctor_t)   ResolveExport("??0RenderBuffer@Video@@QEAA@XZ");
    s_RenderBuffer_Dtor    = (fn_RenderBuffer_Dtor_t)   ResolveExport("??1RenderBuffer@Video@@QEAA@XZ");
    s_Frustum_Ortho_Ctor   = (fn_Frustum_Ortho_Ctor_t)  ResolveExport("??0Frustum@Volume@@QEAA@_NAEBVScalar@Math@@11111@Z");
    s_Renderer_Dtor        = (fn_Renderer_Dtor_t)       ResolveExport("??1Renderer@Graphics@@QEAA@XZ");
    s_Scene_Dtor           = (fn_Scene_Dtor_t)          ResolveExport("??1Scene@Graphics@@QEAA@XZ");

    s_ok = s_RenderBuffer_Ctor && s_RenderBuffer_Dtor &&
           s_Frustum_Ortho_Ctor &&
           s_Renderer_Dtor && s_Scene_Dtor &&
           // engine_funcs primitives this port relies on:
           System::MemoryAllocator::_singleton &&
           // B.3.5: the CopyFromTexture readiness term went with its door.
           fn_Graphics_Camera_Ctor &&
           fn_Graphics_Renderer_Ctor && fn_Graphics_Scene_Ctor;
           // B.3.5 batch K1: the Renderer::Draw readiness term went with its door;
           // ENDGAME B.3.9 SS2: the Video::Configuration door term with its door.

    if (!s_ok) {
        printf("[MinimapPaint] primitives unresolved — terrain paint disabled\n"
               "  RB(ctor=%p dtor=%p) Frustum=%p Rdtor=%p Sdtor=%p\n",
               (void*)s_RenderBuffer_Ctor, (void*)s_RenderBuffer_Dtor,
               (void*)s_Frustum_Ortho_Ctor,
               (void*)s_Renderer_Dtor, (void*)s_Scene_Dtor);
        fflush(stdout);
    }
    return s_ok;
}

// Allocate + construct a Video::ImageBuffer (retail: the imported ctor + the
// exe's own copy of the class's table), then Create it as a render-target
// (color) / depth surface. Returns the buffer or null.
void* MakeRenderTargetBuffer(int width, int usage, int format) {
    void* img = System::MemoryAllocator::MemAlloc(0x68, 8);
    if (!img) return nullptr;
    ::new (img) Video::ImageBuffer();   // B.3.9 EXA: the real class (exe table 0x14015c8c0 is its own)
    reinterpret_cast<Video::ImageBuffer*>(img)->Create(
        /*type*/0, usage, format, /*flags*/0u, width, width,
        /*filter*/1, /*addressU*/1, /*addressV*/1, /*addressW*/1,
        /*levels*/nullptr);   // B.3.5 bridge
    return img;
}

// Free an ImageBuffer via its scalar-deleting destructor (vtable[0](self, 1)).
void FreeImageBuffer(void* img) {
    if (!img) return;
    delete reinterpret_cast<Video::ImageBuffer*>(img);   // B.3.x bridge (slot 0, free)
}

}  // namespace

// ============================================================
// Public entry — paint the minimap terrain (port of FUN_1400a5140).
// Signature matches fn_ImageCapture_DrawCheck_t in gameupdateloop.cpp.
// ============================================================
extern "C" void Minimap_PaintTerrain(void* minimap, void* context,
                                     void* worldGeom, void* sunLight) {
    if (!minimap || !context) return;
    if (!ResolveFuncs()) return;

    static bool s_trace = Opt().mapTrace != nullptr;

    uint8_t* map = reinterpret_cast<uint8_t*>(minimap);
    int width = *reinterpret_cast<int*>(map + 0x04);
    if (width <= 0) {
        if (s_trace) printf("[MinimapPaint] width<=0 (%d) — skip\n", width);
        return;
    }

    // --- Off-screen color + depth surfaces (temporary render target) ---
    void* colorBuf = MakeRenderTargetBuffer(width, /*usage*/0, /*format*/5); // ARGB render target
    void* depthBuf = MakeRenderTargetBuffer(width, /*usage*/1, /*format*/7); // depth

    void* renderBuffer = System::MemoryAllocator::MemAlloc(0x30, 8);
    if (renderBuffer) s_RenderBuffer_Ctor(renderBuffer);
    bool rbOk = reinterpret_cast<Video::RenderBuffer*>(renderBuffer)->Create(
        reinterpret_cast<const Video::ImageBuffer*>(colorBuf),
        reinterpret_cast<const Video::ImageBuffer*>(depthBuf));   // B.3.5 bridge
    (void)rbOk;

    // --- Orthographic top-down camera ---
    const float half    = (float)width * 0.5f;
    const float negHalf = -half;
    const float fNear   = 0.0f;
    const float fFar    = 512.0f;    // DAT_140172240

    alignas(16) uint8_t frustum[256];
    void* frustumRet = s_Frustum_Ortho_Ctor(frustum, /*ortho*/true, &fNear, &fFar,
                                             &negHalf, &half, &half, &negHalf);

    // Transform3::Identity is a static that returns a Transform3 BY VALUE (48 bytes)
    // via a hidden return buffer in RCX — the engine_funcs typedef is nullary, so call
    // it through a retbuf-aware signature to avoid writing the identity to a garbage
    // stack slot (which corrupts adjacent buffers -> NaN projection / access violation).
    Math::Transform3 identity = Math::Transform3::Identity();

    alignas(16) uint8_t camera[256];
    fn_Graphics_Camera_Ctor(camera, frustumRet, &identity, /*isPerspective*/true);

    // Look straight down (yaw 0, pitch -90deg), positioned at world (0,0,100).
    const float yaw   = 0.0f;
    const float pitch = -90.0f;      // DAT_14017229c
    const float camZ  = 100.0f;      // DAT_140172208

    Math::Quaternion quat = Math::Quaternion::FromYawPitch(yaw, pitch);

    // Math::Transform = quaternion (xyzw) + position (xyz).
    alignas(16) uint8_t transform[32];
    std::memcpy(transform, &quat, 16);
    *reinterpret_cast<float*>(transform + 0x10) = 0.0f;   // pos.x
    *reinterpret_cast<float*>(transform + 0x14) = 0.0f;   // pos.y
    *reinterpret_cast<float*>(transform + 0x18) = camZ;   // pos.z

    Math::Transform3 transform3 =
        reinterpret_cast<const Math::Transform*>(transform)->AsTransform3();
    reinterpret_cast<Graphics::Camera*>(camera)->SnapCameraToWorld(transform3);   // B.3.5 bridge

    // View matrix from the camera's cameraToWorld transform (cam+0x64); projection
    // matrix is stored at cam+0x94 — exactly the raw reads FUN_1400a5140 inlines.
    Math::Matrix viewMatrix =
        reinterpret_cast<const Math::Transform3*>(camera + 0x64)->AsMatrix();
    reinterpret_cast<Video::Context*>(context)->SetCamera(
        viewMatrix, *reinterpret_cast<const Math::Matrix*>(camera + 0x94));   // B.3.5 bridge

    // Viewport {x=0, y=0, w=width, h=width}.
    int32_t viewport[4] = { 0, 0, width, width };
    reinterpret_cast<Video::Context*>(context)->SetViewport(
        *reinterpret_cast<const Video::Viewport*>(viewport));   // B.3.5 bridge

    const float clearDepth = g_fUVScale ? *g_fUVScale : 1.0f; // DAT_14017214c == 1.0
    reinterpret_cast<Video::Context*>(context)->Clear(1, 0xff000000, clearDepth, 0);   // B.3.5 bridge

    // Redirect rendering to the off-screen buffer (saving the current + default).
    uint8_t* ctx = reinterpret_cast<uint8_t*>(context);
    void* savedRenderBuffer  = *reinterpret_cast<void**>(ctx + 0x28);
    void* savedDefaultBuffer = *reinterpret_cast<void**>(ctx + 0x2138);

    alignas(16) uint8_t renderer[256];
    alignas(16) uint8_t scene[256];
    fn_Graphics_Renderer_Ctor(renderer);
    fn_Graphics_Scene_Ctor(scene);

    reinterpret_cast<Video::Context*>(context)->SetDefaultRenderBuffer(renderBuffer);   // B.3.5 bridge
    reinterpret_cast<Video::Context*>(context)->SetRenderBuffer(renderBuffer);          // B.3.5 bridge
    reinterpret_cast<Video::Context*>(context)->Clear(7, 0xffff00ff, clearDepth, 0);    // B.3.5 bridge

    // Fog + context flags matching the retail minimap render setup.
    float fogColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const float fogStart = 1000.0f;  // DAT_140172248
    const float fogEnd   = 1001.0f;  // DAT_14017224c
    reinterpret_cast<Video::Context*>(context)->SetFog(
        fogStart, fogEnd, *reinterpret_cast<const Math::Vector4*>(fogColor));   // B.3.5 bridge

    *reinterpret_cast<float*>(ctx + 0x3b8) = 0.0f;
    *reinterpret_cast<float*>(ctx + 0x3bc) = 0.0f;
    *reinterpret_cast<float*>(ctx + 0x3c0) = clearDepth;
    *reinterpret_cast<float*>(ctx + 0x3c4) = 0.0f;
    *reinterpret_cast<float*>(ctx + 0x288) = 0.0f;
    *reinterpret_cast<float*>(ctx + 0x28c) = 0.0f;
    *reinterpret_cast<float*>(ctx + 0x290) = clearDepth;
    *reinterpret_cast<float*>(ctx + 0x294) = 0.0f;
    *reinterpret_cast<uint32_t*>(ctx + 0x10) |= 0x10800;

    *reinterpret_cast<int32_t*>(renderer + 0x3c) = 0;   // renderer shadow flag off

    // Temporarily clear VideoConfig+0x34 (disables a render feature during capture).
    uint8_t* videoConfig = reinterpret_cast<uint8_t*>(::Video::Configuration::_singleton);
    int32_t savedCfg = 0;
    if (videoConfig) {
        savedCfg = *reinterpret_cast<int32_t*>(videoConfig + 0x34);
        *reinterpret_cast<int32_t*>(videoConfig + 0x34) = 0;
    }

    // Render the terrain (worldGeom) + sun/lighting (sunLight) top-down.
    reinterpret_cast<Graphics::Scene*>(scene)->AddGlobalObject(
        reinterpret_cast<Graphics::Geometry*>(worldGeom));   // B.3.5 bridge
    reinterpret_cast<Graphics::Scene*>(scene)->AddGlobalObject(
        reinterpret_cast<Graphics::Geometry*>(sunLight));    // B.3.5 bridge
    reinterpret_cast<Graphics::Scene*>(scene)->Clean();      // B.3.5 bridge
    // B.3.5 bridge — CONVERTED in batch K1: `Graphics::Renderer::Draw` is DEFINED now
    // (runtime/src/graphics/wave27_lane_gfxbig.cpp:567 is the door delegating to it).
    reinterpret_cast<Graphics::Renderer*>(renderer)->Draw(
        *reinterpret_cast<Video::Context*>(context), 0.0f,
        *reinterpret_cast<const Graphics::Scene*>(scene),
        *reinterpret_cast<const Graphics::Camera*>(camera));   // B.3.5 bridge
    reinterpret_cast<Graphics::Scene*>(scene)->RemoveGlobalObject(
        reinterpret_cast<Graphics::Geometry*>(sunLight));    // B.3.5 bridge
    reinterpret_cast<Graphics::Scene*>(scene)->RemoveGlobalObject(
        reinterpret_cast<Graphics::Geometry*>(worldGeom));   // B.3.5 bridge

    if (videoConfig)
        *reinterpret_cast<int32_t*>(videoConfig + 0x34) = savedCfg;

    // Restore the previous render buffers.
    reinterpret_cast<Video::Context*>(context)->SetRenderBuffer(savedRenderBuffer);          // B.3.5 bridge
    reinterpret_cast<Video::Context*>(context)->SetDefaultRenderBuffer(savedDefaultBuffer);  // B.3.5 bridge

    // Read the rendered color back into the minimap terrain pixel array (+0x18).
    void* terrainPixels = *reinterpret_cast<void**>(map + 0x18);
    if (colorBuf && terrainPixels)
        // B.3.5 bridge — ?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z.
        reinterpret_cast<Video::ImageBuffer*>(colorBuf)->CopyFromTexture(
            terrainPixels, width, width);   // B.3.5 bridge

    // Free the temporary render surfaces.
    if (renderBuffer) {
        s_RenderBuffer_Dtor(renderBuffer);
        System::MemoryAllocator::MemFree(renderBuffer);
    }
    FreeImageBuffer(colorBuf);
    FreeImageBuffer(depthBuf);

    // Create the display GPU buffers (minimap+0x08/+0x10) and upload both pixel
    // arrays — shared tail with minimap_upload.cpp (ports FUN_1400a4e80 + uploads).
    Minimap_CreateAndUpload(minimap);

    reinterpret_cast<Video::Context*>(context)->Reset();   // B.3.5 bridge
    s_Scene_Dtor(scene);
    s_Renderer_Dtor(renderer);

    if (s_trace)
        printf("[MinimapPaint] painted terrain: width=%d colorBuf=%p terrainPixels=%p "
               "display(+0x08)=%p\n", width, colorBuf, terrainPixels,
               *reinterpret_cast<void**>(map + 0x08));
}
