#pragma once
// ============================================================================
// code/Game/Dialog/TradeUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "TradeUI", parent
//   "UI::DialogController", table 0x14015de80, 11 slots, sizeof 0x148.
//   create 0x140015f80: MemAlloc(0x148, 8), ??0DialogController, stamp, fields.
//   [0] 0x140016020 scalar deleting dtor (~TradeUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x140084dd0  OnCreated
//   [5] 0x140082a70  OnOpened
//   [6] 0x140082a00  OnClosed
//   [7] 0x140082150  OnUpdateLayout
//   [8] 0x1400827b0  OnUpdate
//   [9] 0x140081f20  OnElementEvent
//   [10] 0x1400868c0  OnSerializeSettings
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class TradeUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(TradeUI)

public:
    TradeUI() = default;                                 // 0x140015f80 (ctor inlined in the factory)

    // 0x140016020 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail (0x140086a50) frees the Array/String members at +0x128,
    // +0xb0, +0xa0, +0x90, +0x80, +0x70, +0x60, +0x50, +0x40, +0x30; the tree never did
    // (leak kept).
    ~TradeUI() override = default;                                // [0] 0x140016020
    void OnCreated() override;   // [4] 0x140084dd0
    void OnOpened() override;   // [5] 0x140082a70
    void OnClosed() override;   // [6] 0x140082a00
    void OnUpdateLayout() override;   // [7] 0x140082150
    void OnUpdate(const System::Time& time) override;   // [8] 0x1400827b0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140081f20
    void OnSerializeSettings(void* stream) override;   // [10] 0x1400868c0

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
    uint64_t dataE0 = 0;           // +0xE0
    uint64_t dataE8 = 0;           // +0xE8
    uint64_t dataF0 = 0;           // +0xF0
    uint64_t dataF8 = 0;           // +0xF8
    uint64_t data100 = 0;          // +0x100
    uint64_t data108 = 0;          // +0x108
    uint64_t data110 = 0;          // +0x110
    uint64_t data118 = 0;          // +0x118
    uint64_t data120 = 0;          // +0x120
    uint64_t data128 = 0;          // +0x128
    uint64_t data130 = 0;          // +0x130
    uint64_t data138 = 0;          // +0x138
    uint64_t data140 = 0;          // +0x140
};

static_assert(sizeof(TradeUI) == 0x148, "TradeUI is 0x148 (create 0x140015f80 MemAlloc(0x148))");
static_assert(alignof(TradeUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(TradeUI, data30) == 0x30, "data30 @ 0x30");
