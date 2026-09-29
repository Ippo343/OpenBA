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
#include "runtime/types/collection/Collection.h"
#include "Object/exe_type.h"

namespace UI { class Element; }

class StatusIconUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(StatusIconUI)

public:
    StatusIconUI() = default;                            // 0x140014180 (ctor inlined in the factory)

    // 0x140014370 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail frees the Array at +0x30 and zeroes its header: the `icons` Array's dtor.
    ~StatusIconUI() override = default;                           // [0] 0x140014370
    void OnCreated() override;   // [4] 0x14007b270
    void OnUpdateLayout() override;   // [7] 0x14007b0a0
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;

    // ---- fields ----------------------------------------------------------
    // +0x30 the "icons" group's children: a Collection::Array<InternalPtr<UI::Element>>
    // (OnCreated 0x14007b270 SetSize 0x1400032e0 + one Child() per slot; the dtor
    // 0x140014370 MemFrees the data and zeroes the header: the Array's dtor).
    Collection::Array<UI::Element*> icons;   // +0x30
};

static_assert(offsetof(StatusIconUI, icons) == 0x30, "StatusIconUI::icons @ +0x30 (0x14007b270)");
static_assert(sizeof(StatusIconUI) == 0x40, "StatusIconUI is 0x40 (create 0x140014180 MemAlloc(0x40))");
static_assert(alignof(StatusIconUI) == 8, "qword-aligned (the vptr)");
