#pragma once
// ============================================================================
// code/Game/Dialog/NoiseUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "NoiseUI", parent
//   "UI::DialogController", table 0x14015ca80, 11 slots, sizeof 0x70.
//   create 0x140012d30: MemAlloc(0x70, 8), ??0DialogController, stamp, fields.
//   [0] 0x140012da0 scalar deleting dtor (~NoiseUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x140071bb0  RET 0
//   [4] 0x1400714c0  OnCreated
//   [9] 0x1400714a0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class NoiseUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(NoiseUI)

public:
    NoiseUI() = default;                                 // 0x140012d30 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140012da0
    void Persist(void* /*stream*/) override {}   // [3] 0x140071bb0  RET 0 in retail
    void OnCreated() override;   // [4] 0x1400714c0  PORT GAP
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x1400714a0  PORT GAP

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint8_t _pad30[8] = {};        // +0x30: gap (not initialized by factory)
    uint64_t data38 = 0;           // +0x38: zeroed
    uint64_t data40 = 0;           // +0x40
    uint64_t data48 = 0;           // +0x48
    uint64_t data50 = 0;           // +0x50
    uint64_t data58 = 0;           // +0x58
    uint64_t data60 = 0;           // +0x60
    uint64_t data68 = 0;           // +0x68

protected:
    // 0x140012da0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail (0x140071bc0) destroys the three owned objects at +0x40,
    // +0x38, +0x30 through their slot 0 with flags 1; not transcribed.
    ~NoiseUI() = default;
};

static_assert(sizeof(NoiseUI) == 0x70, "NoiseUI is 0x70 (create 0x140012d30 MemAlloc(0x70))");
static_assert(alignof(NoiseUI) == 8, "qword-aligned (the vptr)");
