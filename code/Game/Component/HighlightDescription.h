#pragma once
// ============================================================================
// code/Game/Component/HighlightDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "HighlightDescription", parent "Description", ti 0x1401c4448
//   create 0x140006ea0: MemAlloc(0x40, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140006f10.
//   table 0x140159a10, 10 slots:
//     [0] 0x140006fe0 scalar deleting dtor   [1] 0x140006e90 TypeName   [2] 0x140006e80 Type
//     [3] 0x14004cad0 GetFileVersion = 2   [4] External::Compile (import, inherited)
//     [5] 0x14004c9d0 Serialize (`MOV AL,1`)
//     [6] 0x140006f70 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x140006fd0 GetComponentType: HighlightComponent's TypeId
//     [9] 0x140006f10 Construct (folded with createInPlace)
//   sizeof 0x40.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class HighlightDescription final : public Description {
    DECLARE_EXE_TYPE(HighlightDescription)

public:
    HighlightDescription();

    ~HighlightDescription() override;                                      // [0] 0x140006fe0
    int32_t           GetFileVersion() const override { return 2; }    // [3] 0x14004cad0
    bool              Serialize(void* stream) override;                    // [5] 0x14004c9d0
    Object::External* Reconstruct() override;                              // [6] 0x140006f70
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x140006fd0
    virtual Object::External* Construct();                                 // [9] 0x140006f10

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    void* _groups;                              // +0x20 group array data (0x10 stride: {External* array, count, cap})
    int32_t _groupCount;                        // +0x28
    int32_t _groupCap;                          // +0x2c
    void* _ref0;                                // +0x30 ExternalPtr
    void* _ref1;                                // +0x38 ExternalPtr
};

static_assert(sizeof(HighlightDescription) == 0x40, "HighlightDescription is 0x40 (create 0x140006ea0 MemAlloc(0x40))");
static_assert(alignof(HighlightDescription) == 8, "HighlightDescription alignment");
static_assert(offsetof(HighlightDescription, _groups) == 0x20, "HighlightDescription::_groups @ +0x20");
static_assert(offsetof(HighlightDescription, _groupCount) == 0x28, "HighlightDescription::_groupCount @ +0x28");
static_assert(offsetof(HighlightDescription, _groupCap) == 0x2c, "HighlightDescription::_groupCap @ +0x2c");
static_assert(offsetof(HighlightDescription, _ref0) == 0x30, "HighlightDescription::_ref0 @ +0x30");
static_assert(offsetof(HighlightDescription, _ref1) == 0x38, "HighlightDescription::_ref1 @ +0x38");
