// code/Game/Game/serialize_image_upload.cpp
//
// M3.5 Path B session 7 — image-buffer heightmap upload helper.
//
// One exe-internal helper:
//   sub_c780  @ 0x1400ac780  — TradeWork_impl_sub_c780  (heightmap → texture upload)
//
// Called from Geometry::Serialize step 14 (the final IsLoading-only block, see
// notes/M35_geom_serialize_findings.md lines 102-110) AFTER the VertexBuffer /
// IndexBuffer / heightmap deserialization is complete. Its job is to take the
// freshly-loaded heightmap raw bytes at self+0x140 and turn them into a
// Video::ImageBuffer at self+0x148 that downstream rendering can sample.
//
// Body shape (verified via Ghidra disassembly @ 0x1400ac780, ~30 instructions):
//   1. img = MemAlloc(0x68, 8)                         // ImageBuffer size
//   2. ImageBuffer::ImageBuffer(img)                   // Runtime DLL ctor
//   3. *(void**)img = &g_NoiseUI_ImageBufferVtable     // override vtable
//   4. self+0x148 = img                                // store on self
//   5. img->vtbl[0x48 / 8](img,                        // index 9: Create
//          0,           // ImageType
//          2,           // Usage
//          5,           // Format
//          0,           // mipLevels (unsigned)
//          width,       // (= self+0xC0)
//          width,       // height (square)
//          2,           // FilterMode
//          1, 1, 1,     // AddressMode x3
//          0)           // Array<Array<byte>>* mipData (null)
//   6. Device::Lock( *Device::_singleton )
//   7. ImageBuffer::CopyToTexture(img, self+0x140, width, width)
//   8. Device::Unlock( *Device::_singleton )
//
// The slot+0x48 dispatch is `Create` (?Create@ImageBuffer@Video@@UEAA_N
//   W4ImageType@12@W4Usage@12@W4Format@12@IHHW4FilterMode@12@W4AddressMode@12@
//   44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z`). Argument shape
// matches the established NoiseUI / ModDialog usage (see
// code/Game/Dialog/noiseui.cpp lines 290-299; code/Game/Dialog/moddialog.cpp
// lines 393-398) exactly — including the 12 args via stack overflow on Win64.
//
// Standalone-safety:
// - All Runtime primitives are resolved at startup in engine_funcs.cpp.
// - If any of them is null at runtime (Runtime DLL load failure), the helper
//   one-shot logs and returns early — no crash, no upload.
// - The vtable at 0x14015c8c0 is exe-resident; in DLL mode `g_NoiseUI_ImageBufferVtable`
//   already resolves via `ResolveAddr(Module::Exe, ...)`. In standalone mode,
//   address_registry.cpp's RegisterNoiseUIImageBufferVtable (added this session)
//   provides our own slot table that points at the same Runtime exports the
//   exe thunks did.
//
// This helper is NOT yet called from anywhere — Geometry::Serialize body is
// session 8. Session 7's only goal is that the helper exists, links, and is
// callable both in DLL mode (delegates to original) and standalone (executes
// our cleaned C body).

#include "engine_funcs.h"
#include "resolve.h"
#include <cstdint>
#include <cstdio>

namespace {

// ============================================================
// Vtable +0x48 dispatch — ImageBuffer::Create signature
// ============================================================
//
// Original disasm @ 0x14003aa20 is `JMP qword ptr [0x140151a88]`. The
// Runtime export it resolves to is:
//   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@
//      IHHW4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@
//      Collection@@@Collection@@@Z
//
// Eleven int32 / pointer args after `self`. The exe pushes them through
// shadow-space + stack overflow as observed at 0x1400ac7df-0x1400ac818:
//   [RSP+0x58] = 0  (Array<Array<byte>>* mipData)
//   [RSP+0x50] = 1  (AddressMode)
//   [RSP+0x48] = 1  (AddressMode)
//   [RSP+0x40] = 1  (AddressMode)
//   [RSP+0x38] = 2  (FilterMode)
//   [RSP+0x30] = w  (height)
//   [RSP+0x28] = w  (width — packed as uint at +0x24 too)
//   [RSP+0x20] = 0  (mipLevels)
//   R9d        = 5  (Format)
//   R8d        = 2  (Usage)
//   EDX        = 0  (ImageType)
//   RCX        = self
typedef bool (*fn_ImageBuffer_Create_t)(void* self,
                                         int   imageType,
                                         int   usage,
                                         int   format,
                                         unsigned int mipLevels,
                                         int   width,
                                         int   height,
                                         int   filterMode,
                                         int   addrU,
                                         int   addrV,
                                         int   addrW,
                                         void* mipData);

constexpr int kImageBufferSize        = 0x68;     // sizeof(Video::ImageBuffer)
constexpr int kImageBufferAlign       = 8;
constexpr int kCreateVtableSlotOffset = 0x48;     // bytes into vtable
constexpr int kImageBufferOffset      = 0x148;    // self+0x148 = ImageBuffer*
constexpr int kHeightmapBytesOffset   = 0x140;    // self+0x140 = raw bytes
constexpr int kGridSizeOffset         = 0xC0;     // self+0xC0 = grid edge

// ============================================================
// Helper — TradeWork_impl_sub_c780 (image-buffer heightmap upload)
// ============================================================
//
// param_1 (self) : Geometry container (RCX)
//
// Field reads:
//   self+0xC0 : int    width/height (square heightmap edge, e.g. 256)
//   self+0x140: void*  raw heightmap bytes (4 bytes/cell, deserialized in step 5)
// Field writes:
//   self+0x148: ImageBuffer*  newly-allocated buffer wired to NoiseUI vtable
extern "C" void TradeWork_impl_sub_c780_Body(void* self) {
    if (!self) return;

    if (!fn_MemAlloc || !g_pMemAllocSingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] MemAlloc/singleton unavailable — skipping image upload "
                   "(self=%p)\n", self);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    if (!fn_ImageBuffer_Ctor || !fn_ImageBuffer_CopyToTexture ||
        !fn_VideoDevice_Lock || !fn_VideoDevice_Unlock) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] Runtime primitives unresolved (Ctor=%p CopyToTex=%p "
                   "Lock=%p Unlock=%p) — skipping (cascade-safe no-op)\n",
                   (void*)fn_ImageBuffer_Ctor,
                   (void*)fn_ImageBuffer_CopyToTexture,
                   (void*)fn_VideoDevice_Lock,
                   (void*)fn_VideoDevice_Unlock);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    if (g_NoiseUI_ImageBufferVtable == 0) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] g_NoiseUI_ImageBufferVtable unresolved — "
                   "skipping image upload (cascade-safe no-op)\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    if (!g_pVideoDeviceSingleton || !*g_pVideoDeviceSingleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] Video::Device singleton not yet ready — "
                   "skipping image upload\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    uint8_t* selfBytes = reinterpret_cast<uint8_t*>(self);

    // (1) Allocate a 0x68-byte ImageBuffer with 8-byte alignment.
    void* img = fn_MemAlloc(g_pMemAllocSingleton, kImageBufferSize, kImageBufferAlign);
    if (!img) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] MemAlloc(0x68) failed — skipping image upload\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    // (2) Run the base ImageBuffer ctor (sets default vtable, zeros fields).
    fn_ImageBuffer_Ctor(img);

    // Diagnostic: dump ImageBuffer struct after ctor to verify
    // PlatformConstruct populated +0x50/+0x58 (linked-list ptrs) and +0x60
    // (ImageBufferData wrapper). Black-screen triage 2026-05-01.
    {
        const uint64_t* iq = reinterpret_cast<const uint64_t*>(img);
        printf("[sub_c780] post-ctor img=%p:\n", img);
        printf("    +0x00..0x18 vtbl=%p q1=%p q2=%p q3=%p\n",
               (void*)iq[0], (void*)iq[1], (void*)iq[2], (void*)iq[3]);
        printf("    +0x20..0x40 (params)= %p %p %p %p\n",
               (void*)iq[4], (void*)iq[5], (void*)iq[6], (void*)iq[7]);
        printf("    +0x40..0x60 = %p %p %p %p\n",
               (void*)iq[8], (void*)iq[9], (void*)iq[10], (void*)iq[11]);
        printf("    +0x60 (ImageBufferData) = %p\n", (void*)iq[12]);
        fflush(stdout);
    }

    // (3) Override the vtable with the NoiseUI/ModDialog/Geometry shared one.
    *reinterpret_cast<uintptr_t*>(img) = g_NoiseUI_ImageBufferVtable;

    // (4) Store on the Geometry container.
    *reinterpret_cast<void**>(selfBytes + kImageBufferOffset) = img;

    // (5) Read the heightmap edge length (width == height, square texture).
    int width = *reinterpret_cast<int*>(selfBytes + kGridSizeOffset);
    if (width <= 0) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] non-positive width %d at self+0xC0 — "
                   "skipping Create/CopyToTexture\n", width);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    // (6) Dispatch through the custom vtable's +0x48 slot (Create). Index 9
    //     in the 8-byte-stride vtable. Mirrors NoiseUI_Slot4 lines 290-299
    //     and ModDialog lines 395-397 exactly.
    uintptr_t vtable = *reinterpret_cast<uintptr_t*>(img);
    if (vtable == 0) {
        // Defensive: vtable write should always succeed since g_NoiseUI is non-zero.
        printf("[sub_c780] vtable write disappeared (img=%p) — skipping\n", img);
        fflush(stdout);
        return;
    }
    fn_ImageBuffer_Create_t fnCreate = *reinterpret_cast<fn_ImageBuffer_Create_t*>(
        vtable + kCreateVtableSlotOffset);
    if (!fnCreate) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] vtable +0x48 (Create) is null — "
                   "skipping CopyToTexture\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    bool createOk = fnCreate(img,
             /*imageType */ 0,
             /*usage     */ 2,
             /*format    */ 5,
             /*mipLevels */ 0,
             /*width     */ width,
             /*height    */ width,
             /*filterMode*/ 2,
             /*addrU     */ 1,
             /*addrV     */ 1,
             /*addrW     */ 1,
             /*mipData   */ nullptr);

    // Diagnostic: verify Create populated the buffer params/data slots.
    {
        const uint64_t* iq = reinterpret_cast<const uint64_t*>(img);
        printf("[sub_c780] post-Create img=%p createOk=%d width=%d:\n",
               img, (int)createOk, width);
        printf("    +0x20..0x40 (params)= %p %p %p %p\n",
               (void*)iq[4], (void*)iq[5], (void*)iq[6], (void*)iq[7]);
        printf("    +0x40..0x60 = %p %p %p %p\n",
               (void*)iq[8], (void*)iq[9], (void*)iq[10], (void*)iq[11]);
        printf("    +0x60 (ImageBufferData) = %p\n", (void*)iq[12]);
        fflush(stdout);
    }

    // (7) Lock the device, copy heightmap bytes -> texture, unlock.
    void* device = *g_pVideoDeviceSingleton;
    fn_VideoDevice_Lock(device);

    void* heightmapBytes = *reinterpret_cast<void**>(selfBytes + kHeightmapBytesOffset);
    printf("[sub_c780] CopyToTexture(img=%p, bytes=%p, w=%d, h=%d)\n",
           img, heightmapBytes, width, width);
    if (heightmapBytes) {
        const uint8_t* hb = reinterpret_cast<const uint8_t*>(heightmapBytes);
        printf("    first 16 bytes: %02x %02x %02x %02x %02x %02x %02x %02x  %02x %02x %02x %02x %02x %02x %02x %02x\n",
               hb[0], hb[1], hb[2], hb[3], hb[4], hb[5], hb[6], hb[7],
               hb[8], hb[9], hb[10], hb[11], hb[12], hb[13], hb[14], hb[15]);
    }
    fflush(stdout);
    fn_ImageBuffer_CopyToTexture(img, heightmapBytes, width, width);

    // Re-fetch — original disassembly reads `[0x140151a80]` then `[RAX]` again
    // for the Unlock call; preserves the same indirection level we used above.
    void* deviceForUnlock = *g_pVideoDeviceSingleton;
    fn_VideoDevice_Unlock(deviceForUnlock);

    {
        const uint64_t* iq = reinterpret_cast<const uint64_t*>(img);
        printf("[sub_c780] DONE img=%p — final +0x60=%p\n", img, (void*)iq[12]);
        fflush(stdout);
    }
}

} // namespace

// ============================================================
// Public reimpl entry point
// ============================================================
//
// Tries the original exe address first (DLL mode + standalone when the exe
// code page is mapped). If that pointer is null (standalone mode, no exe
// code), falls through to our cleaned C body. The cleaned body mirrors the
// original instruction-for-instruction so a future A/B trace can verify
// equivalence.

extern "C" void Reimpl_TradeWork_sub_c780(void* self) {
    TradeWork_impl_sub_c780_Body(self);
}
