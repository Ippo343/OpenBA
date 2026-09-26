#pragma once
// ============================================================================
// code/Game/Dialog/DestroyUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "DestroyUI", parent
//   "UI::DialogController", table 0x14015bb00, 11 slots, sizeof 0x78.
//   create 0x140011130: MemAlloc(0x78, 8), ??0DialogController, stamp, fields.
//   [0] 0x140011200 scalar deleting dtor (~DestroyUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x140066150  OnCreated
//   [7] 0x140065b00  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   [9] 0x140066080  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class DestroyUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(DestroyUI)

public:
    DestroyUI() = default;                               // 0x140011130 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140011200
    void OnCreated() override;   // [4] 0x140066150
    void OnUpdateLayout() override;   // [7] 0x140065b00
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140066080

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38
    uint64_t data40 = 0;           // +0x40
    uint64_t data48 = 0;           // +0x48
    uint64_t data50 = 0;           // +0x50
    uint64_t data58 = 0;           // +0x58
    uint64_t data60 = 0;           // +0x60
    uint64_t data68 = 0;           // +0x68
    uint64_t data70 = 0;           // +0x70

protected:
    // 0x140011200 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~DestroyUI() = default;
};

static_assert(sizeof(DestroyUI) == 0x78, "DestroyUI is 0x78 (create 0x140011130 MemAlloc(0x78))");
static_assert(alignof(DestroyUI) == 8, "qword-aligned (the vptr)");
