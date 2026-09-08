// ============================================================================
// runtime/src/video/context_lifecycle.cpp — Wave 9 lane VID2
//   Video::Context::ClearResources, Video::Context::~Context and the five
//   destruction helpers the dtor reaches that nothing else in the tree owned.
// ============================================================================
//
// WHY THIS FILE EXISTS
// --------------------
// `??1Context@Video@@QEAA@XZ` and `?ClearResources@Context@Video@@QEAAXXZ` are
// two of the 43 names `BANISHED_RT_STRICT` recorded unresolved on cutover boot
// attempt 14 (docs/runtime_port/wave9_attempt14_unresolved.txt). Lane VID
// classified both as RESOLVE-ONLY and left them, which was correct as far as it
// went: `engine/funcs/engine_funcs.cpp` resolves both and nothing calls either.
//
// What that classification missed is the SHAPE OF THE NEIGHBOURHOOD, which the
// integrator's address-door census (docs/runtime_port/wave9_unregistered_addr_doors.csv)
// makes plain. The `Video::Context` lifecycle reaches the DLL through TWO
// DIFFERENT DOORS:
//
//     engine_funcs.cpp:1621  fn_VideoCtx_Ctor    ResolveAddr(Runtime, 0x1800f1820)
//     engine_funcs.cpp:1623  fn_VideoCtx_Dtor    ResolveExport("??1Context@Video@@QEAA@XZ")
//                                                (falling back to 0x1800f16e0)
//     engine_funcs.cpp:1625  fn_VideoCtx_Delete  ResolveAddr(Runtime, 0x180018500)
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
#include "../../funcs/rt_diff.h"
#include "../../types/video/Buffers.h"
#include "../../types/video/Context.h"
#include "../../types/video/Device.h"

using RtVideo::Context;
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

// Ported siblings in buffers.cpp (Wave 6 lane C / Wave 9 lane W). Retail's dtor
// reaches these by direct near CALL; ours reaches the C++ symbols, so no
// resolver sits on the path at all.
void Video_IndexBuffer_PlatformDestruct(IndexBuffer* self);
void Video_VertexBuffer_PlatformDestruct(VertexBuffer* self);

// Declared locally rather than by including standalone/resolve.h, which drags
// in <windows.h> — PLAN §4.1 decision 1 keeps runtime/src/ platform-neutral
// outside a `_win` twin. Byte-identical to the declarations in
// runtime/src/collection/array.cpp:133; the linker checks the two agree.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace {

// ---------------------------------------------------------------------------
// The allocator boundary, by address, with the RtResolveCached contract:
// CACHE A HIT, RETRY A MISS. Freezing a null here would take the heap down for
// the life of the process if anything ran before RegisterAllocatorAddresses()
// armed the address door — the ordering defect attempt 14 fixed. Same shape as
// runtime/src/collection/array.cpp:155.
// ---------------------------------------------------------------------------
void MemFree(void* p) {
    using Fn = void (*)(void*);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7ad0));
    if (fn) fn(p);
}

Device** DeviceSlot() {
    return RT_DATA_SLOT(Device**, "?_singleton@Device@Video@@0PEAV12@EA");
}
RenderBuffer** FirstRenderBufferSlot() {
    return RT_DATA_SLOT(RenderBuffer**,
        "?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA");
}

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
inline void DeletingDtor(void* obj) {
    using Fn = void* (*)(void*, int);
    Fn slot0 = *reinterpret_cast<Fn*>(*reinterpret_cast<void**>(obj));
    slot0(obj, 1);
}

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
    MemFree(*field);
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
    if (*field) MemFree(*field);
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
    if (data) MemFree(data);
    *reinterpret_cast<void**>(a)     = nullptr;   // _data
    *reinterpret_cast<uint64_t*>(a + 8) = 0;      // _size and _capacity together
}

}  // namespace

// ============================================================================
// ??1ConstantBuffer@Video@@QEAA@XZ — retail 0x1800eeee0, ordinal 558
// ============================================================================
// The seven ConstantBufferOpaque members embedded in a Context are destroyed
// through this. Layout touched (0x48 bytes total, runtime/types/video/Context.h):
//   +0x00, +0x08  refcounted System::String data pointers
//   +0x28         Collection::Array<0x18-byte string-pair record>
//   +0x40         the ConstantBufferData companion (deleting dtor, slot 0)
// ORDER: companion, then the array, then +0x08, then +0x00. Kept.
// ============================================================================
extern "C" void Video_ConstantBuffer_Dtor(void* self) {
    uint8_t* b = static_cast<uint8_t*>(self);

    void* companion = *reinterpret_cast<void**>(b + 0x40);
    if (companion) DeletingDtor(companion);

    StringPairArrayDtor(b + 0x28);

    ReleaseString(reinterpret_cast<void**>(b + 0x08));
    ReleaseString(reinterpret_cast<void**>(b + 0x00));
}

// ============================================================================
// ??3RenderBuffer@Video@@SAXPEAX@Z — retail 0x18001f360, ordinal 995
// ============================================================================
// A class `operator delete`: `if (p) { Enter(alloc CS); Free(_singleton, p);
// Leave; }`. Byte-for-byte the same body as 0x180018500 (Video::Context's own
// operator delete) and 0x180006a10 (the buffers'). Our 0x1800c7ad0 MemFree is
// that composition, with the null test inside it, which is why the pointer is
// handed over unguarded exactly as retail's `TEST RCX,RCX; JZ ret` allows.
// ============================================================================
extern "C" void Video_RenderBuffer_OperatorDelete(void* p) {
    MemFree(p);
}

// The same body at its two other addresses. They are DISTINCT FUNCTIONS in the
// image (three separate `operator delete` instantiations that the compiler did
// not fold), so each needs its own address registration; sharing one C++ body
// is safe only because the three are provably identical instruction sequences.
extern "C" void Video_Context_OperatorDelete(void* p) { MemFree(p); }
extern "C" void Video_Buffer_OperatorDelete(void* p)  { MemFree(p); }

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
extern "C" void Video_RenderBuffer_PlatformDestruct(RenderBuffer* self) {
    void* companion = self->platformData;
    if (companion) DeletingDtor(companion);

    Device** slot = DeviceSlot();
    RtVideoDeviceLock(reinterpret_cast<void**>(slot));

    if (self->prev != nullptr && self->next != nullptr) {
        RenderBuffer** head = FirstRenderBufferSlot();

        // 0x1800f445d: head == this ? this->next : head
        if (*head == self) *head = static_cast<RenderBuffer*>(self->next);

        // 0x1800f4472: prev->next = next ; next->prev = prev
        static_cast<RenderBuffer*>(self->prev)->next = self->next;
        static_cast<RenderBuffer*>(self->next)->prev = self->prev;
        self->prev = nullptr;
        self->next = nullptr;

        // 0x1800f4494: head == this ? nullptr : head
        if (*head == self) *head = nullptr;
    }

    RtVideoDeviceUnlock(reinterpret_cast<void**>(slot));
}

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
extern "C" void RtVideo_Context_ClearResources(Context* self) {
    RT_DIFF_BEGIN("Video::Context::ClearResources");

    uint32_t imageBit = 1;
    for (int i = 0; i < RtVideo::kImageBufferSlots; ++i) {
        if (self->_imageBuffers[i] != nullptr) {
            self->_dirty            |= RtVideo::kDirtyImageBuffers;
            self->_imageBufferDirty |= imageBit;
            self->_imageBuffers[i]   = nullptr;
        }
        imageBit = (imageBit << 1) | (imageBit >> 31);   // ROL EAX,1
    }

    uint32_t vertexBit = 1;
    for (int i = 0; i < RtVideo::kVertexBufferSlots; ++i) {
        if (self->_vertexBuffers[i] != nullptr) {
            self->_dirty             |= RtVideo::kDirtyVertexBuffers;
            self->_vertexBufferDirty |= vertexBit;
            self->_vertexBuffers[i]   = nullptr;
            self->_vertexOffsets[i]   = 0;
            self->_vertexStrides[i]   = 0;
        }
        vertexBit = (vertexBit << 1) | (vertexBit >> 31);   // ROL EDX,1
    }

    RT_DIFF_WORK(true);
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
extern "C" void RtVideo_Context_Dtor(Context* self) {
    if (self->_contextData) DeletingDtor(self->_contextData);

    if (void* p = self->_dynamicVertexBuffer) {
        Video_VertexBuffer_PlatformDestruct(static_cast<VertexBuffer*>(p));
        Video_Buffer_OperatorDelete(p);
    }
    if (void* p = self->_dynamicIndexBuffer) {
        Video_IndexBuffer_PlatformDestruct(static_cast<IndexBuffer*>(p));
        Video_Buffer_OperatorDelete(p);
    }
    if (void* p = self->_defaultColorImage) DeletingDtor(p);
    if (void* p = self->_defaultDepthImage) DeletingDtor(p);
    if (void* p = self->_defaultRenderBuffer) {
        Video_RenderBuffer_PlatformDestruct(static_cast<RenderBuffer*>(p));
        Video_RenderBuffer_OperatorDelete(p);
    }
    if (void* p = self->_dynamicInstanceBuffer) {
        Video_VertexBuffer_PlatformDestruct(static_cast<VertexBuffer*>(p));
        Video_Buffer_OperatorDelete(p);
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
// the reference side is the SHIPPED body reached through `ResolveExportRaw`
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
    std::memset(c, 0, sizeof(Context));
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

extern "C" void RtVideoContextLifecycleProbe() {
    using Fn = void (*)(Context*);
    Fn theirs = reinterpret_cast<Fn>(
        ResolveExportRaw("?ClearResources@Context@Video@@QEAAXXZ"));
    if (!theirs) {
        std::printf("[CtxClear] SKIP -- the shipped ?ClearResources@Context@Video@@ is not "
                    "reachable (cutover, or the DLL is absent). No comparison was made.\n");
        return;
    }

    // Heap-free: two Context-sized statics. sizeof(Context) is 0x2168, so these
    // are ~17 KB of .bss, which is cheaper and far more predictable at this
    // point in boot than touching the allocator.
    static Context ours;
    static Context refs;

    int cases = 0, mismatches = 0, cleared = 0, theirsCleared = 0;
    ClearSnapshot so, st;

    for (unsigned seed = 0; seed < 512u; ++seed) {
        BuildCase(&ours, seed);
        std::memcpy(&refs, &ours, sizeof(Context));

        ClearSnapshot before;
        Capture(&ours, &before);

        theirs(&refs);
        RtVideo_Context_ClearResources(&ours);

        Capture(&ours, &so);
        Capture(&refs, &st);
        ++cases;
        // EFFECTIVENESS control, measured on BOTH sides. `theirsCleared` is the
        // authoritative count because it is produced by the shipped body, which
        // this lane did not write; `cleared` must equal it, and if it does not
        // the two implementations disagree about WHETHER to act even in cases
        // where they agree about the resulting state.
        if (!SameSnapshot(before, so)) ++cleared;
        if (!SameSnapshot(before, st)) ++theirsCleared;
        if (!SameSnapshot(so, st)) {
            if (mismatches < 4) {
                std::printf("[CtxClear] MISMATCH seed=%u  ours dirty=%08x/%08x/%08x  "
                            "theirs=%08x/%08x/%08x\n", seed,
                            so.dirty, so.imageDirty, so.vertexDirty,
                            st.dirty, st.imageDirty, st.vertexDirty);
            }
            ++mismatches;
        }
    }

    // NEGATIVE control: a copy of the port's first loop with the image mask
    // seeded at 2 instead of 1. It MUST be caught, or the comparison is not
    // reading _imageBufferDirty and every green case above is meaningless.
    bool negativeCaught = false;
    {
        BuildCase(&ours, 0xFFFFu);
        std::memcpy(&refs, &ours, sizeof(Context));
        theirs(&refs);
        uint32_t bit = 2;                            // deliberately wrong seed
        for (int i = 0; i < RtVideo::kImageBufferSlots; ++i) {
            if (ours._imageBuffers[i]) {
                ours._dirty            |= RtVideo::kDirtyImageBuffers;
                ours._imageBufferDirty |= bit;
                ours._imageBuffers[i]   = nullptr;
            }
            bit = (bit << 1) | (bit >> 31);
        }
        uint32_t vbit = 1;
        for (int i = 0; i < RtVideo::kVertexBufferSlots; ++i) {
            if (ours._vertexBuffers[i]) {
                ours._dirty             |= RtVideo::kDirtyVertexBuffers;
                ours._vertexBufferDirty |= vbit;
                ours._vertexBuffers[i]   = nullptr;
                ours._vertexOffsets[i]   = 0;
                ours._vertexStrides[i]   = 0;
            }
            vbit = (vbit << 1) | (vbit >> 31);
        }
        Capture(&ours, &so);
        Capture(&refs, &st);
        negativeCaught = !SameSnapshot(so, st);
    }

    std::printf("[CtxClear] cases=%d mismatches=%d cleared=%d theirsCleared=%d "
                "(effectiveness) negative-control=%s\n",
                cases, mismatches, cleared, theirsCleared,
                negativeCaught ? "CAUGHT" : "*** MISSED ***");
    if (cleared == 0)
        std::printf("[CtxClear] *** cleared=0: every case was a no-op, this sweep "
                    "verified NOTHING ***\n");
    if (cleared != theirsCleared)
        std::printf("[CtxClear] *** cleared != theirsCleared: the two bodies disagree "
                    "about WHEN to act, not just about the result ***\n");
    if (cleared == cases)
        std::printf("[CtxClear] *** cleared == cases: seed 0 builds an all-null Context "
                    "that neither body can change, so this figure is an artefact -- "
                    "check Capture's memset ***\n");
    if (!negativeCaught)
        std::printf("[CtxClear] *** the negative control was NOT caught: the comparison "
                    "does not read the dirty masks, so the sweep above is void ***\n");
}

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
//   ?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ     0x1800f4400  ord 3369
//   ??3RenderBuffer@Video@@SAXPEAX@Z                  0x18001f360  ord  995
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

    RegisterRuntimeExport("?ClearResources@Context@Video@@QEAAXXZ",
                          (void*)&RtVideo_Context_ClearResources); ++n;
    RegisterRuntimeExport("??1Context@Video@@QEAA@XZ",
                          (void*)&RtVideo_Context_Dtor); ++n;
    RegisterRuntimeExport("??1ConstantBuffer@Video@@QEAA@XZ",
                          (void*)&Video_ConstantBuffer_Dtor); ++n;
    RegisterRuntimeExport("?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ",
                          (void*)&Video_RenderBuffer_PlatformDestruct); ++n;
    RegisterRuntimeExport("??3RenderBuffer@Video@@SAXPEAX@Z",
                          (void*)&Video_RenderBuffer_OperatorDelete); ++n;

    RegisterRuntimeAddr(0x1800f2090, (void*)&RtVideo_Context_ClearResources);
    RegisterRuntimeAddr(0x1800f16e0, (void*)&RtVideo_Context_Dtor);
    RegisterRuntimeAddr(0x1800eeee0, (void*)&Video_ConstantBuffer_Dtor);
    RegisterRuntimeAddr(0x1800f4400, (void*)&Video_RenderBuffer_PlatformDestruct);
    RegisterRuntimeAddr(0x18001f360, (void*)&Video_RenderBuffer_OperatorDelete);
    RegisterRuntimeAddr(0x180018500, (void*)&Video_Context_OperatorDelete);
    RegisterRuntimeAddr(0x180006a10, (void*)&Video_Buffer_OperatorDelete);

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
