// engine/src/ui/workplaceui.cpp
// Clean reimplementation of WorkplaceUI game logic.
//
// WorkplaceUI (0x60 bytes) — workplace worker count panel.
// Extends DialogController. Fields:
//   +0x30 workPlaceCount, +0x38 maxCount (InternalBasePtr)
//   +0x40 workerCountUp, +0x48 workerCountDown (InternalBasePtr)
//   +0x50 jobCountUp, +0x58 jobCountDown (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Helper: Find element by name in dialog, return InternalPtr value (8 bytes)
static uint64_t FindElemByName(void* dialogElem, const char* name) {
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, name);

    uint64_t result = 0;
    fn_FindElement(&result, dialogElem, nameBuf);

    fn_String_Dtor(nameBuf);
    return result;
}

// Helper: Find element by name, dereference, wrap in InternalBasePtr
static uint64_t FindElemAsBasePtr(void* dialogElem, const char* name) {
    uint64_t intPtr = FindElemByName(dialogElem, name);
    void* rawElem = (void*)intPtr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, rawElem);
    return *(uint64_t*)basePtrBuf;
}

// ============================================================
// WorkplaceUI::Serialize (vtable +0x20) — Setup/refresh
// ============================================================
// Finds 6 named elements in the dialog, stores them, sets
// element IDs on the 4 buttons plus the workPlaceButton.
// Checks entity component to conditionally hide job count buttons.
//
// Original: 0x140087940.

void WorkplaceUI_Serialize(WorkPlaceUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // --- Find and store named elements ---
    self->data30 = FindElemAsBasePtr(dialogElem, "workPlaceCount");
    self->data38 = FindElemAsBasePtr(dialogElem, "maxCount");
    self->data40 = FindElemAsBasePtr(dialogElem, "workerCountUp");
    self->data48 = FindElemAsBasePtr(dialogElem, "workerCountDown");
    self->data50 = FindElemAsBasePtr(dialogElem, "jobCountUp");
    self->data58 = FindElemAsBasePtr(dialogElem, "jobCountDown");

    // --- Set element IDs on buttons ---
    fn_Element_SetElementId((void*)self->data40, 0x1450);
    fn_Element_SetElementId((void*)self->data48, 0x1451);
    fn_Element_SetElementId((void*)self->data50, 0x1452);
    fn_Element_SetElementId((void*)self->data58, 0x1453);

    // Find and set workPlaceButton element ID
    uint64_t workplaceBtn = FindElemAsBasePtr(dialogElem, "workPlaceButton");
    fn_Element_SetElementId((void*)workplaceBtn, 0x1454);

    // --- Check entity component for job count visibility ---
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find WorkplaceComponent
    void* workComp = nullptr;
    fn_FindComponent_Workplace(entityInternal, &workComp);

    // Check sub-component at +0x18 for type DAT_1401c80a8
    void* subComp = *(void**)((uintptr_t)workComp + 0x18);
    bool isJobType = fn_IsDerivedFrom(subComp, *g_TypeId_WorkplaceJobType);

    if (isJobType && *(uint8_t*)((uintptr_t)subComp + 0x20) == 0) {
        // Hide job count up/down buttons if sub-component flag is zero
        fn_Element_SetFlagsOnOff((void*)self->data50, 1, 0);
        fn_Element_SetFlagsOnOff((void*)self->data58, 1, 0);
    }
}
