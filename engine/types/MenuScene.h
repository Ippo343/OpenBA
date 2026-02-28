#pragma once
// MenuScene — Main menu scene controller, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x208.
//
// Decompiled from: MenuScene_Ctor (0x1400a3d70)
// Base: Object::External (0x20 bytes)
// Contains scene objects, camera transform, lighting colors.
// Has embedded Transform (called at +0x88, likely 4x3 or 4x4 matrix).
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
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    void*    _field38;           // +0x38: zeroed
    void*    _field40;           // +0x40: zeroed
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
    void*    _field68;           // +0x68: zeroed
    void*    _field70;           // +0x70: zeroed
    void*    _field80;           // +0x80: zeroed (gap at +0x78?)
    uint8_t  _pad78[8];          // +0x78: gap

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
    float    ambientIntensity;   // +0x1B0: 65.0f (packed uint64 with next)
    uint8_t  _pad1B4[4];         // +0x1B4: second half of packed uint64

    // --- Remaining fields ---
    void*    _field1B8;          // +0x1B8: zeroed
    uint32_t _field1C0;          // +0x1C0: zeroed
    uint8_t  _pad1C4[4];         // +0x1C4: padding
    void*    _field1C8;          // +0x1C8: zeroed
    void*    _field1D0;          // +0x1D0: zeroed
    void*    _field1D8;          // +0x1D8: zeroed
    uint32_t _field1E0;          // +0x1E0: zeroed
    uint8_t  _pad1E4[4];         // +0x1E4: padding
    void*    _field1E8;          // +0x1E8: zeroed
    void*    _field1F0;          // +0x1F0: zeroed
    void*    _field1F8;          // +0x1F8: zeroed
    uint32_t _field200;          // +0x200: zeroed
    uint8_t  _pad204[4];         // +0x204: padding
};

static_assert(sizeof(MenuScene) == 0x208, "MenuScene size mismatch");
static_assert(offsetof(MenuScene, vtable) == 0x00, "");
static_assert(offsetof(MenuScene, ambientColor) == 0x194, "");
