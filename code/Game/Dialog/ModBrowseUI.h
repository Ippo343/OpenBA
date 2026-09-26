#pragma once
// ============================================================================
// code/Game/Dialog/ModBrowseUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ModBrowseUI", parent
//   "UI::DialogController", table 0x14015c500, 11 slots, sizeof 0x30.
//   create 0x1400126d0: MemAlloc(0x30, 8), ??0DialogController, stamp, fields.
//   [0] 0x140012720 scalar deleting dtor (~ModBrowseUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14006d390  RET 0
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ModBrowseUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ModBrowseUI)

public:
    ModBrowseUI() = default;                             // 0x1400126d0 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140012720
    void Persist(void* /*stream*/) override {}   // [3] 0x14006d390  RET 0 in retail


protected:
    // 0x140012720 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~ModBrowseUI() = default;
};

static_assert(sizeof(ModBrowseUI) == 0x30, "ModBrowseUI is 0x30 (create 0x1400126d0 MemAlloc(0x30))");
static_assert(alignof(ModBrowseUI) == 8, "qword-aligned (the vptr)");
