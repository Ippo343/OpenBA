#pragma once
// ============================================================================
// code/Game/Dialog/CropUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (B38_SLOTS.md §6): reflected "CropUI", parent "SelectResourceUI",
//   table 0x14015f270, 14 slots, sizeof 0x60. create 0x140020b10 ->
//   ctor 0x1400b2c90: ??0DialogController, stamp 0x14015d330, InternalBasePtr
//   (+0x30, null), stamp 0x14015f270, +0x38..+0x58 = 0.
//   [0] 0x140020b60 scalar deleting dtor
//   [4] 0x1400b2b50  OnCreated (the base's 0x140078be0, then "yield" at +0x38)
//   [7] 0x1400b2a50  OnUpdateLayout (maturity bar, then the base's 0x140078cb0)
//   [8] [9] = parent (SelectResourceUI's 0x140078d30 / 0x140078d80)
//   [11] 0x1400b2920 SetResource  [12] 0x1400b29d0 GetResource
//   [13] 0x1400b29c0 GetCategory -> 0
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Dialog/SelectResourceUI.h"
#include "Object/exe_type.h"

namespace UI { class ProgressElement; }

class CropUI final : public SelectResourceUI {
    DECLARE_EXE_TYPE(CropUI)

public:
    CropUI() = default;                                           // 0x1400b2c90

    // PORT GAP (B38 D2): retail's teardown helper 0x1400b2c10 frees the
    // Array/String members at +0x50 and +0x40; the tree never did (leak kept).
    ~CropUI() override = default;                                 // [0]  0x140020b60
    void  OnCreated() override;                                   // [4]  0x1400b2b50
    void  OnUpdateLayout() override;                              // [7]  0x1400b2a50
    // [8] STANDALONE DEVIATION: retail inherits SelectResourceUI's 0x140078d30;
    // the tree wraps it with a Slot4 self-heal and a liveness guard.
    void  OnUpdate(const System::Time& time) override;
    void  SetResource(ComponentDescription* type) override;       // [11] 0x1400b2920
    ComponentDescription** GetResource(ComponentDescription** out) override;  // [12] 0x1400b29d0
    int   GetCategory() override;                                 // [13] 0x1400b29c0  XOR EAX,EAX

    // ---- fields (the Panels.h POD's names) -----------------------------------
    UI::ProgressElement* yield = nullptr;  // +0x38: "yield" ProgressElement (0x1400b2b50)
    // +0x40 / +0x50: two Array/String members (H5 raw: retail's teardown frees them)
    uint64_t data40 = 0;           // +0x40
    uint64_t data48 = 0;           // +0x48
    uint64_t data50 = 0;           // +0x50
    uint64_t data58 = 0;           // +0x58
};

static_assert(sizeof(CropUI) == 0x60, "CropUI is 0x60 (create 0x140020b10 MemAlloc(0x60))");
static_assert(alignof(CropUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(CropUI, yield) == 0x38, "yield @ +0x38 (0x1400b2b50)");
static_assert(offsetof(CropUI, data40) == 0x40, "+0x40");
static_assert(offsetof(CropUI, data50) == 0x50, "+0x50");
