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
// The buffer is a real Video::ImageBuffer (the exe's table 0x14015c8c0 is the
// class's own); CopyToTexture and Device Lock/Unlock are members;
// see code/Game/Game/serialize_image_upload.cpp for the sibling single-buffer
// upload this mirrors.

// ENDGAME B.3.5 batch K1: the runtime type headers come FIRST. Neither reaches
// runtime/types/volume/Volume.h, but the house rule (a runtime type header ahead of
// resolve.h's <windows.h>, whose <wingdi.h> `Rectangle()` hides Volume.h's global
// `struct Rectangle`) is kept so a later include change cannot break it silently.
#include "video/ImageBuffer.h"
#include "video/Device.h"
#include "engine_funcs.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
#include "options.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

// ENDGAME B.3.5 batch K1: the `fn_ImageBuffer_Create_t` slot typedef and the
// `kCreateSlot` (+0x48) offset went with the hand-inlined slot read below, which is a
// virtual call on `Video::ImageBuffer` now.
constexpr int kImageBufferSize  = 0x68;
constexpr int kImageBufferAlign = 8;

// Allocate + Create one ImageBuffer, store it at mapObj+dstOff, then upload the
// raw pixel array at mapObj+srcOff into it. Returns the buffer (or null).
void* CreateAndUploadOne(uint8_t* map, int dstOff, int srcOff, int width,
                         const char* tag, bool trace) {
    void* img = System::MemoryAllocator::MemAlloc(kImageBufferSize, kImageBufferAlign);
    if (!img) { if (trace) printf("[Minimap] %s: MemAlloc(0x68) failed\n", tag); return nullptr; }

    ::new (img) Video::ImageBuffer();   // B.3.9 EXA: the real class (exe table 0x14015c8c0 is its own)
    *reinterpret_cast<void**>(map + dstOff) = img;

    bool ok = reinterpret_cast<Video::ImageBuffer*>(img)->Create(
        /*imageType*/0, /*usage*/2, /*format*/5, /*mip*/0u,
        width, width, /*filterMode*/1, 1, 1, 1, nullptr);   // B.3.5 bridge

    // Upload the raw pixel array (mapObj+srcOff) into the texture.
    void* pixels = *reinterpret_cast<void**>(map + srcOff);
    void* device = ::Video::Device::_singleton;
    reinterpret_cast<Video::Device*>(device)->Lock();   // B.3.5 bridge
    if (pixels)
        reinterpret_cast<Video::ImageBuffer*>(img)
            ->CopyToTexture(pixels, width, width);     // B.3.5 bridge
    ::Video::Device::_singleton
        ->Unlock();                                    // B.3.5 bridge

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

    // ENDGAME B.3.5 batch K1: the CopyToTexture / Device::Lock / Device::Unlock terms
    // went with their call sites; B.3.9 EXA took the ImageBuffer ctor-door and
    // exe-table terms (the buffer is constructed as the real class).
    if (!System::MemoryAllocator::_singleton ||
        !::Video::Device::_singleton) {
        static bool warned = false;
        if (!warned) {
            printf("[Minimap] primitives unresolved — skipping upload "
                   "(MemAlloc=%p dev=%p)\n",
                   (void*)fn_MemAlloc,
                   ::Video::Device::_singleton);
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
