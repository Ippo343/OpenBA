#pragma once
// ============================================================================
// code/Game/Dialog/UpgradeUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "UpgradeUI", parent
//   "UI::DialogController", table 0x14015df00, 11 slots, sizeof 0x38.
//   create 0x140016b30: MemAlloc(0x38, 8), ??0DialogController, stamp, fields.
//   [0] 0x140016d50 scalar deleting dtor (~UpgradeUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x140086d90  OnCreated
//   [7] 0x140086d80  RET 0
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   [9] 0x140086c90  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class UpgradeUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(UpgradeUI)

public:
    UpgradeUI() = default;                               // 0x140016b30 (ctor inlined in the factory)

    // 0x140016d50 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~UpgradeUI() override = default;                              // [0] 0x140016d50
    void OnCreated() override;   // [4] 0x140086d90
    void OnUpdateLayout() override {}   // [7] 0x140086d80  RET 0 in retail
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140086c90

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
};

static_assert(sizeof(UpgradeUI) == 0x38, "UpgradeUI is 0x38 (create 0x140016b30 MemAlloc(0x38))");
static_assert(alignof(UpgradeUI) == 8, "qword-aligned (the vptr)");
