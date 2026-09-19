#pragma once
// ============================================================================
// runtime/types/graphics/SunLight.h — Graphics::SunLight   Wave 9 lane GFX2
// ============================================================================
//
// Recovered from the three shipped bodies that write the class:
//
//   0x1801081f0  ??0SunLight@Graphics@@QEAA@XZ                        (ctor)
//   0x180107f10  ?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z
//   0x180108160  ??1SunLight@Graphics@@UEAA@XZ                        (dtor)
//
// SIZE. 0x108, from `MemAlloc(0x108, 8)` at the two exe call sites that build
// one (code/Game/Game/menudialogloopt.cpp:733 and
// code/Game/Game/gameupdateloop_load.cpp:1209). Consistent with the last field
// the ctor writes, `+0x100`.
//
// THE INHERITANCE CHAIN, and why `Light` contributes no bytes. The ctor calls
// `Graphics::Geometry::Geometry` (0x1800a4270) and then stamps TWO vtables into
// the SAME slot, back to back:
//
//   1801081f0+0x26  LEA RAX,[0x180158d30]  MOV [RDI],RAX   ; ??_7Light@Graphics@@6B@
//   1801081f0+0x30  LEA RAX,[0x180158e10]  MOV [RDI],RAX   ; ??_7SunLight@Graphics@@6B@
//
// Both addresses were confirmed against the shipped export table
// (tools/pe_exports.py: ord for ??_7Light@Graphics@@6B@ = 0x180158d30,
// ??_7SunLight@Graphics@@6B@ = 0x180158e10). One slot, single inheritance, and
// the next store is at +0xC0 — which is `sizeof(Geometry)`. So `Graphics::Light`
// adds a vtable override and NO data members, and SunLight's own fields start
// immediately at the end of Geometry.
//
// +0xEC IS A HOLE AND IS NEVER WRITTEN BY ANY OF THE THREE BODIES. The ctor
// goes +0xe8 -> +0xf0 and skips it; `Set` writes +0xc0..+0xe8 and stops. It is
// almost certainly the tail padding of the 12-byte `_ambient`/16-byte alignment
// pair. Any oracle over this class MUST exclude it
// (reference_oracle_must_not_compare_unwritten_bytes.md) — and the probe in
// sunlight.cpp does.
//
// A CORRECTION TO WAVE9_LANE_GFX.md §6.5, which named the two buffers the other
// way round ("depth ImageBuffer* +0xf0, colour ImageBuffer* +0xf8"). Read off
// the two `ImageBuffer::Create` argument tuples plus RenderBuffer::Create's own
// parameter order:
//
//   +0xf0  Create(ImageType=0, Usage=0, Format=5, ...)   Usage 0 -> render target
//   +0xf8  Create(ImageType=0, Usage=1, Format=7, ...)   Usage 1 -> DEPTH
//   RenderBuffer::Create(rb, +0xf0, +0xf8) and RenderBuffer's own layout is
//   {colorBuffer @ +0x00, depthBuffer @ +0x08} (runtime/types/video/Device.h)
//
// so +0xf0 is the COLOUR target and +0xf8 is the DEPTH target. That ordering is
// also what makes the capability gate make sense: the colour target is the
// OPTIONAL one (see sunlight.cpp's note on DeviceData slot 6) and the depth
// target is built unconditionally, which is how every shadow-map path works.
// ============================================================================

#include <cstdint>
#include <cstddef>

#include "Geometry.h"
#include "Light.h"      // ENDGAME B.3.5 block E: Graphics::Light, the real base

namespace RtGraphics {

struct SunLight {
    Geometry _base;             // +0x00 .. +0xBF  (0xC0 bytes; Light adds none)

    float    _dirX;             // +0xC0  ctor 0.0f
    float    _dirY;             // +0xC4  ctor 0.0f
    float    _dirZ;             // +0xC8  ctor -1.0f
    float    _sunR;             // +0xCC  ctor 1.0f
    float    _sunG;             // +0xD0  ctor 1.0f
    float    _sunB;             // +0xD4  ctor 1.0f
    float    _sunA;             // +0xD8  ctor 1.0f
    float    _ambientR;         // +0xDC  ctor 0.0f
    float    _ambientG;         // +0xE0  ctor 0.0f
    float    _ambientB;         // +0xE4  ctor 0.0f
    float    _ambientA;         // +0xE8  ctor 1.0f
    uint32_t _holdEC;           // +0xEC  NEVER WRITTEN by ctor, Set or dtor
    void*    _colourTarget;     // +0xF0  Video::ImageBuffer*  (capability-gated)
    void*    _depthTarget;      // +0xF8  Video::ImageBuffer*  (always built)
    void*    _renderBuffer;     // +0x100 Video::RenderBuffer*
};

static_assert(sizeof(SunLight) == 0x108, "Graphics::SunLight is 0x108 bytes "
              "(both exe call sites MemAlloc exactly 0x108)");
static_assert(offsetof(SunLight, _dirX)         == 0xC0,  "SunLight::_dirX");
static_assert(offsetof(SunLight, _dirZ)         == 0xC8,  "SunLight::_dirZ");
static_assert(offsetof(SunLight, _sunR)         == 0xCC,  "SunLight::_sunR");
static_assert(offsetof(SunLight, _sunA)         == 0xD8,  "SunLight::_sunA");
static_assert(offsetof(SunLight, _ambientR)     == 0xDC,  "SunLight::_ambientR");
static_assert(offsetof(SunLight, _ambientA)     == 0xE8,  "SunLight::_ambientA");
static_assert(offsetof(SunLight, _holdEC)       == 0xEC,  "SunLight::+0xEC hole");
static_assert(offsetof(SunLight, _colourTarget) == 0xF0,  "SunLight::_colourTarget");
static_assert(offsetof(SunLight, _depthTarget)  == 0xF8,  "SunLight::_depthTarget");
static_assert(offsetof(SunLight, _renderBuffer) == 0x100, "SunLight::_renderBuffer");

}  // namespace RtGraphics

// ############################################################################
// ENDGAME B.3 step 5 (block E) — `Graphics::SunLight`, the REAL C++ class
// ############################################################################
//
// `??_7SunLight@Graphics@@6B@` @ 0x180158e10, ELEVEN slots, sizeof 0x108,
// parent `Graphics::Light` (B35_SLOTS.md; the parent edge is the one this
// file's own banner above derived independently from the two back-to-back
// vtable stamps in the constructor):
//
//   [0] +0x00  0x180064620  (unexported) vector deleting destructor
//   [1] +0x08  0x1801067a0  ?Update@SunLight@…          wave28_lane_partsys.cpp
//   [2] +0x10  0x180106790  ?Draw@…MHHH@Z               wave28_lane_partsys.cpp
//   [3] +0x18  0x180106f50  ?AddDrawItems@…             wave28_lane_partsys.cpp
//   [4] +0x20  0x1800a4020  Geometry::UpdateAttached         inherited
//   [5] +0x28  0x1800a43c0  Geometry::GetSubTransform        inherited
//   [6] +0x30  0x1800154a0  Geometry::GetInstanceTransform   inherited
//   [7] +0x38  0x180015590  Geometry::SetSharedInstanceData  inherited
//   [8] +0x40  0x1800a4420  Geometry::Clean                  inherited
//   [9] +0x48  0x180106bc0  ?DrawShadowMap@…            lights_vtable.cpp
//  [10] +0x50  0x1801067b0  ?Draw@…HHHH@Z  -> DrawLight lights_vtable.cpp
//
// The POD above keeps its name and its users; this class describes the same
// bytes with `Light` as a real base and is cross-asserted against it field for
// field. The field NAMES are the POD's, deliberately: the bodies in
// `sunlight.cpp`, `sun_camera.cpp` and `wave28_lane_partsys.cpp` read
// `_dirX` / `_sunR` / `_ambientA` and move onto these members unchanged.
// ############################################################################

namespace Graphics {

class SunLight : public Light {
public:
    SunLight(const SunLight& src);                 // 0x180064410  not ported
    SunLight& operator=(const SunLight& src);      // 0x180064510  not ported

    Geometry* DeletingDtor(unsigned flags) override = 0;                 // [0] 0x180064620
    void Update(const System::Time& time) override;                      // [1] 0x1801067a0
    void Draw(Video::Context& context, const Camera& camera,
              ExternalPtrHandle material,
              float opacity, int a, int b, int c) const override;         // [2] 0x180106790
    void AddDrawItems(const Scene& scene, Renderer& renderer) const override; // [3] 0x180106f50
    void DrawShadowMap(Video::Context& context, Renderer& renderer,
                       int a, int b) const override;                       // [9] 0x180106bc0
    void DrawLight(Video::Context& context, Renderer& renderer,
                   int a, int b, int c, int d) const override;             // [10] 0x1801067b0

    // `?Set@SunLight@Graphics@@QEAAXAEBVVector3@Math@@AEBVVector4@4@1@Z`
    // @ 0x180107f10 — PORTED, body in `runtime/src/graphics/sunlight.cpp`.
    // The mangled name's trailing `1` is a back-reference to the SECOND
    // parameter's type, so arguments two and three are both
    // `const Math::Vector4&`.
    void Set(const Math::Vector3& direction, const Math::Vector4& sun,
             const Math::Vector4& ambient);

    // `?SetDirection@SunLight@Graphics@@QEAAXAEBVVector3@Math@@@Z`
    // @ 0x180107ee0 — exported, NOT PORTED, no caller. Declared, undefined.
    void SetDirection(const Math::Vector3& direction);

    // `?GetSunCamera@SunLight@Graphics@@QEBA?AVCamera@2@AEAVRenderer@2@@Z`
    // @ 0x180107010 is PORTED (`runtime/src/graphics/sun_camera.cpp`) and is a
    // HIDDEN-RETSLOT export: it returns `Graphics::Camera` — 0xd8 — BY VALUE
    // (B35_RULES §2.5). A by-value return needs the COMPLETE type, and
    // `Graphics::Camera` is block F's class, so this member cannot be declared
    // until `runtime/types/graphics/Camera.h` carries the real class. Its door
    // keeps the registration and the body; block F adds:
    //     Camera GetSunCamera(Renderer& renderer) const;   // 0x180107010
    // Recorded here rather than left to be rediscovered.

protected:
    ~SunLight();     // 0x180108160  PORTED, body in sunlight.cpp

public:
    float    _dirX;             // +0xC0  ctor 0.0f
    float    _dirY;             // +0xC4  ctor 0.0f
    float    _dirZ;             // +0xC8  ctor -1.0f
    float    _sunR;             // +0xCC  ctor 1.0f
    float    _sunG;             // +0xD0  ctor 1.0f
    float    _sunB;             // +0xD4  ctor 1.0f
    float    _sunA;             // +0xD8  ctor 1.0f
    float    _ambientR;         // +0xDC  ctor 0.0f
    float    _ambientG;         // +0xE0  ctor 0.0f
    float    _ambientB;         // +0xE4  ctor 0.0f
    float    _ambientA;         // +0xE8  ctor 1.0f
    uint32_t _holdEC;           // +0xEC  NEVER WRITTEN by ctor, Set or dtor
    void*    _colourTarget;     // +0xF0  Video::ImageBuffer*  (capability-gated)
    void*    _depthTarget;      // +0xF8  Video::ImageBuffer*  (always built)
    void*    _renderBuffer;     // +0x100 Video::RenderBuffer*
};

static_assert(sizeof(SunLight) == 0x108,
              "Graphics::SunLight is 0x108 bytes (both exe call sites MemAlloc "
              "exactly 0x108, and the deleting dtor 0x180064620 agrees)");
static_assert(alignof(SunLight) == 8, "qword aligned (the vptr)");
static_assert(sizeof(SunLight) == sizeof(RtGraphics::SunLight),
              "the real class and the POD describe the same bytes");

static_assert(offsetof(SunLight, _dirX)         == offsetof(RtGraphics::SunLight, _dirX),         "alias _dirX");
static_assert(offsetof(SunLight, _sunR)         == offsetof(RtGraphics::SunLight, _sunR),         "alias _sunR");
static_assert(offsetof(SunLight, _ambientR)     == offsetof(RtGraphics::SunLight, _ambientR),     "alias _ambientR");
static_assert(offsetof(SunLight, _holdEC)       == offsetof(RtGraphics::SunLight, _holdEC),       "alias +0xEC hole");
static_assert(offsetof(SunLight, _colourTarget) == offsetof(RtGraphics::SunLight, _colourTarget), "alias _colourTarget");
static_assert(offsetof(SunLight, _depthTarget)  == offsetof(RtGraphics::SunLight, _depthTarget),  "alias _depthTarget");
static_assert(offsetof(SunLight, _renderBuffer) == offsetof(RtGraphics::SunLight, _renderBuffer), "alias _renderBuffer");

static_assert(offsetof(SunLight, _dirX)         == 0xC0,  "SunLight::_dirX @ +0xC0 (??0 0x1801081f0)");
static_assert(offsetof(SunLight, _dirZ)         == 0xC8,  "SunLight::_dirZ @ +0xC8 (-1.0f)");
static_assert(offsetof(SunLight, _sunR)         == 0xCC,  "SunLight::_sunR @ +0xCC (Set arg 2)");
static_assert(offsetof(SunLight, _sunA)         == 0xD8,  "SunLight::_sunA @ +0xD8");
static_assert(offsetof(SunLight, _ambientR)     == 0xDC,  "SunLight::_ambientR @ +0xDC (Set arg 3)");
static_assert(offsetof(SunLight, _ambientA)     == 0xE8,  "SunLight::_ambientA @ +0xE8");
static_assert(offsetof(SunLight, _holdEC)       == 0xEC,  "SunLight::+0xEC is never written by any of the three bodies");
static_assert(offsetof(SunLight, _colourTarget) == 0xF0,  "SunLight::_colourTarget @ +0xF0 (Usage 0 = render target)");
static_assert(offsetof(SunLight, _depthTarget)  == 0xF8,  "SunLight::_depthTarget @ +0xF8 (Usage 1 = depth)");
static_assert(offsetof(SunLight, _renderBuffer) == 0x100, "SunLight::_renderBuffer @ +0x100");

}  // namespace Graphics
