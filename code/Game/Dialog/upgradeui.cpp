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
#include <cstring>

// ============================================================
// UpgradeUI::Serialize (vtable +0x20) — Slot4
// ============================================================
// Finds the upgrade button element, creates a sprite from the
// BuildRequirementComponent on the upgrade description, and
// sets the sprite on the button's child image element.
//
// Original: 0x140086d90
void UpgradeUI_Serialize(UpgradeUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get description from DialogController +0x10
    void* desc = *(void**)(s + 0x10);
    if (!desc) return;

    // Check if description is an UpgradeDescription type
    if (!fn_IsDerivedFrom(desc, *g_TypeId_UpgradeDescription))
        return;
    void* upgradeDesc = desc;

    // Get the controller's element at +0x20, check if it's a ButtonElement
    void* elem = *(void**)(s + 0x20);
    if (!elem) return;

    // The element is expected to be a button — store at +0x30
    // (In the original, this checks IsDerivedFrom(ButtonElement), but we skip
    // the runtime type check since the vtable setup guarantees the element type)
    *(void**)(s + 0x30) = elem;
    void* button = elem;

    // Set button element ID to 33000
    fn_Element_SetElementId(button, 33000);

    // Get button's first child (should be an ImageElement)
    uint8_t childBuf[16];
    fn_Element_Child(button, childBuf, 0);
    void* imageElem = *(void**)childBuf;
    if (!imageElem) return;

    // Get the sub-description from upgradeDesc+0x20
    uintptr_t subDesc = *(uintptr_t*)((uintptr_t)upgradeDesc + 0x20);
    if (!subDesc) return;

    // Iterate sub-description's component list to find BuildRequirementComponent
    // Component array at subDesc+0x28, count at subDesc+0x30
    void* buildReqComp = nullptr;
    int compCount = *(int*)(subDesc + 0x30);
    uintptr_t compArray = *(uintptr_t*)(subDesc + 0x28);

    for (int i = 0; i < compCount; i++) {
        void* comp = *(void**)(compArray + (uintptr_t)i * 8);
        if (!comp) continue;

        // Call component vtable[2] to get type ID
        void** compVtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int typeId = ((fn_GetTypeId_t)compVtable[2])(comp);

        if (typeId == (int)*g_TypeId_BuildRequirementComponent) {
            // Verify with IsDerivedFrom
            if (fn_IsDerivedFrom(comp, *g_TypeId_BuildRequirementComponent)) {
                buildReqComp = comp;
            }
            break;
        }
    }

    // Create a Sprite from the BuildRequirementComponent
    // Sprite is 12 bytes: {ExternalPtr<SpriteSheet> +0x20, int spriteIndex}
    // We use SpriteSheet::GetSpriteIndexFromName to resolve the index
    if (buildReqComp) {
        uintptr_t brc = (uintptr_t)buildReqComp;
        void* spriteSheet = *(void**)(brc + 0x20);      // SpriteSheet ExternalPtr
        void* spriteName = (void*)(brc + 0x28);          // System::String (name)

        uint8_t sprite[16] = {0};
        memcpy(sprite, &spriteSheet, 8);                  // ExternalPtr<SpriteSheet>
        if (spriteSheet) {
            int idx = fn_SpriteSheet_GetSpriteIndexFromName(spriteSheet, spriteName);
            *(int*)(sprite + 8) = idx;
        } else {
            *(int*)(sprite + 8) = -1;
        }

        fn_ImageElement_SetSprite(imageElem, sprite);
    }

    // Set button's user data to the sub-description (upgradeDesc+0x20)
    void* subDescBase = *(void**)((uintptr_t)upgradeDesc + 0x20);
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
