#pragma once
// ============================================================================
// code/Game/Game/ClimateGenerator.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `ClimateGenerator` (0x110): one climate's weather-generation parameters (an
// element of Game/Climate/ClimateList.rsc), a real `Object::External` subclass.
// Field layout from ctor 0x1400930c0 + Serialize 0x140092fa0.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md sec. 6):
//   reflected "ClimateGenerator", parent "Object::External", table
//   0x14015e140, 9 slots, sizeof 0x110.
//   [0] 0x140018f60 scalar deleting dtor (~String(+0x28), ~External, MemFree)
//   [1] 0x140018ee0 TypeName   [2] 0x140018ed0 Type
//   [3] 0x1400930b0 GetFileVersion `MOV EAX,3` (External::Load gates the
//       element Serialize on it: a wrong constant silently drops the element)
//   [4] IMPORT Object::External::Compile
//   [5] 0x140092fa0 Serialize (AL=1)
//   [6] 0x140018f40 Reconstruct (DeletingDtor(0), ctor again)
//   [7] 0x140001640 Destruct (DeletingDtor(0), the shared body)
//   [8] 0x140018f30 Construct (placement-new, null-checked)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class ClimateGenerator final : public Object::External {
    DECLARE_EXE_TYPE(ClimateGenerator)

public:
    ClimateGenerator();                                           // 0x1400930c0

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140018f60
    int32_t GetFileVersion() const override { return 3; }         // [3] 0x1400930b0
    CompileResult Compile(const System::Filename& path) override; // [4] PORT GAP (tree stub; retail inherits)
    bool Serialize(void* stream) override;                        // [5] 0x140092fa0
    Object::External* Reconstruct() override;                     // [6] 0x140018f40
    void Destruct() override;                                     // [7] 0x140001640 PORT GAP
    virtual Object::External* Construct();                        // [8] 0x140018f30

protected:
    ~ClimateGenerator();

public:
    void*    _field20;           // +0x20: ExternalPtr (serialized via FUN_14000bf40)
    void*    _field28;           // +0x28: System::String (8-byte COW, name/reference)
    uint8_t  _field30;           // +0x30: 1 (bool, enabled?)
    uint8_t  _pad31[3];          // +0x31: padding
    float    temperatureBase;    // +0x34: 50.0f (base temp in F?)
    float    tempVariation;      // +0x38: 30.0f
    float    precipitation;      // +0x3C: 5.0f
    float    windSpeed;          // +0x40: 10.0f
    uint8_t  _climateData[0xC0]; // +0x44: 48 floats (seasonal/biome data, memset to 0)
    float    _field104;          // +0x104: 0.0f (serialized as float)
    float    _field108;          // +0x108: 1.0f
    float    _field10C;          // +0x10C: 1.0f
};

static_assert(sizeof(ClimateGenerator) == 0x110, "ClimateGenerator is 0x110");
static_assert(alignof(ClimateGenerator) == 8, "qword-aligned");
static_assert(offsetof(ClimateGenerator, _field20) == 0x20, "+0x20");
static_assert(offsetof(ClimateGenerator, _field28) == 0x28, "+0x28");
static_assert(offsetof(ClimateGenerator, _field30) == 0x30, "+0x30");
static_assert(offsetof(ClimateGenerator, temperatureBase) == 0x34, "+0x34");
static_assert(offsetof(ClimateGenerator, windSpeed) == 0x40, "+0x40");
static_assert(offsetof(ClimateGenerator, _climateData) == 0x44, "+0x44");
static_assert(offsetof(ClimateGenerator, _field104) == 0x104, "+0x104");
static_assert(offsetof(ClimateGenerator, _field108) == 0x108, "+0x108");
static_assert(offsetof(ClimateGenerator, _field10C) == 0x10C, "+0x10C");
