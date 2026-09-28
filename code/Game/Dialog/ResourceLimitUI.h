#pragma once
// ============================================================================
// code/Game/Dialog/ResourceLimitUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ResourceLimitUI", parent
//   "UI::DialogController", table 0x14015d1f0, 11 slots, sizeof 0x40.
//   create 0x1400136b0: MemAlloc(0x40, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400138d0 scalar deleting dtor (~ResourceLimitUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x140077970  OnCreated
//   [7] 0x1400778d0  OnUpdateLayout
//   [9] 0x1400777e0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ResourceLimitUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ResourceLimitUI)

public:
    ResourceLimitUI() = default;                         // 0x1400136b0 (ctor inlined in the factory)

    // 0x1400138d0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~ResourceLimitUI() override = default;                        // [0] 0x1400138d0
    void OnCreated() override;   // [4] 0x140077970
    void OnUpdateLayout() override;   // [7] 0x1400778d0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x1400777e0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed
};

static_assert(sizeof(ResourceLimitUI) == 0x40, "ResourceLimitUI is 0x40 (create 0x1400136b0 MemAlloc(0x40))");
static_assert(alignof(ResourceLimitUI) == 8, "qword-aligned (the vptr)");
