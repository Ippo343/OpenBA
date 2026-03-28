#pragma once
// MenuScene — Main menu scene controller, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x208.
//
// Decompiled from: MenuScene_Ctor (0x1400a3d70), MenuScene_Dtor (0x1400a3970),
//                  MenuScene_Serialize (0x1400a3630)
// Base: Object::External (0x20 bytes)
//
// Contains scene name string, scene geometry object, dynamic arrays of
// scene objects (0x20-byte, ExternalPtr, 0x14-byte, ExternalPtr elements),
// external references, an embedded Transform at +0x88, atmosphere/lighting
// colors and fog parameters, two tree/map structures, and a Vector4.
//
// Atmosphere colors match Weather defaults (shared initial values).

#include <cstdint>
#include <cstddef>

namespace MenuSceneAddr {
    constexpr uintptr_t Ctor    = 0x1400a3d70;
    constexpr uintptr_t Vtable  = 0x14015ede0;
}

struct MenuScene {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ede0
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- MenuScene fields ---
    void*    _field20;           // +0x20: System::String (scene name), serialized
    void*    _field28;           // +0x28: Geometry* (scene object, scalar-deleted in dtor)

    // Dynamic array 1: 0x20-byte elements (serialized in FUN_14001bbf0)
    // Layout: +0x30=data ptr, +0x38=count|capacity packed
    void*    _field30;           // +0x30: array data pointer
    void*    _field38;           // +0x38: count (low 32b) | capacity (high 32b)

    // Dynamic array 2: ExternalPtr elements (serialized in FUN_14001bcc0)
    // Layout: +0x40=data ptr, +0x48=count|capacity packed
    void*    _field40;           // +0x40: array data pointer
    void*    _field48;           // +0x48: count (low 32b) | capacity (high 32b)

    // Dynamic array 3: 0x14-byte elements (serialized per-field: 4 floats + 1 int)
    // Layout: +0x50=data ptr, +0x58=count|capacity packed
    void*    _field50;           // +0x50: array data pointer
    void*    _field58;           // +0x58: count (low 32b) | capacity (high 32b)

    // Dynamic array 4: ExternalPtr elements (serialized in FUN_14001bff0)
    // Layout: +0x60=data ptr, +0x68=count|capacity packed
    void*    _field60;           // +0x60: array data pointer
    void*    _field68;           // +0x68: count (low 32b) | capacity (high 32b)

    // External reference (serialized by FUN_14001b8e0)
    void*    _field70;           // +0x70: ExternalPtr (typed reference)

    // +0x78: float, serialized in version >= 2 section
    float    _field78;           // +0x78: float (version >= 2)
    uint8_t  _pad7C[4];          // +0x7C: padding

    // External reference for Weather type (serialized by FUN_1400196a0)
    void*    _field80;           // +0x80: ExternalPtr (Weather)

    // Embedded transform at +0x88 (initialized by FUN_140098800)
    uint8_t  transform[0x10C];   // +0x88: Transform data (0x10C bytes)

    // --- Atmosphere/lighting colors (same defaults as Weather) ---
    uint32_t ambientColor;       // +0x194: 0xff595965
    uint32_t sunColor;           // +0x198: 0xffcbcbbc
    uint32_t skyColor;           // +0x19C: 0xffbfbff2
    float    fogStart;           // +0x1A0: 90.0f
    float    fogEnd;             // +0x1A4: 225.0f
    float    fogDensity;         // +0x1A8: 0.95f
    float    sunIntensity;       // +0x1AC: 120.0f
    float    ambientIntensity;   // +0x1B0: 65.0f
    uint8_t  _pad1B4[4];         // +0x1B4: zero (second half of packed qword write)

    // --- Remaining fields ---
    void*    _field1B8;          // +0x1B8: Math::Vector4 (serialized)
                                 //         (Actually starts at +0x1B4 as 16 bytes,
                                 //          but Serialize call is at +0x1B4)

    uint32_t _field1C0;          // +0x1C0: zeroed
    uint8_t  _pad1C4[4];         // +0x1C4: padding

    // Tree/map structure 1 at +0x1C8 (20 bytes)
    // Layout: +0x1C8 data, +0x1D0 data, +0x1D8 root, +0x1E0 count
    void*    _field1C8;          // +0x1C8: tree data ptr
    void*    _field1D0;          // +0x1D0: tree data ptr
    void*    _field1D8;          // +0x1D8: tree root
    uint32_t _field1E0;          // +0x1E0: tree count
    uint8_t  _pad1E4[4];         // +0x1E4: padding

    // Tree/map structure 2 at +0x1E8 (20 bytes)
    void*    _field1E8;          // +0x1E8: tree data ptr
    void*    _field1F0;          // +0x1F0: tree data ptr
    void*    _field1F8;          // +0x1F8: tree root
    uint32_t _field200;          // +0x200: tree count
    uint8_t  _pad204[4];         // +0x204: padding
};

static_assert(sizeof(MenuScene) == 0x208, "MenuScene size mismatch");
static_assert(offsetof(MenuScene, vtable) == 0x00, "");
static_assert(offsetof(MenuScene, _field20) == 0x20, "");
static_assert(offsetof(MenuScene, _field28) == 0x28, "");
static_assert(offsetof(MenuScene, _field30) == 0x30, "");
static_assert(offsetof(MenuScene, _field40) == 0x40, "");
static_assert(offsetof(MenuScene, _field50) == 0x50, "");
static_assert(offsetof(MenuScene, _field60) == 0x60, "");
static_assert(offsetof(MenuScene, _field70) == 0x70, "");
static_assert(offsetof(MenuScene, _field78) == 0x78, "");
static_assert(offsetof(MenuScene, _field80) == 0x80, "");
static_assert(offsetof(MenuScene, transform) == 0x88, "");
static_assert(offsetof(MenuScene, ambientColor) == 0x194, "");
static_assert(offsetof(MenuScene, sunColor) == 0x198, "");
static_assert(offsetof(MenuScene, skyColor) == 0x19C, "");
static_assert(offsetof(MenuScene, fogStart) == 0x1A0, "");
static_assert(offsetof(MenuScene, fogEnd) == 0x1A4, "");
static_assert(offsetof(MenuScene, fogDensity) == 0x1A8, "");
static_assert(offsetof(MenuScene, sunIntensity) == 0x1AC, "");
static_assert(offsetof(MenuScene, ambientIntensity) == 0x1B0, "");
static_assert(offsetof(MenuScene, _field1C8) == 0x1C8, "");
static_assert(offsetof(MenuScene, _field1E8) == 0x1E8, "");
static_assert(offsetof(MenuScene, _field200) == 0x200, "");
