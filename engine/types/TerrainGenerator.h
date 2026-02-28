#pragma once
// TerrainGenerator — Terrain height/noise generation, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x90.
//
// Decompiled from: TerrainGenerator_Ctor (0x1400b1810)
// Base: Object::External (0x20 bytes)
// Contains noise parameters and terrain generation configs.

#include <cstdint>
#include <cstddef>

namespace TerrainGeneratorAddr {
    constexpr uintptr_t Ctor    = 0x1400b1810;
    constexpr uintptr_t Vtable  = 0x14015f170;
}

struct TerrainGenerator {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015f170
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- TerrainGenerator fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    uint8_t  _field30;           // +0x30: 1 (bool, enabled?)
    uint8_t  _pad31[7];          // +0x31: padding
    void*    _field38;           // +0x38: zeroed
    int32_t  _field40;           // +0x40: -1 sentinel
    uint8_t  _pad44[4];          // +0x44: padding
    void*    _field48;           // +0x48: zeroed
    void*    _field50;           // +0x50: zeroed
    int32_t  _field58;           // +0x58: -1 sentinel
    uint8_t  _pad5C[4];          // +0x5C: padding
    void*    _field60;           // +0x60: zeroed
    void*    _field68;           // +0x68: zeroed
    int32_t  _field70;           // +0x70: -1 sentinel
    uint8_t  _pad74[4];          // +0x74: padding
    void*    _field78;           // +0x78: zeroed
    void*    _field80;           // +0x80: zeroed
    void*    _field88;           // +0x88: zeroed
};

static_assert(sizeof(TerrainGenerator) == 0x90, "TerrainGenerator size mismatch");
static_assert(offsetof(TerrainGenerator, vtable) == 0x00, "");
