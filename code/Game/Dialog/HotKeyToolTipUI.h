#pragma once
// ============================================================================
// code/Game/Dialog/HotKeyToolTipUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "HotKeyToolTipUI", parent
//   "UI::DialogController", table 0x14015bfc0, 11 slots, sizeof 0x30.
//   create 0x140011e20: MemAlloc(0x30, 8), ??0DialogController, stamp, fields.
//   [0] 0x140011e70 scalar deleting dtor (~HotKeyToolTipUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14006ab80  RET 0
//   [4] 0x14006a980  OnCreated
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class HotKeyToolTipUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(HotKeyToolTipUI)

public:
    HotKeyToolTipUI() = default;                         // 0x140011e20 (ctor inlined in the factory)

    // 0x140011e70 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~HotKeyToolTipUI() override = default;                        // [0] 0x140011e70
    void Persist(void* /*stream*/) override {}   // [3] 0x14006ab80  RET 0 in retail
    void OnCreated() override;   // [4] 0x14006a980

};

static_assert(sizeof(HotKeyToolTipUI) == 0x30, "HotKeyToolTipUI is 0x30 (create 0x140011e20 MemAlloc(0x30))");
static_assert(alignof(HotKeyToolTipUI) == 8, "qword-aligned (the vptr)");
