#pragma once
// ============================================================================
// code/Game/Component/TerraformDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TerraformDescription", parent "Description", ti 0x1401c49f8
//   create 0x14000ccf0: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000cd60.
//   table 0x14015a900, 10 slots:
//     [0] 0x14000ce30 scalar deleting dtor   [1] 0x14000cce0 TypeName   [2] 0x14000ccd0 Type
//     [3] 0x140057730 GetFileVersion = 3   [4] External::Compile (import, inherited)
//     [5] 0x1400576a0 Serialize (`MOV AL,1`)
//     [6] 0x14000cdc0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000ce20 GetComponentType: TerraformComponent's TypeId
//     [9] 0x14000cd60 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class TerraformDescription final : public Description {
    DECLARE_EXE_TYPE(TerraformDescription)

public:
    TerraformDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x14000ce30
    int32_t           GetFileVersion() const override { return 3; }    // [3] 0x140057730
    bool              Serialize(void* stream) override;                    // [5] 0x1400576a0
    Object::External* Reconstruct() override;                              // [6] 0x14000cdc0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000ce20
    virtual Object::External* Construct();                                 // [9] 0x14000cd60

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _ref20;                               // +0x20 ExternalPtr (a raw pointer)
    void* _ref28;                               // +0x28 ExternalPtr (a raw pointer)
    uint32_t _u30;                              // +0x30 enum (serialized through the uint door)
    uint32_t _u34;                              // +0x34 enum (serialized through the uint door)

protected:
    ~TerraformDescription() = default;
};

static_assert(sizeof(TerraformDescription) == 0x38, "TerraformDescription is 0x38 (create 0x14000ccf0 MemAlloc(0x38))");
static_assert(alignof(TerraformDescription) == 8, "TerraformDescription alignment");
static_assert(offsetof(TerraformDescription, _ref20) == 0x20, "TerraformDescription::_ref20 @ +0x20");
static_assert(offsetof(TerraformDescription, _ref28) == 0x28, "TerraformDescription::_ref28 @ +0x28");
static_assert(offsetof(TerraformDescription, _u30) == 0x30, "TerraformDescription::_u30 @ +0x30");
static_assert(offsetof(TerraformDescription, _u34) == 0x34, "TerraformDescription::_u34 @ +0x34");
