// ============================================================================
// runtime/src/video/context_ctor.cpp — Wave 9 lane VID2 phase 4
//   ??0Context@Video@@, its four zero-fill sub-object constructors, and
//   ?CreateVideoContext@MainLoop@Engine@@ — the last two links of the chain
//   ConstantBuffer -> AssignConstants -> Context ctor -> CreateVideoContext.
// ============================================================================
//
// THE ORACLE WAS CHOSEN BEFORE ANY BODY WAS WRITTEN, AND MY EARLIER REASON FOR
// THINKING IT IMPOSSIBLE WAS WRONG
// ---------------------------------------------------------------------------
// Phase 3's lane doc said the ctor "makes seven CALL [RAX+0x58] virtual calls on
// a real Video::Device, so the synthetic subject that verified AssignConstants
// does not carry over". **That is wrong, and reading the bodies is what showed
// it.** Not one of those calls is on the Device. Every one of them is
//
//     factory = device->+0x100;                       // the backend DeviceData
//     buf->+0x40 = factory ? factory->vtbl[11](factory, buf) : nullptr;
//
// — a companion factory hanging off the Device, and it is NULL-CHECKED. The
// same is true of the one `[RAX+0x48]` (slot 9, the Context's own companion).
// The only UNGUARDED virtual calls in the whole function are the three
// `buf->+0x08->vtbl[1](…)` after each dynamic buffer is constructed and the two
// `img->+0x60->vtbl[1|2]()` in the default-render-buffer branch — and every one
// of those sits behind an ARGUMENT-VALUE guard the caller controls
// (`size > 0`, `renderBuffer == nullptr`).
//
// So the subject is synthetic, in TWO configurations, and neither needs a
// device:
//
//   A. `Context(zeroedDevice, 0, 0, 0, aNonNullRenderBuffer)`. Every guarded
//      virtual takes its null branch and every unguarded one is skipped
//      entirely. This covers the seven inlined ConstantBuffer constructors, the
//      four zero-fills, the four memsets, AssignConstants and the whole field
//      map — with no allocation and no backend at all.
//   B. The LIVE shape, `(device, 0x800000, 0x400000, 0x100000, nullptr)`, which
//      is literally what CreateVideoContext passes, driven against a
//      DEVICE-LEVEL FAKE: one object with a vtable whose slot 11 and slot 9
//      hand back fake companions, installed in the `?_singleton@Device@Video@@`
//      slot for the duration (borrow-and-restore, the trick lane VID2 already
//      used for Video::Configuration) so that the ported PlatformConstructs
//      find it too.
//
// A live `RT_VIDCONSTBUF` A/B was the alternative and was REJECTED: it
// can only report "did it boot", which is precisely the executed-but-undiffed
// state, and it cannot compare a single field of the object under construction.
//
// THE FOUR "ARRAY FILLS" ARE NOT ARRAY FILLS
// ------------------------------------------
// Phase 3 called 0x180053730/840/8b0/920 "four Array-family fills, siblings of
// 0x180053510". Checked by BODY, they are nothing of the kind — no data/size/
// capacity, no allocator, no element type. They are four plain zero-fill
// constructors for four differently-shaped sub-objects:
//
//   0x180053730  0xd0 bytes at +0x328   two 4x4 float matrices + 10 pointers
//   0x180053840  51 x 0x40 at +0x418    -> bones[51]
//   0x1800538b0  52 x 0x40 at +0x10d8   -> transform[52]
//   0x180053920  0x1a0 bytes at +0x1dd8
//
// The middle two differ ONLY in one immediate — `MOV EDX,0x32` against
// `MOV EDX,0x33` — which is exactly the lookalike trap that family has sprung
// before. And the counts are 51 and 52, NOT 50 and 51: the loop is
// `DEC EDX … JNS`, so it runs until EDX goes negative and executes imm+1 times.
// Both numbers are confirmed twice over: the fill sizes tile the Context
// exactly (0x418 + 51*0x40 == 0x10d8, and 0x10d8 + 52*0x40 == 0x1dd8), and they
// match `bones[51]` and `Format("transform[%i]", 52)` in AssignConstants.
//
// THE ALLOCATOR CALLS, ALSO CHECKED BY BODY
// -----------------------------------------
// The ctor's `operator new` is 0x180011640 and CreateVideoContext inlines its
// own allocation. Disassembled, both are
// `Enter(alloc+0x640); Allocate(alloc, n, align); Leave;` — byte-for-byte the
// shape of `?MemAlloc@MemoryAllocator@System@@` (0x1800c7a50), with align 4 for
// `operator new` and align 8 for the Context block. So both are expressed here
// as MemAlloc, which is the registered door this file family already uses, and
// engine_lifecycle.cpp:1111 records the same equivalence and the same reason
// for not registering 0x180011640 itself.
//
// FIELDS THE CTOR NEVER WRITES — an oracle must not compare them
// ---------------------------------------------------------------
// The inlined ConstantBuffer constructor writes +0x18 as a BYTE and +0x1c as an
// UNALIGNED QWORD, so **+0x19, +0x1a and +0x1b are never written**. In a fresh
// heap block they hold whatever was there. The probe compares fields, not
// bytes, and does not look at them
// (reference_oracle_must_not_compare_unwritten_bytes.md).
// ============================================================================

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../../funcs/rt_port.h"
#include "../../types/video/Context.h"

// ============================================================================
// ENDGAME B.3.5 block D1 — WHAT DID AND DID NOT CHANGE HERE
// ============================================================================
// NOTHING IN THIS FILE BECOMES A MEMBER. `??0Context@Video@@` stays a registered
// ctor DOOR (B4b-2 / B35_RULES 11): it runs on 0x2168 raw bytes from
// `System::MemoryAllocator` whose first field it then writes, it is inlined
// seven times over `Video::ConstantBuffer`'s own constructor, and a real
// constructor would additionally destroy `std::is_trivially_copyable<Context>`,
// which nine files depend on. The four sub-object zero-fills stay free functions
// for the same reason. What the conversion bought here is TYPES: `Context*` and
// `Video::ConstantBuffer*` instead of `void*` plus raw offsets, and one
// RT_EXPORT_ONCE retired.
//
// THE FOUR BY-ADDRESS-ONLY ROWS ARE CONFIRMED AS `Video::Context`'s, not
// inferred from their names (B35_FILEMAP flags them as INFERRED). Ghidra
// `getReferencesTo` on each of 0x180053730 / 0x180053840 / 0x1800538b0 /
// 0x180053920 in the retail DLL (15290 functions) returns exactly TWO callers
// each, and both are `Video::Context` constructors:
//
//   0x1800f19cc / 0x1800f19f4 / 0x1800f1a00 / 0x1800f1a0c
//        inside ??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z
//        (0x1800f1820) — the five-argument ctor this file ports
//   0x1800f1422 / 0x1800f144a / 0x1800f1456 / 0x1800f1462
//        inside ??0Context@Video@@QEAA@PEAVDevice@1@@Z (0x1800f1280, body
//        0x1800f1280..0x1800f16d5) — the ONE-ARGUMENT ctor, which is NOT ported
//
// No other referencer exists, so the attribution is proved rather than guessed,
// and the second caller is new information: the four zero-fills are SHARED by
// both Context constructors, which is why they are registered separately
// instead of being folded into the ported ctor.
//
// The two `operator new`s this file ports — `??2ImageBuffer@Video@@` and
// `??2RenderBuffer@Video@@` — are `Video::ImageBuffer`'s and
// `Video::RenderBuffer`'s members (proved by their mangled names, not by a
// prefix), i.e. block C's and block D2's classes. They stay UNTOUCHED doors
// here; moving them onto those classes is owed to those blocks.
// ============================================================================

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

extern "C" void RtVideo_Context_Dtor(Video::Context* self);    // context_lifecycle.cpp
// `??3Context@Video@@SAXPEAX@Z`'s ported body, also context_lifecycle.cpp. This
// used to be an `RT_EXPORT_ONCE(…, "??3Context@Video@@SAXPEAX@Z")` indirect call
// — binding spelling (c) — on a symbol whose only implementation in the process
// is the one three lines away in this block's own file set. B.3.5 makes it a
// direct call; the registration in context_lifecycle.cpp is untouched, so an
// outside caller that still asks by name or by address gets the same body.
extern "C" void Video_Context_OperatorDelete(void* p);         // context_lifecycle.cpp

namespace {

void* MemAlloc(uint64_t bytes, int align) {
    using Fn = void* (*)(uint64_t, int);
    static Fn fn = nullptr;
    if (!fn) fn = reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, 0x1800c7a50));
    return fn ? fn(bytes, align) : nullptr;
}

using VoidThisFn   = void  (*)(void*);
using NewFn        = void* (*)(uint64_t);
using RbCreateFn   = bool  (*)(void*, const void*, const void*);
using CompanionMk  = void* (*)(void*, void*);          // factory, subject
using BufSetSizeFn = void  (*)(void*, int32_t, bool, int32_t);
using ImgSlotFn    = void  (*)(void*);

template <typename T> inline T& Field(void* p, size_t off) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + off);
}
inline void* At(void* p, size_t off) { return static_cast<uint8_t*>(p) + off; }

// `?_singleton@Device@Video@@0PEAV12@EA` — a MINTED data slot, the same door
// sunlight.cpp and singleton_ctors.cpp use.
void** DeviceSingletonSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
}

// ---------------------------------------------------------------------------
// The INLINED ConstantBuffer constructor, emitted seven times by MSVC.
//
//   +0x00 = 0        +0x08 = 0        +0x10 = -1 as a QWORD (so +0x10 and
//   +0x14 both become -1)             +0x18 = 0 as a BYTE
//   +0x1c = 0 as an UNALIGNED QWORD (covers +0x1c..+0x23)
//   +0x28 = 0        +0x30 = 0        +0x38 = the Device
//   +0x40 = device->+0x100 ? deviceData->vtbl[11](deviceData, this) : 0
//
// +0x19..+0x1b are deliberately left alone; see the banner.
// ---------------------------------------------------------------------------
// B.3.5: the destination is the real `Video::ConstantBuffer` now, so every write
// but one is a named field. The exception is retail's `MOV qword ptr
// [RSI+0x1c],RBX` — a single UNALIGNED eight-byte store covering +0x1c..+0x23,
// i.e. `_registerCount` AND `_bankStride` AND the unproven `_unknown24`. It
// stays a `memset` because splitting it into two field stores would silently
// stop writing +0x24..+0x27, which retail does write here and nowhere else.
void InitConstantBuffer(Video::ConstantBuffer* buf, void* device) {
    buf->_prefix        = nullptr;                      // +0x00
    buf->_typeName      = nullptr;                      // +0x08
    Field<int64_t>(buf, 0x10) = -1;                     // _ordinalIndex AND _uploadIndex
    buf->_bindToVertex  = 0;                            // +0x18, a BYTE
    std::memset(At(buf, 0x1c), 0, 8);                   // +0x1c..+0x23, unaligned
    buf->_constantData  = nullptr;                      // +0x28
    Field<int64_t>(buf, 0x30) = 0;                      // _constantCount AND _capacity
    buf->_device        = device;                       // +0x38

    void* factory = device ? Field<void*>(device, 0x100) : nullptr;
    void* companion = nullptr;
    if (factory) {
        void** vt = *reinterpret_cast<void***>(factory);
        companion = reinterpret_cast<CompanionMk>(vt[11])(factory, buf);  // +0x58
    }
    buf->_platformData = companion;                     // +0x40
}

// One dynamic buffer: `operator new(0x20)`, two field writes, PlatformConstruct,
// store, then an UNGUARDED `p->+0x08->vtbl[1](companion, bytes, true, 0)`.
//
// The unguarded deref is retail's and is kept: when the allocation fails retail
// stores null and dereferences it anyway. Do not add a guard here — the project
// rule is that an unguarded deref in a faithful body means the value is always
// there, and the fix for a null is upstream.
void MakeDynamicBuffer(void* self, size_t slot, int32_t bytes,
                       VoidThisFn platformConstruct) {
    void* p = MemAlloc(0x20, 4);                        // 0x180011640
    if (p) {
        Field<int32_t>(p, 0x00) = 0;
        Field<uint8_t>(p, 0x04) = 0;
        platformConstruct(p);
    }
    Field<void*>(self, slot) = p;

    void* companion = Field<void*>(p, 0x08);
    void** vt = *reinterpret_cast<void***>(companion);
    reinterpret_cast<BufSetSizeFn>(vt[1])(companion, bytes, true, 0);
}

}  // namespace

// ============================================================================
// ??2ImageBuffer@Video@@SAPEAX_K@Z    retail 0x18001e910, ord 801
// ??2RenderBuffer@Video@@SAPEAX_K@Z   retail 0x18001f2e0, ord 861
// ============================================================================
// Two class `operator new`s, ported because the ctor's NAME doors exposed them
// as new cutover debt the moment it started asking for them by name (the
// unsatisfied-export set-diff went -1 +3, and these were two of the three).
//
// **ALIGNMENT IS THE WHOLE DIFFERENCE, AND IT WAS CHECKED BY BODY.** These two
// and 0x180011640 are the same nineteen instructions —
// `Enter(alloc+0x640); Allocate(alloc, n, align); Leave; return` — and differ
// only in one immediate: `MOV R8D,0x8` here against `MOV R8D,0x4` in
// 0x180011640. Reading one and assuming the others match by resemblance is
// exactly how this family has been got wrong before. Each was disassembled.
// ============================================================================
extern "C" void* RtVideo_ImageBuffer_OperatorNew(uint64_t bytes) {
    return MemAlloc(bytes, 8);                          // MOV R8D,0x8
}
extern "C" void* RtVideo_RenderBuffer_OperatorNew(uint64_t bytes) {
    return MemAlloc(bytes, 8);                          // MOV R8D,0x8
}

// ============================================================================
// The four sub-object zero-fills. All four return their argument in RAX.
// ============================================================================
extern "C" void* RtVideo_Ctx_ZeroTransformPair(void* p) {      // 0x180053730
    // Two 4x4 float matrices written COLUMN BY COLUMN (retail interleaves the
    // four rows of each), then ten zeroed qwords at +0x80..+0xd0.
    std::memset(p, 0, 0x80);
    for (size_t off = 0x80; off <= 0xc8; off += 8) Field<void*>(p, off) = nullptr;
    return p;
}

extern "C" void* RtVideo_Ctx_ZeroBoneMatrices(void* p) {       // 0x180053840
    // `MOV EDX,0x32` with a DEC/JNS loop = 51 iterations, not 50.
    std::memset(p, 0, 51u * 0x40u);
    return p;
}

extern "C" void* RtVideo_Ctx_ZeroTransformMatrices(void* p) {  // 0x1800538b0
    // `MOV EDX,0x33` -> 52. The ONLY difference from the body above.
    std::memset(p, 0, 52u * 0x40u);
    return p;
}

extern "C" void* RtVideo_Ctx_ZeroLightBlock(void* p) {         // 0x180053920
    // Twelve qwords, then every dword from +0x60 to +0x19f. Coverage checked
    // exhaustively against the listing: 0x00..0x19f with no hole.
    std::memset(p, 0, 0x1a0);
    return p;
}

// ============================================================================
// ??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z
//                                                   retail 0x1800f1820, ord 125
// ============================================================================
extern "C" void* RtVideo_Context_Ctor(Video::Context* self, void* device,
                                      int32_t vertexBytes, int32_t indexBytes,
                                      int32_t instanceBytes,
                                      void* renderBuffer) {
    Field<void*>(self, 0x00) = device;
    for (size_t off = 0x08; off <= 0x28; off += 8) Field<void*>(self, off) = nullptr;
    Field<void*>(self, 0xb0)   = nullptr;
    Field<void*>(self, 0x1b8)  = nullptr;
    Field<int32_t>(self, 0x1c0) = 0;
    for (size_t off = 0x1c8; off <= 0x290; off += 8) Field<void*>(self, off) = nullptr;

    InitConstantBuffer(&self->_materialConstants, device);   // mc_
    InitConstantBuffer(&self->_pixelConstants, device);      // pc_

    RtVideo_Ctx_ZeroTransformPair(At(self, 0x328));
    for (size_t off = 0x3f8; off <= 0x410; off += 8) Field<void*>(self, off) = nullptr;
    RtVideo_Ctx_ZeroBoneMatrices(At(self, 0x418));        // 51 x 0x40 -> +0x10d8
    RtVideo_Ctx_ZeroTransformMatrices(At(self, 0x10d8));  // 52 x 0x40 -> +0x1dd8
    RtVideo_Ctx_ZeroLightBlock(At(self, 0x1dd8));         // 0x1a0     -> +0x1f78

    InitConstantBuffer(&self->_globalConstants, device);     // gc_
    InitConstantBuffer(&self->_instanceConstants, device);   // ic_
    InitConstantBuffer(&self->_boneConstants, device);       // bc_
    InitConstantBuffer(&self->_transformConstants, device);  // tc_
    InitConstantBuffer(&self->_lightConstants, device);      // lc_

    Field<void*>(self, 0x20e0)   = nullptr;
    Field<int32_t>(self, 0x20e8) = 0;
    for (size_t off = 0x20f0; off <= 0x2108; off += 8) Field<void*>(self, off) = nullptr;
    Field<int32_t>(self, 0x2110) = 0;
    for (size_t off = 0x2118; off <= 0x2150; off += 8) Field<void*>(self, off) = nullptr;

    // 0x1800881b0 is the CRT memset (device_startup.cpp established this).
    std::memset(At(self, 0x30),  0, 0x80);
    std::memset(At(self, 0xb8),  0, 0x80);
    std::memset(At(self, 0x138), 0, 0x40);
    std::memset(At(self, 0x178), 0, 0x40);

    self->Video::Context::AssignConstants();

    // The Context's OWN backend companion, from the Device SINGLETON rather
    // than from the `device` argument — retail reads 0x1801fe7a8 here even
    // though it has a Device in RBP. Kept: the two are the same object in every
    // real call, and reading the argument instead would be a silent
    // normalisation.
    {
        void* singleton = *DeviceSingletonSlot();
        void* factory = singleton ? Field<void*>(singleton, 0x100) : nullptr;
        void* companion = nullptr;
        if (factory) {
            void** vt = *reinterpret_cast<void***>(factory);
            companion = reinterpret_cast<CompanionMk>(vt[9])(factory, self); // +0x48
        }
        Field<void*>(self, 0x2160) = companion;
    }

    if (vertexBytes > 0)
        MakeDynamicBuffer(self, 0x20f8, vertexBytes,
            RT_EXPORT_ONCE(VoidThisFn,
                           "?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ"));
    if (indexBytes > 0)
        MakeDynamicBuffer(self, 0x2118, indexBytes,
            RT_EXPORT_ONCE(VoidThisFn,
                           "?PlatformConstruct@IndexBuffer@Video@@QEAAXXZ"));
    if (instanceBytes > 0)
        MakeDynamicBuffer(self, 0x2130, instanceBytes,
            RT_EXPORT_ONCE(VoidThisFn,
                           "?PlatformConstruct@VertexBuffer@Video@@QEAAXXZ"));

    if (renderBuffer) {
        // Retail JUMPS STRAIGHT TO THE RETURN here: with a render buffer handed
        // in, the two default ImageBuffers are never built and +0x2140/+0x2148
        // stay null. That is why ~Context null-checks them.
        Field<void*>(self, 0x2138) = renderBuffer;
        return self;
    }

    void* rb = RT_EXPORT_ONCE(NewFn, "??2RenderBuffer@Video@@SAPEAX_K@Z")(0x30);
    if (rb) {
        Field<void*>(rb, 0x00) = nullptr;
        Field<void*>(rb, 0x08) = nullptr;
        Field<void*>(rb, 0x10) = nullptr;
        RT_EXPORT_ONCE(VoidThisFn,
            "?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ")(rb);
    }
    Field<void*>(self, 0x2138) = rb;

    void* color = RT_EXPORT_ONCE(NewFn, "??2ImageBuffer@Video@@SAPEAX_K@Z")(0x68);
    if (color) RT_EXPORT_ONCE(VoidThisFn, "??0ImageBuffer@Video@@QEAA@XZ")(color);
    Field<void*>(self, 0x2140) = color;

    void* depth = RT_EXPORT_ONCE(NewFn, "??2ImageBuffer@Video@@SAPEAX_K@Z")(0x68);
    if (depth) RT_EXPORT_ONCE(VoidThisFn, "??0ImageBuffer@Video@@QEAA@XZ")(depth);
    Field<void*>(self, 0x2148) = depth;

    // Two UNGUARDED companion calls on the images just built — slot 1 on the
    // colour image, slot 2 on the depth image. Different slots, and swapping
    // them would be silent.
    {
        void* c = Field<void*>(Field<void*>(self, 0x2140), 0x60);
        reinterpret_cast<ImgSlotFn>((*reinterpret_cast<void***>(c))[1])(c);
    }
    {
        void* d = Field<void*>(Field<void*>(self, 0x2148), 0x60);
        reinterpret_cast<ImgSlotFn>((*reinterpret_cast<void***>(d))[2])(d);
    }

    RT_EXPORT_ONCE(RbCreateFn,
        "?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z")(
            Field<void*>(self, 0x2138), Field<void*>(self, 0x2140),
            Field<void*>(self, 0x2148));
    return self;
}

// ============================================================================
// ?CreateVideoContext@MainLoop@Engine@@AEAAXXZ     retail 0x1800dd500, ord 2139
// ============================================================================
// Replace `MainLoop::+0x10`. The three sizes are hard-coded at the call site and
// the render buffer is NULL, so the live shape is precisely the branch set that
// probe configuration B drives.
//
// The old Context is destroyed and freed BEFORE the new block is allocated, so
// the allocator may well hand back the same address; nothing here may assume
// otherwise.
// ============================================================================
extern "C" void RtEngine_MainLoop_CreateVideoContext(void* self) {
    if (void* old = Field<void*>(self, 0x10)) {
        RtVideo_Context_Dtor(static_cast<Video::Context*>(old));
        Video_Context_OperatorDelete(old);   // was RT_EXPORT_ONCE("??3Context@Video@@SAXPEAX@Z")
        Field<void*>(self, 0x10) = nullptr;
    }

    // Retail inlines `Enter ; Allocate(alloc, 0x2168, 8) ; Leave`, which is
    // MemAlloc(0x2168, 8) — see the banner.
    void* block = MemAlloc(sizeof(Video::Context), 8);
    if (!block) {
        Field<void*>(self, 0x10) = nullptr;
        return;
    }
    void* device = *DeviceSingletonSlot();
    Field<void*>(self, 0x10) = RtVideo_Context_Ctor(static_cast<Video::Context*>(block),
                                                    device, 0x800000, 0x400000,
                                                    0x100000, nullptr);
}

// ============================================================================
int RtVideoContextCtorRegister() {
    int n = 0;
    RegisterRuntimeExport("??0Context@Video@@QEAA@PEAVDevice@1@HHHPEAVRenderBuffer@1@@Z",
                          (void*)&RtVideo_Context_Ctor); ++n;
    RegisterRuntimeAddr(0x1800f1820, (void*)&RtVideo_Context_Ctor);

    RegisterRuntimeExport("?CreateVideoContext@MainLoop@Engine@@AEAAXXZ",
                          (void*)&RtEngine_MainLoop_CreateVideoContext); ++n;
    RegisterRuntimeAddr(0x1800dd500, (void*)&RtEngine_MainLoop_CreateVideoContext);

    RegisterRuntimeExport("??2ImageBuffer@Video@@SAPEAX_K@Z",
                          (void*)&RtVideo_ImageBuffer_OperatorNew); ++n;
    RegisterRuntimeAddr(0x18001e910, (void*)&RtVideo_ImageBuffer_OperatorNew);

    RegisterRuntimeExport("??2RenderBuffer@Video@@SAPEAX_K@Z",
                          (void*)&RtVideo_RenderBuffer_OperatorNew); ++n;
    RegisterRuntimeAddr(0x18001f2e0, (void*)&RtVideo_RenderBuffer_OperatorNew);

    // Unexported, so address-only.
    RegisterRuntimeAddr(0x180053730, (void*)&RtVideo_Ctx_ZeroTransformPair);
    RegisterRuntimeAddr(0x180053840, (void*)&RtVideo_Ctx_ZeroBoneMatrices);
    RegisterRuntimeAddr(0x1800538b0, (void*)&RtVideo_Ctx_ZeroTransformMatrices);
    RegisterRuntimeAddr(0x180053920, (void*)&RtVideo_Ctx_ZeroLightBlock);
    return n;
}
