// code/Game/Dialog/productionui.cpp
// Clean reimplementation of ProductionUI game logic.
//
// ProductionUI (0x50 bytes) -- production display panel for buildings.
// Extends DialogController. Two dynamic arrays at +0x30 and +0x40 hold
// pairs of {labelStorage, numberCount} element InternalBasePtrs for
// consumed items (array1) and produced items (array2).
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "ui/UserInterface.h"
#include "object/TypeInfo.h"
#include "Entity/exe_bodies.h"
#include "Dialog/exe_bodies.h"
#include "Game/GameUpdateLoop.h"   // B3R STORAGE: GameUpdateLoop::_singleton
#include "Object/factory_registration.h"   // Object::ResolveTypeIdByNameLazy
#include "system/String.h"
#include "Component/coll_bridge.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/String.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/Sprite.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/SpriteSheet.h"  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/StringTable.h"  // ENDGAME B.3.6 UI callers (K3)
#include "object/External.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/LabelElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/NumberElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/TextElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/RawMaterialDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/TrackerComponent.h"   // X::TypeId() (B.3.9 CG2)
#include "system/MemoryAllocator.h"   // B3R: the allocator doors
#include "Dialog/ProductionUI.h"

// ============================================================
// ProductionUI layout: Dialog/ProductionUI.h (two Collection::Arrays of
// ProductionUIRow {labelStorage, numberCount} at +0x30 consumed, +0x40 produced).
// LEFT (B4 DLG-c): the TrackerComponent's fields (TrackerComponent.h, COMP-e:
// raw) and the day counter through GameUpdateLoop::m_dayCounter (void*, GAME-a).
// ============================================================

// ---- element type ids for retail's two casts --------------------------------
// Retail Slot4 stores `IsDerivedFrom(hit, id) ? hit : nullptr`, reading the ids
// through exe .rdata exrefs 0x140151668 (UI::LabelElement) and 0x1401516c8
// (UI::NumberElement — the engine_funcs label "SpinnerElement" is a known
// misnomer, see standalone/reg_element_typeids.cpp:35).
//
// ENDGAME B.3.9 CG2: TypeId_LabelElement / TypeId_NumberElement -> the ported
// Runtime statics UI::LabelElement::TypeId() / UI::NumberElement::TypeId(). That
// static is a source independent of the exe-side reflection cell (and the
// BANISHED_LIVE_ELEMENT_TIDS gate) this file used to read through the shared
// global via a private by-name fallback; it answers correctly either way, so the
// ResolveTypeIdByNameLazy fallback retires with this conversion.
static uint32_t ProductionUI_LabelElementTypeId() {
    return UI::LabelElement::TypeId();
}
static uint32_t ProductionUI_NumberElementTypeId() {
    return UI::NumberElement::TypeId();
}

// Gate helper for the promotion in ui_dialog_stub_factories.cpp.
bool ProductionUI_TypeIdsReady() {
    return ProductionUI_LabelElementTypeId() != 0
        && ProductionUI_NumberElementTypeId() != 0;
}

static UI::LabelElement* CastLabel(UI::Element* elem) {
    if (!elem) return nullptr;
    return ::Object::IsDerivedFromNullSafe(elem, ProductionUI_LabelElementTypeId())
        ? static_cast<UI::LabelElement*>(elem) : nullptr;
}
static UI::NumberElement* CastNumber(UI::Element* elem) {
    if (!elem) return nullptr;
    return ::Object::IsDerivedFromNullSafe(elem, ProductionUI_NumberElementTypeId())
        ? static_cast<UI::NumberElement*>(elem) : nullptr;
}

// Helper: find a named element under a parent element.
static UI::Element* FindChildElem(UI::Element* parent, const char* name) {
    System::String nameStr(name);
    return parent->FindElement(nameStr).Get();
}

// ---- ProductionUI: PROMOTED to the real reimpl (production-statistics content) -
// ProductionUI drives Dialog/Production.rsc:production4 at pageUser — the page
// PageToggleUI above flips to. Retail bodies:
//   Slot4 (+0x20, Setup)  0x140075da0 — find "production"/"production3"/"production4",
//                         take child 0 (consumed) + child 1 (produced), and cache
//                         each row's {labelStorage, numberCount} pair into the two
//                         dynamic arrays at +0x30 / +0x40.
//   Slot7 (+0x38, Update) 0x140075a20 -> helper 0x140075a50 twice (list 0, list 1) —
//                         reads the entity's TrackerComponent and fills sprite/text/
//                         count per row, hiding the leftovers.
//   Slot3 (+0x18)         0x140076300 decompiles to a bare `return;` — left a no-op.
// The generic stub made both no-ops, so even once the page could be reached it was
// blank.
//
// HAZARD: Slot7 runs EVERY frame, including before Slot4 has populated the caches,
// and the helper derefs tracker+0x48 unguarded. The tracker id is fixed properly
// (standalone/reg_tracker_typeid.cpp registers .rdata 0x1401c4b10 and game_main
// fills it after Object::Factory::Startup); the remaining null-tracker case is
// guarded inside productionui.cpp with an explicit standalone rationale.
namespace productionui_sa {
    // ComponentEntity (.rdata 0x1401c5d70) and RawMaterialDescription (0x1401c7cb8)
    // are already registered+filled, ungated, by reg_action_child_ctors2, and
    // TrackerComponent (0x1401c4b10) by reg_tracker_typeid.cpp. The two ELEMENT ids
    // are resolved privately inside productionui.cpp (ProductionUI_TypeIdsReady) —
    // deliberately NOT published into TypeId_LabelElement / TypeId_NumberElement,
    // which reg_element_typeids.cpp owns and keeps behind BANISHED_LIVE_ELEMENT_TIDS.
    bool Ready() {
        // ENDGAME B.3.9 CG2: TypeId_ComponentEntity, TypeId_RawMaterialDescription,
        // TypeId_TrackerComponent -> the class statics, which always answer.
        bool ok = GameUpdateLoop::_singleton;
        static bool s_logged = false;
        if (!s_logged) {
            s_logged = true;
            fprintf(stderr, "[ProductionUI] Ready=%d entity=%u rawmat=%u tracker=%u elementIds=%d\n",
                    (int)ok,
                    ComponentEntity::TypeId(),
                    RawMaterialDescription::TypeId(),
                    TrackerComponent::TypeId(),
                    (int)ProductionUI_TypeIdsReady());
            fflush(stderr);
        }
        return ok;
    }
    // Standalone-only precondition (see PageToggleUI::Slot7 above): with no cached
    // rows there is nothing for the retail body to write, and the two arrays are
    // still null.
}

// ============================================================
// ProductionUI::Slot4 (vtable +0x20) -- Setup
// ============================================================
// Finds the "production" container (fallback "production3", "production4"),
// gets two child containers (consumed items, produced items), and for
// each child's sub-children (starting from index 1), finds "labelStorage"
// and "numberCount" elements and caches them in the dynamic arrays.
//
// Original: 0x140075DA0.

// [4] 0x140075da0
void ProductionUI::OnCreated() {
    if (!productionui_sa::Ready()) return;

    // The controller's container element (+0x20)
    UI::Element* dialogElem = _container;

    // Find "production" element (with fallbacks)
    UI::Element* productionElem = FindChildElem(dialogElem, "production");

    if (productionElem == nullptr) {
        productionElem = FindChildElem(dialogElem, "production3");

        if (productionElem == nullptr) {
            productionElem = FindChildElem(dialogElem, "production4");
        }
    }

    // Get two child containers: Child(0) = consumed, Child(1) = produced
    UI::Element* consumedContainer = productionElem->Child(0).Get();
    UI::Element* producedContainer = productionElem->Child(1).Get();

    // Iterate consumed container children (start at index 1)
    int childCount = consumedContainer->ChildCount();
    int idx = 1;
    while (idx < childCount) {
        // Get child element at index
        UI::Element* childElem = consumedContainer->Child(idx).Get();

        // Retail casts the "labelStorage" hit through IsDerivedFrom(LabelElement) and
        // stores NULL when it isn't one (0x140075da0 uses TypeId_LabelElement_exref
        // @0x140151668); "numberCount" through IsDerivedFrom(NumberElement) (exref
        // 0x1401516c8, engine_funcs label "SpinnerElement" — the id is NumberElement's).
        ProductionUIRow row;
        row.labelStorage = CastLabel(FindChildElem(childElem, "labelStorage"));
        row.numberCount = CastNumber(FindChildElem(childElem, "numberCount"));

        // Push pair to the consumed array (+0x30).
        // B.3.2 bridge: Collection::Array<ProductionUIRow>, raw storage until
        // ~ProductionUI destroys it (H5).
        auto& arr = GameColl::ArrayAt<ProductionUIRow>(this, 0x30);
        arr.SetSize(arr.GetSize() + 1, row);

        idx++;
        childCount = consumedContainer->ChildCount();
    }

    // Iterate produced container children (start at index 1)
    childCount = producedContainer->ChildCount();
    idx = 1;
    while (idx < childCount) {
        UI::Element* childElem = producedContainer->Child(idx).Get();

        ProductionUIRow row;
        row.labelStorage = CastLabel(FindChildElem(childElem, "labelStorage"));
        row.numberCount = CastNumber(FindChildElem(childElem, "numberCount"));

        // Push pair to the produced array (+0x40), the same bridge
        auto& arr = GameColl::ArrayAt<ProductionUIRow>(this, 0x40);
        arr.SetSize(arr.GetSize() + 1, row);

        idx++;
        childCount = producedContainer->ChildCount();
    }
}

// ============================================================
// ProductionUI::UpdateList — update one production list
// ============================================================
// Updates the UI for either consumed (listIndex=0) or produced (listIndex=1)
// items by reading from the TrackerComponent and setting sprite/text/count
// on cached label+number elements.
//
// Original helper: 0x140075A50.

void ProductionUI::UpdateList(int listIndex) {
    // Get entity from the controller's data source (+0x28)
    Object::Base* entity = _dataSource;
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    ComponentEntity* entityPtr = isEntity ? static_cast<ComponentEntity*>(entity) : nullptr;

    // Find TrackerComponent via fn_FindComponent_Tracker
    uint64_t trackerBP = 0;
    fn_FindComponent_Tracker(entityPtr, &trackerBP);
    uintptr_t tracker = (uintptr_t)trackerBP;

    // STANDALONE DEVIATION (the only guard in this file). Retail derefs
    // tracker+0x48 unguarded because every entity that can reach Dialog/
    // Production.rsc carries a TrackerComponent. Our panel templates are shared
    // across entity kinds and this controller runs EVERY frame (slot 7 is the
    // per-frame update, dispatched before any click ever selects a producing
    // building), so a bound entity legitimately without a TrackerComponent would
    // fault here. Skipping the list update is the correct standalone behaviour —
    // there is nothing to show. Do NOT widen this into "null checks everywhere":
    // if the pasture/crop entity itself lacks a tracker, that is an upstream bug.
    if (!tracker) {
        return;
    }

    // Check if game world item count changed; rebuild tracker if so
    int gameItemCount = *(int*)GameUpdateLoop::_singleton->m_dayCounter;   // LEFT: void* (GAME-a)
    if (gameItemCount != *(int*)(tracker + 0x48)) {
        // Call tracker rebuild: FUN_14000d9d0 moves data from +0x38 to +0x28
        fn_TrackerComponent_Rebuild((void*)(tracker + 0x38), (void*)(tracker + 0x28));
        System::MemoryAllocator::MemFree(*(void**)(tracker + 0x28));
        *(uint64_t*)(tracker + 0x28) = 0;
        *(uint64_t*)(tracker + 0x30) = 0;
        *(int*)(tracker + 0x48) = gameItemCount;
    }

    // Get item count for this list
    int itemCount;
    if (listIndex == 0) {
        itemCount = *(int*)(tracker + 0x30);
    } else {
        itemCount = *(int*)(tracker + 0x40);
    }

    // Get element array for this list (+0x30 consumed, +0x40 produced)
    ProductionUIRow* elemArray = (listIndex == 0) ? consumedData : producedData;
    int elemCount = (listIndex == 0) ? consumedCount : producedCount;


    int i = 0;
    while (i < itemCount) {
        // Stop if we've used all cached UI elements
        if (i == elemCount) break;

        ProductionUIRow& elemPair = elemArray[i];

        // Get item array from tracker
        uintptr_t itemArray;
        if (listIndex == 0) {
            itemArray = *(uintptr_t*)(tracker + 0x28);
        } else {
            itemArray = *(uintptr_t*)(tracker + 0x38);
        }

        uintptr_t item = *(uintptr_t*)(itemArray + (uintptr_t)i * 0x10);

        // Search sub-entries for ResourceType description
        int subCount = *(int*)(item + 0x30);
        uintptr_t subArray = *(uintptr_t*)(item + 0x28);
        RawMaterialDescription* resourceDesc = nullptr;

        for (int j = 0; j < subCount; j++) {
            Object::Base* subEntry = *(Object::Base**)(subArray + (uintptr_t)j * 8);
            // Call GetTypeId virtual (vtable+0x10)
            int typeId = subEntry->Type();
            if (typeId == (int)RawMaterialDescription::TypeId()) {
                if (::Object::IsDerivedFromNullSafe(subEntry, RawMaterialDescription::TypeId())) {
                    resourceDesc = static_cast<RawMaterialDescription*>(subEntry);
                }
                break;
            }
        }

        // Get count from item entry (+0x08 relative to item array base)
        if (listIndex == 0) {
            itemArray = *(uintptr_t*)(tracker + 0x28);
        } else {
            itemArray = *(uintptr_t*)(tracker + 0x38);
        }
        int count = *(int*)(itemArray + (uintptr_t)i * 0x10 + 0x08);
        // Create sprite from the RawMaterialDescription
        UI::Sprite sprite(resourceDesc->_spriteSheet, resourceDesc->_name);

        // Set sprite on label element
        UI::LabelElement* labelElem = elemPair.labelStorage;
        labelElem->SetSprite(&sprite);

        // Create UI::String from the RawMaterialDescription
        UI::String uiStr(resourceDesc->_stringTable, resourceDesc->_name);

        // Set text on label element
        labelElem->SetText(&uiStr);

        // Set count on number element
        UI::NumberElement* numberElem = elemPair.numberCount;
        numberElem->SetValue(static_cast<int32_t>(count));

        // Show both elements
        labelElem->SetFlagsOnOff(0, 1);
        numberElem->SetFlagsOnOff(0, 1);

        i++;
    }

    // Hide unused elements
    while (i < elemCount) {
        ProductionUIRow& elemPair = elemArray[i];

        // Set empty sprite on label element
        UI::Sprite emptySprite;
        UI::LabelElement* labelElem = elemPair.labelStorage;
        labelElem->SetSprite(&emptySprite);

        // Set empty text on label element
        UI::String emptyStr;
        labelElem->SetText(&emptyStr);

        // Hide number element
        UI::NumberElement* numberElem = elemPair.numberCount;
        numberElem->SetFlagsOnOff(1, 0);

        i++;
    }
}

// ============================================================
// ProductionUI::Slot7 (vtable +0x38) -- Update
// ============================================================
// Updates both consumed and produced item lists.
//
// Original: 0x140075A20.

// [7] 0x140075a20
void ProductionUI::OnUpdateLayout() {
    if (!productionui_sa::Ready()) return;
    // Standalone-only precondition (see PageToggleUI::OnUpdateLayout): with no cached
    // rows there is nothing for the retail body to write, and the two arrays are
    // still null.
    if (consumedCount == 0 && producedCount == 0) return;
    UpdateList(0);
    UpdateList(1);
}

// ============================================================
