#pragma once
// ============================================================================
// runtime/types/graphics/FrameCopyImageBuffer.h — Graphics::FrameCopyImageBuffer
// ============================================================================
//
// 0x88 bytes. Derived from Video::ImageBuffer (0x68) — the ctor calls
// `??0ImageBuffer@Video@@QEAA@XZ` on `this` before touching anything of its
// own, and the dtor tail-jumps to `??1ImageBuffer@Video@@UEAA@XZ`.
//
// The SIZE is confirmed three independent ways, none of which is this header:
//   * `runtime/src/save/saveload_draw.cpp:451` allocates exactly 0x88 for one
//     through `??2FrameCopyImageBuffer@Graphics@@SAPEAX_K@Z`;
//   * `code/Game/Game/w2_takescreenshot.cpp:229` MemAllocs 0x88 for one,
//     transcribed from exe 0x140095da0;
//   * the ctor (Runtime 0x1800a3d10) writes +0x68, +0x70, +0x78 and +0x80 and
//     nothing past it.
//
// Every offset below was read from the DISASSEMBLY of the four bodies that
// touch them (ctor 0x1800a3d10, dtor 0x1800a3c80, Create 0x1800a3b70,
// Generate 0x1800a3b20), not from Ghidra's C — the decompiler's rendering of
// the ctor invents an `extraout_RAX` for the inlined `operator new` result and
// would have sent a reader looking for a field that is not there. See
// docs/runtime_port/WAVE23_LANE_GFXBODY.md §3.
//
// WHAT THE THREE OWNED POINTERS ARE FOR (from the four bodies, in order):
//   +0x70  a SECOND, plain Video::ImageBuffer (0x68, ctor-constructed). The
//          ctor immediately calls slot 1 of its platform companion, which
//          dx11/src/imagebufferdata.cpp:11 names "alias the DeviceData color
//          buffer, build an RTV" — i.e. this member IS the back buffer.
//   +0x78  a Video::RenderBuffer whose colour attachment is +0x70 (the ctor's
//          last act: `RenderBuffer::Create(+0x78, +0x70, nullptr)`).
//   +0x80  a Video::RenderBuffer whose colour attachment is `this` — created
//          by `Create` once the texture behind `this` exists.
// `Generate` then asks +0x80's platform companion to resolve/blit FROM +0x78,
// which is what makes this class "a copy of the frame".
//
// +0x68 is a plain int LATCH, not a flag: Generate compares it with its own
// `int` argument and only does the copy when they differ, then stores the
// argument. The ctor zeroes it. Both live callers pass 1
// (saveload_draw.cpp:479, w2_takescreenshot.cpp), so on those routes it fires
// once per object.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "../video/ImageBuffer.h"

namespace RtGraphics {

struct FrameCopyImageBuffer {
    ::ImageBuffer base;             // +0x00 .. +0x67  (ImageBuffer.h declares it at GLOBAL scope)
    int32_t  _generation;           // +0x68  latch compared against Generate's argument
    uint32_t _pad6c;                // +0x6c  never written by any of the four bodies
    void*    _backBufferImage;      // +0x70  Video::ImageBuffer*   (owned; deleted via vtable slot 0)
    void*    _sourceTarget;         // +0x78  Video::RenderBuffer*  (owned; colour = _backBufferImage)
    void*    _destTarget;           // +0x80  Video::RenderBuffer*  (owned; colour = this)
};

static_assert(sizeof(FrameCopyImageBuffer) == 0x88,
              "FrameCopyImageBuffer must be 0x88 bytes (saveload_draw.cpp:451 allocates 0x88)");
static_assert(offsetof(FrameCopyImageBuffer, _generation) == 0x68, "_generation @ +0x68");
static_assert(offsetof(FrameCopyImageBuffer, _backBufferImage) == 0x70, "_backBufferImage @ +0x70");
static_assert(offsetof(FrameCopyImageBuffer, _sourceTarget) == 0x78, "_sourceTarget @ +0x78");
static_assert(offsetof(FrameCopyImageBuffer, _destTarget) == 0x80, "_destTarget @ +0x80");

// ----------------------------------------------------------------------------
// The two vtables the ctor and dtor stamp, as RETAIL addresses.
//
// `??_7FrameCopyImageBuffer@Graphics@@6B@` is 0x180154f00 — corroborated
// independently at runtime/src/save/saveload_draw.cpp:83, which walked its
// slot 0x50 and found 0x1800a3b20 (= ?Generate@FrameCopyImageBuffer@).
// `??_7GeneratedImageBuffer@Graphics@@6B@` is 0x180154f60, the address the
// ctor stamps before its own and the dtor restores on the way out (the MSVC
// "hand the object back to my base class" idiom).
//
// CORRECTION (2026-09-12, vtable batch 6): that sentence is right about the
// mechanism and understates what the other table IS. `Graphics::
// GeneratedImageBuffer` is a REAL INTERMEDIATE CLASS with its own 0x70 layout,
// its own constructor (0x1800a3e70), destructor (0x1800a3e40), `?_typeInfo@`
// node and five vtable slots — all of them now in
// runtime/types/graphics/GeneratedImageBuffer.h and
// runtime/src/graphics/imagebuffer_derived.cpp. The field this header calls
// `_generation` at +0x68 is ITS member, inherited here, not FrameCopy's own,
// which is why `Graphics::ReflectionImageBuffer` has a field at the same offset
// doing the same job. The hierarchy is
// Video::ImageBuffer -> GeneratedImageBuffer -> {FrameCopy, Reflection}.
// ----------------------------------------------------------------------------
constexpr uintptr_t kVtFrameCopyImageBuffer = 0x180154f00ull;
constexpr uintptr_t kVtGeneratedImageBuffer = 0x180154f60ull;

}  // namespace RtGraphics
