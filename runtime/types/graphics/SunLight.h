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
