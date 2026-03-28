#pragma once
// ClimateGenerator — Climate/weather generation, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x110.
//
// Decompiled from: ClimateGenerator_Ctor (0x1400930c0)
// Base: Object::External (0x20 bytes)
// Contains temperature ranges, precipitation params, and seasonal data.
// Constructor memsets +0x44 through +0x103 to zero (0xC0 bytes of climate data).

#include <cstdint>
#include <cstddef>

namespace ClimateGeneratorAddr {
    constexpr uintptr_t Ctor    = 0x1400930c0;
    constexpr uintptr_t Vtable  = 0x14015e140;
}

struct ClimateGenerator {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015e140
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- ClimateGenerator fields ---
    void*    _field20;           // +0x20: ExternalPtr (serialized via FUN_14000bf40)
    void*    _field28;           // +0x28: System::String (8-byte COW, name/reference)
    uint8_t  _field30;           // +0x30: 1 (bool, enabled?)
    uint8_t  _pad31[3];          // +0x31: padding
    float    temperatureBase;    // +0x34: 50.0f (0x42480000) (base temp in F?)
    float    tempVariation;      // +0x38: 30.0f (0x41f00000)
    float    precipitation;      // +0x3C: 5.0f (0x40a00000)
    float    windSpeed;          // +0x40: 10.0f (0x41200000)
    uint8_t  _climateData[0xC0]; // +0x44: 48 floats (seasonal/biome data, memset to 0)
    float    _field104;          // +0x104: 0.0f (serialized as float)
    float    _field108;          // +0x108: 1.0f (0x3f800000)
    float    _field10C;          // +0x10C: 1.0f (0x3f800000)
};

static_assert(sizeof(ClimateGenerator) == 0x110, "ClimateGenerator size mismatch");
static_assert(offsetof(ClimateGenerator, vtable) == 0x00, "");
static_assert(offsetof(ClimateGenerator, temperatureBase) == 0x34, "");
static_assert(offsetof(ClimateGenerator, _field108) == 0x108, "");
