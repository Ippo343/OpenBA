#pragma once
// ============================================================================
// code/Game/Component/PopulationDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PopulationDescription", parent "Description", ti 0x1401c47c8
//   create 0x14000afb0: MemAlloc(0x58, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000aff0.
//   table 0x14015a3e0, 10 slots:
//     [0] 0x14000b030 scalar deleting dtor   [1] 0x14000afa0 TypeName   [2] 0x14000af90 Type
//     [3] 0x1400548e0 GetFileVersion = 3   [4] External::Compile (import, inherited)
//     [5] 0x1400547e0 Serialize (`MOV AL,1`)
//     [6] 0x14000b000 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000b020 GetComponentType: PopulationComponent's TypeId
//     [9] 0x14000aff0 Construct (folded with createInPlace)
//   +0x20 System::String: the member's own ctor/dtor are retail's inlined String() / ~String()
//   sizeof 0x58.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/String.h"

class PopulationDescription final : public Description {
    DECLARE_EXE_TYPE(PopulationDescription)

public:
    PopulationDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x14000b030
    int32_t           GetFileVersion() const override { return 3; }    // [3] 0x1400548e0
    bool              Serialize(void* stream) override;                    // [5] 0x1400547e0
    Object::External* Reconstruct() override;                              // [6] 0x14000b000
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000b020
    virtual Object::External* Construct();                                 // [9] 0x14000aff0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    System::String _name;                       // +0x20 String (empty)
    int32_t _i28;                               // +0x28
    int32_t _i2c;                               // +0x2c
    uint32_t _u30;                              // +0x30 enum (serialized through the uint door)
    uint32_t _u34;                              // +0x34 enum (serialized through the uint door)
    bool _b38;                                  // +0x38
    uint8_t _pad39[3];                          // +0x39 never written by the ctor
    int32_t _i3c;                               // +0x3c
    int32_t _i40;                               // +0x40
    int32_t _i44;                               // +0x44
    bool _b48;                                  // +0x48
    uint8_t _pad49[7];                          // +0x49 never written by the ctor
    void* _ref50;                               // +0x50 ExternalPtr (a raw pointer)

protected:
    ~PopulationDescription() = default;
};

static_assert(sizeof(PopulationDescription) == 0x58, "PopulationDescription is 0x58 (create 0x14000afb0 MemAlloc(0x58))");
static_assert(alignof(PopulationDescription) == 8, "PopulationDescription alignment");
static_assert(offsetof(PopulationDescription, _name) == 0x20, "PopulationDescription::_name @ +0x20");
static_assert(offsetof(PopulationDescription, _i28) == 0x28, "PopulationDescription::_i28 @ +0x28");
static_assert(offsetof(PopulationDescription, _i2c) == 0x2c, "PopulationDescription::_i2c @ +0x2c");
static_assert(offsetof(PopulationDescription, _u30) == 0x30, "PopulationDescription::_u30 @ +0x30");
static_assert(offsetof(PopulationDescription, _u34) == 0x34, "PopulationDescription::_u34 @ +0x34");
static_assert(offsetof(PopulationDescription, _b38) == 0x38, "PopulationDescription::_b38 @ +0x38");
static_assert(offsetof(PopulationDescription, _i3c) == 0x3c, "PopulationDescription::_i3c @ +0x3c");
static_assert(offsetof(PopulationDescription, _i40) == 0x40, "PopulationDescription::_i40 @ +0x40");
static_assert(offsetof(PopulationDescription, _i44) == 0x44, "PopulationDescription::_i44 @ +0x44");
static_assert(offsetof(PopulationDescription, _b48) == 0x48, "PopulationDescription::_b48 @ +0x48");
static_assert(offsetof(PopulationDescription, _ref50) == 0x50, "PopulationDescription::_ref50 @ +0x50");
