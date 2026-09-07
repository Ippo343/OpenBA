#pragma once
// Video::IndexBuffer / Video::VertexBuffer — binary-compatible struct
// definitions. Runtime-x64.dll classes, 0x20 bytes each, identical layout.
//
// Neither class carries a vtable of its own — Create/Set/Bind are thin
// dispatches through the platform buffer object at +0x08 (dx11's
// IndexBufferData / VertexBufferData, see dx11/types/VideoDX11Data.h), which
// this lane does not own and does not duplicate here.
//
// Decompiled from (Wave 6 lane C):
//   IndexBuffer ctor:   0x1800f3490 (??0IndexBuffer@Video@@QEAA@XZ)
//   VertexBuffer ctor:  0x1800f55f0 (??0VertexBuffer@Video@@QEAA@XZ)
//   IndexBuffer::Create:  0x1800f3880 (?Create@IndexBuffer@Video@@QEAA_NH_NPEAX@Z)
//   VertexBuffer::Create: 0x1800f59e0 (?Create@VertexBuffer@Video@@QEAA_NH_NPEAX@Z)
//   IndexBuffer::Set:     0x1800f38a0 (?Set@IndexBuffer@Video@@QEAA_NHHPEAX@Z)
//   VertexBuffer::Set:    0x1800f5a00 (?Set@VertexBuffer@Video@@QEAA_NHHPEAX@Z)
//   IndexBuffer::GetSize (not this lane's scope, used only to confirm +0x00): 0x180011e00
//   VertexBuffer::GetSize (not this lane's scope, same confirmation):        0x180011db0
//

#include <cstdint>
#include <cstddef>

// PlatformConstruct/PlatformDestruct (0x1800f3680/0x1800f3760 for IndexBuffer,
// the VertexBuffer twins are not in this lane's 22-function scope) additionally
// maintain an intrusive doubly-linked list of every live buffer of that type,
// rooted at the exported `_firstBuffer` static and guarded by
// Device::_singleton's critical section at +0x60. That list is NOT
// reproduced here: PlatformConstruct/PlatformDestruct are not in this lane's
// scope (our tree never calls them directly — only Create/Set, which reach
// the platform object without touching the list), so the fields exist only
// to keep the struct's layout — and therefore Create/Set's `this+8` and the
// ctor's byte offsets — correct.
struct IndexBuffer {
    int32_t  size;          // +0x00  index count (Create's first param); zeroed in ctor
    bool     dynamic;        // +0x04  zeroed in ctor
    uint8_t  pad05[3];
    void*    platformData;  // +0x08  dx11::IndexBufferData* (or equivalent), set by PlatformConstruct
    void*    listPrev;       // +0x10  intrusive list (Device's live-buffer registry); not touched by Create/Set
    void*    listNext;        // +0x18
};
static_assert(sizeof(IndexBuffer) == 0x20, "IndexBuffer must be 0x20 bytes");
static_assert(offsetof(IndexBuffer, size) == 0x00, "size at +0x00");
static_assert(offsetof(IndexBuffer, dynamic) == 0x04, "dynamic at +0x04");
static_assert(offsetof(IndexBuffer, platformData) == 0x08, "platformData at +0x08");
static_assert(offsetof(IndexBuffer, listPrev) == 0x10, "listPrev at +0x10");
static_assert(offsetof(IndexBuffer, listNext) == 0x18, "listNext at +0x18");

// Layout-identical to IndexBuffer (confirmed: both ctors write the same byte
// pattern, both Create/Set dispatch through the same +0x08/vtable+0x08 and
// +0x08/vtable+0x10 shape).
struct VertexBuffer {
    int32_t  size;
    bool     dynamic;
    uint8_t  pad05[3];
    void*    platformData;
    void*    listPrev;
    void*    listNext;
};
static_assert(sizeof(VertexBuffer) == 0x20, "VertexBuffer must be 0x20 bytes");
static_assert(offsetof(VertexBuffer, size) == 0x00, "size at +0x00");
static_assert(offsetof(VertexBuffer, dynamic) == 0x04, "dynamic at +0x04");
static_assert(offsetof(VertexBuffer, platformData) == 0x08, "platformData at +0x08");
static_assert(offsetof(VertexBuffer, listPrev) == 0x10, "listPrev at +0x10");
static_assert(offsetof(VertexBuffer, listNext) == 0x18, "listNext at +0x18");
