#pragma once
// ============================================================================
// code/Game/Dialog/TitleDescToolTipUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "TitleDescToolTipUI", parent
//   "UI::DialogController", table 0x14015d9f0, 11 slots, sizeof 0x30.
//   create 0x1400144c0: MemAlloc(0x30, 8), ??0DialogController, stamp, fields.
//   [0] 0x140014510 scalar deleting dtor (~TitleDescToolTipUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14007b8a0  RET 0
//   [4] 0x14007b730  OnCreated
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class TitleDescToolTipUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(TitleDescToolTipUI)

public:
    TitleDescToolTipUI() = default;                      // 0x1400144c0 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140014510
    void Persist(void* /*stream*/) override {}   // [3] 0x14007b8a0  RET 0 in retail
    void OnCreated() override;   // [4] 0x14007b730


protected:
    // 0x140014510 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~TitleDescToolTipUI() = default;
};

static_assert(sizeof(TitleDescToolTipUI) == 0x30, "TitleDescToolTipUI is 0x30 (create 0x1400144c0 MemAlloc(0x30))");
static_assert(alignof(TitleDescToolTipUI) == 8, "qword-aligned (the vptr)");
