#pragma once
// ============================================================================
// code/Game/Component/TombstoneDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "TombstoneDescription", parent "Description", ti 0x1401c4a68
//   create 0x14000d2e0: MemAlloc(0x28, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x14000d350.
//   table 0x14015aa00, 10 slots:
//     [0] 0x14000d420 scalar deleting dtor   [1] 0x14000d2d0 TypeName   [2] 0x14000d2c0 Type
//     [3] 0x140057a80 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140057a20 Serialize (`MOV AL,1`)
//     [6] 0x14000d3b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x14000d410 GetComponentType: TombstoneComponent's TypeId
//     [9] 0x14000d350 Construct (folded with createInPlace)
//   sizeof 0x28.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class TombstoneDescription final : public Description {
    DECLARE_EXE_TYPE(TombstoneDescription)

public:
    TombstoneDescription();

    ~TombstoneDescription() override = default;                            // [0] 0x14000d420
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140057a80
    bool              Serialize(void* stream) override;                    // [5] 0x140057a20
    Object::External* Reconstruct() override;                              // [6] 0x14000d3b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x14000d410
    virtual Object::External* Construct();                                 // [9] 0x14000d350

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    // Names from TombstoneComponent::Startup 0x140057860: lifetime = rand * range + minimum.
    float _minLifetime;                         // +0x20 (default 360)
    float _lifetimeRange;                       // +0x24 (default 24)
};

static_assert(sizeof(TombstoneDescription) == 0x28, "TombstoneDescription is 0x28 (create 0x14000d2e0 MemAlloc(0x28))");
static_assert(alignof(TombstoneDescription) == 8, "TombstoneDescription alignment");
static_assert(offsetof(TombstoneDescription, _minLifetime) == 0x20, "TombstoneDescription::_minLifetime @ +0x20");
static_assert(offsetof(TombstoneDescription, _lifetimeRange) == 0x24, "TombstoneDescription::_lifetimeRange @ +0x24");
