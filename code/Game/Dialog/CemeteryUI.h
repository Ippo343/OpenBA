#pragma once
// ============================================================================
// code/Game/Dialog/CemeteryUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "CemeteryUI", parent
//   "UI::DialogController", table 0x14015b240, 11 slots, sizeof 0x38.
//   create 0x140010900: MemAlloc(0x38, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400109b0 scalar deleting dtor (~CemeteryUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x14005f090  OnCreated
//   [7] 0x14005ef50  OnUpdateLayout
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

namespace UI { class ProgressElement; }

class CemeteryUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(CemeteryUI)

public:
    CemeteryUI() = default;                              // 0x140010900 (ctor inlined in the factory)

    // 0x1400109b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~CemeteryUI() override = default;                             // [0] 0x1400109b0
    void OnCreated() override;   // [4] 0x14005f090
    void OnUpdateLayout() override;   // [7] 0x14005ef50

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    UI::ProgressElement* progress = nullptr;  // +0x30 FindElementType(ProgressElement) (0x14005f090)
};

static_assert(sizeof(CemeteryUI) == 0x38, "CemeteryUI is 0x38 (create 0x140010900 MemAlloc(0x38))");
static_assert(alignof(CemeteryUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(CemeteryUI, progress) == 0x30, "CemeteryUI::progress @ +0x30 (0x14005f090)");
