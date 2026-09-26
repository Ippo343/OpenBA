#pragma once
// ============================================================================
// code/Game/Dialog/StatusIconUIConfig.h — ENDGAME B.3.8 block PNL
//
// The status-icon ribbon's config (`bool _showOnMainPage`).
//
// RETAIL (B38_SLOTS.md §6): reflected "StatusIconUIConfig", parent
//   "UI::DialogControllerConfig", ti 0x1401c56e0, table 0x14015d900, 9 slots,
//   sizeof 0x28. create 0x140014250: MemAlloc(0x28, 8),
//   ??0DialogControllerConfig, stamp, ONE byte zero at +0x20.
//   [0] 0x140014320 dtor  [1] 0x140014240  [2] 0x140014230
//   [3] 0x14007b450 GetFileVersion -> 1   [4] External::Compile (inherited)
//   [5] 0x14007b400 Serialize   [6] 0x1400142e0 Reconstruct
//   [7] 0x140001640 Destruct    [8] 0x1400142a0 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogControllerConfig.h"
#include "Object/exe_type.h"

class StatusIconUIConfig final : public UI::DialogControllerConfig {
    DECLARE_EXE_TYPE(StatusIconUIConfig)

public:
    StatusIconUIConfig() = default;                                // 0x140014250 (inlined)

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140014320
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x14007b450
    bool              Serialize(void* stream) override;            // [5] 0x14007b400
    Object::External* Reconstruct() override;                      // [6] 0x1400142e0
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x1400142a0

    bool    _showOnMainPage = false;                               // +0x20
    uint8_t _pad21[7];                                             // +0x21 (never written by retail)

protected:
    ~StatusIconUIConfig() = default;
};

static_assert(sizeof(StatusIconUIConfig) == 0x28, "StatusIconUIConfig is 0x28 (create 0x140014250)");
static_assert(alignof(StatusIconUIConfig) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(StatusIconUIConfig, _showOnMainPage) == 0x20, "flag @ +0x20 (Serialize 0x14007b400)");
