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
// name, so a name-keyed residue scan never recorded it. At cutover that
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
#include "../../types/graphics/SunLight.h"
#include "../../types/video/Device.h"      // RtVideo::Device, RtVideo::RenderBuffer

#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <initializer_list>

using RtGraphics::SunLight;

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
// ENDGAME B.3.5 caller batch: `Create`, `PlatformConstruct` and `PlatformDestruct`
// are DEFINED members of the real `Video::RenderBuffer` (device.cpp / buffers.cpp),
// so the three by-name doors become member calls. They were spelled as
// FUNCTION-POINTER GETTERS (`RenderBufferCreate(rb, …)`); a member call has no
// pointer to hand back, so each is now an ordinary wrapper taking the receiver and
// the five call sites lost their extra `()`. `RenderBufferCreateFn` went with them.
bool RenderBufferCreate(void* rb, const void* colour, const void* depth) {
    return reinterpret_cast<Video::RenderBuffer*>(rb)->Create(
        reinterpret_cast<const Video::ImageBuffer*>(colour),
        reinterpret_cast<const Video::ImageBuffer*>(depth));           // B.3.5 bridge
}
void RenderBufferPlatformConstructPort(void* rb) {
    reinterpret_cast<Video::RenderBuffer*>(rb)->PlatformConstruct();   // B.3.5 bridge
}
void RenderBufferPlatformDestructPort(void* rb) {
    reinterpret_cast<Video::RenderBuffer*>(rb)->PlatformDestruct();    // B.3.5 bridge
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
    RenderBufferPlatformConstructPort(rb);
    return rb;
}

// `if (rb) { RenderBuffer::PlatformDestruct(rb); operator delete(rb); }`
void DeleteRenderBuffer(void* rb) {
    if (rb == nullptr)
        return;
    RenderBufferPlatformDestructPort(rb);
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
// ENDGAME B.3.5 block E: BODY MOVED onto `Graphics::SunLight::Set` below. The
// door keeps its name, its address and its registration. Its `const float*`
// parameters are the retail ABI — `const Math::Vector3&` and two
// `const Math::Vector4&` arrive as pointers — and the member spells them as the
// references the mangled name says they are.
void Graphics_SunLight_Set(SunLight* self, const float* dir,
                           const float* sun, const float* ambient) {
    reinterpret_cast<::Graphics::SunLight*>(self)
        ->::Graphics::SunLight::Set(*reinterpret_cast<const Math::Vector3*>(dir),
                                    *reinterpret_cast<const Math::Vector4*>(sun),
                                    *reinterpret_cast<const Math::Vector4*>(ambient));
}

namespace Graphics {

// ============================================================================
// Graphics::SunLight::Set(const Math::Vector3&, const Math::Vector4&,
//                         const Math::Vector4&)          retail 0x180107f10
//
// ELEVEN DWORD COPIES, then two rebuild branches this backend never takes. The
// copies are `MOV EAX,[src] / MOV [this+off],EAX` in the shipped body — no float
// arithmetic at all, so there is no association order to get wrong.
// ============================================================================
void SunLight::Set(const Math::Vector3& direction, const Math::Vector4& sun,
                   const Math::Vector4& ambient) {
    const float* const dir     = &direction.x;
    const float* const sunP    = &sun.x;
    const float* const ambientP = &ambient.x;

    // ---- the eleven copies, in the shipped order -------------------------
    _dirX     = dir[0];
    _dirY     = dir[1];
    _dirZ     = dir[2];
    _sunR     = sunP[0];
    _sunG     = sunP[1];
    _sunB     = sunP[2];
    _sunA     = sunP[3];
    _ambientR = ambientP[0];
    _ambientG = ambientP[1];
    _ambientB = ambientP[2];
    _ambientA = ambientP[3];

    // ---- the two rebuild branches ----------------------------------------
    if (DeviceSupportsColourShadowTarget() && _colourTarget == nullptr) {
        // The device grew the capability: build the colour target and a fresh
        // render buffer around it.
        DeleteRenderBuffer(_renderBuffer);
        _colourTarget = NewImageBuffer();
        ImageBufferCreate(_colourTarget, 0, 0, 5, 0x40, 0x800, 0x800, 1, 1, 1, 1, nullptr);
        _renderBuffer = NewRenderBuffer();
        RenderBufferCreate(_renderBuffer, _colourTarget, _depthTarget);
    } else if (!DeviceSupportsColourShadowTarget() && _colourTarget != nullptr) {
        // The device lost it: drop the colour target and rebuild depth-only.
        DeleteRenderBuffer(_renderBuffer);
        DeleteImageBuffer(_colourTarget);
        _colourTarget = nullptr;
        _renderBuffer = NewRenderBuffer();
        RenderBufferCreate(_renderBuffer, _colourTarget, _depthTarget);
    }
    // A rebuild call verifies NOTHING against an oracle here, and the old
    // `float pre[11]` / `bool rebuilt` locals that said so were dead: both were
    // written and never read, and both were -Wunused warnings in the perf build.
    // The refusal is stated in the banner above the door instead.
}

}  // namespace Graphics

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


    if (DeviceSupportsColourShadowTarget()) {
        self->_colourTarget = NewImageBuffer();
        ImageBufferCreate(self->_colourTarget, 0, 0, 5, 0x40, 0x800, 0x800, 1, 1, 1, 1, nullptr);
    }

    // Unconditional — the join label 0x180108325 is BELOW the capability test's
    // JZ target, so the depth target and the render buffer are built whether or
    // not the colour target was.
    self->_depthTarget = NewImageBuffer();
    ImageBufferCreate(self->_depthTarget, 0, 1, 7, 0x40, 0x800, 0x800, 1, 1, 1, 1, nullptr);

    self->_renderBuffer = NewRenderBuffer();
    RenderBufferCreate(self->_renderBuffer, self->_colourTarget, self->_depthTarget);

    // Inlined Geometry::SetBounds.
    auto& b = self->_base._localBounds;
    const bool same = b.min.x == kBoundsMin && b.min.y == kBoundsMin && b.min.z == kBoundsMin &&
                      b.max.x == kBoundsMax && b.max.y == kBoundsMax && b.max.z == kBoundsMax;
    if (!same) {
        b.min.x = kBoundsMin; b.min.y = kBoundsMin; b.min.z = kBoundsMin;
        b.max.x = kBoundsMax; b.max.y = kBoundsMax; b.max.z = kBoundsMax;
        self->_base._dirtyFlags |= 2u;
    }

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
    self->_base._vtable = SunLightVtable();
    DeleteImageBuffer(self->_colourTarget);
    DeleteImageBuffer(self->_depthTarget);
    DeleteRenderBuffer(self->_renderBuffer);
    // Retail TAIL-JUMPS here (0x1801081db JMP 0x1800a41c0), so whatever ~Geometry
    // leaves in RAX is ~SunLight's result. Both are `UEAA@XZ`, i.e. void, and the
    // only caller is the vtable's slot-0 deleting destructor, which discards it —
    // so a plain call and a void return are faithful. Spelled out because the
    // NewImageBuffer note above is the same hazard read the other way round.
    GeometryDtor()(self);
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
