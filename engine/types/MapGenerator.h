#pragma once
// MapGenerator — World map generation system, binary-compatible struct layout.
// Application-x64.exe, build 170910. Size = 0x178.
//
// Decompiled from: MapGenerator_Ctor (0x1400a3030), MapGenerator_Serialize (0x1400a2980)
// Base: Object::External (0x20 bytes)
//
// MapGenerator holds all parameters for procedural world generation:
// terrain settings, resource lists (trees, stones, ores, herbs, animals),
// biome configs, and references to description files.

#include <cstdint>
#include <cstddef>

namespace MapGeneratorAddr {
    constexpr uintptr_t Ctor    = 0x1400a3030;
    constexpr uintptr_t Vtable  = 0x14015ed80;
    constexpr uintptr_t Dtor    = 0x14001ae70;
    constexpr uintptr_t DtorBody = 0x14001aea0;
    constexpr uintptr_t Serialize = 0x1400a2980;
    constexpr uintptr_t Cleanup = 0x14001ae30;
    constexpr uintptr_t Slot8   = 0x14001ae20;
}

// ExternalRef: an External reference stored as String + factoryGen + resolvedPtr (0x18 bytes)
// Matches the pattern: String(8) + int32 factoryGen(4) + pad(4) + void* resolved(8)
struct ExternalRef {
    void*    strPtr;        // +0x00: System::String data pointer
    int32_t  factoryGen;    // +0x08: factory generation counter (-1 = unresolved)
    uint8_t  _pad0C[4];     // +0x0C: padding
    void*    resolved;      // +0x10: resolved External* pointer
};
static_assert(sizeof(ExternalRef) == 0x18, "ExternalRef size mismatch");

// DynArray: generic dynamic array header (ptr + count + capacity)
// Layout: void* data(8) + int32 count(4) + int32 capacity(4) = 0x10 bytes
struct DynArrayHeader {
    void*    data;          // +0x00: pointer to element array
    int32_t  count;         // +0x08: number of elements
    int32_t  capacity;      // +0x0C: allocated capacity
};
static_assert(sizeof(DynArrayHeader) == 0x10, "DynArrayHeader size mismatch");

// BoolIntPair: element of the +0x60 array (8 bytes each)
struct BoolIntPair {
    uint8_t  flag;          // +0x00: bool
    uint8_t  _pad[3];       // +0x01: padding
    int32_t  value;         // +0x04: int
};
static_assert(sizeof(BoolIntPair) == 8, "BoolIntPair size mismatch");

struct MapGenerator {
    // --- Object::External base (0x20 bytes) ---
    void*    vtable;             // +0x00: -> 0x14015ed80
    uint8_t  _extBase[0x18];     // +0x08: External base fields

    // --- MapGenerator fields ---
    void*    _field20;           // +0x20: ExternalPtr (serialized via ExternalPtr_Serialize2)
    void*    _field28;           // +0x28: System::String
    void*    _field30;           // +0x30: System::String
    uint8_t  enabled;            // +0x38: bool, default 1
    uint8_t  _pad39[3];          // +0x39: padding
    int32_t  subdivisions;       // +0x3C: default 8 (grid subdivision count)
    float    scale;              // +0x40: default 2.0f (terrain scale)
    uint8_t  _pad44[4];          // +0x44: padding

    // +0x48: ExternalRef (String + factoryGen + resolvedPtr) — map template ref
    ExternalRef templateRef;     // +0x48 (0x18 bytes)

    // +0x60: DynArray of BoolIntPair (8-byte elements, raw MemAlloc/MemFree)
    void*    boolIntArray;       // +0x60: BoolIntPair* data
    uint64_t boolIntCount;       // +0x68: count (serialized as int32)

    // +0x70: DynArray of ExternalRef (0x18-byte elements) — resource list 1
    DynArrayHeader refArray1;    // +0x70 (ptr+count+cap = 0x10)

    // +0x80: DynArray of 0x30-byte elements — resource list 2 (complex entries)
    DynArrayHeader refArray2;    // +0x80 (ptr+count+cap = 0x10)

    // +0x90: DynArray of ExternalRef (0x18-byte) — resource list 3
    DynArrayHeader refArray3;    // +0x90

    // +0xA0: DynArray of ExternalRef (0x18-byte) — resource list 4
    DynArrayHeader refArray4;    // +0xA0

    // +0xB0: DynArray of ExternalRef (0x18-byte) — resource list 5
    DynArrayHeader refArray5;    // +0xB0

    // +0xC0: DynArray of ExternalRef (0x18-byte) — resource list 6
    DynArrayHeader refArray6;    // +0xC0

    // +0xD0: DynArray of ExternalRef (0x18-byte) — resource list 7
    DynArrayHeader refArray7;    // +0xD0

    // +0xE0: Scalar generation parameters
    int32_t  paramE0;            // +0xE0: int
    int32_t  paramE4;            // +0xE4: int
    int32_t  paramE8;            // +0xE8: int
    float    paramEC;            // +0xEC: float
    float    paramF0;            // +0xF0: float
    float    paramF4;            // +0xF4: float
    int32_t  paramF8;            // +0xF8: int
    int32_t  paramFC;            // +0xFC: int
    int32_t  param100;           // +0x100: int
    int32_t  param104;           // +0x104: int
    int32_t  param108;           // +0x108: int
    int32_t  param10C;           // +0x10C: int
    int32_t  param110;           // +0x110: int
    int32_t  param114;           // +0x114: int

    // +0x118: ExternalRef — reference (e.g., terrain description)
    ExternalRef descRef1;        // +0x118 (0x18 bytes)

    float    terrainAmplitude;   // +0x130: default 6.0f (0x40c00000)
    uint8_t  _pad134[4];         // +0x134: padding

    // +0x138: ExternalRef — reference (e.g., climate description)
    ExternalRef descRef2;        // +0x138 (0x18 bytes)

    int32_t  chunkSize;          // +0x150: default 16 (0x10)
    uint8_t  _pad154[4];         // +0x154: padding

    // +0x158: ExternalRef — reference (e.g., vegetation description)
    ExternalRef descRef3;        // +0x158 (0x18 bytes)

    float    param170;           // +0x170: float (last serialized field)
    uint8_t  _pad174[4];         // +0x174: padding to 0x178
};

static_assert(sizeof(MapGenerator) == 0x178, "MapGenerator size mismatch");
static_assert(offsetof(MapGenerator, vtable) == 0x00, "");
static_assert(offsetof(MapGenerator, _field20) == 0x20, "");
static_assert(offsetof(MapGenerator, enabled) == 0x38, "");
static_assert(offsetof(MapGenerator, subdivisions) == 0x3C, "");
static_assert(offsetof(MapGenerator, scale) == 0x40, "");
static_assert(offsetof(MapGenerator, templateRef) == 0x48, "");
static_assert(offsetof(MapGenerator, boolIntArray) == 0x60, "");
static_assert(offsetof(MapGenerator, boolIntCount) == 0x68, "");
static_assert(offsetof(MapGenerator, refArray1) == 0x70, "");
static_assert(offsetof(MapGenerator, refArray2) == 0x80, "");
static_assert(offsetof(MapGenerator, refArray3) == 0x90, "");
static_assert(offsetof(MapGenerator, refArray4) == 0xA0, "");
static_assert(offsetof(MapGenerator, refArray5) == 0xB0, "");
static_assert(offsetof(MapGenerator, refArray6) == 0xC0, "");
static_assert(offsetof(MapGenerator, refArray7) == 0xD0, "");
static_assert(offsetof(MapGenerator, paramE0) == 0xE0, "");
static_assert(offsetof(MapGenerator, paramEC) == 0xEC, "");
static_assert(offsetof(MapGenerator, param114) == 0x114, "");
static_assert(offsetof(MapGenerator, descRef1) == 0x118, "");
static_assert(offsetof(MapGenerator, terrainAmplitude) == 0x130, "");
static_assert(offsetof(MapGenerator, descRef2) == 0x138, "");
static_assert(offsetof(MapGenerator, chunkSize) == 0x150, "");
static_assert(offsetof(MapGenerator, descRef3) == 0x158, "");
static_assert(offsetof(MapGenerator, param170) == 0x170, "");
