// code/Game/Dialog/toolbardialog.cpp
// Clean reimplementation of ToolbarDialog game logic.
//
// ToolbarDialog (0x108 bytes) — main gameplay toolbar controller.
// Extends DialogInstance (0xB8 base).
//
// Field layout:
//   +0xB8: InternalBasePtr radioElement (toolbar radio element)
//   +0xC0: void* buttonElements (dynamic array of Element* ptrs, parallel to items)
//   +0xC8: int32 buttonCount (count for +0xC0 array)
//   +0xD0: void* toolbarItems (dynamic array of ToolbarItem* ptrs)
//   +0xD8: int32 itemCount (count for +0xD0 array)
//   +0xE0: uint64 toolbarGroupId (which toolbar group this dialog shows)
//   +0xE8: InternalBasePtr subDialog (child toolbar dialog instance)
//   +0xF0: int32 openSubIndex (-1 = none)
//   +0xF4: int32 currentSpeedIndex
//   +0xF8: void* speedValues (int32[] array)
//   +0x100: int32 speedValueCount
//
// Toolbar item fields (accessed via pointer from +0xD0 array):
//   +0x48 (int32): action type enum
//     0=Tool, 1=ShowGroup, 2=Utility, 3=ToggleAll, 4=FocusEntity,
//     5=CenterOnPoint, 6=SpeedDown, 7=Pause, 8=Unpause, 9=SpeedUp,
//     10=SpeedDisplay
//   +0x4C (int32): sub-parameter (e.g., point index for CenterOnPoint)
//   +0x58 (int64): action data (group ID, entity type, etc.)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// Static inline implementations (replaces fn_* exe pointer calls)
// ============================================================

// ToolbarDialog_CloseSubDialog (FUN_1400b9730)
// Closes the active sub-dialog if present, resets radio selection.
// self+0xE8 = InternalBasePtr to sub-dialog, self+0xF0 = open sub index, self+0xB8 = radio.
static void ToolbarDialog_CloseSubDialog_Impl(void* self) {
    uintptr_t s = (uintptr_t)self;
    uint64_t subDialogPtr = *(uint64_t*)(s + 0xE8);
    if (subDialogPtr != 0) {
        void* ui = *(void**)(s + 0x10);
        fn_UI_CloseDialog(ui, (void*)(s + 0xE8));
        // Null out the sub-dialog ptr
        uint8_t nullBuf[8];
        fn_InternalBasePtr_Ctor(nullBuf, nullptr);
        *(uint64_t*)(s + 0xE8) = *(uint64_t*)nullBuf;
        // Reset sub-dialog index to -1
        *(int32_t*)(s + 0xF0) = -1;
        // Reset radio selection
        fn_InternalBasePtr_CheckPtr((void*)(s + 0xB8));
        void* radioElem = *(void**)(s + 0xB8);
        fn_RadioElement_SetActiveRadio(radioElem, -1);
    }
}

// ToolbarDialog_ChangeSpeed (FUN_1400b91b0)
// Changes game speed by direction (+1 or -1), updates speed singleton.
// self+0xF8 = speed value array, self+0x100 = speed count, self+0xF4 = current index.
// Speed singleton at gameWorld+0x120 (+0x34 = current speed value).
static void ToolbarDialog_ChangeSpeed_Impl(void* self, int direction) {
    uintptr_t s = (uintptr_t)self;
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gameSys = *(uintptr_t*)((uintptr_t)gameWorld + 0x120);
    int32_t speedVal = *(int32_t*)(gameSys + 0x34);
    int32_t count = *(int32_t*)(s + 0x100);
    int32_t* speedArr = *(int32_t**)(s + 0xF8);
    // Find current speed in array
    int32_t idx = 0;
    for (int i = 0; i < count; i++) {
        if (speedArr[i] == speedVal) { idx = i; break; }
    }
    // Apply direction, clamp to [0, count-1]
    idx += direction;
    if (idx < 0) idx = 0;
    *(int32_t*)(s + 0xF4) = idx;
    if (count <= idx) {
        *(int32_t*)(s + 0xF4) = count - 1;
    }
    // Write new speed value
    *(int32_t*)(gameSys + 0x34) = speedArr[*(int32_t*)(s + 0xF4)];
}

// Toolbar_FindEntity (FUN_140088d40)
// BST search: walks tree at entityList+0x160 looking for typeId match.
// Returns InternalBasePtr to found entity, or null.
static void* Toolbar_FindEntity_Impl(void* entityList, void* retBuf, uint64_t typeId, int index) {
    uintptr_t* node = *(uintptr_t**)((uintptr_t)entityList + 0x160);
    while (node != nullptr) {
        if (*node > typeId) {
            node = (uintptr_t*)node[4];  // left child
        } else if (*node < typeId) {
            node = (uintptr_t*)node[5];  // right child
        } else {
            // Found matching typeId — fetch entity at index from node[1] array
            uint64_t entityArray = node[1];
            *(void**)retBuf = (void*)*(uint64_t*)(entityArray + (uint64_t)index * 8);
            return *(void**)retBuf;
        }
    }
    // Not found: return null
    fn_InternalBasePtr_Ctor(retBuf, nullptr);
    return nullptr;
}

// ============================================================
// ToolbarDialog_UpdateButtons_Impl (was fn_ToolbarDialog_UpdateButtons, FUN_1400b8fd0)
//
// Secondary update for ShowGroup (case 1) and FocusEntity (case 4) buttons.
// For each toolbar item:
//   - ShowGroup: enables button if the group has entities in the entity list BST
//     (checks item+0x58 type against entity count in BST at entityList+0x160)
//   - FocusEntity: enables button if entities of that type exist
// Uses the same entity BST search as Toolbar_FindEntity_Impl.
// ============================================================
static void ToolbarDialog_UpdateButtons_Impl(void* self) {
    uintptr_t s = (uintptr_t)self;
    uintptr_t entityList = *(uintptr_t*)((uintptr_t)*g_pGameWorldSingleton + 0xF0);

    int32_t itemCount = *(int32_t*)(s + 0xD8);
    if (itemCount <= 0) return;

    int64_t* itemArray = *(int64_t**)(s + 0xD0);

    for (int i = 0; i < itemCount; i++) {
        uintptr_t item = (uintptr_t)itemArray[i];
        uint64_t typeId = *(uint64_t*)(item + 0x58);

        if (typeId == 0) continue;

        // Search sub-items (item+0x58 -> data with +0x28 array, +0x30 count)
        // to find the matching component type for this toolbar item.
        // This traverses the toolbar item's typed sub-list.
        int32_t subCount = 0;
        uintptr_t subData = typeId;
        if (subData != 0) {
            subCount = *(int32_t*)(subData + 0x30);
        }

        // Find the component type matching g_TypeId_BuildRequirementComponent
        void* matchedComponent = nullptr;
        if (g_TypeId_BuildRequirementComponent && subCount > 0) {
            uint32_t targetType = *g_TypeId_BuildRequirementComponent;
            for (int j = 0; j < subCount; j++) {
                void* subItem = *(void**)(*(uintptr_t*)(subData + 0x28) + (int64_t)j * 8);
                if (!subItem) continue;
                // vtable[2] (offset 0x10) = GetTypeId, returns uint32_t
                typedef uint32_t (*GetTypeId_t)(void* self);
                uint32_t subType = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)subItem + 0x10)))(subItem);
                if (subType == targetType) {
                    bool isDerived = fn_IsDerivedFrom(subItem, targetType);
                    if (isDerived) matchedComponent = subItem;
                    break;
                }
            }
        }

        int32_t actionType = *(int32_t*)(item + 0x48);

        if (actionType == 1) {
            // ShowGroup: check if group count meets entity requirement
            bool enabled;
            if (matchedComponent == nullptr || *(int32_t*)((uintptr_t)matchedComponent + 0x60) < 0) {
                enabled = true;
            } else {
                // Search entity BST for typeId count
                int entityCount = 0;
                uintptr_t* node = *(uintptr_t**)(entityList + 0x160);
                while (node != nullptr) {
                    if (typeId < *node) {
                        node = (uintptr_t*)node[4];
                    } else if (typeId > *node) {
                        node = (uintptr_t*)node[5];
                    } else {
                        entityCount = (int)node[2];
                        break;
                    }
                }
                enabled = entityCount >= *(int32_t*)((uintptr_t)matchedComponent + 0x60);
            }

            // Set button enabled via vtable[8] (offset 0x40)
            void** buttonArray = *(void***)(s + 0xC0);
            fn_InternalBasePtr_CheckPtr((void*)&buttonArray[i]);
            void* elem = buttonArray[i];
            typedef void (*SetEnabled_t)(void* self, bool enabled);
            SetEnabled_t fn = (SetEnabled_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));
            fn(elem, enabled);
        }
        else if (actionType == 4) {
            // FocusEntity: check if any entities of that type exist
            void** buttonArray = *(void***)(s + 0xC0);
            fn_InternalBasePtr_CheckPtr((void*)&buttonArray[i]);
            void* elem = buttonArray[i];

            // Search entity BST for typeId count
            int entityCount = 0;
            uintptr_t* node = *(uintptr_t**)(entityList + 0x160);
            while (node != nullptr) {
                if (typeId < *node) {
                    node = (uintptr_t*)node[4];
                } else if (typeId > *node) {
                    node = (uintptr_t*)node[5];
                } else {
                    entityCount = (int)node[2];
                    break;
                }
            }

            bool enabled = entityCount > 0;
            typedef void (*SetEnabled_t)(void* self, bool enabled);
            SetEnabled_t fn = (SetEnabled_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));
            fn(elem, enabled);
        }
    }
}

// ============================================================
// Toolbar_ShowGroup_Impl (was fn_Toolbar_ShowGroup, FUN_140102b60)
//
// Shows/hides a tool group dialog. Called with gameSubsys (PlayerController at
// gameWorld+0x120) and a groupId (toolbar item type ID).
//
// Logic:
//   1. If lockToggle flag (+0x80) is set and groupId != 0, use locked group (+0x88)
//   2. If same group already shown AND controller exists AND groupId != 0: early return
//   3. Otherwise: store new groupId, close existing controller, find matching
//      tool node in BST, create ToolController + ToolSetting, configure & start
//
// PlayerController layout (relevant fields):
//   +0x10: BST root node (tool registry, red-black tree of ToolNode)
//   +0x20: int64_t activeGroupId (currently active tool group)
//   +0x28: InternalBasePtr activeController (current ToolController)
//   +0x80: uint8_t lockToggle (if set, forces locked group)
//   +0x88: int64_t lockedGroupId (the locked group to use)
//
// ToolNode (BST node, 0x30 bytes, red-black tree):
//   +0x00: uint32_t typeId (key)
//   +0x08: uint32_t factoryTypeId (passed to Factory::CreateObject)
//   +0x10: InternalBasePtr toolSetting (cached ToolSetting, lazily created)
//   +0x18: ToolNode* parent
//   +0x20: ToolNode* left
//   +0x28: ToolNode* right
//
// GroupData (pointed to by activeGroupId):
//   +0x28: void** subItems (array of component pointers)
//   +0x30: int32_t subItemCount
//
// ToolController vtable:
//   +0x20: Start()
//   +0x28: Close()
//   +0x58: GetToolSettingTypeId(retBuf) -> int* (returns ptr to typeId, 0 = none)
//
// ToolController instance fields (set before Start):
//   +0x10: void* playerController (back-pointer)
//   +0x18: InternalBasePtr toolSetting
//   +0x20: int64_t groupId
// ============================================================
static void Toolbar_ShowGroup_Impl(void* gameSubsys, int64_t groupId) {
    uintptr_t pc = (uintptr_t)gameSubsys;

    // Step 1: If lockToggle is set and groupId is non-zero, override with locked group
    if (*(uint8_t*)(pc + 0x80) != 0 && groupId != 0) {
        groupId = *(int64_t*)(pc + 0x88);
    }

    // Step 2: Early return if same group is already active with a live controller
    int64_t activeGroup = *(int64_t*)(pc + 0x20);
    if (activeGroup == groupId && *(int64_t*)(pc + 0x28) != 0 && groupId != 0) {
        return;
    }

    // Step 3: Store new group, close existing controller if any
    *(int64_t*)(pc + 0x20) = groupId;

    if (*(int64_t*)(pc + 0x28) != 0) {
        // CheckPtr on the InternalBasePtr
        fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
        // Call controller->Close() via vtable[5] (offset 0x28)
        void* controller = *(void**)(pc + 0x28);
        typedef void (*Close_t)(void* self);
        Close_t closeFn = (Close_t)(*(uintptr_t*)(*(uintptr_t*)controller + 0x28));
        closeFn(controller);
        // DeletePtr releases the reference
        fn_InternalBasePtr_DeletePtr((void*)(pc + 0x28));
    }

    // Step 4: In-order BST traversal to find the matching tool node typeId.
    // The BST at pc+0x10 maps typeIds to tool factory/setting info.
    // We scan all nodes, checking each node's typeId against the group data's
    // sub-item component list. The first match sets foundTypeId.
    uint32_t foundTypeId = 0;

    // Navigate to the in-order minimum (leftmost node)
    uintptr_t node = *(uintptr_t*)(pc + 0x10);
    if (node != 0) {
        // Go to leftmost
        while (*(uintptr_t*)(node + 0x20) != 0) {
            node = *(uintptr_t*)(node + 0x20);
        }

        // In-order traversal
        while (node != 0) {
            // Check if this node's typeId matches a sub-item in the group data
            int64_t groupData = *(int64_t*)(pc + 0x20);
            if (groupData != 0) {
                uint32_t nodeTypeId = *(uint32_t*)node;
                int32_t subCount = *(int32_t*)(groupData + 0x30);
                for (int i = 0; i < subCount; i++) {
                    void* subItem = *(void**)(*(uintptr_t*)(groupData + 0x28) + (uintptr_t)i * 8);
                    // Call vtable[2] (GetTypeId) on the sub-item
                    typedef uint32_t (*GetTypeId_t)(void* self);
                    uint32_t subTypeId = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)subItem + 0x10)))(subItem);
                    if (subTypeId == nodeTypeId) {
                        // Verify sub-item pointer is non-null (redundant but matches original)
                        if (*(uintptr_t*)(*(uintptr_t*)(groupData + 0x28) + (uintptr_t)i * 8) != 0) {
                            foundTypeId = nodeTypeId;
                        }
                        break;
                    }
                }
            }

            // In-order successor: if right child exists, go to its leftmost descendant
            uintptr_t right = *(uintptr_t*)(node + 0x28);
            if (right != 0) {
                node = right;
                while (*(uintptr_t*)(node + 0x20) != 0) {
                    node = *(uintptr_t*)(node + 0x20);
                }
            } else {
                // Walk up to find ancestor where we came from the left
                uintptr_t parent = *(uintptr_t*)(node + 0x18);
                if (parent != 0) {
                    if (*(uintptr_t*)(parent + 0x20) == node) {
                        // We are the left child — parent is successor
                        node = parent;
                    } else {
                        // We are the right child — walk up
                        while (parent != 0 && *(uintptr_t*)(parent + 0x28) == node) {
                            node = parent;
                            parent = *(uintptr_t*)(parent + 0x18);
                        }
                        node = *(uintptr_t*)(node + 0x18);
                    }
                } else {
                    // No parent — we were at root, done
                    node = 0;
                }
            }
        }
    }

    // Step 5: BST search for the foundTypeId to get the matching ToolNode
    uintptr_t foundNode = 0;
    uintptr_t searchNode = *(uintptr_t*)(pc + 0x10);
    while (searchNode != 0) {
        uint32_t key = *(uint32_t*)searchNode;
        if (foundTypeId < key) {
            searchNode = *(uintptr_t*)(searchNode + 0x20); // left
        } else if (foundTypeId > key) {
            searchNode = *(uintptr_t*)(searchNode + 0x28); // right
        } else {
            foundNode = searchNode;
            break;
        }
    }
    // If not found exactly, foundNode stays 0 — but the original code uses
    // the last "less-or-equal" node (lower_bound). Match that behavior:
    if (foundNode == 0) {
        // Re-do search tracking lower_bound (last node where key <= foundTypeId)
        searchNode = *(uintptr_t*)(pc + 0x10);
        while (searchNode != 0) {
            uint32_t key = *(uint32_t*)searchNode;
            if (foundTypeId < key) {
                searchNode = *(uintptr_t*)(searchNode + 0x20);
            } else {
                foundNode = searchNode;
                if (foundTypeId <= key) break;
                searchNode = *(uintptr_t*)(searchNode + 0x28);
            }
        }
    }

    // Step 6: Create ToolController via Factory::CreateObject
    uint32_t factoryTypeId = *(uint32_t*)(foundNode + 0x08);
    void* newObj = fn_Factory_CreateObject(*g_pFactorySingleton, factoryTypeId);

    // IsDerivedFrom check for ToolController type
    bool isToolCtrl = fn_IsDerivedFrom(newObj, *g_TypeId_ToolController);
    void* toolController = isToolCtrl ? newObj : nullptr;

    // Store as InternalBasePtr at pc+0x28
    uint64_t ctrlRef = 0;
    fn_InternalBasePtr_Ctor(&ctrlRef, toolController);
    *(uint64_t*)(pc + 0x28) = ctrlRef;

    // Step 7: Create ToolSetting if needed (lazy init on the ToolNode)
    if (*(int64_t*)(foundNode + 0x10) == 0) {
        // Ask the controller for its tool setting type via vtable[11] (offset 0x58)
        fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
        void* ctrl = *(void**)(pc + 0x28);

        typedef int* (*GetSettingTypeId_t)(void* self, void* retBuf);
        uint64_t settingBuf = 0;
        GetSettingTypeId_t getSettingFn = (GetSettingTypeId_t)(*(uintptr_t*)(*(uintptr_t*)ctrl + 0x58));
        int* settingTypePtr = getSettingFn(ctrl, &settingBuf);

        if (*settingTypePtr != 0) {
            // Re-query (matches original's double-call pattern)
            fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
            ctrl = *(void**)(pc + 0x28);
            getSettingFn = (GetSettingTypeId_t)(*(uintptr_t*)(*(uintptr_t*)ctrl + 0x58));
            uint32_t* typeIdPtr = (uint32_t*)getSettingFn(ctrl, &settingBuf);
            uint32_t settingFactoryId = *typeIdPtr;

            // Create the ToolSetting object
            void* settingObj = fn_Factory_CreateObject(*g_pFactorySingleton, settingFactoryId);
            bool isToolSetting = fn_IsDerivedFrom(settingObj, *g_TypeId_ToolSetting);
            void* toolSetting = isToolSetting ? settingObj : nullptr;

            uint64_t settingRef = 0;
            fn_InternalBasePtr_Ctor(&settingRef, toolSetting);
            *(uint64_t*)(foundNode + 0x10) = settingRef;
        }
    }

    // Step 8: Configure and start the controller
    uint64_t toolSettingRef = *(uint64_t*)(foundNode + 0x10);
    fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
    void* ctrl = *(void**)(pc + 0x28);

    // Set controller fields
    uintptr_t c = (uintptr_t)ctrl;
    *(uintptr_t*)(c + 0x10) = pc;                             // back-pointer to PlayerController
    *(int64_t*)(c + 0x20) = *(int64_t*)(pc + 0x20);          // groupId
    *(uint64_t*)(c + 0x18) = toolSettingRef;                  // ToolSetting ref

    // Call controller->Start() via vtable[4] (offset 0x20)
    fn_InternalBasePtr_CheckPtr((void*)(pc + 0x28));
    ctrl = *(void**)(pc + 0x28);
    typedef void (*Start_t)(void* self);
    Start_t startFn = (Start_t)(*(uintptr_t*)(*(uintptr_t*)ctrl + 0x20));
    startFn(ctrl);
}

// ============================================================
// Toolbar_Utility_Impl (was fn_Toolbar_Utility, FUN_140101a80)
//
// Opens/closes a utility dialog (e.g., town hall, trade post UI).
//
// gameSubsys = ToolController at gameWorld+0x120
// retBuf     = output InternalBasePtr (hidden return ptr)
// param      = pointer to Description object for this utility type
//
// The Description object (param) has:
//   +0x28: pointer to array of sub-item ptrs (Description children)
//   +0x30: int32 count of sub-items
// One child will be a UIDescription (g_TypeId_UIDescription),
// which has the external resource ref at +0x28.
//
// The BST at gameSubsys+0x50 tracks utility dialog positions:
//   Node layout (0x38 bytes):
//     +0x00: uint64 key (= param, the Description ptr value)
//     +0x08: int32 saved X position
//     +0x0C: int32 saved Y position
//     +0x10: uint8 isOpen flag
//     +0x20: left child ptr
//     +0x28: right child ptr
//
// Flow:
//   1. FindDialogByControllerType — search open dialogs for one
//      whose controller is a ToolUI with +0x30 == param
//   2. If found: close the dialog, clear BST isOpen flag
//   3. If not found: find UIDescription child, create DialogInstance,
//      set ToolUI vtable, look up/insert in BST, position, add to UI
// ============================================================
static void Toolbar_Utility_Impl(void* gameSubsys, void* retBuf, uint64_t param) {
    uintptr_t self = (uintptr_t)gameSubsys;

    // Get the UserInterface singleton: *(gameWorld + 0x98)
    void* gameWorld = *g_pGameWorldSingleton;
    void* userInterface = *(void**)((uintptr_t)gameWorld + 0x98);

    // Step 1: Check if a dialog with this controller type is already open
    void* existingDialog = nullptr;
    fn_FindDialogByControllerType(gameSubsys, &existingDialog, param);

    if (existingDialog != nullptr) {
        // Dialog is already open — close it
        uint64_t closeRef = (uint64_t)existingDialog;
        fn_UI_CloseDialog(userInterface, &closeRef);

        // Search BST at self+0x50 and clear the isOpen flag
        uintptr_t* node = *(uintptr_t**)(self + 0x50);
        while (node != nullptr) {
            uint64_t nodeKey = *(uint64_t*)node;
            if (param < nodeKey) {
                node = (uintptr_t*)node[4];  // left child (+0x20)
            } else if (param > nodeKey) {
                node = (uintptr_t*)node[5];  // right child (+0x28)
            } else {
                // Found — clear the isOpen flag at +0x10
                *(uint8_t*)(node + 2) = 0;
                break;
            }
        }

        // Return null InternalBasePtr
        fn_InternalBasePtr_Ctor(retBuf, nullptr);
        return;
    }

    // Step 2: No existing dialog — find the UIDescription sub-item
    int32_t subCount = *(int32_t*)(param + 0x30);
    void* uiDescItem = nullptr;

    for (int i = 0; i < subCount; i++) {
        void* subItem = *(void**)(*(uintptr_t*)(param + 0x28) + (int64_t)i * 8);
        if (!subItem) continue;

        // Call vtable[2] (+0x10) to get the type ID
        typedef uint32_t (*GetTypeId_t)(void* obj);
        uint32_t subType = ((GetTypeId_t)(*(uintptr_t*)(*(uintptr_t*)subItem + 0x10)))(subItem);

        if (subType == *g_TypeId_UIDescription) {
            bool isDerived = fn_IsDerivedFrom(subItem, *g_TypeId_UIDescription);
            if (isDerived) {
                uiDescItem = subItem;
            }
            break;
        }
    }

    if (uiDescItem == nullptr) {
        fn_InternalBasePtr_Ctor(retBuf, nullptr);
        return;
    }

    // Step 3: Create a new DialogInstance from the UIDescription's resource
    void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    void* dialogInst = nullptr;

    if (newBuf != nullptr) {
        // Null parent InternalBasePtr
        uint8_t nullParent[8];
        fn_InternalBasePtr_Ctor(nullParent, nullptr);

        // External resource from uiDescItem+0x28
        void* extRes = *(void**)((uintptr_t)uiDescItem + 0x28);

        // DialogInstance ctor: (self, ui, extRes, parentPtr, dataSource, extraPtr, flags)
        // param is passed as the data source (identifies the utility type)
        dialogInst = fn_DialogInstance_Ctor(newBuf, userInterface, extRes,
                                            nullParent, (void*)param, nullptr, 0);

        // Override vtable to ToolUI child dialog vtable
        *(uintptr_t*)dialogInst = g_ToolUI_ChildDialog_Vtable;
    }

    // Wrap in InternalBasePtr
    void* dialogRef = nullptr;
    fn_InternalBasePtr_Ctor(&dialogRef, dialogInst);

    if (dialogRef == nullptr) {
        fn_InternalBasePtr_Ctor(retBuf, nullptr);
        return;
    }

    // Step 4: Look up position in BST at self+0x50
    uintptr_t* bstNode = nullptr;
    uintptr_t* bstRoot = (uintptr_t*)(self + 0x50);

    // BST search: find existing node for this param key
    uintptr_t* current = *(uintptr_t**)bstRoot;
    while (current != nullptr) {
        uint64_t nodeKey = *(uint64_t*)current;
        if (param < nodeKey) {
            current = (uintptr_t*)current[4];  // left child (+0x20)
        } else if (param > nodeKey) {
            current = (uintptr_t*)current[5];  // right child (+0x28)
        } else {
            bstNode = current;
            break;
        }
    }

    if (bstNode == nullptr) {
        // Not in BST — insert new node via fn_BST_Insert_0x14
        // Insert data: key=param, position=(default 1,0), flags=0
        uint8_t insertBuf[24];
        *(uint64_t*)&insertBuf[0] = param;          // key
        *(uint64_t*)&insertBuf[8] = 0;              // x=0, y=0
        *(uint64_t*)&insertBuf[16] = 0;             // flags=0

        void* retNode = nullptr;
        fn_BST_Insert_0x14(bstRoot, &retNode, insertBuf);
        bstNode = (uintptr_t*)retNode;
    }

    // Set the isOpen flag at +0x10 to 1
    *(uint8_t*)((uintptr_t)bstNode + 0x10) = 1;

    // Position dialog at saved coordinates from the BST node
    fn_InternalBasePtr_CheckPtr(&dialogRef);
    int32_t savedX = *(int32_t*)((uintptr_t)bstNode + 0x08);
    int32_t savedY = *(int32_t*)((uintptr_t)bstNode + 0x0C);
    fn_DialogInstance_SetUpperLeft((void*)(uintptr_t)dialogRef, savedX, savedY, false);

    // Add dialog to the UI
    uint64_t addRef = (uint64_t)dialogRef;
    fn_UI_AddDialog(userInterface, &addRef);

    // Return the dialog reference
    *(void**)retBuf = (void*)(uintptr_t)dialogRef;
}

// ============================================================
// Toolbar_ToggleAll_Impl (was fn_Toolbar_ToggleAll, FUN_140101ce0)
//
// "Toggle All" button handler: closes all open tool groups, creates
// the pause/overview dialog.
//
// Flow:
//   1. If tool controller at +0x28 is active, call its Close (vtable[9])
//   2. Call ShowGroup(gameSubsys, 0) to clear current group
//   3. Recompute camera view transform and apply to camera
//   4. Load "Dialog/Pause.rsc" via ExternalFactory::FindOrLoad
//   5. IsDerivedFrom check on loaded resource (DialogInstance type)
//   6. MemAlloc 0xC8 bytes for PauseDialog
//   7. Construct DialogInstance base, set PauseDialog vtable, set gameWorld ptr
//   8. Add dialog to UI
//
// gameSubsys layout (ToolController subsystem at gameWorld+0x120):
//   +0x28: InternalBasePtr — current active tool controller
//
// PauseDialog layout (0xC8 bytes, extends DialogInstance 0xB8):
//   +0x00: vtable (PTR_FUN_14015f550)
//   +0xB8: gameWorld pointer (DAT_1401c3dd8)
//   +0xC0: 0 (unused/flags)
// ============================================================
static void Toolbar_ToggleAll_Impl(void* gameSubsys) {
    uintptr_t gs = (uintptr_t)gameSubsys;

    // Get UserInterface from gameWorld+0x98
    void* gameWorld = *g_pGameWorldSingleton;
    void* userInterface = *(void**)((uintptr_t)gameWorld + 0x98);

    // Step 1: If there's an active tool controller at +0x28, close it via vtable[9]
    if (*(uintptr_t*)(gs + 0x28) != 0) {
        fn_InternalBasePtr_CheckPtr((void*)(gs + 0x28));
        void* toolCtrl = *(void**)(gs + 0x28);
        // vtable[9] = Close (offset 0x48)
        typedef void (*VClose_t)(void* self);
        VClose_t vClose = (VClose_t)(*(uintptr_t*)(*(uintptr_t*)toolCtrl + 0x48));
        vClose(toolCtrl);
    }

    // Step 2: Clear current tool group by calling ShowGroup(gameSubsys, 0)
    Toolbar_ShowGroup_Impl(gameSubsys, 0);

    // Step 3: Recompute camera view transform
    // Time::Time(0.0f) — constructs a zero time value (unused, but matches original)
    uint8_t timeVal[16];
    if (fn_Time_Ctor_Float) {
        fn_Time_Ctor_Float(timeVal, 0.0f);
    }

    // Get camera system at gameWorld+0xE0
    void* cameraSys = *(void**)((uintptr_t)gameWorld + 0xE0);

    // CameraSys_ComputeViewTransform: computes look-at transform from camera params
    // Returns a compact Transform (7 floats = 0x1C bytes) written to outBuf and cameraSys[0..6]
    uint8_t transformBuf[32]; // enough for 7 dwords
    if (fn_CameraSys_ComputeViewTransform) {
        void* result = fn_CameraSys_ComputeViewTransform(cameraSys, transformBuf);

        // Convert compact Transform to Transform3 (48 bytes) and set on camera
        if (fn_Transform_AsTransform3 && fn_Camera_SetCameraToWorld) {
            uint8_t transform3[48];
            fn_Transform_AsTransform3(result, transform3);
            // Camera object is at cameraSys+0x34
            fn_Camera_SetCameraToWorld((void*)((uintptr_t)cameraSys + 0x34), transform3);
        }
    }

    // Step 4: Load "Dialog/Pause.rsc" resource
    uint8_t filename[16]; // System::Filename is 8-16 bytes
    if (fn_Filename_CtorCStr) {
        fn_Filename_CtorCStr(filename, "Dialog/Pause.rsc");
    }

    void* externalRes = nullptr;
    if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton && fn_ExternalFactory_FindOrLoad) {
        externalRes = fn_ExternalFactory_FindOrLoad(*g_pExternalFactorySingleton, filename);
    }

    // Step 5: IsDerivedFrom check — verify the loaded resource is a DialogInstance type
    void* checkedRes = nullptr;
    if (externalRes != nullptr) {
        bool isDerived = fn_IsDerivedFrom(externalRes, *g_TypeId_DialogInstance);
        if (isDerived) {
            checkedRes = externalRes;
        }
    }

    // Clean up filename
    if (fn_Filename_Dtor) {
        fn_Filename_Dtor(filename);
    }

    // Step 6: Allocate PauseDialog (0xC8 bytes)
    void* pauseDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    void* finalDialog = nullptr;

    uintptr_t gameWorldPtr = (uintptr_t)gameWorld;

    if (pauseDialog != nullptr) {
        // Construct null parent InternalBasePtr
        uint64_t parentRef;
        fn_InternalBasePtr_Ctor(&parentRef, nullptr);
        uint64_t parentCopy = parentRef;

        // Step 7: Construct DialogInstance base class
        // DialogInstance::DialogInstance(self, userInterface, externalRes, parentPtr, 0, 0, 0)
        fn_DialogInstance_Ctor(pauseDialog, userInterface, checkedRes, &parentCopy, 0, 0, 0);

        // Set PauseDialog vtable
        *(uintptr_t*)pauseDialog = g_PauseDialog_Vtable;
        // Set gameWorld pointer at +0xB8
        *(uintptr_t*)((uintptr_t)pauseDialog + 0xB8) = gameWorldPtr;
        // Zero out +0xC0
        *(uint64_t*)((uintptr_t)pauseDialog + 0xC0) = 0;

        finalDialog = pauseDialog;
    }

    // Step 8: Wrap in InternalBasePtr and add to UI
    uint64_t dialogRef;
    fn_InternalBasePtr_Ctor(&dialogRef, finalDialog);
    uint64_t addRef = dialogRef;
    fn_UI_AddDialog(userInterface, &addRef);
}

// ============================================================
// Helper: push_back for engine dynamic arrays (ptr, count at +0, capacity at +4)
// Mimics the inlined vector growth pattern seen throughout the exe.
// ============================================================
static void DynArray8_Push(void** pData, int32_t* pCount, int32_t* pCapacity, uint64_t value) {
    int32_t oldCount = *pCount;
    int32_t newCount = oldCount + 1;
    if (newCount == oldCount) return; // overflow guard (matches original)

    *pCount = newCount;
    void* oldData = *pData;

    if (*pCapacity < newCount) {
        int32_t newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            newCap = newCount * 3;
        }
        *pCapacity = newCap;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)newCap * 8, 8);
        *pData = newData;
    }

    // Copy existing elements to new buffer (if buffer changed)
    int32_t copyStart = (oldData == *pData) ? oldCount : 0;
    uint64_t* dst = (uint64_t*)*pData;
    uint64_t* src = (uint64_t*)oldData;
    for (int32_t i = copyStart; i < newCount; i++) {
        if (i < oldCount) {
            dst[i] = src[i];
        } else {
            dst[i] = value; // new element
        }
    }

    if (oldData != *pData && oldData != nullptr) {
        fn_MemFreeStatic(oldData);
    }
}

static void DynArray4_Push(void** pData, int32_t* pCount, int32_t* pCapacity, int32_t value) {
    int32_t oldCount = *pCount;
    int32_t newCount = oldCount + 1;
    if (newCount == oldCount) return;

    *pCount = newCount;
    void* oldData = *pData;

    if (*pCapacity < newCount) {
        int32_t newCap = newCount;
        if (newCount == oldCount + 1 && oldCount != 0) {
            newCap = newCount * 3;
        }
        *pCapacity = newCap;
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)newCap * 4, 4);
        *pData = newData;
    }

    int32_t copyStart = (oldData == *pData) ? oldCount : 0;
    int32_t* dst = (int32_t*)*pData;
    int32_t* src = (int32_t*)oldData;
    for (int32_t i = copyStart; i < newCount; i++) {
        if (i < oldCount) {
            dst[i] = src[i];
        } else {
            dst[i] = value;
        }
    }

    if (oldData != *pData && oldData != nullptr) {
        fn_MemFreeStatic(oldData);
    }
}

// ============================================================
// ToolbarDialog_Ctor_Impl (was fn_ToolbarDialog_Ctor, FUN_1400b9910)
//
// Constructs a ToolbarDialog instance. This is the main constructor that:
//   1. Calls base DialogInstance ctor
//   2. Patches vtable to ToolbarDialog vtable
//   3. Iterates all Toolbar externals matching the group ID
//   4. Sorts them by priority (+0x50 field)
//   5. Determines icon sizes based on nesting depth
//   6. For each toolbar item, clones a UI template element, sets up
//      name, tooltip, sprite, keyboard shortcut, and adds to table
//   7. Calls RadioElement::FindCheckBoxes to wire up radio behavior
//   8. Populates the speed values array {1, 2, 5, 10}
//   9. Finds current game speed index in the array
//
// Parameters:
//   self          — uninitialized 0x108-byte allocation
//   userInterface — UserInterface* (UI system)
//   dialog        — Dialog* (dialog resource with templates)
//   parentPtr     — InternalBasePtr* to parent dialog (deref'd to get Base*)
//   toolbarData   — toolbar group ID (uint64_t cast to void*)
//
// Returns: self (constructed ToolbarDialog)
// ============================================================
void* ToolbarDialog_Ctor_Impl(void* self, void* userInterface, void* dialog,
                               void* parentPtr, void* toolbarData) {
    uintptr_t s = (uintptr_t)self;
    uint64_t groupId = (uint64_t)toolbarData;

    // --- Step 1: Call base DialogInstance constructor ---
    fn_DialogInstance_Ctor(self, userInterface, dialog, parentPtr, nullptr, nullptr, 0);

    // --- Step 2: Patch vtable to ToolbarDialog ---
    *(uintptr_t*)s = g_ToolbarDialog_Vtable;

    // --- Step 3: Initialize ToolbarDialog-specific fields ---
    *(uint64_t*)(s + 0xB8) = 0;         // radioElement
    *(uint64_t*)(s + 0xC0) = 0;         // buttonElements data
    *(uint64_t*)(s + 0xC8) = 0;         // buttonCount + capacity (two int32s)
    *(uint64_t*)(s + 0xD0) = 0;         // toolbarItems data
    *(uint64_t*)(s + 0xD8) = 0;         // itemCount + capacity
    *(uint64_t*)(s + 0xE0) = groupId;   // toolbar group ID
    fn_InternalBasePtr_Ctor((void*)(s + 0xE8), nullptr); // subDialog = null
    *(int32_t*)(s + 0xF0) = -1;         // openSubIndex = -1
    *(uint64_t*)(s + 0xF8) = 0;         // speedValues data
    *(uint64_t*)(s + 0x100) = 0;        // speedValueCount + capacity

    // --- Step 4: Find key UI elements ---
    // Find "toolbarContent" table element
    uint8_t strBuf[16];
    uint8_t elemBuf[16];

    fn_String_Ctor_CStr(strBuf, "toolbarContent");
    void* retPtr = fn_DialogInstance_FindElement(self, elemBuf, strBuf);
    void* tableElement = *(void**)retPtr;
    fn_String_Dtor(strBuf);

    // Find "toolBar" radio element
    fn_String_Ctor_CStr(strBuf, "toolBar");
    retPtr = fn_DialogInstance_FindElement(self, elemBuf, strBuf);
    *(uint64_t*)(s + 0xB8) = *(uint64_t*)retPtr;
    fn_String_Dtor(strBuf);

    // Set radio element ID to 1000
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xB8));
    fn_Element_SetElementId(*(void**)(s + 0xB8), 1000);

    // --- Step 5: Collect all Toolbar items matching our group ID ---
    // Use ExternalFactory::FindNext to iterate all Toolbar externals
    void* factory = *g_pExternalFactorySingleton;
    uint32_t toolbarTypeId = *g_TypeId_ToolbarItem;

    // Temporary dynamic array for matching toolbar items
    void* itemList = nullptr;
    int32_t itemListCount = 0;
    int32_t itemListCap = 0;

    void* ext = fn_ExternalFactory_FindNext(factory, toolbarTypeId, nullptr);
    while (ext != nullptr) {
        // IsDerivedFrom check
        bool isDerived = fn_IsDerivedFrom(ext, toolbarTypeId);
        void* toolbarItem = isDerived ? ext : nullptr;

        // Check: item must have name (byte at +0x45 != 0) and group must match
        if (toolbarItem != nullptr) {
            uint8_t hasName = *(uint8_t*)((uintptr_t)toolbarItem + 0x45);
            uint64_t itemGroup = *(uint64_t*)((uintptr_t)toolbarItem + 0x60);
            if (hasName != 0 && itemGroup == groupId) {
                DynArray8_Push(&itemList, &itemListCount, &itemListCap,
                               (uint64_t)toolbarItem);
            }
        }

        ext = fn_ExternalFactory_FindNext(factory, toolbarTypeId, ext);
    }

    // Sort by priority field at +0x50
    if (itemListCount > 1) {
        fn_ToolbarSort(itemList, itemListCount);
    }

    // --- Step 6: Determine icon sizes based on nesting depth ---
    // Walk linked list at first item's +0x60 chain to count depth
    int depth = 0;
    if (itemListCount > 0) {
        uint64_t* items = (uint64_t*)itemList;
        uint64_t firstItem = items[0];
        for (uint64_t link = *(uint64_t*)(firstItem + 0x60);
             link != 0;
             link = *(uint64_t*)(link + 0x60)) {
            depth++;
        }
    }

    // Map depth to keyboard shortcut ID range
    int shortcutStart, shortcutEnd;
    if (depth == 0) {
        shortcutStart = 0x18; shortcutEnd = 0x20;
    } else if (depth == 1) {
        shortcutStart = 0x23; shortcutEnd = 0x2c;
    } else if (depth == 2) {
        shortcutStart = 0x2d; shortcutEnd = 0x31;
    } else {
        shortcutStart = 0; shortcutEnd = -1; // no shortcuts
    }

    // Build array of available shortcut IDs
    int32_t* shortcutIds = nullptr;
    int32_t shortcutCount = 0;
    int32_t shortcutCap = 0;
    for (int id = shortcutStart; id <= shortcutEnd; id++) {
        DynArray4_Push((void**)&shortcutIds, &shortcutCount, &shortcutCap, id);
    }

    // --- Step 7: Create UI elements for each toolbar item ---
    int buttonIndex = 0;
    for (int i = 0; i < itemListCount; i++) {
        uint64_t* items = (uint64_t*)itemList;
        uintptr_t item = (uintptr_t)items[i];

        // Determine template index based on action type
        int32_t actionType = *(int32_t*)(item + 0x48);
        int templateIndex;
        if (actionType == 10) {
            templateIndex = 0;  // SpeedDisplay: plain text template
        } else if (actionType == 0) {
            templateIndex = 1;  // Tool/group: expandable button
        } else if (actionType == 1) {
            templateIndex = 2;  // ShowGroup: standard button
        } else {
            templateIndex = 3;  // Other: utility button
        }

        // Get template element from Dialog and clone it
        uint8_t templateBuf[16];
        fn_Dialog_GetTemplate(dialog, templateBuf, templateIndex);
        void* templateElem = *(void**)templateBuf;

        // Clone the template: virtual call at vtable[8] (offset 0x40)
        // Element::Clone(this, retBuf, dialog) — member, non-trivial return
        typedef void* (*CloneFn)(void* self, void* retBuf, void* dialog);
        uint8_t cloneBuf[16];
        uintptr_t vtable = *(uintptr_t*)templateElem;
        CloneFn cloneFn = (CloneFn)(*(uintptr_t*)(vtable + 0x40));
        cloneFn(templateElem, cloneBuf, dialog);
        void* newElement = *(void**)cloneBuf;

        // Push to buttonElements array (+0xC0/+0xC8/+0xCC)
        DynArray8_Push(
            (void**)(s + 0xC0),
            (int32_t*)(s + 0xC8),
            (int32_t*)(s + 0xCC),
            (uint64_t)newElement
        );

        // Push to toolbarItems array (+0xD0/+0xD8/+0xDC)
        DynArray8_Push(
            (void**)(s + 0xD0),
            (int32_t*)(s + 0xD8),
            (int32_t*)(s + 0xDC),
            (uint64_t)item
        );

        // Add element as child of table at current buttonIndex
        fn_InternalBasePtr_CheckPtr((void*)&tableElement);
        void* elemPtr = newElement;
        fn_TableElement_SetChild2(tableElement, &elemPtr, buttonIndex);

        // Set element name from toolbar item's resource path
        // Set element name from toolbar item's resource path
        uint8_t pathBuf[8];
        fn_External_GetPath((void*)item, pathBuf);
        uint8_t pathIdBuf[8];
        fn_Filename_GetPathIdentifier(pathBuf, pathIdBuf);
        fn_InternalBasePtr_CheckPtr((void*)&newElement);
        fn_Element_SetName(newElement, pathIdBuf);
        fn_Filename_Dtor(pathIdBuf);
        fn_Filename_Dtor(pathBuf);

        // Get toolbar item's display string and sprite
        uint8_t uiString[16];
        uint8_t uiSprite[16];
        fn_UIString_Ctor(uiString, *(uint64_t*)(item + 0x20), (void*)(item + 0x30));
        fn_Sprite_Ctor(uiSprite, *(uint64_t*)(item + 0x28), (void*)(item + 0x38));

        // --- Configure button element (skip for SpeedDisplay) ---
        if (templateIndex != 0) {
            // Cast element to ButtonElement via IsDerivedFrom
            fn_InternalBasePtr_CheckPtr((void*)&newElement);
            void* elemForCast = newElement;
            bool isButton = fn_IsDerivedFrom(elemForCast, *g_TypeId_HelpButtonElem);
            void* buttonElem = isButton ? elemForCast : nullptr;

            uint64_t buttonRef;
            fn_InternalBasePtr_Ctor(&buttonRef, buttonElem);

            // For ShowGroup (1) and Utility (2), override string/sprite from
            // the item's entity description (BuildRequirementComponent)
            if (actionType == 1 || actionType == 2) {
                uint64_t entityDesc = *(uint64_t*)(item + 0x58);
                void* buildReqComp = nullptr;
                if (entityDesc != 0) {
                    int32_t descCount = *(int32_t*)(entityDesc + 0x30);
                    for (int j = 0; j < descCount; j++) {
                        void* subItem = *(void**)(*(uintptr_t*)(entityDesc + 0x28) + (int64_t)j * 8);
                        // Virtual call to GetTypeId (vtable[2], offset 0x10)
                        typedef uint32_t (*GetTypeIdFn)(void*);
                        uint32_t subType = ((GetTypeIdFn)(*(uintptr_t*)(*(uintptr_t*)subItem + 0x10)))(subItem);
                        if (subType == *g_TypeId_BuildRequirementComponent) {
                            bool derived = fn_IsDerivedFrom(subItem, *g_TypeId_BuildRequirementComponent);
                            if (derived) {
                                buildReqComp = subItem;
                            }
                            break;
                        }
                    }
                }

                // Override UI::String from buildReqComp+0x30/+0x48
                if (buildReqComp != nullptr) {
                    uint8_t overrideStr[16];
                    fn_UIString_Ctor(overrideStr, *(uint64_t*)((uintptr_t)buildReqComp + 0x30),
                                     (void*)((uintptr_t)buildReqComp + 0x48));
                    *(uint64_t*)&uiString[0] = *(uint64_t*)overrideStr;
                    *(uint64_t*)&uiString[8] = *(uint64_t*)(overrideStr + 8);

                    // Override sprite: Sprite() default (empty/invalid sprite)
                    uint8_t overrideSpr[16];
                    fn_Sprite_Default_Ctor(overrideSpr);
                    *(uint64_t*)&uiSprite[0] = *(uint64_t*)overrideSpr;
                    *(uint64_t*)&uiSprite[8] = *(uint64_t*)(overrideSpr + 8);
                }
            }

            // Assign keyboard shortcut ID
            if (*(uint8_t*)(item + 0x44) == 0) {
                // Item has explicit shortcut override at +0x40
                fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
                *(int32_t*)(buttonRef + 0xC0) = *(int32_t*)(item + 0x40);

                // Remove this shortcut ID from the available pool
                int32_t targetId = *(int32_t*)(item + 0x40);
                for (int j = 0; j < shortcutCount; j++) {
                    if (shortcutIds[j] == targetId) {
                        // Shift remaining elements left
                        for (int k = j; k < shortcutCount - 1; k++) {
                            shortcutIds[k] = shortcutIds[k + 1];
                        }
                        shortcutCount--;
                        j--; // re-check same index
                    }
                }
            } else if (shortcutCount > 0) {
                // Auto-assign first available shortcut
                fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
                *(int32_t*)(buttonRef + 0xC0) = shortcutIds[0];

                // Remove first shortcut from pool
                for (int k = 0; k < shortcutCount - 1; k++) {
                    shortcutIds[k] = shortcutIds[k + 1];
                }
                shortcutCount--;
            }

            // Set element ID (button index within this toolbar)
            // Set element ID (button index within this toolbar)
            fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
            fn_Element_SetElementId((void*)buttonRef, buttonIndex);

            void* userData = *(void**)(item + 0x58);
            fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
            fn_Element_SetUserData((void*)buttonRef, userData);

            uint8_t tooltipData[16];
            *(uint64_t*)tooltipData = *(uint64_t*)uiString;
            *(uint64_t*)(tooltipData + 8) = *(uint64_t*)(uiString + 8);
            fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
            fn_Element_SetToolTip((void*)buttonRef, tooltipData);

            fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
            fn_Element_SetFlagsOnOff((void*)buttonRef, 0x10000, 0);

            // Set sprite on all ImageElement children
            int childIdx = 0;
            fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
            int childCount = fn_Element_ChildCount((void*)buttonRef);
            while (childIdx < childCount) {
                fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
                uint8_t childBuf[16];
                fn_Element_Child((void*)buttonRef, childBuf, childIdx);
                fn_InternalBasePtr_CheckPtr((void*)childBuf);
                void* childElem = *(void**)childBuf;

                // Cast to ImageElement
                bool isImage = fn_IsDerivedFrom(childElem, *g_TypeId_ImageElement);
                void* imageElem = isImage ? childElem : nullptr;

                uint64_t imageRef;
                fn_InternalBasePtr_Ctor(&imageRef, imageElem);

                // Set sprite if valid and imageElem is non-null
                bool spriteValid = fn_Sprite_IsValid(uiSprite);
                if (spriteValid && imageRef != 0) {
                    uint8_t sprCopy[16];
                    *(uint64_t*)sprCopy = *(uint64_t*)uiSprite;
                    *(uint64_t*)(sprCopy + 8) = *(uint64_t*)(uiSprite + 8);
                    fn_InternalBasePtr_CheckPtr((void*)&imageRef);
                    fn_ImageElement_SetSprite((void*)imageRef, sprCopy);
                }

                childIdx++;
                fn_InternalBasePtr_CheckPtr((void*)&buttonRef);
                childCount = fn_Element_ChildCount((void*)buttonRef);
            }
        }

        buttonIndex++;
    }

    // --- Step 8: Wire up radio button behavior ---
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xB8));
    fn_RadioElement_FindCheckBoxes(*(void**)(s + 0xB8));

    // --- Step 9: Populate speed values array {1, 2, 5, 10} ---
    *(int32_t*)(s + 0xF4) = 0; // currentSpeedIndex = 0

    DynArray4_Push(
        (void**)(s + 0xF8), (int32_t*)(s + 0x100), (int32_t*)(s + 0x104), 1);
    DynArray4_Push(
        (void**)(s + 0xF8), (int32_t*)(s + 0x100), (int32_t*)(s + 0x104), 2);
    DynArray4_Push(
        (void**)(s + 0xF8), (int32_t*)(s + 0x100), (int32_t*)(s + 0x104), 5);
    DynArray4_Push(
        (void**)(s + 0xF8), (int32_t*)(s + 0x100), (int32_t*)(s + 0x104), 10);

    // --- Step 10: Find current game speed in speed array ---
    int32_t speedIndex = 0;
    uint64_t gameWorldAddr = (uintptr_t)*g_pGameWorldSingleton;
    int32_t currentSpeed = *(int32_t*)(*(uintptr_t*)(gameWorldAddr + 0x120) + 0x34);
    int32_t speedCount = *(int32_t*)(s + 0x100);
    int32_t* speedArr = *(int32_t**)(s + 0xF8);

    for (int i = 0; i < speedCount; i++) {
        if (speedArr[i] == currentSpeed) {
            if (i >= 0) {
                speedIndex = i;
                goto found_speed;
            }
            break;
        }
    }
    speedIndex = 0; // default to first speed
found_speed:
    *(int32_t*)(s + 0xF4) = speedIndex;
    // Write the speed value back to the game system
    *(int32_t*)(*(uintptr_t*)(gameWorldAddr + 0x120) + 0x34) = speedArr[speedIndex];

    // --- Cleanup ---
    fn_MemFreeStatic(shortcutIds);
    fn_MemFreeStatic(itemList);

    return self;
}

// ============================================================
// ToolbarDialog::Update (vtable +0x30) — Sync button states
// ============================================================
// Iterates toolbar items, updates button checked/enabled states
// based on current game state (speed, pause, group visibility).
// Then calls the secondary update helper for ShowGroup/FocusEntity buttons.
//
// Original: 0x1400b8d40.

void ToolbarDialog_Update(void* self, uint64_t /*param2*/, uint64_t /*param3*/, uint64_t /*param4*/) {
    uintptr_t s = (uintptr_t)self;

    // Get game subsystem at gameWorld->+0x120
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gameSys = *(uintptr_t*)((uintptr_t)gameWorld + 0x120);

    // Current active dialog pointer at gameSys->+0x20
    int64_t activeDialog = *(int64_t*)(gameSys + 0x20);
    // Current game speed at gameSys->+0x34
    uint32_t gameSpeed = *(uint32_t*)(gameSys + 0x34);

    int32_t itemCount = *(int32_t*)(s + 0xD8);
    int64_t* itemArray = *(int64_t**)(s + 0xD0);
    int64_t* buttonArray = *(int64_t**)(s + 0xC0);

    for (int i = 0; i < itemCount; i++) {
        uintptr_t item = (uintptr_t)itemArray[i];
        int32_t actionType = *(int32_t*)(item + 0x48);

        if (actionType == 1) {
            // ShowGroup: uncheck button if this group is not the active dialog
            int64_t groupId = *(int64_t*)(item + 0x58);
            if (activeDialog != groupId) {
                // Get the button element from parallel array
                uintptr_t elemPtr = (uintptr_t)&buttonArray[i];
                // CheckPtr is a no-op in retail
                void* elem = *(void**)elemPtr;

                // IsDerivedFrom check, then cast
                bool isDerived = fn_IsDerivedFrom(elem, *g_TypeId_ToolbarElement);
                void* typedElem = isDerived ? elem : nullptr;

                // InternalBasePtr::Ctor to create a local ref
                int64_t localRef;
                fn_InternalBasePtr_Ctor(&localRef, typedElem);

                // If button is currently checked (+0xD8 != 0), uncheck it
                if (localRef != 0 && *(uint8_t*)(localRef + 0xD8) != 0) {
                    *(uint8_t*)(localRef + 0xD8) = 0;
                }
            }
        }
        else if (actionType == 9) {
            // SpeedUp: enable/disable based on whether current speed < max speed
            void* elem = *(void**)&buttonArray[i];
            // Virtual call: elem->vtable[8] = SetEnabled/SetChecked (vtable +0x40)
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            // Check if current speed index < max speed entry
            int32_t speedCount = *(int32_t*)(s + 0x100);
            int32_t* speedVals = *(int32_t**)(s + 0xF8);
            bool canSpeedUp = (int32_t)gameSpeed < speedVals[speedCount - 1];
            fn(elem, canSpeedUp);
        }
        else if (actionType == 6) {
            // SpeedDown: enable/disable based on whether current speed > min speed
            void* elem = *(void**)&buttonArray[i];
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            int32_t* speedVals = *(int32_t**)(s + 0xF8);
            bool canSpeedDown = speedVals[0] < (int32_t)gameSpeed;
            fn(elem, canSpeedDown);
        }
        else if (actionType == 8) {
            // Unpause button: checked state = isPaused flag at gameSys->+0x30
            void* elem = *(void**)&buttonArray[i];
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            uint8_t isPaused = *(uint8_t*)(gameSys + 0x30);
            fn(elem, isPaused);
        }
        else if (actionType == 7) {
            // Pause button: checked state = NOT paused
            void* elem = *(void**)&buttonArray[i];
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            uint8_t isPaused = *(uint8_t*)(gameSys + 0x30);
            fn(elem, isPaused == 0);
        }
        else if (actionType == 10) {
            // SpeedDisplay: get first child (text element), format speed as "%ix"
            void* elem = *(void**)&buttonArray[i];

            // Element::Child(elem, retBuf, 1) — child[1] is the text element
            uint8_t childBuf[16];
            fn_Element_Child(elem, childBuf, 1);

            void* childElem = *(void**)childBuf;

            // IsDerivedFrom check for TextElement type (exref at 0x140151508)
            bool isDerived = fn_IsDerivedFrom(childElem, *g_TypeId_TextElement);
            void* textElem = isDerived ? childElem : nullptr;

            // Build local InternalPtr
            int64_t textRef;
            fn_InternalBasePtr_Ctor(&textRef, textElem);

            // Format speed string: "%ix" with gameSpeed value
            uint8_t fmtStr[8];
            fn_String_Format(fmtStr, "%ix", (uint64_t)gameSpeed);

            // TextElement::SetParameter(textRef, 0, &fmtStr)
            if (textRef != 0) {
                fn_TextElement_SetParameter((void*)textRef, 0, fmtStr);
            }

            // Clean up formatted string
            fn_String_Dtor(fmtStr);
        }
    }

    // Call secondary update helper — handles ShowGroup and FocusEntity button enable states
    ToolbarDialog_UpdateButtons_Impl(self);
}

// ============================================================
// ToolbarDialog::OnEvent (vtable +0x40) — Handle toolbar clicks
// ============================================================
// Dispatches toolbar button click events to game actions.
// Event type 0x12 = button click, 0x16 = toolbar selection change.
//
// Original: 0x1400b92a0.

void ToolbarDialog_OnEvent(void* self, void* element, int eventType, int eventData,
                           uint64_t /*p5*/, uint64_t /*p6*/, int p7) {
    uintptr_t s = (uintptr_t)self;

    // Get game subsystem
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gameSys = *(uintptr_t*)((uintptr_t)gameWorld + 0x120);

    if (eventType == 0x12) {
        // Button click: eventData = button index
        int32_t buttonIndex = eventData;

        // Get toolbar item from array
        int64_t* itemArray = *(int64_t**)(s + 0xD0);
        uintptr_t item = (uintptr_t)itemArray[buttonIndex];

        // IsDerivedFrom check on the element
        void* elemObj = *(void**)element;
        bool isDerived = fn_IsDerivedFrom(elemObj, *g_TypeId_ToolbarElement);
        void* typedElem = isDerived ? elemObj : nullptr;

        int64_t localElemRef;
        fn_InternalBasePtr_Ctor(&localElemRef, typedElem);

        int32_t actionType = *(int32_t*)(item + 0x48);

        switch (actionType) {
        case 0: {
            // Tool: open/close sub-toolbar dialog
            int64_t* subDialogPtr = (int64_t*)(s + 0xE8);

            // If there's already a sub-dialog open, close it
            if (*subDialogPtr != 0) {
                uint64_t closeRef = (uint64_t)*subDialogPtr;
                fn_UI_CloseDialog(*(void**)(s + 0x10), &closeRef);
            }

            if (buttonIndex == *(int32_t*)(s + 0xF0)) {
                // Same button clicked again — toggle off
                *(int32_t*)(s + 0xF0) = -1;
                int64_t nullRef;
                fn_InternalBasePtr_Ctor(&nullRef, nullptr);
                *subDialogPtr = nullRef;
            } else {
                // Open new sub-toolbar
                // Allocate new ToolbarDialog (0x108 bytes)
                void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0x108, 8);
                void* childDialog = nullptr;
                if (newDialog) {
                    // Create InternalPtr to self for parent reference
                    int64_t selfRef;
                    fn_InternalBasePtr_Ctor(&selfRef, self);

                    // Construct child ToolbarDialog (recursive call)
                    childDialog = ToolbarDialog_Ctor_Impl(
                        newDialog,
                        *(void**)(s + 0x10),    // UserInterface
                        *(void**)(s + 0x18),    // Dialog
                        &selfRef,               // parent InternalPtr
                        (void*)item             // toolbar item (group parent)
                    );
                }

                // Store child dialog reference
                int64_t childRef;
                fn_InternalBasePtr_Ctor(&childRef, childDialog);
                *subDialogPtr = childRef;

                // Position the sub-dialog relative to toolbar
                // Get root element of sub-dialog
                void* subDialogInst = *(void**)subDialogPtr;
                void* subRoot = *(void**)((uintptr_t)subDialogInst + 0x50); // GetRoot inlined

                // Get padded size of sub-dialog
                int subWidth, subHeight;
                fn_Element_GetPaddedSize(subRoot, &subWidth, &subHeight);

                // Get parent toolbar element position
                void* parentRadio = *(void**)(s + 0xB8);
                int parentBottom = *(int32_t*)((uintptr_t)parentRadio + 100); // +0x64

                void* selfRoot = *(void**)((uintptr_t)self + 0x50); // GetRoot inlined
                int selfTop = *(int32_t*)((uintptr_t)selfRoot + 0x24);

                // Get element position for centering
                int elemX = *(int32_t*)(localElemRef + 0x68);
                int elemWidth = *(int32_t*)(localElemRef + 0x70);

                // Calculate horizontal position: center sub-dialog on button
                void* videoDevice = *g_pVideoDeviceSingleton;
                float uiScale = *(float*)(*(uintptr_t*)(s + 0x10) + 0xDC);
                int screenWidth = *(int32_t*)((uintptr_t)videoDevice + 4);
                int xPos = (int)((float)screenWidth * uiScale) -
                           (elemWidth + elemX) / 2 - subWidth / 2;
                if (xPos < 0) xPos = 0;

                // Set padding on sub-dialog root to position it
                void* subRoot2 = *(void**)((uintptr_t)*(void**)subDialogPtr + 0x50);
                fn_Element_SetPadding(subRoot2, 0, xPos, 0, parentBottom + selfTop);

                // Record which button index is open
                *(int32_t*)(s + 0xF0) = buttonIndex;

                // Add the sub-dialog to the UI
                uint64_t addRef = (uint64_t)*subDialogPtr;
                fn_UI_AddDialog(*(void**)(s + 0x10), &addRef);
            }
            break;
        }
        case 1: {
            // ShowGroup: toggle visibility of a game group
            void* groupId = nullptr;
            if (localElemRef != 0 && *(uint8_t*)(localElemRef + 0xD8) != 0) {
                groupId = *(void**)(item + 0x58);
            }
            Toolbar_ShowGroup_Impl((void*)gameSys, (int64_t)groupId);
            break;
        }
        case 2: {
            // Utility action
            uint8_t retBuf[16];
            Toolbar_Utility_Impl((void*)gameSys, retBuf, *(uint64_t*)(item + 0x58));
            break;
        }
        case 3: {
            // Toggle all groups
            Toolbar_ToggleAll_Impl((void*)gameSys);
            break;
        }
        case 4: {
            // Focus on entity
            uintptr_t entityList = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);

            // Create null InternalPtr for parent
            int64_t parentRef;
            fn_InternalBasePtr_Ctor(&parentRef, nullptr);

            // Find entity by type
            uint8_t entityBuf[16];
            void* entity = Toolbar_FindEntity_Impl(
                (void*)entityList, entityBuf,
                *(uint64_t*)(item + 0x58), 0
            );

            // Center camera on entity
            Toolbar_CenterOnEntity_Impl((void*)gameSys, &parentRef, entity);
            break;
        }
        case 5: {
            // CenterOnPoint: set camera position from predefined points
            int32_t pointIndex = *(int32_t*)(item + 0x4C);
            uintptr_t pointArray = *(uintptr_t*)(gameSys + 0x70);
            uint32_t* point = (uint32_t*)(pointArray + (int64_t)pointIndex * 0x1C);

            uintptr_t cameraSys = *(uintptr_t*)((uintptr_t)gameWorld + 0xE0);

            // Set point as active: flag at +0x18 = 1
            *(uint8_t*)(point + 6) = 1;  // +0x18 as bytes
            // Copy heading from gameSys
            point[5] = *(uint32_t*)(gameSys + 0x3C);
            point[4] = *(uint32_t*)(gameSys + 0x40);
            // Copy camera position from cameraSys
            point[3] = *(uint32_t*)(cameraSys + 0x28);
            uint32_t camY = *(uint32_t*)(cameraSys + 0x20);
            uint32_t camZ = *(uint32_t*)(cameraSys + 0x24);
            point[0] = *(uint32_t*)(cameraSys + 0x1C);
            point[1] = camY;
            point[2] = camZ;

            // Close sub-dialog and reset radio selection
            void* parentDialog = *(void**)(s + 0x28);
            bool isDerivedParent = fn_IsDerivedFrom(parentDialog, *g_TypeId_ToolbarDialogParent);
            void* typedParent = isDerivedParent ? parentDialog : nullptr;
            ToolbarDialog_CloseSubDialog_Impl(typedParent);
            break;
        }
        case 6: {
            // SpeedDown
            ToolbarDialog_ChangeSpeed_Impl(self, -1);
            break;
        }
        case 7: {
            // Pause
            *(uint8_t*)(gameSys + 0x30) = 1;
            break;
        }
        case 8: {
            // Unpause
            *(uint8_t*)(gameSys + 0x30) = 0;
            break;
        }
        case 9: {
            // SpeedUp
            ToolbarDialog_ChangeSpeed_Impl(self, 1);
            break;
        }
        }
    }
    else if (eventType == 0x16 && eventData == 1000 && p7 >= 0) {
        // Toolbar selection change: p7 = selected item index
        int32_t selIndex = p7;
        int64_t* itemArray = *(int64_t**)(s + 0xD0);
        uintptr_t item = (uintptr_t)itemArray[selIndex];

        // Dynamic cast the element to get toolbar controller
        void* elemObj = *(void**)element;
        void* castResult = nullptr;
        // FUN_140010e70: IsDerivedFrom check + cast
        bool isDerived = fn_IsDerivedFrom(elemObj, *g_TypeId_ToolbarElement);
        if (isDerived) castResult = elemObj;

        int64_t localElemRef;
        fn_InternalBasePtr_Ctor(&localElemRef, castResult);

        int32_t actionType = *(int32_t*)(item + 0x48);

        // If not a Tool (case 0) and we have an open sub-dialog, close it
        if (actionType != 0 && *(int64_t*)(s + 0xE8) != 0) {
            uint64_t closeRef = *(uint64_t*)(s + 0xE8);
            fn_UI_CloseDialog(*(void**)(s + 0x10), &closeRef);

            int64_t nullRef;
            fn_InternalBasePtr_Ctor(&nullRef, nullptr);
            *(int64_t*)(s + 0xE8) = nullRef;
            *(int32_t*)(s + 0xF0) = -1;
        }
    }
}
