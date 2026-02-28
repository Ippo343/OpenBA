#pragma once
// Weather — Runtime weather state, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x80.
//
// Decompiled from: Weather_Ctor (0x1400d3e50), Weather_Factory (0x140024fb0)
// Base: Object::External (0x20 bytes)
// Contains current weather state: temperatures, colors, fog, wind, precipitation.
// Many float constants are weather parameter defaults.

#include <cstdint>
#include <cstddef>

namespace WeatherAddr {
    constexpr uintptr_t Ctor    = 0x1400d3e50;
    constexpr uintptr_t Factory = 0x140024fb0;
    constexpr uintptr_t Vtable  = 0x140160290;
}

struct Weather {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x140160290
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- Weather fields ---
    void*    _field20;           // +0x20: zeroed
    int64_t  _field28;           // +0x28: 100 (duration/intensity?)
    uint32_t _field30;           // +0x30: zeroed
    float    _field34;           // +0x34: -1.0f (0xbf800000) (wind direction?)
    void*    _field38;           // +0x38: zeroed
    uint8_t  _field40;           // +0x40: zeroed (bool, is raining?)
    uint8_t  _pad41[3];          // +0x41: padding
    float    _field44;           // +0x44: 0.25f (0x3e800000) (cloud cover?)
    float    temperature;        // +0x48: 30.0f (0x41f00000)
    float    tempMin;            // +0x4C: 25.0f (0x41c80000) (daily min?)
    float    tempMax;            // +0x50: 30.0f (0x41f00000) (daily max?)
    float    _field54;           // +0x54: 1.0f (0x3f800000, packed as uint64 with field58)
    uint32_t _field58;           // +0x58: 0 (second half of packed uint64)
    uint8_t  _pad5C[4];          // +0x5C: padding

    // --- Light/atmosphere colors (ARGB packed uint32) ---
    uint32_t ambientColor;       // +0x60: 0xff595965 (dark blue-gray ambient)
    uint32_t sunColor;           // +0x64: 0xffcbcbbc (warm sunlight)
    uint32_t skyColor;           // +0x68: 0xffbfbff2 (light blue sky)

    // --- Additional atmosphere params ---
    float    fogStart;           // +0x6C: 90.0f (0x42b40000)
    float    fogEnd;             // +0x70: 225.0f (0x43610000)
    float    fogDensity;         // +0x74: 0.95f (0x3f733333)
    float    sunIntensity;       // +0x78: 120.0f (0x42f00000)
    float    ambientIntensity;   // +0x7C: 65.0f (0x42820000)
};

static_assert(sizeof(Weather) == 0x80, "Weather size mismatch");
static_assert(offsetof(Weather, vtable) == 0x00, "");
static_assert(offsetof(Weather, ambientColor) == 0x60, "");
static_assert(offsetof(Weather, fogStart) == 0x6C, "");
static_assert(offsetof(Weather, ambientIntensity) == 0x7C, "");
