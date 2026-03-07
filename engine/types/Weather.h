#pragma once
// Weather — binary-compatible struct layout (0x80 bytes).
// Application-x64.exe, build 170910.
//
// Decompiled from: Weather_Ctor (0x1400d3e50), Serialize (0x1400d3c90)
// Base: Object::External (0x20 bytes)
//
// Field names recovered from ToolKit resource files:
//   Environment/Rain.rsc, Environment/Snow.rsc, Environment/Environment.rsc
// Serialization order confirmed from Slot5 decompilation.

#include <cstdint>
#include <cstddef>

namespace WeatherAddr {
    constexpr uintptr_t Ctor    = 0x1400d3e50;
    constexpr uintptr_t Factory = 0x140024fb0;
    constexpr uintptr_t Vtable  = 0x140160290;
    constexpr uintptr_t Dtor    = 0x140025020;
    constexpr uintptr_t Serialize = 0x1400d3c90;
    constexpr uintptr_t Cleanup = 0x140025000;
    constexpr uintptr_t Reset   = 0x140024ff0;
}

struct Weather {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160290
    uint8_t  _extBase[0x18];    // +0x08: External base fields

    // --- Weather particle fields ---
    void*    material;           // +0x20: MaterialInstance* _material (External ref)
    int32_t  particleCount;      // +0x28: _particleCount (default 100)
    float    velocityDirX;       // +0x2C: _velocityDirection.x (default 0)
    float    velocityDirY;       // +0x30: _velocityDirection.y (default 0)
    float    velocityDirZ;       // +0x34: _velocityDirection.z (default -1.0)
    float    velocity;           // +0x38: _velocity (default 0)
    float    velocityTolerence;  // +0x3C: _velocityTolerence (default 0) [sic: typo in original]
    bool     velocityStretch;    // +0x40: _velocityStretch (default false)
    uint8_t  _pad41[3];          // +0x41: padding
    float    particleSize;       // +0x44: _particleSize (default 0.25)
    float    size;               // +0x48: _size (default 30.0)
    float    nearFade;           // +0x4C: _nearFade (default 25.0)
    float    nearFadeStart;      // +0x50: _nearFadeStart (default 30.0)
    float    farFadeStart;       // +0x54: _farFadeStart (default 1.0)
    float    farFade;            // +0x58: _farFade (default 0)
    float    transitionTime;     // +0x5C: _transitionTime (default 0)

    // --- EnvironmentSetting fields (lighting/atmosphere) ---
    uint32_t ambientColor;       // +0x60: _ambientColor (default 0xFF595965)
    uint32_t lightColor;         // +0x64: _lightColor (default 0xFFCBCBBC)
    uint32_t fogColor;           // +0x68: _fogColor (default 0xFFBFBFF2)
    float    fogNear;            // +0x6C: _fogNear (default 90.0)
    float    fogFar;             // +0x70: _fogFar (default 225.0)
    float    fogMax;             // +0x74: _fogMax (default 0.95)
    float    sunHeading;         // +0x78: _sunHeading (default 120.0)
    float    sunPitch;           // +0x7C: _sunPitch (default 65.0)
};

static_assert(sizeof(Weather) == 0x80, "Weather size mismatch");
static_assert(offsetof(Weather, vtable) == 0x00, "vtable at +0x00");
static_assert(offsetof(Weather, material) == 0x20, "material at +0x20");
static_assert(offsetof(Weather, particleCount) == 0x28, "particleCount at +0x28");
static_assert(offsetof(Weather, velocityDirX) == 0x2C, "velocityDirX at +0x2C");
static_assert(offsetof(Weather, velocityDirY) == 0x30, "velocityDirY at +0x30");
static_assert(offsetof(Weather, velocityDirZ) == 0x34, "velocityDirZ at +0x34");
static_assert(offsetof(Weather, velocity) == 0x38, "velocity at +0x38");
static_assert(offsetof(Weather, velocityTolerence) == 0x3C, "velocityTolerence at +0x3C");
static_assert(offsetof(Weather, velocityStretch) == 0x40, "velocityStretch at +0x40");
static_assert(offsetof(Weather, particleSize) == 0x44, "particleSize at +0x44");
static_assert(offsetof(Weather, size) == 0x48, "size at +0x48");
static_assert(offsetof(Weather, nearFade) == 0x4C, "nearFade at +0x4C");
static_assert(offsetof(Weather, nearFadeStart) == 0x50, "nearFadeStart at +0x50");
static_assert(offsetof(Weather, farFadeStart) == 0x54, "farFadeStart at +0x54");
static_assert(offsetof(Weather, farFade) == 0x58, "farFade at +0x58");
static_assert(offsetof(Weather, transitionTime) == 0x5C, "transitionTime at +0x5C");
static_assert(offsetof(Weather, ambientColor) == 0x60, "ambientColor at +0x60");
static_assert(offsetof(Weather, lightColor) == 0x64, "lightColor at +0x64");
static_assert(offsetof(Weather, fogColor) == 0x68, "fogColor at +0x68");
static_assert(offsetof(Weather, fogNear) == 0x6C, "fogNear at +0x6C");
static_assert(offsetof(Weather, fogFar) == 0x70, "fogFar at +0x70");
static_assert(offsetof(Weather, fogMax) == 0x74, "fogMax at +0x74");
static_assert(offsetof(Weather, sunHeading) == 0x78, "sunHeading at +0x78");
static_assert(offsetof(Weather, sunPitch) == 0x7C, "sunPitch at +0x7C");
