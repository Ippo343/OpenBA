#pragma once
// ============================================================================
// code/Game/Dialog/ProfessionUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ProfessionUI", parent
//   "UI::DialogController", table 0x14015d0f0, 11 slots, sizeof 0x78.
//   create 0x140013400: MemAlloc(0x78, 8), ??0DialogController, stamp, fields.
//   [0] 0x140013450 scalar deleting dtor (~ProfessionUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [5] 0x140076ca0  OnOpened
//   [8] 0x1400765a0  OnUpdate
//   [9] 0x140076c10  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ProfessionUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ProfessionUI)

public:
    ProfessionUI() = default;                            // 0x140013400 (ctor inlined in the factory)

    // 0x140013450 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail's dtor mirrors ctor 0x1400775a0 (owning members released);
    // the tree never did (leak kept).
    ~ProfessionUI() override = default;                           // [0] 0x140013450
    void OnOpened() override;   // [5] 0x140076ca0
    void OnUpdate(const System::Time& time) override;   // [8] 0x1400765a0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140076c10

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed
    uint64_t data40 = 0;           // +0x40: zeroed
    uint64_t data48 = 0;           // +0x48: zeroed
    uint64_t data50 = 0;           // +0x50: zeroed
    uint64_t data58 = 0;           // +0x58: zeroed
    uint64_t data60 = 0;           // +0x60: zeroed
    uint32_t data68 = 0;           // +0x68: zeroed (u32 write)
    uint8_t _pad6C[4] = {};        // +0x6C: gap
    uint32_t data70 = 0;           // +0x70: zeroed (u32 write)
    uint8_t _pad74[4] = {};        // +0x74: gap
};

static_assert(sizeof(ProfessionUI) == 0x78, "ProfessionUI is 0x78 (create 0x140013400 MemAlloc(0x78))");
static_assert(alignof(ProfessionUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(ProfessionUI, data30) == 0x30, "data30 @ 0x30");
static_assert(offsetof(ProfessionUI, data68) == 0x68, "data68 @ 0x68");
