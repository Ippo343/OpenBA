#pragma once
// MapGenerator — World map generation system, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x178.
//
// Decompiled from: MapGenerator_Ctor (0x1400a3030)
// Base: Object::External (0x20 bytes)
// Large struct with many generation parameters, seed data, and terrain/resource configs.

#include <cstdint>
#include <cstddef>

namespace MapGeneratorAddr {
    constexpr uintptr_t Ctor    = 0x1400a3030;
    constexpr uintptr_t Vtable  = 0x14015ed80;
}

struct MapGenerator {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ed80
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- MapGenerator fields ---
    void*    _field20;           // +0x20: zeroed
    void*    _field28;           // +0x28: zeroed
    void*    _field30;           // +0x30: zeroed
    uint8_t  _field38;           // +0x38: 1 (bool, enabled?)
    uint8_t  _pad39[3];          // +0x39: padding
    int32_t  _field3C;           // +0x3C: 8 (grid size / subdivision?)
    float    _field40;           // +0x40: 2.0f (0x40000000) (scale?)
    uint8_t  _pad44[4];          // +0x44: padding
    void*    _field48;           // +0x48: zeroed
    int32_t  _field50;           // +0x50: -1 sentinel (seed?)
    uint8_t  _pad54[4];          // +0x54: padding
    void*    _field58;           // +0x58: zeroed
    void*    _field60;           // +0x60: zeroed
    void*    _field68;           // +0x68: zeroed
    void*    _field70;           // +0x70: zeroed
    void*    _field78;           // +0x78: zeroed
    void*    _field80;           // +0x80: zeroed
    void*    _field88;           // +0x88: zeroed
    void*    _field90;           // +0x90: zeroed
    void*    _field98;           // +0x98: zeroed
    void*    _fieldA0;           // +0xA0: zeroed
    void*    _fieldA8;           // +0xA8: zeroed
    void*    _fieldB0;           // +0xB0: zeroed
    void*    _fieldB8;           // +0xB8: zeroed
    void*    _fieldC0;           // +0xC0: zeroed
    void*    _fieldC8;           // +0xC8: zeroed
    void*    _fieldD0;           // +0xD0: zeroed
    void*    _fieldD8;           // +0xD8: zeroed
    void*    _fieldE0;           // +0xE0: zeroed
    void*    _fieldE8;           // +0xE8: zeroed
    void*    _fieldF0;           // +0xF0: zeroed
    void*    _fieldF8;           // +0xF8: zeroed
    void*    _field100;          // +0x100: zeroed
    void*    _field108;          // +0x108: zeroed
    void*    _field110;          // +0x110: zeroed
    void*    _field118;          // +0x118: zeroed
    int32_t  _field120;          // +0x120: -1 sentinel
    uint8_t  _pad124[4];         // +0x124: padding
    void*    _field128;          // +0x128: zeroed
    float    _field130;          // +0x130: 6.0f (0x40c00000) (terrain amplitude?)
    uint8_t  _pad134[4];         // +0x134: padding
    void*    _field138;          // +0x138: zeroed
    int32_t  _field140;          // +0x140: -1 sentinel
    uint8_t  _pad144[4];         // +0x144: padding
    void*    _field148;          // +0x148: zeroed
    int32_t  _field150;          // +0x150: 16 (0x10) (chunk size?)
    uint8_t  _pad154[4];         // +0x154: padding
    void*    _field158;          // +0x158: zeroed
    int32_t  _field160;          // +0x160: -1 sentinel
    uint8_t  _pad164[4];         // +0x164: padding
    void*    _field168;          // +0x168: zeroed
    uint8_t  _pad170[8];         // +0x170: padding to 0x178
};

static_assert(sizeof(MapGenerator) == 0x178, "MapGenerator size mismatch");
static_assert(offsetof(MapGenerator, vtable) == 0x00, "");
static_assert(offsetof(MapGenerator, _field40) == 0x40, "");
static_assert(offsetof(MapGenerator, _field130) == 0x130, "");
