#pragma once
// ============================================================================
// code/Game/Component/MarketDescription.h — ENDGAME B.3.8 K1 (B38_RULES.md D1-D5, §2.2)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6):
//   reflected "MarketDescription", parent "Description", ti 0x1401c4528
//   create 0x140008290: MemAlloc(0x38, 8), `??0External@Object@@`, the
//   Description stamp, the leaf stamp, the field defaults. createInPlace 0x140008300.
//   table 0x140159cf0, 10 slots:
//     [0] 0x1400083d0 scalar deleting dtor   [1] 0x140008280 TypeName   [2] 0x140008270 Type
//     [3] 0x14004ec80 GetFileVersion = 3   [4] External::Compile (import, inherited)
//     [5] 0x14004ebb0 Serialize (`MOV AL,1`)
//     [6] 0x140008360 Reconstruct   [7] 0x140001640 Destruct (shared)
//     [8] 0x1400083c0 GetComponentType: MarketComponent's TypeId
//     [9] 0x140008300 Construct (folded with createInPlace)
//   sizeof 0x38.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Component/Description.h"
#include "Object/exe_type.h"

class MarketDescription final : public Description {
    DECLARE_EXE_TYPE(MarketDescription)

public:
    MarketDescription();

    ~MarketDescription() override;                                         // [0] 0x1400083d0
    int32_t           GetFileVersion() const override { return 3; }    // [3] 0x14004ec80
    bool              Serialize(void* stream) override;                    // [5] 0x14004ebb0
    Object::External* Reconstruct() override;                              // [6] 0x140008360
    void              Destruct() override;                                 // [7] 0x140001640
    Object::Type*     GetComponentType(Object::Type* ret) const override;  // [8] 0x1400083c0
    virtual Object::External* Construct();                                 // [9] 0x140008300

    // ---- fields (PUBLIC: the components read them at raw offsets today) ----
    float _fillRatio;                           // +0x20 global fill ratio
    void* _ratios;                              // +0x28 array of {enum resource @+0, float ratio @+4}, stride 8
    int32_t _ratioCount;                        // +0x30
    int32_t _ratioCap;                          // +0x34
};

static_assert(sizeof(MarketDescription) == 0x38, "MarketDescription is 0x38 (create 0x140008290 MemAlloc(0x38))");
static_assert(alignof(MarketDescription) == 8, "MarketDescription alignment");
static_assert(offsetof(MarketDescription, _fillRatio) == 0x20, "MarketDescription::_fillRatio @ +0x20");
static_assert(offsetof(MarketDescription, _ratios) == 0x28, "MarketDescription::_ratios @ +0x28");
static_assert(offsetof(MarketDescription, _ratioCount) == 0x30, "MarketDescription::_ratioCount @ +0x30");
static_assert(offsetof(MarketDescription, _ratioCap) == 0x34, "MarketDescription::_ratioCap @ +0x34");
