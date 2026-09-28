#pragma once
// ============================================================================
// code/Game/Dialog/ImmigrationUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ImmigrationUI", parent
//   "UI::DialogController", table 0x14015c080, 11 slots, sizeof 0x50.
//   create 0x140011ee0: MemAlloc(0x50, 8), ??0DialogController, stamp, fields.
//   [0] 0x140011fd0 scalar deleting dtor (~ImmigrationUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x14006b330  OnCreated
//   [7] 0x14006b130  OnUpdateLayout
//   [9] 0x14006b300  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ImmigrationUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ImmigrationUI)

public:
    ImmigrationUI() = default;                           // 0x140011ee0 (ctor inlined in the factory)

    // 0x140011fd0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~ImmigrationUI() override = default;                          // [0] 0x140011fd0
    void OnCreated() override;   // [4] 0x14006b330
    void OnUpdateLayout() override;   // [7] 0x14006b130
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14006b300

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38
    uint64_t data40 = 0;           // +0x40
    uint64_t data48 = 0;           // +0x48
};

static_assert(sizeof(ImmigrationUI) == 0x50, "ImmigrationUI is 0x50 (create 0x140011ee0 MemAlloc(0x50))");
static_assert(alignof(ImmigrationUI) == 8, "qword-aligned (the vptr)");
