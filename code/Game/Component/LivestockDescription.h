#pragma once
// ============================================================================
// code/Game/Component/LivestockDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "LivestockDescription", parent "Description", ti 0x1401c79a8
//   create 0x1400354e0: MemAlloc(0x30, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140035550.
//   table 0x140163a30, 10 slots:
//     [0] 0x140035620 scalar deleting dtor   [1] 0x1400354d0 TypeName   [2] 0x1400354c0 Type
//     [3] 0x140123ff0 GetFileVersion = 1   [4] External::Compile (import, inherited)
//     [5] 0x140123f60 Serialize (`MOV AL,1`)
//     [6] 0x1400355b0 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140035610 GetComponentType: LivestockComponent's TypeId
//     [9] 0x140035550 Construct (folded with createInPlace)
//   sizeof 0x30.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class LivestockDescription final : public Description {
    DECLARE_EXE_TYPE(LivestockDescription)

public:
    LivestockDescription();

    ~LivestockDescription() override = default;                            // [0] 0x140035620
    int32_t           GetFileVersion() const override { return 1; }    // [3] 0x140123ff0
    bool              Serialize(void* stream) override;                    // [5] 0x140123f60
    Object::External* Reconstruct() override;                              // [6] 0x1400355b0
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140035610
    virtual Object::External* Construct();                                 // [9] 0x140035550

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _byproduct;                           // +0x20 ExternalPtr (byproduct)
    uint32_t _type;                             // +0x28 enum (serialized through the uint door)
    float _f2c;                                 // +0x2c
};

static_assert(sizeof(LivestockDescription) == 0x30, "LivestockDescription is 0x30 (create 0x1400354e0 MemAlloc(0x30))");
static_assert(alignof(LivestockDescription) == 8, "LivestockDescription alignment");
static_assert(offsetof(LivestockDescription, _byproduct) == 0x20, "LivestockDescription::_byproduct @ +0x20");
static_assert(offsetof(LivestockDescription, _type) == 0x28, "LivestockDescription::_type @ +0x28");
static_assert(offsetof(LivestockDescription, _f2c) == 0x2c, "LivestockDescription::_f2c @ +0x2c");
