// engine/src/ui/controlui.cpp
// Clean reimplementation of ControlUI panel methods.
//
// ControlUI (0x40 bytes): Building enable/disable toggle panel.
// Layout: +0x30 = checkbox element (InternalBasePtr)
//         +0x38 = sub-component (InternalBasePtr) found via component array search

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot4: Find checkbox element, find sub-component by type, set initial state.
void ControlUI_Slot4(ControlUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Check datasource component type
    void* datasource = (void*)*(uint64_t*)(s + 0x10);
    uint32_t controlTypeId = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
    void* component = nullptr;
    if (fn_IsDerivedFrom(datasource, controlTypeId)) {
        component = datasource;
    }

    // Check dialog element type, store at +0x30
    uint32_t elemBaseTypeId = g_TypeId_UIElementBase ? *g_TypeId_UIElementBase : 0;
    void* dialogElem = (void*)*(uint64_t*)(s + 0x20);
    void* checkedElem = nullptr;
    if (fn_IsDerivedFrom(dialogElem, elemBaseTypeId)) {
        checkedElem = dialogElem;
    }
    uint64_t stored = 0;
    fn_InternalBasePtr_Ctor(&stored, checkedElem);
    *(uint64_t*)(s + 0x30) = stored;

    // Set element ID 50000 (0xC350)
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x30), 50000);

    // Get entity, find sub-component via component type array search
    void* entity = (void*)*(uint64_t*)(s + 0x28);
    int subTypeKey = *(int*)((uintptr_t)component + 0x24);
    uint32_t entityTypeId = g_TypeId_ComponentEntity ? *g_TypeId_ComponentEntity : 0;
    void* entityChecked = nullptr;
    if (fn_IsDerivedFrom(entity, entityTypeId)) {
        entityChecked = entity;
    }
    void* subComp = FindComponentByTypeId(entityChecked, subTypeKey);
    *(uint64_t*)(s + 0x38) = (uint64_t)subComp;

    // If element at +0x30 IsA CheckboxElement, set initial checkbox state
    uint32_t checkboxTypeId = g_TypeId_CheckboxElement ? *g_TypeId_CheckboxElement : 0;
    void* elemAt30 = (void*)*(uint64_t*)(s + 0x30);
    bool isCheckbox = fn_IsDerivedFrom(elemAt30, checkboxTypeId);
    if (isCheckbox) {
        void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
        void** vtable = *(void***)subCompPtr;
        // vtable[13] (offset 0x68): query enabled state with key from component+0x20
        typedef uint32_t (*fn_QueryState_t)(void*, int);
        uint32_t value = ((fn_QueryState_t)vtable[13])(subCompPtr, *(int*)((uintptr_t)component + 0x20));

        // Validate element again for safe access
        void* checkboxElem = nullptr;
        if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
            checkboxElem = elemAt30;
        }
        // Set checkbox state: bit 1 of return value
        *(char*)((uintptr_t)checkboxElem + 0xd8) = (char)((value >> 1) & 1);
    }
}

// Slot7: Update checkbox state from component, set enabled/disabled.
void ControlUI_Slot7(ControlUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get datasource component
    void* datasource = (void*)*(uint64_t*)(s + 0x10);
    uint32_t controlTypeId = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
    void* component = nullptr;
    if (fn_IsDerivedFrom(datasource, controlTypeId)) {
        component = datasource;
    }

    // Check if element IsA CheckboxElement
    uint32_t checkboxTypeId = g_TypeId_CheckboxElement ? *g_TypeId_CheckboxElement : 0;
    void* elemAt30 = (void*)*(uint64_t*)(s + 0x30);
    bool isCheckbox = fn_IsDerivedFrom(elemAt30, checkboxTypeId);
    if (isCheckbox) {
        // Query current state from sub-component vtable[13]
        void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
        void** subVtable = *(void***)subCompPtr;
        typedef uint32_t (*fn_QueryState_t)(void*, int);
        uint32_t value = ((fn_QueryState_t)subVtable[13])(subCompPtr, *(int*)((uintptr_t)component + 0x20));
        char newState = (char)((value >> 1) & 1);

        // Only update if state changed
        void* checkboxElem = nullptr;
        if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
            checkboxElem = elemAt30;
        }
        if (newState != *(char*)((uintptr_t)checkboxElem + 0xd8)) {
            void* checkboxElem2 = nullptr;
            if (fn_IsDerivedFrom(elemAt30, checkboxTypeId)) {
                checkboxElem2 = elemAt30;
            }
            *(char*)((uintptr_t)checkboxElem2 + 0xd8) = newState;
        }
    }

    // Set enabled/disabled via vtable[8] based on component's enabled state
    void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
    void** subVtable = *(void***)subCompPtr;
    typedef uint32_t (*fn_QueryState_t)(void*, int);
    uint32_t enabledBits = ((fn_QueryState_t)subVtable[13])(subCompPtr, *(int*)((uintptr_t)component + 0x20));
    bool enabled = (enabledBits & 1) != 0;

    void* elem = (void*)*(uint64_t*)(s + 0x30);
    void** elemVtable = *(void***)elem;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)elemVtable[8])(elem, enabled);
}

// Slot9: On checkbox click (event 0x12, id 50000), toggle component state.
// param2 is a POINTER — must deref to compare against stored element ptr.
// Original asm: CMP [RDI],RAX where RDI=param2, RAX=*(self+0x30)
void ControlUI_Slot9(ControlUI* self, uint64_t* param2, int param3, int param4) {
    uintptr_t s = (uintptr_t)self;

    if (param3 == 0x12 && param4 == 50000) {
        // CheckPtr on stored element before comparison
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
        // Verify: *param2 must match stored element pointer at +0x30
        if (*param2 == *(uint64_t*)(s + 0x30)) {
            // Get datasource component
            void* datasource = (void*)*(uint64_t*)(s + 0x10);
            uint32_t controlTypeId = g_TypeId_ControlComponent ? *g_TypeId_ControlComponent : 0;
            void* component = nullptr;
            if (fn_IsDerivedFrom(datasource, controlTypeId)) {
                component = datasource;
            }

            // CheckPtr on sub-component before vtable call
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
            // Call vtable[14] (offset 0x70) on sub-component to toggle state
            void* subCompPtr = (void*)*(uint64_t*)(s + 0x38);
            void** subVtable = *(void***)subCompPtr;
            typedef void (*fn_ToggleState_t)(void*, int);
            ((fn_ToggleState_t)subVtable[14])(subCompPtr, *(int*)((uintptr_t)component + 0x20));
        }
    }
}
