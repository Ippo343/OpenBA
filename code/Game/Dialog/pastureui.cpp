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
    fn_PastureUI_UpdateSlider(self);
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
    uint32_t maxCount = fn_PastureCalcMaxCount((void*)pastureComp);
    *(int32_t*)(pastureComp + 0x40) = (int32_t)maxCount;

    // Update slider display
    fn_PastureUI_UpdateSlider(self);
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
