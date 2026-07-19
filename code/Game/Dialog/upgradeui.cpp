// code/Game/Dialog/upgradeui.cpp
// Clean reimplementation of UpgradeUI game logic.
//
// UpgradeUI (0x38 bytes) — upgrade panel controller.
// Extends DialogController (0x30 base).
// +0x30 = button element (InternalBasePtr)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// UpgradeUI::Serialize (vtable +0x20) — Slot4
// ============================================================
// Finds the upgrade button element, creates a sprite from the
// ToolbarDescription on the upgrade description, and
// sets the sprite on the button's child image element.
//
// Original: 0x140086d90
void UpgradeUI_Serialize(UpgradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get description from DialogController +0x10
    void* desc = *(void**)(s + 0x10);

    // Check if description is an UpgradeDescription type
    bool isUpgrade = fn_IsDerivedFrom(desc, *g_TypeId_UpgradeDescription);
    void* upgradeDesc = isUpgrade ? desc : nullptr;

    // CheckPtr on the InternalBasePtr at +0x20 before reading
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x20));
    void* elem = *(void**)(s + 0x20);

    // Check if element is a ButtonElement (exref 0x1401514f8)
    bool isButton = fn_IsDerivedFrom(elem, *g_TypeId_HelpButtonElem);
    void* checkedElem = isButton ? elem : nullptr;

    // Properly construct InternalBasePtr at +0x30 (button reference)
    fn_InternalBasePtr_Ctor((void*)(s + 0x30), checkedElem);

    // Set button element ID to 33000
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    void* button = *(void**)(s + 0x30);
    fn_Element_SetElementId(button, 33000);

    // Get button's child at index 4 (the ImageElement)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    button = *(void**)(s + 0x30);
    uint8_t childBuf[16];
    fn_Element_Child(button, childBuf, 4);

    // Check if child is an ImageElement type
    fn_InternalBasePtr_CheckPtr(childBuf);
    void* childElem = *(void**)childBuf;
    bool isImage = fn_IsDerivedFrom(childElem, *g_TypeId_ImageElement);
    void* checkedImage = isImage ? childElem : nullptr;

    // Construct an IBP for the image element
    uint8_t imageIBP[8];
    fn_InternalBasePtr_Ctor(imageIBP, checkedImage);

    // Get the sub-description from upgradeDesc+0x20
    uintptr_t subDesc = *(uintptr_t*)((uintptr_t)upgradeDesc + 0x20);

    // Find ToolbarDescription in sub-description's component list
    void* buildReqComp = nullptr;
    if (subDesc && *(int*)(subDesc + 0x30) > 0) {
        int compCount = *(int*)(subDesc + 0x30);
        uintptr_t compArray = *(uintptr_t*)(subDesc + 0x28);

        for (int i = 0; i < compCount; i++) {
            void* comp = *(void**)(compArray + (uintptr_t)i * 8);
            if (!comp) continue;

            // Call component vtable[2] to get type ID
            void** compVtable = *(void***)comp;
            typedef uint32_t (*fn_GetTypeId_t)(void*);
            uint32_t typeId = ((fn_GetTypeId_t)compVtable[2])(comp);

            if (typeId == *g_TypeId_ToolbarDescription_FC) {
                void* checked = fn_IsDerivedFrom(comp, *g_TypeId_ToolbarDescription_FC)
                    ? comp : nullptr;
                buildReqComp = checked;
                break;
            }
        }
    }

    // Create a Sprite from the ToolbarDescription using proper constructor
    uintptr_t brc = (uintptr_t)buildReqComp;
    uint8_t sprite[16];
    fn_Sprite_Ctor(sprite, *(uint64_t*)(brc + 0x20), (void*)(brc + 0x28));

    // Set sprite on the image element
    fn_InternalBasePtr_CheckPtr(imageIBP);
    void* imageElem = *(void**)imageIBP;
    fn_ImageElement_SetSprite(imageElem, sprite);

    // Set button's user data to the sub-description
    void* subDescBase = *(void**)((uintptr_t)upgradeDesc + 0x20);
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x30));
    button = *(void**)(s + 0x30);
    fn_Element_SetUserData(button, subDescBase);
}

// ============================================================
// UpgradeUI::OnRemove (vtable +0x48) — Slot9
// ============================================================
// On button click (ID 33000), finds DestroyComponent on the entity,
// copies the UpgradeDescription reference to it, and starts the upgrade.
//
// Original: 0x140086c90
void UpgradeUI_OnRemove(UpgradeUI* self, void* p2, int eventType, int eventId) {
    uintptr_t s = (uintptr_t)self;

    // Only handle button clicks (msg 0x12) with ID 33000
    if (eventType != 0x12 || eventId != 33000)
        return;

    // Verify the event source matches our stored button element
    void* button = *(void**)(s + 0x30);
    void* eventSource = *(void**)p2;
    if (eventSource != button)
        return;

    // Get the data source entity from DialogController +0x28
    void* dataSource = *(void**)(s + 0x28);
    if (!dataSource)
        return;

    // Check if data source is a ComponentEntity
    if (!fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity))
        return;

    // Find DestroyComponent on the entity
    uint8_t compBuf[16];
    fn_FindComponent_Destroy(dataSource, compBuf);
    void* destroyComp = *(void**)compBuf;
    if (!destroyComp)
        return;

    // Get the description from DialogController +0x10
    void* desc = *(void**)(s + 0x10);
    if (!desc)
        return;

    // Check if description is an UpgradeDescription type
    if (!fn_IsDerivedFrom(desc, *g_TypeId_UpgradeDescription))
        return;

    // Set the upgrade description reference on DestroyComponent
    // destroy+0x60 = desc+0x20 (the sub-description pointer)
    *(uint64_t*)((uintptr_t)destroyComp + 0x60) = *(uint64_t*)((uintptr_t)desc + 0x20);

    // Start the upgrade process
    // Call our clean reimplementation instead of exe function pointer
    extern void DestroyComponent_StartUpgrade_Impl(void* destroyComp);
    DestroyComponent_StartUpgrade_Impl(destroyComp);
}
