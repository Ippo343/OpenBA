#pragma once
// ============================================================================
// code/Game/Dialog/PageToggleUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "PageToggleUI", parent
//   "UI::DialogController", table 0x14015cfb0, 11 slots, sizeof 0x48.
//   create 0x140013160: MemAlloc(0x48, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400131c0 scalar deleting dtor (~PageToggleUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x1400759a0  RET 0
//   [4] 0x140075850  OnCreated
//   [7] 0x140075780  OnUpdateLayout
//   [9] 0x140075710  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class PageToggleUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(PageToggleUI)

public:
    PageToggleUI() = default;                            // 0x140013160 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x1400131c0
    void Persist(void* /*stream*/) override {}   // [3] 0x1400759a0  RET 0 in retail
    void OnCreated() override;   // [4] 0x140075850
    void OnUpdateLayout() override;   // [7] 0x140075780
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140075710

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed
    uint32_t data40 = 0;           // +0x40: zeroed (u32 write)
    uint8_t _pad44[4] = {};        // +0x44: gap

protected:
    // 0x1400131c0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~PageToggleUI() = default;
};

static_assert(sizeof(PageToggleUI) == 0x48, "PageToggleUI is 0x48 (create 0x140013160 MemAlloc(0x48))");
static_assert(alignof(PageToggleUI) == 8, "qword-aligned (the vptr)");
