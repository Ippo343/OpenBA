#pragma once
// ============================================================================
// code/Game/Game/MapGenerator.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `MapGenerator` (0x178): world map generation parameters (a start condition:
// terrain settings, the initial families / structures / resource lists and the
// description-file refs), a real `Object::External` subclass. Layout from ctor
// 0x1400a3030 + Serialize 0x1400a2980 + dtor body 0x14001aea0.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md):
//   reflected "MapGenerator", parent "Object::External", table 0x14015ed80,
//   9 slots, sizeof 0x178.
//   [0] 0x14001ae70 scalar deleting dtor (body 0x14001aea0)
//   [1] 0x14001add0 TypeName               [2] 0x14001adc0 Type
//   [3] 0x1400a3020 GetFileVersion `MOV EAX,8`
//   [4] IMPORT Object::External::Compile   [5] 0x1400a2980 Serialize (AL=1)
//   [6] 0x14001ae30 Reconstruct            [7] 0x140001640 Destruct
//   [8] 0x14001ae20 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class MapGenerator final : public Object::External {
    DECLARE_EXE_TYPE(MapGenerator)

public:
    // An External reference stored as String + factoryGen + resolvedPtr (0x18).
    struct ExternalRef {
        void*    strPtr;        // +0x00: System::String data pointer
        int32_t  factoryGen;    // +0x08: factory generation counter (-1 = unresolved)
        uint8_t  _pad0C[4];     // +0x0C: padding
        void*    resolved;      // +0x10: resolved External* pointer
    };
    // Generic dynamic array header: data + count + capacity (0x10).
    struct DynArrayHeader {
        void*    data;          // +0x00: pointer to element array
        int32_t  count;         // +0x08: number of elements
        int32_t  capacity;      // +0x0C: allocated capacity
    };

    MapGenerator();                                               // 0x1400a3030

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x14001ae70
    int32_t GetFileVersion() const override { return 8; }         // [3] 0x1400a3020
    // [4] Compile: inherited (retail's slot is the Object::External import)
    bool Serialize(void* stream) override;                        // [5] 0x1400a2980
    Object::External* Reconstruct() override;                     // [6] 0x14001ae30
    void Destruct() override;                                     // [7] 0x140001640 DeletingDtor(0)
    virtual Object::External* Construct();                        // [8] 0x14001ae20

protected:
    ~MapGenerator();                                              // body 0x14001aea0

public:
    void*    _field20;           // +0x20: ExternalPtr (_stringTable)
    void*    _field28;           // +0x28: System::String (_name)
    void*    _field30;           // +0x30: System::String (_description)
    uint8_t  enabled;            // +0x38: bool, default 1 (_exposed)
    uint8_t  _pad39[3];          // +0x39: padding
    int32_t  subdivisions;       // +0x3C: default 8 (_startAreaSize)
    float    scale;              // +0x40: default 2.0f (_citizenRadius)
    uint8_t  _pad44[4];          // +0x44: padding

    ExternalRef templateRef;     // +0x48: _citizen template ref

    // +0x60: InitialFamily array (8-byte elements, raw MemAlloc/MemFree)
    void*    boolIntArray;       // +0x60: data
    uint64_t boolIntCount;       // +0x68: count (serialized as int32)

    DynArrayHeader refArray1;    // +0x70: ExternalRef (0x18) pool
    DynArrayHeader refArray2;    // +0x80: InitialStructure (0x30) pool
    DynArrayHeader refArray3;    // +0x90: ExternalRef pool
    DynArrayHeader refArray4;    // +0xA0: ExternalRef pool
    DynArrayHeader refArray5;    // +0xB0: ExternalRef pool
    DynArrayHeader refArray6;    // +0xC0: ExternalRef pool
    DynArrayHeader refArray7;    // +0xD0: ExternalRef pool

    // +0xE0: Scalar generation parameters
    int32_t  paramE0;            // +0xE0
    int32_t  paramE4;            // +0xE4
    int32_t  paramE8;            // +0xE8
    float    paramEC;            // +0xEC
    float    paramF0;            // +0xF0
    float    paramF4;            // +0xF4
    int32_t  paramF8;            // +0xF8
    int32_t  paramFC;            // +0xFC
    int32_t  param100;           // +0x100
    int32_t  param104;           // +0x104
    int32_t  param108;           // +0x108
    int32_t  param10C;           // +0x10C
    int32_t  param110;           // +0x110
    int32_t  param114;           // +0x114

    ExternalRef descRef1;        // +0x118: terrain description ref
    float    terrainAmplitude;   // +0x130: default 6.0f
    uint8_t  _pad134[4];         // +0x134
    ExternalRef descRef2;        // +0x138: climate description ref
    int32_t  chunkSize;          // +0x150: default 16
    uint8_t  _pad154[4];         // +0x154
    ExternalRef descRef3;        // +0x158: vegetation description ref
    float    param170;           // +0x170: last serialized field
    uint8_t  _pad174[4];         // +0x174: padding to 0x178
};

static_assert(sizeof(MapGenerator::ExternalRef) == 0x18, "ExternalRef is 0x18");
static_assert(sizeof(MapGenerator::DynArrayHeader) == 0x10, "DynArrayHeader is 0x10");
static_assert(sizeof(MapGenerator) == 0x178, "MapGenerator is 0x178");
static_assert(alignof(MapGenerator) == 8, "qword-aligned");
static_assert(offsetof(MapGenerator, _field20) == 0x20, "+0x20");
static_assert(offsetof(MapGenerator, enabled) == 0x38, "+0x38");
static_assert(offsetof(MapGenerator, subdivisions) == 0x3C, "+0x3C");
static_assert(offsetof(MapGenerator, scale) == 0x40, "+0x40");
static_assert(offsetof(MapGenerator, templateRef) == 0x48, "+0x48");
static_assert(offsetof(MapGenerator, boolIntArray) == 0x60, "+0x60");
static_assert(offsetof(MapGenerator, boolIntCount) == 0x68, "+0x68");
static_assert(offsetof(MapGenerator, refArray1) == 0x70, "+0x70");
static_assert(offsetof(MapGenerator, refArray2) == 0x80, "+0x80");
static_assert(offsetof(MapGenerator, refArray3) == 0x90, "+0x90");
static_assert(offsetof(MapGenerator, refArray4) == 0xA0, "+0xA0");
static_assert(offsetof(MapGenerator, refArray5) == 0xB0, "+0xB0");
static_assert(offsetof(MapGenerator, refArray6) == 0xC0, "+0xC0");
static_assert(offsetof(MapGenerator, refArray7) == 0xD0, "+0xD0");
static_assert(offsetof(MapGenerator, paramE0) == 0xE0, "+0xE0");
static_assert(offsetof(MapGenerator, paramEC) == 0xEC, "+0xEC");
static_assert(offsetof(MapGenerator, param114) == 0x114, "+0x114");
static_assert(offsetof(MapGenerator, descRef1) == 0x118, "+0x118");
static_assert(offsetof(MapGenerator, terrainAmplitude) == 0x130, "+0x130");
static_assert(offsetof(MapGenerator, descRef2) == 0x138, "+0x138");
static_assert(offsetof(MapGenerator, chunkSize) == 0x150, "+0x150");
static_assert(offsetof(MapGenerator, descRef3) == 0x158, "+0x158");
static_assert(offsetof(MapGenerator, param170) == 0x170, "+0x170");
