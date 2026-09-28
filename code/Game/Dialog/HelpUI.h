#pragma once
// ============================================================================
// code/Game/Dialog/HelpUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "HelpUI", parent
//   "UI::DialogController", table 0x14015bf30, 11 slots, sizeof 0x78.
//   create 0x140011ca0: MemAlloc(0x78, 8), ??0DialogController, stamp, fields.
//   [0] 0x140011cf0 scalar deleting dtor (~HelpUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x14006a500  OnCreated
//   [5] 0x14006a4f0  RET 0
//   [7] 0x14006a230  OnUpdateLayout
//   [9] 0x14006a290  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class HelpUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(HelpUI)

public:
    HelpUI() = default;                                  // 0x140011ca0 (ctor inlined in the factory)

    // 0x140011cf0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail frees the Array/String member at +0x68 before
    // ~DialogController (0x14006a8d0); the tree never did (leak kept).
    ~HelpUI() override = default;                                 // [0] 0x140011cf0
    void OnCreated() override;   // [4] 0x14006a500
    void OnOpened() override {}   // [5] 0x14006a4f0  RET 0 in retail
    void OnUpdateLayout() override;   // [7] 0x14006a230
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14006a290

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed
    uint64_t data40 = 0;           // +0x40: zeroed
    uint64_t data48 = 0;           // +0x48: zeroed
    uint64_t data50 = 0;           // +0x50: zeroed
    uint64_t data58 = 0;           // +0x58: zeroed
    int64_t data60 = -1;           // +0x60: initialized to -1 (0xFFFFFFFFFFFFFFFF)
    uint64_t data68 = 0;           // +0x68: zeroed
    uint64_t data70 = 0;           // +0x70: zeroed
};

static_assert(sizeof(HelpUI) == 0x78, "HelpUI is 0x78 (create 0x140011ca0 MemAlloc(0x78))");
static_assert(alignof(HelpUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(HelpUI, data60) == 0x60, "data60 @ 0x60");
