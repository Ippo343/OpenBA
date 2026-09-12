#pragma once
// ============================================================================
// runtime/types/video/ShaderProgram.h — Video::ShaderProgram and its two
// concrete subclasses, Video::VertexProgram and Video::PixelProgram.
// ============================================================================
//
// Every offset is pinned by a named instruction in RETAIL Runtime-x64.dll
// (image base 0x180000000), read out of `objdump -d` over the shipped file on
// 2026-09-12 — never out of Ghidra's decompiled C
// (reference_ghidra_c_output_vs_disassembly.md).
//
// ---------------------------------------------------------------------------
// Video::ShaderProgram : Object::CompiledExternal   (sizeof 0x50)
// ---------------------------------------------------------------------------
// ??0ShaderProgram@Video@@QEAA@XZ (0x1800f5030) stamps four vtables in
// sequence — ??_7Base@Object@@6B@ (0x180154788), ??_7External@Object@@6B@
// (0x1801547a0), ??_7CompiledExternal@Object@@6B@ (0x180154a30) and finally
// ??_7ShaderProgram@Video@@6B@ (0x180157b60) — zeroes +0x08/+0x10/+0x18
// (External's Filename and Array) and then six more qwords, +0x20 .. +0x48.
// It never touches +0x50, and the vector deleting destructor at 0x18004e360
// carries `MOV EDX,0x50`, so sizeof is exactly 0x50.
//
//   +0x20  Array<Parameter>        _textures    (GetTextureCount  0x18004e410
//                                                reads +0x28; GetTextureName
//                                                0x18004e420 indexes +0x20 by
//                                                SHL 4; GetTextureDest
//                                                0x18004e470 reads elem+0x08;
//                                                IsTextureInternal 0x18004e480
//                                                reads elem+0x0c)
//   +0x30  Array<Parameter>        _constants   (GetConstantCount 0x18004e4a0
//                                                reads +0x38; GetConstantName
//                                                0x18004e4b0 indexes +0x30;
//                                                GetConstantDest  0x18004e500
//                                                reads elem+0x08)
//   +0x40  Array<Array<uint8_t>>   _byteCode    (GetByteCode 0x18004e100 is
//                                                `(Array<E>*)(+0x40) + idx*0x10`
//                                                and IsValid 0x18004e0f0 is
//                                                `+0x48 > 0`)
//
// Video::ShaderProgram::Parameter — 0x10 bytes. Named by
// ??0Parameter@ShaderProgram@Video@@QEAA@VString@System@@H_N@Z (0x1800f4f30),
// whose (String, int, bool) argument list is exactly the three fields the
// Serialize loop inside 0x1800f5110 touches: String::Serialize at elem+0x00,
// ReaderWriter::Int at elem+0x08 and, when GetVersion() >= 1, Bool at
// elem+0x0c. The last three bytes are padding and are never written.
//
// ---------------------------------------------------------------------------
// Video::VertexProgram / Video::PixelProgram : ShaderProgram  (sizeof 0x58)
// ---------------------------------------------------------------------------
// Each adds ONE pointer at +0x50 — the platform companion object — and nothing
// else. ??0VertexProgram (0x1800f6100) and ??0PixelProgram (0x1800f3950) are
// the ShaderProgram ctor inlined, followed by the class's own vtable stamp
// (0x180157b10 / 0x180157ac0) and an inlined PlatformConstruct that asks
// Video::Device::_singleton->deviceData for the companion through a DIFFERENT
// DeviceData vtable slot per class:
//     VertexProgram  slot +0x78   (CreateVertexProgramData)
//     PixelProgram   slot +0x80   (CreatePixelProgramData)
// ?GetData@VertexProgram@Video@@QEBAPEBVVertexProgramData@2@XZ (0x18004e720) is
// `MOV RAX,[RCX+0x50]; RET`, which names +0x50 in the export table itself.
// Both deleting destructors (0x18004e810, 0x18004eb90) carry `MOV EDX,0x58`.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../collection/Collection.h"
#include "../object/External.h"

namespace rt {
namespace video {

// Video::ShaderProgram::Parameter — a named binding slot.
struct ShaderParameter {
    void*   name;      // +0x00  System::String (refcounted, str[-4] is the count)
    int32_t dest;      // +0x08  the register/sampler index this binds to
    uint8_t internal;  // +0x0c  bool; only serialized when GetVersion() >= 1
    uint8_t _pad0d[3]; // +0x0d  never written by any arm
};
static_assert(sizeof(ShaderParameter) == 0x10,
              "ShaderProgram::Parameter is 0x10 (SHL RAX,0x4 at 0x18004e103)");
static_assert(offsetof(ShaderParameter, dest) == 0x08,
              "Parameter::dest @ +0x08 (MOV EAX,[RAX+R8*8+0x8] at 0x18004e47a)");
static_assert(offsetof(ShaderParameter, internal) == 0x0c,
              "Parameter::internal @ +0x0c (MOVZX EAX,[RAX+R8*8+0xc] at 0x18004e48a)");

struct ShaderProgram {
    RtObject::External                            base;        // +0x00 (0x20)
    Collection::Array<ShaderParameter>            _textures;   // +0x20
    Collection::Array<ShaderParameter>            _constants;  // +0x30
    Collection::Array<Collection::Array<uint8_t>> _byteCode;   // +0x40
};
static_assert(sizeof(ShaderProgram) == 0x50,
              "Video::ShaderProgram is 0x50 (MOV EDX,0x50 at 0x18004e392)");
static_assert(offsetof(ShaderProgram, _textures)  == 0x20, "ShaderProgram::_textures @ +0x20");
static_assert(offsetof(ShaderProgram, _constants) == 0x30, "ShaderProgram::_constants @ +0x30");
static_assert(offsetof(ShaderProgram, _byteCode)  == 0x40, "ShaderProgram::_byteCode @ +0x40");

// VertexProgram and PixelProgram have IDENTICAL layouts; they differ only in
// their vtable, their type info, their file extension and the DeviceData slot
// their PlatformConstruct calls. One struct serves both, and the two typedefs
// exist so a call site says which one it means.
struct ShaderProgramWithData {
    ShaderProgram base;   // +0x00 (0x50)
    void*         _data;  // +0x50  Video::VertexProgramData* / PixelProgramData*
};
static_assert(sizeof(ShaderProgramWithData) == 0x58,
              "Video::VertexProgram / PixelProgram are 0x58 (MOV EDX,0x58 at "
              "0x18004e82f and 0x18004ebaf)");
static_assert(offsetof(ShaderProgramWithData, _data) == 0x50,
              "_data @ +0x50 (MOV RAX,[RCX+0x50] — ?GetData@VertexProgram@Video@@)");

using VertexProgram = ShaderProgramWithData;
using PixelProgram  = ShaderProgramWithData;

// DeviceData vtable slots that make the platform companion. Same object, two
// different slots; getting these two swapped produces a PixelProgramData behind
// a VertexProgram's vtable, which fails only on the first Bind.
namespace devicedata_vtbl {
    constexpr size_t kCreateVertexProgramData = 0x78;  // CALL [RAX+0x78] @0x1800f6192
    constexpr size_t kCreatePixelProgramData  = 0x80;  // CALL [RAX+0x80] @0x1800f39e2
    constexpr size_t kCreateRenderStateData   = 0x88;  // CALL [RAX+0x88] @0x1800f4c0d
}

// The companion's own vtable, shared by all three *Data classes.
//   slot 0 (+0x00) deleting destructor(flags)
//   slot 1 (+0x08) Create / Rebuild
//   slot 2 (+0x10) Bind
namespace programdata_vtbl {
    constexpr size_t kDeletingDtor = 0x00;
    constexpr size_t kCreate       = 0x08;
    constexpr size_t kBind         = 0x10;
}

} // namespace video
} // namespace rt
