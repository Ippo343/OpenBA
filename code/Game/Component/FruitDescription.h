#pragma once
// ============================================================================
// code/Game/Component/FruitDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "FruitDescription", parent "Description", ti 0x1401c4288
//   create 0x140005980: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x1400059f0.
//   table 0x1401594d0, 10 slots:
//     [0] 0x140005ac0 scalar deleting dtor   [1] 0x140005970 TypeName   [2] 0x140005960 Type
//     [3] 0x14004a260 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x14004a1e0 Serialize (`MOV AL,1`)
//     [6] 0x140005a50 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140005ab0 GetComponentType: FruitComponent's TypeId
//     [9] 0x1400059f0 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class FruitDescription final : public Description {
    DECLARE_EXE_TYPE(FruitDescription)

public:
    FruitDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140005ac0
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x14004a260
    bool              Serialize(void* stream) override;                    // [5] 0x14004a1e0
    Object::External* Reconstruct() override;                              // [6] 0x140005a50
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140005ab0
    virtual Object::External* Construct();                                 // [9] 0x1400059f0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    float _f20;                                 // +0x20
    float _f24;                                 // +0x24 ctor stores int 0
    float _f28;                                 // +0x28 ctor stores int 0
    uint8_t _pad2c[4];                          // +0x2c never written by the ctor
    void* _ref30;                               // +0x30 ExternalPtr (a raw pointer)

protected:
    ~FruitDescription() = default;
};

static_assert(sizeof(FruitDescription) == 0x38, "FruitDescription is 0x38 (create 0x140005980 MemAlloc(0x38))");
static_assert(alignof(FruitDescription) == 8, "FruitDescription alignment");
static_assert(offsetof(FruitDescription, _f20) == 0x20, "FruitDescription::_f20 @ +0x20");
static_assert(offsetof(FruitDescription, _f24) == 0x24, "FruitDescription::_f24 @ +0x24");
static_assert(offsetof(FruitDescription, _f28) == 0x28, "FruitDescription::_f28 @ +0x28");
static_assert(offsetof(FruitDescription, _ref30) == 0x30, "FruitDescription::_ref30 @ +0x30");
