// engine/src/ui/pastureui.cpp
// Clean reimplementation of PastureUI game logic.
//
// PastureUI (0x40 bytes) — pasture animal selection panel.
// Extends SelectResourceUI > DialogController.
// +0x30 = combo element (SelectResourceUI base), +0x38 = slider element.
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

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
