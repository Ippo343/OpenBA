// engine/src/ui/pagetoggleui.cpp
// Clean reimplementation of PageToggleUI panel methods.
//
// PageToggleUI (0x48 bytes): A checkbox that toggles between page 0 and page 4.
// Layout: +0x30 = checkbox element (InternalBasePtr), +0x38 = "pages" PageElement,
//         +0x40 = saved page index (int32)

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot4: Find checkbox by type, find "pages" element, restore saved page.
void PageToggleUI_Slot4(PageToggleUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Find checkbox element by type ID in the dialog element tree
    uint32_t checkboxTypeId = g_TypeId_CheckboxElement ? *g_TypeId_CheckboxElement : 0;
    uint64_t foundElem = 0;
    fn_Element_FindElementType((void*)*(uint64_t*)(s + 0x20), &foundElem, checkboxTypeId);

    // IsDerivedFrom check + store at +0x30
    void* elemPtr = (void*)foundElem;
    void* checked = nullptr;
    if (elemPtr) {
        bool derived = fn_IsDerivedFrom(elemPtr, checkboxTypeId);
        if (derived) checked = elemPtr;
    }
    uint64_t basePtr = 0;
    fn_InternalBasePtr_Ctor(&basePtr, checked);
    *(uint64_t*)(s + 0x30) = basePtr;

    // SetElementId on checkbox = 700
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x30), 700);

    // Find "pages" element in dialog instance
    char nameBuf[8] = {};
    fn_String_Ctor_CStr(nameBuf, "pages");
    uint64_t pagesElem = 0;
    fn_DialogInstance_FindElement((void*)*(uint64_t*)(s + 0x18), &pagesElem, nameBuf);
    *(uint64_t*)(s + 0x38) = pagesElem;
    fn_String_Dtor(nameBuf);

    // If active page is 0 or 4, restore saved page; otherwise reset saved page to 0
    int activePage = fn_PageElement_GetActivePage((void*)*(uint64_t*)(s + 0x38));
    if (activePage != 0) {
        activePage = fn_PageElement_GetActivePage((void*)*(uint64_t*)(s + 0x38));
        if (activePage != 4) {
            *(uint32_t*)(s + 0x40) = 0;
            return;
        }
    }
    fn_PageElement_SetActivePage((void*)*(uint64_t*)(s + 0x38), *(int*)(s + 0x40));
}

// Slot7: Enable/disable checkbox based on whether active page is 0 or 4.
void PageToggleUI_Slot7(PageToggleUI* self) {
    uintptr_t s = (uintptr_t)self;

    void* checkbox = (void*)*(uint64_t*)(s + 0x30);
    void** vtable = *(void***)checkbox;

    int activePage = fn_PageElement_GetActivePage((void*)*(uint64_t*)(s + 0x38));
    bool enabled;
    if (activePage != 0) {
        activePage = fn_PageElement_GetActivePage((void*)*(uint64_t*)(s + 0x38));
        enabled = (activePage == 4);
    } else {
        enabled = true;
    }

    // vtable[8] = SetEnabled(this, bool)
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable[8])(checkbox, enabled);
}

// Slot9: On checkbox toggle event, switch between page 0 and page 4.
void PageToggleUI_Slot9(PageToggleUI* self, void* param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 0x12 && param4 == 700) {
        // Read checkbox state at +0xd8 (bool: checked or not)
        void* checkbox = (void*)*(uint64_t*)(s + 0x30);
        char checkState = *(char*)((uintptr_t)checkbox + 0xd8);

        if (checkState == 0) {
            *(uint32_t*)(s + 0x40) = 0;
        } else {
            *(uint32_t*)(s + 0x40) = 4;
        }

        fn_PageElement_SetActivePage((void*)*(uint64_t*)(s + 0x38), *(int*)(s + 0x40));
    }
}

// ============================================================
