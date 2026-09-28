#pragma once
// ============================================================================
// code/Game/Dialog/ProductionUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ProductionUI", parent
//   "UI::DialogController", table 0x14015d070, 11 slots, sizeof 0x50.
//   create 0x140013230: MemAlloc(0x50, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400132b0 scalar deleting dtor (~ProductionUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x140076300  RET 0
//   [4] 0x140075da0  OnCreated
//   [7] 0x140075a20  OnUpdateLayout
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ProductionUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ProductionUI)

public:
    ProductionUI() = default;                            // 0x140013230 (ctor inlined in the factory)

    // 0x1400132b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail runs `eh vector destructor iterator` over the two 0x10-byte
    // members at +0x30; the tree never did (leak kept).
    ~ProductionUI() override = default;                           // [0] 0x1400132b0
    void Persist(void* /*stream*/) override {}   // [3] 0x140076300  RET 0 in retail
    void OnCreated() override;   // [4] 0x140075da0
    void OnUpdateLayout() override;   // [7] 0x140075a20

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint8_t vectorItems[0x20] = {};// +0x30: 2x 0x10-byte items (constructor-initialized)
};

static_assert(sizeof(ProductionUI) == 0x50, "ProductionUI is 0x50 (create 0x140013230 MemAlloc(0x50))");
static_assert(alignof(ProductionUI) == 8, "qword-aligned (the vptr)");
