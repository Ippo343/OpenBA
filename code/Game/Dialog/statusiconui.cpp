// engine/src/ui/statusiconui.cpp
// Clean reimplementation of StatusIconUI game logic.
//
// StatusIconUI (0x40 bytes) — status icon grid panel.
// Extends DialogController. data30 = icon element array (ptr),
// data38 = icon element count.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// StatusIconUI::Serialize (vtable +0x20) — Cache icon child elements
// ============================================================
// Finds the "icons" container element, gets its child count,
// resizes the internal array, and copies all child InternalPtrs
// into the array for fast access during Startup.
//
// Original: 0x14007b270.

void StatusIconUI_Serialize(StatusIconUI* self) {
    // Get dialog element from controller base (+0x20)
    void* dialogElem = *(void**)((uintptr_t)self + 0x20);

    // Find "icons" container element by name
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "icons");

    uint64_t findResult = 0;
    // fn_FindElement: member with non-trivial return: RCX=this, RDX=retval, R8=name
    fn_FindElement(dialogElem, &findResult, nameBuf);

    // Dereference InternalPtr result to get raw Element*
    // The original does IsDerivedFrom check here, but we skip it
    // since FindElement by name always returns a valid element.
    void* rawElem = (void*)findResult;
    uint8_t iconsBuf[8];
    fn_InternalBasePtr_Ctor(iconsBuf, rawElem);
    void* iconsElem = *(void**)iconsBuf;

    fn_String_Dtor(nameBuf);

    // Get child count
    int childCount = fn_Element_ChildCount(iconsElem);

    // Resize the array at self+0x30 to hold childCount entries
    // FUN_1400032e0(arrayBase, newCount) — sets up the array
    fn_ExeArrayResize((void*)((uintptr_t)self + 0x30), childCount);

    // Copy each child's InternalPtr (8 bytes) into the array
    int storedCount = *(int*)((uintptr_t)self + 0x38);
    for (int i = 0; i < storedCount; i++) {
        // Element::Child returns InternalPtr<Element> via non-trivial return
        uint64_t childResult = 0;
        fn_Element_Child(iconsElem, &childResult, i);

        // Copy 8-byte InternalPtr into array slot
        uint64_t* arrayBase = *(uint64_t**)((uintptr_t)self + 0x30);
        arrayBase[i] = childResult;
    }
}

// ============================================================
// StatusIconUI::Startup (vtable +0x38) — Update icon visibility
// ============================================================
// Reads a bitfield from the MeshInstance component (+0xF0),
// checks the active page of the PageToggleUI controller,
// and shows/hides each icon element accordingly.
//
// Original: 0x14007b0a0.

void StatusIconUI_Startup(StatusIconUI* self) {
    // Get entity from controller data (+0x28)
    void* entity = *(void**)((uintptr_t)self + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;

    // Wrap entity in InternalBasePtr
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);
    void* entityInternal = *(void**)entityBuf;

    // Find MeshInstance component
    void* meshComp = nullptr;
    fn_FindComponent_MeshInstance(entityInternal, &meshComp);

    // Determine if icons should be visible based on page state
    bool iconsActive = true;

    void* parentController = *(void**)((uintptr_t)self + 0x10);
    if (parentController != nullptr) {
        // Check if parent is a DialogController
        bool isDC = fn_IsDerivedFrom(parentController, *g_TypeId_DialogController);
        void* dcPtr = isDC ? parentController : nullptr;

        // Check if controller's dialog element (+0x20) is null
        if (dcPtr && *(void**)((uintptr_t)dcPtr + 0x20) == nullptr) {
            // Get PageToggleUI controller from dialog instance
            void* dialogInst = *(void**)((uintptr_t)self + 0x18);
            uint64_t ctrlResult = 0;
            fn_DialogInstance_GetController(dialogInst, &ctrlResult, 0);

            void* pageToggle = nullptr;
            if (ctrlResult != 0) {
                void* ctrlPtr = (void*)ctrlResult;
                bool isPageToggle = fn_IsDerivedFrom(ctrlPtr, *g_TypeId_PageToggleUI);
                pageToggle = isPageToggle ? ctrlPtr : nullptr;
            }

            uint8_t ptBuf[8];
            fn_InternalBasePtr_Ctor(ptBuf, pageToggle);
            void* ptInternal = *(void**)ptBuf;

            // GetActivePage is just *(int*)(pageElement + 0xC0)
            // PageToggleUI stores its page element at +0x40
            void* pageElem = *(void**)((uintptr_t)ptInternal + 0x40);
            int activePage = *(int*)((uintptr_t)pageElem + 0xC0);

            // Icons are active on pages 2 and 3 (iVar3 - 2U <= 1)
            iconsActive = (activePage >= 2 && activePage <= 3);
        }
    }

    // Loop through icon elements, show/hide based on bitfield
    int iconCount = *(int*)((uintptr_t)self + 0x38);
    uint64_t* iconArray = *(uint64_t**)((uintptr_t)self + 0x30);
    uint32_t bitMask = 1;

    for (int i = 0; i < iconCount; i++) {
        void* iconElem = (void*)iconArray[i];

        // Read bitfield from MeshInstance component at +0xF0
        uint32_t bitfield = *(uint32_t*)((uintptr_t)meshComp + 0xF0);
        bool bitSet = (bitfield & bitMask) != 0;

        if (bitSet && iconsActive) {
            // Show icon
            fn_Element_SetFlagsOnOff(iconElem, 0, 1);
        } else {
            // Hide icon
            fn_Element_SetFlagsOnOff(iconElem, 1, 0);
        }

        // Rotate bit mask left
        bitMask = (bitMask << 1) | (bitMask >> 31);
    }
}
