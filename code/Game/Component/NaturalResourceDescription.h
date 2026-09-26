#pragma once
// ============================================================================
// code/Game/Component/NaturalResourceDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "NaturalResourceDescription", parent "Description", ti 0x1401c7b68
//   create 0x1400365d0: MemAlloc(0x88, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140036610.
//   table 0x140163e90, 10 slots:
//     [0] 0x140036650 scalar deleting dtor   [1] 0x1400365c0 TypeName   [2] 0x1400365b0 Type
//     [3] 0x140125fc0 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x140125e10 Serialize (`MOV AL,1`)
//     [6] 0x140036620 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140036640 GetComponentType: NaturalResourceComponent's TypeId
//     [9] 0x140036610 Construct (folded with createInPlace)
//   slot 0: the tree's hand-built slot 0 never ran ~External; the compiler chain now does (B38 D2, behaviour)
//   sizeof 0x88.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/Value.h"

class NaturalResourceDescription final : public Description {
    DECLARE_EXE_TYPE(NaturalResourceDescription)

public:
    NaturalResourceDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140036650
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x140125fc0
    bool              Serialize(void* stream) override;                    // [5] 0x140125e10
    Object::External* Reconstruct() override;                              // [6] 0x140036620
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140036640
    virtual Object::External* Construct();                                 // [9] 0x140036610

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _building;                            // +0x20 ExternalPtr<BuildingDescription> (0x140002340 serialize)
    float _f28;                                 // +0x28
    float _f2c;                                 // +0x2c
    float _f30;                                 // +0x30
    float _f34;                                 // +0x34
    float _f38;                                 // +0x38
    float _f3c;                                 // +0x3c
    float _f40;                                 // +0x40
    float _f44;                                 // +0x44 0.95
    bool _b48;                                  // +0x48
    uint8_t _pad49[3];                          // +0x49 never written by the ctor
    float _f4c;                                 // +0x4c
    float _f50;                                 // +0x50
    float _f54;                                 // +0x54
    float _f58;                                 // +0x58
    int32_t _i5c;                               // +0x5c
    float _f60;                                 // +0x60 high dword of the ctor's 8-byte +0x5c store
    bool _b64;                                  // +0x64
    uint8_t _pad65[3];                          // +0x65 never written by the ctor
    System::Time _time68;                       // +0x68 System::Time(1.0f)
    System::Time _time70;                       // +0x70 System::Time(1.0f)
    void* _profession;                          // +0x78 ExternalRef<Profession> (0x140018020 serialize)
    uint32_t _u80;                              // +0x80 enum, default 11
    uint8_t _pad84[4];                          // +0x84 never written by the ctor

protected:
    ~NaturalResourceDescription() = default;
};

static_assert(sizeof(NaturalResourceDescription) == 0x88, "NaturalResourceDescription is 0x88 (create 0x1400365d0 MemAlloc(0x88))");
static_assert(alignof(NaturalResourceDescription) == 8, "NaturalResourceDescription alignment");
static_assert(offsetof(NaturalResourceDescription, _building) == 0x20, "NaturalResourceDescription::_building @ +0x20");
static_assert(offsetof(NaturalResourceDescription, _f28) == 0x28, "NaturalResourceDescription::_f28 @ +0x28");
static_assert(offsetof(NaturalResourceDescription, _f2c) == 0x2c, "NaturalResourceDescription::_f2c @ +0x2c");
static_assert(offsetof(NaturalResourceDescription, _f30) == 0x30, "NaturalResourceDescription::_f30 @ +0x30");
static_assert(offsetof(NaturalResourceDescription, _f34) == 0x34, "NaturalResourceDescription::_f34 @ +0x34");
static_assert(offsetof(NaturalResourceDescription, _f38) == 0x38, "NaturalResourceDescription::_f38 @ +0x38");
static_assert(offsetof(NaturalResourceDescription, _f3c) == 0x3c, "NaturalResourceDescription::_f3c @ +0x3c");
static_assert(offsetof(NaturalResourceDescription, _f40) == 0x40, "NaturalResourceDescription::_f40 @ +0x40");
static_assert(offsetof(NaturalResourceDescription, _f44) == 0x44, "NaturalResourceDescription::_f44 @ +0x44");
static_assert(offsetof(NaturalResourceDescription, _b48) == 0x48, "NaturalResourceDescription::_b48 @ +0x48");
static_assert(offsetof(NaturalResourceDescription, _f4c) == 0x4c, "NaturalResourceDescription::_f4c @ +0x4c");
static_assert(offsetof(NaturalResourceDescription, _f50) == 0x50, "NaturalResourceDescription::_f50 @ +0x50");
static_assert(offsetof(NaturalResourceDescription, _f54) == 0x54, "NaturalResourceDescription::_f54 @ +0x54");
static_assert(offsetof(NaturalResourceDescription, _f58) == 0x58, "NaturalResourceDescription::_f58 @ +0x58");
static_assert(offsetof(NaturalResourceDescription, _i5c) == 0x5c, "NaturalResourceDescription::_i5c @ +0x5c");
static_assert(offsetof(NaturalResourceDescription, _f60) == 0x60, "NaturalResourceDescription::_f60 @ +0x60");
static_assert(offsetof(NaturalResourceDescription, _b64) == 0x64, "NaturalResourceDescription::_b64 @ +0x64");
static_assert(offsetof(NaturalResourceDescription, _time68) == 0x68, "NaturalResourceDescription::_time68 @ +0x68");
static_assert(offsetof(NaturalResourceDescription, _time70) == 0x70, "NaturalResourceDescription::_time70 @ +0x70");
static_assert(offsetof(NaturalResourceDescription, _profession) == 0x78, "NaturalResourceDescription::_profession @ +0x78");
static_assert(offsetof(NaturalResourceDescription, _u80) == 0x80, "NaturalResourceDescription::_u80 @ +0x80");
