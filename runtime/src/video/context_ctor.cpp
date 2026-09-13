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

enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);
uintptr_t ResolveRuntimeRaw(uintptr_t ghidra_addr);

extern "C" void RtVideo_Context_AssignConstants(void* self);   // constant_buffer.cpp
extern "C" void RtVideo_Context_Dtor(RtVideo::Context* self);  // context_lifecycle.cpp

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
void InitConstantBuffer(void* buf, void* device) {
    Field<void*>(buf, 0x00)   = nullptr;
    Field<void*>(buf, 0x08)   = nullptr;
    Field<int64_t>(buf, 0x10) = -1;                     // +0x10 AND +0x14
    Field<uint8_t>(buf, 0x18) = 0;
    // Unaligned by construction — retail's `MOV qword ptr [RSI+0x1c],RBX`.
    std::memset(At(buf, 0x1c), 0, 8);                   // +0x1c..+0x23
    Field<void*>(buf, 0x28)   = nullptr;
    Field<void*>(buf, 0x30)   = nullptr;
    Field<void*>(buf, 0x38)   = device;

    void* factory = device ? Field<void*>(device, 0x100) : nullptr;
    void* companion = nullptr;
    if (factory) {
        void** vt = *reinterpret_cast<void***>(factory);
        companion = reinterpret_cast<CompanionMk>(vt[11])(factory, buf);  // +0x58
    }
    Field<void*>(buf, 0x40) = companion;
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
extern "C" void* RtVideo_Context_Ctor(void* self, void* device,
                                      int32_t vertexBytes, int32_t indexBytes,
                                      int32_t instanceBytes,
                                      void* renderBuffer) {
    Field<void*>(self, 0x00) = device;
    for (size_t off = 0x08; off <= 0x28; off += 8) Field<void*>(self, off) = nullptr;
    Field<void*>(self, 0xb0)   = nullptr;
    Field<void*>(self, 0x1b8)  = nullptr;
    Field<int32_t>(self, 0x1c0) = 0;
    for (size_t off = 0x1c8; off <= 0x290; off += 8) Field<void*>(self, off) = nullptr;

    InitConstantBuffer(At(self, 0x298), device);   // mc_ MaterialConstants
    InitConstantBuffer(At(self, 0x2e0), device);   // pc_ PixelConstants

    RtVideo_Ctx_ZeroTransformPair(At(self, 0x328));
    for (size_t off = 0x3f8; off <= 0x410; off += 8) Field<void*>(self, off) = nullptr;
    RtVideo_Ctx_ZeroBoneMatrices(At(self, 0x418));        // 51 x 0x40 -> +0x10d8
    RtVideo_Ctx_ZeroTransformMatrices(At(self, 0x10d8));  // 52 x 0x40 -> +0x1dd8
    RtVideo_Ctx_ZeroLightBlock(At(self, 0x1dd8));         // 0x1a0     -> +0x1f78

    InitConstantBuffer(At(self, 0x1f78), device);  // gc_ GlobalConstants
    InitConstantBuffer(At(self, 0x1fc0), device);  // ic_ InstanceConstants
    InitConstantBuffer(At(self, 0x2008), device);  // bc_ BoneConstants
    InitConstantBuffer(At(self, 0x2050), device);  // tc_ TransformConstants
    InitConstantBuffer(At(self, 0x2098), device);  // lc_ LightConstants

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

    RtVideo_Context_AssignConstants(self);

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
        RtVideo_Context_Dtor(static_cast<RtVideo::Context*>(old));
        RT_EXPORT_ONCE(VoidThisFn, "??3Context@Video@@SAXPEAX@Z")(old);
        Field<void*>(self, 0x10) = nullptr;
    }

    // Retail inlines `Enter ; Allocate(alloc, 0x2168, 8) ; Leave`, which is
    // MemAlloc(0x2168, 8) — see the banner.
    void* block = MemAlloc(sizeof(RtVideo::Context), 8);
    if (!block) {
        Field<void*>(self, 0x10) = nullptr;
        return;
    }
    void* device = *DeviceSingletonSlot();
    Field<void*>(self, 0x10) = RtVideo_Context_Ctor(block, device,
                                                    0x800000, 0x400000,
                                                    0x100000, nullptr);
}

// ============================================================================
// THE PROBE — two configurations, chosen to cover the branch set
// ============================================================================
namespace {

// --- the device-level fake ---------------------------------------------------
struct FakeCompanion { void** vtbl; int32_t tag; int32_t calls; };
struct FakeFactory   { void** vtbl; int32_t made; FakeCompanion pool[256]; };

uint64_t g_companionSlotHits = 0;     // bitmask of the slots actually called

// One trampoline per slot, so no call can land on a null and every call says
// WHICH slot it was. The signature is the widest of the ones retail uses
// (`(this, int32, bool, int32)`); a Microsoft-x64 callee may always ignore
// arguments it was not passed.
template <int N>
void* FakeCompanionSlot(void* c, int32_t a, bool, int32_t) {
    g_companionSlotHits |= (1ull << N);
    FakeCompanion* fc = static_cast<FakeCompanion*>(c);
    ++fc->calls;
    if (N == 1) fc->tag = a;          // record the size the ctor asked for
    return nullptr;
}

constexpr int kFakeSlots = 64;
void* g_fakeCompanionVt[kFakeSlots] = {};

template <int N> struct FillCompanionVt {
    static void Do(void** vt) {
        vt[N] = reinterpret_cast<void*>(&FakeCompanionSlot<N>);
        FillCompanionVt<N - 1>::Do(vt);
    }
};
template <> struct FillCompanionVt<-1> { static void Do(void**) {} };
uint64_t g_factorySlotHits = 0;       // bitmask of the FACTORY slots called

// Same lesson as the companion vtable, one rung up: the first version wired
// only slots 9 and 11 (the two the ctor itself uses) and the run died
// EXEC-of-0 on a null slot, because the ported PlatformConstructs reach the
// same factory for their OWN companions. Every slot is implemented and every
// slot mints, so no call can land on a null and the probe reports the set.
template <int N>
void* FakeFactorySlotN(void* f, void* subject);

void* g_fakeFactoryVt[kFakeSlots] = {};

void* FakeFactoryMake(void* f, void* /*subject*/) {
    FakeFactory* ff = static_cast<FakeFactory*>(f);
    if (ff->made >= 256) return nullptr;
    FakeCompanion* c = &ff->pool[ff->made++];
    c->vtbl = g_fakeCompanionVt;
    c->tag = 0;
    c->calls = 0;
    return c;
}
template <int N>
void* FakeFactorySlotN(void* f, void* subject) {
    g_factorySlotHits |= (1ull << N);
    return FakeFactoryMake(f, subject);
}

template <int N> struct FillFactoryVt {
    static void Do(void** vt) {
        vt[N] = reinterpret_cast<void*>(&FakeFactorySlotN<N>);
        FillFactoryVt<N - 1>::Do(vt);
    }
};
template <> struct FillFactoryVt<-1> { static void Do(void**) {} };

void InstallFakeVtbls() {
    FillFactoryVt<kFakeSlots - 1>::Do(g_fakeFactoryVt);
    FillCompanionVt<kFakeSlots - 1>::Do(g_fakeCompanionVt);
}

// A Device is only read at +0x100 by anything the ctor reaches, but the ported
// PlatformConstructs read more of it, so give the fake a full-size body.
alignas(16) uint8_t g_fakeDevice[0x400];
FakeFactory g_fakeFactory;

alignas(16) uint8_t g_ctxOurs[sizeof(RtVideo::Context)];
alignas(16) uint8_t g_ctxTheirs[sizeof(RtVideo::Context)];

int g_cbCompanion = 0, g_cbNull = 0;   // branch counters INSIDE InitConstantBuffer

const size_t kBufOffs[7] = { 0x298, 0x2e0, 0x1f78, 0x1fc0, 0x2008, 0x2050, 0x2098 };

int CompareCtx(void* a, void* b, bool quiet, bool companionsAreFakes) {
    int bad = 0;
#define VID2_C(cond, fmt, ...)                                                \
    do { if (!(cond)) { ++bad; if (!quiet)                                    \
            printf("[CtxCtor] *** " fmt "\n", __VA_ARGS__); } } while (0)

    // The whole zero-filled field map, compared as bytes EXCEPT the seven
    // ConstantBuffers (whose +0x19..+0x1b are never written) and the seven
    // companion pointers (which are distinct objects on the two sides).
    static const struct { size_t off, len; } kZeroRuns[] = {
        { 0x08, 0x28 }, { 0x30, 0x80 }, { 0xb0, 0x08 }, { 0xb8, 0x80 },
        { 0x138, 0x40 }, { 0x178, 0x40 }, { 0x1b8, 0x0c },
        { 0x1c8, 0xd0 },   // 0x1c8..0x297, NOT 0x1d0 -- see below { 0x328, 0xd0 }, { 0x3f8, 0x20 },
        { 0x418, 51 * 0x40 }, { 0x10d8, 52 * 0x40 }, { 0x1dd8, 0x1a0 },
        { 0x20e0, 0x0c },
        // +0x20f0 and +0x2100..+0x210f only. **+0x20f8 is DELIBERATELY NOT
        // HERE**: it is the dynamic vertex buffer, a heap pointer that is a
        // different allocation on each side, and byte-comparing it failed 100%
        // of the time in configuration B for a reason with nothing to do with
        // the port. It is compared by PRESENCE below, with the other five
        // sub-object pointers.
        { 0x20f0, 0x08 }, { 0x2100, 0x10 }, { 0x2110, 0x04 },
    };
    for (const auto& r : kZeroRuns)
        VID2_C(std::memcmp(At(a, r.off), At(b, r.off), r.len) == 0,
               "field run +0x%04zx len 0x%zx differs ***", r.off, r.len);

    for (size_t off : kBufOffs) {
        void* x = At(a, off);
        void* y = At(b, off);
        VID2_C(Field<int32_t>(x, 0x10) == Field<int32_t>(y, 0x10),
               "buf+0x%04zx +0x10 ours=%d theirs=%d ***", off,
               Field<int32_t>(x, 0x10), Field<int32_t>(y, 0x10));
        VID2_C(Field<int32_t>(x, 0x14) == Field<int32_t>(y, 0x14),
               "buf+0x%04zx +0x14 ours=%d theirs=%d ***", off,
               Field<int32_t>(x, 0x14), Field<int32_t>(y, 0x14));
        VID2_C(Field<uint8_t>(x, 0x18) == Field<uint8_t>(y, 0x18),
               "buf+0x%04zx +0x18 ours=%d theirs=%d ***", off,
               Field<uint8_t>(x, 0x18), Field<uint8_t>(y, 0x18));
        VID2_C(Field<void*>(x, 0x38) == Field<void*>(y, 0x38),
               "buf+0x%04zx device ours=%p theirs=%p ***", off,
               Field<void*>(x, 0x38), Field<void*>(y, 0x38));
        // The companion is a distinct object per side when a fake is in play,
        // so compare only whether one EXISTS. Comparing the pointers would fail
        // 100% of the time for a reason unrelated to the port.
        VID2_C((Field<void*>(x, 0x40) != nullptr) ==
               (Field<void*>(y, 0x40) != nullptr),
               "buf+0x%04zx companion presence ours=%d theirs=%d ***", off,
               Field<void*>(x, 0x40) != nullptr,
               Field<void*>(y, 0x40) != nullptr);
    }

    VID2_C((Field<void*>(a, 0x2160) != nullptr) ==
           (Field<void*>(b, 0x2160) != nullptr),
           "+0x2160 contextData presence ours=%d theirs=%d ***",
           Field<void*>(a, 0x2160) != nullptr, Field<void*>(b, 0x2160) != nullptr);

    for (size_t off : { (size_t)0x20f8, (size_t)0x2118, (size_t)0x2130,
                        (size_t)0x2138, (size_t)0x2140, (size_t)0x2148 }) {
        const bool ao = Field<void*>(a, off) != nullptr;
        const bool bo = Field<void*>(b, off) != nullptr;
        VID2_C(ao == bo, "+0x%04zx presence ours=%d theirs=%d ***", off, ao, bo);
    }
    (void)companionsAreFakes;
#undef VID2_C
    return bad;
}

}  // namespace

extern "C" void RtVideoContextCtorProbeMaybeRun();
namespace { bool g_ctxProbeArmed = false; }
extern "C" void RtVideoContextCtorProbeArm(bool on) { g_ctxProbeArmed = on; }

extern "C" void RtVideoContextCtorProbeMaybeRun() {
    if (!g_ctxProbeArmed) return;
    g_ctxProbeArmed = false;

    using CtorFn = void* (*)(void*, void*, int32_t, int32_t, int32_t, void*);
    const uintptr_t shipped = ResolveRuntimeRaw(0x1800f1820);
    if (!shipped) {
        printf("[CtxCtor] SKIP -- the shipped ctor is not mapped "
                    "(cutover); no oracle is possible without it\n");
        return;
    }
    CtorFn theirs = reinterpret_cast<CtorFn>(shipped);

    void** slot = DeviceSingletonSlot();
    void* savedSingleton = *slot;

    // ---- CONFIGURATION A: no device, no sizes, a caller-supplied render
    // buffer. Every guarded virtual takes its null branch and every unguarded
    // one is skipped. Nothing is allocated.
    std::memset(g_fakeDevice, 0, sizeof(g_fakeDevice));
    uint8_t dummyRenderBuffer[0x30] = {};
    *slot = g_fakeDevice;                 // +0x100 is null in a zeroed device

    std::memset(g_ctxOurs, 0xCD, sizeof(g_ctxOurs));
    std::memset(g_ctxTheirs, 0xCD, sizeof(g_ctxTheirs));
    g_cbCompanion = g_cbNull = 0;
    RtVideo_Context_Ctor(g_ctxOurs, g_fakeDevice, 0, 0, 0, dummyRenderBuffer);
    theirs(g_ctxTheirs, g_fakeDevice, 0, 0, 0, dummyRenderBuffer);

    const int badA = CompareCtx(g_ctxOurs, g_ctxTheirs, false, false);
    int aNullCompanions = 0;
    for (size_t off : kBufOffs)
        if (Field<void*>(g_ctxTheirs, off + 0x40) == nullptr) ++aNullCompanions;
    printf("[CtxCtor] A (no device, sizes 0, caller render buffer): "
                "mismatches=%d\n", badA);
    printf("[CtxCtor] A effectiveness (on THEIRS): "
                "null-companion buffers=%d/7 renderBuffer=%p colour=%p depth=%p\n",
                aNullCompanions, Field<void*>(g_ctxTheirs, 0x2138),
                Field<void*>(g_ctxTheirs, 0x2140),
                Field<void*>(g_ctxTheirs, 0x2148));

    // Negative control A: corrupt one matrix word and one buffer scalar.
    Field<float>(g_ctxOurs, 0x10d8 + 51 * 0x40) += 1.0f;
    const int cA1 = CompareCtx(g_ctxOurs, g_ctxTheirs, true, false);
    Field<float>(g_ctxOurs, 0x10d8 + 51 * 0x40) -= 1.0f;
    Field<int32_t>(g_ctxOurs, 0x1f78 + 0x14) ^= 1;
    const int cA2 = CompareCtx(g_ctxOurs, g_ctxTheirs, true, false);
    Field<int32_t>(g_ctxOurs, 0x1f78 + 0x14) ^= 1;
    printf("[CtxCtor] A negative-controls: matrix-tail=%s buffer-scalar=%s\n",
                cA1 > badA ? "CAUGHT" : "*** MISSED ***",
                cA2 > badA ? "CAUGHT" : "*** MISSED ***");

    // ---- CONFIGURATION B: the LIVE shape CreateVideoContext passes.
    //
    // **The fake device had to SHRINK, not grow, and the measurement said so.**
    // The first version swapped a zeroed 0x400-byte buffer into the singleton
    // slot; the run died in ntdll on a WRITE of 0x24 from
    // `RtVideoDeviceLock` <- `Video_VertexBuffer_PlatformConstruct`. So the
    // ctor's real dependency on a Device is NOT the seven virtual calls (those
    // are null-checked and a zeroed device satisfies them); it is that
    // PlatformConstruct links each dynamic buffer into a device-wide list under
    // a CRITICAL_SECTION that lives inside the Device — and a zeroed one is not
    // an initialised lock.
    //
    // So configuration B keeps the REAL Device singleton, with its real lock and
    // its real buffer list, and borrows exactly ONE FIELD: `device->+0x100`, the
    // backend DeviceData factory, which is still null this early in
    // PlatformStartup. Nothing DX11 is touched, the lock is real, and the field
    // is restored afterwards.
    void* realDevice = savedSingleton;
    if (!realDevice) {
        printf("[CtxCtor] B SKIP -- no Video::Device singleton yet, so the "
                    "real device lock PlatformConstruct needs does not exist\n");
        *slot = savedSingleton;
        return;
    }
    std::memset(&g_fakeFactory, 0, sizeof(g_fakeFactory));
    InstallFakeVtbls();
    g_fakeFactory.vtbl = g_fakeFactoryVt;
    *slot = realDevice;
    void* savedFactory = Field<void*>(realDevice, 0x100);
    Field<void*>(realDevice, 0x100) = &g_fakeFactory;

    std::memset(g_ctxOurs, 0xCD, sizeof(g_ctxOurs));
    g_companionSlotHits = g_factorySlotHits = 0;
    RtVideo_Context_Ctor(g_ctxOurs, realDevice, 0x800000, 0x400000,
                         0x100000, nullptr);
    const int ourMade = g_fakeFactory.made;
    const uint64_t ourSlots = g_companionSlotHits;
    const uint64_t ourFactorySlots = g_factorySlotHits;

    g_fakeFactory.made = 0;
    std::memset(g_fakeFactory.pool, 0, sizeof(g_fakeFactory.pool));
    std::memset(g_ctxTheirs, 0xCD, sizeof(g_ctxTheirs));
    g_companionSlotHits = g_factorySlotHits = 0;
    theirs(g_ctxTheirs, realDevice, 0x800000, 0x400000, 0x100000, nullptr);
    const int theirMade = g_fakeFactory.made;
    const uint64_t theirSlots = g_companionSlotHits;
    const uint64_t theirFactorySlots = g_factorySlotHits;

    const int badB = CompareCtx(g_ctxOurs, g_ctxTheirs, false, true);
    printf("[CtxCtor] B (live shape 0x800000/0x400000/0x100000, "
                "no render buffer): mismatches=%d\n", badB);
    printf("[CtxCtor] B effectiveness: companions minted ours=%d "
                "theirs=%d (7 constant buffers + the Context + the six that "
                "the three PlatformConstructs and the two ImageBuffers mint "
                "through the same factory = 14)\n",
                ourMade, theirMade);
    printf("[CtxCtor] B companion slots called: ours=0x%016llx theirs=0x%016llx %s\n",
                (unsigned long long)ourSlots, (unsigned long long)theirSlots,
                ourSlots == theirSlots ? "" : "*** DIFFER ***");
    printf("[CtxCtor] B factory   slots called: ours=0x%016llx theirs=0x%016llx %s\n",
                (unsigned long long)ourFactorySlots, (unsigned long long)theirFactorySlots,
                ourFactorySlots == theirFactorySlots ? "" : "*** DIFFER ***");
    if (ourMade != theirMade)
        printf("[CtxCtor] *** companion count differs -- a buffer was "
                    "constructed on one side and not the other ***\n");

    Field<void*>(g_ctxOurs, 0x2140) = nullptr;
    const int cB1 = CompareCtx(g_ctxOurs, g_ctxTheirs, true, true);
    printf("[CtxCtor] B negative-control (default colour image): %s\n",
                cB1 > badB ? "CAUGHT" : "*** MISSED ***");

    // Tear both down through the ported destructor, so the probe does not leak
    // two render buffers, four image buffers and six dynamic buffers.
    RtVideo_Context_Dtor(reinterpret_cast<RtVideo::Context*>(g_ctxOurs));
    RtVideo_Context_Dtor(reinterpret_cast<RtVideo::Context*>(g_ctxTheirs));

    Field<void*>(realDevice, 0x100) = savedFactory;
    *slot = savedSingleton;
    printf("[CtxCtor] Device singleton and its +0x100 factory restored "
                "(%p / %p)\n", savedSingleton, savedFactory);
}

// ============================================================================
// Registration
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
