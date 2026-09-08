#pragma once
// ============================================================================
// runtime/types/video/CompanionData.h — Wave 9 lane B2
//
// The twelve `Video::*Data` companion base classes, and the two small owner
// classes whose only unported members dispatch through them.
//
// SHAPE, FROM THE BYTES. Runtime declares one abstract base per renderer
// resource (`DeviceData`, `ContextData`, `VertexBufferData`, ...). The plugin
// — VideoDX11, which in this project is `dx11/` and therefore already ours —
// derives from each one. Every base is the same two-pointer object and every
// ctor/dtor pair is the same three or four instructions, read at the twelve
// addresses listed in runtime/src/video/companion_data.cpp:
//
//     ??0XData(XData* this, X* owner)   this->vtable = ??_7XData; this->owner = owner; return this;
//     ??1XData(XData* this)             this->vtable = ??_7XData;
//
// so ONE struct covers all twelve. That is not an assumption carried over
// from one class to the others: all 24 bodies were disassembled individually
// and each writes its OWN class's exported vtable — the per-class vtable
// addresses are tabulated in companion_data.cpp and each was checked against
// the LEA operand in that class's own ctor and dtor.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace RtVideo {

// ----------------------------------------------------------------------------
// The shared layout of all twelve `Video::*Data` bases — 0x10 bytes.
//
// `owner` is typed `void*` deliberately: it is a `Device*` for `DeviceData`, a
// `VertexBuffer*` for `VertexBufferData` and so on, and none of the ported
// bodies dereference it. They store it and nothing else.
//
// There is no `sizeof` assertion here because 0x10 is a LOWER bound, not a
// measured total: the ctor writes +0x00 and +0x08 and the derived dx11 object
// continues past that (dx11/types/VideoDX11Data.h owns those layouts). The two
// offsets the ported bodies actually touch are asserted.
// ----------------------------------------------------------------------------
struct CompanionData {
    void* vtable;  // +0x00  ??_7<Class>@Video@@6B@, stamped by both ctor and dtor
    void* owner;   // +0x08  the Video-side object this platform record belongs to
};
static_assert(offsetof(CompanionData, vtable) == 0x00, "CompanionData::vtable at +0x00");
static_assert(offsetof(CompanionData, owner) == 0x08, "CompanionData::owner at +0x08");

// ----------------------------------------------------------------------------
// Video::ConstantBuffer — the fields ??0ConstantBuffer@Video@@QEAA@PEAVDevice@1@@Z
// (0x1800eedf0) writes. Only two of them are read by this lane's one ported
// member (`Bind`), but the whole ctor is transcribed so the two offsets are
// anchored in something checkable rather than asserted in isolation:
//
//   1800eee08  MOV [RCX],RAX        \  RAX = 0
//   1800eee0b  MOV [RCX+0x08],RAX    |
//   1800eee0f  MOV [RCX+0x10],-1     |  ctor constants
//   1800eee17  MOV [RCX+0x18],AL     |
//   1800eee1a  MOV [RCX+0x1c],RAX    |  (deliberately unaligned in retail)
//   1800eee1e  MOV [RCX+0x28],RAX    |
//   1800eee22  MOV [RCX+0x30],RAX   /
//   1800eee26  MOV [RCX+0x38],RDX      device
//   1800eee3c  CALL [RAX+0x58]         Device::CreateConstantBufferData(this)
//   1800eee3f  MOV [RBX+0x40],RAX      platformData
//
// No `sizeof` assertion: the ctor's last write is at +0x40, so 0x48 is a lower
// bound and nothing in the shipped binary pins the total. Asserting a guessed
// size would be a static_assert that cannot fail for the right reason.
// ----------------------------------------------------------------------------
struct ConstantBuffer {
    uint8_t  head[0x38];    // +0x00..+0x37  name/prefix Strings, indices, flags — not read here
    void*    device;        // +0x38  Video::Device* (ctor's second parameter)
    void*    platformData;  // +0x40  dx11::ConstantBufferData*, from Device vtable +0x58
};
static_assert(offsetof(ConstantBuffer, device) == 0x38, "ConstantBuffer::device at +0x38");
static_assert(offsetof(ConstantBuffer, platformData) == 0x40, "ConstantBuffer::platformData at +0x40");

}  // namespace RtVideo
