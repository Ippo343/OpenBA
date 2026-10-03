#pragma once
// ============================================================================
// code/Game/Component/AutoPickupDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "AutoPickupDescription", parent "Description", ti 0x1401c3fe8
//   create 0x140003420: MemAlloc(0x28, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140003480.
//   table 0x140158e70, 10 slots:
//     [0] 0x140003530 scalar deleting dtor   [1] 0x140003410 TypeName   [2] 0x140003400 Type
//     [3] 0x140044bb0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140044b60 Serialize (`MOV AL,1`)
//     [6] 0x1400034d0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140003520 GetComponentType: AutoPickupComponent's TypeId
//     [9] 0x140003480 Construct (folded with createInPlace)
//   sizeof 0x28.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class AutoPickupDescription final : public Description {
    DECLARE_EXE_TYPE(AutoPickupDescription)

public:
    AutoPickupDescription();

    ~AutoPickupDescription() override = default;                           // [0] 0x140003530
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140044bb0
    bool              Serialize(IO::Stream& stream) override;                    // [5] 0x140044b60
    Object::External* Reconstruct() override;                              // [6] 0x1400034d0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140003520
    virtual Object::External* Construct();                                 // [9] 0x140003480

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    bool _enabled;                              // +0x20
    uint8_t _pad21[7];                          // +0x21 never written by the ctor
};

static_assert(sizeof(AutoPickupDescription) == 0x28, "AutoPickupDescription is 0x28 (create 0x140003420 MemAlloc(0x28))");
static_assert(alignof(AutoPickupDescription) == 8, "AutoPickupDescription alignment");
static_assert(offsetof(AutoPickupDescription, _enabled) == 0x20, "AutoPickupDescription::_enabled @ +0x20");
