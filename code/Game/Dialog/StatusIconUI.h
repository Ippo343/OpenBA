#pragma once
// ============================================================================
// code/Game/Dialog/StatusIconUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "StatusIconUI", parent
//   "UI::DialogController", table 0x14015d8a0, 11 slots, sizeof 0x40.
//   create 0x140014180: MemAlloc(0x40, 8), ??0DialogController, stamp, fields.
//   [0] 0x140014370 scalar deleting dtor (~StatusIconUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x14007b270  OnCreated
//   [7] 0x14007b0a0  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class StatusIconUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(StatusIconUI)

public:
    StatusIconUI() = default;                            // 0x140014180 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140014370
    void OnCreated() override;   // [4] 0x14007b270
    void OnUpdateLayout() override;   // [7] 0x14007b0a0
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed

protected:
    // 0x140014370 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail frees the Array/String member at +0x30; the tree never did
    // (leak kept).
    ~StatusIconUI() = default;
};

static_assert(sizeof(StatusIconUI) == 0x40, "StatusIconUI is 0x40 (create 0x140014180 MemAlloc(0x40))");
static_assert(alignof(StatusIconUI) == 8, "qword-aligned (the vptr)");
