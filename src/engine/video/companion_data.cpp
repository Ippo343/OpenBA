// ============================================================================
// src/engine/video/companion_data.cpp — Wave 9 lane B2
//
//   the twelve Video::*Data companion base classes (ctor + dtor),
//   Video::IndexBuffer::Lock/Unlock, Video::VertexBuffer::Lock/Unlock,
//   Video::ConstantBuffer::Bind, and Video::Device::DisplayMode's ctor/dtor.
//
// 29 functions, closing 16 classes. Layouts and the evidence for every offset
// are in src/engine/video/CompanionData.h and src/engine/video/Device.h.
// The lane report is docs/runtime_port/WAVE9_LANE_B2.md.
// ============================================================================
//
// WHY THESE, AND WHY TOGETHER. Every function in this file is reached from
// `src/render/dx11/`, our VideoDX11 replacement, and from nowhere else in the tree.
// WAVE9.md §4 records that `src/render/dx11/` is part of the cutover surface and was
// invisible to the census until 2026-09-07; these 29 are the cheapest part of
// that surface. Sixteen classes go to 100% in one file, which is what PLAN
// §4.1 decision 5 (class atomicity) asks for: an object built by our ctor
// carries our vtable and must then find every method ours.
//
// EVERY BODY BELOW WAS READ AS DISASSEMBLY, NOT DECOMPILED C
// (reference_ghidra_c_output_vs_disassembly.md). The twelve ctors and twelve
// dtors are three and four instructions each, and all 24 were disassembled
// INDIVIDUALLY rather than one being read and the rest assumed — the point of
// checking was the vtable operand, which differs per class and is the only
// thing that can be wrong. The table:
//
//   | Class              | ctor        | dtor        | ??_7 (LEA operand, both) |
//   |--------------------|-------------|-------------|--------------------------|
//   | RenderBufferData   | 0x1800f8b70 | 0x1800f8db0 | 0x180158a28              |
//   | VideoProgramData   | 0x1800f8b90 | 0x1800f8d90 | 0x180158a48              |
//   | RenderStateData    | 0x1800f8bb0 | (ported)    | 0x180158a60              |
//   | PixelProgramData   | 0x1800f8bd0 | 0x1800f8dd0 | 0x180158a78              |
//   | VertexProgramData  | 0x1800f8bf0 | 0x1800f8d10 | 0x180158a90              |
//   | VertexLayoutData   | 0x1800f8c10 | 0x1800f8d30 | 0x180158aa8              |
//   | VertexBufferData   | 0x1800f8c30 | 0x1800f8d50 | 0x180158ac0              |
//   | IndexBufferData    | 0x1800f8c50 | 0x1800f8df0 | 0x180158af8              |
//   | ConstantBufferData | 0x1800f8c70 | 0x1800f8e50 | 0x180158b30              |
//   | ImageBufferData    | 0x1800f8c90 | (ported)    | 0x180158b50              |
//   | ContextData        | 0x1800f8cb0 | 0x1800f8e30 | 0x180158b90              |
//   | DeviceData         | 0x1800f8cd0 | 0x1800f8cf0 | 0x180158c30              |
//
// The two "(ported)" dtors are NOT re-registered here — `??1ImageBufferData`
// landed in buffers.cpp (Wave 6 lane C) and `??1RenderStateData` in device.cpp
// (Wave 6 lane B). Registering a name twice is not an error the linker or the
// registry reports: the later binding silently wins and the earlier line
// becomes dead code, which is the SHADOWED case `tools/dispatch_slot_map.py
// --check` reports on the exe side. Those two classes complete here on their
// ctor alone.
//
// ALL 24 MANGLED NAMES WERE READ OUT OF THE SHIPPED EXPORT DIRECTORY, not
// reconstructed by hand (`tools/pe_exports.py` over
// C:\GOG Games\Banished\Runtime-x64.dll). That matters more than usual here:
// the dtors are `UEAA` (virtual) and DisplayMode's is `QEAA` (non-virtual),
// and reference_phantom_dtor_mangling_qeaa_vs_ueaa.md is exactly the bug of
// guessing the wrong one — a name the DLL does not contain resolves NULL
// silently, the port registers, the census counts it and the diff reports
// calls=0.
//
// ORACLE. Full differential on 25 of the 29, and it is a real one rather than
// a tautology. Our ctor writes the vtable pointer this file resolves by
// exported name, where retail writes an absolute LEA; those agree only if the
// export name is right and resolves, which is the phantom-name failure mode
// above, caught here rather than at cutover.
// Retail's ctor is replayed into a PRIVATE 16-byte scratch (never into the
// live object) and the two are compared byte for byte. This is sound because
// the ctor is a pure write of two fields: it reads no prior contents of
// `self`, so ours having already run cannot change what theirs produces, and
// its only input is the scalar `owner`, which nothing aliases —
// reference_oracle_must_snapshot_aliased_inputs.md's trap (a call site passing
// one buffer as both `self` and `ret`) cannot arise. The dtors replay the same
// way into a bare `void*`, following device.cpp's `??1RenderStateData` port.
//
// The remaining four are trampolines (Lock/Unlock/Bind, DisplayMode's dtor)
// and carry the diff oracle with the reason stated at each one.
//
// NEW LANE-D ROW: exactly one, `0x180010760`, named below.
// ============================================================================
//
// ENDGAME B.3.9 block VID (B39_RULES D1, section 5 item 4): the twelve classes
// are CONCRETE (CompanionData.h). The twelve ctor DOORS that dx11 reached through
// its import struct (B9-6) are GONE since B5 W0: B.3R had already moved the
// construction into dx11's factories, so no caller was left. The minted-table accessors,
// the hand stores of the MINTED `??_7XData` in the ctors and the dtors, and the
// ten `??1XData` doors below (their one caller was the RT_DELDTOR row) are GONE
// with the twelve mint rows. The table above is the retail evidence and stays.

#include <cstdint>
#include <cstring>
#include <new>

#include "video/Buffers.h"
#include "video/CompanionData.h"
// ENDGAME B.3.5 block D1 — `Video::Context`, for the one line
// `Video::ConstantBuffer::Bind` takes it by reference in.
#include "video/Context.h"
#include "video/Device.h"

using DisplayMode = Video::Device::DisplayMode;

// ============================================================================
// The twelve companion-base constructors are INLINE in CompanionData.h (the owner
// store at +0x08 and the compiler's vptr):
//
//   1800f8cd0  LEA RAX,[0x180158c30]      ; ??_7DeviceData@Video@@6B@
//   1800f8cd7  MOV qword ptr [RCX+0x8],RDX
//   1800f8cdb  MOV qword ptr [RCX],RAX
//   1800f8cde  MOV RAX,RCX
//   1800f8ce1  RET
//
// B5 (the dx11 seam) W0: the twelve `Video_XData_Ctor` DOORS that used to sit here
// had no caller left -- B.3R moved the construction into dx11's factories
// (`::new (p) Video::XData(owner)`), which the B5 subclasses' own constructors now
// chain to. Deleted.
// ============================================================================

// ============================================================================
// The twelve member bodies -- `Video::XData::~XData()`.
//
//   1800f8cf0  LEA RAX,[0x180158c30]      ; ??_7DeviceData@Video@@6B@
//   1800f8cf7  MOV qword ptr [RCX],RAX
//   1800f8cfa  RET
//
// The whole of retail's body is the base-class vptr re-stamp, which is the
// compiler's own entry store now (ENDGAME B.3.9 VID: no hand store of the
// minted table). These bases own no memory; the derived dx11 object's slot 0
// does the MemFree and calls this as its base-destructor step (RULE B5-3). The
// `owner` field at +0x08 is deliberately LEFT ALONE, as retail leaves it.
// ============================================================================
namespace Video {

RenderBufferData::~RenderBufferData()     {}
VideoProgramData::~VideoProgramData()     {}
PixelProgramData::~PixelProgramData()     {}
VertexProgramData::~VertexProgramData()   {}
VertexLayoutData::~VertexLayoutData()     {}
VertexBufferData::~VertexBufferData()     {}
IndexBufferData::~IndexBufferData()       {}
ConstantBufferData::~ConstantBufferData() {}
ContextData::~ContextData()               {}
DeviceData::~DeviceData()                 {}
ImageBufferData::~ImageBufferData()       {}
RenderStateData::~RenderStateData()       {}

}  // namespace Video

// ============================================================================
// Video::IndexBuffer::Lock / Unlock  —  0x1800f34e0 / 0x1800f34c0
// Video::VertexBuffer::Lock / Unlock —  0x1800f5640 / 0x1800f5620
//
//   1800f34e0  MOV RCX,qword ptr [RCX+0x8]   ; this->platformData
//   1800f34e4  MOV RAX,qword ptr [RCX]       ; its vtable
//   1800f34e7  JMP qword ptr [RAX+0x18]      ; slot 3
//
// Unlock is the same three instructions through slot +0x20 (slot 4). All four
// are pure tail-call trampolines into the platform buffer object at +0x08 —
// dx11::IndexBufferData / VertexBufferData, src/render/dx11/VideoDX11Data.h — and
// the index/vertex pair use identical slots, which is why one comment covers
// both. This completes the shape Wave 6 lane C started: Create and Set are
// already ported here through slots +0x08 and +0x10 (buffers.cpp).
//
// ORACLE: the diff oracle, not a replay, and the reason is not caution. The far
// side of the dispatch is dx11's vtable, and dx11 is ALREADY OURS — there is
// no second implementation to diverge from. A replay would additionally be
// unsound: Lock/Unlock are a matched pair over a mapped GPU buffer, and
// calling Lock twice or Unlock on an already-unlocked buffer is exactly the
// mutate-twice trap WAVE9.md describes for Audio::Scene::Pause.
// ============================================================================

namespace Video {

void* IndexBuffer::Lock(int sizeBytes, int& outByteOffset) {
    return platformData->Lock(sizeBytes, &outByteOffset);
}

void IndexBuffer::Unlock() {
    platformData->Unlock();
}

void* VertexBuffer::Lock(int sizeBytes, int& outByteOffset) {
    return platformData->Lock(sizeBytes, &outByteOffset);
}

void VertexBuffer::Unlock() {
    platformData->Unlock();
}

}  // namespace Video

// ============================================================================
// Video::ConstantBuffer::Bind  —  0x1800eef70
//
//   1800eef70  MOV RCX,qword ptr [RCX+0x40]  ; this->platformData
//   1800eef74  MOV R10,qword ptr [RCX]
//   1800eef77  JMP qword ptr [R10+0x10]      ; slot 2
//
// Same trampoline shape as the four above, through +0x40 rather than +0x08
// (the field the ConstantBuffer ctor at 0x1800eedf0 fills from the Device
// factory — see CompanionData.h). Five arguments, so the fifth arrives on the
// stack; retail's tail JMP preserves the frame and so does an ordinary call
// here, since the signature matches exactly.
// ============================================================================

// ============================================================================
// Video::Device::DisplayMode::DisplayMode(int, int)  —  0x18000d300
//
//   18000d311  MOV dword ptr [RCX],EDX        ; width
//   18000d313  MOV dword ptr [RCX+0x4],R8D    ; height
//   18000d317  XOR EAX,EAX
//   18000d319  MOV qword ptr [RCX+0x8],RAX    ; Array<RefreshRate>::_data
//   18000d31d  MOV qword ptr [RCX+0x10],RAX   ; _size, _capacity (one qword)
//
// (The `MOV qword ptr [RSP],-2` at 18000d309 is the SEH unwind cookie, not a
// field write.) Pure constants and two scalar parameters: a full replay into a
// private scratch DisplayMode is sound, and unlike the companion ctors this
// one is worth the whole 0x18 bytes because the two zeroing stores span three
// named fields.
// ============================================================================
void* Video_DisplayMode_Ctor(DisplayMode* self, int width, int height) {
    self->width           = width;
    self->height          = height;
    self->refreshData     = nullptr;
    self->refreshCount    = 0;
    self->refreshCapacity = 0;
    return self;
}

// ============================================================================
// Video::Device::DisplayMode::~DisplayMode  —  0x18000cb80
//
//   18000cb92  ADD RCX,0x8
//   18000cb9a  JMP 0x180010760
//
// The entire body is a tail call to the `Collection::Array<T>::~Array`
// instantiation for `Array<RefreshRate>`: free `_data` under the allocator's
// critical section, then zero the {data,size,capacity} triple.
//
// 0x180010760 is `RtVideo_ArrayRefreshDtor` (device_startup.cpp, declared in
// types/video/Device.h), called by name.
// ============================================================================
void Video_DisplayMode_Dtor(DisplayMode* self) {
    RtVideo_ArrayRefreshDtor(self->refreshData, self->refreshCount, self->refreshCapacity);
}

