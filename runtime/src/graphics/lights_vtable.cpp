// ============================================================================
// runtime/src/graphics/lights_vtable.cpp
// ??_7AmbientLight@Graphics@@6B@ (0x180158cd0), ??_7SpotLight@ (0x180158db0)
// and the two remaining ??_7SunLight@ slots.  Vtable batch 6.
// ============================================================================
//
//   AmbientLight  slot 1 Update 0x1800fa7a0   slot 2 Draw          0x1800fa790
//                 slot 3 AddDrawItems 0x1800fa890
//                 slot 9 DrawShadowMap 0x1800fa880
//                 slot10 Draw          0x1800fa7b0
//                 ??0AmbientLight 0x1800fa940, ??1AmbientLight 0x180058f30
//   SpotLight     slot 1 Update 0x180105950   slot 2 Draw          0x180105940
//                 slot 3 AddDrawItems 0x180105e30
//                 slot 9 DrawShadowMap 0x180105d00
//                 slot10 Draw          0x180105960
//                 ??0SpotLight 0x180106690, ??1SpotLight 0x180106270
//   SunLight      slot 9 DrawShadowMap 0x180106bc0
//                 slot10 Draw          0x1801067b0
//                 (ctor/dtor/Set already ported in sunlight.cpp)
//
//   slot 0 of AmbientLight and SpotLight are ROWS in
//   runtime/src/object/deleting_dtor.cpp, not bodies here.
//
// ---------------------------------------------------------------------------
// FOUR THINGS THAT WOULD BE WRONG IF READ FROM THE DECOMPILER
// ---------------------------------------------------------------------------
// 1. `Renderer::DrawItemList` TAKES FIVE VALUES (this + four params), so its
//    last int travels on the STACK at [RSP+0x20]. Three of the five bodies
//    below reach it by a TAIL JMP and pass that int by WRITING THEIR OWN arg5
//    HOME SLOT first (`MOV [RSP+0xa0],EAX`), and the two DrawShadowMaps pass
//    their own arg5 THROUGH UNTOUCHED — the slot is already correct, so retail
//    writes nothing. Both idioms are invisible in C output and both are spelled
//    out as explicit arguments below.
// 2. `??1AmbientLight@Graphics@@UEAA@XZ` (0x180058f30) IS A FIVE-INSTRUCTION
//    THUNK that tail-jumps `??1Geometry@Graphics@@UEAA@XZ` (0x1800a41c0): the
//    linker folded the two, and 0x180059080's SCALAR arm calls ~Geometry
//    DIRECTLY while its VECTOR arm names the thunk. AmbientLight therefore has
//    no destructor logic of its own, and the body below is the thunk.
// 3. THE SHADOW-BIAS MATRICES ARE BUILT FROM IMMEDIATES, four `MOVAPS` rows at
//    a time out of overlapping 8- and 4-byte stores. SunLight has TWO of them,
//    chosen by a virtual query on `Video::Device::_singleton->[0x100]`;
//    SpotLight has only the first, unconditionally. Getting the sign of the
//    [1][1] term wrong flips every shadow vertically.
// 4. `Video::ImageBuffer::Width()` / `Height()` ARE INLINED at six sites, as a
//    three-way branch on the image's flags word that reads the Video::Device
//    singleton. `ImageDim` below is that inlining, spelled once.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "../../funcs/rt_port.h"
#include "../../types/math/Math.h"
// ENDGAME B.3.5 block E: the real `Graphics::Light` / `AmbientLight` /
// `SpotLight` / `SunLight`. `Light.h` is AUTHORED FROM THE BODIES IN THIS FILE —
// every offset in the "Layout constants" block below is one of its field pins,
// and the header cites this file as its first source.
#include "../../types/graphics/Light.h"
#include "../../types/graphics/SunLight.h"
// ENDGAME B.3.5 caller batch: the cross-class edges this file opens. Every one
// of them is a real class today, so the by-name doors below become member calls.
#include "../../types/graphics/Camera.h"    // Graphics::Renderer, Graphics::Camera
#include "../../types/graphics/Scene.h"     // Graphics::Scene
#include "../../types/video/Context.h"      // Video::Context
#include "../../types/video/Device.h"       // Video::RenderBuffer
#include "../../types/video/ImageBuffer.h"  // Video::ImageBuffer (B3X W0: slot 0 by name)

#include "../../types/collection/Collection.h"   // rtcoll:: the array instantiations

namespace {

// ---------------------------------------------------------------------------
// Layout constants, all read from the bodies that use them.
// ---------------------------------------------------------------------------
// Graphics::Light (the shared base, 0xc0 and up) —
//   +0x30   int    the "kind" word both constructors set to 0x8000
//   +0x84   Math::Transform (0x1c), SpotLight's light-to-world
//   +0xc0   Volume::Frustum, SpotLight's projection; AmbientLight's Vector4
//
// Graphics::AmbientLight — 0xd8
//   +0xc0   Vector4  the ambient colour       (ctor: two zeroed qwords)
//   +0xd0   float    10.0f (0x41200000)
//
// Graphics::SpotLight — 0x168
//   +0xc0 .. +0x118  twelve zeroed qwords (the Frustum and what follows)
//   +0x120  bool  = 1
//   +0x128  float = 0.1f    (0x3dcccccd)
//   +0x12c  float = 10.0f   (0x41200000)
//   +0x130  float \  the two scalars Context::SetLight is handed
//   +0x134  float /
//   +0x138  Vector4  the light colour
//   +0x148  Video::ImageBuffer*   the shadow map      (owned; deleting dtor)
//   +0x150  Video::ImageBuffer*   the shadow depth    (owned; deleting dtor)
//   +0x158  Video::RenderBuffer*  the shadow target   (owned; long-form dtor)
//   +0x160  void*                 bound at Draw time into Context+0x50
//
// Graphics::SunLight — the three the two slots below touch:
//   +0xc0 / +0xcc / +0xdc   the three vectors SetLight is handed
//   +0xf8   Video::ImageBuffer*   the cascade shadow map
//   +0x100  Video::RenderBuffer*  the cascade target
//
// Video::Context — the state block the Draw arms poke:
//   +0x10   dirty mask       +0x18 / +0x1c  sub-masks
//   +0x28   render target    +0x50 / +0x58  the two bound images
//   +0x1d0  viewport (x, y, w, h)    +0x1e0  scissor (x, y, w, h)
//   +0x1e78 a 4x4 Matrix (the shadow/texture matrix)
//   +0x2160 the render-state object whose vtable slot 4 the shadow pass calls
// ---------------------------------------------------------------------------
constexpr uint32_t kCtxDirty       = 0x10;
constexpr uint32_t kCtxDirty18     = 0x18;
constexpr uint32_t kCtxTarget      = 0x28;
constexpr uint32_t kCtxImage50     = 0x50;
constexpr uint32_t kCtxImage58     = 0x58;
constexpr uint32_t kCtxViewport    = 0x1d0;
constexpr uint32_t kCtxScissor     = 0x1e0;
constexpr uint32_t kCtxShadowMtx   = 0x1e78;
constexpr uint32_t kCtxRenderState = 0x2160;

constexpr uint32_t kDrawGroupStride = 0x1f0;   // IMUL RDI,RDI,0x1f0

// The float immediates, as BIT PATTERNS. Read out of the image with
// read_memory, not converted from decimal:
//   0x18016169c = 0x3f800000 =  1.0f
//   0x1801616d8 = 0x40000000 =  2.0f
//   0x180161754 = 0xbf000000 = -0.5f
//   0x180161768 = 0xc0000000 = -2.0f
constexpr uint32_t kBits_0      = 0x00000000u;
constexpr uint32_t kBits_half   = 0x3f000000u;
constexpr uint32_t kBits_neghalf= 0xbf000000u;
constexpr uint32_t kBits_one    = 0x3f800000u;

float AsFloat(uint32_t bits) { float f; std::memcpy(&f, &bits, 4); return f; }

// ---------------------------------------------------------------------------
// Named dependencies, called by name (B3R).
// ---------------------------------------------------------------------------

// `?CreateTargets@SpotLight@Graphics@@AEAAXXZ` 0x180106050 — **PORTED**
// (runtime/src/graphics/batchc_gfx_leaves.cpp, BATCH C 2026-09-12), reached BY
// NAME because `AEAA` is PRIVATE, not unexported, and this DLL exports its
// private members (reference_aeaa_is_private_not_unexported.md).
// It allocates the shadow map / depth / render target trio the destructor then
// owns. This comment used to say "NOT PORTED ... it stays on the DLL until the
// Video side of that is ported"; the Video side it named (ImageBuffer /
// RenderBuffer operator new, ctor, PlatformConstruct and Create) was already
// ported, which is what made the port possible.
// ENDGAME B.3.5 caller batch: a DEFINED member (batchc_gfx_leaves.cpp:885).
// ENDGAME B.3.9 block GFX: the real `SpotLight()` calls it as a member; the
// file-local bridge went with the ??0 door. Its own door stays registered.

// ENDGAME B.3.5 caller batch: `Video::RenderBuffer::PlatformDestruct` is a
// DEFINED member (runtime/src/video/buffers_win.cpp).
void RenderBufferPlatformDestruct(void* p) {
    reinterpret_cast<Video::RenderBuffer*>(p)->PlatformDestruct();   // B.3.5 bridge
}
void RenderBufferOperatorDelete(void* p) {
    Video::RenderBuffer::operator delete(p);       // 0x18001f360
}

// `?SetLight@Context@Video@@QEAAXAEBVVector3@Math@@0AEBVVector4@4@1MMH@Z`
// 0x18001a950 — **PORTED** (runtime/src/video/batchc_context_setlight.cpp,
// BATCH C 2026-09-12; it used to read "NOT PORTED"), exported, reached by name.
// All five Draw arms end in it, and its EIGHT values (this + seven params) are
// what the whole of each body exists to compute.
// ENDGAME B.3.5 caller batch: a DEFINED member of the real `Video::Context`
// (runtime/src/video/batchc_context_setlight.cpp), whose declaration takes the
// same raw pointers this door did. The `SetLightFn` typedef went with the door.
void ContextSetLight(void* ctx, const float* a, const float* b,
                     const float* c, const float* d, float e, float f, int32_t g) {
    reinterpret_cast<Video::Context*>(ctx)->SetLight(a, b, c, d, e, f, g);  // B.3.5 bridge
}

// `?DrawItemList@Renderer@Graphics@@QEBAXAEAVContext@Video@@HHH@Z` 0x180105240
// — **PORTED** (runtime/src/graphics/batchc_gfx_leaves.cpp, BATCH C
// 2026-09-12; it used to read "NOT PORTED"), exported, reached by name.
// FIVE VALUES; see the banner. The fifth is the SHADER INDEX — that body is
// where the three arms' `MOV [RSP+0xa0],EAX` finally lands.
// ENDGAME B.3.5 caller batch: a DEFINED member (batchc_gfx_leaves.cpp:943).
void RendererDrawItemList(void* renderer, void* ctx, int32_t a, int32_t b, int32_t c) {
    reinterpret_cast<const Graphics::Renderer*>(renderer)
        ->DrawItemList(*reinterpret_cast<Video::Context*>(ctx), a, b, c);  // B.3.5 bridge
}

// The five Math / Graphics helpers SpotLight and SunLight's Draw use. Every one
// is already PORTED and every one is exported, so all five are name doors.
//
// THREE OF THEM RETURN A STRUCT BY VALUE FROM A NON-STATIC MEMBER, so the
// hidden return slot is **RDX** with `this` in RCX — verified against the
// disassembly at each call site rather than inferred from the mangling, which
// is the check batch 4 documented for `GetSunCamera`:
//   ?Inverse@Transform3@Math@@QEBA?AV12@XZ            0x180060040  (this, ret)
//   ?AsMatrix@Transform3@Math@@QEBA?AVMatrix@2@XZ     0x1800346e0  (this, ret)
//   ??DMatrix@Math@@QEBA?AV01@AEBV01@@Z               0x18001d980  (this, ret, rhs)
//   ?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ 0x18001fcd0 (this, ret)
// Each returns the slot in RAX, which the caller then uses as `this` for the
// next call in the chain — the shape below preserves that exactly.
// ENDGAME B.3.1 step 2: all four are members of the real Math classes. The
// `(self, ret) -> void*` SHAPE is kept because the chain below feeds each
// call's result in as the next call's `this`, on scratch storage the
// Graphics::SpotLight body owns; that shape goes when THAT class is converted.
void* Transform3Inverse(const void* self, void* ret) {
    *static_cast<Math::Transform3*>(ret) =
        static_cast<const Math::Transform3*>(self)->Inverse();
    return ret;
}
void* Transform3AsMatrix(const void* self, void* ret) {
    *static_cast<Math::Matrix*>(ret) =
        static_cast<const Math::Transform3*>(self)->AsMatrix();
    return ret;
}
void* MatrixMultiply(const void* self, void* ret, const void* rhs) {
    *static_cast<Math::Matrix*>(ret) =
        *static_cast<const Math::Matrix*>(self) *
        *static_cast<const Math::Matrix*>(rhs);
    return ret;
}
void* TransformAsTransform3(const void* self, void* ret) {
    *static_cast<Math::Transform3*>(ret) =
        static_cast<const Math::Transform*>(self)->AsTransform3();
    return ret;
}

// `??0Camera@Graphics@@QEAA@AEBVFrustum@Volume@@AEBVTransform3@Math@@_N@Z`
// 0x1800fe960 — PORTED (renderer.cpp's door RtGraphics_Camera_Ctor, by name).
// FOUR arguments; the bool is `MOV R9B,1`.
void CameraFromFrustum(void* cam, const void* frustum, const void* xform, bool flag) {
    RtGraphics_Camera_Ctor(static_cast<RtGraphics::Camera*>(cam),        // B.3.x bridge
                           static_cast<const ::Frustum*>(frustum),
                           static_cast<const Math::Transform3*>(xform), flag);
}

// The four Renderer / Scene entry points SpotLight::AddDrawItems drives. All
// PORTED, all exported (`AEBA` on the two Scene ones is private, which this DLL
// still exports).
// ENDGAME B.3.5 caller batch: all five are DEFINED members today —
// `Renderer::AddLight` / `::EndDrawGroup` and `Scene::AddGlobalDrawItems` in
// wave29_lane_gfxbody.cpp, `Renderer::BeginDrawGroup` in wave28_lane_gfxbody.cpp,
// `Scene::AddCulledDrawItems` in cull.cpp. The four function-pointer typedefs
// went with their doors; the doors themselves stay registered.
void RendererAddLight(void* renderer, const void* light) {
    reinterpret_cast<Graphics::Renderer*>(renderer)
        ->AddLight(reinterpret_cast<const Graphics::Light*>(light));   // B.3.5 bridge
}
int32_t RendererBeginDrawGroup(void* renderer, const void* target,
                               const void* camA, const void* camB,
                               int32_t a, uint32_t b) {
    return reinterpret_cast<Graphics::Renderer*>(renderer)->BeginDrawGroup(
        reinterpret_cast<const Video::RenderBuffer*>(target),
        *reinterpret_cast<const Graphics::Camera*>(camA),
        *reinterpret_cast<const Graphics::Camera*>(camB), a, b);       // B.3.5 bridge
}
void RendererEndDrawGroup(void* renderer, int32_t id) {
    reinterpret_cast<Graphics::Renderer*>(renderer)->EndDrawGroup(id);  // B.3.5 bridge
}
void SceneAddGlobalDrawItems(const void* scene, void* renderer) {
    reinterpret_cast<const Graphics::Scene*>(scene)
        ->AddGlobalDrawItems(*reinterpret_cast<Graphics::Renderer*>(renderer));  // B.3.5 bridge
}
void SceneAddCulledDrawItems(const void* scene, void* renderer) {
    reinterpret_cast<const Graphics::Scene*>(scene)
        ->AddCulledDrawItems(*reinterpret_cast<Graphics::Renderer*>(renderer));  // B.3.5 bridge
}

// The two unexported Collection::Array<T>::SetSize instantiations
// AmbientLight::AddDrawItems drives (no export name in either shipped build),
// both ported and called by body name:
//   0x180023830  Array<int>::SetSize(n, const int&)          array.cpp kSpec_Fill4
//   0x180063a80  Array<{Light*,int}>::SetSize(n, const T&)   (stride 0x10)

// ---------------------------------------------------------------------------
// The two Video singletons the dimension logic reads. Both are pointers TO the
// object, so each read is a double indirection.
// ---------------------------------------------------------------------------
// ENDGAME B.3.9 SS1 (B39_RULES D7): both class statics now, read fresh.
const uint8_t* VideoDevice() {
    return reinterpret_cast<const uint8_t*>(::Video::Device::_singleton);
}
const uint8_t* VideoConfiguration() {
    return reinterpret_cast<const uint8_t*>(::Video::Configuration::_singleton);
}

// ---------------------------------------------------------------------------
// `Video::ImageBuffer::Width()` / `Height()`, INLINED — the three-way branch on
// the image's flags word at +0x34 that appears at six sites in this file.
//
//     flags = image->[0x34];
//     if (flags & (1 << 8))  return device->[devField];          // full screen
//     if (flags & (1 << 9))  return device->[devField] / 2;       // half screen
//     v = image->[imgField];
//     if (flags & (1 << 6))  v >>= configuration->[0x2c];         // mip shift
//     return v;
//
// `devField` is 0x04 with `imgField` 0x20 for WIDTH, and 0x08 with 0x24 for
// HEIGHT — which is how runtime/types/video/ImageBuffer.h already describes
// bits 8 and 9 ("read only by Width()/Height()"), independently recovered.
//
// THE HALVING IS A SIGNED ROUND-TOWARD-ZERO DIVIDE, NOT A SHIFT:
// `CDQ; SUB EAX,EDX; SAR EAX,1`. For the non-negative values a device size can
// hold the two agree, and the transcription keeps retail's form anyway because
// "cannot be negative" is an argument about the caller, not about this code.
//
// The mip shift IS an arithmetic right shift by a variable count (`SAR EAX,CL`).
// ---------------------------------------------------------------------------
int32_t ImageDim(const void* image, uint32_t devField, uint32_t imgField) {
    const uint8_t* const ib = static_cast<const uint8_t*>(image);
    const int32_t flags = *reinterpret_cast<const int32_t*>(ib + 0x34);

    if (flags & (1 << 8))
        return *reinterpret_cast<const int32_t*>(VideoDevice() + devField);
    if (flags & (1 << 9)) {
        const int32_t v = *reinterpret_cast<const int32_t*>(VideoDevice() + devField);
        return (v - (v >> 31)) >> 1;                 // CDQ / SUB / SAR 1
    }
    int32_t v = *reinterpret_cast<const int32_t*>(ib + imgField);
    if ((flags >> 6) & 1) {
        const int32_t shift = *reinterpret_cast<const int32_t*>(VideoConfiguration() + 0x2c);
        v >>= shift;                                 // SAR EAX,CL
    }
    return v;
}
int32_t ImageWidth (const void* ib) { return ImageDim(ib, 0x04, 0x20); }
int32_t ImageHeight(const void* ib) { return ImageDim(ib, 0x08, 0x24); }

// ---------------------------------------------------------------------------
// `if (ctx->rect != {x,y,w,h}) { ctx->dirty |= bit; ctx->rect = {x,y,w,h}; }`
// The compare is FOUR separate dword compares in retail and the write is a
// qword for the first pair plus two dwords; the observable effect is identical
// and the dirty bit is only set when at least one term differs, which is the
// part that matters.
// ---------------------------------------------------------------------------
void SetRect(void* ctx, uint32_t off, int32_t x, int32_t y, int32_t w, int32_t h,
             uint32_t dirtyBit) {
    int32_t* const r = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(ctx) + off);
    if (r[0] == x && r[1] == y && r[2] == w && r[3] == h) return;
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + kCtxDirty) |= dirtyBit;
    r[0] = x; r[1] = y; r[2] = w; r[3] = h;
}

// The shadow pass's one virtual call: `obj = ctx->[0x2160];
// obj->vtable[4](obj, 6, 0, 1.0f, 0)`. FIVE values — the float travels in XMM3
// (the fourth register slot) and the trailing int at [RSP+0x20]. Ghidra prints
// this with the float and the last int dropped.
void RenderStateSlot4(void* ctx) {
    void* const obj = *reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + kCtxRenderState);
    void** const vt = *reinterpret_cast<void***>(obj);
    using Fn = void (*)(void*, int32_t, int32_t, float, int32_t);
    reinterpret_cast<Fn>(vt[4])(obj, 6, 0, AsFloat(kBits_one), 0);
}

// ---------------------------------------------------------------------------
// The shadow bias matrix. TWO variants; SpotLight always uses A, SunLight picks
// between them with a virtual query on the Device.
//
//   A (0x1800fa7... / 0x180105993 / 0x18010681f):   B (0x18010689d):
//     [ 0.5   0    0   0 ]                            [ 0.5  0    0    0 ]
//     [ 0   -0.5   0   0 ]                            [ 0    0.5  0    0 ]
//     [ 0     0    1   0 ]                            [ 0    0    0.5  0 ]
//     [ 0.5  0.5   0   1 ]                            [ 0.5  0.5  0.5  1 ]
//
// A maps a [-1,1] clip box with a top-left texture origin onto [0,1]x[0,1] and
// leaves z alone (the DX10+ [0,1] depth range); B also remaps z from [-1,1],
// which is the DX9 convention. That reading is offered as orientation only —
// the numbers are the port.
// ---------------------------------------------------------------------------
void BiasMatrixA(float* m) {
    for (int i = 0; i < 16; ++i) m[i] = AsFloat(kBits_0);
    m[0]  = AsFloat(kBits_half);
    m[5]  = AsFloat(kBits_neghalf);
    m[10] = AsFloat(kBits_one);
    m[12] = AsFloat(kBits_half);
    m[13] = AsFloat(kBits_half);
    m[15] = AsFloat(kBits_one);
}
void BiasMatrixB(float* m) {
    for (int i = 0; i < 16; ++i) m[i] = AsFloat(kBits_0);
    m[0]  = AsFloat(kBits_half);
    m[5]  = AsFloat(kBits_half);
    m[10] = AsFloat(kBits_half);
    m[12] = AsFloat(kBits_half);
    m[13] = AsFloat(kBits_half);
    m[14] = AsFloat(kBits_half);
    m[15] = AsFloat(kBits_one);
}

// ---------------------------------------------------------------------------
// The shared tail of SpotLight::Draw and SunLight::Draw: take the draw group's
// projection Matrix and its view Transform3, and write
//
//     ctx->shadowMatrix = AsMatrix(Inverse(viewTransform)) * projection * bias
//
// into Context+0x1e78, then raise the 0x2000 dirty bit.
//
// THE CHAIN'S SHAPE IS RETAIL'S, INCLUDING THE SCRATCH REUSE. Each call is
// handed the RAX the previous one returned as its `this`, and the second
// multiply writes back over the buffer the FIRST multiply read from. Written
// out with the same three scratch buffers so no intermediate is kept alive that
// retail frees.
// ---------------------------------------------------------------------------
void WriteShadowMatrix(void* ctx, const float* projection, const float* viewXform,
                       const float* bias) {
    float inverse[12];                 // Transform3, 0x30
    float asMatrix[16];                // Matrix, 0x40
    float product[16];
    float result[16];

    void* const inv = Transform3Inverse(viewXform, inverse);
    void* const mat = Transform3AsMatrix(inv, asMatrix);
    void* const p1  = MatrixMultiply(mat, product, projection);
    MatrixMultiply(p1, result, bias);

    std::memcpy(static_cast<uint8_t*>(ctx) + kCtxShadowMtx, result, sizeof result);
    *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + kCtxDirty) |= 0x2000u;
}

// `if (ctx->[off] != p) { ctx->dirty |= 2; ctx->[0x18] |= bit; ctx->[off] = p; }`
void BindImage(void* ctx, uint32_t off, void* p, uint32_t subBit) {
    void** const slot = reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + off);
    if (*slot == p) return;
    uint32_t* const c = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx));
    c[kCtxDirty / 4]   |= 2u;
    c[kCtxDirty18 / 4] |= subBit;
    *slot = p;
}

// The draw-group record the two Draw arms index: `renderer->[0x10] + i * 0x1f0`.
// 0x1f0 is the stride closure batch 2b pinned from the other end (its element
// destructor and its copy constructor).
const uint8_t* DrawGroup(const void* renderer, int32_t index) {
    const uint8_t* const data =
        *reinterpret_cast<const uint8_t* const*>(static_cast<const uint8_t*>(renderer) + 0x10);
    return data + static_cast<int64_t>(index) * kDrawGroupStride;
}

}  // namespace

// ############################################################################
// Graphics::AmbientLight
// ############################################################################

// ============================================================================
// ??0AmbientLight@Graphics@@QEAA@XZ 0x1800fa940 / ??1AmbientLight@Graphics@@UEAA@XZ
// 0x180058f30 -- the two DOORS are gone (ENDGAME B.3.9 block GFX, B39_RULES D1).
// The ctor's body is the real `AmbientLight()` (Light.h); the destructor thunk
// (five instructions, a tail jump onto ~Geometry that the linker folded) is the
// empty `~AmbientLight()` below plus the compiler's chain. Nothing in retail or
// the tree ever called ??0AmbientLight.
// ============================================================================

// ############################################################################
// ENDGAME B.3.5 block E — the Light-branch bodies, MOVED ONTO THE MEMBERS
// ############################################################################
//
// Every body below is a member of `Graphics::AmbientLight`, `Graphics::SpotLight`
// or `Graphics::SunLight`. ENDGAME B.3.9 block GFX: AmbientLight and SpotLight
// are concrete and wear the compiler's table, so their slot doors are gone;
// SunLight's two slot doors below are the pilot's (c2) to settle.
//
// RULE B5-17 IS APPLIED HERE. Retail names slot 2 and slot 10 both `Draw`. Slot
// 2 keeps the retail spelling; slot 10 is `DrawLight`. Both mangled names are
// unchanged in every registration, so nothing outside this file moves.
//
// The `Video::Context&` and `Graphics::Renderer&` parameters are the real retail
// types, forward-declared: blocks D and F own those classes, so each body takes
// the reference and reads its offsets through ONE bridge cast, which is where it
// read them before.
// ############################################################################

// slot 1 — `RET`. Empty in the shipped build; an ambient light has nothing to
// tick (reference_retail_ships_empty_bodies.md).
// slot 2 — `RET`. The per-item Draw arm; an ambient light emits no draw items,
// only the light state its slot-10 Draw sets.
// slot 9 — `RET`. An ambient light casts no shadow.
// ============================================================================
// slot 3 — ?AddDrawItems@AmbientLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
//                                                    retail 0x1800fa890
// ============================================================================
// TWO `Array<T>::SetSize(n + 1, fill)` calls and nothing else. THE SCENE
// ARGUMENT IS NEVER READ — retail keeps `this` in RBX and the renderer in RDI
// and touches RDX not once. Transcribed as such rather than "tidied" into a use.
//
//   1. `renderer->groups[renderer->[0x30]].lights` — the Array at +0x100 of the
//      0x110-byte record at `renderer->[0x00] + index * 0x110` (the SAME array
//      ReflectionImageBuffer::Render appends to, which is what pins 0x110 twice
//      over) — grows by one, filled with `renderer->[0x28]`.
//   2. `renderer->[0x20]`, the Array of {Light*, int} pairs, grows to
//      `renderer->[0x28] + 1`, filled with `{ this, renderer->[0x38] }`.
//
// The two counts come from DIFFERENT places: the first is the target array's
// own `_size`, the second is `renderer->[0x28]`. Swapping them would be
// invisible at a single light and wrong at two.
// ============================================================================
namespace Graphics {

// `??1Light@Graphics@@UEAA@XZ` 0x180058d20 is a bare tail into ~Geometry with
// NO restamp of its own. ENDGAME B.3.8 block RB defines the C++ destructor
// EMPTY so the real subclass chains link; the compiler adds a Light restamp
// retail does not make, between two restamps, with nothing dispatching in
// between. (ENDGAME B.3.9 block GFX: its door `RtGraphics_Light_Dtor` and the
// Light RT_DELDTOR row are retired with the minted `??_7Light@` -- every Light
// subclass is concrete and the compiler stamps the Light table in passing.)
Light::~Light() {}

// ??1AmbientLight@Graphics@@UEAA@XZ 0x180058f30 -- five instructions, four of
// them the MSVC unwind marker, then `JMP ??1Geometry`: AmbientLight adds no
// destructible member and the linker folded its destructor onto ~Geometry. The
// empty body plus the compiler's ~Light / ~Geometry chain is all of it.
AmbientLight::~AmbientLight() {}

void AmbientLight::Update(const System::Time& time) { (void)time; }

void AmbientLight::Draw(Video::Context& context, const Camera& camera,
                        ExternalPtrHandle material, float opacity,
                        int a, int b, int c) const {
    (void)context; (void)camera; (void)material; (void)opacity;
    (void)a; (void)b; (void)c;
}

void AmbientLight::DrawShadowMap(Video::Context& context, Renderer& renderer,
                                 int a, int b) const {
    (void)context; (void)renderer; (void)a; (void)b;
}

void AmbientLight::AddDrawItems(const Scene& scene, Renderer& renderer) const {
    (void)scene;                                   // retail never touches RDX
    uint8_t* const r = reinterpret_cast<uint8_t*>(&renderer);     // B.3.5 bridge
    // Retail keeps `this` in RBX and stores it into the {Light*, int} pair; the
    // slot is a non-const `Light*` in the array element, so the const is cast
    // away exactly where retail's `UEBA` (const member) does the same store.
    void* const self = const_cast<AmbientLight*>(this);

    const int32_t groupIndex = *reinterpret_cast<const int32_t*>(r + 0x30);
    const int32_t lightId    = *reinterpret_cast<const int32_t*>(r + 0x28);

    uint8_t* const groups = *reinterpret_cast<uint8_t**>(r);
    uint8_t* const lights = groups + static_cast<int64_t>(groupIndex) * 0x110 + 0x100;

    int32_t fill = lightId;                        // MOV [RSP+0x40],EAX
    rtcoll::RtColl_ArraySetSizeFill_180023830(lights,
                    *reinterpret_cast<const int32_t*>(lights + 8) + 1, &fill);

    struct { void* light; int32_t tag; int32_t _pad; } pair;
    pair.light = self;                             // MOV [RSP+0x20],RBX
    pair.tag   = *reinterpret_cast<const int32_t*>(r + 0x38);
    pair._pad  = 0;
    rtcoll::RtColl_ArraySetSizeFill_180063a80(r + 0x20, lightId + 1, &pair);
}

// ============================================================================
// slot 10 — ?Draw@AmbientLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z
//                                                    retail 0x1800fa7b0
// ============================================================================
// Set the ambient light state on the Context, then tail-jump DrawItemList.
//
// SIX of `SetLight`'s seven parameters are CONSTANTS here — two zero Vector3s,
// a zero Vector4, 0.0f, and the int 1 — and only two come from the object: the
// colour at +0xc0 and the scalar at +0xd0. The three zero locals are built
// exactly as retail builds them (a qword plus a dword each for the Vector3s,
// two qwords for the Vector4) rather than as `= {}`, because the WIDTH of each
// zero store is the evidence for the size of each parameter.
//
// THE TAIL: `DrawItemList(renderer, ctx, arg4, arg6, arg7)`. arg5 is NOT
// forwarded — retail OVERWRITES its own arg5 home slot with arg7 to make the
// stack argument land, which is the idiom the banner describes.
// ============================================================================
void AmbientLight::DrawLight(Video::Context& context, Renderer& renderer,
                             int a4, int a5, int a6, int a7) const {
    (void)a5;                                      // consumed only as the ABI slot
    void* const ctx = reinterpret_cast<void*>(&context);          // B.3.5 bridge

    float zeroV3a[3] = { 0.0f, 0.0f, 0.0f };
    float zeroV3b[3] = { 0.0f, 0.0f, 0.0f };
    float zeroV4 [4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    // The two values that come from the object are now named members: the
    // Vector4 colour at +0xc0 and the 10.0f intensity at +0xd0.
    ContextSetLight(ctx, zeroV3a, zeroV3b, zeroV4,
                    reinterpret_cast<const float*>(&_colour),
                    0.0f,
                    _intensity,
                    1);

    RendererDrawItemList(&renderer, ctx, a4, a6, a7);
}

}  // namespace Graphics

// ############################################################################
// Graphics::SpotLight
// ############################################################################

// ============================================================================
// ??0SpotLight@Graphics@@QEAA@XZ                     retail 0x180106690
//
// Geometry ctor, the two vptr stamps, TWELVE zeroed qwords from +0xc0 to
// +0x118, the three literals, five more zeroed qwords, `CreateTargets()`, and
// only THEN `[+0x30] = 0x8000` — the kind word is written AFTER the call, not
// before, and the order is transcribed because CreateTargets reads the object.
//
// +0x158 is NOT zeroed by the ctor (the run of stores skips from +0x150 to
// +0x160). CreateTargets fills it. Recorded, not helpfully zeroed.
//
// ENDGAME B.3.9 block GFX: that body IS the real `SpotLight()` (Light.h); the
// door that stamped the minted tables is gone.
// ============================================================================
namespace Graphics {

// ============================================================================
// ??1SpotLight@Graphics@@UEAA@XZ                     retail 0x180106270
//
// Restamp, then the three owned resources in retail's order, then tail-jump
// ~Geometry. TWO DESTROY IDIOMS again: the two ImageBuffers go through their
// own vtable slot 0 with flags = 1 (destroy AND free), the RenderBuffer goes
// the long way because it has no virtual destructor.
//
// ENDGAME B.3.9 block GFX: the body moved here from the retired ??1 door. The
// restamp and the tail into ~Light (empty) / ~Geometry are the compiler's.
// ============================================================================
SpotLight::~SpotLight() {
    void* const* const owned[2] = { &_shadowMap, &_shadowDepth };   // +0x148, +0x150
    for (void* const* slot : owned) {
        void* const img = *slot;
        if (img == nullptr) continue;
        // Slot 0, flags 1, by name on the image's own class (B3X W0).
        delete static_cast<Video::ImageBuffer*>(img);   // B.3.x bridge
    }
    void* const rb = _shadowTarget;                          // +0x158
    if (rb != nullptr) {
        RenderBufferPlatformDestruct(rb);
        RenderBufferOperatorDelete(rb);
    }
}

// slot 1 / slot 2 — both `RET`, like AmbientLight's.
void SpotLight::Update(const System::Time& time) { (void)time; }

void SpotLight::Draw(Video::Context& context, const Camera& camera,
                     ExternalPtrHandle material, float opacity,
                     int a, int b, int c) const {
    (void)context; (void)camera; (void)material; (void)opacity;
    (void)a; (void)b; (void)c;
}

}  // namespace Graphics

// ============================================================================
// slot 3 — ?AddDrawItems@SpotLight@Graphics@@UEBAXAEBVScene@2@AEAVRenderer@2@@Z
//                                                    retail 0x180105e30
// ============================================================================
// Queue TWO draw groups: the shadow-map pass into the light's own render
// target, then a second pass with a null target.
//
// THE WHOLE BODY IS BEHIND `_shadowMap && _shadowDepth` (+0x148 and +0x150,
// both non-null). A spot light whose targets failed to allocate contributes
// nothing rather than crashing, and that is retail's own guard, not one added
// here.
//
// The camera is built from the light's Transform at +0x84 (through
// `Transform::AsTransform3`) and its Frustum at +0xc0, with the bool argument
// `MOV R9B,1`. It is passed as BOTH camera arguments of the first
// BeginDrawGroup — retail really does put the same pointer in R8 and R9.
//
// The second BeginDrawGroup's first camera is `renderer->[0x10] + 8`, i.e. the
// Camera embedded at +0x08 of draw group ZERO — the main view's camera — while
// the second stays the light's. Its flags are 3/3 where the first pass used
// 1/1, and its render target is NULL (`XOR EDX,EDX`).
// ============================================================================
namespace Graphics {

void SpotLight::AddDrawItems(const Scene& sceneRef, Renderer& rendererRef) const {
    // Bridges, at the one place the foreign objects' offsets are read.
    const void* const scene = reinterpret_cast<const void*>(&sceneRef);      // B.3.5 bridge
    void* const renderer    = reinterpret_cast<void*>(&rendererRef);         // B.3.5 bridge

    // THE WHOLE BODY IS BEHIND `_shadowMap && _shadowDepth` — retail's own
    // guard, not one added here. A spot light whose targets failed to allocate
    // contributes nothing rather than crashing.
    if (_shadowMap == nullptr) return;
    if (_shadowDepth == nullptr) return;

    float xform[7];                                 // Math::Transform, 0x1c
    std::memcpy(xform, &_transform, sizeof xform);
    float asT3[12];                                 // Math::Transform3, 0x30
    void* const t3 = TransformAsTransform3(xform, asT3);

    alignas(8) uint8_t camera[0xd8];
    // `_frustum` is the `Volume::Frustum` at +0xc0 this class's header pins; it
    // is handed over as `const Frustum&`, which is the independent corroboration
    // of that field (Light.h's third source).
    CameraFromFrustum(camera, &_frustum, t3, true);  // MOV R9B,1

    RendererAddLight(renderer, this);

    int32_t id = RendererBeginDrawGroup(renderer, _shadowTarget,
                                        camera, camera, 1, 1u);
    SceneAddGlobalDrawItems(scene, renderer);
    if (*reinterpret_cast<const int32_t*>(static_cast<uint8_t*>(renderer) + 0x48) != 0)
        SceneAddCulledDrawItems(scene, renderer);
    RendererEndDrawGroup(renderer, id);

    const uint8_t* const group0 =
        *reinterpret_cast<uint8_t* const*>(static_cast<uint8_t*>(renderer) + 0x10);
    id = RendererBeginDrawGroup(renderer, nullptr, group0 + 8, camera, 3, 3u);
    SceneAddGlobalDrawItems(scene, renderer);
    if (*reinterpret_cast<const int32_t*>(static_cast<uint8_t*>(renderer) + 0x48) != 0)
        SceneAddCulledDrawItems(scene, renderer);
    RendererEndDrawGroup(renderer, id);
}

}  // namespace Graphics

// ============================================================================
// slot 9 — ?DrawShadowMap@SpotLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z
//                                                    retail 0x180105d00
// ============================================================================
// Bind the light's render target, set the viewport to the shadow map's own
// size, kick the render state, and hand the list to the renderer.
//
// THE FIFTH VALUE OF THE TAIL `DrawItemList` IS THIS FUNCTION'S OWN arg5,
// PASSED THROUGH UNTOUCHED: retail tail-jumps without writing its arg5 home
// slot, so the callee reads the very int this function was given. That is not
// an omission, it is the calling convention being used as a pass-through, and
// it is spelled as an explicit argument here.
// ============================================================================
namespace Graphics {

void SpotLight::DrawShadowMap(Video::Context& context, Renderer& renderer,
                              int a4, int a5) const {
    void* const ctx = reinterpret_cast<void*>(&context);          // B.3.5 bridge

    void* const target = _shadowTarget;
    void** const slot = reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + kCtxTarget);
    if (*slot != target) {
        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + kCtxDirty) |= 1u;
        *slot = target;
    }

    const void* const image = _shadowMap;
    const int32_t h = ImageHeight(image);           // read FIRST in retail
    const int32_t w = ImageWidth(image);
    SetRect(ctx, kCtxViewport, 0, 0, w, h, 0x200u);

    RenderStateSlot4(ctx);
    // The fifth value is THIS FUNCTION'S OWN arg5, passed through untouched —
    // retail tail-jumps without writing its arg5 home slot.
    RendererDrawItemList(&renderer, ctx, a4, 0, a5);
}

}  // namespace Graphics

// ============================================================================
// slot 10 — ?Draw@SpotLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z
//                                                    retail 0x180105960
// ============================================================================
// The draw-group index is arg5 (`MOVSXD R13,[RBP+0xf0]`), NOT arg4 — arg4
// arrives in R9D and is never read by this body. Both facts are transcribed.
//
// From the group record at `renderer->[0x10] + arg5 * 0x1f0`:
//   +0x9c .. +0xd8   sixteen floats, the shadow projection Matrix
//   +0x6c .. +0x98   twelve floats, the shadow view Transform3
//   +0x78 / +0x7c / +0x80   ALSO read as a Vector3 for SetLight
//   +0x90 / +0x94 / +0x98   ditto, as the other Vector3
// The two Vector3s overlap the Transform3 on purpose: they are its third basis
// row and its translation, re-read out of the group rather than out of the
// local copy.
//
// The tail is `DrawItemList(renderer, ctx, arg5 + 1, arg6, arg7)` — note the
// **+ 1**, which SunLight's tail does not have.
// ============================================================================
namespace Graphics {

void SpotLight::DrawLight(Video::Context& context, Renderer& rendererRef,
                          int a4, int a5, int a6, int a7) const {
    (void)a4;                                       // R9D, never read by retail
    void* const ctx = reinterpret_cast<void*>(&context);          // B.3.5 bridge
    void* const renderer = reinterpret_cast<void*>(&rendererRef); // B.3.5 bridge

    // The draw-group index is arg5, NOT arg4.
    const uint8_t* const g = DrawGroup(renderer, a5);

    float bias[16];
    BiasMatrixA(bias);

    float projection[16];
    std::memcpy(projection, g + 0x9c, sizeof projection);
    float viewXform[12];
    std::memcpy(viewXform, g + 0x6c, sizeof viewXform);

    WriteShadowMatrix(ctx, projection, viewXform, bias);

    BindImage(ctx, kCtxImage50, _boundImage, 0x10u);
    BindImage(ctx, kCtxImage58, _shadowDepth, 0x20u);

    const int32_t dim = ImageWidth(_shadowDepth);

    float v3a[3], v3b[3];
    std::memcpy(v3a, g + 0x90, sizeof v3a);         // RDX
    std::memcpy(v3b, g + 0x78, sizeof v3b);         // R8
    const float zeroV4[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    ContextSetLight(ctx, v3a, v3b,
                    reinterpret_cast<const float*>(&_colour),
                    zeroV4,
                    _spotScalarA,
                    _spotScalarB,
                    dim);

    // Note the `+ 1`, which SunLight's tail does not have.
    RendererDrawItemList(renderer, ctx, a5 + 1, a6, a7);
}

}  // namespace Graphics

// ############################################################################
// Graphics::SunLight — the two slots sunlight.cpp did not port
// ############################################################################

// ============================================================================
// slot 10 — ?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z
//                                                    retail 0x1801067b0
// ============================================================================
// The same shape as SpotLight's, with three differences, all of them load-
// bearing:
//   * the bias matrix is CHOSEN, not fixed — `Video::Device::_singleton->[0x100]`
//     is asked through its own vtable slot 7 (`CALL [RAX+0x38]`) and its bool
//     answer picks matrix A over matrix B (see BiasMatrixA/B);
//   * the group fields are at +0x174 (projection) and +0x144 (view), not +0x9c
//     and +0x6c — SunLight's cascade record is laid out differently;
//   * SetLight's two Vector3/Vector4 arguments come from the OBJECT (+0xc0,
//     +0xcc, +0xdc) rather than from the group, its scalars are the literals
//     0.0f and 1.0f, and the first argument is a ZERO Vector3.
//
// The tail is `DrawItemList(renderer, ctx, arg4, arg6, arg7)` — arg4 here,
// where SpotLight used arg5 + 1, and arg5 is used only as the group index.
// ============================================================================
extern "C" void RtGraphics_SunLight_Draw(void* self, void* ctx, void* renderer,
                                         int32_t a4, int32_t a5, int32_t a6, int32_t a7) {
    reinterpret_cast<const ::Graphics::SunLight*>(self)
        ->::Graphics::SunLight::DrawLight(
              *reinterpret_cast<Video::Context*>(ctx),
              *reinterpret_cast<::Graphics::Renderer*>(renderer), a4, a5, a6, a7);
}

namespace Graphics {

void SunLight::DrawLight(Video::Context& context, Renderer& rendererRef,
                         int a4, int a5, int a6, int a7) const {
    void* const ctx = reinterpret_cast<void*>(&context);          // B.3.5 bridge
    void* const renderer = reinterpret_cast<void*>(&rendererRef); // B.3.5 bridge
    const uint8_t* const g = DrawGroup(renderer, a5);

    // `MOV RCX,[Device+0x100]; MOV RAX,[RCX]; CALL [RAX+0x38]` — slot 7 of
    // whatever hangs off the Device at +0x100, one argument, returns a bool in
    // AL. Read BEFORE anything else in the body (0x1801067e9..0x18010680d).
    bool modern;
    {
        void* const obj = *reinterpret_cast<void* const*>(VideoDevice() + 0x100);
        void** const vt = *reinterpret_cast<void***>(obj);
        modern = reinterpret_cast<bool (*)(void*)>(vt[7])(obj);
    }

    float bias[16];
    if (modern) BiasMatrixA(bias); else BiasMatrixB(bias);

    float projection[16];
    std::memcpy(projection, g + 0x174, sizeof projection);
    float viewXform[12];
    std::memcpy(viewXform, g + 0x144, sizeof viewXform);

    WriteShadowMatrix(ctx, projection, viewXform, bias);

    BindImage(ctx, kCtxImage58, _depthTarget, 0x20u);

    const int32_t dim = ImageWidth(_depthTarget);

    // The three vectors come from the OBJECT here, not from the draw group: the
    // direction Vector3 at +0xc0, the sun Vector4 at +0xcc and the ambient
    // Vector4 at +0xdc — the three `?Set@SunLight@` writes, under their names.
    const float zeroV3[3] = { 0.0f, 0.0f, 0.0f };
    ContextSetLight(ctx, zeroV3, &_dirX, &_sunR, &_ambientR, 0.0f, 1.0f, dim);

    // arg4 here, where SpotLight used arg5 + 1.
    RendererDrawItemList(renderer, ctx, a4, a6, a7);
}

}  // namespace Graphics

// ============================================================================
// slot 9 — ?DrawShadowMap@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z
//                                                    retail 0x180106bc0
// ============================================================================
// Longer than SpotLight's because it sets THREE rectangles, not one, and the
// third is inset by two pixels:
//
//   1. viewport (+0x1d0) = (0, 0, w, h)                     dirty bit 0x200
//   2. scissor  (+0x1e0) = (0, 0, w, h)                     dirty bit 0x400
//   3. the render state call
//   4. viewport (+0x1d0) = (1, 1, w - 2, h - 2)             dirty bit 0x200
//
// Step 4 REWRITES the viewport step 1 just set, to a one-pixel border inside
// the shadow map — the classic guard band that keeps a cascade's edge texels
// from bleeding. `ADD EAX,-2` / `ADD R9D,-2` on BOTH dimensions, and the origin
// becomes (1, 1), not (0, 0).
//
// *** THE IMAGE IS RE-READ FROM `this->[0xf8]` BEFORE EVERY ONE OF THE FOUR
// DIMENSION QUERIES. *** Retail never caches it, and the queries themselves
// re-read the Device singleton each time. Hoisting either would be one fewer
// indirection than retail performs and would miss a target swapped mid-call.
//
// THE WHOLE BODY IS BEHIND `renderer->[0x3c] != 0` (`CMP dword [R8+0x3c],0 /
// JZ`) — with no shadow-casting items queued there is nothing to draw and even
// the Context is left untouched.
// ============================================================================
extern "C" void RtGraphics_SunLight_DrawShadowMap(void* self, void* ctx, void* renderer,
                                                  int32_t a4, int32_t a5) {
    reinterpret_cast<const ::Graphics::SunLight*>(self)
        ->::Graphics::SunLight::DrawShadowMap(
              *reinterpret_cast<Video::Context*>(ctx),
              *reinterpret_cast<::Graphics::Renderer*>(renderer), a4, a5);
}

namespace Graphics {

void SunLight::DrawShadowMap(Video::Context& context, Renderer& rendererRef,
                             int a4, int a5) const {
    void* const ctx = reinterpret_cast<void*>(&context);          // B.3.5 bridge
    void* const renderer = reinterpret_cast<void*>(&rendererRef); // B.3.5 bridge

    // THE WHOLE BODY IS BEHIND `renderer->[0x3c] != 0`: with no shadow-casting
    // items queued there is nothing to draw and even the Context is left alone.
    if (*reinterpret_cast<const int32_t*>(static_cast<uint8_t*>(renderer) + 0x3c) == 0)
        return;

    void* const target = _renderBuffer;
    void** const slot = reinterpret_cast<void**>(static_cast<uint8_t*>(ctx) + kCtxTarget);
    if (*slot != target) {
        *reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(ctx) + kCtxDirty) |= 1u;
        *slot = target;
    }

    {
        const void* const image = _depthTarget;
        const int32_t h = ImageHeight(image);
        const int32_t w = ImageWidth(image);
        SetRect(ctx, kCtxViewport, 0, 0, w, h, 0x200u);
    }
    {
        const void* const image = _depthTarget;   // re-read
        const int32_t h = ImageHeight(image);
        const int32_t w = ImageWidth(image);
        SetRect(ctx, kCtxScissor, 0, 0, w, h, 0x400u);
    }

    RenderStateSlot4(ctx);

    {
        const void* const image = _depthTarget;   // re-read
        const int32_t h = ImageHeight(image) - 2;
        const int32_t w = ImageWidth(image) - 2;
        SetRect(ctx, kCtxViewport, 1, 1, w, h, 0x200u);
    }

    const int32_t dim = ImageWidth(_depthTarget);  // re-read

    const float zeroV3[3] = { 0.0f, 0.0f, 0.0f };
    ContextSetLight(ctx, zeroV3, &_dirX, &_sunR, &_ambientR, 0.0f, 1.0f, dim);

    RendererDrawItemList(renderer, ctx, a4, 0, a5);
}

}  // namespace Graphics

// ============================================================================
// Registration — name AND address for every body.
// ============================================================================
int RtGraphicsLightsVtableRegister() {
    int n = 0;

    // ---- Graphics::AmbientLight / Graphics::SpotLight ----------------------
    // ENDGAME B.3.9 block GFX: nothing. The ??0 / ??1 doors and the five slot
    // doors of each class are gone: their only readers were the minted
    // `??_7AmbientLight@` / `??_7SpotLight@` tables and their RT_DELDTOR rows,
    // all retired (tools/rt_mint_retired/gfx.txt). The bodies are the members.

    // ---- Graphics::SunLight -------------------------------------------------
    RegisterRuntimeExport("?DrawShadowMap@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HH@Z",
                          (void*)&RtGraphics_SunLight_DrawShadowMap); ++n;
    RegisterRuntimeAddr(0x180106bc0ull, (void*)&RtGraphics_SunLight_DrawShadowMap);

    RegisterRuntimeExport("?Draw@SunLight@Graphics@@UEBAXAEAVContext@Video@@AEAVRenderer@2@HHHH@Z",
                          (void*)&RtGraphics_SunLight_Draw); ++n;
    RegisterRuntimeAddr(0x1801067b0ull, (void*)&RtGraphics_SunLight_Draw);

    return n;
}
