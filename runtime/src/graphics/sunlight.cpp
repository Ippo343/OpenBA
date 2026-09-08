// ============================================================================
// runtime/src/graphics/sunlight.cpp — Graphics::SunLight   Wave 9 lane GFX2
// ============================================================================
//
// Three functions, two of them on the cutover residue list from attempt 14
// (docs/runtime_port/wave9_attempt14_unresolved.txt):
//
//   0x1801081f0  ??0SunLight@Graphics@@QEAA@XZ                    <- residue row
//   0x180107f10  ?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z
//                                                                 <- residue row
//   0x180108160  ??1SunLight@Graphics@@UEAA@XZ                    (not on the list;
//                ported anyway — see "Why the dtor is here" below)
//
// All three mangled names were verified present in the shipped export table
// (tools/pe_exports.py over the shipped Runtime-x64.dll) BEFORE anything here
// was written, per reference_resolveexport_phantom_names.md.
//
// ---------------------------------------------------------------------------
// WHY THIS LANDS NOW, AND WHAT LANE GFX'S BLOCKER WAS
// ---------------------------------------------------------------------------
// Wave 9 lane GFX declined this pair (WAVE9_LANE_GFX.md §6.5) on one live
// reason: "the real blocker is two unported functions, and it is the ctor that
// needs them — ?PlatformConstruct@RenderBuffer@Video@@ (0x1800f4320) and
// ?PlatformDestruct@RenderBuffer@Video@@ (0x1800f4400) are exported and
// UNPORTED". Its recommended order was to land those two first so that porting
// SunLight closes a row rather than moving it one frame deeper. Both are now
// ported, in runtime/src/video/buffers.cpp, as the fourth instantiation of Wave
// 9 lane W's Platform{Construct,Destruct} family. So the triage rule this
// project uses — "a port closes a cutover row only if every callee it needs is
// already satisfied on OUR side" — is now satisfied for the ctor:
//
//   Graphics::Geometry::Geometry     0x1800a4270   ported (scene.cpp, lane D)
//   Video::ImageBuffer::ImageBuffer  0x1800f2580   ported (buffers.cpp, lane C)
//   Video::ImageBuffer::Create       vtable +0x48  ported (buffers.cpp, lane C)
//   Video::RenderBuffer::PlatformConstruct         ported (buffers.cpp, THIS lane)
//   Video::RenderBuffer::PlatformDestruct          ported (buffers.cpp, THIS lane)
//   Video::RenderBuffer::Create      0x1800f3f10   ported (dynamic_resources.cpp)
//   MemoryAllocator::MemAlloc/MemFree              ported (system/allocator.cpp)
//   Graphics::Geometry::~Geometry    0x1800a41c0   (dtor only) ported (scene.cpp)
//
// ---------------------------------------------------------------------------
// THE FACT THAT CHANGES THE SHAPE OF ALL THREE BODIES: DeviceData SLOT 6 IS
// `xor al,al; ret`
// ---------------------------------------------------------------------------
// Both the ctor and `Set` open by asking the device a yes/no question:
//
//     dd = Video::Device::_singleton->deviceData;   // Device+0x100
//     cap = (*(bool(**)(void*))((*(char**)dd) + 0x30))(dd);   // vtable slot 6
//
// On the DX11 backend that call is a hardcoded FALSE, and this is verified two
// independent ways rather than assumed:
//
//   1. The shipped VideoDX11-x64.dll's DeviceData vtable is at 0x180018bb0;
//      the eight bytes at 0x180018be0 (slot 6) read `40 54 00 80 01 00 00 00`
//      = 0x180005440, and 0x180005440 is the two-byte body `32 c0 / c3`
//      — `xor al,al; ret`.
//   2. VideoDX11 is ALREADY REPLACED by dx11/ and is never loaded; our own
//      slot 6 is dx11/src/devicedata_stubs.cpp, whose header records the same
//      two bytes and which returns false.
//
// So on this backend, on both sides of the port:
//
//   * the ctor's capability-gated block is DEAD — `_colourTarget` (+0xF0) is
//     never built and stays null for the object's whole life;
//   * `Set`'s FIRST rebuild branch needs `cap == true`  -> unreachable;
//   * `Set`'s SECOND rebuild branch needs `cap == false && _colourTarget != 0`
//     -> unreachable, because nothing else ever writes +0xF0.
//
// **`SunLight::Set` is therefore exactly eleven dword copies on this backend,
// every call.** That is a stronger and differently-derived statement than
// WAVE9_LANE_GFX.md §6.5's, which said the rebuild branches are skipped "with
// the capability present and +0xf0 non-null (the state the ctor leaves)". The
// conclusion was right; the stated condition was the exact inverse of the live
// one, and the state the ctor leaves is +0xf0 NULL. Recorded because a later
// lane reading that sentence would look for the capability to be TRUE and find
// the opposite.
//
// The dead branches are still TRANSCRIBED here, faithfully, because the gate is
// a property of the BACKEND and not of SunLight: a DX9 device could answer
// true. They carry their own branch counters so the claim "unreachable" is
// measured on every diff run rather than asserted (see the [branch:] sites).
//
// ---------------------------------------------------------------------------
// WHY THE DTOR IS HERE THOUGH IT IS NOT ON THE RESIDUE LIST
// ---------------------------------------------------------------------------
// `??1SunLight@Graphics@@UEAA@XZ` is reached through the SunLight vtable, not by
// name, which is why BANISHED_RT_STRICT never recorded it. At cutover that
// vtable is minted, and a minted vtable slot pointing at a DLL that is not
// there is the "mint is blind to what its own minted vtables READ" failure
// (reference_mint_population_rule_blind_to_its_own_output.md). It is also
// nineteen instructions, it is the exact teardown the ctor's construction
// needs, and the probe below needs a teardown for BOTH sides. Porting it is
// cheaper than arguing about it.
//
// ---------------------------------------------------------------------------
// operator new / operator delete
// ---------------------------------------------------------------------------
// The ctor uses three allocation sites and they are all the same thing:
//   0x18001e910  `operator new`    called;    lock alloc+0x640, Allocate(n,8), unlock
//   0x18001f2e0  `operator new`    inlined;   identical body
//   0x18001f360  `operator delete` called;    if (p) { lock, Free(p), unlock }
// Disassembled here rather than taken on trust: 0x18001e910 is
// `RDI = [_singleton]+0x640; Enter(RDI); Allocate([_singleton], size, 8);
// Leave(RDI)`, and 0x1800c7a50 (`?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z`)
// is byte-for-byte that with the alignment taken from EDX instead of an
// immediate 8. So `MemAlloc(n, 8)` / `if (p) MemFree(p)` reproduce all three
// exactly, and they route through the ported allocator instead of adding three
// more unregistered by-address doors.
// ============================================================================

#include "../../funcs/rt_port.h"
#include "../../funcs/rt_diff.h"
#include "../../types/graphics/SunLight.h"
#include "../../types/video/Device.h"      // RtVideo::Device, RtVideo::RenderBuffer

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

using RtGraphics::SunLight;

// Defined below; called from the ctor on the first diffed construction.
void RtGraphicsSunLightProbeCtor();
void  Graphics_SunLight_Dtor(SunLight* self);

namespace {

// ---------------------------------------------------------------------------
// Dependencies. PORT-AWARE (RT_EXPORT_ONCE) throughout: every one of these is a
// dependency call, not an oracle, so it must reach OUR body where one exists
// and must keep working with the DLL absent
// (reference_converting_a_door_is_not_satisfying_it.md).
// ---------------------------------------------------------------------------
using GeometryCtorFn      = void* (*)(void*);
using GeometryDtorFn      = void  (*)(void*);   // ~Geometry is UEAA@XZ: VOID. Our port (scene.cpp:622) is void too.
using ImageBufferCtorFn   = void* (*)(void*);
using MemAllocFn          = void* (*)(size_t, int);
using MemFreeFn           = void  (*)(void*);
using RenderBufferCreateFn= bool  (*)(void*, const void*, const void*);
using PlatformFn          = void  (*)(void*);
using DeletingDtorFn      = void  (*)(void*, int);

GeometryCtorFn GeometryCtor() {
    return RT_EXPORT_ONCE(GeometryCtorFn, "??0Geometry@Graphics@@QEAA@XZ");
}
GeometryDtorFn GeometryDtor() {
    return RT_EXPORT_ONCE(GeometryDtorFn, "??1Geometry@Graphics@@UEAA@XZ");
}
ImageBufferCtorFn ImageBufferCtor() {
    return RT_EXPORT_ONCE(ImageBufferCtorFn, "??0ImageBuffer@Video@@QEAA@XZ");
}
MemAllocFn MemAlloc() {
    return RT_EXPORT_ONCE(MemAllocFn, "?MemAlloc@MemoryAllocator@System@@SAPEAX_KH@Z");
}
MemFreeFn MemFree() {
    return RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z");
}
RenderBufferCreateFn RenderBufferCreate() {
    return RT_EXPORT_ONCE(RenderBufferCreateFn,
                          "?Create@RenderBuffer@Video@@QEAA_NPEBVImageBuffer@2@0@Z");
}
PlatformFn RenderBufferPlatformConstructPort() {
    return RT_EXPORT_ONCE(PlatformFn, "?PlatformConstruct@RenderBuffer@Video@@AEAAXXZ");
}
PlatformFn RenderBufferPlatformDestructPort() {
    return RT_EXPORT_ONCE(PlatformFn, "?PlatformDestruct@RenderBuffer@Video@@AEAAXXZ");
}

// The two vtables the ctor stamps, and the one the dtor restores. Exported
// DATA; ResolveExport is port-aware and the mint owns them at cutover.
void* LightVtable() {
    static void* v = ResolveExport("??_7Light@Graphics@@6B@");
    return v;
}
void* SunLightVtable() {
    static void* v = ResolveExport("??_7SunLight@Graphics@@6B@");
    return v;
}

RtVideo::Device** DeviceSlot() {
    return RT_DATA_SLOT(RtVideo::Device**, "?_singleton@Device@Video@@0PEAV12@EA");
}

// `dd->vtable[6]()` — the capability gate. See the header block: this is
// `xor al,al; ret` on DX11 and therefore always false in this process, but the
// call is made rather than folded away, because the answer belongs to the
// device and not to us.
bool DeviceSupportsColourShadowTarget() {
    RtVideo::Device* dev = *DeviceSlot();
    void* dd = dev->deviceData;
    using CapFn = bool (*)(void*);
    CapFn slot6 = *reinterpret_cast<CapFn*>(
        static_cast<uint8_t*>(*reinterpret_cast<void**>(dd)) + 0x30);
    return slot6(dd);
}

// Video::ImageBuffer::Create, vtable slot +0x48. Eleven arguments after `this`,
// counted against the mangled name
//   ?Create@ImageBuffer@Video@@UEAA_NW4ImageType@12@W4Usage@12@W4Format@12@IHH
//    W4FilterMode@12@W4AddressMode@12@44PEAV?$Array@V?$Array@E@Collection@@@Collection@@@Z
// = (ImageType, Usage, Format, unsigned, int, int, FilterMode, AddressMode x3,
//    Array*), which is exactly the four register slots plus the eight stack
// slots [RSP+0x20]..[RSP+0x58] the two call sites fill.
using ImageCreateFn = bool (*)(void*, int, int, int, unsigned, int, int,
                               int, int, int, int, void*);

bool ImageBufferCreate(void* ib, int imageType, int usage, int format,
                       unsigned flags, int w, int h,
                       int filter, int addrU, int addrV, int addrW, void* data) {
    ImageCreateFn slot = *reinterpret_cast<ImageCreateFn*>(
        static_cast<uint8_t*>(*reinterpret_cast<void**>(ib)) + 0x48);
    return slot(ib, imageType, usage, format, flags, w, h,
                filter, addrU, addrV, addrW, data);
}

// `operator new` / `operator delete`, transcribed — see the header block.
void* OperatorNew(size_t n) { return MemAlloc()(n, 8); }
void  OperatorDelete(void* p) { if (p != nullptr) MemFree()(p); }

// ---------------------------------------------------------------------------
// The two composite steps the ctor and Set share, spelled once.
// ---------------------------------------------------------------------------

// `p = operator new(0x68); ib = p ? ImageBuffer::ImageBuffer(p) : nullptr`
//
// Retail keeps the ctor's RETURN value, not `p`. The first version of this
// function transcribed that literally — `return ImageBufferCtor()(p);` — and it
// CRASHED on the first diff run, in `ImageBufferCreate` reading a vtable at
// -1 (standalone_crash.log, RIP inside ImageBufferCreate+0x76, with R8=1/R9=7/
// RSI=0x40 — this file's own depth-target argument tuple).
//
// THE CAUSE IS A PORT-SEAM ABI HAZARD, worth stating rather than just fixing.
// MSVC returns `this` in RAX from a constructor, so retail's `MOV RCX,RAX` is
// correct against the SHIPPED body. But `??0ImageBuffer@Video@@QEAA@XZ` is
// PORTED, and our port is declared `void Video_ImageBuffer_Ctor(ImageBuffer*)`
// (runtime/src/video/buffers.cpp:387) — a void-returning C++ function whose RAX
// is whatever the last expression left there. Reading a ported constructor's
// return value therefore reads garbage, on the DLL-present path AND at cutover.
// (`RtGraphics_Geometry_Ctor` does return `Geometry*`; the convention is not
// uniform across the tree, which is exactly why this must not be relied on.)
//
// `p` and the returned `this` are the same pointer by construction, so using
// `p` is byte-faithful to what retail computes while being independent of how
// any port spells its signature.
void* NewImageBuffer() {
    void* p = OperatorNew(0x68);
    if (p == nullptr)
        return nullptr;
    ImageBufferCtor()(p);
    return p;
}

// `rb = operator new(0x30); if (rb) { rb[0]=rb[8]=rb[0x10]=0;
//                                      RenderBuffer::PlatformConstruct(rb); }`
// This is `Video::RenderBuffer::RenderBuffer` (0x1800f3ee0) INLINED into both
// call sites — the zeroing of colorBuffer/depthBuffer/width+height followed by
// PlatformConstruct — and it is transcribed as the inline that it is rather
// than redirected to the ctor export, because the shipped bytes at both sites
// are the inline.
void* NewRenderBuffer() {
    void* rb = OperatorNew(0x30);
    if (rb == nullptr)
        return nullptr;
    uint64_t* w = static_cast<uint64_t*>(rb);
    w[0] = 0;   // +0x00 colorBuffer
    w[1] = 0;   // +0x08 depthBuffer
    w[2] = 0;   // +0x10 width | height
    RenderBufferPlatformConstructPort()(rb);
    return rb;
}

// `if (rb) { RenderBuffer::PlatformDestruct(rb); operator delete(rb); }`
void DeleteRenderBuffer(void* rb) {
    if (rb == nullptr)
        return;
    RenderBufferPlatformDestructPort()(rb);
    OperatorDelete(rb);
}

// `if (ib) (*ib->vtable[0])(ib, 1)` — the scalar deleting destructor
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md: slot 0 is ??_E, not ??_G).
void DeleteImageBuffer(void* ib) {
    if (ib == nullptr)
        return;
    DeletingDtorFn slot0 = *reinterpret_cast<DeletingDtorFn*>(*reinterpret_cast<void**>(ib));
    slot0(ib, 1);
}

// The two float constants the ctor's bounds block uses, LIFTED AS RAW BITS and
// not retyped from a decimal (reference_crt_coefficients_must_be_lifted_not_retyped.md).
//   0x18016173c = 00 24 74 49 = 0x49742400
//   0x180161778 = 00 24 74 c9 = 0xc9742400
float BitsToFloat(uint32_t b) { float f; std::memcpy(&f, &b, 4); return f; }
const float kBoundsMax = BitsToFloat(0x49742400u);   // DAT_18016173c
const float kBoundsMin = BitsToFloat(0xc9742400u);   // DAT_180161778

}  // namespace

// ============================================================================
// Graphics::SunLight::Set(const Math::Vector3& dir, const Math::Vector4& sun,
//                         const Math::Vector4& ambient)   — 0x180107f10
//
// ELEVEN DWORD COPIES, then two rebuild branches that this backend never takes.
// The copies are `MOV EAX,[src] / MOV [this+off],EAX` in the shipped body — no
// float arithmetic at all, so there is no association order to get wrong and no
// bit-exactness risk here (WAVE9_LANE_GFX.md §6.5 finding 1, confirmed).
//
// THE ORACLE, and its one refusal. On the copy-only path this is a pure写-once
// function of three inputs that our body does not touch, so it replays exactly:
// snapshot the eleven dwords, run ours, read them back, restore the snapshot,
// run the shipped body, read again, compare. The three SOURCE vectors are the
// caller's and are never written by either side, so the aliasing trap in
// reference_oracle_must_snapshot_aliased_inputs.md does not apply — but the
// DESTINATION is `this`, which is live state, which is why the snapshot is
// restored before the shipped body runs rather than after.
//
// If either rebuild branch is taken the diff is REFUSED, not faked: replaying
// would destroy and rebuild GPU resources a second time and the "comparison"
// would be between two different objects. That call is counted on its own site
// so a refusal is visible instead of silently inflating `calls=`.
// ============================================================================
void Graphics_SunLight_Set(SunLight* self, const float* dir,
                           const float* sun, const float* ambient) {
    RT_DIFF_BEGIN("Graphics::SunLight::Set");

    float pre[11];
    if (rt_diff_on__)
        std::memcpy(pre, &self->_dirX, sizeof(pre));

    // ---- the eleven copies, in the shipped order -------------------------
    self->_dirX     = dir[0];
    self->_dirY     = dir[1];
    self->_dirZ     = dir[2];
    self->_sunR     = sun[0];
    self->_sunG     = sun[1];
    self->_sunB     = sun[2];
    self->_sunA     = sun[3];
    self->_ambientR = ambient[0];
    self->_ambientG = ambient[1];
    self->_ambientB = ambient[2];
    self->_ambientA = ambient[3];

    // ---- the two rebuild branches ----------------------------------------
    static RtDiffSite brBuild = {
        "Graphics::SunLight::Set[branch:build-colour-target, DEAD while DeviceData slot 6 is `xor al,al;ret`]" };
    static RtDiffSite brTear = {
        "Graphics::SunLight::Set[branch:drop-colour-target, DEAD while DeviceData slot 6 is `xor al,al;ret`]" };
    if (rt_diff_on__) { RtDiffRegister(&brBuild); RtDiffRegister(&brTear); }

    bool rebuilt = false;
    if (DeviceSupportsColourShadowTarget() && self->_colourTarget == nullptr) {
        // The device grew the capability: build the colour target and a fresh
        // render buffer around it.
        if (rt_diff_on__) RtDiffReport(&brBuild, true, "taken", "taken");
        rebuilt = true;
        DeleteRenderBuffer(self->_renderBuffer);
        self->_colourTarget = NewImageBuffer();
        ImageBufferCreate(self->_colourTarget, 0, 0, 5, 0x40, 0x800, 0x800, 1, 1, 1, 1, nullptr);
        self->_renderBuffer = NewRenderBuffer();
        RenderBufferCreate()(self->_renderBuffer, self->_colourTarget, self->_depthTarget);
    } else if (!DeviceSupportsColourShadowTarget() && self->_colourTarget != nullptr) {
        // The device lost it: drop the colour target and rebuild depth-only.
        if (rt_diff_on__) RtDiffReport(&brTear, true, "taken", "taken");
        rebuilt = true;
        DeleteRenderBuffer(self->_renderBuffer);
        DeleteImageBuffer(self->_colourTarget);
        self->_colourTarget = nullptr;
        self->_renderBuffer = NewRenderBuffer();
        RenderBufferCreate()(self->_renderBuffer, self->_colourTarget, self->_depthTarget);
    }

    // ---- oracle -----------------------------------------------------------
    RT_DIFF_WORK(!rebuilt);   // a rebuild call verifies NOTHING here; say so
    if (rt_diff_on__) {
        if (rebuilt) {
            static RtDiffSite refused = {
                "Graphics::SunLight::Set[REFUSED: a rebuild branch ran, replay would double-build GPU state]" };
            RtDiffRegister(&refused);
            RtDiffReport(&refused, true, "not compared", "not compared");
        } else {
            float ours[11];
            std::memcpy(ours, &self->_dirX, sizeof(ours));
            std::memcpy(&self->_dirX, pre, sizeof(pre));
            using SetFn = void (*)(void*, const float*, const float*, const float*);
            RT_ORIGINAL_ONCE(SetFn,
                "?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z")(
                    self, dir, sun, ambient);
            RT_DIFF_BYTES(ours, &self->_dirX, sizeof(ours));
            // The object is deliberately LEFT holding RETAIL's values, not
            // restored to ours — the same way round buffers.cpp's oracle leaves
            // the list. If our copies were ever wrong, the frame after the
            // divergence renders on the DLL's numbers and the report is a log
            // line rather than a visibly wrong sun.
        }
    }
}

// ============================================================================
// Graphics::SunLight::SunLight()   — 0x1801081f0
//
// Base ctor, two vtable stamps into one slot, eleven scalar defaults, three null
// pointers, the material-type flag, the capability-gated colour target, the
// unconditional depth target, the render buffer, and an INLINED
// `Geometry::SetBounds(Box(-1e6,-1e6,-1e6, +1e6,+1e6,+1e6))`.
//
// The SetBounds tail is transcribed as an inline because that is what the
// shipped bytes are — six UCOMISS/JP/JNZ pairs against the two constants,
// falling through to six MOVSS and `_dirtyFlags |= 2` when any of the six
// differs. NaN takes the write path in retail (JP), and `!(a == b)` in C++ is
// true for NaN, so the two agree without a special case.
// ============================================================================
void* Graphics_SunLight_Ctor(SunLight* self) {
    RT_DIFF_BEGIN("Graphics::SunLight::SunLight");

    GeometryCtor()(self);

    // TWO stores into the SAME slot, in this order. Kept as two, not folded:
    // retail writes Light's vtable and then overwrites it with SunLight's, and
    // an intervening throw would be observed with Light's installed.
    self->_base._vtable = LightVtable();
    self->_base._vtable = SunLightVtable();

    self->_dirX = 0.0f;  self->_dirY = 0.0f;  self->_dirZ = -1.0f;
    self->_sunR = 1.0f;  self->_sunG = 1.0f;  self->_sunB = 1.0f;  self->_sunA = 1.0f;
    self->_ambientR = 0.0f; self->_ambientG = 0.0f; self->_ambientB = 0.0f;
    self->_ambientA = 1.0f;
    // +0xEC is NOT written by retail and is NOT written here.
    self->_colourTarget = nullptr;
    self->_depthTarget  = nullptr;
    self->_renderBuffer = nullptr;
    self->_base._materialTypeFlags = 0x8000;

    static RtDiffSite brColour = {
        "Graphics::SunLight::SunLight[branch:colour-target built, DEAD while DeviceData slot 6 is `xor al,al;ret`]" };
    static RtDiffSite brBounds = {
        "Graphics::SunLight::SunLight[branch:bounds differed -> written]" };
    static RtDiffSite brBoundsSkip = {
        "Graphics::SunLight::SunLight[branch:bounds already equal -> skipped]" };
    if (rt_diff_on__) {
        RtDiffRegister(&brColour);
        RtDiffRegister(&brBounds);
        RtDiffRegister(&brBoundsSkip);
    }

    if (DeviceSupportsColourShadowTarget()) {
        if (rt_diff_on__) RtDiffReport(&brColour, true, "taken", "taken");
        self->_colourTarget = NewImageBuffer();
        ImageBufferCreate(self->_colourTarget, 0, 0, 5, 0x40, 0x800, 0x800, 1, 1, 1, 1, nullptr);
    }

    // Unconditional — the join label 0x180108325 is BELOW the capability test's
    // JZ target, so the depth target and the render buffer are built whether or
    // not the colour target was.
    self->_depthTarget = NewImageBuffer();
    ImageBufferCreate(self->_depthTarget, 0, 1, 7, 0x40, 0x800, 0x800, 1, 1, 1, 1, nullptr);

    self->_renderBuffer = NewRenderBuffer();
    RenderBufferCreate()(self->_renderBuffer, self->_colourTarget, self->_depthTarget);

    // Inlined Geometry::SetBounds.
    auto& b = self->_base._localBounds;
    const bool same = b.min.x == kBoundsMin && b.min.y == kBoundsMin && b.min.z == kBoundsMin &&
                      b.max.x == kBoundsMax && b.max.y == kBoundsMax && b.max.z == kBoundsMax;
    if (!same) {
        b.min.x = kBoundsMin; b.min.y = kBoundsMin; b.min.z = kBoundsMin;
        b.max.x = kBoundsMax; b.max.y = kBoundsMax; b.max.z = kBoundsMax;
        self->_base._dirtyFlags |= 2u;
    }
    if (rt_diff_on__) {
        if (!same) RtDiffReport(&brBounds, true, "taken", "taken");
        else       RtDiffReport(&brBoundsSkip, true, "taken", "taken");
    }

    // The ctor's own oracle is the once-per-process probe below, not a replay:
    // running the shipped ctor over THIS object would build a second render
    // target set and orphan ours. See RtGraphicsSunLightProbeCtor.
    if (rt_diff_on__)
        RtGraphicsSunLightProbeCtor();

    // Report on this site so it does not print `calls=0`. It would have — the
    // ctor's oracle lives on the three PROBE sites, so nothing else ever calls
    // RtDiffReport here — and `calls=0` on a registered port is this project's
    // standard read for "unverified" (reference_uncalled_port_is_not_verified.md).
    // Printing the real entry count with a description that says where the
    // comparison actually happened is the honest version of that line.
    RT_DIFF_WORK(true);
    RT_DIFF_SHADOW(true,
                   "constructed (fields + buffers + bounds)",
                   "NOT replayed here; compared once on the [PROBE:*] sites below");

    return self;
}

// ============================================================================
// Graphics::SunLight::~SunLight()   — 0x180108160
//
// Restore this class's own vtable, release the two image buffers through their
// slot-0 deleting destructors, tear the render buffer out of the global list
// and free it, then TAIL-CALL ~Geometry. The tail call matters: ~Geometry frees
// the `_attached` array, so skipping it leaks on every teardown
// (reference_phantom_dtor_mangling_qeaa_vs_ueaa.md is the same family).
//
// No oracle: it frees. It IS executed on both sides of the ctor probe below,
// which is what stops it being wholly unexercised code.
// ============================================================================
void Graphics_SunLight_Dtor(SunLight* self) {
    RT_DIFF_BEGIN("Graphics::SunLight::~SunLight");
    self->_base._vtable = SunLightVtable();
    DeleteImageBuffer(self->_colourTarget);
    DeleteImageBuffer(self->_depthTarget);
    DeleteRenderBuffer(self->_renderBuffer);
    RT_DIFF_SHADOW(true, "buffers released + ~Geometry", "no replay: this body frees");
    // Retail TAIL-JUMPS here (0x1801081db JMP 0x1800a41c0), so whatever ~Geometry
    // leaves in RAX is ~SunLight's result. Both are `UEAA@XZ`, i.e. void, and the
    // only caller is the vtable's slot-0 deleting destructor, which discards it —
    // so a plain call and a void return are faithful. Spelled out because the
    // NewImageBuffer note above is the same hazard read the other way round.
    GeometryDtor()(self);
}

// ============================================================================
// THE CTOR PROBE — ours on a scratch object vs theirs on a scratch object
// ============================================================================
//
// WHY A REPLAY IS IMPOSSIBLE AND A PROBE IS NOT. Running the shipped ctor over
// a live SunLight would overwrite `_depthTarget` and `_renderBuffer` with a
// second set, orphaning the first — 2048x2048 of GPU memory leaked and a stale
// entry left on `_firstRenderBuffer`. Lane F declined the ctor on exactly this
// ("no oracle exists and constructing one is not free"). It is not free, but it
// is affordable, because the SUBJECT can be synthetic
// (reference_synthetic_subject_beats_a_fake.md): both sides run on their own
// zeroed 0x108 scratch, and each side's construction is torn down by the
// matching destructor before the next step.
//
// WHAT IS COMPARED, and what is deliberately NOT:
//
//   compared  +0x00..+0xBF   Geometry's own ctor output, INCLUDING the bounds
//                            the SetBounds tail rewrites and _dirtyFlags
//   compared  +0xC0..+0xEB   the eleven scalar defaults
//   compared  +0xF0/F8/100   NULL-vs-non-null only (the addresses differ by
//                            construction — two separate allocations)
//   compared  the DEPTH TARGET's ten scalar fields (+0x20..+0x44): width,
//             height, dimension, kind, format, flags, addressU/V/W, filter.
//             This is what actually verifies the eleven-argument
//             `ImageBuffer::Create` tuple, which is the most error-prone thing
//             this port transcribes. Every one of those ten is written either
//             by ImageBuffer's ctor or by Create, so none of them is an
//             unwritten byte (reference_oracle_must_not_compare_unwritten_bytes.md).
//   compared  `_firstRenderBuffer` before and after each side's teardown
//
//   EXCLUDED  +0xEC  — never written by retail; comparing it would compare
//                      whatever the allocator left there.
//   EXCLUDED  +0x00  the vtable slot. Both sides stamp the SAME exported
//                      address, so it is compared; it is listed here only
//                      because the two Light/SunLight stores land in one slot
//                      and only the second survives.
//   EXCLUDED  the three pointer VALUES, and the colour target entirely (it is
//             null on both sides while slot 6 answers false; when it is null
//             there is nothing to compare and the probe says so).
//
// COST. Once per process, only under BANISHED_RT_DIFF, and it is the cost of
// ONE extra 2048x2048 depth target for the lifetime of the probe (not for the
// process: both scratch objects are destroyed before the probe returns).
// BANISHED_RT_SUNLIGHT_PROBE=0 disables it; =neg is the NEGATIVE CONTROL, which
// perturbs one of OUR bytes so a run can show the probe going RED — without
// that, a green result cannot be told from a probe that compares nothing
// (reference_probe_needs_an_effectiveness_control.md).
// ============================================================================
namespace {

bool SunProbeEnabled() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_RT_SUNLIGHT_PROBE");
        s = (e && (e[0] == '0' || std::strcmp(e, "false") == 0)) ? 0 : 1;
    }
    return s != 0;
}
bool SunProbeNegative() {
    static int s = -1;
    if (s < 0) {
        const char* e = std::getenv("BANISHED_RT_SUNLIGHT_PROBE");
        s = (e && std::strcmp(e, "neg") == 0) ? 1 : 0;
    }
    return s != 0;
}

// The probe's three comparison sites. At NAMESPACE scope, not function-local,
// for a reason worth recording: a function-local `static RtDiffSite s = { "x" };`
// puts a brace PAIR on one line inside the function, and tools/rawdoor_audit.py's
// `diff_only_fn_depth` is cleared by ANY `}` that brings the brace depth back to
// the function body's own level — including the closing brace of a one-line
// aggregate initialiser. That made the audit report this probe's two
// RT_ORIGINAL_ONCE calls as UNGUARDED-CALL even with `if (!RtDiffEnabled())
// return;` as the function's first statement. Namespace scope is better code
// anyway (it is what buffers.cpp's probe does), but the audit's blind spot is
// real and is written up in WAVE9_LANE_GFX2.md rather than silently worked around.
RtDiffSite siteFields = { "Graphics::SunLight::SunLight[PROBE:object fields, ours vs shipped]" };
RtDiffSite siteImage  = { "Graphics::SunLight::SunLight[PROBE:depth-target Create tuple, ours vs shipped]" };
RtDiffSite siteList   = { "Graphics::SunLight::SunLight[PROBE:_firstRenderBuffer restored by each dtor]" };

// The ten ImageBuffer scalar fields Create's argument tuple lands in.
// runtime/types/video/ImageBuffer.h pins every one of them.
struct ImageScalars {
    int32_t  width, height, dimension, kind;
    uint32_t format, flags;
    int32_t  addressU, addressV, addressW, filter;
};
ImageScalars ReadImageScalars(const void* ib) {
    ImageScalars s{};
    if (ib == nullptr) return s;
    const uint8_t* p = static_cast<const uint8_t*>(ib);
    std::memcpy(&s.width,     p + 0x20, 4);
    std::memcpy(&s.height,    p + 0x24, 4);
    std::memcpy(&s.dimension, p + 0x28, 4);
    std::memcpy(&s.kind,      p + 0x2C, 4);
    std::memcpy(&s.format,    p + 0x30, 4);
    std::memcpy(&s.flags,     p + 0x34, 4);
    std::memcpy(&s.addressU,  p + 0x38, 4);
    std::memcpy(&s.addressV,  p + 0x3C, 4);
    std::memcpy(&s.addressW,  p + 0x40, 4);
    std::memcpy(&s.filter,    p + 0x44, 4);
    return s;
}

}  // namespace

void RtGraphicsSunLightProbeCtor() {
    // DEFENCE IN DEPTH, and it is what rawdoor_audit.py reads to see that the
    // two RT_ORIGINAL_ONCE calls below are diff-only. The ctor already gates the
    // call on `rt_diff_on__`; this makes the whole body unreachable without the
    // oracle from INSIDE, so a future caller cannot turn it into a live call
    // into a DLL that is not there.
    if (!RtDiffEnabled()) return;
    static bool done = false;
    if (done || !SunProbeEnabled())
        return;
    done = true;   // BEFORE the work: the probe calls our own ctor.

    RtDiffRegister(&siteFields);
    RtDiffRegister(&siteImage);
    RtDiffRegister(&siteList);

    void** head = reinterpret_cast<void**>(
        ResolveExport("?_firstRenderBuffer@RenderBuffer@Video@@0PEAV12@EA"));
    void* const headAtEntry = (head != nullptr) ? *head : nullptr;

    alignas(16) uint8_t oursNode[0x108];
    alignas(16) uint8_t theirsNode[0x108];

    using CtorFn = void* (*)(void*);
    using DtorFn = void  (*)(void*);

    // ---- OURS ------------------------------------------------------------
    std::memset(oursNode, 0, sizeof(oursNode));
    Graphics_SunLight_Ctor(reinterpret_cast<SunLight*>(oursNode));
    uint8_t oursCopy[0x108];
    std::memcpy(oursCopy, oursNode, sizeof(oursCopy));
    ImageScalars oursImg =
        ReadImageScalars(reinterpret_cast<SunLight*>(oursNode)->_depthTarget);
    Graphics_SunLight_Dtor(reinterpret_cast<SunLight*>(oursNode));
    void* headAfterOurs = (head != nullptr) ? *head : nullptr;
    // NEGATIVE CONTROL, part 2. The first version perturbed only the object-field
    // copy, and the run proved it: [PROBE:object fields] went RED while
    // [PROBE:depth-target Create tuple] and [PROBE:_firstRenderBuffer restored]
    // stayed green — i.e. two of the four probes were still indistinguishable
    // from probes that compare nothing. Perturbing all three inputs is what makes
    // the control cover every site it is supposed to cover.
    // Braceless on purpose: see the note on the three RtDiffSite objects above —
    // ANY brace pair that returns to the function body's depth clears
    // rawdoor_audit.py's `diff_only_fn_depth` and re-flags this function's two
    // RT_ORIGINAL_ONCE calls as UNGUARDED-CALL. Writing this as a `{ ... }` block
    // did exactly that, and it is worth one comment to stop the next edit
    // reintroducing it.
    if (SunProbeNegative()) oursImg.width += 1;
    if (SunProbeNegative()) headAfterOurs = static_cast<uint8_t*>(headAfterOurs) + 8;

    // ---- THEIRS ----------------------------------------------------------
    std::memset(theirsNode, 0, sizeof(theirsNode));
    RT_ORIGINAL_ONCE(CtorFn, "??0SunLight@Graphics@@QEAA@XZ")(theirsNode);
    uint8_t theirsCopy[0x108];
    std::memcpy(theirsCopy, theirsNode, sizeof(theirsCopy));
    const ImageScalars theirsImg =
        ReadImageScalars(reinterpret_cast<SunLight*>(theirsNode)->_depthTarget);
    RT_ORIGINAL_ONCE(DtorFn, "??1SunLight@Graphics@@UEAA@XZ")(theirsNode);
    void* const headAfterTheirs = (head != nullptr) ? *head : nullptr;

    // ---- compare: object fields -----------------------------------------
    // Blank the three pointer VALUES to their null/non-null verdict, and blank
    // +0xEC, which neither side writes.
    for (uint8_t* n : { oursCopy, theirsCopy }) {
        for (size_t off : { size_t(0xF0), size_t(0xF8), size_t(0x100) }) {
            void* v = nullptr;
            std::memcpy(&v, n + off, sizeof(v));
            const uint64_t verdict = (v != nullptr) ? 1u : 0u;
            std::memcpy(n + off, &verdict, sizeof(verdict));
        }
        const uint32_t zero = 0;
        std::memcpy(n + 0xEC, &zero, 4);
    }
    if (SunProbeNegative())
        oursCopy[0xC8] ^= 0x01;   // perturb _dirZ's low byte

    {
        const bool eq = std::memcmp(oursCopy, theirsCopy, sizeof(oursCopy)) == 0;
        char a[160], b[160];
        std::snprintf(a, sizeof(a),
                      "ours dirZ=%g sunA=%g ambA=%g matFlags=0x%x dirty=0x%x colour=%s depth=%s rb=%s",
                      *reinterpret_cast<float*>(oursCopy + 0xC8),
                      *reinterpret_cast<float*>(oursCopy + 0xD8),
                      *reinterpret_cast<float*>(oursCopy + 0xE8),
                      *reinterpret_cast<uint32_t*>(oursCopy + 0x30),
                      *reinterpret_cast<uint32_t*>(oursCopy + 0x2C),
                      oursCopy[0xF0] ? "set" : "null",
                      oursCopy[0xF8] ? "set" : "null",
                      oursCopy[0x100] ? "set" : "null");
        std::snprintf(b, sizeof(b),
                      "theirs dirZ=%g sunA=%g ambA=%g matFlags=0x%x dirty=0x%x colour=%s depth=%s rb=%s",
                      *reinterpret_cast<float*>(theirsCopy + 0xC8),
                      *reinterpret_cast<float*>(theirsCopy + 0xD8),
                      *reinterpret_cast<float*>(theirsCopy + 0xE8),
                      *reinterpret_cast<uint32_t*>(theirsCopy + 0x30),
                      *reinterpret_cast<uint32_t*>(theirsCopy + 0x2C),
                      theirsCopy[0xF0] ? "set" : "null",
                      theirsCopy[0xF8] ? "set" : "null",
                      theirsCopy[0x100] ? "set" : "null");
        RtDiffReport(&siteFields, eq, a, b);
    }

    // ---- compare: the depth target's Create tuple ------------------------
    {
        const bool eq = std::memcmp(&oursImg, &theirsImg, sizeof(ImageScalars)) == 0;
        char a[192], b[192];
        std::snprintf(a, sizeof(a),
                      "ours %dx%d dim=%d kind=%d fmt=%u flags=0x%x uvw=%d/%d/%d filt=%d",
                      oursImg.width, oursImg.height, oursImg.dimension, oursImg.kind,
                      oursImg.format, oursImg.flags, oursImg.addressU, oursImg.addressV,
                      oursImg.addressW, oursImg.filter);
        std::snprintf(b, sizeof(b),
                      "theirs %dx%d dim=%d kind=%d fmt=%u flags=0x%x uvw=%d/%d/%d filt=%d",
                      theirsImg.width, theirsImg.height, theirsImg.dimension, theirsImg.kind,
                      theirsImg.format, theirsImg.flags, theirsImg.addressU, theirsImg.addressV,
                      theirsImg.addressW, theirsImg.filter);
        RtDiffReport(&siteImage, eq, a, b);
    }

    // ---- compare: the global list is left exactly as it was --------------
    {
        const bool eq = headAfterOurs == headAtEntry && headAfterTheirs == headAtEntry;
        char a[96], b[96];
        std::snprintf(a, sizeof(a), "entry=%p afterOurs=%p", headAtEntry, headAfterOurs);
        std::snprintf(b, sizeof(b), "entry=%p afterTheirs=%p", headAtEntry, headAfterTheirs);
        RtDiffReport(&siteList, eq, a, b);
    }
}

// ============================================================================
// Registration
// ============================================================================
int RtGraphicsSunLightRegister() {
    int n = 0;

    RegisterRuntimeExport("??0SunLight@Graphics@@QEAA@XZ", (void*)&Graphics_SunLight_Ctor);
    RegisterRuntimeAddr(0x1801081f0, (void*)&Graphics_SunLight_Ctor);
    n++;

    RegisterRuntimeExport("?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z",
                          (void*)&Graphics_SunLight_Set);
    RegisterRuntimeAddr(0x180107f10, (void*)&Graphics_SunLight_Set);
    n++;

    RegisterRuntimeExport("??1SunLight@Graphics@@UEAA@XZ", (void*)&Graphics_SunLight_Dtor);
    RegisterRuntimeAddr(0x180108160, (void*)&Graphics_SunLight_Dtor);
    n++;

    printf("[RtPort] Graphics::SunLight (lane GFX2): %d exports registered\n", n);
    return n;
}
