#pragma once
// ============================================================================
// code/Game/Component/ConsumeProduceDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "ConsumeProduceDescription", parent "Description", ti 0x1401c7388
//   create 0x140032360: MemAlloc(0x58, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x1400323a0.
//   table 0x140162b30, 10 slots:
//     [0] 0x1400323e0 scalar deleting dtor   [1] 0x140032350 TypeName   [2] 0x140032340 Type
//     [3] 0x140119200 GetFileVersion = 12   [4] External::Compile (import, inherited)
//     [5] 0x1401190e0 Serialize (`MOV AL,1`)
//     [6] 0x1400323b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400323d0 GetComponentType: ConsumeProduceComponent's TypeId
//     [9] 0x1400323a0 Construct (folded with createInPlace)
//   sizeof 0x58.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ConsumeProduceDescription final : public Description {
    DECLARE_EXE_TYPE(ConsumeProduceDescription)

public:
    ConsumeProduceDescription();

    ~ConsumeProduceDescription() override;                                 // [0] 0x1400323e0
    int32_t           GetFileVersion() const override { return 12; }    // [3] 0x140119200
    bool              Serialize(void* stream) override;                    // [5] 0x1401190e0
    Object::External* Reconstruct() override;                              // [6] 0x1400323b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400323d0
    virtual Object::External* Construct();                                 // [9] 0x1400323a0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _ref20;                               // +0x20 ExternalPtr (a raw pointer)
    void* _recipes;                             // +0x28 recipe array data (0x28 stride)
    int32_t _recipeCount;                       // +0x30
    int32_t _recipeCap;                         // +0x34
    uint32_t _toolType;                         // +0x38 Object::Type (the tool's type id; serialized as a name)
    int32_t _i3c;                               // +0x3c
    int32_t _i40;                               // +0x40
    float _f44;                                 // +0x44
    int32_t _i48;                               // +0x48
    int32_t _i4c;                               // +0x4c
    uint32_t _u50;                              // +0x50 enum (serialized through the uint door)
    uint8_t _pad54[4];                          // +0x54 never written by the ctor
};

static_assert(sizeof(ConsumeProduceDescription) == 0x58, "ConsumeProduceDescription is 0x58 (create 0x140032360 MemAlloc(0x58))");
static_assert(alignof(ConsumeProduceDescription) == 8, "ConsumeProduceDescription alignment");
static_assert(offsetof(ConsumeProduceDescription, _ref20) == 0x20, "ConsumeProduceDescription::_ref20 @ +0x20");
static_assert(offsetof(ConsumeProduceDescription, _recipes) == 0x28, "ConsumeProduceDescription::_recipes @ +0x28");
static_assert(offsetof(ConsumeProduceDescription, _recipeCount) == 0x30, "ConsumeProduceDescription::_recipeCount @ +0x30");
static_assert(offsetof(ConsumeProduceDescription, _recipeCap) == 0x34, "ConsumeProduceDescription::_recipeCap @ +0x34");
static_assert(offsetof(ConsumeProduceDescription, _toolType) == 0x38, "ConsumeProduceDescription::_toolType @ +0x38");
static_assert(offsetof(ConsumeProduceDescription, _i3c) == 0x3c, "ConsumeProduceDescription::_i3c @ +0x3c");
static_assert(offsetof(ConsumeProduceDescription, _i40) == 0x40, "ConsumeProduceDescription::_i40 @ +0x40");
static_assert(offsetof(ConsumeProduceDescription, _f44) == 0x44, "ConsumeProduceDescription::_f44 @ +0x44");
static_assert(offsetof(ConsumeProduceDescription, _i48) == 0x48, "ConsumeProduceDescription::_i48 @ +0x48");
static_assert(offsetof(ConsumeProduceDescription, _i4c) == 0x4c, "ConsumeProduceDescription::_i4c @ +0x4c");
static_assert(offsetof(ConsumeProduceDescription, _u50) == 0x50, "ConsumeProduceDescription::_u50 @ +0x50");
