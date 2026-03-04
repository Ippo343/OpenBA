// engine/src/ui/selectresourceui.cpp
// Clean reimplementation of SelectResourceUI shared methods.
//
// SelectResourceUI is the abstract base for CropUI, OrchardUI, PastureUI.
// +0x30 = combo element (set during Slot4 initialization).
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// SelectResourceUI::Slot4 (vtable +0x20) — Initialize combo element
// ============================================================
// Finds the "selectResource" combo element in the dialog, stores at +0x30,
// sets its element ID to 500, and populates it with resources.
//
// Original: 0x140078be0 (shared by CropUI, OrchardUI, PastureUI).

void SelectResourceUI_Slot4(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "selectResource" combo element via DialogInstance at +0x18
    uint8_t nameBuf[8];
    fn_String_Ctor_CStr(nameBuf, "selectResource");

    void* dialogInstance = *(void**)(s + 0x18);
    uint64_t findResult = 0;
    fn_DialogInstance_FindElement(dialogInstance, &findResult, nameBuf);
    fn_String_Dtor(nameBuf);

    // Store combo element at +0x30 (IsDerivedFrom check skipped — null check only)
    void* rawElem = (void*)findResult;
    void* checked = rawElem ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    fn_InternalBasePtr_Ctor(&basePtrVal, checked);
    *(uint64_t*)(s + 0x30) = basePtrVal;

    // Set element ID to 500 (combo selection event ID)
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x30), 500);

    // Populate combo with available resources
    fn_SelectResourceUI_PopulateCombo(self);
}

// ============================================================
// SelectResourceUI::Slot8 (vtable +0x40) — Repopulate combo if count changed
// ============================================================
// Checks if the combo element's item count matches the expected resource count.
// If different, repopulates the combo with current resources.
//
// Original: 0x140078D30 (shared by OrchardUI and PastureUI).

void SelectResourceUI_Slot8(void* self) {
    uintptr_t s = (uintptr_t)self;

    void* comboElem = *(void**)(s + 0x30);
    int currentCount = fn_ComboElement_GetItemCount(comboElem);
    int expectedCount = fn_SelectResourceUI_GetResourceCount(self);

    if (currentCount != expectedCount) {
        fn_SelectResourceUI_PopulateCombo(self);
    }
}

// ============================================================
// SelectResourceUI::Slot9 (vtable +0x48) — Combo selection handler
// ============================================================
// On combo selection (msg=0x14, id=500), looks up the selected resource
// type from the game world's resource list and calls Slot11 (vtable +0x58)
// to apply it to the component.
//
// Original: 0x140078D80 (shared by CropUI and OrchardUI).

void SelectResourceUI_Slot9(void* self, void* param2, int param3, int param4) {
    if (param3 != 0x14 || param4 != 500)
        return;

    uintptr_t s = (uintptr_t)self;
    void* comboElem = *(void**)(s + 0x30);

    // Get current selected item index from combo
    int currentItem = fn_ComboElement_GetCurrentItem(comboElem);

    // Get the item data (resource index) for the current selection
    void* itemData = fn_ComboElement_GetItemData(comboElem, currentItem);
    int resourceIndex = (int)(intptr_t)itemData;

    // Look up resource type from game world's entity list
    void* world = *g_pGameWorldSingleton;
    uintptr_t entityList = *(uintptr_t*)((uintptr_t)world + 0xf0);
    uintptr_t resourceArray = *(uintptr_t*)(entityList + 0x120);
    int64_t resourceType = *(int64_t*)(resourceArray + (int64_t)resourceIndex * 8);

    // Call Slot11 via vtable (polymorphic — sets resource type on component)
    typedef void (*fn_Slot11_t)(void* self, int64_t resourceType);
    void** vtable = *(void***)self;
    ((fn_Slot11_t)vtable[11])(self, resourceType);
}
