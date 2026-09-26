#pragma once
// ============================================================================
// code/Game/Dialog/StorageCapacityUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "StorageCapacityUI", parent
//   "UI::DialogController", table 0x14015d950, 11 slots, sizeof 0x38.
//   create 0x1400143f0: MemAlloc(0x38, 8), ??0DialogController, stamp, fields.
//   [0] 0x140014450 scalar deleting dtor (~StorageCapacityUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14007b6b0  RET 0
//   [4] 0x14007b600  OnCreated
//   [7] 0x14007b4d0  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class StorageCapacityUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(StorageCapacityUI)

public:
    StorageCapacityUI() = default;                       // 0x1400143f0 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140014450
    void Persist(void* /*stream*/) override {}   // [3] 0x14007b6b0  RET 0 in retail
    void OnCreated() override;   // [4] 0x14007b600
    void OnUpdateLayout() override;   // [7] 0x14007b4d0
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed

protected:
    // 0x140014450 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    ~StorageCapacityUI() = default;
};

static_assert(sizeof(StorageCapacityUI) == 0x38, "StorageCapacityUI is 0x38 (create 0x1400143f0 MemAlloc(0x38))");
static_assert(alignof(StorageCapacityUI) == 8, "qword-aligned (the vptr)");
