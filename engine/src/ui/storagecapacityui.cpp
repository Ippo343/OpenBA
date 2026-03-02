// engine/src/ui/storagecapacityui.cpp
// Clean reimplementation of StorageCapacityUI game logic.
//
// StorageCapacityUI (0x38 bytes) — storage capacity progress bar.
// Extends DialogController. data30 = progress element InternalPtr.
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// StorageCapacityUI::Serialize (vtable +0x20) — Find progress element
// ============================================================
// Finds the ProgressElement in the dialog by type ID and stores it
// at self+0x30. Sets the progress element's max scale to 1.0.
//
// Original: 0x14007b600.

void StorageCapacityUI_Serialize(StorageCapacityUI* self) {
    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find ProgressElement by type
    uint32_t typeId = g_TypeId_ProgressElement ? *g_TypeId_ProgressElement : 0;
    uint64_t findResult = 0;
    fn_Element_FindElementType(dialogElem, &findResult, typeId);

    // Dereference InternalPtr to get raw Element*, type check, wrap in InternalBasePtr
    void* rawElem = (void*)findResult;
    bool valid = fn_IsDerivedFrom(rawElem, typeId);
    void* checkedElem = valid ? rawElem : nullptr;

    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, checkedElem);
    self->data30 = *(uint64_t*)basePtrBuf;

    // Set progress max scale at element +0x124 to 1.0f
    void* progElem = (void*)self->data30;
    *(float*)((uintptr_t)progElem + 0x124) = 1.0f;
}

// ============================================================
// StorageCapacityUI::Startup (vtable +0x38) — Update progress display
// ============================================================
// Queries the entity's storage component for current count and capacity,
// calculates the fill ratio, and updates the progress bar with a
// percentage string.
//
// Original: 0x14007b4d0.

void StorageCapacityUI_Startup(StorageCapacityUI* self) {
    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find StorageComponent via FUN_140006270
    void* component = nullptr;
    fn_FindComponent_Storage(entityInternal, &component);

    // Query current count
    int count = fn_StorageComponent_QueryCount(component);

    // Get capacity from component +0x38
    int capacity = *(int*)((uintptr_t)component + 0x38);

    // Calculate fill ratio
    float ratio;
    if (capacity == 0) {
        ratio = 0.0f;
    } else {
        ratio = (float)count / (float)capacity;
    }

    // Calculate display percentage: (int)(ratio * 100.0 + 0.5)
    float displayVal = ratio * 100.0f + 0.5f;

    // Store ratio at progress element +0x128
    void* progElem = (void*)self->data30;
    *(float*)((uintptr_t)progElem + 0x128) = ratio;

    // Format percentage string
    uint8_t strBuf[8];
    fn_String_Format(strBuf, "%i", (uint64_t)(uint32_t)(int)displayVal);

    // Set on progress element
    fn_ProgressElement_SetProgressTextParameter(progElem, 0, strBuf);
    fn_String_Dtor(strBuf);
}
