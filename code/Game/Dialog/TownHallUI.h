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
//
// ~TownHallUI 0x1400810d0: frees the agriculture tree (+0x1d8) and label array
// (+0x1c8), the inventory tree (+0x138) and row array (+0x118), the production
// rows (+0xf0), the two rating groups (+0xd0, +0xb0: each two arrays), the
// profession tree (+0x50) and the profession rows (+0x40, 0x140015930).
//
// The element pointers are retail's InternalPtr<X> (8 bytes, no refcount),
// spelled as the typed raw pointer (B4 H4); each is the FindElement result of
// the resource element its comment names (InitGeneral 0x1400807f0 and the
// other Init bodies).
//
// H5: the arrays stay raw {data, count, capacity} storage: the tree's
// ~TownHallUI frees them by hand in retail's order, and the rows are copied
// byte-wise by the exe helpers (TownHall_ProfArrayPush 0x140015800,
// TownHall_ProdArrayPush 0x1400159b0). A real Collection::Array member would
// add a destructor run.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "runtime/types/ui/DialogController.h"
#include "Object/exe_type.h"

class ComponentEntity;
namespace UI {
class ButtonElement;
class ComboElement;
class GraphElement;
class LabelElement;
class NumberEditElement;
class NumberElement;
class StringTable;
class TabElement;
class TextElement;
}

class TownHallUI final : public UI::DialogController {
    DECLARE_EXE_TYPE(TownHallUI)

public:
    // +0x40: one row per profession (InitProfessions 0x140080160), stride 0x40.
    struct ProfessionRow {
        int32_t                workers;        // +0x00 adult citizens with the profession
        int32_t                slots;          // +0x04 worker slots over its buildings
        UI::LabelElement*      label;          // +0x08 row child(0)
        UI::NumberEditElement* desired;        // +0x10 content child(0), id 0x1966 + i
        UI::NumberElement*     slotsNumber;    // +0x18 row child(2)
        UI::NumberElement*     buildingsNumber;// +0x20 row child(3)
        UI::ButtonElement*     button;         // +0x28 row child(4), id 0x1999 + i
        // +0x30 Collection::Array<ComponentEntity*> (H5 raw: MemFree'd by hand)
        ComponentEntity**      entities;       // +0x30
        int32_t                entityCount;    // +0x38
        int32_t                entityCapacity; // +0x3C
    };
    // +0xB0 / +0xD0: a rating group (TownHall_GroupRatingInit 0x14006abf0):
    // two arrays of the group's whole and half icons (H5 raw).
    struct RatingGroup {
        void*   whole;          // +0x00
        int32_t wholeCount;     // +0x08
        int32_t wholeCapacity;  // +0x0C
        void*   half;           // +0x10
        int32_t halfCount;      // +0x18
        int32_t halfCapacity;   // +0x1C
    };
    // +0xF0: one row per "limits" table row (InitProduction 0x14007fc30), stride 0x28.
    struct ProductionRow {
        UI::NumberElement* stock;         // +0x00
        UI::NumberElement* used;          // +0x08 this period's use
        UI::NumberElement* produced;      // +0x10 this period's production
        UI::NumberElement* usedPrev;      // +0x18 the previous span's use
        UI::NumberElement* producedPrev;  // +0x20 the previous span's production
    };
    // +0x118: one per "inventoryTable" cell (InitInventory 0x14007f660), stride 0x10.
    struct InventoryRow {
        UI::LabelElement*  label;         // +0x00 cell child(0)
        UI::NumberElement* count;         // +0x08 cell child(1)
    };
    // +0x50: the profession tree node (0x30; key = the Profession, value = its row).
    struct ProfessionNode {
        uint64_t        key;       // +0x00
        uint64_t        index;     // +0x08
        ProfessionNode* parent;    // +0x10
        ProfessionNode* left;      // +0x18
        ProfessionNode* right;     // +0x20
        int32_t         balance;   // +0x28
        int32_t         _pad2c;    // +0x2C
    };
    // +0x138: the inventory tree node (0x38; key = the raw material's description).
    struct InventoryNode {
        uint64_t       key;        // +0x00
        int32_t        counts[3];  // +0x08 used / produced / storage (UpdateAll 0x14007d590)
        int32_t        _pad14;     // +0x14
        InventoryNode* parent;     // +0x18
        InventoryNode* left;       // +0x20
        InventoryNode* right;      // +0x28
        int32_t        balance;    // +0x30
        int32_t        _pad34;     // +0x34
    };
    // +0x1D8: the agriculture tree node (0x28; key = the building's description).
    struct AgricultureNode {
        uint64_t         key;      // +0x00
        AgricultureNode* parent;   // +0x08
        AgricultureNode* left;     // +0x10
        AgricultureNode* right;    // +0x18
        int32_t          balance;  // +0x20
        int32_t          _pad24;   // +0x24
    };

    TownHallUI() = default;                              // 0x140014660 (ctor inlined in the factory)

    // 0x1400146b0 minus the free. The compiler chains ~DialogController (and so
    // ~Internal: RemoveObject), which the tree's hand-built slot 0 skipped.
    // Retail (0x1400810d0) releases the owned trees/arrays at +0x1d8, +0x1c8, +0x138,
    // +0x118, +0xf0, +0xe0, +0xd0, +0xc0, +0xb0, +0x50 and +0x40; the tree's teardown
    // body (moved into ~TownHallUI unchanged) does the same frees.
    ~TownHallUI() override;                                       // [0] 0x1400146b0
    void OnCreated() override;   // [4] 0x140081000
    void OnUpdateLayout() override;   // [7] 0x14007d550
    void OnUpdate(const System::Time& time) override;   // [8] 0x14007e550
    void OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t a, int32_t b,
                        int32_t c, int32_t d, int32_t e, void* userData) override;   // [9] 0x14007c180
    void OnSerializeSettings(IO::Stream& stream) override;   // [10] 0x140080db0

    // ---- non-virtual members (retail addresses) ---------------------------
    void UpdateAgriculture();                   // 0x14007c220
    void UpdateProduction();                    // 0x14007c7f0 (inventory tab)
    void UpdateProfessions();                   // 0x14007ccb0 (production tab)
    void UpdateGeneral();                       // 0x14007d030
    void UpdateAll();                           // 0x14007d590
    void OnProfessionButton(int index);         // 0x14007dca0
    void OnProfessionSpinner(int index);        // 0x14007dd10
    void UpdateInventory();                     // 0x14007def0 (professions tab)
    void UpdateGraph();                         // 0x14007e5b0
    void InitAgriculture();                     // 0x14007ebf0
    void InitGraph();                           // 0x14007f0a0
    void InitInventory();                       // 0x14007f660
    void InitProduction();                      // 0x14007fc30
    void InitProfessions();                     // 0x140080160
    void InitGeneral();                         // 0x1400807f0

    // ---- fields --------------------------------------------------------------
    UI::TabElement* tabElement = nullptr;         // +0x30 "tabs"
    uint32_t profButtonIdx = 0;                   // +0x38 current profession button cycle index
    uint8_t _pad3C[4] = {};                       // +0x3C gap
    ProfessionRow* professionRows = nullptr;      // +0x40 Array<ProfessionRow> data (H5 raw)
    int32_t professionRowCount = 0;               // +0x48
    int32_t professionRowCapacity = 0;            // +0x4C
    ProfessionNode* professionTree = nullptr;     // +0x50 profession tree root (sorted by Profession)
    uint32_t professionTreeCount = 0;             // +0x58
    uint8_t _pad5C[4] = {};                       // +0x5C gap
    // --- General tab elements ---
    UI::NumberElement* numberYear = nullptr;      // +0x60 "numberYear"
    UI::TextElement* textMonth = nullptr;         // +0x68 "textMonth"
    UI::NumberElement* numberHomes = nullptr;     // +0x70 "numberHomes"
    UI::NumberElement* numberFamily = nullptr;    // +0x78 "numberFamily"
    UI::NumberElement* numberCitizens = nullptr;  // +0x80 "numberCitizens"
    UI::NumberElement* numberAdults = nullptr;    // +0x88 "numberAdults"
    UI::NumberElement* numberStudents = nullptr;  // +0x90 "numberStudents"
    UI::NumberElement* numberChildren = nullptr;  // +0x98 "numberChildren"
    UI::TextElement* percentClothed = nullptr;    // +0xA0 "percentClothed" (retail writes the EDUCATED % here, 0x14007d030)
    UI::TextElement* percentEducated = nullptr;   // +0xA8 "percentEducated" (retail writes the CLOTHED % here)
    RatingGroup groupHearts = {};                 // +0xB0 "groupHearts"
    RatingGroup groupStars = {};                  // +0xD0 "groupStars"
    // --- Production tab elements ---
    ProductionRow* prodLimits = nullptr;          // +0xF0 Array<ProductionRow> data (H5 raw)
    int32_t prodLimitsCount = 0;                  // +0xF8
    int32_t prodLimitsCapacity = 0;               // +0xFC
    UI::ComboElement* productionCombo = nullptr;  // +0x100 "productionCombo"
    UI::TextElement* limitPrevUsedHdr = nullptr;  // +0x108 "limitPrevUsedHeader"
    UI::TextElement* limitPrevProdHdr = nullptr;  // +0x110 "limitPrevProducedHeader"
    // --- Inventory tab elements ---
    InventoryRow* invTable = nullptr;             // +0x118 Array<InventoryRow> data (H5 raw)
    int32_t invTableCount = 0;                    // +0x120
    int32_t invTableCap = 0;                      // +0x124
    UI::ComboElement* inventoryCombo = nullptr;   // +0x128 "inventoryCombo"
    UI::ComboElement* sortCombo = nullptr;        // +0x130 "sortCombo" (sort order)
    InventoryNode* invBST = nullptr;              // +0x138 inventory tree root
    uint32_t invBSTCount = 0;                     // +0x140
    uint8_t _pad144[4] = {};                      // +0x144 gap
    int32_t cachedGameTick = 0;                   // +0x148 cached game time tick for graph refresh
    uint8_t _pad14C[4] = {};                      // +0x14C gap
    // --- Graph tab elements ---
    UI::StringTable* graphYearsTable = nullptr;   // +0x150 "Dialog/StringTable.rsc:graphYears"
    UI::StringTable* graphTypesTable = nullptr;   // +0x158 "Dialog/StringTable.rsc:graphTypes"
    UI::GraphElement* graphData = nullptr;        // +0x160 "graphData"
    UI::ComboElement* timeCombo = nullptr;        // +0x168 "timeCombo" (time range)
    UI::ComboElement* graphCombo = nullptr;       // +0x170 "graphCombo" (graph data type)
    UI::TextElement* textXMin = nullptr;          // +0x178 "textXMin" (x-axis min label)
    UI::NumberElement* number0 = nullptr;         // +0x180 "number0" (y-axis max)
    UI::NumberElement* number1 = nullptr;         // +0x188 "number1" (y-axis 3/4)
    UI::NumberElement* number2 = nullptr;         // +0x190 "number2" (y-axis 1/2)
    UI::NumberElement* number3 = nullptr;         // +0x198 "number3" (y-axis 1/4)
    UI::NumberElement* number4 = nullptr;         // +0x1A0 "number4" (y-axis min)
    UI::LabelElement* label0 = nullptr;           // +0x1A8 "label0" (graph legend 0)
    UI::LabelElement* label1 = nullptr;           // +0x1B0 "label1" (graph legend 1)
    UI::LabelElement* label2 = nullptr;           // +0x1B8 "label2" (graph legend 2)
    UI::LabelElement* label3 = nullptr;           // +0x1C0 "label3" (graph legend 3)
    // --- Agriculture tab elements ---
    UI::LabelElement** agriLabels = nullptr;      // +0x1C8 Array<LabelElement*> data (H5 raw)
    int32_t agriLabelCount = 0;                   // +0x1D0
    int32_t agriLabelCap = 0;                     // +0x1D4
    AgricultureNode* agriBST = nullptr;           // +0x1D8 agriculture tree root (sorted by description)
    uint32_t agriBSTCount = 0;                    // +0x1E0
    // +0x1E4: tail padding in retail (nothing reads it); the tree's OnUpdate
    // counts its throttled OnUpdateLayout refresh here (STANDALONE DEVIATION).
    uint32_t _pad1E4 = 0;                         // +0x1E4
};

static_assert(sizeof(TownHallUI::ProfessionRow) == 0x40, "profession row stride 0x40 (0x140015800)");
static_assert(offsetof(TownHallUI::ProfessionRow, entities) == 0x30, "ProfessionRow::entities @ +0x30");
static_assert(sizeof(TownHallUI::RatingGroup) == 0x20, "rating group 0x20 (0x1400811c0)");
static_assert(sizeof(TownHallUI::ProductionRow) == 0x28, "production row stride 0x28 (0x1400159b0)");
static_assert(sizeof(TownHallUI::InventoryRow) == 0x10, "inventory row stride 0x10");
static_assert(sizeof(TownHallUI::ProfessionNode) == 0x30, "profession node 0x30 (MemAlloc(0x30))");
static_assert(offsetof(TownHallUI::ProfessionNode, left) == 0x18, "ProfessionNode::left @ +0x18 (0x140081234)");
static_assert(sizeof(TownHallUI::InventoryNode) == 0x38, "inventory node 0x38");
static_assert(offsetof(TownHallUI::InventoryNode, left) == 0x20, "InventoryNode::left @ +0x20 (0x140081163)");
static_assert(sizeof(TownHallUI::AgricultureNode) == 0x28, "agriculture node 0x28 (MemAlloc(0x28))");
static_assert(offsetof(TownHallUI::AgricultureNode, left) == 0x10, "AgricultureNode::left @ +0x10 (0x14008110b)");

static_assert(sizeof(TownHallUI) == 0x1e8, "TownHallUI is 0x1e8 (create 0x140014660 MemAlloc(0x1e8))");
static_assert(alignof(TownHallUI) == 8, "qword-aligned (the vptr)");
static_assert(offsetof(TownHallUI, tabElement) == 0x30, "tabElement @ 0x30");
static_assert(offsetof(TownHallUI, profButtonIdx) == 0x38, "profButtonIdx @ 0x38");
static_assert(offsetof(TownHallUI, professionRows) == 0x40, "professionRows @ 0x40 (dtor 0x140081254)");
static_assert(offsetof(TownHallUI, professionRowCount) == 0x48, "professionRowCount @ 0x48");
static_assert(offsetof(TownHallUI, professionTree) == 0x50, "professionTree @ 0x50 (dtor 0x14008121c)");
static_assert(offsetof(TownHallUI, professionTreeCount) == 0x58, "professionTreeCount @ 0x58 (dtor 0x140081251)");
static_assert(offsetof(TownHallUI, numberYear) == 0x60, "numberYear @ 0x60");
static_assert(offsetof(TownHallUI, textMonth) == 0x68, "textMonth @ 0x68");
static_assert(offsetof(TownHallUI, numberHomes) == 0x70, "numberHomes @ 0x70");
static_assert(offsetof(TownHallUI, numberFamily) == 0x78, "numberFamily @ 0x78");
static_assert(offsetof(TownHallUI, numberCitizens) == 0x80, "numberCitizens @ 0x80");
static_assert(offsetof(TownHallUI, numberAdults) == 0x88, "numberAdults @ 0x88");
static_assert(offsetof(TownHallUI, numberStudents) == 0x90, "numberStudents @ 0x90");
static_assert(offsetof(TownHallUI, numberChildren) == 0x98, "numberChildren @ 0x98");
static_assert(offsetof(TownHallUI, percentClothed) == 0xA0, "percentClothed @ 0xA0");
static_assert(offsetof(TownHallUI, percentEducated) == 0xA8, "percentEducated @ 0xA8");
static_assert(offsetof(TownHallUI, groupHearts) == 0xB0, "groupHearts @ 0xB0 (dtor 0x1400811ee)");
static_assert(offsetof(TownHallUI, groupStars) == 0xD0, "groupStars @ 0xD0 (dtor 0x1400811c0)");
static_assert(offsetof(TownHallUI, prodLimits) == 0xF0, "prodLimits @ 0xF0 (dtor 0x1400811a5)");
static_assert(offsetof(TownHallUI, prodLimitsCount) == 0xF8, "prodLimitsCount @ 0xF8");
static_assert(offsetof(TownHallUI, productionCombo) == 0x100, "productionCombo @ 0x100");
static_assert(offsetof(TownHallUI, limitPrevUsedHdr) == 0x108, "limitPrevUsedHdr @ 0x108");
static_assert(offsetof(TownHallUI, limitPrevProdHdr) == 0x110, "limitPrevProdHdr @ 0x110");
static_assert(offsetof(TownHallUI, invTable) == 0x118, "invTable @ 0x118 (dtor 0x14008118a)");
static_assert(offsetof(TownHallUI, invTableCount) == 0x120, "invTableCount @ 0x120");
static_assert(offsetof(TownHallUI, inventoryCombo) == 0x128, "inventoryCombo @ 0x128");
static_assert(offsetof(TownHallUI, sortCombo) == 0x130, "sortCombo @ 0x130");
static_assert(offsetof(TownHallUI, invBST) == 0x138, "invBST @ 0x138 (dtor 0x14008114f)");
static_assert(offsetof(TownHallUI, invBSTCount) == 0x140, "invBSTCount @ 0x140 (dtor 0x140081187)");
static_assert(offsetof(TownHallUI, cachedGameTick) == 0x148, "cachedGameTick @ 0x148");
static_assert(offsetof(TownHallUI, graphYearsTable) == 0x150, "graphYearsTable @ 0x150");
static_assert(offsetof(TownHallUI, graphTypesTable) == 0x158, "graphTypesTable @ 0x158");
static_assert(offsetof(TownHallUI, graphData) == 0x160, "graphData @ 0x160");
static_assert(offsetof(TownHallUI, timeCombo) == 0x168, "timeCombo @ 0x168");
static_assert(offsetof(TownHallUI, graphCombo) == 0x170, "graphCombo @ 0x170");
static_assert(offsetof(TownHallUI, textXMin) == 0x178, "textXMin @ 0x178");
static_assert(offsetof(TownHallUI, number0) == 0x180, "number0 @ 0x180");
static_assert(offsetof(TownHallUI, number4) == 0x1A0, "number4 @ 0x1A0");
static_assert(offsetof(TownHallUI, label0) == 0x1A8, "label0 @ 0x1A8");
static_assert(offsetof(TownHallUI, label3) == 0x1C0, "label3 @ 0x1C0");
static_assert(offsetof(TownHallUI, agriLabels) == 0x1C8, "agriLabels @ 0x1C8 (dtor 0x140081134)");
static_assert(offsetof(TownHallUI, agriLabelCount) == 0x1D0, "agriLabelCount @ 0x1D0");
static_assert(offsetof(TownHallUI, agriBST) == 0x1D8, "agriBST @ 0x1D8 (dtor 0x1400810f7)");
static_assert(offsetof(TownHallUI, agriBSTCount) == 0x1E0, "agriBSTCount @ 0x1E0 (dtor 0x140081131)");
static_assert(offsetof(TownHallUI, _pad1E4) == 0x1E4, "_pad1E4 @ 0x1E4");
