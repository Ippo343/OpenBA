#pragma once
// ============================================================================
// code/Game/Component/AnimalDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AnimalDescription", parent "Description", ti 0x1401c6fd0
//   create 0x14002f080: MemAlloc(0x58, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14002f0c0.
//   table 0x140162190, 10 slots:
//     [0] 0x14002f100 scalar deleting dtor   [1] 0x14002f070 TypeName   [2] 0x14002f060 Type
//     [3] 0x140107170 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x140107050 Serialize (`MOV AL,1`)
//     [6] 0x14002f0d0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14002f0f0 GetComponentType: AnimalComponent's TypeId
//     [9] 0x14002f0c0 Construct (folded with createInPlace)
//   sizeof 0x58.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class AnimalDescription final : public Description {
    DECLARE_EXE_TYPE(AnimalDescription)

public:
    AnimalDescription();

    ~AnimalDescription() override;                                         // [0] 0x14002f100
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x140107170
    bool              Serialize(void* stream) override;                    // [5] 0x140107050
    Object::External* Reconstruct() override;                              // [6] 0x14002f0d0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14002f0f0
    virtual Object::External* Construct();                                 // [9] 0x14002f0c0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    int32_t _i20;                               // +0x20
    int32_t _i24;                               // +0x24
    float _f28;                                 // +0x28
    float _f2c;                                 // +0x2c
    float _f30;                                 // +0x30
    float _f34;                                 // +0x34
    float _f38;                                 // +0x38
    bool _b3c;                                  // +0x3c
    uint8_t _pad3d[3];                          // +0x3d never written by the ctor
    float _f40;                                 // +0x40
    uint8_t _pad44[4];                          // +0x44 never written by the ctor
    void* _refs;                                // +0x48 External* array data
    int32_t _refCount;                          // +0x50
    int32_t _refCap;                            // +0x54
};

static_assert(sizeof(AnimalDescription) == 0x58, "AnimalDescription is 0x58 (create 0x14002f080 MemAlloc(0x58))");
static_assert(alignof(AnimalDescription) == 8, "AnimalDescription alignment");
static_assert(offsetof(AnimalDescription, _i20) == 0x20, "AnimalDescription::_i20 @ +0x20");
static_assert(offsetof(AnimalDescription, _i24) == 0x24, "AnimalDescription::_i24 @ +0x24");
static_assert(offsetof(AnimalDescription, _f28) == 0x28, "AnimalDescription::_f28 @ +0x28");
static_assert(offsetof(AnimalDescription, _f2c) == 0x2c, "AnimalDescription::_f2c @ +0x2c");
static_assert(offsetof(AnimalDescription, _f30) == 0x30, "AnimalDescription::_f30 @ +0x30");
static_assert(offsetof(AnimalDescription, _f34) == 0x34, "AnimalDescription::_f34 @ +0x34");
static_assert(offsetof(AnimalDescription, _f38) == 0x38, "AnimalDescription::_f38 @ +0x38");
static_assert(offsetof(AnimalDescription, _b3c) == 0x3c, "AnimalDescription::_b3c @ +0x3c");
static_assert(offsetof(AnimalDescription, _f40) == 0x40, "AnimalDescription::_f40 @ +0x40");
static_assert(offsetof(AnimalDescription, _refs) == 0x48, "AnimalDescription::_refs @ +0x48");
static_assert(offsetof(AnimalDescription, _refCount) == 0x50, "AnimalDescription::_refCount @ +0x50");
static_assert(offsetof(AnimalDescription, _refCap) == 0x54, "AnimalDescription::_refCap @ +0x54");
