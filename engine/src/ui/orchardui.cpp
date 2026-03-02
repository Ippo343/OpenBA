// engine/src/ui/orchardui.cpp
// Clean reimplementation of OrchardUI game logic.
//
// OrchardUI (0x48 bytes) — orchard crop selection and growth display.
// Extends SelectResourceUI > DialogController.
// +0x30 = combo element (SelectResourceUI base), +0x38 = yield progress element.
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// OrchardUI::Slot7 (vtable +0x38) — Update growth progress
// ============================================================
// Gets the OrchardComponent from the entity, queries growth progress
// as a float ratio, updates the yield progress bar with percentage text,
// then enables/disables the combo based on item count.
//
// Original: 0x1400754B0.

void OrchardUI_Slot7(OrchardUI* self) {
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

    // Find OrchardComponent on entity
    uint64_t orchardComp = 0;
    fn_FindComponent_Orchard((void*)entityBasePtr, &orchardComp);

    // Get average growth ratio (0.0 - 1.0)
    float growth = fn_OrchardGetGrowth((void*)orchardComp);

    // Set progress bar value directly at element+0x128
    void* yieldElem = (void*)*(uint64_t*)(s + 0x38);
    *(float*)((uintptr_t)yieldElem + 0x128) = growth;

    // Format percentage text: (int)(growth * 100.0f + 0.5f)
    int percent = (int)(growth * 100.0f + 0.5f);
    uint8_t textBuf[16];
    fn_String_Format((char*)textBuf, "%i", (uint64_t)(uint32_t)percent);

    // Set progress text parameter
    fn_ProgressElement_SetProgressTextParameter(yieldElem, 0, textBuf);
    fn_String_Dtor(textBuf);

    // Enable/disable combo based on item count (SelectResourceUI base logic)
    void* combo = (void*)*(uint64_t*)(s + 0x30);
    int itemCount = fn_ComboElement_GetItemCount(combo);
    void** vtable = *(void***)combo;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable[8])(combo, itemCount > 0);
}

// ============================================================
// OrchardUI::Slot11 (vtable +0x58) — Set resource type
// ============================================================
// Sets the selected crop/resource type on the OrchardComponent.
// If changing from no resource (0) to a valid resource, calls
// vtable[14](comp, 2) to initialize production.
//
// Original: 0x140075380.

void OrchardUI_Slot11(OrchardUI* self, int64_t param2) {
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

    // Find OrchardComponent on entity
    uint64_t orchardComp = 0;
    fn_FindComponent_Orchard((void*)entityBasePtr, &orchardComp);

    // If currently no resource set (comp+0x60 == 0) and new resource is valid,
    // call initialization via vtable[14](comp, 2)
    int64_t currentResource = *(int64_t*)(orchardComp + 0x60);
    if (currentResource == 0 && param2 != 0) {
        void** vtable = *(void***)(void*)orchardComp;
        typedef void (*fn_InitProduction_t)(void*, int);
        ((fn_InitProduction_t)vtable[14])((void*)orchardComp, 2);
    }

    // Set the resource type
    *(int64_t*)(orchardComp + 0x60) = param2;
}

// ============================================================
// OrchardUI::Slot12 (vtable +0x60) — Get resource type
// ============================================================
// Returns the current crop/resource type from the OrchardComponent
// at comp+0x60 via the output parameter.
//
// Original: 0x140075430.

void* OrchardUI_Slot12(OrchardUI* self, void* param2) {
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

    // Find OrchardComponent on entity
    uint64_t orchardComp = 0;
    fn_FindComponent_Orchard((void*)entityBasePtr, &orchardComp);

    // Return resource type at comp+0x60
    *(uint64_t*)param2 = *(uint64_t*)(orchardComp + 0x60);
    return param2;
}
