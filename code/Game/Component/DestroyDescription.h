#pragma once
// ============================================================================
// code/Game/Component/DestroyDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "DestroyDescription", parent "Description", ti 0x1401c7628
//   create 0x140033c30: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140033ca0.
//   table 0x1401631b0, 10 slots:
//     [0] 0x140033d70 scalar deleting dtor   [1] 0x140033c20 TypeName   [2] 0x140033c10 Type
//     [3] 0x14011def0 GetFileVersion = 5   [4] External::Compile (import, inherited)
//     [5] 0x14011de70 Serialize (`MOV AL,1`)
//     [6] 0x140033d00 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140033d60 GetComponentType: DestroyComponent's TypeId
//     [9] 0x140033ca0 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class ComponentDescription;   // Component/ComponentDescription.h

class DestroyDescription final : public Description {
    DECLARE_EXE_TYPE(DestroyDescription)

public:
    DestroyDescription();

    ~DestroyDescription() override = default;                              // [0] 0x140033d70
    int32_t           GetFileVersion() const override { return 5; }    // [3] 0x14011def0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x14011de70
    Object::External* Reconstruct() override;                              // [6] 0x140033d00
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140033d60
    virtual Object::External* Construct();                                 // [9] 0x140033ca0

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    ComponentDescription* _replaceEntity;       // +0x20 ExternalPtr<ComponentDescription> (Serialize 0x140002340)
    int32_t _damagedSkin;                       // +0x28 rubble MATERIAL-SET index
    int32_t _damagedDecal;                      // +0x2c rubble DECAL state
    int32_t _minDamageLevel;                    // +0x30 retail leaves it unset; the tree zeroes it (kept)
    uint8_t _pad34[4];                          // +0x34 never written by the ctor
};

static_assert(sizeof(DestroyDescription) == 0x38, "DestroyDescription is 0x38 (create 0x140033c30 MemAlloc(0x38))");
static_assert(alignof(DestroyDescription) == 8, "DestroyDescription alignment");
static_assert(offsetof(DestroyDescription, _replaceEntity) == 0x20, "DestroyDescription::_replaceEntity @ +0x20");
static_assert(offsetof(DestroyDescription, _damagedSkin) == 0x28, "DestroyDescription::_damagedSkin @ +0x28");
static_assert(offsetof(DestroyDescription, _damagedDecal) == 0x2c, "DestroyDescription::_damagedDecal @ +0x2c");
static_assert(offsetof(DestroyDescription, _minDamageLevel) == 0x30, "DestroyDescription::_minDamageLevel @ +0x30");
