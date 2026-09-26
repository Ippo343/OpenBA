#pragma once
// ============================================================================
// code/Game/Dialog/CitizenUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "CitizenUI", parent
//   "UI::DialogController", table 0x14015ba00, 11 slots, sizeof 0xe8.
//   create 0x140010f80: MemAlloc(0xe8, 8), ??0DialogController, stamp, fields.
//   [0] 0x140010fd0 scalar deleting dtor (~CitizenUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x1400651a0  OnCreated
//   [6] 0x140065130  OnClosed
//   [7] 0x140064ae0  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   [9] 0x1400650f0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class CitizenUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(CitizenUI)

public:
    CitizenUI() = default;                               // 0x140010f80 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140010fd0
    void OnCreated() override;   // [4] 0x1400651a0
    void OnClosed() override;   // [6] 0x140065130
    void OnUpdateLayout() override;   // [7] 0x140064ae0
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x1400650f0

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
    uint64_t data78 = 0;           // +0x78
    uint64_t data80 = 0;           // +0x80
    uint64_t data88 = 0;           // +0x88
    uint64_t data90 = 0;           // +0x90
    uint64_t data98 = 0;           // +0x98
    uint64_t dataA0 = 0;           // +0xA0
    uint64_t dataA8 = 0;           // +0xA8
    uint64_t dataB0 = 0;           // +0xB0
    uint64_t dataB8 = 0;           // +0xB8
    uint64_t dataC0 = 0;           // +0xC0
    uint64_t dataC8 = 0;           // +0xC8
    uint64_t dataD0 = 0;           // +0xD0
    uint64_t dataD8 = 0;           // +0xD8
    uint64_t internalBasePtr = 0;  // +0xE0: Object::InternalBasePtr (init to null)

protected:
    // 0x140010fd0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail (0x140065990) frees the Array/String members at +0xd0,
    // +0xc0, +0xb0, +0xa0; the tree never did (leak kept).
    ~CitizenUI() = default;
};

static_assert(sizeof(CitizenUI) == 0xe8, "CitizenUI is 0xe8 (create 0x140010f80 MemAlloc(0xe8))");
static_assert(alignof(CitizenUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(CitizenUI, data30) == 0x30, "data30 @ 0x30");
static_assert(offsetof(CitizenUI, internalBasePtr) == 0xE0, "internalBasePtr @ 0xE0");
