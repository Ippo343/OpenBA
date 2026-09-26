#pragma once
// ============================================================================
// code/Game/Component/RawMaterialDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "RawMaterialDescription", parent "Description", ti 0x1401c7cb8
//   create 0x1400372d0: MemAlloc(0x60, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140037310.
//   table 0x1401641f0, 10 slots:
//     [0] 0x140037350 scalar deleting dtor   [1] 0x1400372c0 TypeName   [2] 0x1400372b0 Type
//     [3] 0x140129630 GetFileVersion = 8   [4] External::Compile (import, inherited)
//     [5] 0x140129520 Serialize (`MOV AL,1`)
//     [6] 0x140037320 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140037340 GetComponentType: RawMaterialComponent's TypeId
//     [9] 0x140037310 Construct (folded with createInPlace)
//   engine/types/RawMaterialDescription.h's POD forwards here (B8-4); field names are its .rsc names
//   sizeof 0x60.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"
#include "system/String.h"

class RawMaterialDescription final : public Description {
    DECLARE_EXE_TYPE(RawMaterialDescription)

public:
    RawMaterialDescription();

    Object::Base*     DeletingDtor(unsigned flags) override;               // [0] 0x140037350
    int32_t           GetFileVersion() const override { return 8; }    // [3] 0x140129630
    bool              Serialize(void* stream) override;                    // [5] 0x140129520
    Object::External* Reconstruct() override;                              // [6] 0x140037320
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140037340
    virtual Object::External* Construct();                                 // [9] 0x140037310

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _spriteSheet;                         // +0x20 SpriteSheet* (.rsc _spriteSheet)
    void* _stringTable;                         // +0x28 StringTable* (.rsc _stringTable)
    System::String _name;                       // +0x30 .rsc _name (full-size sprite name)
    System::String _nameSmall;                  // +0x38 .rsc _nameSmall
    int32_t _lowCreateCount;                    // +0x40
    int32_t _highCreateCount;                   // +0x44
    int32_t _weight;                            // +0x48
    int32_t _createChance;                      // +0x4c
    float _carryScale;                          // +0x50 0.33
    int32_t _i54;                               // +0x54 the POD's _pad54: zeroed, serialized as an int
    bool _unknown58;                            // +0x58
    uint8_t _pad59[3];                          // +0x59 never written by the ctor
    uint32_t _flags;                            // +0x5c RawMaterialFlags bitfield (.rsc _flags)

protected:
    ~RawMaterialDescription() = default;
};

static_assert(sizeof(RawMaterialDescription) == 0x60, "RawMaterialDescription is 0x60 (create 0x1400372d0 MemAlloc(0x60))");
static_assert(alignof(RawMaterialDescription) == 8, "RawMaterialDescription alignment");
static_assert(offsetof(RawMaterialDescription, _spriteSheet) == 0x20, "RawMaterialDescription::_spriteSheet @ +0x20");
static_assert(offsetof(RawMaterialDescription, _stringTable) == 0x28, "RawMaterialDescription::_stringTable @ +0x28");
static_assert(offsetof(RawMaterialDescription, _name) == 0x30, "RawMaterialDescription::_name @ +0x30");
static_assert(offsetof(RawMaterialDescription, _nameSmall) == 0x38, "RawMaterialDescription::_nameSmall @ +0x38");
static_assert(offsetof(RawMaterialDescription, _lowCreateCount) == 0x40, "RawMaterialDescription::_lowCreateCount @ +0x40");
static_assert(offsetof(RawMaterialDescription, _highCreateCount) == 0x44, "RawMaterialDescription::_highCreateCount @ +0x44");
static_assert(offsetof(RawMaterialDescription, _weight) == 0x48, "RawMaterialDescription::_weight @ +0x48");
static_assert(offsetof(RawMaterialDescription, _createChance) == 0x4c, "RawMaterialDescription::_createChance @ +0x4c");
static_assert(offsetof(RawMaterialDescription, _carryScale) == 0x50, "RawMaterialDescription::_carryScale @ +0x50");
static_assert(offsetof(RawMaterialDescription, _i54) == 0x54, "RawMaterialDescription::_i54 @ +0x54");
static_assert(offsetof(RawMaterialDescription, _unknown58) == 0x58, "RawMaterialDescription::_unknown58 @ +0x58");
static_assert(offsetof(RawMaterialDescription, _flags) == 0x5c, "RawMaterialDescription::_flags @ +0x5c");
