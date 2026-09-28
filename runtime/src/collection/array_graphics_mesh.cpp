// ============================================================================
// runtime/src/collection/array_graphics_mesh.cpp
// The four Collection::Array<T> instantiations underneath Graphics::GraphicsMesh
// (vtable batch 6, 2026-09-12), plus the two element constructors one of them
// is parameterized by.
// ============================================================================
//
//   0x180013fb0  ~Array<GraphicsMesh::TriangleList>      stride 0xb0
//   0x180014910    its per-element destroy
//   0x180014160  ~Array<Collection::Array<T>>            stride 0x10
//   0x180014af0    its per-element destroy
//   0x18005d840   Array<GraphicsMesh::TriangleList>::SetSize(n)
//   0x18005cda0   Array<Collection::Array<T>>::SetSize(n)
//   0x1800fefc0   GraphicsMesh::TriangleList::TriangleList()
//   0x180013c50   GraphicsMesh::TriangleList::TriangleList(const TriangleList&)
//
// WHY A NEW FILE RATHER THAN ROWS IN array_destroy.cpp / array_graphics_anim.cpp
// -----------------------------------------------------------------------------
// Those two are APPEND-ONLY shared files that several agents are editing in the
// same session, and array_destroy.cpp's table is POSITIONAL (its banner records
// a Wave 27 merge that silently gave two lanes the same indices). A new file
// with no table of its own cannot collide. Nothing here is a second copy of a
// Collection algorithm: `SetSizeCore`, `ArrayDtorImpl`, `MemAlloc` and
// `MemFree` are array.cpp's bodies, reached through array_internal.h, exactly
// as array_graphics_anim.cpp reaches them.
//
// GREPPED BEFORE WRITING: all eight addresses were searched across code/,
// engine/, runtime/, standalone/ and dx11/. None is registered anywhere, and
// the only hits are the comments this batch's own graphics files carry.
//
// ---------------------------------------------------------------------------
// GraphicsMesh::TriangleList — 0xb0 bytes
// ---------------------------------------------------------------------------
// Read from the three bodies that cover it end to end (the default ctor's
// stores, the copy ctor's moves, and the element destroy's releases); no field
// is named from a guess.
//
//   +0x00  System::String            refcounted; released by the destroy
//   +0x08  (one qword)               copied; the ctor zeroes it
//   +0x10  float = 10000.0f          0x461c4000, the same constant GraphicsMesh
//                                    itself puts at its +0x48
//   +0x18  Video::VertexBuffer  0x20   PlatformConstruct / PlatformDestruct
//   +0x38  Video::IndexBuffer   0x20   ditto
//   +0x58  Video::VertexBuffer  0x20   ditto
//   +0x78  Video::IndexBuffer   0x20   ditto
//   +0x98  Collection::Array<T>      the index/triangle array (16-byte header)
//   +0xa8  int32 = 1
//
// Each of the four buffer sub-objects gets `dword = 0; byte[+4] = 0` written
// before its PlatformConstruct, and the platform companion each one builds
// lands at ITS +0x08 — which is where `GraphicsMesh::TriangleList::Serialize`
// (graphicsmesh_vtable.cpp §T) reads +0x20/+0x40/+0x60/+0x80 from.
// ============================================================================

#include <cstdint>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/collection/Collection.h"
#include "../../types/video/Buffers.h"   // ENDGAME B.3.5: Video::{Vertex,Index}Buffer
#include "array_internal.h"
#include "array_destroy_internal.h"

namespace rtcoll {
namespace gfxmesh {

constexpr uint32_t kTriStride   = 0xb0;
constexpr uint32_t kInnerStride = 0x10;

// The four Video platform hooks. All four are members of the real
// `Video::VertexBuffer` / `Video::IndexBuffer` (B.3.5), so these four wrappers
// call them directly. They are the buffer's OWN members, not the element
// ctor/dtor helpers B.3.2 left alone: the sub-object at +0x18/+0x38/+0x58/+0x78
// IS a Video buffer, which is why the conversion applies here.
void VertexBufferPlatformConstruct(void* p) {
    reinterpret_cast<Video::VertexBuffer*>(p)->PlatformConstruct();  // B.3.5 bridge
}
void IndexBufferPlatformConstruct(void* p) {
    reinterpret_cast<Video::IndexBuffer*>(p)->PlatformConstruct();   // B.3.5 bridge
}
void VertexBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::VertexBuffer*>(p)->PlatformDestruct();   // B.3.5 bridge
}
void IndexBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::IndexBuffer*>(p)->PlatformDestruct();    // B.3.5 bridge
}

// `Collection::Array<T>::operator=` for the 0x98 index array — retail
// 0x180013640, the copy constructor's only call. UNEXPORTED and NOT YET PORTED,
// so this is an address door that reaches the DLL today and our body the day
// someone ports it (ResolveAddr is registry-first). Named here so the next
// reader can find it rather than meeting a bare hex number.
void InnerIndexArrayAssign(void* dst, const void* src) {
    rtcoll::RtColl_ArrayAssign_180013640(dst, src);
}

// The same job one level up, for `Array<Array<T>>`'s element assign — retail
// 0x180014c70, PORTED. Address door for the same reason (no export name).
void InnerArrayAssign(void* dst, const void* src) {
    rtcoll::RtColl_ArrayAssign_180014c70(dst, src);
}

// ---------------------------------------------------------------------------
// 0x1800fefc0 — GraphicsMesh::TriangleList::TriangleList()
// ---------------------------------------------------------------------------
// Transcribed store for store from 0x1800fefdd .. 0x1800ff039.
//
// *** THE `dword = 0; byte[+4] = 0` PAIR BEFORE EACH PlatformConstruct IS FIVE
// BYTES, NOT EIGHT. *** Retail writes `MOV dword [RCX],EDI` then
// `MOV byte [RCX+4],DIL` and leaves +0x05..+0x07 of each buffer untouched.
// Zeroing the whole qword would be unfaithful and would make any byte-wise
// oracle over those three bytes fail on every call
// (reference_oracle_must_not_compare_unwritten_bytes.md).
//
// Returns `this` (`MOV RAX,RBX`).
void* TriangleListCtor(void* self) {
    uint8_t* const d = static_cast<uint8_t*>(self);

    *reinterpret_cast<uint64_t*>(d + 0x00) = 0;          // the String slot
    *reinterpret_cast<uint64_t*>(d + 0x08) = 0;
    *reinterpret_cast<uint32_t*>(d + 0x10) = 0x461c4000u;   // 10000.0f

    const uint32_t kBuffers[4] = { 0x18, 0x38, 0x58, 0x78 };
    for (int i = 0; i < 4; ++i) {
        uint8_t* const b = d + kBuffers[i];
        *reinterpret_cast<uint32_t*>(b) = 0;
        b[4] = 0;
        if ((i & 1) == 0) VertexBufferPlatformConstruct(b);  // +0x18, +0x58
        else              IndexBufferPlatformConstruct(b);   // +0x38, +0x78
    }

    *reinterpret_cast<uint64_t*>(d + 0x98) = 0;          // the Array header
    *reinterpret_cast<uint64_t*>(d + 0xa0) = 0;
    *reinterpret_cast<uint32_t*>(d + 0xa8) = 1;
    return self;
}

// ---------------------------------------------------------------------------
// 0x180013c50 — GraphicsMesh::TriangleList::TriangleList(const TriangleList&)
// ---------------------------------------------------------------------------
// The String at +0x00 is REFCOUNTED and the copy takes a reference:
//     dst[0] = 0; if (src[0]) { dst[0] = src[0]; ++src[0][-1]; }
// The zero-first is not redundant — it is what makes the null-source case leave
// a null rather than whatever the raw storage held, which matters because this
// ctor writes into memory `SetSizeCore` has just allocated.
//
// Then +0x08 .. +0x90 are copied as SEVENTEEN QWORDS plus one dword at +0x10,
// verbatim — including the four buffer sub-objects, whose platform companion
// POINTERS are therefore SHARED with the source, not duplicated. That is
// retail's behaviour and it is why a copied TriangleList must not outlive its
// original; recorded, not corrected.
//
// The Array at +0x98 is zeroed and then assigned through 0x180013640, the
// zero-then-operator= order every nested Array copy in this DLL uses (the
// assignment starts by destroying what the destination holds).
//
// +0xa8 is copied last. +0xac..+0xaf are NEVER WRITTEN.
void* TriangleListCopyCtor(void* dst, const void* src) {
    uint8_t* const d = static_cast<uint8_t*>(dst);
    const uint8_t* const s = static_cast<const uint8_t*>(src);

    *reinterpret_cast<uint64_t*>(d) = 0;
    uint8_t* const name = *reinterpret_cast<uint8_t* const*>(s);
    if (name != nullptr) {
        *reinterpret_cast<uint8_t**>(d) = name;
        ++*(reinterpret_cast<int32_t*>(name) - 1);
    }

    *reinterpret_cast<uint64_t*>(d + 0x08) = *reinterpret_cast<const uint64_t*>(s + 0x08);
    *reinterpret_cast<uint32_t*>(d + 0x10) = *reinterpret_cast<const uint32_t*>(s + 0x10);
    for (uint32_t o = 0x18; o <= 0x90; o += 8)
        *reinterpret_cast<uint64_t*>(d + o) = *reinterpret_cast<const uint64_t*>(s + o);

    *reinterpret_cast<uint64_t*>(d + 0x98) = 0;
    *reinterpret_cast<uint64_t*>(d + 0xa0) = 0;
    InnerIndexArrayAssign(d + 0x98, s + 0x98);

    *reinterpret_cast<uint32_t*>(d + 0xa8) = *reinterpret_cast<const uint32_t*>(s + 0xa8);
    return dst;
}

// ---------------------------------------------------------------------------
// 0x180014910 — the per-element destroy of Array<TriangleList>
// ---------------------------------------------------------------------------
// `TEST R8D,R8D / JLE` first: a count of zero (or negative) does nothing at
// all, not even a virtual call. `MOVSXD R14,R8D` — the counter is the SIGN-
// extended 32-bit argument.
//
// PER ELEMENT, IN RETAIL'S ORDER (which is neither ascending nor descending by
// offset, and is transcribed rather than sorted):
//     +0x98  free the Array buffer, then zero its 16-byte header —
//            UNCONDITIONALLY, including when the buffer was already null
//     +0x78  IndexBuffer::PlatformDestruct
//     +0x58  VertexBuffer::PlatformDestruct
//     +0x38  IndexBuffer::PlatformDestruct
//     +0x18  VertexBuffer::PlatformDestruct
//     +0x00  release the String
//
// THE STRING RELEASE'S TWO ARMS LEAVE THE SLOT HOLDING DIFFERENT THINGS —
// null when the string survives, `str - 8` when it does not — which is the
// asymmetry array_destroy.cpp's Body B documents and the only part of this a
// comparator can see. Reproduced rather than reached through
// `ReleaseStringsCore`, because that helper walks its own loop over the whole
// range and would visit the strings in a separate pass, changing the order of
// allocator calls relative to the four PlatformDestructs.
void DestroyTriangleListRange(void*, uint8_t* base, int count) {
    if (count <= 0) return;                                  // TEST R8D,R8D / JLE
    const int64_t n = static_cast<int64_t>(count);           // MOVSXD

    for (int64_t i = 0; i < n; ++i) {
        uint8_t* const e = base + i * kTriStride;

        uint8_t** const buf = reinterpret_cast<uint8_t**>(e + 0x98);
        if (*buf != nullptr) MemFree(*buf);
        *reinterpret_cast<uint64_t*>(e + 0x98) = 0;          // unconditional
        *reinterpret_cast<uint64_t*>(e + 0xa0) = 0;

        IndexBufferPlatformDestruct(e + 0x78);
        VertexBufferPlatformDestruct(e + 0x58);
        IndexBufferPlatformDestruct(e + 0x38);
        VertexBufferPlatformDestruct(e + 0x18);

        uint8_t** const nameSlot = reinterpret_cast<uint8_t**>(e);
        uint8_t* const name = *nameSlot;
        if (name == nullptr) continue;
        int32_t* const refs = reinterpret_cast<int32_t*>(name) - 1;
        --*refs;
        if (*refs != 0) {
            *nameSlot = nullptr;                             // MOV [RSI],R15 (== 0)
        } else {
            *nameSlot = name - 8;                            // ADD qword [RSI],-8
            if (*nameSlot != nullptr) MemFree(*nameSlot);    // the JZ on that ADD
        }
    }
}

// ---------------------------------------------------------------------------
// 0x180014af0 — the per-element destroy of Array<Array<T>>
// ---------------------------------------------------------------------------
// One inner Array per 0x10 element at offset 0: free the buffer, zero the
// header. That is array_destroy.cpp's BODY A with stride 0x10 and one offset,
// so it is reached through `FreeInnerArraysCore` and the row below rather than
// written out again.
const DestroySpec kD_InnerArr_10 = { "Array<Collection::Array<T>>", 0x10, 1, { 0x00, 0, 0 } };

void DestroyInnerArrayRange(void*, uint8_t* base, int count) {
    FreeInnerArraysCore(base, count, &kD_InnerArr_10);
}

// ---------------------------------------------------------------------------
// The two new-tail initialisers, i.e. the `else` limb of each fill loop.
// ---------------------------------------------------------------------------
void InitTriangleList(uint8_t* p) { TriangleListCtor(p); }   // CALL 0x1800fefc0

// 0x18005cda0's arms BOTH open with `[rcx] = 0; [rcx+8] = 0`; the copy arm then
// calls 0x180014c70 over the top. So the init is the two zero stores alone and
// the assign is zero-then-assign — which is why `AssignInnerArray` below repeats
// them instead of delegating.
void InitInnerArray(uint8_t* p) {
    *reinterpret_cast<uint64_t*>(p + 0) = 0;
    *reinterpret_cast<uint64_t*>(p + 8) = 0;
}

void AssignTriangleList(uint8_t* d, const uint8_t* s) { TriangleListCopyCtor(d, s); }

void AssignInnerArray(uint8_t* d, const uint8_t* s) {
    *reinterpret_cast<uint64_t*>(d + 0) = 0;
    *reinterpret_cast<uint64_t*>(d + 8) = 0;
    InnerArrayAssign(d, s);
}

// ---------------------------------------------------------------------------
// The specs. Field order is array_internal.h's:
//   name, stride, align, assign, init, destroy, byteCopy,
//   assignUnwritten{Lo,Hi}, initUnwritten{Lo,Hi}, seedZero{Lo,Hi}
//
// Both alignments are 8: `MOV EDX,0x8` at 0x18005d8a3 and 0x18005ce02.
// ---------------------------------------------------------------------------
constexpr ArraySpec kSpec_TriangleList = {
    "Array<Graphics::GraphicsMesh::TriangleList>", kTriStride, 8,
    &AssignTriangleList, &InitTriangleList, &DestroyTriangleListRange, false,
    0xac, 0xb0,    // the copy ctor's last store ends at +0xac
    0xac, 0xb0,    // ... and so does the default ctor's
    0x00, 0x08 };  // the name slot is refcounted — a probe must seed it null

constexpr ArraySpec kSpec_InnerArray = {
    "Array<Collection::Array<T>>", kInnerStride, 8,
    &AssignInnerArray, &InitInnerArray, &DestroyInnerArrayRange, false,
    0, 0,
    0, 0,
    0x00, 0x10 };  // the whole element is an Array header and IS dereferenced

static_assert(kSpec_TriangleList.initUnwrittenHi <= kSpec_TriangleList.stride,
              "TriangleList init hole must lie inside its own stride");
static_assert(kSpec_TriangleList.seedZeroHi <= kSpec_TriangleList.stride,
              "TriangleList seed window must lie inside its own stride");
static_assert(kSpec_InnerArray.seedZeroHi <= kSpec_InnerArray.stride,
              "inner-array seed window must lie inside its own stride");

}  // namespace gfxmesh
}  // namespace rtcoll

// ============================================================================
// The registered entry points. One free function per ADDRESS, with the address
// in the name so a crash log or a census line names it directly.
// ============================================================================
namespace {

using rtcoll::ArrayHdr;
using namespace rtcoll::gfxmesh;

void* Rt_ArrayDtor_180013fb0(void* a) {       // ~Array<TriangleList>
    // ENDGAME B.3.2: the element destroy is kSpec_TriangleList's own
    // `destroy` (DestroyTriangleListRange), so ~Array runs exactly the two
    // steps this body used to spell out. The `this` retail passes the helper
    // is spilled and never read (the helper's first parameter is unnamed).
    rtcoll::AsArray<&kSpec_TriangleList>(a)->~Array();
    return nullptr;
}
void* Rt_ArrayDtor_180014160(void* a) {       // ~Array<Array<T>>
    rtcoll::AsArray<&kSpec_InnerArray>(a)->~Array();
    return nullptr;
}

// Retail's arity for the two element-destroy helpers, including the `this`
// each spills to the shadow space and never reads (array_destroy.cpp's
// "ignoredThis" note applies unchanged).

void Rt_SetSize_18005d840(void* a, int n) {   // Array<TriangleList>
    rtcoll::AsArray<&kSpec_TriangleList>(a)->SetSize(n);
}
void Rt_SetSize_18005cda0(void* a, int n) {   // Array<Array<T>>
    rtcoll::AsArray<&kSpec_InnerArray>(a)->SetSize(n);
}

}  // namespace

// Every one of the six Collection rows is a template instantiation with NO
// export name in either shipped build (checked against the shipped export
// directory, not assumed), so all six are RegisterRuntimeAddr and none is a
// RegisterRuntimeExport. The two TriangleList constructors are unexported too —
// `??0TriangleList@GraphicsMesh@Graphics@@QEAA@XZ` IS in the directory, but at
// 0x1800125a0, which is a DIFFERENT function from the 0x1800fefc0 this array's
// fill loop calls; registering our body under that name would bind it to the
// wrong address. Addresses only.
// ENDGAME B.3.2 closing pass (2026-09-18): the two per-element destroy wrappers
// (0x180014910 TriangleLists, 0x180014af0 inner Arrays) and their rows were
// removed at zero callers — nothing resolves those addresses in any form, and
// their cores (DestroyTriangleListRange / DestroyInnerArrayRange) stay, called
// from this file's own dtor bodies. So the banner above now reads "six rows"
// for four; the two it counted are these.
int RtCollectionGraphicsMeshRegister() {
    int n = 0;
    RegisterRuntimeAddr(0x180013fb0ull, (void*)&Rt_ArrayDtor_180013fb0);            ++n;
    RegisterRuntimeAddr(0x180014160ull, (void*)&Rt_ArrayDtor_180014160);            ++n;
    RegisterRuntimeAddr(0x18005d840ull, (void*)&Rt_SetSize_18005d840);              ++n;
    RegisterRuntimeAddr(0x18005cda0ull, (void*)&Rt_SetSize_18005cda0);              ++n;
    return n;
}
