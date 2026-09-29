// code/Game/Dialog/townhallui.cpp
// Clean reimplementation of TownHallUI vtable methods.
//
// TownHallUI (0x1E8 bytes), vtable at 0x14015dce0.
// Extends DialogController (0x30 base). Multi-tab town overview panel.
// The field layout is TownHallUI.h's.
//
// All 15 TownHallUI methods are members (retail addresses in the header):
//   InitGeneral, InitGraph, InitProfessions, InitProduction,
//   InitInventory, InitAgriculture, UpdateGraph, UpdateGeneral,
//   UpdateProfessions, UpdateProduction, UpdateAgriculture,
//   UpdateInventory, UpdateAll, OnProfessionButton, OnProfessionSpinner
//
// READS LEFT AT AN OFFSET (B.4, owner named on each line):
//   GameUpdateLoop::m_citizenList (the town manager, CitizenBehavior: WORK-a):
//     its citizen list, homes, statistics history, production records, entity
//     trees and lists.
//   GameUpdateLoop::m_dayCounter (W0: void*): the year and the time of year.
//   ProfessionList::arrayData (GAME-b: void*, H5 raw) and Profession's String
//     keys (GAME-b: H5 raw).
//   StorageComponent::storageSlots (W0 raw storage).
//   ComponentDescription::_subDescs (COMP-c: void**).
//   UI::Element::_description (R-UI-a: void*).
//   The exe helpers' untyped parameters (Pf_AvlRebalanceB_13480, the
//   CitizenEmploy strategies, ToolbarUtility_BstInsert_4890: other blocks).

#include "ui/Panels.h"
#include "Dialog/TownHallUI.h"
#include "ui/UserInterface.h"
#include "runtime/types/ui/DialogInstance.h"
#include "object/TypeInfo.h"
#include "Game/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Game/ProfessionList.h"
#include "Game/Profession.h"
#include "Component/ValueDescription.h"   // ENDGAME B.3.9 TR: ValueDescription::TypeId()
#include "Component/CitizenComponent.h"
#include "system/String.h"
#include "system/Filename.h"
#include "Component/coll_bridge.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "io/Serializer.h"   // ENDGAME B.3.4 caller batch D
#include "io/Stream.h"
#include "system/Value.h"    // ENDGAME B.3.4b System callers: System::Time
#include <new>
#include "runtime/types/ui/Element.h"      // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/ElementDescription.h"  // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/String.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/Sprite.h"       // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K1)
#include "runtime/types/ui/TextElement.h"       // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/LabelElement.h"      // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/NumberElement.h"     // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/NumberEditElement.h" // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/ComboElement.h"      // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/GraphElement.h"      // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/SpinnerElement.h"    // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/TableElement.h"      // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/TabElement.h"        // ENDGAME B.3.6 UI callers round 2 (K1)
#include "runtime/types/ui/ButtonElement.h"      // X::TypeId() (B.3.9 CG2)
#include "Component/RawMaterialDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/CitizenDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/SelectDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/ComponentDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/StateMachineComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/TradeComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/StorageComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/WorkPlaceComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/WorkComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "Entity/ComponentEntity.h"
#include "Object/factory_registration.h"
#include <cstddef>
#include "Dialog/exe_bodies.h"
#include "Component/exe_bodies.h"   // B3R: CitizenEmploy_Strat_*
#include "system/MemoryAllocator.h"   // B3R: the allocator doors
#include "Action/exe_bodies.h"   // B3R: Pf_AvlRebalanceB_13480
#include "Work/exe_bodies.h"   // B3R: WorkBase_TreeNodeFree_14c30

// ============================================================
// Helpers
// ============================================================

// Find element by name in dialog (the tree's InitGeneral/InitGraph lookups
// keep it without a type check; retail 0x1400807f0 checks IsDerivedFrom).
static UI::Element* TownHall_FindElemRaw(UI::Element* dialogElem, const char* name) {
    System::String strBuf(name);
    return dialogElem->FindElement(strBuf).Get();
}

// Find element by name with IsDerivedFrom check
template <class T>
static T* TownHall_FindElem(UI::Element* dialogElem, const char* name) {
    System::String strBuf(name);
    UI::Element* elem = dialogElem->FindElement(strBuf).Get();

    T* castElem = nullptr;
    if (elem) {
        if (::Object::IsDerivedFromNullSafe(elem, T::TypeId())) castElem = static_cast<T*>(elem);
    }
    return castElem;
}

// Keep an element only when IsDerivedFrom T (retail's inlined narrowing).
template <class T>
static T* CastElement(UI::Element* elem) {
    if (!elem) return nullptr;
    if (::Object::IsDerivedFromNullSafe(elem, T::TypeId())) return static_cast<T*>(elem);
    return nullptr;
}

// ============================================================
// TownHallUI::InitGeneral (0x1400807f0)
// ============================================================
void TownHallUI::InitGeneral() {
    UI::Element* dialogElem = _container;

    numberYear      = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberYear"));
    textMonth       = static_cast<UI::TextElement*>(TownHall_FindElemRaw(dialogElem, "textMonth"));
    numberHomes     = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberHomes"));
    numberCitizens  = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberCitizens"));
    numberFamily    = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberFamily"));
    numberAdults    = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberAdults"));
    numberStudents  = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberStudents"));
    numberChildren  = static_cast<UI::NumberElement*>(TownHall_FindElemRaw(dialogElem, "numberChildren"));
    percentClothed  = static_cast<UI::TextElement*>(TownHall_FindElemRaw(dialogElem, "percentClothed"));
    percentEducated = static_cast<UI::TextElement*>(TownHall_FindElemRaw(dialogElem, "percentEducated"));

    UI::Element* heartsElem = TownHall_FindElemRaw(dialogElem, "groupHearts");
    TownHall_GroupRatingInit(&groupHearts, &heartsElem);   // 0x14006abf0

    UI::Element* starsElem = TownHall_FindElemRaw(dialogElem, "groupStars");
    TownHall_GroupRatingInit(&groupStars, &starsElem);   // 0x14006abf0
}

// ============================================================
// TownHallUI::InitGraph (0x14007f0a0)
// ============================================================
void TownHallUI::InitGraph() {
    UI::Element* dialogElem = _container;

    graphData  = static_cast<UI::GraphElement*>(TownHall_FindElemRaw(dialogElem, "graphData"));
    textXMin   = static_cast<UI::TextElement*>(TownHall_FindElemRaw(dialogElem, "textXMin"));
    timeCombo  = static_cast<UI::ComboElement*>(TownHall_FindElemRaw(dialogElem, "timeCombo"));
    graphCombo = static_cast<UI::ComboElement*>(TownHall_FindElemRaw(dialogElem, "graphCombo"));

    timeCombo->UI::Element::SetElementId(0x1964);

    graphCombo->UI::Element::SetElementId(0x1965);

    UI::NumberElement** const numberPtrs[5] = { &number0, &number1, &number2, &number3, &number4 };
    for (int i = 0; i < 5; i++) {
        System::String fmtBuf = System::String::Format("number%i", (uint64_t)i);
        *numberPtrs[i] = static_cast<UI::NumberElement*>(_container->FindElement(fmtBuf).Get());
    }

    UI::LabelElement** const labelPtrs[4] = { &label0, &label1, &label2, &label3 };
    for (int i = 0; i < 4; i++) {
        System::String fmtBuf = System::String::Format("label%i", (uint64_t)i);
        *labelPtrs[i] = static_cast<UI::LabelElement*>(_container->FindElement(fmtBuf).Get());
    }

    // Load string tables
    {
        System::Filename filenameBuf("Dialog/StringTable.rsc:graphYears");
        const ::Object::External* ext = ::Object::ExternalFactory::_singleton->FindOrLoad(filenameBuf);
        graphYearsTable = (ext && ::Object::IsDerivedFromNullSafe(ext, UI::StringTable::TypeId()))
            ? const_cast<UI::StringTable*>(static_cast<const UI::StringTable*>(ext)) : nullptr;
    }
    {
        System::Filename filenameBuf("Dialog/StringTable.rsc:graphTypes");
        const ::Object::External* ext = ::Object::ExternalFactory::_singleton->FindOrLoad(filenameBuf);
        graphTypesTable = (ext && ::Object::IsDerivedFromNullSafe(ext, UI::StringTable::TypeId()))
            ? const_cast<UI::StringTable*>(static_cast<const UI::StringTable*>(ext)) : nullptr;
    }

    // Populate time combo
    for (int i = 0; i < 7; i++) {
        UI::String uiStr(::Object::ExternalPtr<UI::StringTable>(graphYearsTable), i);
        System::String sysStrBuf;
        ::new (&sysStrBuf) System::String(uiStr.operator System::String());
        UI::Sprite emptySprite;
        timeCombo->AddItem(emptySprite, sysStrBuf, nullptr);
    }

    // Populate graph combo (skip empty entries)
    int typeCount = graphTypesTable->GetCount();
    int maxEntries = (13 < typeCount) ? typeCount : 13;
    for (int i = 0; i < maxEntries; i++) {
        System::String textBuf;
        ::new (&textBuf) System::String(graphTypesTable->GetText(i));
        bool isEmpty = textBuf.GetLength() == 0;

        if (!isEmpty) {
            UI::String uiStr(::Object::ExternalPtr<UI::StringTable>(graphTypesTable), i);
            System::String sysStrBuf;
            ::new (&sysStrBuf) System::String(uiStr.operator System::String());
            UI::Sprite emptySprite;
            graphCombo->AddItem(emptySprite, sysStrBuf, nullptr);
        }
    }

    timeCombo->SetCurrentItem(0);
    graphCombo->SetCurrentItem(0);

    UpdateGraph();
}

// ============================================================
// TownHallUI::InitProfessions (0x140080160)
// ============================================================
// Sets up the professions table: finds "tableProfessions", sizes it
// to profession count, clones rows from template, extracts child
// elements (label, numberEdit, number, spinner, button) per row,
// inserts profession type into BST, sets text/sprite/tooltip/IDs,
// and pushes profession entries into the dynamic array at +0x40.
void TownHallUI::InitProfessions() {
    // Find "tableProfessions" and cast to TableElement
    UI::TableElement* table = TownHall_FindElem<UI::TableElement>(_container, "tableProfessions");

    // Get profession list from game singleton
    ProfessionList* profList = GameUpdateLoop::_singleton->m_professionList;

    // Get template row (Child(0)) and its description (the clonable object at
    // field +0xB8 of the template element — NOT vtable+0xB8)
    UI::Element* templateChild = table->Child(0).Get();
    const UI::ElementDescription* cloneObj =
        reinterpret_cast<const UI::ElementDescription*>(templateChild->_description);   // LEFT: Element::_description is void* (R-UI-a)

    // Set table size to profession count x 1
    int profCount = profList->arrayCount;
    table->SetSize(profCount, 1);

    // Iterate rows
    for (int i = 0; i < profCount; i++) {
        // Clone template row into table slot: cloneObj is the template row's
        // Element::_description, so this is UI::ElementDescription slot 8,
        // CreateInstance (retslot, B6-2).
        ::Object::InternalPtr<UI::Element> cloneBuf;
        ::Object::InternalPtr<UI::Element>* clonedElem =
            cloneObj->CreateInstance(&cloneBuf, _dialog->_dialog);

        table->SetChild(clonedElem, i, 0);

        // Get the cloned row element
        UI::Element* rowElem = table->Child(i).Get();

        // Get row's child(1) — the spinner/content container
        UI::Element* contentElem = rowElem->Child(1).Get();

        // Profession row (0x40 bytes for ProfArrayPush), zeroed
        ProfessionRow profEntry = {};

        // Row child 0: LabelElement (profession icon/name)
        UI::LabelElement* label = CastElement<UI::LabelElement>(rowElem->Child(0).Get());
        profEntry.label = label;

        // Content child 0: NumberEditElement (desired worker count spinner)
        profEntry.desired = CastElement<UI::NumberEditElement>(contentElem->Child(0).Get());

        // Row child 2: NumberElement (current count display)
        profEntry.slotsNumber = CastElement<UI::NumberElement>(rowElem->Child(2).Get());

        // Row child 3: NumberElement (another counter)
        profEntry.buildingsNumber = CastElement<UI::NumberElement>(rowElem->Child(3).Get());

        // Row child 4: ButtonElement (go-to button)
        UI::ButtonElement* button = CastElement<UI::ButtonElement>(rowElem->Child(4).Get());
        profEntry.button = button;

        // Get profession type entry: profList->types[i]
        uint64_t profTypeId = reinterpret_cast<uint64_t*>(profList->arrayData)[i];   // LEFT: ProfessionList::arrayData is void* (GAME-b)
        profEntry.workers = i;  // profession index

        // Insert into profession BST at +0x50
        ProfessionNode* curNode = professionTree;

        if (curNode == nullptr) {
            // Allocate first BST node (0x30 bytes)
            ProfessionNode* newNode = static_cast<ProfessionNode*>(System::MemoryAllocator::MemAlloc(0x30, 8));
            if (newNode) {
                newNode->key = profTypeId;          // +0x00: key
                newNode->index = (uint64_t)i;       // +0x08: index
                newNode->parent = nullptr;          // +0x10: parent
                newNode->left = nullptr;            // +0x18: left
                newNode->right = nullptr;           // +0x20: right
                newNode->balance = 0;               // +0x28: balance
            }
            professionTree = newNode;
        } else {
            // Walk BST and insert
            ProfessionNode* parent = nullptr;
            bool inserted = false;
            while (!inserted) {
                parent = curNode;
                if (profTypeId < curNode->key) {
                    // Go left
                    if (curNode->left == nullptr) {
                        ProfessionNode* newNode = static_cast<ProfessionNode*>(System::MemoryAllocator::MemAlloc(0x30, 8));
                        if (newNode) {
                            newNode->key = profTypeId;
                            newNode->index = (uint64_t)i;
                            newNode->parent = parent;
                            newNode->left = nullptr;
                            newNode->right = nullptr;
                            newNode->balance = 0;
                        }
                        parent->left = newNode;
                        // Rebalance
                        Pf_AvlRebalanceB_13480(reinterpret_cast<int64_t*>(&professionTree), reinterpret_cast<int64_t*>(&curNode));   // 0x140013480; LEFT: its int64_t* signature (ACT)
                        inserted = true;
                    } else {
                        curNode = curNode->left;
                    }
                } else if (profTypeId > curNode->key) {
                    // Go right
                    if (curNode->right == nullptr) {
                        ProfessionNode* newNode = static_cast<ProfessionNode*>(System::MemoryAllocator::MemAlloc(0x30, 8));
                        if (newNode) {
                            newNode->key = profTypeId;
                            newNode->index = (uint64_t)i;
                            newNode->parent = parent;
                            newNode->left = nullptr;
                            newNode->right = nullptr;
                            newNode->balance = 0;
                        }
                        parent->right = newNode;
                        Pf_AvlRebalanceB_13480(reinterpret_cast<int64_t*>(&professionTree), reinterpret_cast<int64_t*>(&curNode));   // 0x140013480; LEFT: its int64_t* signature (ACT)
                        inserted = true;
                    } else {
                        curNode = curNode->right;
                    }
                } else {
                    // Equal — duplicate, skip (go to label setup)
                    inserted = true;
                }
            }
        }
        professionTreeCount = professionTreeCount + 1;

        // Set label text from the profession:
        //   +0x20 = StringTable pointer, +0x28 = SpriteSheet pointer,
        //   +0x30 = name key, +0x38 = tooltip key (System::Strings)
        Profession* prof = reinterpret_cast<Profession*>(profTypeId);
        UI::String uiStr(::Object::ExternalPtr<UI::StringTable>(prof->stringTable),
                         *reinterpret_cast<const System::String*>(&prof->stringName));   // LEFT: Profession::stringName is H5 raw (GAME-b)
        UI::String uiStrCopy = uiStr;
        label->SetText(&uiStrCopy);

        // Set sprite on label
        UI::Sprite sprBuf(::Object::ExternalPtr<UI::SpriteSheet>(prof->spriteSheet),
                          *reinterpret_cast<const System::String*>(&prof->stringName));   // LEFT: H5 raw (GAME-b)
        UI::Sprite sprCopy = sprBuf;
        label->SetSprite(&sprCopy);

        // Set tooltip
        UI::String ttStr(::Object::ExternalPtr<UI::StringTable>(prof->stringTable),
                         *reinterpret_cast<const System::String*>(&prof->stringDesc));   // LEFT: H5 raw (GAME-b)
        UI::String ttCopy = ttStr;
        label->SetToolTip(ttCopy);

        // Set spinner element IDs on the content container (child 1 of row)
        static_cast<UI::SpinnerElement*>(contentElem)->SetElementId(i + 0x1966);

        // Set inner child(0) of content container with same ID
        contentElem->Child(0).Get()->SetElementId(i + 0x1966);

        // Set button element ID
        button->UI::Element::SetElementId(i + 0x1999);

        // Push profession entry into dynamic array at +0x40
        TownHall_ProfArrayPush(&professionRows, professionRowCount + 1, &profEntry);   // 0x140015800

        // Free entity list if allocated (profEntry.entities was zeroed, so nothing to free);
        // the size and capacity are retail's one qword store (H7).
        System::MemoryAllocator::MemFree(profEntry.entities);
        profEntry.entities = nullptr;
        profEntry.entityCount = 0;
        profEntry.entityCapacity = 0;
    }
}

// ============================================================
// TownHallUI::InitProduction (0x14007fc30)
// ============================================================
// Sets up the production limits table. Finds "limits" TableElement,
// iterates its rows (linear children in groups of colCount),
// extracting 5 NumberElement columns per row into the production
// limits array at +0xF0. Then finds combo elements and populates
// the production year combo from the graphYears string table.
void TownHallUI::InitProduction() {
    // Find "limits" and cast to TableElement
    UI::TableElement* table = TownHall_FindElem<UI::TableElement>(_container, "limits");

    // Read column count from table
    int colCount = table->_columnCount;

    // Iterate rows of the table
    // Children are laid out linearly. Each "row" spans colCount children.
    // Column 0 = row label (skipped), columns 1-5 = NumberElements.
    // childBase starts at 2 (first data column after header row),
    // increments by colCount each row.
    int totalChildren = table->_rowCount;
    int childBase = 2;  // starts at child index 2 (skip first row header)

    UI::NumberElement* ProductionRow::* const columns[5] = {
        &ProductionRow::stock, &ProductionRow::used, &ProductionRow::produced,
        &ProductionRow::usedPrev, &ProductionRow::producedPrev };

    for (int row = 0; row < totalChildren; row++) {
        ProductionRow entry = {};  // 5 * 8 = 0x28 bytes per production entry

        // 5 NumberElement columns per row at indices:
        //   childBase-1, childBase, childBase+1, childBase+2, childBase+3
        int indices[5] = {childBase - 1, childBase, childBase + 1, childBase + 2, childBase + 3};
        for (int col = 0; col < 5; col++) {
            entry.*columns[col] = CastElement<UI::NumberElement>(table->Child(indices[col]).Get());
        }

        // Push entry into production limits array at +0xF0
        TownHall_ProdArrayPush(&prodLimits, prodLimitsCount + 1, &entry);   // 0x1400159b0

        childBase += colCount;  // advance to next row
    }

    // Find "productionCombo" ComboElement
    productionCombo = TownHall_FindElem<UI::ComboElement>(_container, "productionCombo");

    // Find "limitPrevUsedHeader" TextElement
    limitPrevUsedHdr = TownHall_FindElem<UI::TextElement>(_container, "limitPrevUsedHeader");

    // Find "limitPrevProducedHeader" TextElement
    limitPrevProdHdr = TownHall_FindElem<UI::TextElement>(_container, "limitPrevProducedHeader");

    // Load graphYears string table and populate combo
    UI::StringTable* yearsTable = nullptr;
    {
        System::Filename filenameBuf("Dialog/StringTable.rsc:graphYears");
        const ::Object::External* ext = ::Object::ExternalFactory::_singleton->FindOrLoad(filenameBuf);
        yearsTable = (ext && ::Object::IsDerivedFromNullSafe(ext, UI::StringTable::TypeId()))
            ? const_cast<UI::StringTable*>(static_cast<const UI::StringTable*>(ext)) : nullptr;
    }

    // Add 7 combo items from graphYears string table
    for (int i = 0; i < 7; i++) {
        UI::String uiStr(::Object::ExternalPtr<UI::StringTable>(yearsTable), i);
        System::String sysStrBuf;
        ::new (&sysStrBuf) System::String(uiStr.operator System::String());
        UI::Sprite emptySprite;
        productionCombo->AddItem(emptySprite, sysStrBuf, nullptr);
    }
    productionCombo->SetCurrentItem(0);
}

// ============================================================
// TownHallUI::InitInventory (0x14007f660)
// ============================================================
// Sets up the inventory table. Finds "inventoryTable" TableElement,
// counts resource types via ExternalFactory, sizes/clones table rows,
// extracts LabelElement + NumberElement pairs from each cell,
// pushes them into the inventory table array at +0x118.
// Then finds and populates inventoryCombo and sortCombo.
void TownHallUI::InitInventory() {
    // Find "inventoryTable" and cast to TableElement
    UI::TableElement* table = TownHall_FindElem<UI::TableElement>(_container, "inventoryTable");

    // Count resource types via ExternalFactory::FindNext
    int resourceCount = 0;
    uint32_t resTypeId = RawMaterialDescription::TypeId();
    const ::Object::ExternalFactory* efactory = ::Object::ExternalFactory::_singleton;
    const ::Object::External* extIter = efactory->FindNext(resTypeId, nullptr);
    while (extIter) {
        resourceCount++;
        extIter = efactory->FindNext(resTypeId, extIter);
    }

    // Get column count and template row description
    int colCount = table->_columnCount;

    UI::Element* templateChild = table->Child(0).Get();
    const UI::ElementDescription* cloneObj =
        reinterpret_cast<const UI::ElementDescription*>(templateChild->_description);   // LEFT: R-UI-a

    // Size table: ceil(resourceCount / colCount) rows
    int rowCount = ((colCount - 1 + resourceCount) / colCount * colCount) / colCount;
    table->SetSize(rowCount, colCount);

    // Clone template rows into all cells
    int totalChildren = table->ChildCount();
    for (int i = 0; i < totalChildren; i++) {
        // Clone: UI::ElementDescription slot 8, CreateInstance (retslot, B6-2).
        ::Object::InternalPtr<UI::Element> cloneBuf;
        ::Object::InternalPtr<UI::Element>* clonedElem =
            cloneObj->CreateInstance(&cloneBuf, _dialog->_dialog);

        table->SetChild(clonedElem, i);

    }

    // Extract label + number elements from each cell
    totalChildren = table->ChildCount();
    for (int i = 0; i < totalChildren; i++) {
        // Get cell->child(0) = LabelElement
        UI::Element* cell = table->Child(i).Get();
        UI::LabelElement* labelElem = CastElement<UI::LabelElement>(cell->Child(0).Get());

        // Get cell->child(1) = NumberElement
        UI::Element* cell2 = table->Child(i).Get();
        UI::NumberElement* numElem = CastElement<UI::NumberElement>(cell2->Child(1).Get());

        // Push {label, number} into the inventory table array at +0x118.
        // B.3.2 bridge: TownHallUI+0x118 is Collection::Array<InventoryRow>
        // (stride 0x10, count +0x120, capacity +0x124), raw storage (H5).
        auto& invRows = GameColl::AsArray<GameColl::Bytes<0x10, 8>>(&invTable);
        invRows.SetSize(invRows.GetSize() + 1, GameColl::Bytes<0x10, 8>(labelElem, numElem));
    }

    // Load inventoryType string table
    UI::StringTable* invTypeTable = nullptr;
    {
        System::Filename filenameBuf("Dialog/StringTable.rsc:inventoryType");
        const ::Object::External* ext = ::Object::ExternalFactory::_singleton->FindOrLoad(filenameBuf);
        invTypeTable = (ext && ::Object::IsDerivedFromNullSafe(ext, UI::StringTable::TypeId()))
            ? const_cast<UI::StringTable*>(static_cast<const UI::StringTable*>(ext)) : nullptr;
    }

    // Find "inventoryCombo" ComboElement
    inventoryCombo = TownHall_FindElem<UI::ComboElement>(_container, "inventoryCombo");

    // Populate inventory combo with 3 items
    for (int i = 0; i < 3; i++) {
        UI::String uiStr(::Object::ExternalPtr<UI::StringTable>(invTypeTable), i);
        System::String sysStrBuf;
        ::new (&sysStrBuf) System::String(uiStr.operator System::String());
        UI::Sprite emptySprite;
        inventoryCombo->AddItem(emptySprite, sysStrBuf, nullptr);
    }
    inventoryCombo->SetCurrentItem(0);

    // Find "sortCombo" ComboElement
    sortCombo = TownHall_FindElem<UI::ComboElement>(_container, "sortCombo");
    sortCombo->SetCurrentItem(0);
}

// ============================================================
// TownHallUI::InitAgriculture (0x14007ebf0)
// ============================================================
// Sets up the agriculture table. Iterates ExternalFactory for
// ComponentDescription entities, finds SelectDescription sub-components,
// inserts matching building descriptions into BST at +0x1D8.
// Then sizes/clones table rows and extracts LabelElement from
// each cell into the dynamic array at +0x1C8.
void TownHallUI::InitAgriculture() {
    // Find "agriTable" and cast to TableElement
    UI::TableElement* table = TownHall_FindElem<UI::TableElement>(_container, "agriTable");

    // Iterate ExternalFactory for ComponentDescription entities
    uint32_t buildTypeId = ComponentDescription::TypeId();
    uint32_t selectDescId = SelectDescription::TypeId();

    const ::Object::ExternalFactory* efactoryAgri = ::Object::ExternalFactory::_singleton;
    const ::Object::External* buildDesc = efactoryAgri->FindNext(buildTypeId, nullptr);
    while (buildDesc) {
        // Cast to ComponentDescription (IsDerivedFrom check)
        const ComponentDescription* buildComp = nullptr;
        if (::Object::IsDerivedFromNullSafe(buildDesc, buildTypeId)) {
            buildComp = static_cast<const ComponentDescription*>(buildDesc);
        }

        if (buildComp) {
            // Iterate sub-components to find SelectDescription
            int subCount = buildComp->_subDescCount;

            for (int j = 0; j < subCount; j++) {
                ::Object::Base* subComp = reinterpret_cast<::Object::Base*>(buildComp->_subDescs[j]);   // LEFT: _subDescs is void** (COMP-c)
                // Check if sub-component's type matches SelectDescription
                // Virtual call at vtable+0x10 returns type ID
                int subTypeId = subComp->Type();

                if (subTypeId == (int)selectDescId) {
                    // Found a SelectDescription sub-component
                    void* subObj = buildComp->_subDescs[j];
                    if (::Object::IsDerivedFromNullSafe(subObj, selectDescId) && subObj) {
                        // Insert buildComp into agriculture BST at +0x1D8
                        uint64_t key = reinterpret_cast<uint64_t>(buildComp);
                        AgricultureNode* curNode = agriBST;

                        if (curNode == nullptr) {
                            // First node (0x28 bytes)
                            AgricultureNode* newNode = static_cast<AgricultureNode*>(System::MemoryAllocator::MemAlloc(0x28, 8));
                            if (newNode) {
                                newNode->key = key;
                                newNode->parent = nullptr;
                                newNode->left = nullptr;
                                newNode->right = nullptr;
                                newNode->balance = 0;
                            }
                            agriBST = newNode;
                            agriBSTCount = agriBSTCount + 1;
                        } else {
                            // Walk BST and insert
                            AgricultureNode* parent = nullptr;
                            bool inserted = false;
                            while (!inserted) {
                                parent = curNode;
                                if (key < curNode->key) {
                                    if (curNode->left == nullptr) {
                                        AgricultureNode* newNode = static_cast<AgricultureNode*>(System::MemoryAllocator::MemAlloc(0x28, 8));
                                        if (newNode) {
                                            newNode->key = key;
                                            newNode->parent = parent;
                                            newNode->left = nullptr;
                                            newNode->right = nullptr;
                                            newNode->balance = 0;
                                        }
                                        parent->left = newNode;
                                        TownHall_AvlRebalance28(&agriBST, &curNode);   // 0x1400149a0
                                        inserted = true;
                                    } else {
                                        curNode = curNode->left;
                                    }
                                } else if (key > curNode->key) {
                                    if (curNode->right == nullptr) {
                                        AgricultureNode* newNode = static_cast<AgricultureNode*>(System::MemoryAllocator::MemAlloc(0x28, 8));
                                        if (newNode) {
                                            newNode->key = key;
                                            newNode->parent = parent;
                                            newNode->left = nullptr;
                                            newNode->right = nullptr;
                                            newNode->balance = 0;
                                        }
                                        parent->right = newNode;
                                        TownHall_AvlRebalance28(&agriBST, &curNode);   // 0x1400149a0
                                        inserted = true;
                                    } else {
                                        curNode = curNode->right;
                                    }
                                } else {
                                    // Duplicate — already in BST
                                    inserted = true;
                                }
                            }
                            agriBSTCount = agriBSTCount + 1;
                        }
                    }
                    break;  // Found matching sub-component, stop inner loop
                }
            }
        }

        buildDesc = efactoryAgri->FindNext(buildTypeId, buildDesc);
    }

    // Now set up the table with agriculture BST count
    int agriCount = agriBSTCount;
    int colCount = table->_columnCount;

    UI::Element* templateChild = table->Child(0).Get();
    const UI::ElementDescription* cloneObj =
        reinterpret_cast<const UI::ElementDescription*>(templateChild->_description);   // LEFT: R-UI-a

    // Size table: ceil(agriCount / colCount) rows
    int rowCount = ((colCount - 1 + agriCount) / colCount * colCount) / colCount;
    table->SetSize(rowCount, colCount);

    // Clone template rows into all cells
    int totalChildren = table->ChildCount();
    for (int i = 0; i < totalChildren; i++) {
        // Clone: UI::ElementDescription slot 8, CreateInstance (retslot, B6-2).
        ::Object::InternalPtr<UI::Element> cloneBuf;
        ::Object::InternalPtr<UI::Element>* clonedElem =
            cloneObj->CreateInstance(&cloneBuf, _dialog->_dialog);

        table->SetChild(clonedElem, i);

    }

    // Extract LabelElement from each cell into array at +0x1C8
    totalChildren = table->ChildCount();
    for (int i = 0; i < totalChildren; i++) {
        // Cast child to LabelElement
        UI::LabelElement* labelElem = CastElement<UI::LabelElement>(table->Child(i).Get());

        // Push the label into the label array at +0x1C8.
        // B.3.2 bridge: TownHallUI+0x1C8 is Collection::Array<LabelElement*>
        // (stride 8, count +0x1D0, capacity +0x1D4), raw storage (H5).
        auto& labelRows = GameColl::AsArray<GameColl::Bytes<8, 8>>(&agriLabels);
        labelRows.SetSize(labelRows.GetSize() + 1, GameColl::Bytes<8, 8>(labelElem));
    }
}

// ============================================================
// TownHallUI::UpdateGraph (0x14007e5b0)
// ============================================================
// Reads time/graph combo selections, populates the graph element
// with historical data series, and updates y-axis labels.
void TownHallUI::UpdateGraph() {
    // Cache game tick
    cachedGameTick = *(int*)(*(uintptr_t*)((uintptr_t)GameUpdateLoop::_singleton->m_citizenList + 0x150) + 8);   // LEFT: the town manager (WORK-a)

    // Read combo selections
    int timeIdx = timeCombo->_currentItem;
    int graphIdx = graphCombo->_currentItem;

    // Data series indices (-1 = hidden)
    int graphSeries[5] = {-1, -1, -1, -1, 0};
    uint32_t colors[4] = {0xffffff00, 0xff00ff00, 0xff8080ff, 0xffff0000};

    int stepSize = 0, pointCount = 0;
    uint32_t ticksPerStep = 0;

    switch (timeIdx) {
    case 0: stepSize=1;  pointCount=0x30; ticksPerStep=0xc;  break;
    case 1: stepSize=1;  pointCount=0x60; ticksPerStep=0x18; break;
    case 2: stepSize=2;  pointCount=0x78; ticksPerStep=5;    break;
    case 3: stepSize=4;  pointCount=0x78; ticksPerStep=10;   break;
    case 4: stepSize=8;  pointCount=0x96; ticksPerStep=0x19; break;
    case 5: stepSize=0x10; pointCount=0x96; ticksPerStep=0x32; break;
    case 6: stepSize=0x20; pointCount=0x96; ticksPerStep=100; break;
    }

    switch (graphIdx) {
    case 0:  graphSeries[0]=0;    graphSeries[1]=1;  graphSeries[2]=2;  graphSeries[3]=3;  break;
    case 1:  graphSeries[0]=4;    graphSeries[1]=6;  graphSeries[2]=5;  graphSeries[3]=7;  break;
    case 2:  graphSeries[0]=0x17; graphSeries[1]=-1; break;
    case 3:  graphSeries[0]=8;    graphSeries[1]=-1; break;
    case 4:  graphSeries[0]=0xb;  graphSeries[1]=-1; break;
    case 5:  graphSeries[0]=0xe;  graphSeries[1]=-1; break;
    case 6:  graphSeries[0]=0x11; graphSeries[1]=-1; break;
    case 7:  graphSeries[0]=0x23; graphSeries[1]=-1; break;
    case 8:  graphSeries[0]=0x14; graphSeries[1]=-1; break;
    case 9:  graphSeries[0]=0x1a; graphSeries[1]=-1; break;
    case 10: graphSeries[0]=0x1d; graphSeries[1]=-1; break;
    case 11: graphSeries[0]=0x20; graphSeries[1]=-1; break;
    case 12: graphSeries[0]=0x26; graphSeries[1]=-1; break;
    case 13: graphSeries[0]=0x29; graphSeries[1]=-1; break;
    case 14: graphSeries[0]=0x2c; graphSeries[1]=-1; break;
    case 15: graphSeries[0]=0x2f; graphSeries[1]=-1; break;
    case 16: graphSeries[0]=0x32; graphSeries[1]=-1; break;
    case 17: graphSeries[0]=0x35; graphSeries[1]=-1; break;
    case 18: graphSeries[0]=0x38; graphSeries[1]=-1; break;
    case 19: graphSeries[0]=0x3b; graphSeries[1]=-1; break;
    case 20: graphSeries[0]=0x3e; graphSeries[1]=-1; break;
    case 21: graphSeries[0]=0x41; graphSeries[1]=-1; break;
    case 22: graphSeries[0]=0x44; graphSeries[1]=-1; break;
    }

    pointCount += 1;
    graphSeries[4] = pointCount;

    // Set x-axis step
    graphData->_xDivisions = (int32_t)ticksPerStep;

    // Set x-axis min label (UIString from graphYearsTable at timeIdx)
    UI::String xMinStr(::Object::ExternalPtr<UI::StringTable>(graphYearsTable), timeIdx);
    UI::String xMinCopy = xMinStr;
    textXMin->SetText(&xMinCopy);

    // Remove existing graphs
    graphData->RemoveGraphs();

    uintptr_t gameData = (uintptr_t)GameUpdateLoop::_singleton->m_citizenList;   // LEFT: the town manager (WORK-a)
    int currentTick = *(int*)(*(uintptr_t*)(gameData + 0x150) + 8);
    int maxVal = 0;
    int graphCount = 0;

    UI::LabelElement* const* const labelSlots[4] = { &label0, &label1, &label2, &label3 };
    for (int g = 0; g < 4; g++) {
        int seriesIdx = graphSeries[g];
        UI::LabelElement* const* labelPtr = labelSlots[g];

        if (seriesIdx < 0) {
            (*labelPtr)->SetFlagsOnOff(1, 0);
        } else {
            graphData->AddGraph(colors[g]);

            int startIdx;
            if (currentTick / stepSize < pointCount) {
                int padCount = pointCount - currentTick / stepSize;
                for (int p = 0; p < padCount; p++) {
                    graphData->AddGraphPoint(graphCount, 0);
                }
                startIdx = 0;
            } else {
                startIdx = currentTick - pointCount * stepSize;
            }

            for (int64_t idx = startIdx; idx < currentTick; idx += stepSize) {
                uintptr_t histBase = *(uintptr_t*)(*(uintptr_t*)(gameData + 0x150) + (uintptr_t)seriesIdx * 0x10);   // LEFT: WORK-a
                int val = *(int*)(histBase + idx * 4);
                if (val > maxVal) maxVal = val;
                graphData->AddGraphPoint(graphCount, val);
            }

            // Set label color
            (*labelPtr)->_iconColor = colors[g];

            // Set label text from string table
            UI::String lblStr((*labelPtr)->_text.GetStringTable(), seriesIdx);
            UI::String lblCopy = lblStr;
            (*labelPtr)->SetText(&lblCopy);

            (*labelPtr)->SetFlagsOnOff(0, 1);

            pointCount = graphSeries[4];
            graphCount++;
        }
    }

    // Round up y-axis max
    if (maxVal < 5) maxVal = 5;
    int scale;
    if      (maxVal < 10)   scale = 1;
    else if (maxVal < 100)  scale = 10;
    else if (maxVal < 1000) scale = 100;
    else                    scale = 1000;
    int yMax = ((maxVal - 1 + scale) / scale) * scale;

    graphData->_yRange = (float)yMax;

    // Y-axis labels: max, 3/4, 1/2, 1/4
    float quarter = (float)yMax * 0.25f;
    number0->SetValue(static_cast<int32_t>(yMax));
    number1->SetValue(static_cast<int32_t>((int)(quarter * 3.0f)));
    number2->SetValue(static_cast<int32_t>((int)(quarter * 2.0f)));
    number3->SetValue(static_cast<int32_t>((int)quarter));
}

// ============================================================
// TownHallUI::UpdateGeneral (0x14007d030)
// ============================================================
void TownHallUI::UpdateGeneral() {
    uintptr_t gameData = (uintptr_t)GameUpdateLoop::_singleton->m_citizenList;   // LEFT: the town manager (WORK-a)

    int totalCitizens = *(int*)(gameData + 0xa8);
    int adults = 0, students = 0, children = 0, familyCount = 0;
    float clothedSum = 0.0f, educatedSum = 0.0f, healthSum = 0.0f, happinessSum = 0.0f;
    float perCitizen = (totalCitizens > 0) ? (1.0f / (float)totalCitizens) : 0.0f;

    for (int i = 0; i < totalCitizens; i++) {
        CitizenComponent* citizen = (*(CitizenComponent***)(gameData + 0xa0))[i];   // LEFT: WORK-a

        float age = citizen->age;
        float adultAge = citizen->description->adultAge;
        AttendanceComponent* school = citizen->school;

        if (age < adultAge || school != nullptr) {
            if (school == nullptr) { children++; }
            else                   { students++; }
        } else {
            adults++;
            if (citizen->description->maxEducation <= citizen->education) {
                clothedSum += 1.0f;
            }
            CitizenComponent* spouse = citizen->spouse;
            if (spouse == nullptr) {
                if (citizen->spouse == nullptr) {
                    familyCount++;
                }
            } else {
                if (citizen->gender == 0) {
                    familyCount++;
                }
            }
        }

        happinessSum += citizen->happiness;
        healthSum += citizen->health;
        if (citizen->clothingLevel != 0) {
            educatedSum += 1.0f;
        }
    }

    float happinessAvg = happinessSum * perCitizen;
    float educatedPct = educatedSum * perCitizen * 100.0f;
    float clothedPct = (adults > 0) ? ((clothedSum / (float)adults) * 100.0f) : 0.0f;

    // Year
    int year = *(int*)GameUpdateLoop::_singleton->m_dayCounter;   // LEFT: DayCounter (W0 void*)
    numberYear->SetValue(static_cast<int32_t>(year));

    // Month
    float seconds = ((const System::Time*)((uintptr_t)GameUpdateLoop::_singleton->m_dayCounter + 8))->AsSeconds();   // LEFT: W0 void*
    int monthIdx = (int)((seconds / 3600.0f) * 12.0f);
    if (monthIdx > 11) monthIdx = 0;

    UI::String monthStr(textMonth->_text.GetStringTable(), monthIdx);
    UI::String monthCopy = monthStr;
    textMonth->SetText(&monthCopy);

    // Home count
    int homeCount = *(int*)(gameData + 0x28);   // LEFT: WORK-a
    numberHomes->SetValue(static_cast<int32_t>(homeCount));

    // Citizen counts
    numberCitizens->SetValue(static_cast<int32_t>(totalCitizens));
    numberAdults->SetValue(static_cast<int32_t>(adults));
    numberStudents->SetValue(static_cast<int32_t>(students));
    numberChildren->SetValue(static_cast<int32_t>(children));

    // Educated % (at +0xa0: retail writes it to "percentClothed")
    {
        System::String fmtBuf = System::String::Format("%i", (uint64_t)(uint32_t)(int)(educatedPct + 0.5f));
        percentClothed->SetParameter(0, fmtBuf);
    }

    // Clothed % (at +0xa8: retail writes it to "percentEducated")
    {
        System::String fmtBuf = System::String::Format("%i", (uint64_t)(uint32_t)(int)(clothedPct + 0.5f));
        percentEducated->SetParameter(0, fmtBuf);
    }

    // Family count
    numberFamily->SetValue(static_cast<int32_t>(familyCount));

    // Hearts (health) and Stars (happiness) ratings
    if (totalCitizens < 1) {
        TownHall_GroupRatingUpdate(&groupHearts, 0.0f, 1.0f);   // 0x14006aed0
        TownHall_GroupRatingUpdate(&groupStars, 0.0f, 1.0f);   // 0x14006aed0
    } else {
        CitizenComponent* firstCitizen = **(CitizenComponent***)(gameData + 0xa0);   // LEFT: WORK-a
        void* desc = firstCitizen->_description;
        CitizenDescription* citizenDesc = ::Object::IsDerivedFromNullSafe(desc, CitizenDescription::TypeId())
            ? reinterpret_cast<CitizenDescription*>(desc) : nullptr;   // LEFT: Component::_description is void*

        TownHall_GroupRatingUpdate(&groupHearts, healthSum * perCitizen,
                              citizenDesc->maxHealth);

        // Re-read first citizen for the happiness maximum
        firstCitizen = **(CitizenComponent***)(gameData + 0xa0);   // LEFT: WORK-a
        desc = firstCitizen->_description;
        citizenDesc = ::Object::IsDerivedFromNullSafe(desc, CitizenDescription::TypeId())
            ? reinterpret_cast<CitizenDescription*>(desc) : nullptr;   // LEFT: void*

        TownHall_GroupRatingUpdate(&groupStars, happinessAvg,
                              citizenDesc->maxHappiness);
    }
}

// ============================================================
// TownHallUI::UpdateProfessions (0x14007ccb0) — the production tab
// ============================================================
void TownHallUI::UpdateProfessions() {
    int comboIdx = productionCombo->_currentItem;

    uint32_t yearSpan = 0;
    switch (comboIdx) {
    case 0: yearSpan = 1; break;
    case 1: yearSpan = 2; break;
    case 2: yearSpan = 5; break;
    case 3: yearSpan = 10; break;
    case 4: yearSpan = 25; break;
    case 5: yearSpan = 50; break;
    case 6: yearSpan = 100; break;
    }

    // Set header text parameters
    {
        System::String fmtBuf = System::String::Format("%i", (uint64_t)yearSpan);
        limitPrevUsedHdr->SetParameter(0, fmtBuf);
    }
    {
        System::String fmtBuf = System::String::Format("%i", (uint64_t)yearSpan);
        limitPrevProdHdr->SetParameter(0, fmtBuf);
    }

    int* timePtr = (int*)GameUpdateLoop::_singleton->m_dayCounter;   // LEFT: DayCounter (W0 void*)
    int currentYearTick = timePtr[0] * 0x30 - 0x30;
    float secs = ((const System::Time*)(timePtr + 2))->AsSeconds();   // LEFT: W0 void*
    uint32_t monthTick = (uint32_t)((secs / 3600.0f) * 48.0f);
    if ((int)monthTick > 0x2f) monthTick = 0;
    int endTick = monthTick + currentYearTick;
    int prevEnd = timePtr[0] * 0x30 - 0x31;
    int prevStart = (timePtr[0] - (int)yearSpan - 1) * 0x30;

    void* townMgr = GameUpdateLoop::_singleton->m_citizenList;
    uintptr_t gameData = (uintptr_t)townMgr;   // LEFT: the town manager (WORK-a)
    uint32_t resIdx = 10;
    int prodCount = prodLimitsCount;

    if (prodCount > 0) {
        int64_t dataOff = 0;
        for (int i = 0; i < prodCount; i++) {
            int stock = *(int*)(*(uintptr_t*)(gameData + 0x170) + dataOff);   // LEFT: WORK-a

            prodLimits[i].stock->SetValue(static_cast<int32_t>(stock));

            uintptr_t prodData = *(uintptr_t*)(gameData + 0x170);   // LEFT: WORK-a
            int usedCur = TownHall_QueryProductionStat(townMgr, resIdx - 1, currentYearTick, endTick);   // 0x140090f20
            int usedStock = *(int*)(prodData + 0xc + dataOff);
            int prodCur = TownHall_QueryProductionStat(townMgr, resIdx, currentYearTick, endTick);   // 0x140090f20
            int prodStock = *(int*)(prodData + 0x10 + dataOff);
            int usedPrev = TownHall_QueryProductionStat(townMgr, resIdx - 1, prevStart, prevEnd);   // 0x140090f20
            int prodPrev = TownHall_QueryProductionStat(townMgr, resIdx, prevStart, prevEnd);   // 0x140090f20

            prodLimits[i].used->SetValue(static_cast<int32_t>(usedCur + usedStock));

            prodLimits[i].produced->SetValue(static_cast<int32_t>(prodCur + prodStock));

            prodLimits[i].usedPrev->SetValue(static_cast<int32_t>(usedPrev));

            prodLimits[i].producedPrev->SetValue(static_cast<int32_t>(prodPrev));

            resIdx += 3;
            dataOff += 0x20;
        }
    }
}

// ============================================================
// BST in-order traversal helpers for TownHallUI
// ============================================================

// In-order successor for the inventory tree
static TownHallUI::InventoryNode* InvBST_Next(TownHallUI::InventoryNode* node) {
    if (!node) return nullptr;
    TownHallUI::InventoryNode* right = node->right;
    if (right) {
        while (right->left) right = right->left;
        return right;
    }
    // Go up: find first ancestor where we came from the left
    TownHallUI::InventoryNode* parent = node->parent;
    while (parent) {
        if (parent->left == node) return parent; // came from left child
        if (parent->right == node) {
            node = parent;
            parent = parent->parent;
        } else {
            break;
        }
    }
    return nullptr;
}

// In-order successor for the agriculture tree
static TownHallUI::AgricultureNode* AgriBST_Next(TownHallUI::AgricultureNode* node) {
    if (!node) return nullptr;
    TownHallUI::AgricultureNode* right = node->right;
    if (right) {
        while (right->left) right = right->left;
        return right;
    }
    TownHallUI::AgricultureNode* parent = node->parent;
    while (parent) {
        if (parent->left == node) return parent;
        if (parent->right == node) {
            node = parent;
            parent = parent->parent;
        } else {
            break;
        }
    }
    return nullptr;
}

// Find a description's sub-description by type ID (the description's +0x28/+0x30
// array). The count and pointer-range guards are the tree's (STANDALONE; retail
// has none).
static ::Object::Base* TownHall_FindComponentByTypeId(uint64_t entity, uint32_t typeId) {
    if (!entity) {
        return nullptr;
    }
    const ComponentDescription* desc = reinterpret_cast<const ComponentDescription*>(entity);   // LEFT: the key is the pointer's bits
    void** compArray = desc->_subDescs;
    int compCount = desc->_subDescCount;

    if (!compArray) {
        return nullptr;
    }
    if (compCount < 0 || compCount > 200) {
        return nullptr;
    }

    for (int i = 0; i < compCount; i++) {
        void* comp = compArray[i];
        if ((uintptr_t)comp < 0x10000 || (uintptr_t)comp >= 0xFFFFFFFF00000000ULL) {   // LEFT: STANDALONE pointer-range guard
            continue;
        }
        // vtable slot 2 (+0x10) = GetTypeId()
        void** vtable = *(void***)comp;   // LEFT: STANDALONE vtable-null guard
        if (!vtable) {
            continue;
        }
        ::Object::Base* base = reinterpret_cast<::Object::Base*>(comp);   // LEFT: _subDescs is void** (COMP-c)
        int id = base->Type();
        if ((uint32_t)id == typeId) {
            bool derived = ::Object::IsDerivedFromNullSafe(comp, typeId);
            if (derived)
                return base;
            break;
        }
    }
    return nullptr;
}

// Look up a key in the inventory tree (+0x138). Returns node or nullptr.
static TownHallUI::InventoryNode* InvBST_Find(TownHallUI::InventoryNode* root, uint64_t key) {
    TownHallUI::InventoryNode* node = root;
    while (node) {
        uint64_t nodeKey = node->key;
        if (key < nodeKey)
            node = node->left;
        else if (key > nodeKey)
            node = node->right;
        else
            return node;
    }
    return nullptr;
}

// ============================================================
// TownHallUI::UpdateProduction (0x14007c7f0) — the inventory tab
// ============================================================
// Traverses invBST (+0x138) in-order, collects entries with
// ResourceType components, sorts them, then updates the
// inventory table UI elements (+0x118 array).
void TownHallUI::UpdateProduction() {
    // Read combo selections
    int invComboIdx = inventoryCombo->_currentItem;
    int sortComboIdx = sortCombo->_currentItem;

    // Collect entries from invBST in-order traversal
    struct ProdEntry {
        uint64_t entityPtr;
        void*    resourceTypeComp;
        int      value;
        int      _pad;
    };

    // B.3.2 bridge: this is a local Collection::Array<ProdEntry> — retail's
    // hand-inlined SetSize over a stack-local {data,size,capacity} triple, not a
    // struct field — so it becomes a real instance of the template. ProdEntry is
    // 0x18 bytes at align 8, exactly the stride/alignment the MemAlloc call it
    // replaces passed, and every appended element is fully written (including
    // `_pad = 0`), so the spec's init arm is never reached.
    Collection::Array<ProdEntry> entryArr;

    InventoryNode* bstRoot = invBST;
    if (bstRoot) {
        // Start at leftmost node
        InventoryNode* node = bstRoot;
        while (node->left) node = node->left;
        while (node) {
            uint64_t entityPtr = node->key;

            // Find ResourceType component on entity
            ::Object::Base* resourceComp = TownHall_FindComponentByTypeId(entityPtr, RawMaterialDescription::TypeId());

            // Read counter value at selected index
            int counterVal = node->counts[invComboIdx];

            // Push entry into dynamic array
            entryArr.SetSize(entryArr.GetSize() + 1,
                             ProdEntry{entityPtr, resourceComp, counterVal, 0});

            node = InvBST_Next(node);
        }
    }

    // Sort entries. The sort is retail's own in-place body over the array's
    // buffer (COLLECTION_INVENTORY §4: introsort stays a free function, it does
    // not become an Array<T> member), so it is handed GetData()/GetSize().
    ProdEntry* entries    = entryArr.GetData();
    const int  entryCount = entryArr.GetSize();
    if (sortComboIdx == 0) {
        if (entryCount > 1)
            TownHall_InventorySortAsc(entries, entryCount, &sortComboIdx, nullptr);   // 0x140015200
    } else {
        if (entryCount > 1)
            TownHall_InventorySortDesc(entries, entryCount, &sortComboIdx, nullptr);   // 0x140015500
    }

    // Update UI elements for each entry
    int displayIdx = 0;
    if (entryCount > 0) {
        for (int i = 0; i < entryCount; i++) {
            uint64_t entityPtr = entries[i].entityPtr;

            // Find ValueDescription component on entity
            ValueDescription* valDesc = static_cast<ValueDescription*>(
                TownHall_FindComponentByTypeId(entityPtr, ValueDescription::TypeId()));
            if (!valDesc) {
                continue;
            }

            // Create sprite from ValueDescription+0x20 (spriteSheet) and +0x28 (spriteName)
            UI::Sprite spriteBuf(::Object::ExternalPtr<UI::SpriteSheet>(valDesc->_sprite), valDesc->_name);

            // Set sprite on label element
            invTable[i].label->SetSprite(&spriteBuf);

            // Create UIString from ValueDescription+0x30 (stringTable) and +0x38 (stringName)
            UI::String uiStrBuf(::Object::ExternalPtr<UI::StringTable>(valDesc->_ref30), valDesc->_s38);

            // Set text on label element
            invTable[i].label->SetText(&uiStrBuf);

            // Set number value on the number element (+8 from label)
            invTable[i].count->SetValue(static_cast<int32_t>(entries[i].value));

            // Set active flag (vtable +0x50 with -1)
            invTable[i].count->SetColor(static_cast<uint32_t>(-1));
        }
        displayIdx = entryCount;
    }

    // Clear remaining entries
    if (displayIdx < invTableCount) {
        for (int i = displayIdx; i < invTableCount; i++) {
            // Empty sprite
            UI::Sprite emptySprite;
            invTable[i].label->SetSprite(&emptySprite);

            // Empty string: String(ExternalPtr<StringTable>(null), name) stores {null, -1}
            // without reading the name (sprite.cpp, retail 0x18011da60) == the default ctor.
            UI::String emptyStr;
            invTable[i].label->SetText(&emptyStr);

            // Set value to 0
            invTable[i].count->SetValue(static_cast<int32_t>(0));

            // Set inactive flag (vtable +0x50 with 0)
            invTable[i].count->SetColor(0u);
        }
    }

    // entryArr frees its own buffer (rtcoll::MemFree, the same 0x1800c7ad0 that
    // fn_MemFreeStatic resolves) when it goes out of scope here.
}

// ============================================================
// TownHallUI::UpdateAgriculture (0x14007c220)
// ============================================================
// Builds temp BST from entity list, traverses agriBST (+0x1D8)
// in-order, updates agriculture label elements with sprites
// and text from SelectDescription components.
void TownHallUI::UpdateAgriculture() {
    uintptr_t gameData = (uintptr_t)GameUpdateLoop::_singleton->m_citizenList;   // LEFT: the town manager (WORK-a)

    // Build temporary BST from entity list at gameData+0x120/+0x128
    AgricultureNode* tempBSTRoot = nullptr;
    int tempBSTCount = 0;
    int entityCount = *(int*)(gameData + 0x128);   // LEFT: WORK-a

    for (int i = 0; i < entityCount; i++) {
        uint64_t entityPtr = *(uint64_t*)(*(uintptr_t*)(gameData + 0x120) + (uintptr_t)i * 8);   // LEFT: WORK-a

        if (!tempBSTRoot) {
            // First node — allocate root
            AgricultureNode* node = static_cast<AgricultureNode*>(System::MemoryAllocator::MemAlloc(0x28, 8));
            if (node) {
                node->key = entityPtr;
                node->parent = nullptr;
                node->left = nullptr;
                node->right = nullptr;
                node->balance = 0;
                tempBSTRoot = node;
            }
        } else {
            // Insert into existing BST
            AgricultureNode* current = tempBSTRoot;
            AgricultureNode* parent = nullptr;
            while (current) {
                uint64_t curKey = current->key;
                if (entityPtr == curKey) goto next_entity; // already exists
                parent = current;
                if (entityPtr < curKey)
                    current = current->left;
                else
                    current = current->right;
            }
            // Allocate new node
            AgricultureNode* node = static_cast<AgricultureNode*>(System::MemoryAllocator::MemAlloc(0x28, 8));
            if (node) {
                node->key = entityPtr;
                node->parent = parent;
                node->left = nullptr;
                node->right = nullptr;
                node->balance = 0;
                if (entityPtr < parent->key)
                    parent->left = node;
                else
                    parent->right = node;
                // Rebalance
                AgricultureNode* newNodeAddr = node;
                TownHall_AvlRebalance28(&tempBSTRoot, &newNodeAddr);   // 0x1400149a0
            }
        }
        tempBSTCount++;
next_entity: ;
    }

    // Traverse agriBST (+0x1D8) in-order
    AgricultureNode* agriBSTRoot = agriBST;
    int displayIdx = 0;

    if (agriBSTRoot) {
        // Go to leftmost node
        AgricultureNode* node = agriBSTRoot;
        while (node->left) node = node->left;

        while (node) {
            uint64_t entityPtr = node->key;

            // Look up entity in temp BST
            AgricultureNode* found = nullptr;
            AgricultureNode* searchNode = tempBSTRoot;
            while (searchNode) {
                uint64_t sk = searchNode->key;
                if (entityPtr < sk)
                    searchNode = searchNode->left;
                else if (entityPtr > sk)
                    searchNode = searchNode->right;
                else {
                    found = searchNode;
                    break;
                }
            }

            if (found) {
                // Find SelectDescription component (DAT_1401c48a8) on entity
                SelectDescription* selectDesc = static_cast<SelectDescription*>(
                    TownHall_FindComponentByTypeId(entityPtr, SelectDescription::TypeId()));

                // Original always sets sprite/text when found in BST (even if selectDesc is NULL)
                // Create sprite from SelectDescription+0x20/+0x28
                UI::Sprite spriteBuf(::Object::ExternalPtr<UI::SpriteSheet>(selectDesc->_ref20), selectDesc->_s28);

                agriLabels[displayIdx]->SetSprite(&spriteBuf);

                // Create UIString from SelectDescription+0x30/+0x38
                UI::String uiStrBuf(::Object::ExternalPtr<UI::StringTable>(selectDesc->_ref30), selectDesc->_s38);

                agriLabels[displayIdx]->SetText(&uiStrBuf);

                // Vtable +0x50 call (SetColor) — 0xFFC0C0C0 = opaque silver/gray
                agriLabels[displayIdx]->SetColor(0xFFC0C0C0u);
            } else {
            }

            // Show element (SetFlagsOnOff hide=0, show=1)
            agriLabels[displayIdx]->SetFlagsOnOff(0, 1);

            displayIdx++;

            // Move to next node in-order
            node = AgriBST_Next(node);
        }
    }

    // Hide remaining label elements
    if (displayIdx < agriLabelCount) {
        for (int i = displayIdx; i < agriLabelCount; i++) {
            agriLabels[i]->SetFlagsOnOff(1, 0);
        }
    }

    // Free temporary BST
    if (tempBSTRoot) {
        WorkBase_TreeNodeFree_14c30(&tempBSTRoot, tempBSTRoot->left);   // 0x140014c30
        WorkBase_TreeNodeFree_14c30(&tempBSTRoot, tempBSTRoot->right);   // 0x140014c30
        System::MemoryAllocator::MemFree(tempBSTRoot);
    }
}

// ============================================================
// TownHallUI::UpdateInventory (0x14007def0) — the professions tab
// ============================================================
// For each profession, zeros counters, resizes entity arrays,
// iterates entities to count workers per profession, finds
// WorkPlaceComponent / WorkComponent to calculate
// max worker slots, updates NumberEditElement values.
void TownHallUI::UpdateInventory() {
    uintptr_t gameData = (uintptr_t)GameUpdateLoop::_singleton->m_citizenList;   // LEFT: the town manager (WORK-a)
    ProfessionList* profList = GameUpdateLoop::_singleton->m_professionList;
    int profCount = profList->arrayCount;

    // Phase 1: Zero counters and resize entity arrays for each profession
    for (int p = 0; p < profCount; p++) {
        ProfessionRow& profEntry = professionRows[p];
        profEntry.workers = 0;  // worker count = 0
        profEntry.slots = 0;    // secondary count = 0
        // Resize entity ptr array at +0x30 to 0 elements
        Reimpl_Array_Resize_Byte(&profEntry.entities, 0);   // 0x14000a590
    }

    // Phase 2: For each profession, look up the town's profession tree
    // (gameData+0x100) to find the buildings of that profession, count worker slots
    for (int p = 0; p < profCount; p++) {
        uint64_t profTypeId = reinterpret_cast<uint64_t*>(profList->arrayData)[p];   // LEFT: GAME-b void*

        // Look up profession type in entity BST at gameData+0x100
        void* bstNode = *(void**)(gameData + 0x100);   // LEFT: WORK-a
        while (bstNode) {
            uint64_t nodeKey = *(uint64_t*)bstNode;
            if (profTypeId < nodeKey)
                bstNode = *(void**)((uintptr_t)bstNode + 0x20);
            else if (profTypeId > nodeKey)
                bstNode = *(void**)((uintptr_t)bstNode + 0x28);
            else
                break;
        }

        int slotCount = 0;
        if (bstNode) {
            slotCount = (int)*(uint64_t*)((uintptr_t)bstNode + 0x10);
        }

        // For each slot in the profession BST node
        for (int slot = 0; slot < slotCount; slot++) {
            ComponentEntity* entity = (*(ComponentEntity***)((uintptr_t)bstNode + 0x08))[slot];   // LEFT: WORK-a

            // Push the entity into this profession row's entity array at +0x30.
            // B.3.2 bridge: ProfessionRow::entities is Collection::Array<ComponentEntity*>
            // (stride 8, count +0x38, capacity +0x3C), raw storage (H5).
            auto& profEntities = GameColl::AsArray<GameColl::Bytes<8, 8>>(&professionRows[p].entities);
            profEntities.SetSize(profEntities.GetSize() + 1, GameColl::Bytes<8, 8>(entity));

            // Check entity for WorkPlaceComponent -> active flag + worker count
            WorkPlaceComponent* workplaceComp = entity->FindComponent<WorkPlaceComponent>();

            // Check if the workplace accepts workers (workplace+0x50 != 0)
            if (workplaceComp->isAccepting != 0) {
                // Find WorkPlaceComponent again to get the WorkComponent
                WorkPlaceComponent* workplaceComp2 = entity->FindComponent<WorkPlaceComponent>();

                // Get parent entity (workplace+0x10)
                ComponentEntity* parentEntity = workplaceComp2->_entity;

                // Find the WorkComponent (DAT_1401c8070)
                WorkComponent* workComp = parentEntity->FindComponent<WorkComponent>();

                // Get max count
                int maxCount = workComp->workState;
                if (maxCount == 0) {
                    maxCount = fn_StorageCountQuery(workComp);
                    workComp->workState = maxCount;
                }

                professionRows[p].slots += maxCount;
            }
        }

        // Update number+count display
        ProfessionRow& profEntry = professionRows[p];
        profEntry.slotsNumber->SetValue(static_cast<int32_t>(profEntry.slots));

        int entityArrayCount = profEntry.entityCount;
        profEntry.buildingsNumber->SetValue(static_cast<int32_t>(entityArrayCount));

        // Set visibility via vtable+0x40 (UI::Element slot 8, Enable)
        profEntry.button->Enable(entityArrayCount > 0);
    }

    // Phase 3: Count adult citizens per profession
    int citizenCount = *(int*)(gameData + 0xa8);   // LEFT: WORK-a
    for (int c = 0; c < citizenCount; c++) {
        CitizenComponent* citizen = (*(CitizenComponent***)(gameData + 0xa0))[c];   // LEFT: WORK-a

        float age = citizen->age;
        float adultAge = citizen->description->adultAge;
        AttendanceComponent* school = citizen->school;

        if (age >= adultAge && school == nullptr) {
            // Adult citizen — look up profession in profBST (+0x50)
            uint64_t profId = reinterpret_cast<uint64_t>(citizen->profession);
            ProfessionNode* profNode = professionTree;
            while (profNode) {
                uint64_t nodeKey = profNode->key;
                if (profId < nodeKey)
                    profNode = profNode->left;
                else if (profId > nodeKey)
                    profNode = profNode->right;
                else
                    break;
            }
            if (profNode) {
                int profIdx = (int)profNode->index;
                professionRows[profIdx].workers += 1;
            }
        }
    }

    // Phase 4: Update spinner elements with final counts
    // First profession entry
    professionRows[0].desired->SetValue(professionRows[0].workers);
    professionRows[0].desired->SetMinMax(professionRows[0].workers, professionRows[0].workers);

    // Remaining profession entries
    int totalProfs = professionRowCount;
    for (int p = 1; p < totalProfs; p++) {
        professionRows[p].desired->SetValue(professionRows[p].workers);

        professionRows[p].desired->SetMinMax(
            0, professionRows[0].workers + professionRows[p].workers);
    }
}

// ============================================================
// TownHallUI::UpdateAll (0x14007d590)
// ============================================================
// Collects entities from 4 entity lists in game data, zeros
// and populates invBST counters, then updates inventory and
// agriculture table UI based on component types.
void TownHallUI::UpdateAll() {
    uintptr_t gameData = (uintptr_t)GameUpdateLoop::_singleton->m_citizenList;   // LEFT: the town manager (WORK-a)
    uintptr_t* gdPtr = (uintptr_t*)gameData;

    // Phase 1: For each tracked entity (gameData+0x110/+0x118),
    // zero or insert entry in invBST
    int trackedCount = *(int*)(gameData + 0x118);   // LEFT: WORK-a
    for (int i = 0; i < trackedCount; i++) {
        uint64_t entityPtr = *(uint64_t*)(*(uintptr_t*)(gameData + 0x110) + (uintptr_t)i * 8);   // LEFT: WORK-a

        // Look up in invBST
        InventoryNode* bstNode = InvBST_Find(invBST, entityPtr);
        if (bstNode) {
            // Zero the counters (retail's qword store over the first two, H7)
            bstNode->counts[0] = 0;
            bstNode->counts[1] = 0;
            bstNode->counts[2] = 0;
        } else {
            // Insert new node with zeroed counters
            struct { uint64_t key; uint64_t val1; uint32_t val2; } entry;
            entry.key = entityPtr;
            entry.val1 = 0;
            entry.val2 = 0;
            void* retNode = nullptr;
            ToolbarUtility_BstInsert_4890(reinterpret_cast<int64_t*>(&invBST), &retNode,
                                          reinterpret_cast<uint64_t*>(&entry));   // 0x140014890; LEFT: its untyped signature (DLG-a)
        }
    }

    // Phase 2: Collect all entities from 4 entity lists
    // Each list is at gameData[i*2] = array ptr, gameData[i*2+1] = count
    //
    // B.3.2 bridge: this is a local Collection::Array<uint64_t> (retail's
    // hand-inlined SetSize on a stack-local {data,size,capacity} triple, not a
    // struct field), so it is a real instance of the template rather than a
    // GameColl::ArrayAt view over someone else's bytes.
    Collection::Array<uint64_t> entityArr;

    for (int listIdx = 0; listIdx < 4; listIdx++) {
        uintptr_t arrayPtr = gdPtr[listIdx * 2];
        int listCount = (int)gdPtr[listIdx * 2 + 1];

        for (int j = 0; j < listCount; j++) {
            uint64_t entityPtr = *(uint64_t*)(arrayPtr + (uintptr_t)j * 8);   // LEFT: WORK-a
            entityArr.SetSize(entityArr.GetSize() + 1, entityPtr);
        }
    }

    // Phase 3: For each collected entity, find components and update BST counters
    uint64_t* entities = entityArr.GetData();
    int entityCount = entityArr.GetSize();
    for (int i = 0; i < entityCount; i++) {
        ComponentEntity* entity = reinterpret_cast<ComponentEntity*>(entities[i]);   // LEFT: the list holds the pointer bits

        // Find StorageComponent (DAT_1401c7e78)
        StorageComponent* storage = entity->FindComponent<StorageComponent>();

        // Find StateMachineComponent (DAT_1401c7d98)
        StateMachineComponent* buildState = entity->FindComponent<StateMachineComponent>();

        int counterIndex; // which of the 3 counters to increment
        if (buildState != nullptr) {
            counterIndex = 2; // under construction
        } else {
            // Check for TradeComponent (DAT_1401c4b80)
            TradeComponent* building = entity->FindComponent<TradeComponent>();
            if (building != nullptr)
                counterIndex = 1; // has building component
            else
                counterIndex = 0; // basic entity
        }

        // Iterate StorageComponent resource entries
        if (storage == nullptr) continue;
        int resourceCount = storage->slotCount;

        for (int r = 0; r < resourceCount; r++) {
            uintptr_t items = (uintptr_t)storage->storageSlots;   // LEFT: W0 raw storage
            uint64_t resourceEntity = *(uint64_t*)(items + (uintptr_t)r * 0x10);
            int resourceQty = *(int*)(items + (uintptr_t)r * 0x10 + 8);

            // Look up resource entity in invBST
            InventoryNode* bstNode = InvBST_Find(invBST, resourceEntity);
            if (bstNode) {
                // Increment the appropriate counter
                bstNode->counts[counterIndex] += resourceQty;
            }
        }
    }

    // entityArr frees its own buffer (rtcoll::MemFree, the same door
    // fn_MemFreeStatic resolves) when it goes out of scope here.
}

// ============================================================
// TownHallUI::OnProfessionButton (0x14007dca0)
// ============================================================
void TownHallUI::OnProfessionButton(int index) {
    ProfessionRow& entryBase = professionRows[index];

    int entityCount = entryBase.entityCount;
    if (entityCount == 0) return;

    profButtonIdx++;
    int cycleIdx = (int)profButtonIdx;
    if (cycleIdx >= entityCount) cycleIdx = 0;
    profButtonIdx = (uint32_t)cycleIdx;

    ComponentEntity* entityPtr = entryBase.entities[cycleIdx];
    UI::DialogInstance* dialogInst = _dialog;
    Toolbar_CenterOnEntity_Impl(GameUpdateLoop::_singleton->m_toolManager, &dialogInst, &entityPtr);
}

// ============================================================
// TownHallUI::OnProfessionSpinner (0x14007dd10)
// ============================================================
void TownHallUI::OnProfessionSpinner(int index) {
    // NO g_wpcRosterOnly guard (removed 2026-08-17, together with the two sibling
    // spinners this comment used to point at). The fault it named — READ of 0x10 in
    // Detach's e710 getter on a LOWER, which this handler does via
    // fn_CitizenList_RemoveWorker — is now guarded at the source
    // (workplacecomponent.cpp:276 returns early on a null accessor). "Our objects can't
    // survive it" stopped being true when that guard landed. Meanwhile the flag also
    // gates Attach's clear of the "no workers" status bit, so holding it left the ? icon
    // up over every building staffed from this tab. Measured on the lower path too: a
    // seat -> unseat -> re-seat cycle drives Detach's full visual tail with no fault.

    // Get desired vs current count
    int desired = professionRows[index].desired->GetValue();
    int current = professionRows[index].workers;

    if (current < desired) {
        int toAdd = desired - current;
        for (int i = 0; i < toAdd; i++) {
            void* citizenList = GameUpdateLoop::_singleton->m_citizenList;
            uint64_t profId = reinterpret_cast<uint64_t*>(GameUpdateLoop::_singleton->m_professionList->arrayData)[index];   // LEFT: ProfessionList::arrayData is void* (GAME-b)

            uint64_t retBuf[2] = {0, 0};
            FindAvailableBuilding(reinterpret_cast<int64_t>(citizenList), retBuf, profId);   // 0x14008d100 (door misnamed fn_CitizenList_RequestWorker)
            uint64_t slot = retBuf[0];

            // Try idle -> working -> student -> any
            uint64_t slotCopy = slot;
            uint8_t findBuf[8] = {0};
            void* result = CitizenEmploy_Strat_cbf0(reinterpret_cast<int64_t>(citizenList), findBuf, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008cbf0
            uint64_t found = *(uint64_t*)result;

            if (found == 0) {
                if (slot != 0) {
                    slotCopy = slot;
                    uint8_t fb2[8] = {0};
                    result = CitizenEmploy_Strat_ca20(reinterpret_cast<int64_t>(citizenList), fb2, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008ca20
                    found = *(uint64_t*)result;
                    if (found != 0) continue;
                }
                slotCopy = slot;
                uint8_t fb3[8] = {0};
                result = CitizenEmploy_Strat_c5f0(reinterpret_cast<int64_t>(citizenList), fb3, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008c5f0
                found = *(uint64_t*)result;

                if (found == 0) {
                    slotCopy = slot;
                    uint64_t anyBuf = 0;
                    CitizenEmploy_Strat_c4b0(reinterpret_cast<int64_t>(citizenList), &anyBuf, reinterpret_cast<int64_t*>(&slotCopy), static_cast<int64_t>(profId));   // 0x14008c4b0
                }
            }
        }
    }

    if (desired < current) {
        int toRemove = current - desired;
        for (int i = 0; i < toRemove; i++) {
            void* citizenList = GameUpdateLoop::_singleton->m_citizenList;
            uint64_t profId = reinterpret_cast<uint64_t*>(GameUpdateLoop::_singleton->m_professionList->arrayData)[index];   // LEFT: ProfessionList::arrayData is void* (GAME-b)
            TownHall_RemoveWorker(citizenList, profId);   // 0x140091180
        }
    }

    // Refresh display
    UpdateInventory();
}

namespace townhallui_sa {

// ---- exe-.rdata type-id globals the TownHallUI paths used to dereference ----
// ENDGAME B.3.9 CG2: every one of the 19 TypeId_* globals this lazy-fill
// serviced is now read through its class static (X::TypeId() — either the
// ported Runtime static for the UI:: widget/element classes, or the exe
// DECLARE_EXE_TYPE static for the Component/Description classes), which
// always answers, so EnsureGlobals retires. B3R: the empty EnsureGlobals that
// was left went with its calls.

// Every global/function the Serialize+Startup+per-frame paths dereference
// unguarded must be live, or a null-deref faults (no SEH under MinGW). B3R: the
// twelve engine_funcs door pointers this tested were always non-null and are
// called by name now; only the ExternalFactory singleton is left.
bool Ready() {
    if (!(::Object::ExternalFactory::_singleton)) return false;
    return true;
}

// The town-side data the update paths read unguarded: townMgr (gw+0xf0) with its
// citizen roster, the profession list (gw+0xb0), the game clock (gw+0x128), and
// the townMgr history/production record blocks (+0x150 / +0x170) that the graph
// and production tabs index into.
bool TownReady() {
    if (!(GameUpdateLoop::_singleton != nullptr)) return false;
    GameUpdateLoop* gw = GameUpdateLoop::_singleton;
    uintptr_t townMgr = (uintptr_t)gw->m_citizenList;   // LEFT: the town manager (WORK-a)
    ProfessionList* profList = gw->m_professionList;
    if (!townMgr || !profList) return false;
    if (profList->arrayCount <= 0 || profList->arrayData == nullptr) return false;
    if (gw->m_dayCounter == nullptr) return false;
    if (*(uintptr_t*)(townMgr + 0x150) == 0) return false;   // stat-history series
    if (*(uintptr_t*)(townMgr + 0x170) == 0) return false;   // production records
    return true;
}

static void Trace(const char* what, const TownHallUI* self) {
    fprintf(stderr, "[TownHallUI] %s self=%p ready=%d town=%d built=%d\n",
            what, static_cast<const void*>(self), Ready() ? 1 : 0, TownReady() ? 1 : 0,
            self ? self->professionRowCount : -1);
    fflush(stderr);
}

// InitProfessions pushed at least one row once Serialize has run.
inline bool Built(const TownHallUI* self) { return self->professionRowCount != 0; }

// Controller bound to its element + dialog instance (framework sets +0x18/+0x20).
inline bool Bound(const TownHallUI* self) {
    return self->_dialog != nullptr && self->_container != nullptr;
}

// One-shot gate diagnostic — prints WHICH component of Ready()/TownReady() is
// missing (a silently-skipping gate looks identical to "blank dialog"). Logs a
// handful of lines once per session; also callable at world-build time via
// TownHallSA_Diag (gameupdateloop.cpp) so the dialog-independent gates are
// visible without opening the panel.
void DiagPrint(const char* where, const TownHallUI* self) {
    // ENDGAME B.3.9 CG2 / TR: every type-id entry this diagnostic used to list
    // retired — their class statics always answer. B3R: so did the twelve
    // "MISSING fn" door-pointer entries (never null; the bodies are called by name).
    fprintf(stderr, "[TownHallSA] %s: self=%p bound=%d built=%d ready=%d town=%d extFactory=%d\n",
            where, static_cast<const void*>(self),
            self ? (Bound(self) ? 1 : 0) : -1,
            self ? (Built(self) ? 1 : 0) : -1,
            Ready() ? 1 : 0, TownReady() ? 1 : 0,
            (::Object::ExternalFactory::_singleton) ? 1 : 0);
    if (GameUpdateLoop::_singleton != nullptr) {
        GameUpdateLoop* gw = GameUpdateLoop::_singleton;
        uintptr_t townMgr = (uintptr_t)gw->m_citizenList;   // LEFT: the town manager (WORK-a)
        ProfessionList* profList = gw->m_professionList;
        fprintf(stderr, "[TownHallSA]   gw=%p townMgr=%p profList=%p profCnt=%d clock=%p hist(+150)=%p prod(+170)=%p\n",
                static_cast<void*>(gw), gw->m_citizenList, static_cast<void*>(profList),
                profList ? profList->arrayCount : -1,
                gw->m_dayCounter,
                townMgr ? (void*)*(uintptr_t*)(townMgr + 0x150) : nullptr,
                townMgr ? (void*)*(uintptr_t*)(townMgr + 0x170) : nullptr);
        // UpdateGeneral's two data sources: the town citizen list (+0xa0/+0xa8,
        // CitizenComponent_OnAdd) and the residence list (+0x20/+0x28, helper_9de0).
        if (townMgr)
            fprintf(stderr, "[TownHallSA]   citizens(+0xa8)=%d homes(+0x28)=%d\n",
                    *(int*)(townMgr + 0xa8), *(int*)(townMgr + 0x28));
    } else {
        fprintf(stderr, "[TownHallSA]   GameUpdateLoop::_singleton=%p\n",
                static_cast<void*>(GameUpdateLoop::_singleton));
    }
    fflush(stderr);
}

// ---- destructor: free the owned containers (mirrors exe 0x1400810d0) ----

void FreeAgricultureTree(TownHallUI::AgricultureNode* node) {
    if (!node) return;
    FreeAgricultureTree(node->left);
    FreeAgricultureTree(node->right);
    System::MemoryAllocator::MemFree(node);
}
void FreeInventoryTree(TownHallUI::InventoryNode* node) {
    if (!node) return;
    FreeInventoryTree(node->left);
    FreeInventoryTree(node->right);
    System::MemoryAllocator::MemFree(node);
}
void FreeProfessionTree(TownHallUI::ProfessionNode* node) {
    if (!node) return;
    FreeProfessionTree(node->left);
    FreeProfessionTree(node->right);
    System::MemoryAllocator::MemFree(node);
}
}  // namespace townhallui_sa

// ============================================================
// TownHallUI::OnCreated (vtable +0x20, 0x140081000)
// ============================================================
// [4] 0x140081000
void TownHallUI::OnCreated() {
    if (!townhallui_sa::Bound(this) || !townhallui_sa::Ready() || !townhallui_sa::TownReady()) { townhallui_sa::Trace("Serialize SKIP", this); return; }
    if (townhallui_sa::Built(this)) return;   // idempotence: the framework or self-heal already built
    townhallui_sa::Trace("Serialize", this);

    tabElement = TownHall_FindElem<UI::TabElement>(_container, "tabs");

    InitGeneral();
    InitProfessions();
    InitProduction();
    InitInventory();
    InitGraph();
    InitAgriculture();
}

// ============================================================
// TownHallUI::OnSerializeSettings (vtable +0x50, 0x140080db0)
// ============================================================
// [10] 0x140080db0
void TownHallUI::OnSerializeSettings(void* stream) {
    if (!stream || !townhallui_sa::Built(this) || !townhallui_sa::Ready() || !townhallui_sa::TownReady()) return;

    int tabIndex = tabElement->_activeTab;
    int combo1 = productionCombo->_currentItem;
    int combo2 = inventoryCombo->_currentItem;
    int combo3 = sortCombo->_currentItem;
    int combo4 = timeCombo->_currentItem;
    int combo5 = graphCombo->_currentItem;

    IO::Serializer serializer(*reinterpret_cast<IO::Stream*>(stream), 1);   // LEFT: the slot's IO::Stream& is spelled void* (DialogController.h)

    serializer.Serialize(tabIndex, nullptr);
    serializer.Serialize(combo1, nullptr);
    serializer.Serialize(combo2, nullptr);

    int version = serializer.GetVersion();
    if (version > 0) {
        serializer.Serialize(combo3, nullptr);
    }
    serializer.Serialize(combo4, nullptr);
    serializer.Serialize(combo5, nullptr);

    if (serializer.GetDirection() == IO::kReading) {
        tabElement->SetActiveTab(tabIndex);
        productionCombo->SetCurrentItem(combo1);
        inventoryCombo->SetCurrentItem(combo2);
        sortCombo->SetCurrentItem(combo3);
        timeCombo->SetCurrentItem(combo4);
        graphCombo->SetCurrentItem(combo5);
        UpdateGraph();
    }
}

// ============================================================
// TownHallUI::OnUpdateLayout (vtable +0x38)
// ============================================================
// [7] 0x14007d550
void TownHallUI::OnUpdateLayout() {
    if (!townhallui_sa::Bound(this) || !townhallui_sa::Ready() || !townhallui_sa::TownReady()) return;
    if (!townhallui_sa::Built(this)) TownHallUI::OnCreated();
    if (!townhallui_sa::Built(this)) return;
    UpdateGeneral();
    UpdateProfessions();
    UpdateProduction();
    UpdateAgriculture();
}

// ============================================================
// TownHallUI::OnUpdate (vtable +0x40)
// ============================================================
// [8] 0x14007e550
void TownHallUI::OnUpdate(const System::Time& /*time*/) {
    static bool s_diagOnce = false;
    if (!s_diagOnce) { s_diagOnce = true; townhallui_sa::DiagPrint("first OnUpdate", this); }
    if (!townhallui_sa::Bound(this) || !townhallui_sa::Ready() || !townhallui_sa::TownReady()) return;
    if (!townhallui_sa::Built(this)) {
        TownHallUI::OnCreated();
        if (!townhallui_sa::Built(this)) return;
        TownHallUI::OnUpdateLayout();
        return;
    }
    int currentTick = *(int*)(*(uintptr_t*)((uintptr_t)GameUpdateLoop::_singleton->m_citizenList + 0x150) + 8);   // LEFT: the town manager (WORK-a)

    if (this->cachedGameTick != currentTick) {
        this->cachedGameTick = currentTick;
        UpdateGraph();
    }

    UpdateInventory();
    UpdateAll();
    if ((_pad1E4)++ % 30 == 0) TownHallUI::OnUpdateLayout();
}

// ============================================================
// TownHallUI::OnElementEvent (vtable +0x48)
// ============================================================
// [9] 0x14007c180
void TownHallUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t msg, int32_t id,
                                int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    if (!townhallui_sa::Built(this) || !townhallui_sa::Ready() || !townhallui_sa::TownReady()) return;
    int profCount = GameUpdateLoop::_singleton->m_professionList->arrayCount;

    if (msg == 0x12) {
        if (id > 0x1998 && id <= 0x1999 + profCount)
            OnProfessionButton(id - 0x1999);
    } else if (msg == 0x14) {
        if (id == 0x1964 || id == 0x1965)
            UpdateGraph();
    } else if (msg == 0x15) {
        if (id > 0x1965 && id <= 0x1966 + profCount)
            OnProfessionSpinner(id - 0x1966);
    }
}

// ============================================================
// STANDALONE promotion — real TownHallUI controller vtable
// ============================================================
// The town-hall panel (Dialog/TownHall.rsc — the General statistics / Professions /
// Production / Inventory / Graph / Agriculture tab dialog opened by clicking a Town
// Hall) is driven by a TownHallUI controller. In standalone this type was a StubNop
// (all slots no-op, see ui_dialog_stub_factories.cpp — that stub registration is
// REMOVED there to avoid a duplicate REGISTER_TYPE), so the dialog opened with every
// field left on its .rsc template default: the "general statistics" numbers never
// populated. This installs a real per-class vtable wiring the fully-reimplemented
// methods above, following the StorageUI/WorkPlaceUI promotion pattern.
//
// Vtable offsets (verified from the exe TownHallUI vtable @ 0x14015dce0):
//   +0x00 (idx0)  ScalarDtor — frees the prof/prod/inv/agri arrays + 3 BSTs
//   +0x10 (idx2)  GetTypeId
//   +0x20 (idx4)  OnCreated  = element discovery + InitGeneral/Professions/
//                 Production/Inventory/Graph/Agriculture (framework setup call)
//   +0x38 (idx7)  OnUpdateLayout = UpdateGeneral/Professions/Production/Agriculture
//   +0x40 (idx8)  OnUpdate   = per-frame: graph refresh on tick change +
//                 UpdateInventory + UpdateAll (the framework dispatches ONLY +0x40
//                 each frame, so the self-heal build + a throttled refresh
//                 are driven here — keeps the General tab's month/counts live)
//   +0x48 (idx9)  OnElementEvent = profession buttons 0x1999+, graph combos
//                 0x1964/0x1965 (msg 0x14), profession spinners 0x1966+ (msg 0x15)
//   +0x50 (idx10) OnSerializeSettings = tab/combo state persist (IO::Serializer stream)
//
// The exe helper functions these methods call are ported in townhall_helpers.cpp
// + citizen_employ_cascade.cpp and called by name.


// World-build-time gate diagnostic (called once from GameUpdateLoop::Update at an
// early frame, see gameupdateloop.cpp). B3R: the publish this called
// (townhallui_sa::EnsureGlobals) had been emptied at B.3.9 -- every global it
// filled is a class static now -- and is deleted. The entry point stays while
// gameupdateloop.cpp calls it.
extern "C" void TownHallSA_Diag(int frameNum) {
    static bool s_done = false;
    if (s_done || frameNum < 120) return;
    s_done = true;
}

// ~TownHallUI: the tree's own teardown (its hand-built slot 0 ran this, then freed
// WITHOUT ~DialogController); moved here unchanged. The compiler now chains
// ~DialogController -> ~Internal after it. Each {data, count, capacity} pair is
// retail's one qword store over count and capacity (H7).
TownHallUI::~TownHallUI() {
    {
        townhallui_sa::FreeAgricultureTree(agriBST);
        agriBST = nullptr; agriBSTCount = 0;
        System::MemoryAllocator::MemFree(agriLabels);
        agriLabels = nullptr; agriLabelCount = 0; agriLabelCap = 0;
        townhallui_sa::FreeInventoryTree(invBST);
        invBST = nullptr; invBSTCount = 0;
        System::MemoryAllocator::MemFree(invTable);
        invTable = nullptr; invTableCount = 0; invTableCap = 0;
        System::MemoryAllocator::MemFree(prodLimits);
        prodLimits = nullptr; prodLimitsCount = 0; prodLimitsCapacity = 0;
        System::MemoryAllocator::MemFree(groupStars.half);
        groupStars.half = nullptr; groupStars.halfCount = 0; groupStars.halfCapacity = 0;
        System::MemoryAllocator::MemFree(groupStars.whole);
        groupStars.whole = nullptr; groupStars.wholeCount = 0; groupStars.wholeCapacity = 0;
        System::MemoryAllocator::MemFree(groupHearts.half);
        groupHearts.half = nullptr; groupHearts.halfCount = 0; groupHearts.halfCapacity = 0;
        System::MemoryAllocator::MemFree(groupHearts.whole);
        groupHearts.whole = nullptr; groupHearts.wholeCount = 0; groupHearts.wholeCapacity = 0;
        townhallui_sa::FreeProfessionTree(professionTree);
        professionTree = nullptr; professionTreeCount = 0;
        int profRows = professionRowCount;
        ProfessionRow* profArr = professionRows;
        for (int i = 0; i < profRows && profArr; i++) {
            System::MemoryAllocator::MemFree(profArr[i].entities);
            profArr[i].entities = nullptr; profArr[i].entityCount = 0; profArr[i].entityCapacity = 0;
        }
        System::MemoryAllocator::MemFree(professionRows);
        professionRows = nullptr; professionRowCount = 0; professionRowCapacity = 0;
    }
}






// create 0x140014660: MemAlloc(0x1e8, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* TownHallUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(TownHallUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) TownHallUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* TownHallUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) TownHallUI());
}

DEFINE_EXE_TYPE(TownHallUI, "UI::DialogController")
