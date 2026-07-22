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
#include <cstdint>

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
    fn_InternalBasePtr_CheckPtr((void*)(pastureComp + 0x10));
    uintptr_t entity = *(uintptr_t*)(pastureComp + 0x10);

    uint64_t buildCompRet = 0;
    fn_FindComponent_Build((void*)entity, &buildCompRet);
    fn_InternalBasePtr_CheckPtr((void*)&buildCompRet);
    uintptr_t buildComp = buildCompRet;

    int16_t maxOcc = buildComp ? *(int16_t*)(buildComp + 0x2c) : 0;
    int16_t curOcc = buildComp ? *(int16_t*)(buildComp + 0x2e) : 0;

    // Search animal description entity's components for AnimalDescription
    uintptr_t animalEntity = *(uintptr_t*)(pastureComp + 0x28);
    fn_InternalBasePtr_CheckPtr((void*)&animalEntity);

    int compCount = *(int*)(animalEntity + 0x30);
    uintptr_t* compArray = *(uintptr_t**)(animalEntity + 0x28);
    uint32_t animalDescTypeId = g_TypeId_AnimalDescription ? *g_TypeId_AnimalDescription : 0;

    void* animalDesc = nullptr;
    for (int i = 0; i < compCount; i++) {
        uintptr_t compIPtr = compArray[i];
        fn_InternalBasePtr_CheckPtr((void*)&compIPtr);
        void* comp = (void*)compIPtr;
        if (!comp) continue;
        typedef int (*GetTypeId_t)(void*);
        int typeId = ((GetTypeId_t)(*(uintptr_t**)comp)[2])(comp);
        if (typeId == (int)animalDescTypeId) {
            bool isAnimalDesc = fn_IsDerivedFrom(comp, animalDescTypeId);
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
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint64_t entityIPtr = 0;
    fn_InternalBasePtr_Ctor(&entityIPtr, entityPtr);
    fn_InternalBasePtr_CheckPtr(&entityIPtr);
    uintptr_t entityVal = entityIPtr;

    // Find PastureComponent
    uint64_t pastureRet = 0;
    fn_FindComponent_Pasture((void*)entityVal, &pastureRet);
    fn_InternalBasePtr_CheckPtr(&pastureRet);
    uintptr_t pastureComp = pastureRet;
    if (!pastureComp) return;

    // Get slider element at self+0x38
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
    uintptr_t slider = *(uintptr_t*)(s + 0x38);
    if (!slider) return;

    // Set slider min to 0
    *(float*)(slider + 0x118) = 0.0f;

    // Set slider max = PastureCalcMaxCount
    uint32_t maxCount = PastureCalcMaxCount_Impl(pastureComp);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
    slider = *(uintptr_t*)(s + 0x38);
    *(float*)(slider + 0x11c) = (float)(int)maxCount;

    // Set slider current = current animal count at pastureComp+0x40
    fn_InternalBasePtr_CheckPtr(&pastureRet);
    pastureComp = pastureRet;
    int currentCount = *(int*)(pastureComp + 0x40);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
    slider = *(uintptr_t*)(s + 0x38);
    *(float*)(slider + 0x128) = (float)currentCount;
}

// ============================================================
// PastureUI::Slot4 (vtable +0x20) — Initialize combo + slider elements
// ============================================================
// Calls base SelectResourceUI::Slot4 (find combo, populate),
// then finds "slider" element, stores at +0x38, sets ID 501,
// and updates slider display.
//
// Original: 0x1400B5090.

void PastureUI_Slot4(PastureUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Initialize combo via base class
    SelectResourceUI_Slot4(self);

    // Find "slider" element in parent element at +0x20
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "slider");

    void* parentElem = *(void**)(s + 0x20);
    uint64_t findResult = 0;
    fn_FindElement(parentElem, &findResult, nameBuf);
    fn_String_Dtor(nameBuf);

    // Store slider element at +0x38 (skip IsDerivedFrom, null check only)
    void* rawElem = (void*)findResult;
    void* checked = rawElem ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    fn_InternalBasePtr_Ctor(&basePtrVal, checked);
    *(uint64_t*)(s + 0x38) = basePtrVal;

    // Set slider element ID to 501 (0x1f5)
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x38), 0x1f5);

    // Update slider display with current/max animal counts
    PastureUI_UpdateSlider_Impl(self);
}

// ============================================================
// PastureUI::Slot7 (vtable +0x38) — Update slider text + combo enable
// ============================================================
// Enables/disables combo based on item count and current animal count.
// Updates slider text parameters with current and max animal counts.
//
// Original: 0x1400B4E50.

void PastureUI_Slot7(PastureUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Per-frame (+0x38): bail on a dead / marked-for-removal datasource before the
    // IsDerivedFrom probe (reliable via the one-frame removal deferral, see
    // entitylist.cpp Step 0).
    if (!SA_DialogEntityAlive(self)) return;
    // Get entity → find PastureComponent
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    uint8_t entityBuf[16];
    fn_InternalBasePtr_Ctor(entityBuf, entity);

    uint8_t compBuf[16];
    fn_FindComponent_Pasture(*(void**)entityBuf, compBuf);
    uintptr_t comp = *(uintptr_t*)compBuf;

    // Enable combo only if it has items AND pasture has no animals yet
    void* comboElem = *(void**)(s + 0x30);
    int itemCount = fn_ComboElement_GetItemCount(comboElem);
    bool enableCombo = (itemCount > 0) && (*(int32_t*)(comp + 0x38) == 0);

    // Set combo enabled/disabled via vtable[8]
    typedef void (*fn_SetEnabled_t)(void* self, bool enabled);
    void** vtable = *(void***)comboElem;
    ((fn_SetEnabled_t)vtable[8])(comboElem, enableCombo);

    // Format current animal count → slider param 0
    uint8_t fmtStr[8];
    void* sliderElem = *(void**)(s + 0x38);
    fn_String_Format(fmtStr, "%i", *(uint32_t*)(comp + 0x38));
    fn_SliderElement_SetParameter(sliderElem, 0, fmtStr);
    fn_String_Dtor(fmtStr);

    // Format max animal count → slider param 1
    fn_String_Format(fmtStr, "%i", *(uint32_t*)(comp + 0x40));
    fn_SliderElement_SetParameter(sliderElem, 1, fmtStr);
    fn_String_Dtor(fmtStr);
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

void PastureUI_Slot11(PastureUI* self, int64_t param2) {
    uintptr_t s = (uintptr_t)self;

    // Bail on a dead / marked-for-removal datasource before probing it (see Slot7).
    if (!SA_DialogEntityAlive(self)) return;
    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find PastureComponent on entity
    uint64_t pastureComp = 0;
    fn_FindComponent_Pasture((void*)entityBasePtr, &pastureComp);

    // If currently no resource set (comp+0x28 == 0) and new resource is valid,
    // set resource first then call initialization via vtable[14](comp, 2)
    int64_t currentResource = *(int64_t*)(pastureComp + 0x28);
    if (currentResource == 0 && param2 != 0) {
        *(int64_t*)(pastureComp + 0x28) = param2;
        void** vtable = *(void***)(void*)pastureComp;
        typedef void (*fn_InitProduction_t)(void*, int);
        ((fn_InitProduction_t)vtable[14])((void*)pastureComp, 2);
    }

    // Set the resource type
    *(int64_t*)(pastureComp + 0x28) = param2;

    // Recalculate max animal count from pasture dimensions
    uint32_t maxCount = PastureCalcMaxCount_Impl(pastureComp);
    *(int32_t*)(pastureComp + 0x40) = (int32_t)maxCount;

    // Update slider display
    PastureUI_UpdateSlider_Impl(self);
}

// ============================================================
// PastureUI::Slot12 (vtable +0x60) — Get resource type
// ============================================================
// Returns the current animal/resource type from the PastureComponent
// at comp+0x28 via the output parameter.
//
// Original: 0x1400B4D00.

void* PastureUI_Slot12(PastureUI* self, void* param2) {
    uintptr_t s = (uintptr_t)self;

    // Get entity at +0x28, check ComponentEntity
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* checkedEntity = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        checkedEntity = entity;
    }
    uint64_t entityBasePtr = 0;
    fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

    // Find PastureComponent on entity
    uint64_t pastureComp = 0;
    fn_FindComponent_Pasture((void*)entityBasePtr, &pastureComp);

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

void PastureUI_Slot9(PastureUI* self, void* param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 0x14 && param4 == 500) {
        // Combo selection: get selected resource, call Slot11 via vtable
        void* comboElem = *(void**)(s + 0x30);
        int currentItem = fn_ComboElement_GetCurrentItem(comboElem);
        void* itemData = fn_ComboElement_GetItemData(comboElem, currentItem);
        int resourceIndex = (int)(intptr_t)itemData;

        void* world = *g_pGameWorldSingleton;
        uintptr_t entityList = *(uintptr_t*)((uintptr_t)world + 0xf0);
        uintptr_t resourceArray = *(uintptr_t*)(entityList + 0x120);
        int64_t resourceType = *(int64_t*)(resourceArray + (int64_t)resourceIndex * 8);

        typedef void (*fn_Slot11_t)(void* self, int64_t resourceType);
        void** vtable = *(void***)self;
        ((fn_Slot11_t)vtable[11])(self, resourceType);
    }
    else if (param3 == 0x17 && param4 == 501) {
        // Slider change: read slider value, update max count on PastureComponent
        void* entity = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
        void* checkedEntity = isEntity ? entity : nullptr;
        uint64_t entityBasePtr = 0;
        fn_InternalBasePtr_Ctor(&entityBasePtr, checkedEntity);

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
int PastureUI_Slot13(PastureUI* self) {
    (void)self;
    return 2;
}
