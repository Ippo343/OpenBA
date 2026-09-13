// code/Game/Game/minimap_upload.cpp
// GUI Keystone F — map-render bring-up.
//
// The minimap subsystem lives at gameWorld+0x148. Its struct:
//   +0x00  byte   dirty flag
//   +0x04  int    width (== map grid edge, e.g. 256)
//   +0x08  ImageBuffer*  terrain GPU texture   (built here)
//   +0x10  ImageBuffer*  overlay GPU texture   (built here)
//   +0x18  uint32*       terrain raw pixel array (width*width, ARGB)
//   +0x20  int           terrain array count/cap
//   +0x28  uint32*       overlay raw pixel array (width*width, ARGB)
//   +0x30  int           overlay array count/cap
//
// In the retail game the two GPU ImageBuffers are created + uploaded on the
// save/load round-trip's LOAD half — Game::Serialize (0x140095a90) calls the
// minimap Serialize (FUN_1400a57f0 @ 0x1400a57f0) whose IsLoading tail runs:
//     FUN_1400a4e80(mapObj)                       // create both ImageBuffers
//     Device::Lock;  CopyToTexture(+0x08, +0x18, w, w);  Device::Unlock
//     Device::Lock;  CopyToTexture(+0x10, +0x28, w, w);  Device::Unlock
// The standalone builds the world DIRECTLY (no serialize round-trip), so the
// buffers never get created and MapUI::SetupImage reads null → the minimap
// can't render. This helper ports FUN_1400a4e80's buffer creation + the
// CopyToTexture upload tail so the raw pixel arrays reach the GPU.
//
// Buffer-creation args verified against FUN_1400a4e80 disasm:
//     Create(imageType=0, usage=2, format=5, mipLevels=0, w, h,
//            filterMode=1, addrU=1, addrV=1, addrW=1, mipData=null)
// (Note filterMode=1 here vs 2 in TradeWork_sub_c780 — matched to the minimap.)
//
// All primitives (ImageBuffer ctor/CopyToTexture, Device Lock/Unlock, the shared
// NoiseUI ImageBuffer vtable @ 0x14015c8c0) are already wired in engine_funcs;
// see code/Game/Game/serialize_image_upload.cpp for the sibling single-buffer
// upload this mirrors.

#include "engine_funcs.h"
#include "options.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// ImageBuffer::Create — dispatched through the custom vtable's +0x48 slot
// (index 9). Signature copied verbatim from serialize_image_upload.cpp.
typedef bool (*fn_ImageBuffer_Create_t)(void* self, int imageType, int usage,
                                        int format, unsigned int mipLevels,
                                        int width, int height, int filterMode,
                                        int addrU, int addrV, int addrW,
                                        void* mipData);

constexpr int kImageBufferSize  = 0x68;
constexpr int kImageBufferAlign = 8;
constexpr int kCreateSlot       = 0x48;   // bytes into vtable

// Allocate + Create one ImageBuffer, store it at mapObj+dstOff, then upload the
// raw pixel array at mapObj+srcOff into it. Returns the buffer (or null).
void* CreateAndUploadOne(uint8_t* map, int dstOff, int srcOff, int width,
                         const char* tag, bool trace) {
    void* img = fn_MemAlloc(g_pMemAllocSingleton, kImageBufferSize, kImageBufferAlign);
    if (!img) { if (trace) printf("[Minimap] %s: MemAlloc(0x68) failed\n", tag); return nullptr; }

    fn_ImageBuffer_Ctor(img);
    *reinterpret_cast<uintptr_t*>(img) = g_NoiseUI_ImageBufferVtable;  // override vtable
    *reinterpret_cast<void**>(map + dstOff) = img;

    uintptr_t vtable = *reinterpret_cast<uintptr_t*>(img);
    fn_ImageBuffer_Create_t fnCreate =
        *reinterpret_cast<fn_ImageBuffer_Create_t*>(vtable + kCreateSlot);
    if (!fnCreate) { if (trace) printf("[Minimap] %s: vtable+0x48 (Create) null\n", tag); return img; }

    bool ok = fnCreate(img, /*imageType*/0, /*usage*/2, /*format*/5, /*mip*/0,
                       width, width, /*filterMode*/1, 1, 1, 1, nullptr);

    // Upload the raw pixel array (mapObj+srcOff) into the texture.
    void* pixels = *reinterpret_cast<void**>(map + srcOff);
    void* device = *g_pVideoDeviceSingleton;
    fn_VideoDevice_Lock(device);
    if (pixels) fn_ImageBuffer_CopyToTexture(img, pixels, width, width);
    fn_VideoDevice_Unlock(*g_pVideoDeviceSingleton);

    if (trace)
        printf("[Minimap] %s: img=%p createOk=%d pixels=%p w=%d uploaded\n",
               tag, img, (int)ok, pixels, width);
    return img;
}

}  // namespace

// ============================================================
// Public entry — create + upload both minimap ImageBuffers.
// ============================================================
// mapObj = gameWorld+0x148. Idempotent: if a buffer already exists it re-uploads
// (matches the retail Serialize which re-creates on every load). Safe no-op if
// primitives are unresolved or width is invalid.
extern "C" void Minimap_CreateAndUpload(void* mapObj) {
    if (!mapObj) return;

    static bool s_trace = Opt().mapTrace != nullptr;

    if (!fn_MemAlloc || !g_pMemAllocSingleton || !fn_ImageBuffer_Ctor ||
        !fn_ImageBuffer_CopyToTexture || !fn_VideoDevice_Lock ||
        !fn_VideoDevice_Unlock || g_NoiseUI_ImageBufferVtable == 0 ||
        !g_pVideoDeviceSingleton || !*g_pVideoDeviceSingleton) {
        static bool warned = false;
        if (!warned) {
            printf("[Minimap] primitives unresolved — skipping upload "
                   "(MemAlloc=%p Ctor=%p CopyToTex=%p Lock=%p vtbl=%p dev=%p)\n",
                   (void*)fn_MemAlloc, (void*)fn_ImageBuffer_Ctor,
                   (void*)fn_ImageBuffer_CopyToTexture, (void*)fn_VideoDevice_Lock,
                   (void*)g_NoiseUI_ImageBufferVtable,
                   g_pVideoDeviceSingleton ? *g_pVideoDeviceSingleton : nullptr);
            fflush(stdout);
            warned = true;
        }
        return;
    }

    uint8_t* map = reinterpret_cast<uint8_t*>(mapObj);
    int width = *reinterpret_cast<int*>(map + 0x04);
    if (width <= 0) {
        if (s_trace) printf("[Minimap] width<=0 (%d) — skipping\n", width);
        return;
    }

    // Terrain: array +0x18 -> buffer +0x08. Overlay: array +0x28 -> buffer +0x10.
    CreateAndUploadOne(map, 0x08, 0x18, width, "terrain", s_trace);
    CreateAndUploadOne(map, 0x10, 0x28, width, "overlay", s_trace);
}
