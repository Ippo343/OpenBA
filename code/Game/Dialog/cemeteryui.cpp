// engine/src/ui/cemeteryui.cpp
// Clean reimplementation of CemeteryUI game logic.
//
// CemeteryUI (0x38 bytes) — cemetery progress bar panel.
// Extends DialogController. Fields:
//   +0x30 progressElement (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// CemeteryUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds ProgressElement by type, stores at +0x30.
// Gets CemeteryComponent, reads max slots from +0x50,
// sets as progress maximum.
//
// Original: 0x14005F090.

void CemeteryUI_Slot4(CemeteryUI* self) {
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find ProgressElement by type
    uint64_t result = 0;
    fn_Element_FindElementType(dialogElem, &result, *g_TypeId_ProgressElement);
    void* foundElem = (void*)result;

    // Type-check and wrap as InternalBasePtr
    bool isProgress = fn_IsDerivedFrom(foundElem, *g_TypeId_ProgressElement);
    void* progressPtr = isProgress ? foundElem : nullptr;
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, progressPtr);
    self->data30 = *(uint64_t*)basePtrBuf;

    // Get entity, find CemeteryComponent
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    void* cemetComp = nullptr;
    fn_FindComponent_Cemetery(entityInternal, &cemetComp);

    // Set progress maximum from component's max slots field (+0x50)
    int maxSlots = *(int*)((uintptr_t)cemetComp + 0x50);
    void* progress = (void*)self->data30;
    *(float*)((uintptr_t)progress + 0x124) = (float)maxSlots;
}

// ============================================================
// CemeteryUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Gets burial count and max slots from CemeteryComponent,
// updates progress bar value and two text parameters.
//
// Original: 0x14005EF50.

void CemeteryUI_Slot7(CemeteryUI* self) {
    // Get entity, find CemeteryComponent
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    void* cemetComp = nullptr;
    fn_FindComponent_Cemetery(entityInternal, &cemetComp);

    void* progress = (void*)self->data30;

    // Get burial count and set as progress current value
    int burialCount = fn_CemeteryGetBurialCount(cemetComp);
    *(float*)((uintptr_t)progress + 0x128) = (float)burialCount;

    // Set text param 0: burial count
    uint8_t strBuf[8];
    fn_String_Format(strBuf, "%d", (unsigned int)burialCount);
    fn_ProgressElement_SetProgressTextParameter(progress, 0, strBuf);
    fn_String_Dtor(strBuf);

    // Set text param 1: max slots
    int maxSlots = *(int*)((uintptr_t)cemetComp + 0x50);
    fn_String_Format(strBuf, "%d", (unsigned int)maxSlots);
    fn_ProgressElement_SetProgressTextParameter(progress, 1, strBuf);
    fn_String_Dtor(strBuf);
}
