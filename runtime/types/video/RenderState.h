#pragma once
// ============================================================================
// runtime/types/video/RenderState.h — Video::RenderState and its three
// plain-old-data sub-structs, AlphaBlend / Depth / Stencil.
// ============================================================================
//
// Offsets read out of RETAIL Runtime-x64.dll `objdump -d`, 2026-09-12.
//
// *** RenderState DERIVES FROM Object::External DIRECTLY, NOT FROM
//     Object::CompiledExternal. *** Every other Video external in this tree
//     (ImageBuffer, VertexLayout, ShaderProgram and its two subclasses) stamps
//     ??_7CompiledExternal@Object@@6B@ (0x180154a30) between External's vtable
//     and its own. ??0RenderState@Video@@QEAA@XZ (0x1800f4b60) does NOT: it
//     goes Base (0x180154788) -> External (0x1801547a0) -> RenderState
//     (0x180157720), and ??1RenderState (0x1800f4c30) restores only the
//     RenderState vtable before tail-jumping straight to ??1External
//     (0x1800bc950). ?_ParentTypeName@RenderState@Video@@ (0x18004b540)
//     returns the literal "Object::External", where ShaderProgram's
//     (0x18004e090) returns "Object::CompiledExternal" — the export table says
//     the same thing the ctor does.
//
// THE FIELD OFFSETS ARE PINNED BY ?Initialize@RenderState@Video@@ (0x1800f4840),
// which is nothing but a field-by-field copy of the three sub-structs into
// this object, so it states each sub-struct's size and position at once:
//
//   RDX -> +0x20, five dwords         AlphaBlend  (0x14 bytes)
//   R8  -> +0x34, four dwords         Depth       (0x10 bytes)
//   R9  -> +0x44, nine dwords         Stencil     (0x24 bytes)
//   [RSP+0x28] -> +0x68               cullMode
//   [RSP+0x30] -> +0x6c               fillMode   (IsWireframe 0x18004b730 is
//                                                 `CMP dword [RCX+0x6c],1`)
//   [RSP+0x38] -> +0x70               a bool
//   then `_data->vtable[1](_data)`    PlatformCreate
//
// and the ctor's defaults agree field for field:
//   +0x20 = 0            +0x24 = 1   +0x28 = 0   +0x2c = 0   +0x30 = 0xf
//   +0x34 = 1 (byte)     +0x35 = 1   +0x38 = 4   +0x3c = 0   +0x40 = 0
//   +0x44 .. +0x67 = 0
//   +0x68 = 1            +0x6c = 0   +0x70 = 0
//   +0x78 = Device::_singleton->deviceData ? dd->vtable[0x88](dd, this) : null
//
// sizeof is 0x80: the vector deleting destructor at 0x18004b9d0 carries
// `MOV EDX,0x80`, and rt_factories_video.inc's CreateNewObject step asks the
// allocator for 0x80 with alignment 8.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../object/External.h"

namespace rt {
namespace video {

// ?Serialize@AlphaBlend@RenderState@Video@@ (0x1800f4aa0):
//   Bool(+0x00), UInt(+0x04), UInt(+0x08), UInt(+0x0c), UInt(+0x10)
// ??0AlphaBlend@RenderState@Video@@ (0x1800f4b30): +0x00=0, +0x04=1, +0x08=0,
//   +0x0c=0, +0x10=0xf.
struct AlphaBlend {
    uint8_t  enabled;    // +0x00
    uint8_t  _pad01[3];  // +0x01  never written (the ctor stores a BYTE at +0)
    uint32_t srcBlend;   // +0x04  default 1
    uint32_t dstBlend;   // +0x08  default 0
    uint32_t blendOp;    // +0x0c  default 0
    uint32_t writeMask;  // +0x10  default 0xf (all four channels)
};
static_assert(sizeof(AlphaBlend) == 0x14, "RenderState::AlphaBlend is 0x14 (5 dwords copied by Initialize)");
static_assert(offsetof(AlphaBlend, writeMask) == 0x10, "AlphaBlend::writeMask @ +0x10");

// ?Serialize@Depth@RenderState@Video@@ (0x1800f4a00):
//   Bool(+0x00), Bool(+0x01), UInt(+0x04), Float(+0x08), Float(+0x0c)
// ??0Depth@RenderState@Video@@ (0x1800f4a80): word at +0x00 = 0x0101 (so both
//   bools are 1), qword at +0x04 = 4 (func = 4, +0x08 = 0), +0x0c = 0.
struct Depth {
    uint8_t  test;       // +0x00  default 1
    uint8_t  write;      // +0x01  default 1
    uint8_t  _pad02[2];  // +0x02
    uint32_t func;       // +0x04  default 4
    float    bias;       // +0x08  default 0
    float    slopeBias;  // +0x0c  default 0
};
static_assert(sizeof(Depth) == 0x10, "RenderState::Depth is 0x10 (4 dwords copied by Initialize)");
static_assert(offsetof(Depth, func) == 0x04, "Depth::func @ +0x04");

// ?Serialize@Stencil@RenderState@Video@@ (0x1800f48e0):
//   Bool(+0x00), Bool(+0x01), UInt(+0x04), UChar(+0x08),
//   UInt(+0x0c), UInt(+0x10), UInt(+0x14), UInt(+0x18), UInt(+0x1c), UInt(+0x20)
// ??0Stencil@RenderState@Video@@ (0x1800f49d0) zeroes the whole 0x24.
struct Stencil {
    uint8_t  enabled;      // +0x00
    uint8_t  twoSided;     // +0x01
    uint8_t  _pad02[2];    // +0x02
    uint32_t readMask;     // +0x04
    uint8_t  writeMask;    // +0x08  serialized as UChar, not UInt
    uint8_t  _pad09[3];    // +0x09
    uint32_t func;         // +0x0c
    uint32_t failOp;       // +0x10
    uint32_t depthFailOp;  // +0x14
    uint32_t passOp;       // +0x18
    uint32_t backFunc;     // +0x1c
    uint32_t reference;    // +0x20
};
static_assert(sizeof(Stencil) == 0x24, "RenderState::Stencil is 0x24 (9 dwords copied by Initialize)");
static_assert(offsetof(Stencil, writeMask) == 0x08, "Stencil::writeMask @ +0x08");
static_assert(offsetof(Stencil, reference) == 0x20, "Stencil::reference @ +0x20");

struct RenderState {
    RtObject::External base;        // +0x00 (0x20) — External, NOT CompiledExternal
    AlphaBlend         _alphaBlend; // +0x20
    Depth              _depth;      // +0x34
    Stencil            _stencil;    // +0x44
    uint32_t           _cullMode;   // +0x68  default 1
    uint32_t           _fillMode;   // +0x6c  default 0; ==1 means wireframe
    uint8_t            _flag70;     // +0x70
    uint8_t            _pad71[7];   // +0x71
    void*              _data;       // +0x78  Video::RenderStateData*
};
static_assert(sizeof(RenderState) == 0x80,
              "Video::RenderState is 0x80 (MOV EDX,0x80 at 0x18004ba02)");
static_assert(offsetof(RenderState, _alphaBlend) == 0x20, "RenderState::_alphaBlend @ +0x20");
static_assert(offsetof(RenderState, _depth)      == 0x34, "RenderState::_depth @ +0x34");
static_assert(offsetof(RenderState, _stencil)    == 0x44, "RenderState::_stencil @ +0x44");
static_assert(offsetof(RenderState, _cullMode)   == 0x68, "RenderState::_cullMode @ +0x68");
static_assert(offsetof(RenderState, _fillMode)   == 0x6c, "RenderState::_fillMode @ +0x6c");
static_assert(offsetof(RenderState, _flag70)     == 0x70, "RenderState::_flag70 @ +0x70");
static_assert(offsetof(RenderState, _data)       == 0x78, "RenderState::_data @ +0x78");

} // namespace video
} // namespace rt
