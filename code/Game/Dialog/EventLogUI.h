#pragma once
// ============================================================================
// code/Game/Dialog/EventLogUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "EventLogUI", parent
//   "UI::DialogController", table 0x14015bc40, 11 slots, sizeof 0x80.
//   create 0x1400113e0: MemAlloc(0x80, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400114b0 scalar deleting dtor (~EventLogUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [5] 0x140067640  OnOpened
//   [6] 0x140067630  RET 0
//   [7] 0x140067440  OnUpdateLayout
//   [9] 0x1400675d0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class EventLogUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(EventLogUI)

public:
    EventLogUI() = default;                              // 0x1400113e0 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x1400114b0
    void OnOpened() override;   // [5] 0x140067640
    void OnClosed() override {}   // [6] 0x140067630  RET 0 in retail
    void OnUpdateLayout() override;   // [7] 0x140067440
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x1400675d0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed
    uint64_t data40 = 0;           // +0x40: zeroed
    uint8_t _pad48[8] = {};        // +0x48: gap (not initialized by factory)
    uint64_t data50 = 0;           // +0x50: zeroed
    uint64_t data58 = 0;           // +0x58
    uint64_t data60 = 0;           // +0x60
    uint64_t data68 = 0;           // +0x68
    uint64_t data70 = 0;           // +0x70
    uint64_t data78 = 0;           // +0x78

protected:
    // 0x1400114b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail frees the Array/String members at +0x70, +0x60, +0x50; the
    // tree never did (leak kept).
    ~EventLogUI() = default;
};

static_assert(sizeof(EventLogUI) == 0x80, "EventLogUI is 0x80 (create 0x1400113e0 MemAlloc(0x80))");
static_assert(alignof(EventLogUI) == 8, "qword-aligned (the vptr)");
