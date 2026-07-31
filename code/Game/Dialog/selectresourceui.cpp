// engine/src/ui/selectresourceui.cpp
// Clean reimplementation of SelectResourceUI shared methods.
//
// SelectResourceUI is the abstract base for CropUI, OrchardUI, PastureUI.
// +0x28 = entity (ComponentEntity*)
// +0x30 = combo element (InternalBasePtr, set during Slot4 initialization).
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include "Object/factory_registration.h"
#include <cstdint>

// ============================================================
// SelectDescription layout (from Ghidra analysis):
//   +0x20: SpriteSheet (ExternalPtr, 8 bytes)
//   +0x28: SpriteName (System::String, 8 bytes)
//   +0x30: StringTable (ExternalPtr, 8 bytes)
//   +0x38: StringName (System::String, 8 bytes)
//   +0x40: categoryId (int)
// ============================================================

// ============================================================
// Resource type list in game world (from DAT_1401c3dd8 + 0xF0):
//   +0x120: resourceTypes array pointer (array of pointers)
//   +0x128: resourceTypes count (int)
//
// Each resource type entry:
//   +0x28: sub-components array pointer (array of pointers)
//   +0x30: sub-components count (int)
// ============================================================

// Helper: iterate game world resource types, find SelectDescription
// sub-components matching the category filter, and either count them
// or populate a combo box.
//
// When combo is non-null, adds items to the combo and sets selectedIdx
// to the combo index matching the current resource type. When combo is
// null, just counts matching items.
//
// This pattern is shared by PopulateCombo (0x140078830) and
// GetResourceCount (0x140078a40).

static int SelectResourceUI_IterateResources(void* self, void* combo, int* selectedIndexOut) {
    uintptr_t s = (uintptr_t)self;
    int count = 0;
    int selectedIdx = -1;

    // Belt-and-braces: the .rdata type-id globals are null until seeded
    // (reg_pipeline_typeids.cpp registers 0x1401c48a8); lazy-resolve by name so
    // the combo populate never derefs a null pointer (StorageUI EnsureGlobals
    // pattern).
    static uint32_t s_sd = 0, s_ce = 0;
    if (!g_TypeId_SelectDescription) {
        s_sd = ::Object::ResolveTypeIdByNameLazy("SelectDescription");
        if (s_sd) g_TypeId_SelectDescription = &s_sd;
    }
    if (!g_TypeId_ComponentEntity) {
        s_ce = ::Object::ResolveTypeIdByNameLazy("ComponentEntity");
        if (s_ce) g_TypeId_ComponentEntity = &s_ce;
    }
    if (!g_TypeId_SelectDescription || !g_TypeId_ComponentEntity) return 0;

    // Get resource type list from game world
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t scenarioPtr = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);
    int numTypes = *(int*)(scenarioPtr + 0x128);
    uintptr_t typeArray = *(uintptr_t*)(scenarioPtr + 0x120);

    // Get current resource type via virtual call (vtable +0x60)
    // Returns the currently selected resource type pointer
    uint64_t currentResource = 0;
    if (combo) {
        typedef void (*fn_GetCurrentResource_t)(void* self, void* outPtr);
        void** vtable = *(void***)self;
        fn_GetCurrentResource_t fnGetCurrent =
            reinterpret_cast<fn_GetCurrentResource_t>(vtable[0x60 / 8]);
        fnGetCurrent(self, &currentResource);
    }

    // Get category filter via virtual call (vtable +0x68)
    typedef int (*fn_GetCategoryFilter_t)(void* self);
    void** vtable = *(void***)self;
    fn_GetCategoryFilter_t fnGetCategory =
        reinterpret_cast<fn_GetCategoryFilter_t>(vtable[0x68 / 8]);
    int categoryFilter = fnGetCategory(self);

    // Wrap entity at +0x28 in InternalBasePtr (base class requirement)
    void* entity = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entity, *g_TypeId_ComponentEntity);
    void* entityPtr = isEntity ? entity : nullptr;
    uint8_t entityBuf[8];
    fn_InternalBasePtr_Ctor(entityBuf, entityPtr);

    int comboIdx = 0;

    for (int i = 0; i < numTypes; i++) {
        uintptr_t resType = *(uintptr_t*)(typeArray + (uintptr_t)i * 8);

        // Iterate sub-components of this resource type
        int subCount = *(int*)(resType + 0x30);
        uintptr_t subArray = *(uintptr_t*)(resType + 0x28);

        for (int j = 0; j < subCount; j++) {
            uintptr_t subComp = *(uintptr_t*)(subArray + (uintptr_t)j * 8);

            // Check if sub-component's type matches SelectDescription
            // Virtual call at vtable+0x10 returns type ID
            typedef int (*GetTypeId_t)(void*);
            void** subVtable = *(void***)subComp;
            GetTypeId_t fnGetTypeId = reinterpret_cast<GetTypeId_t>(subVtable[0x10 / 8]);
            int subTypeId = fnGetTypeId((void*)subComp);

            if (subTypeId == (int)*g_TypeId_SelectDescription) {
                void* subObj = *(void**)(subArray + (uintptr_t)j * 8);
                bool isDerived = fn_IsDerivedFrom(subObj, *g_TypeId_SelectDescription);
                if (isDerived && subObj != nullptr) {
                    // Check category filter matches
                    int category = *(int*)((uintptr_t)subObj + 0x40);
                    if (category == categoryFilter) {
                        if (combo) {
                            // Create UI::String from SelectDescription name
                            uint8_t uiStrBuf[16];
                            fn_UIString_Ctor(uiStrBuf,
                                            *(uint64_t*)((uintptr_t)subObj + 0x30),
                                            (void*)((uintptr_t)subObj + 0x38));
                            // Convert to System::String
                            uint8_t sysStrBuf[8];
                            fn_UIString_ToSystemString(uiStrBuf, sysStrBuf);

                            // Create Sprite from SelectDescription icon
                            uint8_t spriteBuf[16];
                            fn_Sprite_Ctor(spriteBuf,
                                          *(uint64_t*)((uintptr_t)subObj + 0x20),
                                          (void*)((uintptr_t)subObj + 0x28));

                            // Add item to combo with index as userData
                            fn_ComboElement_AddItem(combo, spriteBuf, sysStrBuf,
                                                    (void*)(intptr_t)comboIdx);
                            fn_String_Dtor(sysStrBuf);

                            // Check if this is the currently selected resource
                            if (resType == currentResource) {
                                selectedIdx = count;
                            }
                        }
                        count++;
                    }
                }
                break;  // Found the SelectDescription for this type, move to next
            }
        }
        comboIdx++;
    }

    if (selectedIndexOut) *selectedIndexOut = selectedIdx;
    return count;
}

// ============================================================
// SelectResourceUI::PopulateCombo (0x140078830) — Fill combo
// ============================================================
// Clears the combo, iterates game world resource types, finds
// SelectDescription sub-components matching the category filter,
// and adds them as combo items with sprites and names.
// Sets the current selection to match the entity's current resource.
//
// Original: FUN_140078830 (was fn_SelectResourceUI_PopulateCombo).

void SelectResourceUI_PopulateCombo(void* self) {
    uintptr_t s = (uintptr_t)self;

    // Get the selectResource combo element
    void* comboIBP = (void*)(s + 0x30);
    fn_InternalBasePtr_CheckPtr(comboIBP);
    void* combo = *(void**)(s + 0x30);

    // Clear existing items
    fn_ComboElement_RemoveAllItems(combo);

    // Populate combo and get selected index
    int selectedIdx = -1;
    SelectResourceUI_IterateResources(self, combo, &selectedIdx);

    // Set current selection if we found a match
    if (selectedIdx >= 0) {
        fn_InternalBasePtr_CheckPtr(comboIBP);
        fn_ComboElement_SetCurrentItem(*(void**)(s + 0x30), selectedIdx);
    }
}

// ============================================================
// SelectResourceUI::GetResourceCount (0x140078a40) — Count matching
// ============================================================
// Same iteration as PopulateCombo but just counts matching types.
// Used by Slot8 to detect when the combo needs refreshing.
//
// Original: FUN_140078a40 (was fn_SelectResourceUI_GetResourceCount).

int SelectResourceUI_GetResourceCount(void* self) {
    return SelectResourceUI_IterateResources(self, nullptr, nullptr);
}

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

    // Store combo element at +0x30 (IsDerivedFrom check skipped -- null check only)
    void* rawElem = (void*)findResult;
    void* checked = rawElem ? rawElem : nullptr;
    uint64_t basePtrVal = 0;
    fn_InternalBasePtr_Ctor(&basePtrVal, checked);
    *(uint64_t*)(s + 0x30) = basePtrVal;

    // Set element ID to 500 (combo selection event ID)
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x30), 500);

    // Populate combo with available resources
    SelectResourceUI_PopulateCombo(self);
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
    int expectedCount = SelectResourceUI_GetResourceCount(self);

    if (currentCount != expectedCount) {
        SelectResourceUI_PopulateCombo(self);
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

    // Call Slot11 via vtable (polymorphic -- sets resource type on component)
    typedef void (*fn_Slot11_t)(void* self, int64_t resourceType);
    void** vtable = *(void***)self;
    ((fn_Slot11_t)vtable[11])(self, resourceType);
}
