// engine/src/ui/entityui.cpp
// Clean reimplementation of EntityUI panel methods.
//
// EntityUI (0x48 bytes): Entity info panel.
// Layout: +0x28 = data source (entity), +0x30 = entity ptr,
//         +0x38 = checkPin element, +0x40 = pages element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// Slot4 (0x1400670D0): Startup — find elements, get entity, reset ToolUI flags.
void EntityUI_Slot4(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source (+0x28), check if it's a ComponentEntity
    void* dataSource = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? dataSource : nullptr;
    fn_InternalBasePtr_Ctor((void*)(s + 0x30), entity);

    // Find "buttonCenter" element and set ID 101 (0x65)
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "buttonCenter");
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        fn_Element_SetElementId(elem, 0x65);
    }

    // Find "checkPin" element → store at +0x38, set ID 100
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "checkPin");
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        *(void**)(s + 0x38) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        fn_Element_SetElementId(*(void**)(s + 0x38), 100);
    }

    // Get dialog controller, check if it's a ToolUI type
    {
        void* dialogInst = *(void**)(s + 0x18);
        uint8_t ctrlBuf[16];
        fn_DialogInstance_GetController(dialogInst, ctrlBuf, *g_TypeId_ToolUI);
        void* controller = *(void**)ctrlBuf;

        void* toolUI = nullptr;
        if (controller) {
            bool isTool = fn_IsDerivedFrom(controller, *g_TypeId_ToolUI);
            if (isTool) toolUI = controller;
        }

        uint8_t toolBuf[16];
        fn_InternalBasePtr_Ctor(toolBuf, toolUI);
        void* toolPtr = *(void**)toolBuf;

        // Reset flags at controller+0x40 and +0x41
        *(uint8_t*)((uintptr_t)toolPtr + 0x40) = 0;
        if (*(uint8_t*)((uintptr_t)toolPtr + 0x40) == 0 &&
            *(uint8_t*)((uintptr_t)toolPtr + 0x41) != 0) {
            *(uint8_t*)((uintptr_t)toolPtr + 0x41) = 0;
        }
    }

    // Find "pages" element → store at +0x40
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "pages");
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        *(void**)(s + 0x40) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }
}

// Slot6: Remove entity from world tracking set.
void EntityUI_Slot6(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Copy +0x30 (entity InternalBasePtr) to local
    uint64_t entityKey = *(uint64_t*)(s + 0x30);

    // Get BST root: g_pGameWorldSingleton -> +0x120 -> +0x60
    void* worldSingleton = *g_pGameWorldSingleton;
    void* subObj = *(void**)((uintptr_t)worldSingleton + 0x120);
    void* bstRoot = (void*)((uintptr_t)subObj + 0x60);

    fn_EntityTrackingSet_Remove(bstRoot, &entityKey);
}

// ============================================================
// EntityUI::Slot7 (vtable +0x38) — Page switching logic
// ============================================================
// Determines which page to show based on entity state (build state,
// destroy state, etc.) and sets the PageElement active page.
// Also updates ToolUI controller's selection hint.
//
// Original: 0x140066D80.

void EntityUI_Slot7(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get current EditTool selection from player controller
    void* playerController = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
    void* currentTool = *(void**)((uintptr_t)playerController + 0x28);

    // Check if current tool is EditTool type
    bool isEditTool = fn_IsDerivedFrom(currentTool, *g_TypeId_EditTool);
    void* editTool = isEditTool ? currentTool : nullptr;
    uint8_t editToolBuf[8];
    fn_InternalBasePtr_Ctor(editToolBuf, editTool);
    void* editToolPtr = *(void**)editToolBuf;

    // Determine tool hint: check if EditTool's selected entity matches ours
    uint64_t toolHint;
    if (!editToolPtr) {
        toolHint = 0xffffffff;  // no edit tool active
    } else {
        // Get EditTool's selected entity via vtable[13] (offset 0x68)
        void** vtbl = *(void***)editToolPtr;
        typedef void* (*VCall_GetSelected_t)(void*, void*);
        uint64_t selectedEntity = 0;
        ((VCall_GetSelected_t)vtbl[13])(editToolPtr, &selectedEntity);

        if (selectedEntity != *(uint64_t*)(s + 0x30)) {
            toolHint = 0xffffffff;  // different entity selected
        } else {
            toolHint = 0xffffcd4d;  // our entity is selected (special hint value)
        }
    }

    // Get ToolUI controller from dialog
    void* dialogInst = *(void**)(s + 0x18);
    uint8_t ctrlBuf[16];
    fn_DialogInstance_GetController(dialogInst, ctrlBuf, *g_TypeId_ToolUI);
    void* controller = *(void**)ctrlBuf;

    void* toolUI = nullptr;
    if (controller) {
        bool isTool = fn_IsDerivedFrom(controller, *g_TypeId_ToolUI);
        if (isTool) toolUI = controller;
    }
    uint8_t toolBuf[8];
    fn_InternalBasePtr_Ctor(toolBuf, toolUI);
    void* toolPtr = *(void**)toolBuf;

    // Set tool hint via vtable[10] (offset 0x50) on ToolUI's +0x38 element
    {
        void* hintElem = *(void**)((uintptr_t)toolPtr + 0x38);
        void** vtbl = *(void***)hintElem;
        typedef void (*VCall_SetHint_t)(void*, uint64_t);
        ((VCall_SetHint_t)vtbl[10])(hintElem, toolHint);
    }

    // Find components on entity to determine page state
    void* entityPtr = *(void**)(s + 0x30);

    void* buildStateComp = nullptr;
    fn_FindComponent_BuildState(entityPtr, &buildStateComp);

    void* posComp = nullptr;
    fn_FindComponent_Position(entityPtr, &posComp);

    void* smComp = nullptr;  // StateMachine/animation component
    fn_FindComponent_SM(entityPtr, &smComp);

    void* destroyComp = nullptr;
    fn_FindComponent_Destroy(entityPtr, &destroyComp);

    // Determine page index based on component state
    int pageIdx;

    if (buildStateComp == 0) {
        pageIdx = -1;
    } else {
        // Check build state: +0x52 (if negative) use +0x50
        int8_t state52 = *(int8_t*)((uintptr_t)buildStateComp + 0x52);
        int8_t state50 = *(int8_t*)((uintptr_t)buildStateComp + 0x50);
        pageIdx = (state52 < 0) ? (int)state50 : (int)state52;
    }

    if (posComp == 0 && destroyComp == 0) {
        if (smComp == 0 || pageIdx != 0) {
            // Get default page from entity's description
            void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
            void* descResult = nullptr;
            fn_GetDescPage(entityDesc, &descResult);
            pageIdx = descResult ? *(int*)((uintptr_t)descResult + 0x40) : 2;
        } else {
            pageIdx = 2;
        }
    } else {
        // Build/destroy in progress
        if (pageIdx >= 3 && pageIdx <= 7) {
            pageIdx = 3;
        } else if (posComp != 0) {
            // Check if build is complete
            bool complete = fn_BuildComplete(posComp);
            if (!complete) {
                pageIdx = 2;
            } else if (pageIdx != 0) {
                // Use entity description page or keep current page if pinned (page 4)
                void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
                void* descResult = nullptr;
                fn_GetDescPage(entityDesc, &descResult);

                int activePage = fn_PageElement_GetActivePage(*(void**)(s + 0x40));
                if (activePage != 4) {
                    pageIdx = descResult ? *(int*)((uintptr_t)descResult + 0x40) : 2;
                }
            } else {
                pageIdx = 2;
            }
        } else {
            pageIdx = 2;
        }
    }

    fn_PageElement_SetActivePage(*(void**)(s + 0x40), pageIdx);
}

// ============================================================
// EntityUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles button clicks:
//   ID 1 = close entity panel (set edit mode 0)
//   ID 0x65 (101) = center camera on entity
//   ID 100 = pin/unpin entity tracking
//
// Original: 0x140067050.

void EntityUI_Slot9(EntityUI* self, void* param2, int msg, int id) {
    (void)param2;
    if (msg != 0x12) return;

    uintptr_t s = (uintptr_t)self;
    void* playerController = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);

    if (id == 1) {
        // Close entity panel — set edit mode to 0
        fn_EntityUI_SetEditMode(playerController, 0);
        return;
    }

    if (id == 0x65) {
        // Center camera on entity — navigate to entity via BuildComponent
        void* entityPtr = *(void**)(s + 0x30);

        // Try to find a path/position component to center on
        void* pathComp = nullptr;
        {
            // Entity_ClearPathQueue (FUN_14000a170-ish, actually different)
            // Use BuildComponent center position
            void* buildComp = nullptr;
            fn_FindComponent_Build(entityPtr, &buildComp);
            if (buildComp) {
                // Read transform position from BuildComponent+0x28/+0x2A/+0x2C/+0x2E
                int* worldGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xf8);
                int gridSize = *worldGrid;
                int16_t posX = *(int16_t*)((uintptr_t)buildComp + 0x28);
                int16_t posY = *(int16_t*)((uintptr_t)buildComp + 0x2a);
                int16_t sizeX = *(int16_t*)((uintptr_t)buildComp + 0x2c);
                int16_t sizeY = *(int16_t*)((uintptr_t)buildComp + 0x2e);

                // Calculate center tile
                int centerIdx = (int)sizeX + ((int)sizeY + (int)posY) * gridSize + (int)posX;

                // Convert tile to world position
                float worldX = (float)(centerIdx % gridSize) - (float)(gridSize >> 1);
                float worldY = (float)(centerIdx / gridSize) - (float)(gridSize >> 1);

                // Set camera target
                void* cameraTarget = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0xe0);
                *(float*)((uintptr_t)cameraTarget + 0x1c) = worldX * 0.5f;
                *(float*)((uintptr_t)cameraTarget + 0x20) = worldY * 0.5f;
                *(float*)((uintptr_t)cameraTarget + 0x24) = 0.0f;
            }
        }
        return;
    }

    if (id == 100) {
        // Pin/unpin: toggle checkPin state and update entity tracking

        // Get ToolUI controller
        void* dialogInst = *(void**)(s + 0x18);
        uint8_t ctrlBuf[16];
        fn_DialogInstance_GetController(dialogInst, ctrlBuf, *g_TypeId_ToolUI);
        void* controller = *(void**)ctrlBuf;

        void* toolUI = nullptr;
        if (controller) {
            bool isTool = fn_IsDerivedFrom(controller, *g_TypeId_ToolUI);
            if (isTool) toolUI = controller;
        }
        uint8_t toolBuf[8];
        fn_InternalBasePtr_Ctor(toolBuf, toolUI);
        void* toolPtr = *(void**)toolBuf;

        // Read current pin state from checkPin element (+0x38) -> +0xD8
        char pinState = *(char*)(*(uintptr_t*)(s + 0x38) + 0xd8);

        // Set ToolUI +0x40 flag to pin state
        *(char*)((uintptr_t)toolPtr + 0x40) = pinState;
        if (pinState == 0 && *(char*)((uintptr_t)toolPtr + 0x41) != 0) {
            *(char*)((uintptr_t)toolPtr + 0x41) = 0;
        }

        // If unpinned, remove entity from tracking set
        if (*(char*)(*(uintptr_t*)(s + 0x38) + 0xd8) == 0) {
            uint64_t entityKey = *(uint64_t*)(s + 0x30);
            fn_EntityTrackingSet_Remove(
                (void*)(*(uintptr_t*)(*(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x120)) + 0x60),
                &entityKey);
            return;
        }

        // If pinned, select entity in edit tool
        uint32_t dialogX = *(uint32_t*)(*(uintptr_t*)(s + 0x18) + 0x88);
        uint32_t dialogY = *(uint32_t*)(*(uintptr_t*)(s + 0x18) + 0x8c);
        void* entityKey = *(void**)(s + 0x30);
        fn_EntityUI_SelectEntity(playerController, &entityKey, dialogX, dialogY);
        return;
    }
}
