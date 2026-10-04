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
#include "system/String.h"

namespace UI { class StringTable; }

class MapGenerator final : public Object::External {
    DECLARE_EXE_TYPE(MapGenerator)

public:
    // An External reference stored as String + factoryGen + resolvedPtr (0x18).
    // B4 P2 GAME-3 (H5): the name is a real System::String. Retail's inlined
    // ExternalRef ctor in 0x1400a3030 stores qword 0 (String()), dword -1, qword 0;
    // its dtor (0x14001aea0, per ref) is ~String alone. The field keeps the name
    // `strPtr` its readers use.
    struct ExternalRef {
        System::String strPtr;  // +0x00: the referenced External's name
        int32_t  factoryGen;    // +0x08: factory generation counter (-1 = unresolved)
        uint8_t  _pad0C[4];     // +0x0C: padding
        ::Object::External* resolved;   // +0x10: the resolved External (each pool's element class; readers downcast)
    };
    // Generic dynamic array header: data + count + capacity (0x10), typed by its
    // element (B.4 GAME-c: the element per pool is the stride retail's dtor
    // 0x14001aea0 destroys it with: 0x14001b030 = 0x18 ExternalRef, 0x14001b0a0 =
    // 0x30 InitialStructure).
    // B4 P2 GAME-3: the pool owns its elements the way retail's member arrays
    // do: the ctor (0x1400a3030) zeroes both qwords, and each pool's destructor
    // is the one retail's dtor 0x14001aea0 runs at its offset, in member order
    // (defined per element type in mapgenerator.cpp).
    template <class T>
    struct DynArrayHeader {
        T*       data = nullptr;  // +0x00: pointer to element array
        int32_t  count = 0;       // +0x08: number of elements
        int32_t  capacity = 0;    // +0x0C: allocated capacity
        ~DynArrayHeader();
    };

    // One _citizens entry (stride 8, 0x14001b270; Serialize: bool, int).
    struct InitialFamily {
        bool     educated;        // +0x00 _educated
        uint8_t  _pad01[3];       // +0x01 never written by the resize's init
        int32_t  maxChildCount;   // +0x04 _maxChildCount
    };
    // One _structures entry (stride 0x30, 0x14001b4e0).
    struct InitialStructure {
        ExternalRef structure;    // +0x00 _structure
        int32_t  offsetX;         // +0x18 _offsetX
        int32_t  offsetY;         // +0x1C _offsetY
        int32_t  width;           // +0x20 _width (init -1)
        int32_t  height;          // +0x24 _height (init -1)
        int32_t  rotation;        // +0x28 _rotation
        uint8_t  _pad2C[4];       // +0x2C
    };

    MapGenerator();                                               // 0x1400a3030

    // body 0x14001aea0
    ~MapGenerator() override;                                     // [0] 0x14001ae70
    int32_t GetFileVersion() const override { return 8; }         // [3] 0x1400a3020
    // [4] Compile: inherited (retail's slot is the Object::External import)
    bool Serialize(IO::Stream& stream) override;                        // [5] 0x1400a2980
    Object::External* Reconstruct() override;                     // [6] 0x14001ae30
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x14001ae20

public:
    UI::StringTable* _field20;   // +0x20: ExternalPtr<StringTable> (_stringTable)
    System::String _field28;     // +0x28: _name (B.4 GAME-b: the ctor stored 0, the dtor ran ~String last)
    System::String _field30;     // +0x30: _description
    uint8_t  enabled;            // +0x38: bool, default 1 (_exposed)
    uint8_t  _pad39[3];          // +0x39: padding
    int32_t  subdivisions;       // +0x3C: default 8 (_startAreaSize)
    float    scale;              // +0x40: default 2.0f (_citizenRadius)
    uint8_t  _pad44[4];          // +0x44: padding

    ExternalRef templateRef;     // +0x48: _citizen template ref

    // +0x60: InitialFamily array (8-byte elements, raw MemAlloc/MemFree); the
    // count is retail's int32 at +0x68, the capacity +0x6C.
    DynArrayHeader<InitialFamily> families;  // +0x60 _citizens (dtor 0x14001af47: MemFree, zero both qwords)

    DynArrayHeader<ExternalRef>      refArray1;   // +0x70: ExternalRef (0x18) pool (dtor 0x14001af41 -> 0x14001b030)
    DynArrayHeader<InitialStructure> refArray2;   // +0x80: InitialStructure (0x30) pool (dtor 0x14001af37 -> 0x14001b0a0)
    DynArrayHeader<ExternalRef>      refArray3;   // +0x90: ExternalRef pool (dtor 0x14001af2a -> 0x14001b030)
    DynArrayHeader<ExternalRef>      refArray4;   // +0xA0: ExternalRef pool (dtor 0x14001af1d -> 0x14001b030)
    DynArrayHeader<ExternalRef>      refArray5;   // +0xB0: ExternalRef pool (dtor 0x14001af10 -> 0x14001b030)
    DynArrayHeader<ExternalRef>      refArray6;   // +0xC0: ExternalRef pool (dtor 0x14001af03 -> 0x14001b030)
    DynArrayHeader<ExternalRef>      refArray7;   // +0xD0: ExternalRef pool (dtor 0x14001aef6 -> 0x14001b030)

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
static_assert(sizeof(MapGenerator::DynArrayHeader<MapGenerator::ExternalRef>) == 0x10, "DynArrayHeader is 0x10");
static_assert(sizeof(MapGenerator::DynArrayHeader<MapGenerator::InitialStructure>) == 0x10, "DynArrayHeader is 0x10");
static_assert(sizeof(MapGenerator::ExternalRef) == 0x18, "ExternalRef stride 0x18 (0x14001b030)");
static_assert(sizeof(MapGenerator::InitialStructure) == 0x30, "InitialStructure stride 0x30 (0x14001b0a0)");
static_assert(sizeof(MapGenerator) == 0x178, "MapGenerator is 0x178");
static_assert(alignof(MapGenerator) == 8, "qword-aligned");
static_assert(offsetof(MapGenerator, _field20) == 0x20, "+0x20");
static_assert(offsetof(MapGenerator, _field28) == 0x28, "+0x28");
static_assert(offsetof(MapGenerator, _field30) == 0x30, "+0x30");
static_assert(sizeof(MapGenerator::InitialFamily) == 8, "InitialFamily stride 8 (0x14001b270)");
static_assert(offsetof(MapGenerator::InitialFamily, maxChildCount) == 4, "InitialFamily +0x04");
static_assert(sizeof(MapGenerator::InitialStructure) == 0x30, "InitialStructure stride 0x30 (0x14001b4e0)");
static_assert(offsetof(MapGenerator::InitialStructure, offsetX) == 0x18, "InitialStructure +0x18");
static_assert(offsetof(MapGenerator::InitialStructure, rotation) == 0x28, "InitialStructure +0x28");
static_assert(offsetof(MapGenerator, enabled) == 0x38, "+0x38");
static_assert(offsetof(MapGenerator, subdivisions) == 0x3C, "+0x3C");
static_assert(offsetof(MapGenerator, scale) == 0x40, "+0x40");
static_assert(offsetof(MapGenerator, templateRef) == 0x48, "+0x48");
static_assert(offsetof(MapGenerator, families) == 0x60, "+0x60");
static_assert(offsetof(MapGenerator::DynArrayHeader<MapGenerator::InitialFamily>, count) == 0x08, "+0x68 count");
static_assert(offsetof(MapGenerator::ExternalRef, factoryGen) == 0x08, "ExternalRef +0x08");
static_assert(offsetof(MapGenerator::ExternalRef, resolved) == 0x10, "ExternalRef +0x10");

// The three pool destructors (mapgenerator.cpp), declared before any use.
template <> MapGenerator::DynArrayHeader<MapGenerator::InitialFamily>::~DynArrayHeader();
template <> MapGenerator::DynArrayHeader<MapGenerator::ExternalRef>::~DynArrayHeader();
template <> MapGenerator::DynArrayHeader<MapGenerator::InitialStructure>::~DynArrayHeader();
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
