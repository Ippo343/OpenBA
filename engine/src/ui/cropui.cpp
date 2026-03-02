// engine/src/ui/cropui.cpp
// Clean reimplementation of CropUI game logic.
//
// CropUI (0x60 bytes) — crop selection panel.
// Extends SelectResourceUI (abstract) > DialogController.
// data30 (internalBasePtr) = selectResource combo element.
//
// Decompiled from Application-x64.exe.

#include "ui/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// CropUI::Slot4 (vtable +0x20) — Setup/refresh
// ============================================================
// Finds the "selectResource" combo element in the dialog,
// wraps it as InternalBasePtr and stores at +0x30.
// Sets element ID 500, then calls the populate combo helper
// to fill it with available crop types.
//
// Original: 0x140078BE0.

void CropUI_Slot4(CropUI* self) {
    // Get dialog instance from DialogController base
    void* dialogInst = *(void**)((uintptr_t)self + 0x18);

    // Find "selectResource" element
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "selectResource");

    uint8_t elemBuf[8] = {0};
    fn_DialogInstance_FindElement(dialogInst, elemBuf, nameBuf);
    void* foundElem = *(void**)elemBuf;
    fn_String_Dtor(nameBuf);

    // Wrap in InternalBasePtr, store at +0x30
    uint8_t basePtrBuf[8];
    fn_InternalBasePtr_Ctor(basePtrBuf, foundElem);
    self->internalBasePtr = *(uint64_t*)basePtrBuf;

    // Set element ID 500
    fn_Element_SetElementId((void*)self->internalBasePtr, 500);

    // Populate the combo with available crop types
    fn_CropUI_PopulateCombo(self);
}

// ============================================================
// CropUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Enables or disables the selectResource combo based on whether
// it has any items. Uses virtual call at vtable+0x40 (SetEnabled).
//
// Original: 0x140078CB0.

void CropUI_Slot7(CropUI* self) {
    // Wrap entity in InternalBasePtr (base class requirement, not used directly)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);

    // Get selectResource combo element
    void* combo = (void*)self->internalBasePtr;

    // Enable combo if it has items, disable if empty
    int itemCount = fn_ComboElement_GetItemCount(combo);

    // Virtual call: vtable[8] = SetEnabled(hasItems)
    typedef void (*fn_VSlot8_t)(void* self, bool param);
    void** vtable = *(void***)combo;
    fn_VSlot8_t vfunc = (fn_VSlot8_t)vtable[8];
    vfunc(combo, itemCount > 0);
}

// ============================================================
// CropUI::Slot8 (vtable +0x40) — Refresh check
// ============================================================
// Compares current combo item count with expected count.
// If they differ, repopulates the combo with available crop types.
//
// Original: 0x140078D30.

void CropUI_Slot8(CropUI* self) {
    void* combo = (void*)self->internalBasePtr;

    int currentCount = fn_ComboElement_GetItemCount(combo);
    int expectedCount = fn_CropUI_GetExpectedCount(self);

    if (currentCount != expectedCount) {
        fn_CropUI_PopulateCombo(self);
    }
}
