#pragma once
// ============================================================================
// code/Game/Component/GrowthDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "GrowthDescription", parent "Description", ti 0x1401c77e8
//   create 0x140034b70: MemAlloc(0x68, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140034bb0.
//   table 0x140163610, 10 slots:
//     [0] 0x140034bf0 scalar deleting dtor   [1] 0x140034b60 TypeName   [2] 0x140034b50 Type
//     [3] 0x140120b00 GetFileVersion = 3   [4] External::Compile (import, inherited)
//     [5] 0x140120980 Serialize (`MOV AL,1`)
//     [6] 0x140034bc0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140034be0 GetComponentType: GrowthComponent's TypeId
//     [9] 0x140034bb0 Construct (folded with createInPlace)
//   sizeof 0x68.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class GrowthDescription final : public Description {
    DECLARE_EXE_TYPE(GrowthDescription)

public:
    GrowthDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140034bf0
    int32_t           GetFileVersion() const override { return 3; }    // [3] 0x140120b00
    bool              Serialize(void* stream) override;                    // [5] 0x140120980
    Object::External* Reconstruct() override;                              // [6] 0x140034bc0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140034be0
    virtual Object::External* Construct();                                 // [9] 0x140034bb0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    float _f20;                                 // +0x20
    float _f24;                                 // +0x24 ctor stores int 0
    float _f28;                                 // +0x28
    bool _b2c;                                  // +0x2c
    uint8_t _pad2d[3];                          // +0x2d never written by the ctor
    float _f30;                                 // +0x30
    float _f34;                                 // +0x34
    bool _b38;                                  // +0x38
    uint8_t _pad39[3];                          // +0x39 never written by the ctor
    float _f3c;                                 // +0x3c
    float _f40;                                 // +0x40
    bool _b44;                                  // +0x44
    uint8_t _pad45[3];                          // +0x45 never written by the ctor
    int32_t _i48;                               // +0x48
    int32_t _i4c;                               // +0x4c
    int32_t _i50;                               // +0x50
    float _f54;                                 // +0x54
    int32_t _i58;                               // +0x58
    int32_t _i5c;                               // +0x5c
    float _f60;                                 // +0x60 ctor stores int 0
    bool _b64;                                  // +0x64
    uint8_t _pad65[3];                          // +0x65 never written by the ctor

protected:
    ~GrowthDescription() = default;
};

static_assert(sizeof(GrowthDescription) == 0x68, "GrowthDescription is 0x68 (create 0x140034b70 MemAlloc(0x68))");
static_assert(alignof(GrowthDescription) == 8, "GrowthDescription alignment");
static_assert(offsetof(GrowthDescription, _f20) == 0x20, "GrowthDescription::_f20 @ +0x20");
static_assert(offsetof(GrowthDescription, _f24) == 0x24, "GrowthDescription::_f24 @ +0x24");
static_assert(offsetof(GrowthDescription, _f28) == 0x28, "GrowthDescription::_f28 @ +0x28");
static_assert(offsetof(GrowthDescription, _b2c) == 0x2c, "GrowthDescription::_b2c @ +0x2c");
static_assert(offsetof(GrowthDescription, _f30) == 0x30, "GrowthDescription::_f30 @ +0x30");
static_assert(offsetof(GrowthDescription, _f34) == 0x34, "GrowthDescription::_f34 @ +0x34");
static_assert(offsetof(GrowthDescription, _b38) == 0x38, "GrowthDescription::_b38 @ +0x38");
static_assert(offsetof(GrowthDescription, _f3c) == 0x3c, "GrowthDescription::_f3c @ +0x3c");
static_assert(offsetof(GrowthDescription, _f40) == 0x40, "GrowthDescription::_f40 @ +0x40");
static_assert(offsetof(GrowthDescription, _b44) == 0x44, "GrowthDescription::_b44 @ +0x44");
static_assert(offsetof(GrowthDescription, _i48) == 0x48, "GrowthDescription::_i48 @ +0x48");
static_assert(offsetof(GrowthDescription, _i4c) == 0x4c, "GrowthDescription::_i4c @ +0x4c");
static_assert(offsetof(GrowthDescription, _i50) == 0x50, "GrowthDescription::_i50 @ +0x50");
static_assert(offsetof(GrowthDescription, _f54) == 0x54, "GrowthDescription::_f54 @ +0x54");
static_assert(offsetof(GrowthDescription, _i58) == 0x58, "GrowthDescription::_i58 @ +0x58");
static_assert(offsetof(GrowthDescription, _i5c) == 0x5c, "GrowthDescription::_i5c @ +0x5c");
static_assert(offsetof(GrowthDescription, _f60) == 0x60, "GrowthDescription::_f60 @ +0x60");
static_assert(offsetof(GrowthDescription, _b64) == 0x64, "GrowthDescription::_b64 @ +0x64");
