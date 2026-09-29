#pragma once
// ============================================================================
// code/Game/Dialog/ProductionUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ProductionUI", parent
//   "UI::DialogController", table 0x14015d070, 11 slots, sizeof 0x50.
//   create 0x140013230: MemAlloc(0x50, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400132b0 scalar deleting dtor (~ProductionUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x140076300  RET 0
//   [4] 0x140075da0  OnCreated
//   [7] 0x140075a20  OnUpdateLayout
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"
#include "runtime/types/collection/Collection.h"

namespace UI { class LabelElement; class NumberElement; }

// One resource row of the production panel (0x10 bytes): the "labelStorage"
// LabelElement and the "numberCount" NumberElement (OnCreated 0x140075da0).
struct ProductionUIRow {
    UI::LabelElement*  labelStorage;   // +0x00
    UI::NumberElement* numberCount;    // +0x08
};
static_assert(sizeof(ProductionUIRow) == 0x10, "ProductionUIRow is 0x10");

class ProductionUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ProductionUI)

public:
    ProductionUI() = default;                            // 0x140013230 (ctor inlined in the factory)

    // 0x1400132b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail runs `eh vector destructor iterator` over the two Arrays at +0x30
    // (element dtor 0x140001740: MemFree, zero data / count / capacity), last
    // first: `lists`' own dtor does exactly that.
    ~ProductionUI() override = default;                           // [0] 0x1400132b0
    void Persist(void* /*stream*/) override {}   // [3] 0x140076300  RET 0 in retail
    void OnCreated() override;   // [4] 0x140075da0
    void OnUpdateLayout() override;   // [7] 0x140075a20

private:
    void UpdateList(int listIndex);                               // 0x140075a50

public:
    // ---- fields: retail's array of two Collection::Array<ProductionUIRow>
    // ([0] consumed, [1] produced; OnCreated 0x140075da0 appends the rows, the
    // helper 0x140075a50 indexes them by list, ~ProductionUI destroys both).
    Collection::Array<ProductionUIRow> lists[2];   // +0x30, +0x40
};

static_assert(sizeof(ProductionUI) == 0x50, "ProductionUI is 0x50 (create 0x140013230 MemAlloc(0x50))");
static_assert(alignof(ProductionUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(ProductionUI, lists) == 0x30, "ProductionUI +0x30 (the two lists, 0x10 each)");
