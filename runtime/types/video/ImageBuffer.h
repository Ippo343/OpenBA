#pragma once
// Video::ImageBuffer — binary-compatible struct definition.
// Runtime-x64.dll class, 0x68 bytes.
//
// Inheritance: Object::Base -> Object::External -> Object::CompiledExternal -> Video::ImageBuffer
//
// Decompiled from (Wave 6 lane C):
//   Ctor:            0x1800f2580 (??0ImageBuffer@Video@@QEAA@XZ)
//   Copy ctor:       0x18001eb80 (??0ImageBuffer@Video@@QEAA@AEBV01@@Z) — NOT this lane's scope
//   Dtor:            0x1800f2610 (??1ImageBuffer@Video@@UEAA@XZ)
//   Width:           0x1800f2530 (?Width@ImageBuffer@Video@@QEBAHXZ)
//   Height:          0x1800f24e0 (?Height@ImageBuffer@Video@@QEBAHXZ)
//   GetLevelCount:   0x1800f2420 (?GetLevelCount@ImageBuffer@Video@@QEBAHXZ)
//   IsCompressed:    0x1800f2480 (?IsCompressed@ImageBuffer@Video@@QEBA_NXZ)
//   IsDepthBuffer:   0x1800f24c0 (?IsDepthBuffer@ImageBuffer@Video@@QEBA_NXZ)
//   IsDynamic:       0x1800f24d0 (?IsDynamic@ImageBuffer@Video@@QEBA_NXZ)
//   IsGenerated:     0x1800f24a0 (?IsGenerated@ImageBuffer@Video@@QEBA_NXZ)
//   IsRenderBuffer:  0x1800f2460 (?IsRenderBuffer@ImageBuffer@Video@@QEBA_NXZ)
//   IsRendered:      0x1800f2490 (?IsRendered@ImageBuffer@Video@@QEBA_NXZ)
//   CopyToTexture:   0x1800f3380 (?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)
//   CopyFromTexture: 0x1800f33a0 (?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)
//
// STALE-COMMENT CORRECTION (Wave 6 lane C): the +0x28/+0x2C fields were
// previously "unknown". The disassembly of the ctor (0x1800f25d8-0x1800f25db,
// one dword store then one qword store covering both) and the Is*() family
// (all of which read *(int*)(this+0x2c)) show they are `dimension` (2D vs
// cube) and `kind` (an enum selecting bind flags: 0/4/5 = render target,
// 1 = depth, 2 = default/none, 3 = dynamic). This matches
// dx11/src/imagebufferdata.cpp's independently-recovered field map exactly
// (that file reads the same offsets from the DX11 side). +0x38/+0x3C/+0x40
// were also "unknown"; dx11/src/imagebufferdata.cpp's sampler code (and the
// ctor's default value 0) identify them as the address-mode triple
// (U/V/W), and +0x44 (init value 1) as the filter index.
//
// Width()/Height() were PREVIOUSLY WRONG: the old inline `return width;` /
// `return height;` only reproduced the common case. The real accessors
// (0x1800f2530 / 0x1800f24e0) special-case two flag bits that make the
// buffer track the live device resolution (bit 8 = full device size, bit 9 =
// half device size) and a third (bit 6) that right-shifts the stored value
// by a device-configured downsample amount. Nothing in this tree called the
// old inline (only the fn_ImageBuffer_Width/Height pointers, resolved by
// mangled name, are ever used), so this was latent rather than an observed
// bug. Fixed below.

#include <cstdint>
#include <cstddef>

// ============================================================
// Object::CompiledExternal base (0x20 bytes)
// Ctor @ 0x1800bb920 (??0CompiledExternal@Object@@QEAA@XZ)
// Chain: Base(vtable) -> External(+0x08..+0x18 zeroed) -> CompiledExternal(vtable set)
// ============================================================

// ============================================================
// Video::ImageBuffer (0x68 bytes)
// ============================================================
struct ImageBuffer {
    // --- CompiledExternal base (0x20 bytes) ---
    void*       vtable;             // +0x00  vtable pointer (set through Base->External->CompiledExternal->ImageBuffer chain)
    uint64_t    externalData08;     // +0x08  External base field (zeroed in ctor)
    uint64_t    externalData10;     // +0x10  External base field (zeroed in ctor)
    uint64_t    externalData18;     // +0x18  External base field (zeroed in ctor)

    // --- ImageBuffer subclass fields ---
    int32_t     width;              // +0x20  image width in pixels (raw field; use Width() for the accessor)
    int32_t     height;             // +0x24  image height in pixels (raw field; use Height() for the accessor)
    int32_t     dimension;          // +0x28  0 = 2D, non-zero (1) = cube map (zeroed in ctor)
    int32_t     kind;               // +0x2C  bind-flag selector: 0/4/5 render target, 1 depth, 2 default/none, 3 dynamic (init 2 in ctor)
    uint32_t    format;             // +0x30  pixel format: index into an 8-entry DXGI table, < 3 = block-compressed
    uint32_t    flags;              // +0x34  bit0 mipmapped, bit4/5 device-reset-tracked (set by DX11 color/depth attach),
                                     //        bit6 comparison sampler + downsample-shift enable, bit8 = device width/height,
                                     //        bit9 = half device width/height (bits 8/9 read only by Width()/Height())
    int32_t     addressU;           // +0x38  address mode index (0=WRAP, 1=CLAMP), zeroed in ctor
    int32_t     addressV;           // +0x3C  address mode index, zeroed in ctor
    int32_t     addressW;           // +0x40  address mode index, zeroed in ctor
    int32_t     filter;             // +0x44  filter mode index (0=POINT,1=LINEAR,2=ANISOTROPIC), init 1 in ctor
    void*       pixelData;          // +0x48  refcounted pixel buffer (refcount at ptr-4, frees at 0)
    uint64_t    field50;            // +0x50  unknown (copied in copy ctor; not touched by any Lane C export)
    uint64_t    field58;            // +0x58  unknown (copied in copy ctor; not touched by any Lane C export)
    void*       platformData;       // +0x60  Video::ImageBufferData* (platform-specific, set by PlatformConstruct)

    // --- Inline accessors matching the real (non-trivial) originals ---
    bool IsCompressed()   const { return format < 3; }
    bool HasMipMaps()     const { return (flags & 1u) != 0; }
    bool IsDepthBuffer()  const { return kind == 1; }
    bool IsDynamic()      const { return kind == 3; }
    bool IsGenerated()    const { return kind == 4; }
    bool IsRendered()     const { return kind == 5; }
    // kind & ~0b101 == 0  <=>  kind in {0,1,4,5}
    bool IsRenderBuffer() const { return (static_cast<uint32_t>(kind) & 0xfffffffau) == 0; }

    int GetLevelCount() const {
        int count = 1;
        if (HasMipMaps()) {
            int w = width;
            int h = height;
            while (w > 1 || h > 1) {
                h >>= 1; if (h == 0) h = 1;
                w >>= 1; if (w == 0) w = 1;
                count++;
            }
        }
        return count;
    }
};

static_assert(sizeof(ImageBuffer) == 0x68, "ImageBuffer must be 0x68 bytes");
static_assert(offsetof(ImageBuffer, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(ImageBuffer, externalData08) == 0x08, "externalData08 at +0x08");
static_assert(offsetof(ImageBuffer, externalData10) == 0x10, "externalData10 at +0x10");
static_assert(offsetof(ImageBuffer, externalData18) == 0x18, "externalData18 at +0x18");
static_assert(offsetof(ImageBuffer, width) == 0x20, "width at +0x20");
static_assert(offsetof(ImageBuffer, height) == 0x24, "height at +0x24");
static_assert(offsetof(ImageBuffer, dimension) == 0x28, "dimension at +0x28");
static_assert(offsetof(ImageBuffer, kind) == 0x2C, "kind at +0x2C");
static_assert(offsetof(ImageBuffer, format) == 0x30, "format at +0x30");
static_assert(offsetof(ImageBuffer, flags) == 0x34, "flags at +0x34");
static_assert(offsetof(ImageBuffer, addressU) == 0x38, "addressU at +0x38");
static_assert(offsetof(ImageBuffer, addressV) == 0x3C, "addressV at +0x3C");
static_assert(offsetof(ImageBuffer, addressW) == 0x40, "addressW at +0x40");
static_assert(offsetof(ImageBuffer, filter) == 0x44, "filter at +0x44");
static_assert(offsetof(ImageBuffer, pixelData) == 0x48, "pixelData at +0x48");
static_assert(offsetof(ImageBuffer, field50) == 0x50, "field50 at +0x50");
static_assert(offsetof(ImageBuffer, field58) == 0x58, "field58 at +0x58");
static_assert(offsetof(ImageBuffer, platformData) == 0x60, "platformData at +0x60");

// ============================================================
// Video::ImageBufferData — the RUNTIME-side abstract base (0x10 bytes).
// This is NOT dx11::ImageBufferData (dx11/types/VideoDX11Data.h); it is the
// pure-virtual base Runtime-x64.dll itself defines, that dx11's derived
// class inherits from. Runtime owns exactly one method on it in this lane:
// the dtor, which resets the vtable pointer to this base and does nothing
// else (the MSVC "restore this class's vptr" idiom — see rt_diff notes in
// buffers.cpp).
//
// Decompiled from:
//   Ctor(ImageBuffer*):  0x1800f8c90 (??0ImageBufferData@Video@@QEAA@PEAVImageBuffer@1@@Z) — NOT this lane's scope
//   Copy ctor:           0x18007dd80 (??0ImageBufferData@Video@@QEAA@AEBV01@@Z) — NOT this lane's scope
//   Dtor:                0x1800f8e10 (??1ImageBufferData@Video@@UEAA@XZ)
// ============================================================
struct ImageBufferData {
    void* vtable;       // +0x00
    void* imageBuffer;  // +0x08  owning Video::ImageBuffer* (set by the ctor, untouched by the dtor)
};
static_assert(sizeof(ImageBufferData) == 0x10, "ImageBufferData must be 0x10 bytes");
static_assert(offsetof(ImageBufferData, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(ImageBufferData, imageBuffer) == 0x08, "imageBuffer at +0x08");
