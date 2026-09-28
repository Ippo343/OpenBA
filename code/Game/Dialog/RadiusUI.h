#pragma once
// ============================================================================
// code/Game/Dialog/RadiusUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "RadiusUI", parent
//   "UI::DialogController", table 0x14015d160, 11 slots, sizeof 0x38.
//   create 0x140013590: MemAlloc(0x38, 8), ??0DialogController, stamp, fields.
//   [0] 0x140013640 scalar deleting dtor (~RadiusUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [5] 0x140077650  OnOpened
//   [6] 0x140077600  OnClosed
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class RadiusUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(RadiusUI)

public:
    RadiusUI() = default;                                // 0x140013590 (ctor inlined in the factory)

    // 0x140013640 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // STANDALONE DEVIATION: retail's dtor 0x140013640 has no body of its own;
    // the tree's teardown drops the radius overlay (Slot6) here, kept.
    // the tree's teardown body (see the .cpp)
    ~RadiusUI() override;                                         // [0] 0x140013640
    void OnOpened() override;   // [5] 0x140077650
    void OnClosed() override;   // [6] 0x140077600

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    int32_t data30 = -1;           // +0x30: initialized to -1
    uint8_t _pad34[4] = {};        // +0x34: gap
};

static_assert(sizeof(RadiusUI) == 0x38, "RadiusUI is 0x38 (create 0x140013590 MemAlloc(0x38))");
static_assert(alignof(RadiusUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(RadiusUI, data30) == 0x30, "data30 @ 0x30");
