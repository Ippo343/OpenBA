#pragma once
// ============================================================================
// code/Game/Game/Weather.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `Weather` (0x80): a weather description resource (Environment/Rain.rsc,
// Snow.rsc: particle effect + environment lighting), a real `Object::External`
// subclass. Layout from ctor 0x1400d3e50 + Serialize 0x1400d3c90. Field names
// from the ToolKit resource files.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "Weather", parent "Object::External", table 0x140160290,
//   9 slots, sizeof 0x80.
//   [0] 0x140025020 scalar deleting dtor   [1] 0x140024fa0 TypeName
//   [2] 0x140024f90 Type                   [3] 0x1400d3e40 GetFileVersion `MOV EAX,5`
//   [4] IMPORT Object::External::Compile   [5] 0x1400d3c90 Serialize
//   [6] 0x140025000 Reconstruct            [7] 0x140001640 Destruct
//   [8] 0x140024ff0 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class Weather final : public Object::External {
    DECLARE_EXE_TYPE(Weather)

public:
    Weather();                                                    // 0x1400d3e50

    // External::~External only
    ~Weather() override = default;                                // [0] 0x140025020
    int32_t GetFileVersion() const override { return 5; }         // [3] 0x1400d3e40
    // [4] Compile: inherited (retail's slot is the import; the old table's was null)
    bool Serialize(void* stream) override;                        // [5] 0x1400d3c90
    Object::External* Reconstruct() override;                     // [6] 0x140025000
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x140024ff0

public:
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

static_assert(sizeof(Weather) == 0x80, "Weather is 0x80");
static_assert(offsetof(Weather, material) == 0x20, "+0x20");
static_assert(offsetof(Weather, particleCount) == 0x28, "+0x28");
static_assert(offsetof(Weather, velocityDirX) == 0x2C, "+0x2C");
static_assert(offsetof(Weather, velocityDirZ) == 0x34, "+0x34");
static_assert(offsetof(Weather, velocity) == 0x38, "+0x38");
static_assert(offsetof(Weather, velocityTolerence) == 0x3C, "+0x3C");
static_assert(offsetof(Weather, velocityStretch) == 0x40, "+0x40");
static_assert(offsetof(Weather, particleSize) == 0x44, "+0x44");
static_assert(offsetof(Weather, size) == 0x48, "+0x48");
static_assert(offsetof(Weather, nearFade) == 0x4C, "+0x4C");
static_assert(offsetof(Weather, nearFadeStart) == 0x50, "+0x50");
static_assert(offsetof(Weather, farFadeStart) == 0x54, "+0x54");
static_assert(offsetof(Weather, farFade) == 0x58, "+0x58");
static_assert(offsetof(Weather, transitionTime) == 0x5C, "+0x5C");
static_assert(offsetof(Weather, ambientColor) == 0x60, "+0x60");
static_assert(offsetof(Weather, lightColor) == 0x64, "+0x64");
static_assert(offsetof(Weather, fogColor) == 0x68, "+0x68");
static_assert(offsetof(Weather, fogNear) == 0x6C, "+0x6C");
static_assert(offsetof(Weather, fogFar) == 0x70, "+0x70");
static_assert(offsetof(Weather, fogMax) == 0x74, "+0x74");
static_assert(offsetof(Weather, sunHeading) == 0x78, "+0x78");
static_assert(offsetof(Weather, sunPitch) == 0x7C, "+0x7C");
