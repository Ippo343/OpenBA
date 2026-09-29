#pragma once
// ============================================================================
// code/Game/Dialog/BuildRequirementUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "BuildRequirementUI", parent
//   "UI::DialogController", table 0x14015b1d0, 11 slots, sizeof 0x60.
//   create 0x1400107f0: MemAlloc(0x60, 8), ??0DialogController, stamp, fields.
//   [0] 0x140010860 scalar deleting dtor (~BuildRequirementUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [3] 0x14005ee80  RET 0
//   [4] 0x14005e7c0  OnCreated
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ComponentDescription;
namespace UI { class Element; }

class BuildRequirementUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(BuildRequirementUI)

public:
    BuildRequirementUI() = default;                      // 0x1400107f0 (ctor inlined in the factory)

    // 0x140010860 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail frees the Array/String members at +0x50 and +0x40 before
    // ~DialogController; the tree never did (leak kept).
    ~BuildRequirementUI() override = default;                     // [0] 0x140010860
    void Persist(void* /*stream*/) override {}   // [3] 0x14005ee80  RET 0 in retail
    void OnCreated() override;   // [4] 0x14005e7c0

    // ---- fields (B4 DLG-a) ----------------------------------------------
    // +0x30 the building's ComponentDescription (resolved from the data source)
    ComponentDescription* description = nullptr;
    uint32_t field_0x38 = 1;           // +0x38: initialized to 1 by the factory
    uint8_t _pad3C[4] = {};            // +0x3C: gap
    // +0x40 Collection::Array<InternalPtr<Element>>: the "labelBuild%i" labels;
    // +0x50 Collection::Array<int32_t>: each cost's quantity. Both as their raw
    // members (H5: retail's dtor frees them, the tree's does not).
    UI::Element** labelData = nullptr; // +0x40
    int32_t labelCount = 0;            // +0x48
    int32_t labelCapacity = 0;         // +0x4C
    int32_t* countData = nullptr;      // +0x50
    int32_t countCount = 0;            // +0x58
    int32_t countCapacity = 0;         // +0x5C
};

static_assert(sizeof(BuildRequirementUI) == 0x60, "BuildRequirementUI is 0x60 (create 0x1400107f0 MemAlloc(0x60))");
static_assert(alignof(BuildRequirementUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(BuildRequirementUI, description) == 0x30, "BuildRequirementUI::description @ +0x30");
static_assert(offsetof(BuildRequirementUI, field_0x38) == 0x38, "BuildRequirementUI::field_0x38 @ +0x38");
static_assert(offsetof(BuildRequirementUI, labelData) == 0x40, "BuildRequirementUI::labelData @ +0x40");
static_assert(offsetof(BuildRequirementUI, labelCount) == 0x48, "BuildRequirementUI::labelCount @ +0x48");
static_assert(offsetof(BuildRequirementUI, labelCapacity) == 0x4C, "BuildRequirementUI::labelCapacity @ +0x4C");
static_assert(offsetof(BuildRequirementUI, countData) == 0x50, "BuildRequirementUI::countData @ +0x50");
static_assert(offsetof(BuildRequirementUI, countCount) == 0x58, "BuildRequirementUI::countCount @ +0x58");
static_assert(offsetof(BuildRequirementUI, countCapacity) == 0x5C, "BuildRequirementUI::countCapacity @ +0x5C");
