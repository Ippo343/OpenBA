#pragma once
// ============================================================================
// code/Game/Game/TerrainGenerator.h — ENDGAME B.3.8 block LOOP (B38_RULES D4, sec. 2.2)
//
// `TerrainGenerator` (0x90): terrain height/noise generation parameters, a real
// `Object::External` subclass. Layout from ctor 0x1400b1810 + Serialize
// 0x1400b1690 + dtor.
//
// RETAIL (Application-x64.exe, B38_SLOTS.md sec. 6):
//   reflected "TerrainGenerator", parent "Object::External", table
//   0x14015f170, 9 slots, sizeof 0x90.
//   [0] 0x140020600 scalar deleting dtor   [1] 0x140020580 TypeName
//   [2] 0x140020570 Type                   [3] 0x1400b1800 GetFileVersion `MOV EAX,4`
//   [4] IMPORT Object::External::Compile   [5] 0x1400b1690 Serialize (AL=1)
//   [6] 0x1400205e0 Reconstruct            [7] 0x140001640 Destruct
//   [8] 0x1400205d0 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "object/External.h"
#include "Object/exe_type.h"

class TerrainGenerator final : public Object::External {
    DECLARE_EXE_TYPE(TerrainGenerator)

public:
    TerrainGenerator();                                           // 0x1400b1810

    ~TerrainGenerator() override;                                 // [0] 0x140020600
    int32_t GetFileVersion() const override { return 4; }         // [3] 0x1400b1800
    // [4] Compile: inherited (retail's slot is the Object::External import)
    bool Serialize(void* stream) override;                        // [5] 0x1400b1690
    Object::External* Reconstruct() override;                     // [6] 0x1400205e0
    void Destruct() override;                                     // [7] 0x140001640 this->~T()
    virtual Object::External* Construct();                        // [8] 0x1400205d0

public:
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

static_assert(sizeof(TerrainGenerator) == 0x90, "TerrainGenerator is 0x90");
static_assert(alignof(TerrainGenerator) == 8, "qword-aligned");
static_assert(offsetof(TerrainGenerator, _field20) == 0x20, "+0x20");
static_assert(offsetof(TerrainGenerator, _field30) == 0x30, "+0x30");
static_assert(offsetof(TerrainGenerator, _field38) == 0x38, "+0x38");
static_assert(offsetof(TerrainGenerator, _field50) == 0x50, "+0x50");
static_assert(offsetof(TerrainGenerator, _field68) == 0x68, "+0x68");
static_assert(offsetof(TerrainGenerator, _field80) == 0x80, "+0x80");
static_assert(offsetof(TerrainGenerator, _field88) == 0x88, "+0x88");
