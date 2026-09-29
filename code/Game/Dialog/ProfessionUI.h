#pragma once
// ============================================================================
// code/Game/Dialog/ProfessionUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "ProfessionUI", parent
//   "UI::DialogController", table 0x14015d0f0, 11 slots, sizeof 0x78.
//   create 0x140013400: MemAlloc(0x78, 8), ??0DialogController, stamp, fields.
//   [0] 0x140013450 scalar deleting dtor (~ProfessionUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [5] 0x140076ca0  OnOpened
//   [8] 0x1400765a0  OnUpdate
//   [9] 0x140076c10  OnElementEvent
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class Profession;   // Game/Profession.h
namespace UI { class NumberEditElement; class NumberElement; }

class ProfessionUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(ProfessionUI)

public:
    ProfessionUI() = default;                            // 0x140013400 (ctor inlined in the factory)

    // 0x140013450 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // PORT GAP (B38 D2): retail's dtor mirrors ctor 0x1400775a0 (owning members released);
    // the tree never did (leak kept).
    ~ProfessionUI() override = default;                           // [0] 0x140013450
    void OnOpened() override;   // [5] 0x140076ca0
    void OnUpdate(const System::Time& time) override;   // [8] 0x1400765a0
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x140076c10

    // Retail's own Collection::Set node (0x30 bytes): a profession -> its row index
    // (OnOpened 0x140076ca0 inserts, OnUpdate 0x1400765a0 looks up; the AVL
    // rebalance is 0x140013480, the rotations 0x14000e450 / 0x14000e580).
    struct ProfessionIndexNode {
        Profession*          key;      // +0x00 ExternalPtr<Profession>
        int32_t              index;    // +0x08 the row
        int32_t              _pad0C;   // +0x0C
        ProfessionIndexNode* parent;   // +0x10
        ProfessionIndexNode* left;     // +0x18
        ProfessionIndexNode* right;    // +0x20
        int16_t              balance;  // +0x28 right height - left height
        int16_t              height;   // +0x2A
        int32_t              _pad2C;   // +0x2C
    };

private:
    // Non-virtual members (retail passes the controller in RCX).
    void HandleSpinner(int profIndex);                             // 0x1400763c0
    void HandleButton(int profIndex);                              // 0x140076a30

public:
    // ---- fields: retail's three Collection::Arrays and one Set, released by
    // ~ProfessionUI 0x1400774e0. Raw storage until the tree's dtor does the same
    // (H5); the zero initialisers are the create path's stores (0x140013400).
    int32_t*  countsData = nullptr;        // +0x30 Array<int>: citizens per profession row
    int32_t   countsCount = 0;             // +0x38
    int32_t   countsCapacity = 0;          // +0x3C
    UI::NumberEditElement** spinnerData = nullptr; // +0x40 Array<InternalPtr<NumberEditElement>>
    int32_t   spinnerCount = 0;            // +0x48
    int32_t   spinnerCapacity = 0;         // +0x4C
    UI::NumberElement** numberData = nullptr;      // +0x50 Array<InternalPtr<NumberElement>>
    int32_t   numberCount = 0;             // +0x58
    int32_t   numberCapacity = 0;          // +0x5C
    ProfessionIndexNode* professionIndexRoot = nullptr; // +0x60 Set root
    int32_t   professionIndexCount = 0;    // +0x68 (u32 write)
    uint8_t _pad6C[4] = {};                // +0x6C: gap
    int32_t   gotoIndex = 0;               // +0x70 the citizen HandleButton cycles through (u32 write)
    uint8_t _pad74[4] = {};                // +0x74: gap
};

static_assert(sizeof(ProfessionUI) == 0x78, "ProfessionUI is 0x78 (create 0x140013400 MemAlloc(0x78))");
static_assert(alignof(ProfessionUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(ProfessionUI, countsData) == 0x30, "countsData @ 0x30");
static_assert(offsetof(ProfessionUI, countsCount) == 0x38, "countsCount @ 0x38");
static_assert(offsetof(ProfessionUI, countsCapacity) == 0x3C, "countsCapacity @ 0x3C");
static_assert(offsetof(ProfessionUI, spinnerData) == 0x40, "spinnerData @ 0x40");
static_assert(offsetof(ProfessionUI, spinnerCount) == 0x48, "spinnerCount @ 0x48");
static_assert(offsetof(ProfessionUI, spinnerCapacity) == 0x4C, "spinnerCapacity @ 0x4C");
static_assert(offsetof(ProfessionUI, numberData) == 0x50, "numberData @ 0x50");
static_assert(offsetof(ProfessionUI, numberCount) == 0x58, "numberCount @ 0x58");
static_assert(offsetof(ProfessionUI, numberCapacity) == 0x5C, "numberCapacity @ 0x5C");
static_assert(offsetof(ProfessionUI, professionIndexRoot) == 0x60, "professionIndexRoot @ 0x60");
static_assert(offsetof(ProfessionUI, professionIndexCount) == 0x68, "professionIndexCount @ 0x68");
static_assert(offsetof(ProfessionUI, gotoIndex) == 0x70, "gotoIndex @ 0x70");
static_assert(sizeof(ProfessionUI::ProfessionIndexNode) == 0x30, "the Set node is 0x30 (MemAlloc(0x30, 8))");
static_assert(offsetof(ProfessionUI::ProfessionIndexNode, index) == 0x08, "node +0x08");
static_assert(offsetof(ProfessionUI::ProfessionIndexNode, parent) == 0x10, "node +0x10");
static_assert(offsetof(ProfessionUI::ProfessionIndexNode, left) == 0x18, "node +0x18");
static_assert(offsetof(ProfessionUI::ProfessionIndexNode, right) == 0x20, "node +0x20");
static_assert(offsetof(ProfessionUI::ProfessionIndexNode, balance) == 0x28, "node +0x28");
static_assert(offsetof(ProfessionUI::ProfessionIndexNode, height) == 0x2A, "node +0x2a");
