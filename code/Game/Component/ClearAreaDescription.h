#pragma once
// ============================================================================
// code/Game/Component/ClearAreaDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ClearAreaDescription", parent "Description", ti 0x1401c7200
//   create 0x140030f00: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140030f80.
//   table 0x140162760, 10 slots:
//     [0] 0x140031060 scalar deleting dtor   [1] 0x140030ef0 TypeName   [2] 0x140030ee0 Type
//     [3] 0x1401181c0 GetFileVersion = 6   [4] External::Compile (import, inherited)
//     [5] 0x1401180f0 Serialize (`MOV AL,1`)
//     [6] 0x140030fe0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140031050 GetComponentType: ClearAreaComponent's TypeId
//     [9] 0x140030f80 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ClearAreaDescription final : public Description {
    DECLARE_EXE_TYPE(ClearAreaDescription)

public:
    ClearAreaDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140031060
    int32_t           GetFileVersion() const override { return 6; }    // [3] 0x1401181c0
    bool              Serialize(void* stream) override;                    // [5] 0x1401180f0
    Object::External* Reconstruct() override;                              // [6] 0x140030fe0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140031050
    virtual Object::External* Construct();                                 // [9] 0x140030f80

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _descRef;                             // +0x20 ExternalPtr (Description)
    bool _flag0;                                // +0x28
    bool _flag1;                                // +0x29
    bool _flag2;                                // +0x2a
    bool _flag3;                                // +0x2b
    bool _flag4;                                // +0x2c
    bool _flag5;                                // +0x2d
    uint8_t _pad2e[2];                          // +0x2e never written by the ctor
    int32_t _value;                             // +0x30
    uint8_t _pad34[4];                          // +0x34 never written by the ctor

protected:
    ~ClearAreaDescription() = default;
};

static_assert(sizeof(ClearAreaDescription) == 0x38, "ClearAreaDescription is 0x38 (create 0x140030f00 MemAlloc(0x38))");
static_assert(alignof(ClearAreaDescription) == 8, "ClearAreaDescription alignment");
static_assert(offsetof(ClearAreaDescription, _descRef) == 0x20, "ClearAreaDescription::_descRef @ +0x20");
static_assert(offsetof(ClearAreaDescription, _flag0) == 0x28, "ClearAreaDescription::_flag0 @ +0x28");
static_assert(offsetof(ClearAreaDescription, _flag1) == 0x29, "ClearAreaDescription::_flag1 @ +0x29");
static_assert(offsetof(ClearAreaDescription, _flag2) == 0x2a, "ClearAreaDescription::_flag2 @ +0x2a");
static_assert(offsetof(ClearAreaDescription, _flag3) == 0x2b, "ClearAreaDescription::_flag3 @ +0x2b");
static_assert(offsetof(ClearAreaDescription, _flag4) == 0x2c, "ClearAreaDescription::_flag4 @ +0x2c");
static_assert(offsetof(ClearAreaDescription, _flag5) == 0x2d, "ClearAreaDescription::_flag5 @ +0x2d");
static_assert(offsetof(ClearAreaDescription, _value) == 0x30, "ClearAreaDescription::_value @ +0x30");
