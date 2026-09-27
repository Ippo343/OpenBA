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
#include "engine_funcs.h"
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

// ============================================================
// ProductionUI struct layout:
//   +0x00: DialogController base (0x30 bytes)
//   +0x30: consumed array data ptr (8 bytes)
//   +0x38: consumed array count (uint32_t)
//   +0x3C: consumed array capacity (uint32_t)
//   +0x40: produced array data ptr (8 bytes)
//   +0x48: produced array count (uint32_t)
//   +0x4C: produced array capacity (uint32_t)
//
// Each array element = 0x10 bytes:
//   +0x00: InternalBasePtr to "labelStorage" element
//   +0x08: InternalBasePtr to "numberCount" element
// ============================================================

// Helper: dynamically grow and append a 0x10-byte {labelPtr, numberPtr} entry
// to one of the arrays.
// B.3.2 bridge: self+arrayOffset is Collection::Array<{InternalBasePtr,
// InternalBasePtr}> (stride 0x10, count +0x08, capacity +0x0C off
// arrayOffset) inside a struct §B.2 has not converted, so it is reached at
// its offset.
static void PushElementPair(uintptr_t self, int arrayOffset,
                            uint64_t labelPtr, uint64_t numberPtr) {
    auto& arr = GameColl::ArrayAt<GameColl::Bytes<0x10, 8>>(self, arrayOffset);
    arr.SetSize(arr.GetSize() + 1, GameColl::Bytes<0x10, 8>(labelPtr, numberPtr));
}

// ---- element type ids for retail's two casts --------------------------------
// Retail Slot4 stores `IsDerivedFrom(hit, id) ? hit : nullptr`, reading the ids
// through exe .rdata exrefs 0x140151668 (UI::LabelElement) and 0x1401516c8
// (UI::NumberElement — the engine_funcs label "SpinnerElement" is a known
// misnomer, see standalone/reg_element_typeids.cpp:35).
//
// ENDGAME B.3.9 CG2: g_TypeId_LabelElement / g_TypeId_NumberElement -> the ported
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

static void* CastLabel(void* elem) {
    if (!elem) return nullptr;
    return fn_IsDerivedFrom(elem, ProductionUI_LabelElementTypeId()) ? elem : nullptr;
}
static void* CastNumber(void* elem) {
    if (!elem) return nullptr;
    return fn_IsDerivedFrom(elem, ProductionUI_NumberElementTypeId()) ? elem : nullptr;
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
    // deliberately NOT published into g_TypeId_LabelElement / g_TypeId_NumberElement,
    // which reg_element_typeids.cpp owns and keeps behind BANISHED_LIVE_ELEMENT_TIDS.
    bool Ready() {
        // ENDGAME B.3.9 CG2: g_TypeId_ComponentEntity, g_TypeId_RawMaterialDescription,
        // g_TypeId_TrackerComponent -> the class statics, which always answer.
        bool ok = g_pGameWorldSingleton;
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
    // rows there is nothing for the retail body to write, and self+0x30/+0x40 are
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
    uintptr_t s = (uintptr_t)this;

    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)(s + 0x20);

    // Find "production" element (with fallbacks)
    uint64_t result = 0;

    { System::String nameStr("production");
      ::new (&result) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr)); }  // B.3.6 bridge

    if (result == 0) {
        System::String nameStr("production3");
        uint64_t retBuf = 0;
        ::new (&retBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr));  // B.3.6 bridge
        result = retBuf;

        if (result == 0) {
            System::String nameStr2("production4");
            uint64_t retBuf2 = 0;
            ::new (&retBuf2) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(dialogElem)->FindElement(nameStr2));  // B.3.6 bridge
            result = retBuf2;
        }
    }

    void* productionElem = (void*)result;

    // Get two child containers: Child(0) = consumed, Child(1) = produced
    uint64_t consumedContainer = 0;
    ::new (&consumedContainer) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(productionElem)->Child(0));  // B.3.6 bridge

    uint64_t producedContainer = 0;
    ::new (&producedContainer) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(productionElem)->Child(1));  // B.3.6 bridge

    // Iterate consumed container children (start at index 1)
    int childCount = reinterpret_cast<const UI::Element*>(consumedContainer)->ChildCount();  // B.3.6 bridge
    int idx = 1;
    while (idx < childCount) {
        // Get child element at index
        uint64_t childElem = 0;
        ::new (&childElem) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(consumedContainer)->Child(idx));  // B.3.6 bridge

        // Find "labelStorage" sub-element
        uint64_t labelResult = 0;
        { System::String nameStr("labelStorage");
          ::new (&labelResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(childElem)->FindElement(nameStr)); }  // B.3.6 bridge

        // Retail casts the hit through IsDerivedFrom(LabelElement) and stores NULL
        // when it isn't one (0x140075da0 uses g_TypeId_LabelElement_exref @0x140151668).
        uint64_t labelBP = 0;
        new (&labelBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(CastLabel((void*)labelResult)));   // B.3.3 bridge

        // Find "numberCount" sub-element
        uint64_t numberResult = 0;
        { System::String nameStr("numberCount");
          ::new (&numberResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(childElem)->FindElement(nameStr)); }  // B.3.6 bridge

        // Retail casts through IsDerivedFrom(NumberElement) (exref 0x1401516c8,
        // engine_funcs label "SpinnerElement" — the id is UI::NumberElement's).
        uint64_t numberBP = 0;
        new (&numberBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(CastNumber((void*)numberResult)));   // B.3.3 bridge

        // Push pair to consumed array at +0x30
        PushElementPair(s, 0x30, labelBP, numberBP);

        idx++;
        childCount = reinterpret_cast<const UI::Element*>(consumedContainer)->ChildCount();  // B.3.6 bridge
    }

    // Iterate produced container children (start at index 1)
    childCount = reinterpret_cast<const UI::Element*>(producedContainer)->ChildCount();  // B.3.6 bridge
    idx = 1;
    while (idx < childCount) {
        uint64_t childElem = 0;
        ::new (&childElem) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(producedContainer)->Child(idx));  // B.3.6 bridge

        uint64_t labelResult = 0;
        { System::String nameStr("labelStorage");
          ::new (&labelResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(childElem)->FindElement(nameStr)); }  // B.3.6 bridge

        uint64_t labelBP = 0;
        new (&labelBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(CastLabel((void*)labelResult)));   // B.3.3 bridge

        uint64_t numberResult = 0;
        { System::String nameStr("numberCount");
          ::new (&numberResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(childElem)->FindElement(nameStr)); }  // B.3.6 bridge

        uint64_t numberBP = 0;
        new (&numberBP) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(CastNumber((void*)numberResult)));   // B.3.3 bridge

        // Push pair to produced array at +0x40
        PushElementPair(s, 0x40, labelBP, numberBP);

        idx++;
        childCount = reinterpret_cast<const UI::Element*>(producedContainer)->ChildCount();  // B.3.6 bridge
    }
}

// ============================================================
// ProductionUI::Slot7 helper — update one production list
// ============================================================
// Updates the UI for either consumed (listIndex=0) or produced (listIndex=1)
// items by reading from the TrackerComponent and setting sprite/text/count
// on cached label+number elements.
//
// Original helper: 0x140075A50.



static void UpdateProductionList(uintptr_t self, int listIndex) {
    // Get entity from controller data (+0x28)
    void* entity = *(void**)(self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;

    uint64_t entityBP = 0;
    new (&entityBP) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge

    // Find TrackerComponent via fn_FindComponent_Tracker
    uint64_t trackerBP = 0;
    fn_FindComponent_Tracker((void*)entityBP, &trackerBP);
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
    int gameItemCount = **(int**)((uintptr_t)*g_pGameWorldSingleton + 0x128);
    if (gameItemCount != *(int*)(tracker + 0x48)) {
        // Call tracker rebuild: FUN_14000d9d0 moves data from +0x38 to +0x28
        fn_TrackerComponent_Rebuild((void*)(tracker + 0x38), (void*)(tracker + 0x28));
        fn_MemFreeStatic(*(void**)(tracker + 0x28));
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

    // Get element array for this list
    uintptr_t elemArrayPtr = *(uintptr_t*)(self + ((int64_t)listIndex + 3) * 0x10);
    int elemCount = *(int*)(self + (int64_t)listIndex * 0x10 + 0x38);


    int i = 0;
    while (i < itemCount) {
        // Stop if we've used all cached UI elements
        if (i == elemCount) break;

        uintptr_t elemPair = elemArrayPtr + (uintptr_t)i * 0x10;

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
        void* resourceDesc = nullptr;

        for (int j = 0; j < subCount; j++) {
            void* subEntry = *(void**)(subArray + (uintptr_t)j * 8);
            // Call GetTypeId virtual (vtable+0x10)
            int typeId = reinterpret_cast<Object::Base*>(subEntry)->Type();  // B.3.8 bridge
            if (typeId == (int)RawMaterialDescription::TypeId()) {
                if (fn_IsDerivedFrom(subEntry, RawMaterialDescription::TypeId())) {
                    resourceDesc = subEntry;
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
        // Create sprite from ResourceDescription
        uintptr_t rd = (uintptr_t)resourceDesc;
        uint8_t spriteBuf[16];
        ::new (spriteBuf) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(rd + 0x20))), *reinterpret_cast<const System::String*>(rd + 0x30));  // B.3.6 bridge

        // Set sprite on label element
        void* labelElem = (void*)*(uint64_t*)elemPair;
        reinterpret_cast<UI::LabelElement*>(labelElem)->SetSprite(reinterpret_cast<const UI::Sprite*>(spriteBuf));  // B.3.6 bridge

        // Create UI::String from ResourceDescription
        uint8_t uiStrBuf[16];
        ::new (uiStrBuf) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(rd + 0x28))), *reinterpret_cast<const System::String*>(rd + 0x30));  // B.3.6 bridge

        // Set text on label element
        reinterpret_cast<UI::TextElement*>(labelElem)->SetText(reinterpret_cast<const UI::String*>(uiStrBuf));  // B.3.6 bridge

        // Set count on number element
        void* numberElem = (void*)*(uint64_t*)(elemPair + 8);
        reinterpret_cast<UI::NumberElement*>(numberElem)->SetValue(static_cast<int32_t>(count));  // B.3.6 bridge

        // Show both elements
        reinterpret_cast<UI::Element*>(labelElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge
        reinterpret_cast<UI::Element*>(numberElem)->SetFlagsOnOff(0, 1);  // B.3.6 bridge

        i++;
    }

    // Hide unused elements
    while (i < elemCount) {
        uintptr_t elemPair = elemArrayPtr + (uintptr_t)i * 0x10;

        // Set empty sprite on label element
        uint8_t emptySprite[16];
        ::new (emptySprite) UI::Sprite();  // B.3.6 bridge
        void* labelElem = (void*)*(uint64_t*)elemPair;
        reinterpret_cast<UI::LabelElement*>(labelElem)->SetSprite(reinterpret_cast<const UI::Sprite*>(emptySprite));  // B.3.6 bridge

        // Set empty text on label element
        uint8_t emptyStr[16];
        ::new (emptyStr) UI::String();  // B.3.6 bridge
        reinterpret_cast<UI::TextElement*>(labelElem)->SetText(reinterpret_cast<const UI::String*>(emptyStr));  // B.3.6 bridge

        // Hide number element
        void* numberElem = (void*)*(uint64_t*)(elemPair + 8);
        reinterpret_cast<UI::Element*>(numberElem)->SetFlagsOnOff(1, 0);  // B.3.6 bridge

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
    // rows there is nothing for the retail body to write, and self+0x30/+0x40 are
    // still null.
    if (*reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(this) + 0x38) == 0 &&
        *reinterpret_cast<uint32_t*>(reinterpret_cast<uintptr_t>(this) + 0x48) == 0) return;
    uintptr_t s = (uintptr_t)this;
    UpdateProductionList(s, 0);
    UpdateProductionList(s, 1);
}

// ============================================================
