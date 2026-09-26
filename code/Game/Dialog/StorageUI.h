#pragma once
// ============================================================================
// code/Game/Dialog/StorageUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "StorageUI", parent
//   "UI::DialogController", table 0x14015f970, 11 slots, sizeof 0x40.
//   create 0x140021330: MemAlloc(0x40, 8), ??0DialogController, stamp, fields.
//   [0] 0x140021390 scalar deleting dtor (~StorageUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x1400b8ca0  RET 0
//   [4] 0x1400b8820  OnCreated
//   [7] 0x1400b8450  OnUpdateLayout
//   [8] retail inherits the base; the tree overrides it (STANDALONE DEVIATION)
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class StorageUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(StorageUI)

public:
    StorageUI() = default;                               // 0x140021330 (ctor inlined in the factory)

    Object::Base* DeletingDtor(unsigned flags) override;          // [0] 0x140021390
    void Persist(void* /*stream*/) override {}   // [3] 0x1400b8ca0  RET 0 in retail
    void OnCreated() override;   // [4] 0x1400b8820
    void OnUpdateLayout() override;   // [7] 0x1400b8450
    // [8] STANDALONE DEVIATION: retail inherits DialogController::OnUpdate (RET); the tree drives this panel's per-frame refresh here.
    void OnUpdate(const System::Time& time) override;

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t data30 = 0;           // +0x30: zeroed
    uint64_t data38 = 0;           // +0x38: zeroed

protected:
    // 0x140021390 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail frees the row array at +0x30; the tree's teardown body (moved into
    // ~StorageUI unchanged) does the same.
    ~StorageUI();   // the tree's teardown body (see the .cpp)
};

static_assert(sizeof(StorageUI) == 0x40, "StorageUI is 0x40 (create 0x140021330 MemAlloc(0x40))");
static_assert(alignof(StorageUI) == 8, "qword-aligned (the vptr)");
