#pragma once
// ============================================================================
// code/Game/Dialog/UpgradeUIConfig.h — ENDGAME B.3.8 block PNL
//
// The upgrade panel's config: +0x20 is the TARGET building template
// (`Object::ExternalPtr<ComponentEntity>`) UpgradeUI copies into
// DestroyComponent+0x60.
//
// RETAIL (B38_SLOTS.md §6): reflected "UpgradeUIConfig", parent
//   "UI::DialogControllerConfig", ti 0x1401c5868, table 0x14015df60, 9 slots,
//   sizeof 0x28. create 0x140016c00: MemAlloc(0x28, 8),
//   ??0DialogControllerConfig, stamp, qword zero at +0x20.
//   [0] 0x140016d00 dtor  [1] 0x140016bf0  [2] 0x140016be0
//   [3] 0x140087000 GetFileVersion -> 1   [4] External::Compile (inherited)
//   [5] 0x140086fb0 Serialize   [6] 0x140016cb0 Reconstruct
//   [7] 0x140001640 Destruct    [8] 0x140016c60 Construct
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogControllerConfig.h"
#include "Object/exe_type.h"

class UpgradeUIConfig final : public UI::DialogControllerConfig {
    DECLARE_EXE_TYPE(UpgradeUIConfig)

public:
    UpgradeUIConfig() = default;                                   // 0x140016c00 (inlined)

    Object::Base*     DeletingDtor(unsigned flags) override;       // [0] 0x140016d00
    int32_t           GetFileVersion() const override { return 1; }// [3] 0x140087000
    bool              Serialize(void* stream) override;            // [5] 0x140086fb0
    Object::External* Reconstruct() override;                      // [6] 0x140016cb0
    void              Destruct() override;                         // [7] 0x140001640
    virtual Object::External* Construct();                         // [8] 0x140016c60

    // +0x20 Object::ExternalPtr<ComponentEntity> — raw pointer spelling: the
    // readers (UpgradeUI, DestroyComponent state 5) reach it at the offset.
    void* _target = nullptr;                                       // +0x20

protected:
    ~UpgradeUIConfig() = default;
};

static_assert(sizeof(UpgradeUIConfig) == 0x28, "UpgradeUIConfig is 0x28 (create 0x140016c00)");
static_assert(alignof(UpgradeUIConfig) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(UpgradeUIConfig, _target) == 0x20, "target template @ +0x20 (Serialize 0x140086fb0)");
