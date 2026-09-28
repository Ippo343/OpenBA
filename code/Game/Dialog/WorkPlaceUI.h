#pragma once
// ============================================================================
// code/Game/Dialog/WorkPlaceUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "WorkPlaceUI", parent
//   "UI::DialogController", table 0x14015e030, 11 slots, sizeof 0x68.
//   create 0x140016dc0: MemAlloc(0x68, 8), ??0DialogController, stamp, fields.
//   [0] 0x140016e90 scalar deleting dtor (~WorkPlaceUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x140087940  OnCreated
//   [7] 0x140087080  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   [9] 0x1400877e0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class WorkPlaceUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(WorkPlaceUI)

public:
    WorkPlaceUI() = default;                             // 0x140016dc0 (ctor inlined in the factory)

    // 0x140016e90 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~WorkPlaceUI() override = default;                            // [0] 0x140016e90
    void OnCreated() override;   // [4] 0x140087940
    void OnUpdateLayout() override;   // [7] 0x140087080
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x1400877e0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38
    uint64_t data40 = 0;           // +0x40
    uint64_t data48 = 0;           // +0x48
    uint64_t data50 = 0;           // +0x50
    uint64_t data58 = 0;           // +0x58
    uint32_t data60 = 0;           // +0x60: zeroed (u32 write)
    uint8_t _pad64[4] = {};        // +0x64: gap
};

static_assert(sizeof(WorkPlaceUI) == 0x68, "WorkPlaceUI is 0x68 (create 0x140016dc0 MemAlloc(0x68))");
static_assert(alignof(WorkPlaceUI) == 8, "qword-aligned (the vptr)");
