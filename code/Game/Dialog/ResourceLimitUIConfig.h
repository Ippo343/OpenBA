#pragma once
// ============================================================================
// code/Game/Dialog/ResourceLimitUIConfig.h — ENDGAME B.3.8 block PNL
//
// The per-row config behind each Town Hall resource-limit spinner.
//
// RETAIL (B38_SLOTS.md §6): reflected "ResourceLimitUIConfig", parent
//   "UI::DialogControllerConfig", ti 0x1401c5520, table 0x14015d250, 9 slots,
//   sizeof 0x28. create 0x140013780: MemAlloc(0x28, 8),
//   ??0DialogControllerConfig, stamp, dword zero at +0x20.
//   [0] 0x140013880 dtor  [1] 0x140013770  [2] 0x140013760
//   [3] 0x140077b60 GetFileVersion -> 1   [4] External::Compile (inherited)
//   [5] 0x140077b10 Serialize   [6] 0x140013830 Reconstruct
//   [7] 0x140001640 Destruct    [8] 0x1400137e0 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogControllerConfig.h"
#include "Object/exe_type.h"

class ResourceLimitUIConfig final : public UI::DialogControllerConfig {
    DECLARE_EXE_TYPE(ResourceLimitUIConfig)

public:
    ResourceLimitUIConfig() = default;                             // 0x140013780 (inlined)

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140013880
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x140077b60
    bool              Serialize(void* stream) override;            // [5] 0x140077b10
    Object::External* Reconstruct() override;                      // [6] 0x140013830
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x1400137e0

    // +0x20 RESOURCE INDEX of the row (Log=0 … Textile=10) into the town
    // manager's limit table.
    int32_t _resourceIndex = 0;                                    // +0x20
    uint8_t _pad24[4]      = {};                                   // +0x24 (never written by retail)

protected:
    ~ResourceLimitUIConfig() = default;
};

static_assert(sizeof(ResourceLimitUIConfig) == 0x28, "ResourceLimitUIConfig is 0x28 (create 0x140013780)");
static_assert(alignof(ResourceLimitUIConfig) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(ResourceLimitUIConfig, _resourceIndex) == 0x20, "index @ +0x20 (Serialize 0x140077b10)");
