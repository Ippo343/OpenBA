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
//   3. *(void**)img = 0x14015c8c0                      // the exe's copy of the
//                                                      // class's OWN table: 2+3
//                                                      // = `new ImageBuffer()`
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
// - The vtable at 0x14015c8c0 is exe-resident and is Video::ImageBuffer's own
//   (slot 0 the exe scalar deleting dtor, 1-9 thunks onto the class's members):
//   ENDGAME B.3.9 EXA constructs the real class.
//
// This helper is NOT yet called from anywhere — Geometry::Serialize body is
// session 8. Session 7's only goal is that the helper exists, links, and is
// callable, executing our cleaned C body.

// ENDGAME B.3.5 Video/Graphics caller batch: Video::ImageBuffer is a real class
// and the hand-inlined vtable +0x48 dispatch below is its slot 9 (`Create`).
// ImageBuffer.h pulls no Volume.h, so include order against <windows.h> is free.
#include "video/ImageBuffer.h"
#include "video/Device.h"       // ENDGAME B.3.5 batch K1 — Video::Device::{Lock,Unlock}
#include <new>
#include "Game/exe_bodies.h"
#include "system/MemoryAllocator.h"   // System::MemoryAllocator::MemAlloc / MemFree
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
//
// ENDGAME B.3.5: the hand-inlined slot read and its typedef are GONE — +0x48 is
// slot 9 of `Video::ImageBuffer` (B35_RULES 2.3 item 5), so this is a virtual
// call on a PROVED receiver (0x68 bytes from MemAlloc, run through
// `??0ImageBuffer@Video@@`, then given the exe's NoiseUI table). The virtual call
// reads that live vptr at the same +0x48 the slot read did.

constexpr int kImageBufferSize        = 0x68;     // sizeof(Video::ImageBuffer)
constexpr int kImageBufferAlign       = 8;
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

    if (!System::MemoryAllocator::_singleton) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] MemAlloc/singleton unavailable — skipping image upload "
                   "(self=%p)\n", self);
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    if (!::Video::Device::_singleton) {
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
    void* img = System::MemoryAllocator::MemAlloc(kImageBufferSize, kImageBufferAlign);
    if (!img) {
        static bool s_warned = false;
        if (!s_warned) {
            printf("[sub_c780] MemAlloc(0x68) failed — skipping image upload\n");
            fflush(stdout);
            s_warned = true;
        }
        return;
    }

    // (2)+(3) Construct the real class: retail's imported ctor followed by the
    // exe's copy of the class's own table (ENDGAME B.3.9 EXA).
    ::new (img) Video::ImageBuffer();   // B.3.9 EXA: the real class (exe table 0x14015c8c0 is its own)

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

    // (6) Slot 9 of Video::ImageBuffer — `Create` — which is what the exe's
    //     hand-inlined vtable +0x48 dispatch was. Mirrors NoiseUI_Slot4 lines
    //     290-299 and ModDialog lines 395-397 exactly.
    //     The old `vtable == 0` and `fnCreate == null` tests were reads of the
    //     slot itself, not object tests, and go with the slot read.
    bool createOk = reinterpret_cast<Video::ImageBuffer*>(img)->Create(
             /*type      */ 0,
             /*usage     */ 2,
             /*format    */ 5,
             /*flags     */ 0u,
             /*width     */ width,
             /*height    */ width,
             /*filter    */ 2,
             /*addressU  */ 1,
             /*addressV  */ 1,
             /*addressW  */ 1,
             /*levels    */ nullptr);   // B.3.5 bridge

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
    void* device = ::Video::Device::_singleton;
    reinterpret_cast<Video::Device*>(device)->Lock();   // B.3.5 bridge

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
    reinterpret_cast<Video::ImageBuffer*>(img)
        ->CopyToTexture(heightmapBytes, width, width);   // B.3.5 bridge

    // Re-fetch — original disassembly reads `[0x140151a80]` then `[RAX]` again
    // for the Unlock call; preserves the same indirection level we used above.
    void* deviceForUnlock = ::Video::Device::_singleton;
    reinterpret_cast<Video::Device*>(deviceForUnlock)->Unlock();   // B.3.5 bridge

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
// Tries the original exe address first (usable when the exe code page is
// mapped). If that pointer is null (no exe code), falls through to our cleaned
// C body. The cleaned body mirrors the original instruction-for-instruction so
// a future A/B trace can verify equivalence.

extern "C" void Reimpl_TradeWork_sub_c780(void* self) {
    TradeWork_impl_sub_c780_Body(self);
}
