#pragma once
// ============================================================================
// code/Game/Dialog/EntityUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "EntityUI", parent
//   "UI::DialogController", table 0x14015bb90, 11 slots, sizeof 0x48.
//   create 0x140011270: MemAlloc(0x48, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400112d0 scalar deleting dtor (~EntityUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x1400672a0  RET 0
//   [4] 0x1400670d0  OnCreated
//   [6] 0x140067090  OnClosed
//   [7] 0x140066d80  OnUpdateLayout
//   [8] 0x140066ad0  OnUpdate
//   [9] 0x140067050  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class EntityUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(EntityUI)

public:
    EntityUI() = default;                                // 0x140011270 (ctor inlined in the factory)

    // 0x1400112d0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~EntityUI() override = default;                               // [0] 0x1400112d0
    void Persist(void* /*stream*/) override {}   // [3] 0x1400672a0  RET 0 in retail
    void OnCreated() override;   // [4] 0x1400670d0
    void OnClosed() override;   // [6] 0x140067090
    void OnUpdateLayout() override;   // [7] 0x140066d80
    void OnUpdate(const System::Time& time) override;   // [8] 0x140066ad0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140067050

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed
    uint64_t data40 = 0;           // +0x40: zeroed
};

static_assert(sizeof(EntityUI) == 0x48, "EntityUI is 0x48 (create 0x140011270 MemAlloc(0x48))");
static_assert(alignof(EntityUI) == 8, "qword-aligned (the vptr)");
