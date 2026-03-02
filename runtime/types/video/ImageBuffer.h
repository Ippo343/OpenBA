#pragma once
// Video::ImageBuffer — binary-compatible struct definition.
// Runtime-x64.dll class, 0x68 bytes.
//
// Inheritance: Object::Base -> Object::External -> Object::CompiledExternal -> Video::ImageBuffer
//
// Decompiled from:
//   Ctor:           0x1800f2580 (??0ImageBuffer@Video@@QEAA@XZ)
//   Copy ctor:      0x18001eb80 (??0ImageBuffer@Video@@QEAA@AEBV01@@Z)
//   Dtor:           0x1800f2610 (??1ImageBuffer@Video@@UEAA@XZ)
//   GetLevelCount:  0x1800f2420 (?GetLevelCount@ImageBuffer@Video@@QEBAHXZ)
//   IsCompressed:   0x1800f2480 (?IsCompressed@ImageBuffer@Video@@QEBA_NXZ)
//   CopyToTexture:  0x1800f3380 (?CopyToTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)
//   CopyFromTexture:0x1800f33a0 (?CopyFromTexture@ImageBuffer@Video@@QEAA_NPEAXHH@Z)

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
    int32_t     width;              // +0x20  image width in pixels (read by GetLevelCount, Width())
    int32_t     height;             // +0x24  image height in pixels (read by GetLevelCount, Height())
    uint32_t    field28;            // +0x28  unknown (zeroed in ctor)
    uint32_t    field2C;            // +0x2C  unknown (init to 2 via qword write at +0x2C)
    uint32_t    format;             // +0x30  pixel format: < 3 = compressed, >= 3 = uncompressed
    uint32_t    flags;              // +0x34  bit 0 = hasMipMaps (controls GetLevelCount)
    uint32_t    field38;            // +0x38  unknown (zeroed in ctor)
    uint32_t    field3C;            // +0x3C  unknown (zeroed in ctor)
    uint32_t    field40;            // +0x40  unknown (zeroed in ctor)
    uint32_t    field44;            // +0x44  unknown (init to 1 in ctor)
    void*       pixelData;          // +0x48  refcounted pixel buffer (refcount at ptr-4, frees at 0)
    uint64_t    field50;            // +0x50  unknown (copied in copy ctor)
    uint64_t    field58;            // +0x58  unknown (copied in copy ctor)
    void*       platformData;       // +0x60  ImageBufferData* (platform-specific, set by PlatformConstruct)

    // --- Inline accessors (matching original inlines) ---
    int32_t Width() const { return width; }
    int32_t Height() const { return height; }
    bool IsCompressed() const { return format < 3; }
    bool HasMipMaps() const { return (flags & 1) != 0; }

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
static_assert(offsetof(ImageBuffer, field28) == 0x28, "field28 at +0x28");
static_assert(offsetof(ImageBuffer, field2C) == 0x2C, "field2C at +0x2C");
static_assert(offsetof(ImageBuffer, format) == 0x30, "format at +0x30");
static_assert(offsetof(ImageBuffer, flags) == 0x34, "flags at +0x34");
static_assert(offsetof(ImageBuffer, field38) == 0x38, "field38 at +0x38");
static_assert(offsetof(ImageBuffer, field3C) == 0x3C, "field3C at +0x3C");
static_assert(offsetof(ImageBuffer, field40) == 0x40, "field40 at +0x40");
static_assert(offsetof(ImageBuffer, field44) == 0x44, "field44 at +0x44");
static_assert(offsetof(ImageBuffer, pixelData) == 0x48, "pixelData at +0x48");
static_assert(offsetof(ImageBuffer, field50) == 0x50, "field50 at +0x50");
static_assert(offsetof(ImageBuffer, field58) == 0x58, "field58 at +0x58");
static_assert(offsetof(ImageBuffer, platformData) == 0x60, "platformData at +0x60");
