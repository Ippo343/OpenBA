#pragma once
// ============================================================================
// code/Game/Component/PastureDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "PastureDescription", parent "Description", ti 0x1401c7bd8
//   create 0x1400367a0: MemAlloc(0x28, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140036800.
//   table 0x140163ff0, 10 slots:
//     [0] 0x1400368b0 scalar deleting dtor   [1] 0x140036790 TypeName   [2] 0x140036780 Type
//     [3] 0x140128280 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x140128220 Serialize (`MOV AL,1`)
//     [6] 0x140036850 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400368a0 GetComponentType: PastureComponent's TypeId
//     [9] 0x140036800 Construct (folded with createInPlace)
//   sizeof 0x28.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class PastureDescription final : public Description {
    DECLARE_EXE_TYPE(PastureDescription)

public:
    PastureDescription();

    ~PastureDescription() override = default;                              // [0] 0x1400368b0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x140128280
    bool              Serialize(void* stream) override;                    // [5] 0x140128220
    Object::External* Reconstruct() override;                              // [6] 0x140036850
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400368a0
    virtual Object::External* Construct();                                 // [9] 0x140036800

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    uint32_t _type;                             // +0x20 enum (serialized through the uint door)
    uint8_t _pad24[4];                          // +0x24 never written by the ctor
};

static_assert(sizeof(PastureDescription) == 0x28, "PastureDescription is 0x28 (create 0x1400367a0 MemAlloc(0x28))");
static_assert(alignof(PastureDescription) == 8, "PastureDescription alignment");
static_assert(offsetof(PastureDescription, _type) == 0x20, "PastureDescription::_type @ +0x20");
