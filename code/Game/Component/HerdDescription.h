#pragma once
// ============================================================================
// code/Game/Component/HerdDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HerdDescription", parent "Description", ti 0x1401c7858
//   create 0x140034d30: MemAlloc(0x40, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140034d90.
//   table 0x140163700, 10 slots:
//     [0] 0x140034e40 scalar deleting dtor   [1] 0x140034d20 TypeName   [2] 0x140034d10 Type
//     [3] 0x1401222a0 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x1401221e0 Serialize (`MOV AL,1`)
//     [6] 0x140034de0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140034e30 GetComponentType: HerdComponent's TypeId
//     [9] 0x140034d90 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class HerdDescription final : public Description {
    DECLARE_EXE_TYPE(HerdDescription)

public:
    HerdDescription();

    ~HerdDescription() override = default;                                 // [0] 0x140034e40
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x1401222a0
    bool              Serialize(void* stream) override;                    // [5] 0x1401221e0
    Object::External* Reconstruct() override;                              // [6] 0x140034de0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140034e30
    virtual Object::External* Construct();                                 // [9] 0x140034d90

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _ref20;                               // +0x20 ExternalPtr (a raw pointer)
    int32_t _i28;                               // +0x28
    int32_t _i2c;                               // +0x2c
    float _f30;                                 // +0x30
    float _f34;                                 // +0x34
    int32_t _i38;                               // +0x38
    float _f3c;                                 // +0x3c
};

static_assert(sizeof(HerdDescription) == 0x40, "HerdDescription is 0x40 (create 0x140034d30 MemAlloc(0x40))");
static_assert(alignof(HerdDescription) == 8, "HerdDescription alignment");
static_assert(offsetof(HerdDescription, _ref20) == 0x20, "HerdDescription::_ref20 @ +0x20");
static_assert(offsetof(HerdDescription, _i28) == 0x28, "HerdDescription::_i28 @ +0x28");
static_assert(offsetof(HerdDescription, _i2c) == 0x2c, "HerdDescription::_i2c @ +0x2c");
static_assert(offsetof(HerdDescription, _f30) == 0x30, "HerdDescription::_f30 @ +0x30");
static_assert(offsetof(HerdDescription, _f34) == 0x34, "HerdDescription::_f34 @ +0x34");
static_assert(offsetof(HerdDescription, _i38) == 0x38, "HerdDescription::_i38 @ +0x38");
static_assert(offsetof(HerdDescription, _f3c) == 0x3c, "HerdDescription::_f3c @ +0x3c");
