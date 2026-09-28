// engine/src/ui/pastureui.cpp
// Clean reimplementation of PastureUI game logic.
//
// PastureUI (0x40 bytes) — pasture animal selection panel.
// Extends SelectResourceUI > DialogController.
// +0x30 = combo element (SelectResourceUI base), +0x38 = slider element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)
#include "runtime/types/ui/ComboElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "runtime/types/ui/SliderElement.h"  // ENDGAME B.3.6 UI callers round 2 (K3)
#include "Object/factory_registration.h"
#include <cstdlib>
#include <cstdio>
#include "Component/AnimalDescription.h"   // X::TypeId() (B.3.9 CG2)
#include "Entity/ComponentEntity.h"   // X::TypeId() (B.3.9 CG2)
#include "Component/Component.h"   // B.3.x W0: Component::Command by name

// ============================================================
// File-local helper: PastureCalcMaxCount_Impl (FUN_140128570)
// ============================================================
// Calculates the maximum number of animals that can be grazed in the pasture.
//
// PastureComponent (param_1) fields used:
//   +0x10: InternalBasePtr -> entity (for FindComponent_Build)
//   +0x28: InternalBasePtr -> animal description entity
//          (its component list is searched for AnimalDescription)
//
// BuildComponent fields used (found via entity at +0x10):
//   +0x2c: int16 maxOccupants
//   +0x2e: int16 currentOccupants
//
// AnimalDescription (found in +0x28 entity's components):
//   +0x24: int slotsPerAnimal (how many pasture slots each animal uses)
//
// Formula: (maxOccupants * currentOccupants) / slotsPerAnimal
// Returns 0 if the animal description entity is not set (null at +0x28).
//
// Original: FUN_140128570.
//
static uint32_t PastureCalcMaxCount_Impl(uintptr_t pastureComp) {
    // If no animal type set, return 0
    if (*(uintptr_t*)(pastureComp + 0x28) == 0) return 0;

    // Find BuildComponent on the entity at +0x10
    uintptr_t entity = *(uintptr_t*)(pastureComp + 0x10);

    uint64_t buildCompRet = 0;
    fn_FindComponent_Build((void*)entity, &buildCompRet);
    uintptr_t buildComp = buildCompRet;

    int16_t maxOcc = buildComp ? *(int16_t*)(buildComp + 0x2c) : 0;
    int16_t curOcc = buildComp ? *(int16_t*)(buildComp + 0x2e) : 0;

    // Search animal description entity's components for AnimalDescription
    uintptr_t animalEntity = *(uintptr_t*)(pastureComp + 0x28);

    int compCount = *(int*)(animalEntity + 0x30);
    uintptr_t* compArray = *(uintptr_t**)(animalEntity + 0x28);
    uint32_t animalDescTypeId = AnimalDescription::TypeId();

    void* animalDesc = nullptr;
    for (int i = 0; i < compCount; i++) {
        uintptr_t compIPtr = compArray[i];
        void* comp = (void*)compIPtr;
        if (!comp) continue;
        int typeId = reinterpret_cast<Object::Base*>(comp)->Type();  // B.3.8 bridge
        if (typeId == (int)animalDescTypeId) {
            bool isAnimalDesc = ::Object::IsDerivedFromNullSafe(comp, animalDescTypeId);
            if (isAnimalDesc) { animalDesc = comp; break; }
        }
    }

    if (!animalDesc) return 0;
    int slotsPerAnimal = *(int*)((uintptr_t)animalDesc + 0x24);
    if (slotsPerAnimal == 0) return 0;
    return (uint32_t)((int)(maxOcc * curOcc) / slotsPerAnimal);
}

// ============================================================
// File-local helper: PastureUI_UpdateSlider_Impl (FUN_1400b4fa0)
// ============================================================
// Finds PastureComponent on the entity at self+0x28.
// Updates the slider element at self+0x38:
//   +0x118 = 0 (min value)
//   +0x11c = (float)PastureCalcMaxCount (max value)
//   +0x128 = (float)pastureComp->currentAnimalCount (+0x40)
//
// Original: FUN_1400b4fa0.
//
static void PastureUI_UpdateSlider_Impl(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Reached from per-frame + event slots; bail on a dead / marked datasource.
    if (!SA_DialogEntityAlive(self)) return;
    // Get entity from self+0x28, wrap in InternalBasePtr
    void* entity = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
    void* entityPtr = isEntity ? entity : nullptr;
    uint64_t entityIPtr = 0;
    new (&entityIPtr) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entityPtr));   // B.3.3 bridge
    uintptr_t entityVal = entityIPtr;

    // Find PastureComponent
    uint64_t pastureRet = 0;
    fn_FindComponent_Pasture((void*)entityVal, &pastureRet);
    uintptr_t pastureComp = pastureRet;
    if (!pastureComp) return;

    // Get slider element at self+0x38
    uintptr_t slider = *(uintptr_t*)(s + 0x38);
    if (!slider) return;

    // Set slider min to 0
    *(float*)(slider + 0x118) = 0.0f;

    // Set slider max = PastureCalcMaxCount
    uint32_t maxCount = PastureCalcMaxCount_Impl(pastureComp);
    slider = *(uintptr_t*)(s + 0x38);
    *(float*)(slider + 0x11c) = (float)(int)maxCount;

    // Set slider current = current animal count at pastureComp+0x40
    pastureComp = pastureRet;
    int currentCount = *(int*)(pastureComp + 0x40);
    slider = *(uintptr_t*)(s + 0x38);
    *(float*)(slider + 0x128) = (float)currentCount;
}

namespace pastureui_sa {

static void Trace(const char* what, void* self) {
    fprintf(stderr, "[PastureUI] %s self=%p\n", what, self);
    fflush(stderr);
}

}  // namespace pastureui_sa

// ============================================================
// PastureUI::Slot4 (vtable +0x20) — Initialize combo + slider elements
// ============================================================
// Calls base SelectResourceUI::Slot4 (find combo, populate),
// then finds "slider" element, stores at +0x38, sets ID 501,
// and updates slider display.
//
// Original: 0x1400B5090.

// [4] 0x1400b5090
void PastureUI::OnCreated() {
    if (!*reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(this) + 0x18)) {
        pastureui_sa::Trace("Slot4 SKIP (no DialogInstance)", this);
        return;
    }
    pastureui_sa::Trace("Slot4", this);
    uintptr_t s = (uintptr_t)this;

    // Initialize combo via base class
    SelectResourceUI::OnCreated();

    // Find "slider" element in parent element at +0x20
    System::String nameBuf("slider");

    void* parentElem = *(void**)(s + 0x20);
    uint64_t findResult = 0;
    ::new (&findResult) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(parentElem)->FindElement(nameBuf));  // B.3.6 bridge

    // Store slider element at +0x38 (skip IsDerivedFrom, null check only)
    void* rawElem = (void*)findResult;
    void* checked = rawElem ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    new (&basePtrVal) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(checked));   // B.3.3 bridge
    *(uint64_t*)(s + 0x38) = basePtrVal;

    // Set slider element ID to 501 (0x1f5)
    reinterpret_cast<UI::Element*>(*(uint64_t*)(s + 0x38))->SetElementId(0x1f5);  // B.3.6 bridge

    // Update slider display with current/max animal counts
    PastureUI_UpdateSlider_Impl(this);
}

// ============================================================
// PastureUI::Slot7 (vtable +0x38) — Update slider text + combo enable
// ============================================================
// Enables/disables combo based on item count and current animal count.
// Updates slider text parameters with current and max animal counts.
//
// Original: 0x1400B4E50.

// [7] 0x1400b4e50
void PastureUI::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe (reliable via the one-frame removal deferral, see
    // entitylist.cpp Step 0).
    if (!SA_DialogEntityAlive(this)) return;
    // Get entity → find PastureComponent
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = ::Object::IsDerivedFromNullSafe(dataSource, ComponentEntity::TypeId());
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[16];
    new (entityBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(entity));   // B.3.3 bridge

    uint8_t compBuf[16];
    fn_FindComponent_Pasture(*(void**)entityBuf, compBuf);
    uintptr_t comp = *(uintptr_t*)compBuf;
    // SA guard (retail unguarded): entity without a PastureComponent, or the
    // combo/slider elements not seated yet (Slot4 hasn't run) — bail.
    if (!comp) return;
    if (!*(void**)(s + 0x30) || !*(void**)(s + 0x38)) return;

    // Enable combo only if it has items AND pasture has no animals yet
    void* comboElem = *(void**)(s + 0x30);
    int itemCount = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetItemCount();  // B.3.6 bridge
    bool enableCombo = (itemCount > 0) && (*(int32_t*)(comp + 0x38) == 0);

    // Set combo enabled/disabled via vtable[8]
    reinterpret_cast<UI::Element*>(comboElem)->Enable(enableCombo);  // B.3.6 bridge: Element slot 8 Enable

    // Format current animal count → slider param 0
    void* sliderElem = *(void**)(s + 0x38);
    {
        System::String fmtStr = System::String::Format("%i", *(uint32_t*)(comp + 0x38));
        reinterpret_cast<UI::SliderElement*>(sliderElem)->SetParameter(0, fmtStr);  // B.3.6 bridge
    }

    // Format max animal count → slider param 1
    {
        System::String fmtStr = System::String::Format("%i", *(uint32_t*)(comp + 0x40));
        reinterpret_cast<UI::SliderElement*>(sliderElem)->SetParameter(1, fmtStr);  // B.3.6 bridge
    }
}

// ============================================================
// PastureUI::Slot11 (vtable +0x58) — Set resource type
// ============================================================
// Sets the selected animal/resource type on the PastureComponent.
// If changing from no resource (0) to a valid resource, calls
// vtable[14](comp, 2) to initialize pasture. Then recalculates
// max count and updates the slider display.
//
// Original: 0x1400B4C40.

// [11] 0x1400b4c40
void PastureUI::SetResource(int64_t param2) {
    uintptr_t s = (uintptr_t)this;

    // Bail on a dead / marked-for-removal datasource before probing it (see Slot7).
    if (!SA_DialogEntityAlive(this)) return;
    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = ComponentEntity::TypeId();
    void* checkedEntity = nullptr;
    if (::Object::IsDerivedFromNullSafe(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

    // Find PastureComponent on entity
    uint64_t pastureComp = 0;
    fn_FindComponent_Pasture((void*)entityBasePtr, &pastureComp);
    if (!pastureComp) return;   // SA guard (retail unguarded)

    // If currently no resource set (comp+0x28 == 0) and new resource is valid,
    // set resource first then call initialization via vtable[14](comp, 2)
    int64_t currentResource = *(int64_t*)(pastureComp + 0x28);
    if (currentResource == 0 && param2 != 0) {
        *(int64_t*)(pastureComp + 0x28) = param2;
        reinterpret_cast<Component*>(pastureComp)->Command(2);   // B.3.x bridge (slot 14)
    }

    // Set the resource type
    *(int64_t*)(pastureComp + 0x28) = param2;

    // Recalculate max animal count from pasture dimensions
    uint32_t maxCount = PastureCalcMaxCount_Impl(pastureComp);
    *(int32_t*)(pastureComp + 0x40) = (int32_t)maxCount;

    // Update slider display
    PastureUI_UpdateSlider_Impl(this);
}

// ============================================================
// PastureUI::Slot12 (vtable +0x60) — Get resource type
// ============================================================
// Returns the current animal/resource type from the PastureComponent
// at comp+0x28 via the output parameter.
//
// Original: 0x1400B4D00.

// [12] 0x1400b4d00
void* PastureUI::GetResource(void* param2) {
    uintptr_t s = (uintptr_t)this;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = ComponentEntity::TypeId();
    void* checkedEntity = nullptr;
    if (::Object::IsDerivedFromNullSafe(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    new (&entityBasePtr) ::Object::InternalBasePtr(
        reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

    // Find PastureComponent on entity
    uint64_t pastureComp = 0;
    fn_FindComponent_Pasture((void*)entityBasePtr, &pastureComp);
    if (!pastureComp) { *(uint64_t*)param2 = 0; return param2; }  // SA guard

    // Return resource type at comp+0x28
    *(uint64_t*)param2 = *(uint64_t*)(pastureComp + 0x28);
    return param2;
}

// ============================================================
// PastureUI::Slot9 (vtable +0x48) — Combo/slider event handler
// ============================================================
// Handles two event types:
//   0x14/500: Combo selection — dispatches resource type via Slot11.
//   0x17/501: Slider change — updates max animal count on PastureComponent.
//
// Original: 0x1400B4E20.

// [9] 0x1400b4e20
void PastureUI::OnElementEvent(Object::InternalPtr<UI::Element>* /*element*/, int32_t param3, int32_t param4,
                               int32_t /*c*/, int32_t /*d*/, int32_t /*e*/, void* /*userData*/) {
    pastureui_sa::Trace("Slot9(event)", this);
    uintptr_t s = (uintptr_t)this;

    if (param3 == 0x14 && param4 == 500) {
        // Combo selection: get selected resource, call Slot11 via vtable
        void* comboElem = *(void**)(s + 0x30);
        int currentItem = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetCurrentItem();  // B.3.6 bridge
        void* itemData = reinterpret_cast<const UI::ComboElement*>(comboElem)->GetItemData(currentItem);  // B.3.6 bridge
        int resourceIndex = (int)(intptr_t)itemData;

        void* world = *g_pGameWorldSingleton;
        uintptr_t entityList = *(uintptr_t*)((uintptr_t)world + 0xf0);
        uintptr_t resourceArray = *(uintptr_t*)(entityList + 0x120);
        int64_t resourceType = *(int64_t*)(resourceArray + (int64_t)resourceIndex * 8);

        SetResource(resourceType);   // slot 11, virtual (as retail)
    }
    else if (param3 == 0x17 && param4 == 501) {
        // Slider change: read slider value, update max count on PastureComponent
        void* entity = *(void**)(s + 0x28);
        bool isEntity = ::Object::IsDerivedFromNullSafe(entity, ComponentEntity::TypeId());
        void* checkedEntity = isEntity ? entity : nullptr;
        uint64_t entityBasePtr = 0;
        new (&entityBasePtr) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(checkedEntity));   // B.3.3 bridge

        uint64_t pastureComp = 0;
        fn_FindComponent_Pasture((void*)entityBasePtr, &pastureComp);

        void* sliderElem = *(void**)(s + 0x38);
        float sliderValue = *(float*)((uintptr_t)sliderElem + 0x128);
        *(int32_t*)(pastureComp + 0x40) = (int32_t)sliderValue;
    }
}

// ============================================================
// Slot13 (vtable +0x68): GetCategoryFilter — returns 2 (pasture category)
// Original: 0x1400B4CF0 (6 bytes, MOV EAX,2; RET, unhookable)
// ============================================================
// [13] 0x1400b4cf0
int PastureUI::GetCategory() {
    return 2;
}

// ============================================================
// STANDALONE promotion — real PastureUI controller vtable
// ============================================================
// PastureUI was a UI_DIALOG_STUB (all-no-op) so every port above was dead code
// and the pasture panel's type combo / animal slider stayed empty — the
// BANISHED_PASTURE_AUTOTYPE stand-in existed only because Slot11 was
// unreachable. Promotion follows the StorageUI/ResidenceUI pattern (stub
// registration removed from ui_dialog_stub_factories.cpp).
//
// Retail vtable @ 0x14015f610 (14 slots):
//   idx4  (+0x20) 0x1400b5090 Slot4  — setup: base combo find/populate + slider
//   idx7  (+0x38) 0x1400b4e50 Slot7  — per-frame: combo enable + slider text
//   idx8  (+0x40) 0x140078d30        — SelectResourceUI::Slot8 (combo refresh;
//                                      the slot the framework ticks per frame)
//   idx9  (+0x48) 0x1400b4e20 Slot9  — events: combo 0x14/500, slider 0x17/501
//   idx11 (+0x58) 0x1400b4c40 Slot11 — SetLivestockType (drives Pasture Slot14 cmd 2)
//   idx12 (+0x60) 0x1400b4d00 Slot12 — get current type
//   idx13 (+0x68) 0x1400b4cf0 Slot13 — category filter = 2 (livestock)




// [8] STANDALONE DEVIATION (retail inherits the base slot) — the tree's wiring, kept.
void PastureUI::OnUpdate(const System::Time& time) {
    uintptr_t s = reinterpret_cast<uintptr_t>(this);
    if (!SA_DialogEntityAlive(this)) return;
    if (!*reinterpret_cast<void**>(s + 0x38)) {
        PastureUI::OnCreated();
        if (!*reinterpret_cast<void**>(s + 0x38)) return;   // still no slider — dialog lacks it
    }
    if (*reinterpret_cast<void**>(s + 0x30))
        SelectResourceUI::OnUpdate(time);
}





// create 0x140020e10: MemAlloc(0x40, 8), then the ctor (DialogController ->
// Internal: AddObject, the compiler's stamps, the field initialisers).
void* PastureUI::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(PastureUI), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(new (p) PastureUI());
}
// Retail's createInPlace does not build a controller in place (B8-13); the
// tree's in-place entry did, and that is kept.
void* PastureUI::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(new (storage) PastureUI());
}

DEFINE_EXE_TYPE(PastureUI, "SelectResourceUI")
