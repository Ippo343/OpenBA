#pragma once
// ============================================================================
// code/Game/Dialog/AttendanceUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "AttendanceUI", parent
//   "UI::DialogController", table 0x14015b070, 11 slots, sizeof 0x38.
//   create 0x1400104b0: MemAlloc(0x38, 8), ??0DialogController, stamp, fields.
//   [0] 0x140010560 scalar deleting dtor (~AttendanceUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x14005dd40  OnCreated
//   [7] 0x14005dc50  OnUpdateLayout
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

namespace UI { class ProgressElement; }

class AttendanceUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(AttendanceUI)

public:
    AttendanceUI() = default;                            // 0x1400104b0 (ctor inlined in the factory)

    // 0x140010560 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~AttendanceUI() override = default;                           // [0] 0x140010560
    void OnCreated() override;   // [4] 0x14005dd40
    void OnUpdateLayout() override;   // [7] 0x14005dc50

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    UI::ProgressElement* progress = nullptr;  // +0x30 FindElementType(ProgressElement) (0x14005dd40)
};

static_assert(sizeof(AttendanceUI) == 0x38, "AttendanceUI is 0x38 (create 0x1400104b0 MemAlloc(0x38))");
static_assert(alignof(AttendanceUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(AttendanceUI, progress) == 0x30, "AttendanceUI::progress @ +0x30 (0x14005dd40)");
