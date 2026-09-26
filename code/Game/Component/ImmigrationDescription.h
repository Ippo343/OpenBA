#pragma once
// ============================================================================
// code/Game/Component/ImmigrationDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ImmigrationDescription", parent "Description", ti 0x1401c44b8
//   create 0x140007a80: MemAlloc(0x68, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140007ac0.
//   table 0x140159bc0, 10 slots:
//     [0] 0x140007b00 scalar deleting dtor   [1] 0x140007a70 TypeName   [2] 0x140007a60 Type
//     [3] 0x14004e930 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14004e7f0 Serialize (`MOV AL,1`)
//     [6] 0x140007ad0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140007af0 GetComponentType: ImmigrationComponent's TypeId
//     [9] 0x140007ac0 Construct (folded with createInPlace)
//   sizeof 0x68.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ImmigrationDescription final : public Description {
    DECLARE_EXE_TYPE(ImmigrationDescription)

public:
    ImmigrationDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140007b00
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14004e930
    bool              Serialize(void* stream) override;                    // [5] 0x14004e7f0
    Object::External* Reconstruct() override;                              // [6] 0x140007ad0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140007af0
    virtual Object::External* Construct();                                 // [9] 0x140007ac0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _ref20;                               // +0x20 ExternalPtr (a raw pointer)
    int32_t _i28;                               // +0x28
    uint8_t _pad2c[4];                          // +0x2c never written by the ctor
    void* _refs;                                // +0x30 External* array data
    int32_t _refCount;                          // +0x38
    int32_t _refCap;                            // +0x3c
    float _f40;                                 // +0x40
    float _f44;                                 // +0x44
    float _f48;                                 // +0x48
    float _f4c;                                 // +0x4c
    int32_t _i50;                               // +0x50
    int32_t _i54;                               // +0x54
    int32_t _i58;                               // +0x58
    int32_t _i5c;                               // +0x5c
    float _f60;                                 // +0x60
    int32_t _i64;                               // +0x64

protected:
    ~ImmigrationDescription();
};

static_assert(sizeof(ImmigrationDescription) == 0x68, "ImmigrationDescription is 0x68 (create 0x140007a80 MemAlloc(0x68))");
static_assert(alignof(ImmigrationDescription) == 8, "ImmigrationDescription alignment");
static_assert(offsetof(ImmigrationDescription, _ref20) == 0x20, "ImmigrationDescription::_ref20 @ +0x20");
static_assert(offsetof(ImmigrationDescription, _i28) == 0x28, "ImmigrationDescription::_i28 @ +0x28");
static_assert(offsetof(ImmigrationDescription, _refs) == 0x30, "ImmigrationDescription::_refs @ +0x30");
static_assert(offsetof(ImmigrationDescription, _refCount) == 0x38, "ImmigrationDescription::_refCount @ +0x38");
static_assert(offsetof(ImmigrationDescription, _refCap) == 0x3c, "ImmigrationDescription::_refCap @ +0x3c");
static_assert(offsetof(ImmigrationDescription, _f40) == 0x40, "ImmigrationDescription::_f40 @ +0x40");
static_assert(offsetof(ImmigrationDescription, _f44) == 0x44, "ImmigrationDescription::_f44 @ +0x44");
static_assert(offsetof(ImmigrationDescription, _f48) == 0x48, "ImmigrationDescription::_f48 @ +0x48");
static_assert(offsetof(ImmigrationDescription, _f4c) == 0x4c, "ImmigrationDescription::_f4c @ +0x4c");
static_assert(offsetof(ImmigrationDescription, _i50) == 0x50, "ImmigrationDescription::_i50 @ +0x50");
static_assert(offsetof(ImmigrationDescription, _i54) == 0x54, "ImmigrationDescription::_i54 @ +0x54");
static_assert(offsetof(ImmigrationDescription, _i58) == 0x58, "ImmigrationDescription::_i58 @ +0x58");
static_assert(offsetof(ImmigrationDescription, _i5c) == 0x5c, "ImmigrationDescription::_i5c @ +0x5c");
static_assert(offsetof(ImmigrationDescription, _f60) == 0x60, "ImmigrationDescription::_f60 @ +0x60");
static_assert(offsetof(ImmigrationDescription, _i64) == 0x64, "ImmigrationDescription::_i64 @ +0x64");
