#pragma once
// ============================================================================
// code/Game/Dialog/MapUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "MapUI", parent
//   "UI::DialogController", table 0x14015c350, 11 slots, sizeof 0x50.
//   create 0x140012240: MemAlloc(0x50, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400124d0 scalar deleting dtor (~MapUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14006ce80  RET 0
//   [4] 0x14006ca80  OnCreated
//   [9] 0x14006c9e0  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class MapUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(MapUI)

public:
    MapUI() = default;                                   // 0x140012240 (ctor inlined in the factory)

    // 0x1400124d0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail (0x14006ce90) destroys the four owned objects at +0x38,
    // +0x30, +0x48, +0x40 through their slot 0 with flags 1; the tree never did (leak
    // kept).
    ~MapUI() override = default;                                  // [0] 0x1400124d0
    void Persist(void* /*stream*/) override {}   // [3] 0x14006ce80  RET 0 in retail
    void OnCreated() override;   // [4] 0x14006ca80
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14006c9e0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38
    uint64_t data40 = 0;           // +0x40
    uint64_t data48 = 0;           // +0x48
};

static_assert(sizeof(MapUI) == 0x50, "MapUI is 0x50 (create 0x140012240 MemAlloc(0x50))");
static_assert(alignof(MapUI) == 8, "qword-aligned (the vptr)");
