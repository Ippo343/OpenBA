// ============================================================================
// runtime/src/video/context_lifecycle.cpp — Wave 9 lane VID2
//   Video::Context::ClearResources, Video::Context::~Context and the five
//   destruction helpers the dtor reaches that nothing else in the tree owned.
// ============================================================================
//
// WHY THIS FILE EXISTS
// --------------------
// `??1Context@Video@@QEAA@XZ` and `?ClearResources@Context@Video@@QEAAXXZ` are
// two of the 43 names `RT_STRICT` recorded unresolved on cutover boot
// attempt 14 (docs/runtime_port/wave9_attempt14_unresolved.txt). Lane VID
// classified both as RESOLVE-ONLY and left them, which was correct as far as it
// went: `engine/funcs/engine_funcs.cpp` resolves both and nothing calls either.
//
// What that classification missed is the SHAPE OF THE NEIGHBOURHOOD, which the
// integrator's address-door census (docs/runtime_port/wave9_unregistered_addr_doors.csv)
// makes plain. The `Video::Context` lifecycle reaches the DLL through TWO
// DIFFERENT DOORS:
//
//     0x1800f1820  the ctor                 (was engine_funcs's fn_VideoCtx_Ctor,
//                                               resolved BY ADDRESS)
//     ??1Context@Video@@QEAA@XZ  the dtor      (was fn_VideoCtx_Dtor, by name with
//                                               a 0x1800f16e0 address fallback)
//     0x180018500  ??3Context@Video@@           (was fn_VideoCtx_Delete, by address)
//
// The three `fn_VideoCtx_*` pointers this paragraph used to cite BY LINE NUMBER
// no longer exist: all of them reached zero callers and ENDGAME B.3.5 batch
// b35-K2 (41baab1c) deleted the whole Context lifecycle group from
// engine/funcs/engine_funcs.{h,cpp}. THE ADDRESSES AND THE ARGUMENT ARE
// UNCHANGED -- what is described below is the DLL's own lifecycle and the doors
// this file registers, neither of which depended on those pointers.
//
// and `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` — which is on the LIVE
// BOOT PATH, called from `RtEngine_MainLoop_Ctor` — drives the dtor, the
// deleting operator and the ctor by direct near CALL. At cutover all three
// doors are null. So the dtor is not "resolve-only" in any sense that survives
// the DLL going away; it is one third of a live lifecycle.
//
// This file ports the DESTRUCTION half of that lifecycle and registers the
// address door for the dtor alongside the name door. The construction half
// (the 0x1800f1820 ctor, ~400 instructions building seven sub-objects) is NOT
// here — see the "WHAT THIS FILE DOES NOT DO" section at the bottom, which
// states the remaining gap precisely rather than implying it is closed.
//
// ORACLE, HONESTLY
// ----------------
// `ClearResources` has a FULL oracle and a probe that drives it: see
// `RtVideoContextLifecycleProbe` below. It is a pure state transformation on
// the Context with no allocation and no call, so snapshot / ours / restore /
// theirs / compare is exact, and the probe supplies the callers the tree does
// not have (reference_uncalled_port_is_not_verified.md).
//
// The DESTRUCTORS have NO ORACLE and this lane does not pretend otherwise.
// Running ours and then theirs frees every sub-object twice; the second pass
// operates on freed memory. That is Wave 6's argument for the same family and
// it is unchanged. Their correctness rests on transcription from disassembly,
// which is stated here rather than buried.
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/video/Buffers.h"
#include "../../types/video/Context.h"
#include "../../types/video/Device.h"
// ENDGAME B.3.5 block VC — `Video::{Context,ConstantBuffer,ImageBuffer}Data`'s
// slot names, and `Video::ImageBuffer` for the two default images' slot 0.
#include "../../types/video/CompanionData.h"
#include "../../types/video/ImageBuffer.h"

// ENDGAME B.3.5 block D1. `?ClearResources@` MOVED onto `Video::Context`; its
// door keeps its name and both its rows. THE TWO DESTRUCTORS DO NOT MOVE and
// that is a rule, not a shortcut: `??1Context@Video@@` and
// `??1ConstantBuffer@Video@@` stay registered DOORS over free functions, because
// a user-provided destructor on either class would destroy
// `std::is_trivially_copyable` — `Video::Context` holds seven ConstantBuffers by
// value and nine files treat the 0x2168 bytes as a flat block (RULE B5-4, and
// B4b-2 for the ctor side). Both asserts are in runtime/types/video/Context.h
// and ConstantBuffer.h.
using Video::Context;
using RtVideo::Device;
using RtVideo::RenderBuffer;

// The Win32 half of Video::Device's recursive render lock (buffers_win.cpp,
// Wave 9 lane W). `RenderBuffer::PlatformDestruct` takes it by INLINE COPY of
// the singleton-reading form — Enter, owner = GetCurrentThreadId(), depth++;
// then depth--, owner = -1 at zero, Leave with the singleton RE-READ. Those are
// exactly these two bodies, instruction for instruction (0x1800f4431..0x1800f4447
// and 0x1800f44a9..0x1800f44d7), which is why they are reused rather than
// transcribed a fifth time.
void RtVideoDeviceLock(void** deviceSingleton);
void RtVideoDeviceUnlock(void** deviceSingleton);

// The three `Platform*` siblings live in buffers.cpp (Wave 6 lane C / Wave 9
// lanes W and GFX2). Retail's dtor reaches them by direct near CALL; ours reaches
// the C++ MEMBERS — ENDGAME B.3.5 block VC — so no resolver and no free-function
// door sits on the path at all, and the three `extern` declarations that used to
// be here are gone.
//
// (Wave 20 Phase 0c's warning is kept because it is still the reason there is
// exactly one body: this file used to carry its own copy of
// `RenderBuffer::PlatformDestruct`, and because RtVideoContextLifecycleRegister
// runs AFTER RtVideoBuffersRegister, the copy won both doors and lane C's
// instrumented body was shadowed dead code the census still counted as ported.
// The two were behaviourally equivalent, so nothing was ever observably wrong;
// it was one edit away from being wrong.)

namespace {

// `DeviceSlot()` and `FirstRenderBufferSlot()` used to live here. They had
// exactly one caller between them -- this file's duplicate copy of
// RenderBuffer::PlatformDestruct -- and went unused when that copy was deleted
// (Wave 20 Phase 0c), which -Wextra reported. buffers.cpp reaches the same two
// data slots through its own `DeviceSingletonPtr()` and
// `RenderBufferFirstRenderBuffer()`, so nothing is lost:
//   ?_singleton@Device@Video@@0PEAV12@EA
//   ?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA

// ---------------------------------------------------------------------------
// The scalar-deleting-dtor call every one of these bodies makes.
//
//     MOV RAX,[RCX] / MOV EDX,1 / CALL [RAX]
//
// i.e. vtable slot 0 with flag 1 — the MSVC "deleting destructor". Every
// companion object (`ContextData`, `ImageBuffer`, `ConstantBufferData`) is
// released this way. The deref is UNGUARDED past the null test retail already
// makes, exactly as retail leaves it.
// ---------------------------------------------------------------------------
// ENDGAME B.3.5 block VC — the free-function `DeletingDtor(void*)` helper is
// GONE. Its three call sites in `~Context` are virtual `DeletingDtor(1)` calls on
// `Video::ContextData` and `Video::ImageBuffer`, both of which declare slot 0
// under that exact name, and `~ConstantBuffer` makes the same call on
// `Video::ConstantBufferData`.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// The System::String release the ConstantBuffer dtor and 0x18001cfc0 both
// carry inline, four times between them. Retail, at 0x1800eef14:
//
//     MOV RAX,[RBX+0x8]        ; the string's data pointer
//     TEST RAX,RAX / JZ done
//     DEC dword ptr [RAX-0x4]  ; refcount lives 4 bytes BEFORE the data
//     CMP dword ptr [RAX-0x4],0
//     JNZ .clear
//     ADD qword ptr [RBX+0x8],-0x8   ; back the FIELD up to the block start
//     MOV RCX,[RBX+0x8] / CALL MemFree
//     JMP done
//   .clear:
//     MOV qword ptr [RBX+0x8],0
//
// TWO quirks are deliberate and both are reproduced:
//   * the LAST-REFERENCE branch does NOT null the field. It rewinds it by 8 and
//     frees; the field is left pointing at freed memory. Retail relies on the
//     object being dead.
//   * the SHARED branch (refcount still non-zero) DOES null it.
// Anything that "tidies" either of those changes what the next reader sees.
// ---------------------------------------------------------------------------
void ReleaseString(void** field) {
    uint8_t* data = static_cast<uint8_t*>(*field);
    if (!data) return;
    int32_t* rc = reinterpret_cast<int32_t*>(data - 4);
    *rc -= 1;
    if (*rc != 0) { *field = nullptr; return; }
    *field = data - 8;
    System::MemoryAllocator::MemFree(*field);
}

// The 0x18001cfc0 spelling of the same release differs in ONE respect and it is
// not cosmetic: after rewinding the field by 8 it re-reads it and SKIPS the
// free when the result is null (`ADD [RBX+8],-8 ; MOV RSI,[RBX+8] ; JZ`, the ZF
// coming from the ADD). It also does not null the field in that branch. The
// 0x1800c7ad0 spelling used by ReleaseString has the null test INSIDE MemFree,
// so the two agree on behaviour; they are written apart because the flag
// dependency above is the kind of thing a later reader would "simplify".
void ReleaseStringNoTidy(void** field) {
    uint8_t* data = static_cast<uint8_t*>(*field);
    if (!data) return;
    int32_t* rc = reinterpret_cast<int32_t*>(data - 4);
    *rc -= 1;
    if (*rc != 0) { *field = nullptr; return; }
    *field = data - 8;
    if (*field) System::MemoryAllocator::MemFree(*field);
}

// ---------------------------------------------------------------------------
// 0x18001cfc0 — the per-element destroy loop for an array of 0x18-byte records
// whose first two qwords are refcounted System::String data pointers. Retail
// releases +0x08 FIRST and +0x00 second; order kept.
//
// The first parameter is the array object. The callee spills it at [RSP+8] and
// NEVER READS IT (0x18001cfc9 is the only mention), which is why it is unnamed
// here — the same reading `reference_read_the_callee_before_holding_back_on_an_arg.md`
// records for FUN_180023060.
// ---------------------------------------------------------------------------
// *** NO LONGER FILE-LOCAL. *** Phase 1 said "nothing outside this file reaches
// them", which was true only while this file was the tree's only
// `Video::ConstantBuffer` code. Phase 3 found both under the class itself:
// `?BeginConstants@` (0x1800eed10) TAIL-JUMPS to 0x18001cab0, and the
// `Array<ConstantEntry>` resize at 0x180053510 calls 0x18001cfc0. So both now
// have real callers and both are registered at their addresses — by
// constant_buffer.cpp, which owns the registration because that is where the
// callers live.
void StringPairDestroy(void* /*unused*/, void* data, int32_t count) {
    if (count <= 0) return;
    uint8_t* e = static_cast<uint8_t*>(data);
    for (int64_t i = count; i != 0; --i) {
        ReleaseStringNoTidy(reinterpret_cast<void**>(e + 8));
        ReleaseStringNoTidy(reinterpret_cast<void**>(e + 0));
        e += 0x18;
    }
}

// ---------------------------------------------------------------------------
// 0x18001cab0 — Collection::Array<that record>::~Array.
//   destroy the elements, free the block, then zero {_data} and {_size,_capacity}
//   as two qword stores (0x18001cb18 / 0x18001cb1b).
// Retail's `XOR EAX,EAX` at 0x18001cb16 supplies BOTH stores and the return
// value, so the function returns 0, not the array. No caller reads it, so this
// is written `void`.
// ---------------------------------------------------------------------------
void StringPairArrayDtor(void* arr) {
    uint8_t* a = static_cast<uint8_t*>(arr);
    void*   data  = *reinterpret_cast<void**>(a);
    int32_t count = *reinterpret_cast<int32_t*>(a + 8);
    StringPairDestroy(arr, data, count);
    if (data) System::MemoryAllocator::MemFree(data);
    *reinterpret_cast<void**>(a)     = nullptr;   // _data
    *reinterpret_cast<uint64_t*>(a + 8) = 0;      // _size and _capacity together
}

}  // namespace

// The two above, exported for constant_buffer.cpp to call and to register. Thin
// forwarders rather than moved bodies, so this file keeps the transcription and
// the commentary that goes with it, and there is exactly ONE implementation of
// each (a second copy is how two ports of one address drift apart).
extern "C" void RtVideo_ConstEntryArrayDestroy(void* unused, void* data, int32_t count) {
    StringPairDestroy(unused, data, count);
}
extern "C" void RtVideo_ConstEntryArrayDtor(void* arr) {
    StringPairArrayDtor(arr);
}

// ============================================================================
// ??1ConstantBuffer@Video@@QEAA@XZ — retail 0x1800eeee0, ordinal 558
// ============================================================================
// The seven `Video::ConstantBuffer` members embedded in a Context are destroyed
// through this. Layout touched (runtime/types/video/ConstantBuffer.h):
//   _prefix (+0x00), _typeName (+0x08)   refcounted System::String data pointers
//   _constantData.. (+0x28)              Array<ConstantInfo>, stride 0x18
//   _platformData (+0x40)                the ConstantBufferData companion
//                                        (deleting dtor, slot 0)
// ORDER: companion, then the array, then _typeName, then _prefix. Kept.
//
// STILL A FREE FUNCTION, NOT `~ConstantBuffer()` (ENDGAME B.3.5 block D1). A real
// destructor here would make `Video::ConstantBuffer` non-trivially-copyable and
// `Video::Context` — which holds seven by value — with it; RULE B5-4 and the
// `static_assert`s in both headers forbid exactly that. The offsets are now the
// real members, which is what the conversion bought.
// ============================================================================
extern "C" void Video_ConstantBuffer_Dtor(void* selfRaw) {
    Video::ConstantBuffer* self = static_cast<Video::ConstantBuffer*>(selfRaw);

    // ENDGAME B.3.5 block VC: a virtual slot-0 call on block A's real
    // `Video::ConstantBufferData`. `_platformData` is a pinned `void*` at +0x40
    // (dx11 reads it), so the receiver takes one marked bridge cast.
    if (void* companion = self->_platformData)
        reinterpret_cast<Video::ConstantBufferData*>(companion)       // B.3.5 bridge
            ->DeletingDtor(1);

    StringPairArrayDtor(&self->_constantData);

    ReleaseString(&self->_typeName);
    ReleaseString(&self->_prefix);
}

// ============================================================================
// ??3RenderBuffer@Video@@SAXPEAX@Z — retail 0x18001f360, ordinal 995
// ============================================================================
// A class `operator delete`: `if (p) { Enter(alloc CS); Free(_singleton, p);
// Leave; }`. Byte-for-byte the same body as 0x180018500 (Video::Context's own
// operator delete) and 0x180006a10. Our 0x1800c7ad0 MemFree is that composition,
// with the null test inside it, which is why the pointer is handed over
// unguarded exactly as retail's `TEST RCX,RCX; JZ ret` allows.
//
// ENDGAME B.3.5 block VC: the body is `Video::RenderBuffer::operator delete`, a
// STATIC member (which changes no triviality trait, so `Device.h`'s
// `is_trivially_copyable<RenderBuffer>` assert still holds and still proves it).
// The door keeps its name and both rows.
// ============================================================================
void Video::RenderBuffer::operator delete(void* p) {
    System::MemoryAllocator::MemFree(p);
}

extern "C" void Video_RenderBuffer_OperatorDelete(void* p) {
    Video::RenderBuffer::operator delete(p);
}

// `??3Context@Video@@SAXPEAX@Z` (0x180018500) stays a FREE FUNCTION and not
// `Video::Context::operator delete`: `Context.h` declares none of the eight
// lifecycle spellings, on purpose, and that header — not this file — is where
// the decision lives. It is a distinct function in the image from the one above.
extern "C" void Video_Context_OperatorDelete(void* p) { System::MemoryAllocator::MemFree(p); }

// ============================================================================
// 0x180006a10 — ATTRIBUTION CORRECTED, ENDGAME B.3.5 block VC.
// ============================================================================
// This was called `Video_Buffer_OperatorDelete` and described as "the buffers'"
// third `operator delete` instantiation. IT IS NOT A `Video::` MEMBER AT ALL. It
// is UNEXPORTED — there is no `??3{Index,Vertex,Image}Buffer@Video@@` in the
// shipped export directory that resolves here — and it is the tree's shared
// guarded object free, reached by `ResolveAddr(Module::Runtime, 0x180006a10)`
// from at least eight files outside Video: `io/file_stream.cpp`,
// `io/compressed_stream_release.cpp`, `io/binary_writer.cpp`,
// `io/iobase_vtables.cpp`, `input/keymap_save.cpp`, `object/deleting_dtor.cpp`
// (its scalar/array MemFree target), `engine/closure_batch2b.cpp` and
// `engine/engine_platform.cpp`. b35-D1 measured about a hundred callers across
// IO / Save / Graphics.
//
// ONLY THE NAME AND THIS COMMENT CHANGE. The registration below is untouched:
// this remains the one body at that address, `~Context` still frees its three
// buffer sub-objects through it (which is retail — retail's ~Context calls
// 0x180006a10 and NOT 0x180018500 for them), and every other caller keeps
// landing here through the registry-first address door.
extern "C" void Rt_GuardedObjectFree(void* p) { System::MemoryAllocator::MemFree(p); }

// ============================================================================
// ?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ — retail 0x1800f4400, ord 3369
// ============================================================================
// 1. release the RenderBufferData companion at +0x28 (deleting dtor, slot 0);
// 2. take Video::Device's recursive render lock;
// 3. if BOTH links are non-null, unlink `this` from the circular list whose
//    head is `?_firstRenderBuffer@RenderBuffer@Video@@` and fix the head TWICE
//    — once before the unlink (head = this->next when head == this) and once
//    after (head = nullptr when head == this). The second fixup is what makes
//    the LAST buffer leave an empty list rather than a self-referencing one;
// 4. release the lock.
//
// The `if (prev && next)` guard is retail's and it is load-bearing: a buffer
// that was never linked (PlatformConstruct not run) is skipped rather than
// unlinked through null. Retail does NOT guard the head slot pointer itself.
// ============================================================================
// THE BODY THAT WAS HERE IS GONE -- it lives in buffers.cpp (Wave 6 lane C /
// Wave 9 lane GFX2), declared at the top of this file with its two siblings.
// The analysis above stands and was cross-checked against lane C's version
// instruction by instruction before the deletion: `ListUnlink` with
// kRenderLinks = {next 0x18, prev 0x20} performs the same head fixup twice
// around the same unlink, and `PlatformDestructBody(..., /*null=*/false, ...)`
// leaves +0x28 dangling exactly as retail does. Lane C's is kept rather than
// this one because it is the version the RT_DIFF shadow oracle and the
// list-emptied probe actually instrument, and because it is one arm of the
// six-way BufferClass composition -- keeping a second spelling here is the
// "two places to drift" this file warns about for `operator delete`.

// ============================================================================
// ?ClearResources@Context@Video@@QEAAXXZ — retail 0x1800f2090, ordinal 1712
// ============================================================================
// Two sixteen-iteration sweeps that drop every bound texture and every bound
// vertex stream, re-dirtying only the slots that actually held something.
//
//   0x1800f209f  EAX = 1                      ; the rotating image-slot bit
//   0x1800f20a7  for slot i in 0..15:
//                  if (_imageBuffers[i]) {
//                      _dirty           |= 0x2   (kDirtyImageBuffers)
//                      _imageBufferDirty|= bit
//                      _imageBuffers[i]  = nullptr
//                  }
//                  ROL EAX,1
//   0x1800f20d0  for slot i in 0..15:
//                  if (_vertexBuffers[i]) {
//                      _dirty            |= 0x8  (kDirtyVertexBuffers)
//                      _vertexBufferDirty|= bit
//                      _vertexBuffers[i]  = nullptr
//                      _vertexOffsets[i]  = 0     ; [R8-0x40], R8 walking +0x178
//                      _vertexStrides[i]  = 0     ; [R8]
//                  }
//                  ROL EDX,1
//
// The two rotating masks are SEPARATE REGISTERS (EAX and EDX) seeded from the
// same 1 at 0x1800f2090/0x1800f209f, and EDX survives the first loop untouched.
// Writing one shared mask would be wrong the moment either loop's length
// changed; it is written as two.
//
// This function touches NOTHING ELSE. It does not clear the render buffer, the
// index buffer, the vertex layout or the programs — which is exactly why the
// oracle below can compare the full set of affected fields with no risk of
// reading bytes neither side wrote
// (reference_oracle_must_not_compare_unwritten_bytes.md).
// ============================================================================
void Video::Context::ClearResources() {

    uint32_t imageBit = 1;
    for (int i = 0; i < RtVideo::kImageBufferSlots; ++i) {
        if (_imageBuffers[i] != nullptr) {
            _dirty            |= RtVideo::kDirtyImageBuffers;
            _imageBufferDirty |= imageBit;
            _imageBuffers[i]   = nullptr;
        }
        imageBit = (imageBit << 1) | (imageBit >> 31);   // ROL EAX,1
    }

    uint32_t vertexBit = 1;
    for (int i = 0; i < RtVideo::kVertexBufferSlots; ++i) {
        if (_vertexBuffers[i] != nullptr) {
            _dirty             |= RtVideo::kDirtyVertexBuffers;
            _vertexBufferDirty |= vertexBit;
            _vertexBuffers[i]   = nullptr;
            _vertexOffsets[i]   = 0;
            _vertexStrides[i]   = 0;
        }
        vertexBit = (vertexBit << 1) | (vertexBit >> 31);   // ROL EDX,1
    }

}

// The door, unchanged name and unchanged registrations.
extern "C" void RtVideo_Context_ClearResources(Context* self) {
    self->Video::Context::ClearResources();
}

// ============================================================================
// ??1Context@Video@@QEAA@XZ — retail 0x1800f16e0, ordinal 562
// ============================================================================
// Seven sub-objects and seven embedded constant buffers, in retail's order.
// The order is not arbitrary and is not normalised: the backend companion at
// +0x2160 goes FIRST, so that every buffer released afterwards is released
// against a backend that has already torn its own state down.
//
//   +0x2160 _contextData            deleting dtor (slot 0)
//   +0x20f8 _dynamicVertexBuffer    VertexBuffer::PlatformDestruct, then delete
//   +0x2118 _dynamicIndexBuffer     IndexBuffer::PlatformDestruct,  then delete
//   +0x2140 _defaultColorImage      deleting dtor (slot 0)
//   +0x2148 _defaultDepthImage      deleting dtor (slot 0)
//   +0x2138 _defaultRenderBuffer    RenderBuffer::PlatformDestruct, then ??3RenderBuffer
//   +0x2130 _dynamicInstanceBuffer  VertexBuffer::PlatformDestruct, then delete
//   +0x2098, +0x2050, +0x2008, +0x1fc0, +0x1f78, +0x2e0, +0x298
//                                   ~ConstantBuffer, in THAT order (reverse
//                                   declaration order, as MSVC emits)
//
// NOTE WHAT IS ABSENT. The dtor does NOT null any of the seven pointers after
// releasing them, and it does not touch the bindings ClearResources clears. A
// Context is dead after this and retail treats it as such — `CreateVideoContext`
// calls the dtor and then immediately hands the block to `operator delete`.
//
// The three buffer sub-objects are freed through 0x180006a10, NOT through the
// same `operator delete` the Context itself uses (0x180018500). The bodies are
// identical, but the call is by address to a different function and is
// registered as such.
// ============================================================================
// ENDGAME B.3.5 block VC: the seven sub-object releases are direct calls. The
// three `Platform*` free functions became qualified member calls, `operator
// delete` is `Video::RenderBuffer`'s member, and the three `DeletingDtor(p)`
// hand-inlined slot-0 dispatches are VIRTUAL CALLS on block A's
// `Video::ContextData` and on `Video::ImageBuffer` — which is what retail's
// `MOV RAX,[RCX] / MOV EDX,1 / CALL [RAX]` is, slot 0 with the deleting flag. The
// free-function `DeletingDtor` helper is kept for nothing else and is gone.
extern "C" void RtVideo_Context_Dtor(Context* self) {
    // B.3.5 bridge — `_contextData` is a pinned `void*` at +0x2160.
    if (self->_contextData)
        reinterpret_cast<Video::ContextData*>(self->_contextData)->DeletingDtor(1);

    if (void* p = self->_dynamicVertexBuffer) {
        reinterpret_cast<Video::VertexBuffer*>(p)                      // B.3.5 bridge
            ->Video::VertexBuffer::PlatformDestruct();
        Rt_GuardedObjectFree(p);
    }
    if (void* p = self->_dynamicIndexBuffer) {
        reinterpret_cast<Video::IndexBuffer*>(p)                       // B.3.5 bridge
            ->Video::IndexBuffer::PlatformDestruct();
        Rt_GuardedObjectFree(p);
    }
    if (void* p = self->_defaultColorImage)
        delete reinterpret_cast<Video::ImageBuffer*>(p);     // B.3.5 bridge
    if (void* p = self->_defaultDepthImage)
        delete reinterpret_cast<Video::ImageBuffer*>(p);     // B.3.5 bridge
    if (void* p = self->_defaultRenderBuffer) {
        reinterpret_cast<Video::RenderBuffer*>(p)                      // B.3.5 bridge
            ->Video::RenderBuffer::PlatformDestruct();
        Video::RenderBuffer::operator delete(p);
    }
    if (void* p = self->_dynamicInstanceBuffer) {
        reinterpret_cast<Video::VertexBuffer*>(p)                      // B.3.5 bridge
            ->Video::VertexBuffer::PlatformDestruct();
        Rt_GuardedObjectFree(p);
    }

    Video_ConstantBuffer_Dtor(&self->_lightConstants);
    Video_ConstantBuffer_Dtor(&self->_transformConstants);
    Video_ConstantBuffer_Dtor(&self->_boneConstants);
    Video_ConstantBuffer_Dtor(&self->_instanceConstants);
    Video_ConstantBuffer_Dtor(&self->_globalConstants);
    Video_ConstantBuffer_Dtor(&self->_pixelConstants);
    Video_ConstantBuffer_Dtor(&self->_materialConstants);   // retail tail-jumps
}

// ============================================================================
// THE PROBE — the caller ClearResources does not have
// ============================================================================
// `?ClearResources@Context@Video@@QEAAXXZ` has NO call site anywhere in code/,
// engine/, runtime/, standalone/ or dx11/ (grep for fn_VideoCtx_ClearResources
// returns the extern, the header, the assignment and one diagnostic printf —
// lane VID established this and it is re-checked here). A registered port on a
// path nothing executes reports `calls=0 divergences=0` forever, which is not a
// verification (reference_uncalled_port_is_not_verified.md).
//
// So the probe supplies the callers. It is NOT a re-derivation of the port: it
// drives the SAME `RtVideo_Context_ClearResources` the registry hands out, and
// the reference side is the SHIPPED body reached through `the raw DLL door`
// (reference_resolveaddr_consults_port_registry_first.md — ResolveExport would
// hand back our own port once registered, and the comparison would be vacuous).
//
// METHOD, per case:
//   1. build a scratch Context-sized buffer with a pattern
//   2. copy it; run THEIRS on the copy
//   3. run OURS on the original
//   4. compare the 3 dirty words + 16 image slots + 16 vertex slots +
//      16 offsets + 16 strides. Nothing else is compared, because nothing else
//      is written by either side.
//
// TWO CONTROLS, because a sweep of agreeing cases proves nothing on its own
// (reference_probe_needs_an_effectiveness_control.md):
//   * EFFECTIVENESS: `cleared` counts the cases in which at least one binding
//     was actually dropped. A run reporting cleared=0 has verified nothing and
//     says so.
//   * NEGATIVE: one case is run with a DELIBERATELY WRONG mask seed (bit 2
//     instead of bit 1) fed to a local copy of the loop, and the comparison
//     must FAIL. If it passes, the comparison is not looking at the mask and
//     the whole sweep is worthless.
// ============================================================================
namespace {

struct ClearSnapshot {
    uint32_t dirty;
    uint32_t imageDirty;
    uint32_t vertexDirty;
    void*    images[RtVideo::kImageBufferSlots];
    void*    vertices[RtVideo::kVertexBufferSlots];
    int32_t  offsets[RtVideo::kVertexBufferSlots];
    int32_t  strides[RtVideo::kVertexBufferSlots];
};

// *** THE memset IS NOT TIDINESS. *** `ClearSnapshot` has four bytes of
// padding after the three uint32s (the `void*` array that follows needs 8-byte
// alignment), and `SameSnapshot` memcmps the whole struct. Without this the
// comparison reads bytes NOTHING EVER WRITES, which is exactly
// reference_oracle_must_not_compare_unwritten_bytes.md -- and it bit this probe
// on its first run: `cleared` came back 512 out of 512 cases when seed 0 builds
// an all-null Context that ClearResources provably cannot change, so the
// EFFECTIVENESS CONTROL was reporting stack garbage rather than work. The
// mismatch column was unaffected only by luck (the two long-lived snapshots
// happened to share their padding), which is the whole reason a control that
// can be satisfied by noise is worse than no control.
void Capture(const Context* c, ClearSnapshot* s) {
    std::memset(s, 0, sizeof(*s));
    s->dirty       = c->_dirty;
    s->imageDirty  = c->_imageBufferDirty;
    s->vertexDirty = c->_vertexBufferDirty;
    std::memcpy(s->images,   c->_imageBuffers,  sizeof(s->images));
    std::memcpy(s->vertices, c->_vertexBuffers, sizeof(s->vertices));
    std::memcpy(s->offsets,  c->_vertexOffsets, sizeof(s->offsets));
    std::memcpy(s->strides,  c->_vertexStrides, sizeof(s->strides));
}

bool SameSnapshot(const ClearSnapshot& a, const ClearSnapshot& b) {
    return std::memcmp(&a, &b, sizeof(ClearSnapshot)) == 0;
}

// The pattern generator. `seed` selects which slots hold a (fake, never
// dereferenced — ClearResources only ever compares against zero and stores
// zero) binding, and seeds the pre-existing dirty state so the |= is exercised
// against a non-zero starting value rather than always against 0.
void BuildCase(Context* c, unsigned seed) {
    // `static_cast<void*>`: `Video::Context` is no longer trivially
    // default-constructible (its `Math::Matrix` members have zeroing retail
    // constructors), so `-Wclass-memaccess` objects to a bare memset. The probe
    // wants bytes, which is what the cast says. Trivially COPYABLE — the
    // property this and every other memcpy over a Context depends on — is
    // asserted in Context.h and is unaffected.
    std::memset(static_cast<void*>(c), 0, sizeof(Context));
    c->_dirty             = (seed & 1u) ? 0x00000040u : 0u;   // kDirtyVertexProgram
    c->_imageBufferDirty  = (seed & 2u) ? 0x00000080u : 0u;
    c->_vertexBufferDirty = (seed & 4u) ? 0x00004000u : 0u;
    for (int i = 0; i < RtVideo::kImageBufferSlots; ++i) {
        c->_imageBuffers[i] = ((seed >> i) & 1u)
            ? reinterpret_cast<void*>(static_cast<uintptr_t>(0x1000u + i)) : nullptr;
    }
    for (int i = 0; i < RtVideo::kVertexBufferSlots; ++i) {
        const bool bound = ((seed >> (i + 3)) & 1u) != 0;
        c->_vertexBuffers[i] = bound
            ? reinterpret_cast<void*>(static_cast<uintptr_t>(0x2000u + i)) : nullptr;
        c->_vertexOffsets[i] = bound ? (100 + i) : (300 + i);
        c->_vertexStrides[i] = bound ? (200 + i) : (400 + i);
    }
}

}  // namespace

// ============================================================================
// Registration
//
// Every mangled name below was checked against the shipped export directory of
// C:\GOG Games\Banished\Runtime-x64.dll with pefile BEFORE any body was written
// (reference_resolveexport_phantom_names.md):
//
//   ?ClearResources@Context@Video@@QEAAXXZ            0x1800f2090  ord 1712
//   ??1Context@Video@@QEAA@XZ                         0x1800f16e0  ord  562
//   ??1ConstantBuffer@Video@@QEAA@XZ                  0x1800eeee0  ord  558
//   ??3RenderBuffer@Video@@SAXPEAX@Z                  0x18001f360  ord  995
//
// ?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ (0x1800f4400, ord 3369) was in
// this list and is NOT any more: buffers.cpp registers both of its doors. See
// the note beside the declaration at the top of this file. NOTE THE ONE
// BEHAVIOURAL CONSEQUENCE: that registration now rides RT_VIDBUFFERS
// instead of RT_VIDCTXLIFE, so VIDBUFFERS=0 now really does leave the
// export on the DLL. It did not before -- this file re-registered it and the
// kill switch silently failed to kill.
//
// Note `??1` for the destructors and `QEAA` (non-virtual) for Context's and
// ConstantBuffer's — `??1Context@Video@@UEAA@XZ` does not exist and would
// resolve NULL in silence behind the guard
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md).
//
// ADDRESS DOORS. Five, and each has a named reader in the tree today:
//   0x1800f16e0  ??1Context@Video@@      engine_funcs.cpp:1623 falls back to it
//   0x180018500  Context operator delete engine_funcs.cpp:1625 (name-less door)
//   0x180006a10  buffer operator delete  reached by the dtor above
//   0x18001cab0 / 0x18001cfc0            reached by ~ConstantBuffer above
// The last three are UNNAMED in the image, so a name registration alone would
// register something nobody calls.
// ============================================================================
int RtVideoContextLifecycleRegister() {
    int n = 0;

    RegisterRuntimeExport("??1Context@Video@@QEAA@XZ",
                          (void*)&RtVideo_Context_Dtor); ++n;
    // ?PlatformDestruct@RenderBuffer@Video@@ is registered by buffers.cpp, which
    // owns the body. Registering it here too made this file's (now deleted)
    // duplicate body win both doors.
    RegisterRuntimeExport("??3RenderBuffer@Video@@SAXPEAX@Z",
                          (void*)&Video_RenderBuffer_OperatorDelete); ++n;

    RegisterRuntimeAddr(0x1800f16e0, (void*)&RtVideo_Context_Dtor);
    // 0x1800f4400: likewise buffers.cpp's, see above.
    RegisterRuntimeAddr(0x18001f360, (void*)&Video_RenderBuffer_OperatorDelete);
    // Bound by NAME as well as by address since Wave 9 phase 4. It was
    // address-only for three phases and nothing noticed, because nothing in the
    // tree asked for `??3Context@Video@@SAXPEAX@Z` by name until
    // CreateVideoContext was ported; the name door then fell straight through
    // to the DLL while the address door was ours. An exported symbol with a
    // port needs BOTH doors, not whichever one its first caller happened to use.
    RegisterRuntimeExport("??3Context@Video@@SAXPEAX@Z",
                          (void*)&Video_Context_OperatorDelete); ++n;
    RegisterRuntimeAddr(0x180018500, (void*)&Video_Context_OperatorDelete);
    // 0x180006a10 — the SHARED unexported guarded free, not a `Video::` member.
    // See the banner on `Rt_GuardedObjectFree`; the row is unchanged.
    RegisterRuntimeAddr(0x180006a10, (void*)&Rt_GuardedObjectFree);

    return n;
}

// ============================================================================
// WHAT THIS FILE DOES NOT DO — stated so the gap is measured, not implied
// ============================================================================
// `??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z` (0x1800f1820)
// is NOT ported. It is ~400 instructions that zero ~90 fields, construct seven
// ConstantBuffer sub-objects inline (each one taking a companion from the
// DeviceData vtable slot +0x58), run four memsets over the binding arrays, then
// conditionally build a dynamic vertex buffer, a dynamic index buffer, a
// dynamic instance buffer and — when the caller passes no render buffer — a
// RenderBuffer plus two ImageBuffers through four separate class `operator new`
// bodies (0x180011640, 0x18001f2e0, 0x18001e910) and
// `?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ` (0x1800f4320), none of which
// is ported.
//
// So `?CreateVideoContext@MainLoop@Engine@@AEAAXXZ` (0x1800dd500, ordinal 2139
// — it IS exported, which lane VID recorded as "unported AND UNEXPORTED"; that
// half is wrong and is corrected here) still cannot be closed by this lane: two
// of its three calls now have ports, the ctor does not.
// ============================================================================
