// engine/src/ui/entityui.cpp
// Clean reimplementation of EntityUI panel methods.
//
// EntityUI (0x48 bytes): Entity info panel.
// Layout: +0x28 = data source (entity), +0x30 = entity ptr,
//         +0x38 = checkPin element, +0x40 = pages element

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// EntityTrackingSet_Remove (0x14002c690)
// ============================================================
// BST remove: walks tree to find node with key == param_2, calls BST_Delete.
// param_1 = pointer to BST root (root = *param_1)
// param_2 = InternalBasePtr key to remove
//
// BST node layout (at ptr 'this'):
//   +0x00: key (InternalBasePtr, uint64_t)
//   +0x20: left child ptr (node**)  [+0x20/8 = offset 4 from uint64_t* = offset 0x20]
//   +0x28: right child ptr (node**) [+0x28/8 = offset 5 = +0x28]
static void EntityTrackingSet_Remove_Impl(void* bstRoot, void* key)
{
    // param_1 is longlong* (root node double-ptr)
    uint8_t** rootPtr = (uint8_t**)bstRoot;
    uint8_t* node = (uint8_t*)*rootPtr;

    while (node != nullptr) {
        fn_InternalBasePtr_CheckPtr((void*)node);
        uint64_t nodeKey = *(uint64_t*)node;
        uint64_t searchKey = *(uint64_t*)key;

        fn_InternalBasePtr_CheckPtr(key);
        if (searchKey < nodeKey) {
            // Go left: *(node+0x20) = left child
            node = *(uint8_t**)(node + 0x20);
        } else if (searchKey <= nodeKey) {
            // Found the node — delete it
            fn_EntityTrackingSet_BST_Delete(bstRoot, (void*)node);
            return;
        } else {
            // Go right: *(node+0x28) = right child
            node = *(uint8_t**)(node + 0x28);
        }
    }
    // Not found — nothing to do
}

// ============================================================
// GetDescPage (0x140011320)
// ============================================================
// Searches entity description's component array (+0x28 = ptr, +0x30 = count)
// for a component with type ID == g_TypeId_UIDescription (DAT_1401c7f90).
// Returns the found component in retval. Component field +0x40 = page index.
static void GetDescPage_Impl(void* entityDesc, void* retval)
{
    void* result = nullptr;
    if (!entityDesc) { *(void**)retval = result; return; }

    int count = *(int*)((uintptr_t)entityDesc + 0x30);
    void** compArray = *(void***)((uintptr_t)entityDesc + 0x28);

    for (int i = 0; i < count; i++) {
        void* comp = compArray ? compArray[i] : nullptr;
        if (!comp) continue;

        // vtable[2] = GetTypeId (returns int typeId)
        void** vtable = *(void***)comp;
        typedef int (*fn_GetTypeId_t)(void*);
        int typeId = ((fn_GetTypeId_t)vtable[2])(comp);

        if (typeId == (int)*g_TypeId_UIDescription) {
            if (fn_IsDerivedFrom(comp, *g_TypeId_UIDescription)) {
                result = comp;
            }
            break;
        }
    }

    *(void**)retval = result;
}

// ============================================================
// BuildComplete (0x140109390)
// ============================================================
// Returns true if build is complete: ResourceComponent.totalRequired * multiplier <= delivered
// posComp+0x18 = description (checked as ResourceComponent)
// ResourceComponent+0x30 = totalRequired
// posComp+0x4C = multiplier (build count), posComp+0x48 = delivered count
static bool BuildComplete_Impl(void* posComp)
{
    if (!posComp) return false;

    void* desc = *(void**)((uintptr_t)posComp + 0x18);
    bool isResource = fn_IsDerivedFrom(desc, *g_TypeId_ResourceComponent);
    void* resourceComp = isResource ? desc : nullptr;

    if (!resourceComp) return false;

    int totalRequired = *(int*)((uintptr_t)resourceComp + 0x30);
    int multiplier    = *(int*)((uintptr_t)posComp + 0x4C);
    int delivered     = *(int*)((uintptr_t)posComp + 0x48);

    return totalRequired * multiplier <= delivered;
}

// ============================================================
// EntityUI_SetEditMode (0x140102b60)
// ============================================================
// Sets the player controller's active tool. Walks a BST at +0x10
// (tool type registry) to find the matching tool entry for the
// given mode. Creates a new tool controller via Factory::CreateObject
// and optionally a ToolSetting, then activates the tool.
//
// PlayerController layout:
//   +0x10: tool registry BST root (nodes: +0x00=typeId(4), +0x04=pad,
//          +0x08=toolTypeId(4), +0x10=settingPtr(8), +0x18=parent(8),
//          +0x20=left(8), +0x28=right(8))
//   +0x20: current mode (uint64)
//   +0x28: active tool (InternalBasePtr)
//   +0x80: override flag (char)
//   +0x88: override mode (uint64)
//
// BST node layout (0x30 bytes, keyed by typeId at +0x00):
//   +0x00: typeId (uint32_t) — key for BST ordering
//   +0x08: toolTypeId (uint32_t) — passed to Factory::CreateObject
//   +0x10: settingPtr (void*) — cached ToolSetting object
//   +0x18: parent (node*), +0x20: left (node*), +0x28: right (node*)

void EntityUI_SetEditMode_Impl(void* playerController, uint64_t mode)
{
    uintptr_t pc = (uintptr_t)playerController;

    // If override flag is set and mode != 0, use stored override mode
    if (*(char*)(pc + 0x80) != 0 && mode != 0) {
        mode = *(uint64_t*)(pc + 0x88);
    }

    // Check if already in this mode with a valid tool
    uint64_t curMode = *(uint64_t*)(pc + 0x20);
    void* curTool = *(void**)(pc + 0x28);
    if (curMode == mode && curTool != nullptr && mode != 0)
        return;

    // Set new mode
    *(uint64_t*)(pc + 0x20) = mode;

    // Destroy existing tool if present
    if (*(void**)(pc + 0x28) != nullptr) {
        fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
        // Call vtable[5] (offset 0x28) = Shutdown on the tool
        void* tool = *(void**)(pc + 0x28);
        void** vtbl = *(void***)(tool);
        typedef void (*VCall_Shutdown_t)(void*);
        ((VCall_Shutdown_t)vtbl[5])(tool);
        // DeletePtr releases the object
        fn_InternalBasePtr_DeletePtr((void*)(pc + 0x28));
    }

    // Find the matching tool type in the BST at +0x10
    // Walk to the rightmost node first (find max for reverse iteration)
    uint32_t foundTypeId = 0;
    uint32_t* bstNode = *(uint32_t**)(pc + 0x10);
    uint32_t* foundNode = nullptr;

    if (bstNode == nullptr)
        goto create_tool;

    // Walk to rightmost leaf (last in-order)
    {
        uint32_t* node = bstNode;
        while (*(uint32_t**)(node + 8) != nullptr) {  // +0x20 = left child (8 = 0x20/4)
            node = *(uint32_t**)(node + 8);
        }

        // Reverse in-order traversal looking for tool matching mode
        uint32_t* cur = node;
        while (cur != nullptr) {
            // Check if this tool entry's description contains a component
            // matching mode's type
            uint64_t descMode = *(uint64_t*)(pc + 0x20);
            if (descMode != 0) {
                uint32_t nodeKey = *cur;
                int descCount = *(int*)(descMode + 0x30);
                void** descArray = *(void***)(descMode + 0x28);
                for (int i = 0; i < descCount; i++) {
                    void* comp = descArray[i];
                    void** cvtbl = *(void***)comp;
                    typedef int (*fn_GetTypeId_t)(void*);
                    int typeId = ((fn_GetTypeId_t)cvtbl[2])(comp);
                    if ((uint32_t)typeId == nodeKey) {
                        if (comp != nullptr) {
                            foundTypeId = nodeKey;
                        }
                        goto done_search;
                    }
                }
            }

            // Move to predecessor in BST
            if (cur == nullptr) break;
            uint32_t* rightChild = *(uint32_t**)(cur + 10);  // +0x28
            if (rightChild != nullptr) {
                // Go right, then left to the bottom
                uint32_t* next = rightChild;
                while (*(uint32_t**)(next + 8) != nullptr) {  // +0x20
                    next = *(uint32_t**)(next + 8);
                }
                cur = next;
            } else {
                // Go up until we come from a right child
                uint32_t* parent = *(uint32_t**)(cur + 6);  // +0x18
                if (parent != nullptr) {
                    if (*(uint32_t**)(parent + 8) != cur) {  // +0x20 = left != cur
                        // We came from left — find ancestor where we're from right
                        while (parent != nullptr && *(uint32_t**)(parent + 10) == cur) {
                            cur = parent;
                            parent = *(uint32_t**)(parent + 6);
                        }
                        cur = parent;
                    } else {
                        cur = parent;
                    }
                } else {
                    cur = nullptr;
                }
            }
        }
    }
done_search:

    // Find BST node matching foundTypeId
    {
        uint32_t* node = *(uint32_t**)(pc + 0x10);
        foundNode = nullptr;
        while (node != nullptr) {
            if (foundTypeId < *node) {
                node = *(uint32_t**)(node + 8);  // left
            } else if (foundTypeId <= *node) {
                foundNode = node;
                break;
            } else {
                node = *(uint32_t**)(node + 10);  // right
            }
        }
    }

create_tool:
    if (foundNode == nullptr) return;

    // Create tool controller via Factory::CreateObject
    {
        void* factory = *(void**)*g_pFactorySingleton;
        uint32_t toolTypeId = foundNode[2];  // +0x08
        void* newObj = fn_Factory_CreateObject(factory, toolTypeId);

        // IsDerivedFrom check for ToolController type
        bool isToolCtrl = fn_IsDerivedFrom(newObj, *g_TypeId_ToolController);
        void* toolCtrl = isToolCtrl ? newObj : nullptr;

        // Wrap in InternalBasePtr and store at +0x28
        uint64_t ibpBuf[1] = {0};
        fn_InternalBasePtr_Ctor(ibpBuf, toolCtrl);
        *(uint64_t*)(pc + 0x28) = ibpBuf[0];

        // If node's setting (+0x10) is null, create one via vtable call
        void* setting = *(void**)((uintptr_t)foundNode + 0x10);
        if (setting == 0) {
            fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
            void* tool = *(void**)(pc + 0x28);
            void** tvtbl = *(void***)tool;
            // vtable[11] (offset 0x58): GetSettingTypeId(this, retval) -> int*
            typedef int* (*VCall_GetSettingTypeId_t)(void*, void*);
            uint64_t settingBuf[1] = {0};
            int* settingResult = ((VCall_GetSettingTypeId_t)tvtbl[11])(tool, settingBuf);

            if (*settingResult != 0) {
                fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
                tool = *(void**)(pc + 0x28);
                tvtbl = *(void***)tool;
                settingBuf[0] = 0;
                int* sr2 = ((VCall_GetSettingTypeId_t)tvtbl[11])(tool, settingBuf);
                uint32_t settingTypeId = *(uint32_t*)sr2;

                void* settingObj = fn_Factory_CreateObject(factory, settingTypeId);
                bool isSetting = fn_IsDerivedFrom(settingObj, *g_TypeId_ToolSetting);
                void* typedSetting = isSetting ? settingObj : nullptr;

                uint64_t settIbp[1] = {0};
                fn_InternalBasePtr_Ctor(settIbp, typedSetting);
                *(uint64_t*)((uintptr_t)foundNode + 0x10) = settIbp[0];
            }
        }

        // Setup tool: tool+0x10=controller, tool+0x20=mode, tool+0x18=setting
        setting = *(void**)((uintptr_t)foundNode + 0x10);
        fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
        void* activeTool = *(void**)(pc + 0x28);
        uint64_t activeMode = *(uint64_t*)(pc + 0x20);
        *(uintptr_t*)((uintptr_t)activeTool + 0x10) = pc;
        *(uint64_t*)((uintptr_t)activeTool + 0x20) = activeMode;
        *(void**)((uintptr_t)activeTool + 0x18) = (void*)setting;

        // Call tool's vtable[4] (offset 0x20) = Startup
        fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
        activeTool = *(void**)(pc + 0x28);
        void** atVtbl = *(void***)activeTool;
        typedef void (*VCall_Startup_t)(void*);
        ((VCall_Startup_t)atVtbl[4])(activeTool);
    }
}

// ============================================================
// EntityUI_SelectEntity (0x140102e30)
// ============================================================
// Inserts or updates entity in the tracking AVL BST at
// playerController+0x60. Increments count at +0x68 on insert.
//
// BST node (0x38 bytes):
//   +0x00: key (uint64_t, entity InternalBasePtr value)
//   +0x08: x position (int32)
//   +0x0C: y position (int32)
//   +0x10: active flag (int32, set to 1)
//   +0x18: parent (node*)
//   +0x20: left child (node*)
//   +0x28: right child (node*)
//   +0x30: balance factor (int16)
//   +0x32: height (int16)
//
// NOTE: This function is used in both entityui.cpp and toolui.cpp.

void EntityUI_SelectEntity_Impl(void* playerController, void* entityPtr,
                                 uint32_t x, uint32_t y)
{
    uintptr_t pc = (uintptr_t)playerController;
    uint64_t entityKey = *(uint64_t*)entityPtr;
    uintptr_t* rootPtr = (uintptr_t*)(pc + 0x60);

    // Search BST for existing node with matching key
    uintptr_t node = *rootPtr;
    while (node != 0) {
        fn_InternalBasePtr_CheckPtr((void*)node);
        uint64_t nodeKey = *(uint64_t*)node;
        if (entityKey < nodeKey) {
            node = *(uintptr_t*)(node + 0x20);  // left
        } else {
            fn_InternalBasePtr_CheckPtr(entityPtr);
            if (entityKey <= nodeKey) {
                // Found — update position and active flag
                if (node != 0) {
                    *(uint8_t*)(node + 0x10) = 1;  // active = true
                    *(uint32_t*)(node + 0x08) = x;
                    *(uint32_t*)(node + 0x0C) = y;
                    return;
                }
                break;
            }
            node = *(uintptr_t*)(node + 0x28);  // right
        }
    }

    // Not found — insert new node
    // Prepare node data
    uint32_t activeFlag = 1;

    if (*rootPtr == 0) {
        // Empty tree — create root node
        uintptr_t* newNode = (uintptr_t*)fn_MemAlloc(g_pMemAllocSingleton, 0x38, 8);
        if (newNode != nullptr) {
            *(uint64_t*)newNode = 0;
            *(uint64_t*)newNode = entityKey;
            *(uint32_t*)((uintptr_t)newNode + 0x08) = x;
            *(uint32_t*)((uintptr_t)newNode + 0x0C) = y;
            *(uint32_t*)((uintptr_t)newNode + 0x10) = activeFlag;
            newNode[3] = 0;  // parent = null
            newNode[4] = 0;  // left = null
            newNode[5] = 0;  // right = null
            *(uint32_t*)((uintptr_t)newNode + 0x30) = 0;  // balance = 0
        }
        *rootPtr = (uintptr_t)newNode;
    } else {
        // Walk BST to find insertion point
        uintptr_t cur = *rootPtr;
        uintptr_t parent = 0;

        while (true) {
            parent = cur;
            fn_InternalBasePtr_CheckPtr((void*)cur);
            uint64_t curKey = *(uint64_t*)cur;

            if (curKey <= entityKey) {
                fn_InternalBasePtr_CheckPtr(entityPtr);
                if (entityKey <= curKey) {
                    // Duplicate key — should not happen, already handled above
                    return;
                }
                // Go right
                uintptr_t right = *(uintptr_t*)(cur + 0x28);
                if (right == 0) {
                    // Insert as right child
                    uintptr_t* newNode = (uintptr_t*)fn_MemAlloc(
                        g_pMemAllocSingleton, 0x38, 8);
                    if (newNode != nullptr) {
                        *(uint64_t*)newNode = 0;
                        *(uint64_t*)newNode = entityKey;
                        *(uint32_t*)((uintptr_t)newNode + 0x08) = x;
                        *(uint32_t*)((uintptr_t)newNode + 0x0C) = y;
                        *(uint32_t*)((uintptr_t)newNode + 0x10) = activeFlag;
                        newNode[4] = 0;   // left
                        newNode[5] = 0;   // right
                        *(uint32_t*)((uintptr_t)newNode + 0x30) = 0;
                        newNode[3] = parent;  // parent
                    }
                    *(uintptr_t*)(cur + 0x28) = (uintptr_t)newNode;
                    // Rebalance
                    uintptr_t nodeRef = (uintptr_t)newNode;
                    fn_AVL_Rebalance_0x38(rootPtr, &nodeRef);
                    break;
                }
                cur = right;
            } else {
                // Go left
                uintptr_t left = *(uintptr_t*)(cur + 0x20);
                if (left == 0) {
                    // Insert as left child
                    uintptr_t* newNode = (uintptr_t*)fn_MemAlloc(
                        g_pMemAllocSingleton, 0x38, 8);
                    if (newNode != nullptr) {
                        *(uint64_t*)newNode = 0;
                        *(uint64_t*)newNode = entityKey;
                        *(uint32_t*)((uintptr_t)newNode + 0x08) = x;
                        *(uint32_t*)((uintptr_t)newNode + 0x0C) = y;
                        *(uint32_t*)((uintptr_t)newNode + 0x10) = activeFlag;
                        newNode[4] = 0;   // left
                        newNode[5] = 0;   // right
                        *(uint32_t*)((uintptr_t)newNode + 0x30) = 0;
                        newNode[3] = parent;  // parent
                    }
                    *(uintptr_t*)(cur + 0x20) = (uintptr_t)newNode;
                    // Rebalance
                    uintptr_t nodeRef = (uintptr_t)newNode;
                    fn_AVL_Rebalance_0x38(rootPtr, &nodeRef);
                    break;
                }
                cur = left;
            }
        }
    }

    // Increment entity count
    *(int*)(pc + 0x68) = *(int*)(pc + 0x68) + 1;
}

// ============================================================
// EditTool_SelectEntity — NEEDS OPUS (0x1400fd3b0)
// ============================================================
// 100+ lines: iterates all dialogs finding ToolUI controllers,
// creates DialogInstance if entity doesn't have one yet.
// Kept as passthrough via fn_EditTool_SelectEntity.

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

    EntityTrackingSet_Remove_Impl(bstRoot, &entityKey);
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
            GetDescPage_Impl(entityDesc, &descResult);
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
            bool complete = BuildComplete_Impl(posComp);
            if (!complete) {
                pageIdx = 2;
            } else if (pageIdx != 0) {
                // Use entity description page or keep current page if pinned (page 4)
                void* entityDesc = *(void**)((uintptr_t)(*(void**)(s + 0x30)) + 0x28);
                void* descResult = nullptr;
                GetDescPage_Impl(entityDesc, &descResult);

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
// EntityUI::Slot8 (vtable +0x40) — Input handler
// ============================================================
// Reads mouse position from KeyMap, scales by dialog scale factor.
// Gets ToolUI controller, sets +0x4C = 1, syncs map visibility mode.
// On key 5 press: checks if mouse is over this dialog and entity exists,
// then selects the entity in the edit tool (creating one if needed).
//
// Original: 0x140066AD0.

void EntityUI_Slot8(EntityUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get KeyMap singleton (triple deref from exref: slot → External → KeyMap)
    void* keyMap = ***(void****)g_pKeyMapObjectExref;

    // Get dialog element from +0x18 -> +0x10 (dialog root element)
    void* dialogInst = *(void**)(s + 0x18);
    void* dialogElem = *(void**)((uintptr_t)dialogInst + 0x10);

    // Read mouse axes and scale by dialog scale factor (+0xDC on element)
    short axisX = fn_KeyMap_GetValue(keyMap, 0, 0);
    int mouseX = (int)((float)(int)axisX * *(float*)((uintptr_t)dialogElem + 0xDC));

    // Re-read element since CheckPtr pattern accesses +0x18 again
    dialogInst = *(void**)(s + 0x18);
    dialogElem = *(void**)((uintptr_t)dialogInst + 0x10);
    short axisY = fn_KeyMap_GetValue(keyMap, 0, 1);
    int mouseY = (int)((float)(int)axisY * *(float*)((uintptr_t)dialogElem + 0xDC));

    // Get ToolUI controller from dialog
    dialogInst = *(void**)(s + 0x18);
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
    uintptr_t toolPtr = *(uintptr_t*)toolBuf;

    // Set controller +0x4C = 1 (input active flag)
    *(uint8_t*)(toolPtr + 0x4C) = 1;

    // Sync map visibility mode: compare controller +0x40 with checkPin element +0xD8
    char checkVal = *(char*)(*(uintptr_t*)(s + 0x38) + 0xD8);
    char ctrlVal = *(char*)(toolPtr + 0x40);

    if (ctrlVal != checkVal) {
        *(char*)(*(uintptr_t*)(s + 0x38) + 0xD8) = ctrlVal;
    }

    // Check key 5 press (left mouse button / select)
    if (!fn_KeyMap_Pressed(keyMap, 0, 5))
        return;

    // Get dialog at mouse position
    dialogInst = *(void**)(s + 0x18);
    void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);

    uint8_t dialogAtPosBuf[16];
    fn_UI_GetFirstContainedDialog(userInterface, dialogAtPosBuf, mouseX, mouseY);
    void* dialogAtPos = *(void**)dialogAtPosBuf;

    // Check if the dialog at mouse position is this dialog, and entity exists
    dialogInst = *(void**)(s + 0x18);
    if (dialogAtPos != dialogInst || *(uintptr_t*)(s + 0x30) == 0)
        return;

    // Get current tool from player controller
    void* playerController = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
    void* currentTool = *(void**)((uintptr_t)playerController + 0x28);

    // Check if current tool is EditTool
    bool isEditTool = fn_IsDerivedFrom(currentTool, *g_TypeId_EditTool);
    void* editTool = isEditTool ? currentTool : nullptr;

    uint8_t editToolBuf[8];
    fn_InternalBasePtr_Ctor(editToolBuf, editTool);
    void* editToolPtr = *(void**)editToolBuf;

    if (!editToolPtr) {
        // No EditTool active — deselect first, then get new EditTool
        EntityUI_SetEditMode_Impl(playerController, 0);

        // Re-read current tool after deselect
        currentTool = *(void**)((uintptr_t)playerController + 0x28);
        isEditTool = fn_IsDerivedFrom(currentTool, *g_TypeId_EditTool);
        void* newEditTool = isEditTool ? currentTool : nullptr;

        uint8_t newBuf[8];
        fn_InternalBasePtr_Ctor(newBuf, newEditTool);
        editToolPtr = *(void**)newBuf;
    }

    // Select the entity in the edit tool
    if (editToolPtr) {
        void* entityPtr = *(void**)(s + 0x30);
        fn_EditTool_SelectEntity(editToolPtr, &entityPtr);
    }
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
        EntityUI_SetEditMode_Impl(playerController, 0);
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
            uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
            EntityTrackingSet_Remove_Impl(
                (void*)(worldObj + 0x50),
                &entityKey);
            return;
        }

        // If pinned, select entity in edit tool
        uint32_t dialogX = *(uint32_t*)(*(uintptr_t*)(s + 0x18) + 0x88);
        uint32_t dialogY = *(uint32_t*)(*(uintptr_t*)(s + 0x18) + 0x8c);
        void* entityKey = *(void**)(s + 0x30);
        EntityUI_SelectEntity_Impl(playerController, &entityKey, dialogX, dialogY);
        return;
    }
}
