#pragma once
// ============================================================================
// runtime/types/graphics/GeneratedImageBuffer.h
// Graphics::GeneratedImageBuffer and Graphics::ReflectionImageBuffer
// ============================================================================
//
// The third member of this family, Graphics::FrameCopyImageBuffer, already has
// its own header (FrameCopyImageBuffer.h) and is NOT restated here. All three
// derive from Video::ImageBuffer (0x68), and GeneratedImageBuffer is the
// intermediate base both others stamp on the way in and out:
//
//     Video::ImageBuffer (0x68)
//       └─ Graphics::GeneratedImageBuffer (0x70)   ??_7 0x180154f60
//            ├─ Graphics::FrameCopyImageBuffer   (0x88)  FrameCopyImageBuffer.h
//            └─ Graphics::ReflectionImageBuffer  (0x90)  here, ??_7 0x180155190
//
// EVERY OFFSET BELOW WAS READ FROM THE DISASSEMBLY of the bodies that write it,
// never from Ghidra's C:
//   GeneratedImageBuffer   ctor 0x1800a3e70, dtor 0x1800a3e40
//   ReflectionImageBuffer  ctor 0x1800a7130, dtor 0x1800a70b0,
//                          Reconstruct 0x1800223e0, Construct 0x180022450,
//                          Render 0x1800a6b80, Serialize 0x1800a6f00
//
// THE TWO SIZES, each confirmed by its own deleting destructor's `MOV EDX,imm`
// (the stride the `eh vector destructor iterator` is handed):
//   0x18001eee0  MOV EDX,0x70   GeneratedImageBuffer
//   0x180022600  MOV EDX,0x90   ReflectionImageBuffer
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../video/ImageBuffer.h"

namespace RtGraphics {

// ---------------------------------------------------------------------------
// Graphics::GeneratedImageBuffer — 0x70
// ---------------------------------------------------------------------------
// Adds exactly ONE field to Video::ImageBuffer: the int latch at +0x68 that
// every "generated" image compares against the caller's generation counter.
// The ctor zeroes it (`MOV dword ptr [RBX+0x68],0x0` at 0x1800a3e96) and the
// dtor touches nothing of its own — it restamps the vtable and tail-jumps
// ~ImageBuffer. `_pad6c` is NEVER WRITTEN by either body; it is the four bytes
// MSVC leaves before the 8-aligned tail of the derived classes. Recorded, not
// zeroed (FrameCopyImageBuffer.h says the same of its own +0x6c).
struct GeneratedImageBuffer {
    ::ImageBuffer base;             // +0x00 .. +0x67
    int32_t  _generation;           // +0x68
    uint32_t _pad6c;                // +0x6c  never written
};

static_assert(sizeof(GeneratedImageBuffer) == 0x70,
              "GeneratedImageBuffer must be 0x70 (0x18001eee0's MOV EDX,0x70)");
static_assert(offsetof(GeneratedImageBuffer, _generation) == 0x68, "_generation @ +0x68");

// ---------------------------------------------------------------------------
// Graphics::ReflectionImageBuffer — 0x90
// ---------------------------------------------------------------------------
// WHAT THE FIVE FIELDS ARE, read off the four bodies that touch them:
//
//   +0x70  Video::ImageBuffer*   OWNED, and it is the DEPTH buffer, not the
//          colour one. `Serialize` builds it — `??2ImageBuffer(0x68)`, then
//          `??0ImageBuffer`, then the object's own vtable slot 9 `Create` with
//          **usage = 1** (Video::ImageBuffer::IsDepthBuffer() is `kind == 1`)
//          and format 7, at this image's own width/height/flags. So the
//          reflection's depth target is created on LOAD, not on construction:
//          the constructor leaves +0x70 null. The destructor destroys it
//          through ITS OWN vtable slot 0 with flags = 1, the deleting form.
//   +0x78  Video::RenderBuffer*  OWNED. Also built by `Serialize`:
//          `??2RenderBuffer(0x30)`, three zero qwords, `PlatformConstruct`,
//          then `RenderBuffer::Create(rb, /*colour=*/this, /*depth=*/+0x70)`.
//          THIS object is the colour attachment — that is what makes the class
//          a reflection *texture*. Destroyed the long way (`PlatformDestruct`
//          then `??3RenderBuffer`), because RenderBuffer has no virtual
//          destructor; the same asymmetry FrameCopyImageBuffer.h records.
//   +0x80..+0x8c  the REFLECTION PLANE, four floats. `Render` (0x1800a6c01
//          onwards) reads them as normal (x, y, z) at +0x80/+0x84/+0x88 and
//          distance at +0x8c, and builds the Householder matrix
//          `I - 2*n*nᵀ` with a `2*d*n` translation row out of them.
//          The constructor initialises the plane to (0, 0, 1, 0) with two
//          qword stores: `[+0x80] = 0` covers x and y, and
//          `MOV qword ptr [RBX+0x88],0x3f800000` covers z = 1.0f and d = 0.0f
//          in ONE instruction. Two qwords, four floats — the field widths
//          below are what `Render` reads, and the store widths are what the
//          ctor writes; both readings describe the same sixteen bytes.
struct ReflectionImageBuffer {
    ::ImageBuffer base;             // +0x00 .. +0x67
    int32_t  _generation;           // +0x68  the GeneratedImageBuffer latch
    uint32_t _pad6c;                // +0x6c  never written
    void*    _depth;                // +0x70  Video::ImageBuffer*  (owned, usage=1)
    void*    _target;               // +0x78  Video::RenderBuffer* (owned, colour = this)
    float    _planeX;               // +0x80  ctor: 0 (lower half of one qword store)
    float    _planeY;               // +0x84  ctor: 0 (upper half of it)
    float    _planeZ;               // +0x88  ctor: 1.0f
    float    _planeD;               // +0x8c  ctor: 0.0f
};

static_assert(sizeof(ReflectionImageBuffer) == 0x90,
              "ReflectionImageBuffer must be 0x90 (0x180022600's MOV EDX,0x90)");
static_assert(offsetof(ReflectionImageBuffer, _generation) == 0x68, "_generation @ +0x68");
static_assert(offsetof(ReflectionImageBuffer, _depth)  == 0x70, "_depth @ +0x70");
static_assert(offsetof(ReflectionImageBuffer, _target) == 0x78, "_target @ +0x78");
static_assert(offsetof(ReflectionImageBuffer, _planeX) == 0x80, "_planeX @ +0x80");
static_assert(offsetof(ReflectionImageBuffer, _planeY) == 0x84, "_planeY @ +0x84");
static_assert(offsetof(ReflectionImageBuffer, _planeZ) == 0x88, "_planeZ @ +0x88");
static_assert(offsetof(ReflectionImageBuffer, _planeD) == 0x8c, "_planeD @ +0x8c");

// The vtable this class stamps, as a RETAIL address. `kVtGeneratedImageBuffer`
// (0x180154f60) is FrameCopyImageBuffer.h's constant and is reused, not
// restated — the ctor here stamps that one first and then its own, and the
// destructor restores it on the way out.
constexpr uintptr_t kVtReflectionImageBuffer = 0x180155190ull;

}  // namespace RtGraphics
