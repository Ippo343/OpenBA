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

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Shared with minimap_upload.cpp — creates the display GPU buffers at minimap
// +0x08/+0x10 (ports FUN_1400a4e80) and uploads +0x18/+0x28 into them.
extern "C" void Minimap_CreateAndUpload(void* mapObj);

namespace {

// --- New Runtime primitives (not in engine_funcs; self-resolved once) ---
typedef void* (*fn_RenderBuffer_Ctor_t)(void* self);
typedef bool  (*fn_RenderBuffer_Create_t)(void* self, void* colorBuf, void* depthBuf);
typedef void  (*fn_RenderBuffer_Dtor_t)(void* self);
typedef void* (*fn_Frustum_Ortho_Ctor_t)(void* self, bool ortho, const void* near_,
                                          const void* far_, const void* s3,
                                          const void* s4, const void* s5, const void* s6);
typedef void* (*fn_Quaternion_FromYawPitch_t)(void* retQuat, const void* yaw, const void* pitch);
typedef void  (*fn_Camera_SnapToWorld_t)(void* cam, void* transform3);
typedef void* (*fn_Transform3_AsMatrix_t)(void* self, void* retMatrix);
typedef void  (*fn_Context_SetCamera_t)(void* ctx, void* viewMatrix, void* projMatrix);
typedef void  (*fn_Context_SetViewport_t)(void* ctx, void* viewport);
typedef void  (*fn_Context_Clear_t)(void* ctx, unsigned flags, unsigned color, float depth, int stencil);
typedef void  (*fn_Context_SetRenderBuffer_t)(void* ctx, void* rb);
typedef void  (*fn_Context_Reset_t)(void* ctx);
typedef void  (*fn_Renderer_Dtor_t)(void* self);
typedef void  (*fn_Scene_Dtor_t)(void* self);
typedef void  (*fn_Scene_RemoveGlobalObject_t)(void* scene, void* geometry);
// ImageBuffer::Create dispatched through the custom vtable's +0x48 slot.
typedef bool  (*fn_ImageBuffer_Create_t)(void* self, int imageType, int usage, int format,
                                         unsigned mipLevels, int width, int height,
                                         int filterMode, int addrU, int addrV, int addrW, void* mipData);

fn_RenderBuffer_Ctor_t         s_RenderBuffer_Ctor      = nullptr;
fn_RenderBuffer_Create_t       s_RenderBuffer_Create    = nullptr;
fn_RenderBuffer_Dtor_t         s_RenderBuffer_Dtor      = nullptr;
fn_Frustum_Ortho_Ctor_t        s_Frustum_Ortho_Ctor     = nullptr;
fn_Quaternion_FromYawPitch_t   s_Quaternion_FromYawPitch = nullptr;
fn_Camera_SnapToWorld_t        s_Camera_SnapToWorld     = nullptr;
fn_Transform3_AsMatrix_t       s_Transform3_AsMatrix    = nullptr;
fn_Context_SetCamera_t         s_Context_SetCamera      = nullptr;
fn_Context_SetViewport_t       s_Context_SetViewport    = nullptr;
fn_Context_Clear_t             s_Context_Clear          = nullptr;
fn_Context_SetRenderBuffer_t   s_Context_SetRenderBuffer = nullptr;
fn_Context_SetRenderBuffer_t   s_Context_SetDefaultRenderBuffer = nullptr;
fn_Context_Reset_t             s_Context_Reset          = nullptr;
fn_Renderer_Dtor_t             s_Renderer_Dtor          = nullptr;
fn_Scene_Dtor_t                s_Scene_Dtor             = nullptr;
fn_Scene_RemoveGlobalObject_t  s_Scene_RemoveGlobalObject = nullptr;

bool s_resolved = false;
bool s_ok       = false;

bool ResolveFuncs() {
    if (s_resolved) return s_ok;
    s_resolved = true;

    s_RenderBuffer_Ctor    = (fn_RenderBuffer_Ctor_t)   ResolveExport("??0RenderBuffer@Video@@QEAA@XZ");
    s_RenderBuffer_Create  = (fn_RenderBuffer_Create_t) ResolveExport("?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z");
    s_RenderBuffer_Dtor    = (fn_RenderBuffer_Dtor_t)   ResolveExport("??1RenderBuffer@Video@@QEAA@XZ");
    s_Frustum_Ortho_Ctor   = (fn_Frustum_Ortho_Ctor_t)  ResolveExport("??0Frustum@Volume@@QEAA@_NAEBVScalar@Math@@11111@Z");
    s_Quaternion_FromYawPitch = (fn_Quaternion_FromYawPitch_t) ResolveExport("?FromYawPitch@Quaternion@Math@@SA?AV12@AEBVScalar@2@0@Z");
    s_Camera_SnapToWorld   = (fn_Camera_SnapToWorld_t)  ResolveExport("?SnapCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z");
    s_Transform3_AsMatrix  = (fn_Transform3_AsMatrix_t) ResolveExport("?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ");
    s_Context_SetCamera    = (fn_Context_SetCamera_t)   ResolveExport("?SetCamera@Context@Video@@QEAAXAEBVMatrix@Math@@0@Z");
    s_Context_SetViewport  = (fn_Context_SetViewport_t) ResolveExport("?SetViewport@Context@Video@@QEAAXAEBVViewport@2@@Z");
    s_Context_Clear        = (fn_Context_Clear_t)       ResolveExport("?Clear@Context@Video@@QEAAXIIMH@Z");
    s_Context_SetRenderBuffer = (fn_Context_SetRenderBuffer_t) ResolveExport("?SetRenderBuffer@Context@Video@@QEAAXPEBVRenderBuffer@2@@Z");
    s_Context_SetDefaultRenderBuffer = (fn_Context_SetRenderBuffer_t) ResolveExport("?SetDefaultRenderBuffer@Context@Video@@QEAAXPEAVRenderBuffer@2@@Z");
    s_Context_Reset        = (fn_Context_Reset_t)       ResolveExport("?Reset@Context@Video@@QEAAXXZ");
    s_Renderer_Dtor        = (fn_Renderer_Dtor_t)       ResolveExport("??1Renderer@Graphics@@QEAA@XZ");
    s_Scene_Dtor           = (fn_Scene_Dtor_t)          ResolveExport("??1Scene@Graphics@@QEAA@XZ");
    s_Scene_RemoveGlobalObject = (fn_Scene_RemoveGlobalObject_t) ResolveExport("?RemoveGlobalObject@Scene@Graphics@@QEAAXPEAVGeometry@2@@Z");

    s_ok = s_RenderBuffer_Ctor && s_RenderBuffer_Create && s_RenderBuffer_Dtor &&
           s_Frustum_Ortho_Ctor && s_Quaternion_FromYawPitch && s_Camera_SnapToWorld &&
           s_Transform3_AsMatrix && s_Context_SetCamera && s_Context_SetViewport &&
           s_Context_Clear && s_Context_SetRenderBuffer && s_Context_SetDefaultRenderBuffer &&
           s_Context_Reset && s_Renderer_Dtor && s_Scene_Dtor && s_Scene_RemoveGlobalObject &&
           // engine_funcs primitives this port relies on:
           fn_MemAlloc && g_pMemAllocSingleton && fn_MemFreeStatic &&
           fn_ImageBuffer_Ctor && g_NoiseUI_ImageBufferVtable && fn_ImageBuffer_CopyFromTexture &&
           fn_Math_Transform3_Identity && fn_Graphics_Camera_Ctor && fn_Transform_AsTransform3 &&
           fn_Video_Context_SetFog && fn_Graphics_Renderer_Ctor && fn_Graphics_Scene_Ctor &&
           fn_Graphics_Renderer_Draw && fn_Graphics_Scene_AddGlobalObject && fn_Graphics_Scene_Clean &&
           g_pVideoConfigSingleton;

    if (!s_ok) {
        printf("[MinimapPaint] primitives unresolved — terrain paint disabled\n"
               "  RB(ctor=%p create=%p dtor=%p) Frustum=%p Quat=%p Snap=%p AsMat=%p\n"
               "  Ctx(cam=%p vp=%p clr=%p srb=%p sdrb=%p reset=%p) Rdtor=%p Sdtor=%p Rmv=%p vcfg=%p\n",
               (void*)s_RenderBuffer_Ctor, (void*)s_RenderBuffer_Create, (void*)s_RenderBuffer_Dtor,
               (void*)s_Frustum_Ortho_Ctor, (void*)s_Quaternion_FromYawPitch, (void*)s_Camera_SnapToWorld,
               (void*)s_Transform3_AsMatrix, (void*)s_Context_SetCamera, (void*)s_Context_SetViewport,
               (void*)s_Context_Clear, (void*)s_Context_SetRenderBuffer, (void*)s_Context_SetDefaultRenderBuffer,
               (void*)s_Context_Reset, (void*)s_Renderer_Dtor, (void*)s_Scene_Dtor,
               (void*)s_Scene_RemoveGlobalObject, (void*)g_pVideoConfigSingleton);
        fflush(stdout);
    }
    return s_ok;
}

// Allocate + construct an ImageBuffer with the NoiseUI vtable, then Create it as a
// render-target (color) / depth surface. Returns the buffer or null.
void* MakeRenderTargetBuffer(int width, int usage, int format) {
    void* img = fn_MemAlloc(g_pMemAllocSingleton, 0x68, 8);
    if (!img) return nullptr;
    fn_ImageBuffer_Ctor(img);
    *reinterpret_cast<uintptr_t*>(img) = g_NoiseUI_ImageBufferVtable;
    uintptr_t vt = *reinterpret_cast<uintptr_t*>(img);
    fn_ImageBuffer_Create_t create = *reinterpret_cast<fn_ImageBuffer_Create_t*>(vt + 0x48);
    if (create)
        create(img, /*imageType*/0, usage, format, /*mip*/0, width, width,
               /*filterMode*/1, 1, 1, 1, nullptr);
    return img;
}

// Free an ImageBuffer via its scalar-deleting destructor (vtable[0](self, 1)).
void FreeImageBuffer(void* img) {
    if (!img) return;
    typedef void (*ScalarDtor_t)(void* self, int freeFlag);
    ScalarDtor_t dtor = *reinterpret_cast<ScalarDtor_t*>(*reinterpret_cast<uintptr_t*>(img));
    if (dtor) dtor(img, 1);
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

    static bool s_trace = std::getenv("BANISHED_MAP_TRACE") != nullptr;

    uint8_t* map = reinterpret_cast<uint8_t*>(minimap);
    int width = *reinterpret_cast<int*>(map + 0x04);
    if (width <= 0) {
        if (s_trace) printf("[MinimapPaint] width<=0 (%d) — skip\n", width);
        return;
    }

    // --- Off-screen color + depth surfaces (temporary render target) ---
    void* colorBuf = MakeRenderTargetBuffer(width, /*usage*/0, /*format*/5); // ARGB render target
    void* depthBuf = MakeRenderTargetBuffer(width, /*usage*/1, /*format*/7); // depth

    void* renderBuffer = fn_MemAlloc(g_pMemAllocSingleton, 0x30, 8);
    if (renderBuffer) s_RenderBuffer_Ctor(renderBuffer);
    bool rbOk = s_RenderBuffer_Create(renderBuffer, colorBuf, depthBuf);
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
    typedef void* (*fn_Transform3_Identity_ret_t)(void* retbuf);
    alignas(16) uint8_t identityBuf[64];
    ((fn_Transform3_Identity_ret_t)fn_Math_Transform3_Identity)(identityBuf);
    void* identity = identityBuf;

    alignas(16) uint8_t camera[256];
    fn_Graphics_Camera_Ctor(camera, frustumRet, identity, /*isPerspective*/true);

    // Look straight down (yaw 0, pitch -90deg), positioned at world (0,0,100).
    const float yaw   = 0.0f;
    const float pitch = -90.0f;      // DAT_14017229c
    const float camZ  = 100.0f;      // DAT_140172208

    alignas(16) uint8_t quat[16];
    s_Quaternion_FromYawPitch(quat, &yaw, &pitch);

    // Math::Transform = quaternion (xyzw) + position (xyz).
    alignas(16) uint8_t transform[32];
    std::memcpy(transform, quat, 16);
    *reinterpret_cast<float*>(transform + 0x10) = 0.0f;   // pos.x
    *reinterpret_cast<float*>(transform + 0x14) = 0.0f;   // pos.y
    *reinterpret_cast<float*>(transform + 0x18) = camZ;   // pos.z

    alignas(16) uint8_t transform3[64];
    fn_Transform_AsTransform3(transform, transform3);
    s_Camera_SnapToWorld(camera, transform3);

    // View matrix from the camera's cameraToWorld transform (cam+0x64); projection
    // matrix is stored at cam+0x94 — exactly the raw reads FUN_1400a5140 inlines.
    alignas(16) uint8_t viewMatrix[64];
    s_Transform3_AsMatrix((void*)(camera + 0x64), viewMatrix);
    s_Context_SetCamera(context, viewMatrix, camera + 0x94);

    // Viewport {x=0, y=0, w=width, h=width}.
    int32_t viewport[4] = { 0, 0, width, width };
    s_Context_SetViewport(context, viewport);

    const float clearDepth = g_fUVScale ? *g_fUVScale : 1.0f; // DAT_14017214c == 1.0
    s_Context_Clear(context, 1, 0xff000000, clearDepth, 0);

    // Redirect rendering to the off-screen buffer (saving the current + default).
    uint8_t* ctx = reinterpret_cast<uint8_t*>(context);
    void* savedRenderBuffer  = *reinterpret_cast<void**>(ctx + 0x28);
    void* savedDefaultBuffer = *reinterpret_cast<void**>(ctx + 0x2138);

    alignas(16) uint8_t renderer[256];
    alignas(16) uint8_t scene[256];
    fn_Graphics_Renderer_Ctor(renderer);
    fn_Graphics_Scene_Ctor(scene);

    s_Context_SetDefaultRenderBuffer(context, renderBuffer);
    s_Context_SetRenderBuffer(context, renderBuffer);
    s_Context_Clear(context, 7, 0xffff00ff, clearDepth, 0);

    // Fog + context flags matching the retail minimap render setup.
    float fogColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const float fogStart = 1000.0f;  // DAT_140172248
    const float fogEnd   = 1001.0f;  // DAT_14017224c
    fn_Video_Context_SetFog(context, fogStart, fogEnd, fogColor);

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
    uint8_t* videoConfig = *reinterpret_cast<uint8_t**>(g_pVideoConfigSingleton);
    int32_t savedCfg = 0;
    if (videoConfig) {
        savedCfg = *reinterpret_cast<int32_t*>(videoConfig + 0x34);
        *reinterpret_cast<int32_t*>(videoConfig + 0x34) = 0;
    }

    // Render the terrain (worldGeom) + sun/lighting (sunLight) top-down.
    fn_Graphics_Scene_AddGlobalObject(scene, worldGeom);
    fn_Graphics_Scene_AddGlobalObject(scene, sunLight);
    fn_Graphics_Scene_Clean(scene);
    fn_Graphics_Renderer_Draw(renderer, context, 0.0f, scene, camera);
    s_Scene_RemoveGlobalObject(scene, sunLight);
    s_Scene_RemoveGlobalObject(scene, worldGeom);

    if (videoConfig)
        *reinterpret_cast<int32_t*>(videoConfig + 0x34) = savedCfg;

    // Restore the previous render buffers.
    s_Context_SetRenderBuffer(context, savedRenderBuffer);
    s_Context_SetDefaultRenderBuffer(context, savedDefaultBuffer);

    // Read the rendered color back into the minimap terrain pixel array (+0x18).
    void* terrainPixels = *reinterpret_cast<void**>(map + 0x18);
    if (colorBuf && terrainPixels)
        fn_ImageBuffer_CopyFromTexture(colorBuf, terrainPixels, width, width);

    // Free the temporary render surfaces.
    if (renderBuffer) {
        s_RenderBuffer_Dtor(renderBuffer);
        fn_MemFreeStatic(renderBuffer);
    }
    FreeImageBuffer(colorBuf);
    FreeImageBuffer(depthBuf);

    // Create the display GPU buffers (minimap+0x08/+0x10) and upload both pixel
    // arrays — shared tail with minimap_upload.cpp (ports FUN_1400a4e80 + uploads).
    Minimap_CreateAndUpload(minimap);

    s_Context_Reset(context);
    s_Scene_Dtor(scene);
    s_Renderer_Dtor(renderer);

    if (s_trace)
        printf("[MinimapPaint] painted terrain: width=%d colorBuf=%p terrainPixels=%p "
               "display(+0x08)=%p\n", width, colorBuf, terrainPixels,
               *reinterpret_cast<void**>(map + 0x08));
}
