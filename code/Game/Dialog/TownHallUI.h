#pragma once
// ============================================================================
// code/Game/Dialog/TownHallUI.h — ENDGAME B.3.8 block PNL (B38_RULES D1-D5)
//
// RETAIL (Application-x64.exe, B38_SLOTS.md §6): reflected "TownHallUI", parent
//   "UI::DialogController", table 0x14015dce0, 11 slots, sizeof 0x1e8.
//   create 0x140014660: MemAlloc(0x1e8, 8), ??0DialogController, stamp, fields.
//   [0] 0x1400146b0 scalar deleting dtor (~TownHallUI + MemFree on &1)
//   [1] TypeName  [2] Type  (DECLARE_EXE_TYPE)
//   [4] 0x140081000  OnCreated
//   [7] 0x14007d550  OnUpdateLayout
//   [8] 0x14007e550  OnUpdate
//   [9] 0x14007c180  OnElementEvent
//   [10] 0x140080db0  OnSerializeSettings
//   every other slot is UI::DialogController's (inherited, not declared).
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class TownHallUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(TownHallUI)

public:
    TownHallUI() = default;                              // 0x140014660 (ctor inlined in the factory)

    // 0x1400146b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail (0x1400810d0) releases the owned trees/arrays at +0x1d8, +0x1c8, +0x138,
    // +0x118, +0xf0, +0xe0, +0xd0, +0xc0, +0xb0, +0x50 and +0x40; the tree's teardown
    // body (moved into ~TownHallUI unchanged) does the same frees.
    // the tree's teardown body (see the .cpp)
    ~TownHallUI() override;                                       // [0] 0x1400146b0
    void OnCreated() override;   // [4] 0x140081000
    void OnUpdateLayout() override;   // [7] 0x14007d550
    void OnUpdate(const System::Time& time) override;   // [8] 0x14007e550
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14007c180
    void OnSerializeSettings(void* stream) override;   // [10] 0x140080db0

    // ---- fields (the Panels.h POD's names, kept for the bodies) --------
    uint64_t tabElement = 0;       // +0x30: InternalBasePtr to TabElement
    uint32_t profButtonIdx = 0;    // +0x38: current profession button cycle index
    uint8_t _pad3C[4] = {};        // +0x3C: gap
    uint64_t profEntryArray = 0;   // +0x40: dynamic array ptr (0x40-byte profession entries)
    uint64_t profEntryMeta = 0;    // +0x48: [low32]=count, [high32]=capacity
    uint64_t profBST = 0;          // +0x50: profession BST root (sorted by type ID)
    uint32_t profBSTCount = 0;     // +0x58: profession BST node count
    uint8_t _pad5C[4] = {};        // +0x5C: gap
    // --- General tab elements ---
    uint64_t numberYear = 0;       // +0x60: NumberElement
    uint64_t textMonth = 0;        // +0x68: TextElement (label with sprite)
    uint64_t numberHomes = 0;      // +0x70: NumberElement
    uint64_t numberFamily = 0;     // +0x78: NumberElement
    uint64_t numberCitizens = 0;   // +0x80: NumberElement
    uint64_t numberAdults = 0;     // +0x88: NumberElement
    uint64_t numberStudents = 0;   // +0x90: NumberElement
    uint64_t numberChildren = 0;   // +0x98: NumberElement
    uint64_t percentClothed = 0;   // +0xA0: TextElement (formatted %)
    uint64_t percentEducated = 0;  // +0xA8: TextElement (formatted %)
    uint64_t groupHearts[4] = {};   // +0xB0: {wholeArray, wholeCount|wholeCap, halfArray, halfCount|halfCap}
    uint64_t groupStars[4] = {};    // +0xD0: {wholeArray, wholeCount|wholeCap, halfArray, halfCount|halfCap}
    // --- Production tab elements ---
    uint64_t prodLimitsArray = 0;  // +0xF0: dynamic array ptr (0x28-byte limit entries)
    uint64_t prodLimitsMeta = 0;   // +0xF8: [low32]=count, [high32]=capacity
    uint64_t productionCombo = 0;  // +0x100: ComboElement
    uint64_t limitPrevUsedHdr = 0; // +0x108: TextElement
    uint64_t limitPrevProdHdr = 0; // +0x110: TextElement
    // --- Inventory tab elements ---
    uint64_t invTableArray = 0;    // +0x118: dynamic array ptr (0x10-byte entries: label, number)
    uint32_t invTableCount = 0;    // +0x120: count
    uint32_t invTableCap = 0;      // +0x124: capacity
    uint64_t inventoryCombo = 0;   // +0x128: ComboElement
    uint64_t sortCombo = 0;        // +0x130: ComboElement (sort order)
    uint64_t invBST = 0;           // +0x138: inventory BST root
    uint32_t invBSTDummy = 0;      // +0x140: zeroed (u32 write)
    uint8_t _pad144[4] = {};       // +0x144: gap
    int32_t cachedGameTick = 0;   // +0x148: cached game time tick for graph refresh
    uint8_t _pad14C[4] = {};       // +0x14C: gap
    // --- Graph tab elements ---
    uint64_t graphYearsTable = 0;  // +0x150: StringTable (graphYears)
    uint64_t graphTypesTable = 0;  // +0x158: StringTable (graphTypes)
    uint64_t graphData = 0;        // +0x160: GraphElement
    uint64_t timeCombo = 0;        // +0x168: ComboElement (time range)
    uint64_t graphCombo = 0;       // +0x170: ComboElement (graph data type)
    uint64_t textXMin = 0;         // +0x178: TextElement (x-axis min label)
    uint64_t number0 = 0;          // +0x180: NumberElement (y-axis max)
    uint64_t number1 = 0;          // +0x188: NumberElement (y-axis 3/4)
    uint64_t number2 = 0;          // +0x190: NumberElement (y-axis 1/2)
    uint64_t number3 = 0;          // +0x198: NumberElement (y-axis 1/4)
    uint64_t number4 = 0;          // +0x1A0: NumberElement (y-axis min) — unused?
    uint64_t label0 = 0;           // +0x1A8: LabelElement (graph legend 0)
    uint64_t label1 = 0;           // +0x1B0: LabelElement (graph legend 1)
    uint64_t label2 = 0;           // +0x1B8: LabelElement (graph legend 2)
    uint64_t label3 = 0;           // +0x1C0: LabelElement (graph legend 3)
    // --- Agriculture tab elements ---
    uint64_t agriLabelArray = 0;   // +0x1C8: dynamic array ptr (8-byte label ptrs)
    uint32_t agriLabelCount = 0;   // +0x1D0: count
    uint32_t agriLabelCap = 0;     // +0x1D4: capacity
    uint64_t agriBST = 0;          // +0x1D8: agriculture BST root (sorted by entity ptr)
    uint32_t agriBSTCount = 0;     // +0x1E0: agriculture BST item count
    uint8_t _pad1E4[4] = {};       // +0x1E4: gap
};

static_assert(sizeof(TownHallUI) == 0x1e8, "TownHallUI is 0x1e8 (create 0x140014660 MemAlloc(0x1e8))");
static_assert(alignof(TownHallUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(TownHallUI, tabElement) == 0x30, "tabElement @ 0x30");
static_assert(offsetof(TownHallUI, numberYear) == 0x60, "numberYear @ 0x60");
static_assert(offsetof(TownHallUI, productionCombo) == 0x100, "productionCombo @ 0x100");
static_assert(offsetof(TownHallUI, inventoryCombo) == 0x128, "inventoryCombo @ 0x128");
static_assert(offsetof(TownHallUI, cachedGameTick) == 0x148, "cachedGameTick @ 0x148");
static_assert(offsetof(TownHallUI, graphData) == 0x160, "graphData @ 0x160");
static_assert(offsetof(TownHallUI, agriBSTCount) == 0x1E0, "agriBSTCount @ 0x1E0");
