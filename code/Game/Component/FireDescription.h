#pragma once
// ============================================================================
// code/Game/Component/FireDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "FireDescription", parent "Description", ti 0x1401c4218
//   create 0x140005640: MemAlloc(0x50, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140005680.
//   table 0x140159400, 10 slots:
//     [0] 0x1400056c0 scalar deleting dtor   [1] 0x140005630 TypeName   [2] 0x140005620 Type
//     [3] 0x140049d10 GetFileVersion = 4   [4] External::Compile (import, inherited)
//     [5] 0x140049c00 Serialize (`MOV AL,1`)
//     [6] 0x140005690 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400056b0 GetComponentType: FireComponent's TypeId
//     [9] 0x140005680 Construct (folded with createInPlace)
//   sizeof 0x50.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class FireDescription final : public Description {
    DECLARE_EXE_TYPE(FireDescription)

public:
    FireDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x1400056c0
    int32_t           GetFileVersion() const override { return 4; }    // [3] 0x140049d10
    bool              Serialize(void* stream) override;                    // [5] 0x140049c00
    Object::External* Reconstruct() override;                              // [6] 0x140005690
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400056b0
    virtual Object::External* Construct();                                 // [9] 0x140005680

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    float _f20;                                 // +0x20
    float _f24;                                 // +0x24
    int32_t _i28;                               // +0x28
    int32_t _i2c;                               // +0x2c
    float _f30;                                 // +0x30
    float _f34;                                 // +0x34
    int32_t _i38;                               // +0x38
    int32_t _i3c;                               // +0x3c
    int32_t _i40;                               // +0x40
    float _f44;                                 // +0x44
    float _f48;                                 // +0x48
    int32_t _i4c;                               // +0x4c zeroed, never serialized

protected:
    ~FireDescription() = default;
};

static_assert(sizeof(FireDescription) == 0x50, "FireDescription is 0x50 (create 0x140005640 MemAlloc(0x50))");
static_assert(alignof(FireDescription) == 8, "FireDescription alignment");
static_assert(offsetof(FireDescription, _f20) == 0x20, "FireDescription::_f20 @ +0x20");
static_assert(offsetof(FireDescription, _f24) == 0x24, "FireDescription::_f24 @ +0x24");
static_assert(offsetof(FireDescription, _i28) == 0x28, "FireDescription::_i28 @ +0x28");
static_assert(offsetof(FireDescription, _i2c) == 0x2c, "FireDescription::_i2c @ +0x2c");
static_assert(offsetof(FireDescription, _f30) == 0x30, "FireDescription::_f30 @ +0x30");
static_assert(offsetof(FireDescription, _f34) == 0x34, "FireDescription::_f34 @ +0x34");
static_assert(offsetof(FireDescription, _i38) == 0x38, "FireDescription::_i38 @ +0x38");
static_assert(offsetof(FireDescription, _i3c) == 0x3c, "FireDescription::_i3c @ +0x3c");
static_assert(offsetof(FireDescription, _i40) == 0x40, "FireDescription::_i40 @ +0x40");
static_assert(offsetof(FireDescription, _f44) == 0x44, "FireDescription::_f44 @ +0x44");
static_assert(offsetof(FireDescription, _f48) == 0x48, "FireDescription::_f48 @ +0x48");
static_assert(offsetof(FireDescription, _i4c) == 0x4c, "FireDescription::_i4c @ +0x4c");
