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
    void*    _field20;           // +0x20: ExternalPtr (serialized via FUN_14000bf40)
    void*    _field28;           // +0x28: System::String (8-byte COW, destroyed in dtor)
    uint8_t  _field30;           // +0x30: 1 (bool, enabled?)
    uint8_t  _pad31[7];          // +0x31: padding
    // Noise layer 1
    void*    _field38;           // +0x38: System::String (filename, resolved to External on load)
    int32_t  _field40;           // +0x40: -1 sentinel (ExternalFactory generation counter)
    uint8_t  _pad44[4];          // +0x44: padding
    void*    _field48;           // +0x48: resolved External ptr for noise layer 1
    // Noise layer 2
    void*    _field50;           // +0x50: System::String (filename)
    int32_t  _field58;           // +0x58: -1 sentinel
    uint8_t  _pad5C[4];          // +0x5C: padding
    void*    _field60;           // +0x60: resolved External ptr for noise layer 2
    // Noise layer 3
    void*    _field68;           // +0x68: System::String (filename)
    int32_t  _field70;           // +0x70: -1 sentinel
    uint8_t  _pad74[4];          // +0x74: padding
    void*    _field78;           // +0x78: resolved External ptr for noise layer 3
    // Dynamic terrain data array
    void*    _field80;           // +0x80: terrain data array ptr (0x44 bytes per entry, freed in dtor)
    void*    _field88;           // +0x88: count/capacity info for terrain data array
};

static_assert(sizeof(TerrainGenerator) == 0x90, "TerrainGenerator size mismatch");
static_assert(offsetof(TerrainGenerator, vtable) == 0x00, "");
