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

#include "ui/TextElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ImageElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/TableElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/RadioElement.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/ElementDescription.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Element.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/DialogInstance.h"   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/MainMenuDialog.h"
#include "Dialog/ToolbarDialog.h"
#include "ui/UserInterface.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/String.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/StringTable.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Sprite.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/SpriteSheet.h"   // ENDGAME B.3.6 UI callers (K2)
#include "ui/Dialog.h"   // ENDGAME B.3.6 UI callers (K2)
#include <new>   // ENDGAME B.3.6 UI callers (K2)
#include "Dialog/ui_dialogs.h"
#include "Component/coll_bridge.h"
#include "graphics/Camera.h"   // ENDGAME B.3.5 closing pass -- Graphics::Camera
#include "engine_funcs.h"
#include "system/String.h"
#include "system/Filename.h"
#include "Object/factory_registration.h"
#include "object/External.h"   // ENDGAME B.3.3 step 6A
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "object/TypeInfo.h"   // ENDGAME B.3.3 step 6C
#include "Player/Tool.h"


#ifdef _DEBUG
// Debug sprite logging wrappers (runtime/src/ui/)
extern void* Debug_Sprite_Ctor(void* retval, uint64_t spriteSheet, const void* spriteName);
extern void* Debug_Sprite_DefaultCtor(void* retval);
extern bool Debug_Sprite_IsValid(void* sprite);
extern void Debug_ImageElement_SetSprite(void* self, void* spriteData);
#endif

// ============================================================
// Static inline implementations (replaces fn_* exe pointer calls)
// ============================================================

// ToolbarDialog_CloseSubDialog (FUN_1400b9730)
// Closes the active sub-dialog if present, resets radio selection.
// self+0xE8 = InternalBasePtr to sub-dialog, self+0xF0 = open sub index, self+0xB8 = radio.
void ToolbarDialog::CloseSubDialog() {
    uintptr_t s = (uintptr_t)this;
    uint64_t subDialogPtr = *(uint64_t*)(s + 0xE8);
    if (subDialogPtr != 0) {
        void* ui = *(void**)(s + 0x10);
        reinterpret_cast<UI::UserInterface*>(ui)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>((void*)(s + 0xE8)));  // B.3.6 bridge
        // Null out the sub-dialog ptr
        uint8_t nullBuf[8];
        new (nullBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        *(uint64_t*)(s + 0xE8) = *(uint64_t*)nullBuf;
        // Reset sub-dialog index to -1
        *(int32_t*)(s + 0xF0) = -1;
        // Reset radio selection
        void* radioElem = *(void**)(s + 0xB8);
        reinterpret_cast<UI::RadioElement*>(radioElem)->SetActiveRadio(-1);  // B.3.6 bridge
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
    new (retBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
    return nullptr;
}

// ============================================================
// ToolbarDialog::Startup (vtable +0x20, exe 0x1400b97c0): RET 0 (3 bytes)
// ============================================================
// No-op startup; all initialization happens in the constructor (Ctor_Impl).
void ToolbarDialog::OnOpened() {}

// ============================================================
// ToolbarDialog::Shutdown (vtable +0x28, exe 0x1400b97b0): JMP rel to FUN_1400b9730
// ============================================================
// Thunk to CloseSubDialog: closes any open sub-toolbar dialog.
void ToolbarDialog::OnClosed() {
    CloseSubDialog();
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

        // Find the component type matching g_TypeId_ToolbarDescription_FC
        void* matchedComponent = nullptr;
        if (g_TypeId_ToolbarDescription_FC && subCount > 0) {
            uint32_t targetType = *g_TypeId_ToolbarDescription_FC;
            for (int j = 0; j < subCount; j++) {
                void* subItem = *(void**)(*(uintptr_t*)(subData + 0x28) + (int64_t)j * 8);
                if (!subItem) continue;
                // vtable[2] (offset 0x10) = GetTypeId, returns uint32_t
                uint32_t subType = reinterpret_cast<Object::Base*>(subItem)->Type();  // B.3.8 bridge
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
                enabled = entityCount < *(int32_t*)((uintptr_t)matchedComponent + 0x60);
            }

            // Set button enabled via vtable[8] (offset 0x40)
            void** buttonArray = *(void***)(s + 0xC0);
            void* elem = buttonArray[i];
            if (!elem) continue;
            typedef void (*SetEnabled_t)(void* self, bool enabled);
            SetEnabled_t fn = (SetEnabled_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));
            fn(elem, enabled);
        }
        else if (actionType == 4) {
            // FocusEntity: check if any entities of that type exist
            void** buttonArray = *(void***)(s + 0xC0);
            void* elem = buttonArray[i];
            if (!elem) continue;

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
// THE single port of retail 0x140102b60. This function was independently ported
// TWICE — the second copy was entityui.cpp's EntityUI_SetEditMode_Impl, which is
// now a thin forwarder into this body. Both names, both signatures and every
// existing call site are preserved; only the duplicate body is gone.
//
// Shows/hides a tool group dialog. Called with gameSubsys (PlayerController at
// gameWorld+0x120) and a groupId (toolbar item type ID). The SetEditMode spelling
// calls the same object "playerController" / "entityUI" and the same argument
// "mode" — they are the same object and the same field set.
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
//   +0x20: Start()      == vtbl[4]  (Startup, in the SetEditMode spelling)
//   +0x28: Close()      == vtbl[5]  (Shutdown, in the SetEditMode spelling)
//   +0x58: GetToolSettingTypeId(retBuf) -> int* (returns ptr to typeId, 0 = none)
//                       == vtbl[11]
// The two ports spelled the teardown slot differently — *(vtbl + 0x28) here and
// vtbl[5] in the SetEditMode copy. 5*8 == 0x28: SAME SLOT. Byte offsets are used
// throughout this body.
//
// ToolController instance fields (set before Start):
//   +0x10: void* playerController (back-pointer)
//   +0x18: InternalBasePtr toolSetting
//   +0x20: int64_t groupId
//
// groupId == 0 IS A REAL LOOKUP, NOT A "CLEAR" NO-OP.
// The tool registry always contains a node keyed 0 -> EditTool (see
// BuildToolModeTree in tool_activation_standalone.cpp; the runtime log line
// "node{0 -> EditTool=...}" shows it). With groupId 0 the component scan finds
// nothing, foundTypeId stays 0, and the exact-match pass below then hits that
// key-0 node and CREATES the EditTool. That is how the default EditTool gets
// installed at all — retail EntityUI::Update relies on it via SetEditMode(pc, 0).
// An earlier version of this body returned early on foundTypeId == 0 on the
// premise that "no matching type exists"; the log disproves that premise, and
// the early-out would have killed entity selection outright. Do not reinstate it.
// ============================================================
void Toolbar_ShowGroup_Impl(void* gameSubsys, int64_t groupId) {
    uintptr_t pc = (uintptr_t)gameSubsys;
    // Step 1: If lockToggle is set and groupId is non-zero, override with locked group
    // (this is the tutorial's TutorialSetupRestrictPlayer restriction — +0x80/+0x88 are
    // written by TutorialSetupRestrictPlayer_Slot8. A step whose setup list contains
    // `disableSelect` (an all-default instance) locks +0x88 to NULL, which turns EVERY
    // toolbar selection into "no group". Trace both legs: during a tutorial this is the
    // difference between "the build tool does nothing" and a real tool failure.)
    if (*(uint8_t*)(pc + 0x80) != 0 && groupId != 0) {
        int64_t forced = *(int64_t*)(pc + 0x88);
        groupId = forced;
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
        // Call controller->Close() via vtable[5] (offset 0x28)
        void* controller = *(void**)(pc + 0x28);
        reinterpret_cast<Tool*>(controller)->Shutdown();   // slot 5; B.3.8 bridge
        // DeletePtr releases the reference
        reinterpret_cast< ::Object::InternalBasePtr*>((void*)(pc + 0x28))->DeletePtr();   // B.3.3 bridge
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
                    uint32_t subTypeId = reinterpret_cast<Object::Base*>(subItem)->Type();  // B.3.8 bridge
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

    // Step 5: BST search for the foundTypeId to get the matching ToolNode.
    // Retail is exact-match-or-NULL here: its loop head re-zeroes the result
    // register on EVERY iteration (`while (puVar14 = 0, puVar10 != 0)`), so a
    // failed search yields 0, NOT a lower_bound node. An earlier version of this
    // body had a second "lower_bound fallback" pass; retail has no such pass and
    // it only ever fed the foundTypeId == 0 early-out that is documented above as
    // a bug. Both are gone.
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

    // DELIBERATE DIVERGENCE FROM RETAIL (per CLAUDE.md: a guard is legitimate only
    // where our standalone genuinely differs from retail). Retail does NOT test the
    // search result — it goes straight to Factory::CreateObject(..., puVar14[2]) and
    // null-derefs if the registry held no matching key, because retail's registry
    // always does. Ours is built by BuildToolModeTree and can legitimately be empty
    // or partial in a standalone run, so guard the deref rather than fault. Every
    // other step below stays byte-faithful.
    if (foundNode == 0) {
        return;
    }

    // Step 6: Create ToolController via Factory::CreateObject
    // g_pFactorySingleton is dispatch table → points to Factory::_singleton → holds Factory*
    // Need double-deref: *(void**)*g_pFactorySingleton to get the actual Factory object
    uint32_t factoryTypeId = *(uint32_t*)(foundNode + 0x08);
    void* newObj = reinterpret_cast<const ::Object::Factory*>(*(void**)*g_pFactorySingleton)->CreateObject(
        factoryTypeId);   // B.3.3 bridge
    // IsDerivedFrom check for ToolController type
    bool isToolCtrl = fn_IsDerivedFrom(newObj, *g_TypeId_ToolController);
    void* toolController = isToolCtrl ? newObj : nullptr;


    // Store as InternalBasePtr at pc+0x28
    uint64_t ctrlRef = 0;
    new (&ctrlRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(toolController));   // B.3.3 bridge
    *(uint64_t*)(pc + 0x28) = ctrlRef;

    // Step 7: Create ToolSetting if needed (lazy init on the ToolNode)
    if (*(int64_t*)(foundNode + 0x10) == 0) {
        // Ask the controller for its tool setting type via vtable[11] (offset 0x58)
        void* ctrl = *(void**)(pc + 0x28);

        // slot 11 GetSettingType, retslot shape (RCX this, RDX ret); B.3.8 bridge
        Object::Type settingBuf(0);
        const Object::Type* settingTypePtr = reinterpret_cast<Tool*>(ctrl)->GetSettingType(&settingBuf);

        if (settingTypePtr->_type != 0) {
            // Re-query (matches original's double-call pattern)
            ctrl = *(void**)(pc + 0x28);
            uint32_t settingFactoryId = reinterpret_cast<Tool*>(ctrl)->GetSettingType(&settingBuf)->_type;

            // Create the ToolSetting object
            void* settingObj = reinterpret_cast<const ::Object::Factory*>(*(void**)*g_pFactorySingleton)->CreateObject(
                settingFactoryId);   // B.3.3 bridge
            bool isToolSetting = fn_IsDerivedFrom(settingObj, *g_TypeId_ToolSetting);
            void* toolSetting = isToolSetting ? settingObj : nullptr;

            uint64_t settingRef = 0;
            new (&settingRef) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(toolSetting));   // B.3.3 bridge
            *(uint64_t*)(foundNode + 0x10) = settingRef;
        }
    }

    // Step 8: Configure and start the controller
    uint64_t toolSettingRef = *(uint64_t*)(foundNode + 0x10);
    void* ctrl = *(void**)(pc + 0x28);

    // Set controller fields
    uintptr_t c = (uintptr_t)ctrl;
    *(uintptr_t*)(c + 0x10) = pc;                             // back-pointer to PlayerController
    *(int64_t*)(c + 0x20) = *(int64_t*)(pc + 0x20);          // groupId
    *(uint64_t*)(c + 0x18) = toolSettingRef;                  // ToolSetting ref

    // Call controller->Start() via vtable[4] (offset 0x20)
    ctrl = *(void**)(pc + 0x28);
    reinterpret_cast<Tool*>(ctrl)->Startup();   // slot 4; B.3.8 bridge
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

    // g_TypeId_UIDescription resolves from raw .rdata (0x1401c7f90) → NULL in the
    // standalone exe. Without it the UIDescription sub-item search below never
    // matches → no dialog opens. Point it at local storage filled from the live
    // factory tree (the proven placementtool/ToolbarStandalone_Init pattern).
    if (!g_TypeId_UIDescription) {
        static uint32_t s_tid_UIDescription = 0;
        if (s_tid_UIDescription == 0)
            s_tid_UIDescription = ::Object::GetTypeIdByName("UIDescription");
        g_TypeId_UIDescription = &s_tid_UIDescription;
    }

    // Get the UserInterface singleton: *(gameWorld + 0x98)
    void* gameWorld = *g_pGameWorldSingleton;
    void* userInterface = *(void**)((uintptr_t)gameWorld + 0x98);

    // Step 1: Check if a dialog with this controller type is already open
    void* existingDialog = nullptr;
    fn_FindDialogByControllerType(gameSubsys, &existingDialog, param);

    if (existingDialog != nullptr) {
        // Dialog is already open — close it
        uint64_t closeRef = (uint64_t)existingDialog;
        reinterpret_cast<UI::UserInterface*>(userInterface)->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&closeRef));  // B.3.6 bridge

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
        new (retBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        return;
    }

    // Step 2: No existing dialog — find the UIDescription sub-item
    int32_t subCount = *(int32_t*)(param + 0x30);
    void* uiDescItem = nullptr;

    for (int i = 0; i < subCount; i++) {
        void* subItem = *(void**)(*(uintptr_t*)(param + 0x28) + (int64_t)i * 8);
        if (!subItem) continue;

        // Call vtable[2] (+0x10) to get the type ID
        uint32_t subType = reinterpret_cast<Object::Base*>(subItem)->Type();  // B.3.8 bridge

        if (subType == *g_TypeId_UIDescription) {
            bool isDerived = fn_IsDerivedFrom(subItem, *g_TypeId_UIDescription);
            if (isDerived) {
                uiDescItem = subItem;
            }
            break;
        }
    }

    if (uiDescItem == nullptr) {
        new (retBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
        return;
    }

    // Step 3: Create a new DialogInstance from the UIDescription's resource
    void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, 0xB8, 8);
    void* dialogInst = nullptr;

    if (newBuf != nullptr) {
        // Null parent InternalBasePtr
        uint8_t nullParent[8];
        new (nullParent) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

        // External resource from uiDescItem+0x28
        void* extRes = *(void**)((uintptr_t)uiDescItem + 0x28);

        // DialogInstance ctor: (self, ui, extRes, parentPtr, dataSource, extraPtr, flags)
        // param is passed as the data source (identifies the utility type)
        dialogInst = fn_DialogInstance_Ctor(newBuf, userInterface, extRes,
                                            nullParent, (void*)param, nullptr, 0);

        // Override vtable to ToolUI child dialog vtable
        *(uintptr_t*)dialogInst = g_ToolUI_ChildDialog_Vtable;
        extern void* g_HarnessLastUtilityDialog;
        g_HarnessLastUtilityDialog = dialogInst;   // harness: drive its controllers
    }

    // Wrap in InternalBasePtr
    void* dialogRef = nullptr;
    new (&dialogRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(dialogInst));   // B.3.3 bridge

    if (dialogRef == nullptr) {
        new (retBuf) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
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
    int32_t savedX = *(int32_t*)((uintptr_t)bstNode + 0x08);
    int32_t savedY = *(int32_t*)((uintptr_t)bstNode + 0x0C);
    reinterpret_cast<UI::DialogInstance*>((void*)(uintptr_t)dialogRef)->SetUpperLeft(savedX, savedY, false);  // B.3.6 bridge

    // Add dialog to the UI
    uint64_t addRef = (uint64_t)dialogRef;
    reinterpret_cast<UI::UserInterface*>(userInterface)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&addRef));  // B.3.6 bridge

    // Return the dialog reference
    *(void**)retBuf = (void*)(uintptr_t)dialogRef;
}

// Retail entry point for FUN_140101a80, so callers that reach it BY ADDRESS resolve
// to this same body instead of a null pointer.
//
// The address was resolved into two differently-named variables and the sweep filed
// them as separate rows: fn_Toolbar_Utility (dead — this file calls the impl
// directly) and fn_SetFollowBuilding (engine_funcs.h, described as "set player
// follow-building"). That second name is a MISNOMER. The decompile is unambiguous:
// it finds-or-creates the ToolUI child DialogInstance for a Description, tracks its
// position in the BST at arg1+0x50, and AddDialog/CloseDialog's it — the utility
// panel toggler, nothing to do with follow mode.
//
// The two call shapes are identical, which is why one body serves both:
//   arg1 = EntityUI/ToolController at gameWorld+0x120
//   arg2 = InternalBasePtr* retval (non-trivial return)
//   arg3 = ComponentDescription*
// tutorialsetupdialog.cpp:369 passes exactly that and called it UNGUARDED, so with
// the address unregistered a tutorial step that targets a building was an EXEC-of-0.
extern "C" void* Toolbar_Utility_Reg(void* gameSubsys, void* retBuf, uint64_t param) {
    Toolbar_Utility_Impl(gameSubsys, retBuf, param);
    return retBuf;   // retail returns the retval pointer it was handed
}

// Captures the last DialogInstance created by Toolbar_Utility_Impl so the harness
// can reach its controllers (e.g. drive MapUI::SetupImage after open).
void* g_HarnessLastUtilityDialog = nullptr;

// Recursively dump a UI::Element tree. Element layout (Runtime RE):
//   +0x10 name (System::String: *ptr = data, len at data-8) · +0xa0 children
//   array (Element*[]) · +0xa8 child count · vtable[2] = GetTypeId.
static void HarnessDumpElemTree(void* elem, int depth, int* counter) {
    if (!elem || depth > 8 || *counter > 300) return;
    (*counter)++;
    char* nameData = *reinterpret_cast<char**>(reinterpret_cast<uintptr_t>(elem) + 0x10);
    int len = nameData ? *reinterpret_cast<int*>(nameData - 8) : 0;
    uint32_t tid = 0;
    { void** vt = *reinterpret_cast<void***>(elem);
      if (vt && vt[2]) { typedef uint32_t(*G)(void*); tid = reinterpret_cast<G>(vt[2])(elem); } }
    printf("[ElemTree] %*selem=%p tid=%u name='%.*s'\n", depth * 2, "",
           elem, tid, len > 0 ? len : 0, nameData ? nameData : "");
    void** kids = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(elem) + 0xa0);
    int kc = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(elem) + 0xa8);
    for (int i = 0; kids && i < kc; i++) HarnessDumpElemTree(kids[i], depth + 1, counter);
}

// GUI_PLAN Keystone F, Phase 4 — harness entry. Fires the utility-dialog open path
// (toolbar case 2) headlessly: pass the utility item's Description ptr (item+0x58,
// e.g. a resolved Template\Utility*.rsc) as `param`. Mirrors the case-2 dispatch.
extern "C" void ToolbarUtility_HarnessOpen(void* gameSubsys, uint64_t param) {
    uint8_t retBuf[16] = {};
    g_HarnessLastUtilityDialog = nullptr;
    Toolbar_Utility_Impl(gameSubsys, retBuf, param);
}

// Harness: fetch a named controller from the just-opened utility dialog and
// dispatch one of its vtable slots (slotIndex, e.g. 4=+0x20 MapUI "SetupImage",
// 5=+0x28 ProfessionUI setup/populate). Used to drive controller setup after open,
// since AddDialog doesn't reach the setup through the standalone ToolUI child-dialog
// vtable. slotIndex defaults to 4 for the existing map callers.
extern "C" void ToolbarUtility_HarnessDriveController(const char* controllerName, int slotIndex) {
    // (This harness never walks the dialog LIST — it enumerates the _addDialogs
    // array at UI+0x28 below — so the old `!fn_UI_GetFirstDialog` half of this
    // guard tested a pointer the body does not use, and is gone.)
    void* gameWorld = g_pGameWorldSingleton ? *g_pGameWorldSingleton : nullptr;
    if (!gameWorld) { printf("[HarnessDrive] gameWorld null\n"); return; }
    void* ui = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(gameWorld) + 0x98);
    if (!ui) { printf("[HarnessDrive] UserInterface null\n"); return; }

    uint32_t tid = ::Object::GetTypeIdByName(controllerName);
    printf("[HarnessDrive] searching open dialogs for '%s' (tid=%u)\n", controllerName, tid);

    // The open-dialog collection is a DynArray at UserInterface+0x28 (data ptr) /
    // +0x30 (count) — that's where UserInterface::AddDialog (Runtime 0x180120ee0)
    // pushes. Each element is an InternalPtr<DialogInstance> (8 bytes). Enumerate
    // it, dumping each dialog's controllers to locate the (nested) MapUI.
    uintptr_t uiAddr = reinterpret_cast<uintptr_t>(ui);
    uintptr_t* data = *reinterpret_cast<uintptr_t**>(uiAddr + 0x28);
    int count = *reinterpret_cast<int*>(uiAddr + 0x30);
    printf("[HarnessDrive] UI+0x28 dialog array=%p count=%d\n", data, count);
    for (int idx = 0; data && idx < count && idx < 32; idx++) {
        uintptr_t ip = data[idx];              // InternalPtr value
        void* dialog = reinterpret_cast<void*>(ip);
        if (!dialog) { printf("[HarnessDrive] dialog[%d]=null\n", idx); continue; }
        void** ctrlArr = *reinterpret_cast<void***>(reinterpret_cast<uintptr_t>(dialog) + 0x78);
        int cnt = *reinterpret_cast<int*>(reinterpret_cast<uintptr_t>(dialog) + 0x80);
        uint32_t dlgTid = 0;
        { void** dvt = *reinterpret_cast<void***>(dialog);
          if (dvt && dvt[2]) { typedef uint32_t(*G)(void*); dlgTid = reinterpret_cast<G>(dvt[2])(dialog); } }
        printf("[HarnessDrive] dialog[%d]=%p dlgTid=%u controllers=%d:", idx, dialog, dlgTid, cnt);
        for (int i = 0; ctrlArr && i < cnt && i < 16; i++) {
            void* c = ctrlArr[i]; uint32_t ctid = 0;
            if (c) { void** vt = *reinterpret_cast<void***>(c);
                if (vt && vt[2]) { typedef uint32_t(*G)(void*); ctid = reinterpret_cast<G>(vt[2])(c); } }
            printf(" [%d]tid=%u", i, ctid);
        }
        printf("\n");
        // Dump the dialog's element tree (root = dialog+0x50) so we see exactly
        // what content was built (names, types) and whether the minimap is there.
        {
            void* root = *reinterpret_cast<void**>(reinterpret_cast<uintptr_t>(dialog) + 0x50);
            printf("[HarnessDrive] dialog[%d] root elem=%p — tree:\n", idx, root);
            int counter = 0;
            HarnessDumpElemTree(root, 0, &counter);
            printf("[HarnessDrive] (tree had %d elements)\n", counter);
        }
        // Probe the dialog's element tree for the minimap content elements — if
        // they exist here, the content is embedded (no MapUI controller needed to
        // locate them; we can bind textures directly).
        {
            const char* names[] = { "imageMap", "imageOverlay", "buttonOk", "map", "image" };
            for (const char* nm : names) {
                System::String nb(nm);
                uint64_t r = 0; ::new (&r) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::DialogInstance*>(dialog)->FindElement(nb));  // B.3.6 bridge
                printf("[HarnessDrive]   FindElement('%s')=%p\n", nm, reinterpret_cast<void*>(r));
            }
        }
        uintptr_t ctrlBuf[2] = {0, 0};
        ::new (ctrlBuf) Object::InternalPtr<UI::DialogController>(reinterpret_cast<const UI::DialogInstance*>(dialog)->GetController(tid));  // B.3.6 bridge
        if (ctrlBuf[0]) {
            void* ctrl = reinterpret_cast<void*>(ctrlBuf[0]);
            printf("[HarnessDrive] FOUND '%s' ctrl=%p in dialog[%d] — driving Slot%d\n",
                   controllerName, ctrl, idx, slotIndex);
            void** vt = *reinterpret_cast<void***>(ctrl);
            if (vt && vt[slotIndex]) { typedef void(*S)(void*); reinterpret_cast<S>(vt[slotIndex])(ctrl);
                printf("[HarnessDrive] Slot%d done\n", slotIndex); }
            return;
        }
    }
    printf("[HarnessDrive] '%s' not found in %d open dialogs\n", controllerName, count);
}

// ============================================================
// EntityUI_OpenPauseMenu (FUN_140101ce0)
//
// The IN-GAME pause menu opener. Previously misnamed "Toolbar_ToggleAll_Impl":
// there is no toggle-all here. Retail has exactly two callers, and both mean
// "bring up the in-game menu":
//   - ToolbarDialog_OnRemove case 3 (0x1400b96ae) -- the toolbar's menu button
//   - EntityUI::Update (0x1401026a0) -- Application::IsPaused() && the
//     pause-on-focus-loss setting, i.e. the game lost focus
//
// It builds the same MainMenuDialog class the front-end menu uses, but from
// Dialog/Pause.rsc and with +0xB8 = gameWorld (instead of +0xC0 = menu loop).
// That +0xB8 is what makes MainMenuDialog::Setup (slot 4) light up the in-game
// buttons -- buttonResume / buttonSave / buttonMenu / buttonQuit -- and what
// makes OnCommand's Quit reach Application::Quit.
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
// Returns the dialog it created. Retail is void -- nothing there needs the
// pointer, because a real click arrives with the dialog as `this`. Handing it
// back costs nothing and is the only way an unattended driver can address the
// menu it just opened (standalone/pausemenu_harness.cpp).
extern "C" void* EntityUI_OpenPauseMenu(void* gameSubsys) {
    uintptr_t gs = (uintptr_t)gameSubsys;
    // Get UserInterface from gameWorld+0x98
    void* gameWorld = *g_pGameWorldSingleton;
    void* userInterface = *(void**)((uintptr_t)gameWorld + 0x98);
    // Step 1: If there's an active tool controller at +0x28, close it via vtable[9]
    if (*(uintptr_t*)(gs + 0x28) != 0) {
        void* toolCtrl = *(void**)(gs + 0x28);
        // vtable[9] = Close (offset 0x48)
        typedef void (*VClose_t)(void* self);
        VClose_t vClose = (VClose_t)(*(uintptr_t*)(*(uintptr_t*)toolCtrl + 0x48));
        vClose(toolCtrl);
    }

    // Step 2: Clear current tool group by calling ShowGroup(gameSubsys, 0)
    if (fn_Toolbar_ShowGroup)
        fn_Toolbar_ShowGroup(gameSubsys, 0);
    else
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
    Math::Transform viewTransform;
    if (fn_CameraSys_ComputeViewTransform) {
        void* result = fn_CameraSys_ComputeViewTransform(cameraSys, &viewTransform);

        // Convert compact Transform to Transform3 (48 bytes) and set on camera
        // ?AsTransform3@Transform@Math@@QEBA?AVTransform3@2@XZ  retail 0x18001fcd0
        Math::Transform3 transform3 =
            reinterpret_cast<const Math::Transform*>(result)->AsTransform3();
        // B.3.5 bridge -- ?SetCameraToWorld@Camera@Graphics@@QEAAXAEBVTransform3@Math@@@Z.
        // The Camera sits BY VALUE at cameraSys+0x34, and cameraSys is gameWorld+0xE0
        // == GameUpdateLoop::m_playerController, whose ctor
        // (Reimpl_FUN_1400985b0, gameupdateloop_load.cpp) runs
        // `??0Camera@Graphics@@` on exactly `p + 0x34`. The readiness `if` went with
        // the door pointer: it tested nothing but that pointer.
        reinterpret_cast<Graphics::Camera*>((uintptr_t)cameraSys + 0x34)
            ->SetCameraToWorld(transform3);   // B.3.5 bridge
    }

    // Step 4: Load "Dialog/Pause.rsc" resource
    System::Filename filename("Dialog/Pause.rsc");

    void* externalRes = nullptr;
    if (g_pExternalFactorySingleton && *g_pExternalFactorySingleton) {
        externalRes = const_cast<::Object::External*>(
            reinterpret_cast<::Object::ExternalFactory*>(*g_pExternalFactorySingleton)   // B.3.3 bridge
                ->FindOrLoad(filename));
    }

    // Step 6: Allocate PauseDialog (0xC8 bytes)
    void* pauseDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC8, 8);
    void* finalDialog = nullptr;

    uintptr_t gameWorldPtr = (uintptr_t)gameWorld;

    if (pauseDialog != nullptr) {
        // Step 7: the pause menu IS a MainMenuDialog (retail stamps its table,
        // 0x14015f550): DialogInstance(userInterface, externalRes, nullParent,
        // 0, 0, 0), then +0xB8 = gameWorld, +0xC0 = 0. ENDGAME B.3.8 DLG: the
        // real class's ctor (Dialog/MainMenuDialog.h).
        finalDialog = ::new (pauseDialog) MainMenuDialog(
            static_cast<UI::UserInterface*>(userInterface),
            static_cast<const UI::Dialog*>(externalRes),
            Object::InternalPtr<UI::DialogInstance>(),
            reinterpret_cast<void*>(gameWorldPtr), nullptr);
    }

    // Step 8: Wrap in InternalBasePtr and add to UI
    uint64_t dialogRef;
    new (&dialogRef) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(finalDialog));   // B.3.3 bridge
    uint64_t addRef = dialogRef;
    reinterpret_cast<UI::UserInterface*>(userInterface)->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&addRef));  // B.3.6 bridge
    return finalDialog;
}

// ============================================================
// Helper: push_back for engine dynamic arrays (ptr, count at +0, capacity at +4)
// Mimics the inlined vector growth pattern seen throughout the exe.
//
// B.3.2: the ToolbarDialog+0xC0/+0xD0 struct-field call sites below were
// converted to the GameColl::ArrayAt bridge (Component/coll_bridge.h) in
// place. This helper is KEPT because it still has one caller each
// (itemList/itemListCount/itemListCap and shortcutIds/shortcutCount/shortcutCap)
// that cannot go through the bridge: those are three independent locals, not a
// contiguous Collection::Array<T> header, AND shortcutIds/shortcutCount are
// later mutated directly (element-shift + count-- on consumption, see the
// "Wire up radio button behavior" section) which Array<T> has no public API
// for (only SetSize/Clear change _size).
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
//   8. Populates the speed values array {1, 2, 5, 10, 20, 50} (retail: {1, 2, 5, 10})
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
// ENDGAME B.3.8 DLG: the real constructor. Steps 1-2 (the DialogInstance ctor, then
// the table store) are the init list and the compiler's vtable store.
ToolbarDialog::ToolbarDialog(UI::UserInterface* userInterface, const UI::Dialog* dialog,
                             Object::InternalPtr<UI::DialogInstance> parent,
                             uint64_t groupId)
    : UI::DialogInstance(userInterface, dialog, parent, nullptr, nullptr, 0) {
    uintptr_t s = (uintptr_t)this;

    // --- Step 3: Initialize ToolbarDialog-specific fields ---
    *(uint64_t*)(s + 0xB8) = 0;         // radioElement
    *(uint64_t*)(s + 0xC0) = 0;         // buttonElements data
    *(uint64_t*)(s + 0xC8) = 0;         // buttonCount + capacity (two int32s)
    *(uint64_t*)(s + 0xD0) = 0;         // toolbarItems data
    *(uint64_t*)(s + 0xD8) = 0;         // itemCount + capacity
    *(uint64_t*)(s + 0xE0) = groupId;   // toolbar group ID
    new ((void*)(s + 0xE8)) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge // subDialog = null
    *(int32_t*)(s + 0xF0) = -1;         // openSubIndex = -1
    *(uint64_t*)(s + 0xF8) = 0;         // speedValues data
    *(uint64_t*)(s + 0x100) = 0;        // speedValueCount + capacity

    // --- Step 4: Find key UI elements ---
    // Find "toolbarContent" table element
    uint8_t elemBuf[16];

    void* tableElement;
    {
        System::String strStr("toolbarContent");
        void* retPtr = ::new (elemBuf) Object::InternalPtr<UI::Element>(
            FindElement(strStr));
        tableElement = *(void**)retPtr;
    }

    // Find "toolBar" radio element
    {
        System::String strStr("toolBar");
        void* retPtr = ::new (elemBuf) Object::InternalPtr<UI::Element>(
            FindElement(strStr));
        *(uint64_t*)(s + 0xB8) = *(uint64_t*)retPtr;
    }

    // Set radio element ID to 1000
    reinterpret_cast<UI::Element*>(*(void**)(s + 0xB8))->SetElementId(1000);  // B.3.6 bridge

    // --- Step 5: Collect all Toolbar items matching our group ID ---
    // Use ExternalFactory::FindNext to iterate all Toolbar externals
    void* factory = *g_pExternalFactorySingleton;
    uint32_t toolbarTypeId = *g_TypeId_ToolbarItem;

    // Temporary dynamic array for matching toolbar items
    void* itemList = nullptr;
    int32_t itemListCount = 0;
    int32_t itemListCap = 0;

    const ::Object::ExternalFactory* etbFactory =
        reinterpret_cast<const ::Object::ExternalFactory*>(factory);   // B.3.3 bridge
    void* ext = const_cast<::Object::External*>(
        etbFactory->FindNext(toolbarTypeId, nullptr));                  // B.3.3 bridge
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

        ext = const_cast<::Object::External*>(
            etbFactory->FindNext(toolbarTypeId,
                                 reinterpret_cast<const ::Object::External*>(ext)));  // B.3.3 bridge
    }

    // Sort the collected items by their priority field at +0x50, ascending.
    // The original calls fn_ToolbarSort (exe-only introsort @0x140021790, keyed on
    // *(int*)(*item + 0x50)); that address is unregistered in standalone, so the
    // call resolved null and was skipped — leaving the buttons in raw load order.
    // That is exactly the visible "wrong icon order" (main bar AND every sub-menu,
    // e.g. Pasture landing last instead of 3rd in the food group). Port the sort
    // directly so the buttons order correctly. itemListCount is small (~10) and
    // a stable insertion sort matches the ascending-by-priority result; equal
    // priorities keep their load order.
    if (itemListCount > 1) {
        uint64_t* items = (uint64_t*)itemList;
        for (int i = 1; i < itemListCount; i++) {
            uint64_t key = items[i];
            int32_t keyPri = *(int32_t*)(key + 0x50);
            int j = i - 1;
            while (j >= 0 && *(int32_t*)(items[j] + 0x50) > keyPri) {
                items[j + 1] = items[j];
                j--;
            }
            items[j + 1] = key;
        }
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
        ::new (templateBuf) Object::ExternalPtr<UI::ElementDescription>(reinterpret_cast<const UI::Dialog*>(dialog)->GetTemplate(templateIndex));  // B.3.6 bridge
        void* templateElem = *(void**)templateBuf;

        // Clone the template: virtual call at vtable[8] (offset 0x40)
        // Element::Clone(this, retBuf, dialog) — member, non-trivial return
        typedef void* (*CloneFn)(void* self, void* retBuf, const void* dialog);
        uint8_t cloneBuf[16];
        uintptr_t vtable = *(uintptr_t*)templateElem;
        CloneFn cloneFn = (CloneFn)(*(uintptr_t*)(vtable + 0x40));
        cloneFn(templateElem, cloneBuf, dialog);
        void* newElement = *(void**)cloneBuf;

        // Push to buttonElements array at +0xC0.
        // B.3.2 bridge: ToolbarDialog+0xC0 is Collection::Array<{Element*}>
        // (stride 0x08, count +0xC8, capacity +0xCC).
        auto& buttonRows = GameColl::ArrayAt<GameColl::Bytes<0x08, 8>>(s, 0xC0);
        buttonRows.SetSize(buttonRows.GetSize() + 1,
                            GameColl::Bytes<0x08, 8>((uint64_t)newElement));

        // Push to toolbarItems array at +0xD0.
        // B.3.2 bridge: ToolbarDialog+0xD0 is Collection::Array<{ToolbarItem*}>
        // (stride 0x08, count +0xD8, capacity +0xDC).
        auto& toolbarItemRows = GameColl::ArrayAt<GameColl::Bytes<0x08, 8>>(s, 0xD0);
        toolbarItemRows.SetSize(toolbarItemRows.GetSize() + 1,
                                 GameColl::Bytes<0x08, 8>((uint64_t)item));

        // Add element as child of table at current buttonIndex
        void* elemPtr = newElement;
        reinterpret_cast<UI::TableElement*>(tableElement)->SetChild(reinterpret_cast<Object::InternalPtr<UI::Element>*>(&elemPtr), buttonIndex);  // B.3.6 bridge

        // Set element name from toolbar item's resource path.
        // `GetPath` returns by value through a hidden return slot and the callee
        // CONSTRUCTS into it; copy-initialising the local is the same store.
        System::Filename pathObj =
            reinterpret_cast<const ::Object::External*>(item)->GetPath();  // B.3.3 bridge
        System::Filename pathIdObj = pathObj.GetPathIdentifer();
        reinterpret_cast<UI::Element*>(newElement)->SetName(*reinterpret_cast<const System::String*>(&pathIdObj));  // B.3.6 bridge

        // Get toolbar item's display string and sprite
        uint8_t uiString[16];
        uint8_t uiSprite[16];
        ::new (uiString) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)(item + 0x20))), *reinterpret_cast<const System::String*>((void*)(item + 0x30)));  // B.3.6 bridge
#ifdef _DEBUG
        Debug_Sprite_Ctor(uiSprite, *(uint64_t*)(item + 0x28), (void*)(item + 0x38));
#else
        ::new (uiSprite) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)(item + 0x28))), *reinterpret_cast<const System::String*>((void*)(item + 0x38)));  // B.3.6 bridge
#endif

        // --- Configure button element (skip for SpeedDisplay) ---
        if (templateIndex != 0) {
            // Cast element to ButtonElement via IsDerivedFrom
            void* elemForCast = newElement;
            bool isButton = fn_IsDerivedFrom(elemForCast, *g_TypeId_HelpButtonElem);
            void* buttonElem = isButton ? elemForCast : nullptr;

            uint64_t buttonRef;
            new (&buttonRef) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(buttonElem));   // B.3.3 bridge

            // For ShowGroup (1) and Utility (2), override string/sprite from
            // the item's entity description (ToolbarDescription)
            if (actionType == 1 || actionType == 2) {
                uint64_t entityDesc = *(uint64_t*)(item + 0x58);
                void* buildReqComp = nullptr;
                if (entityDesc != 0) {
                    int32_t descCount = *(int32_t*)(entityDesc + 0x30);
                    for (int j = 0; j < descCount; j++) {
                        void* subItem = *(void**)(*(uintptr_t*)(entityDesc + 0x28) + (int64_t)j * 8);
                        // Virtual call to GetTypeId (vtable[2], offset 0x10)
                        uint32_t subType = reinterpret_cast<Object::Base*>(subItem)->Type();  // B.3.8 bridge
                        if (subType == *g_TypeId_ToolbarDescription_FC) {
                            bool derived = fn_IsDerivedFrom(subItem, *g_TypeId_ToolbarDescription_FC);
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
                    ::new (overrideStr) UI::String(Object::ExternalPtr<UI::StringTable>(reinterpret_cast<UI::StringTable*>(*(uint64_t*)((uintptr_t)buildReqComp + 0x30))), *reinterpret_cast<const System::String*>((void*)((uintptr_t)buildReqComp + 0x48)));  // B.3.6 bridge
                    *(uint64_t*)&uiString[0] = *(uint64_t*)overrideStr;
                    *(uint64_t*)&uiString[8] = *(uint64_t*)(overrideStr + 8);

                    // Override sprite from ToolbarDescription: SpriteSheet at +0x20, name at +0x28
                    uint8_t overrideSpr[16];
#ifdef _DEBUG
                    Debug_Sprite_Ctor(overrideSpr, *(uint64_t*)((uintptr_t)buildReqComp + 0x20),
                                   (void*)((uintptr_t)buildReqComp + 0x28));
#else
                    ::new (overrideSpr) UI::Sprite(Object::ExternalPtr<UI::SpriteSheet>(reinterpret_cast<UI::SpriteSheet*>(*(uint64_t*)((uintptr_t)buildReqComp + 0x20))), *reinterpret_cast<const System::String*>((void*)((uintptr_t)buildReqComp + 0x28)));  // B.3.6 bridge
#endif
                    *(uint64_t*)&uiSprite[0] = *(uint64_t*)overrideSpr;
                    *(uint64_t*)&uiSprite[8] = *(uint64_t*)(overrideSpr + 8);
                }
            }

            // Assign keyboard shortcut ID
            if (*(uint8_t*)(item + 0x44) == 0) {
                // Item has explicit shortcut override at +0x40
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
                *(int32_t*)(buttonRef + 0xC0) = shortcutIds[0];

                // Remove first shortcut from pool
                for (int k = 0; k < shortcutCount - 1; k++) {
                    shortcutIds[k] = shortcutIds[k + 1];
                }
                shortcutCount--;
            }

            // Set element ID (button index within this toolbar)
            // Set element ID (button index within this toolbar)
            reinterpret_cast<UI::Element*>((void*)buttonRef)->SetElementId(buttonIndex);  // B.3.6 bridge

            void* userData = *(void**)(item + 0x58);
            reinterpret_cast<UI::Element*>((void*)buttonRef)->SetUserData(static_cast<Object::Base*>(userData));  // B.3.6 bridge

            uint8_t tooltipData[16];
            *(uint64_t*)tooltipData = *(uint64_t*)uiString;
            *(uint64_t*)(tooltipData + 8) = *(uint64_t*)(uiString + 8);
            reinterpret_cast<UI::Element*>((void*)buttonRef)->SetToolTip(*reinterpret_cast<UI::String*>(tooltipData));  // B.3.6 bridge

            reinterpret_cast<UI::Element*>((void*)buttonRef)->SetFlagsOnOff(0x10000, 0);  // B.3.6 bridge

            // Set sprite on all ImageElement children
            int childIdx = 0;
            int childCount = reinterpret_cast<const UI::Element*>((void*)buttonRef)->ChildCount();  // B.3.6 bridge
            while (childIdx < childCount) {
                uint8_t childBuf[16];
                ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>((void*)buttonRef)->Child(childIdx));  // B.3.6 bridge
                void* childElem = *(void**)childBuf;

                // Cast to ImageElement
                bool isImage = fn_IsDerivedFrom(childElem, *g_TypeId_ImageElement);
                void* imageElem = isImage ? childElem : nullptr;

                uint64_t imageRef;
                new (&imageRef) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(imageElem));   // B.3.3 bridge

                // Set sprite if valid and imageElem is non-null
#ifdef _DEBUG
                bool spriteValid = Debug_Sprite_IsValid(uiSprite);
#else
                bool spriteValid = reinterpret_cast<const UI::Sprite*>(uiSprite)->IsValid();  // B.3.6 bridge
#endif
                if (spriteValid && imageRef != 0) {
                    uint8_t sprCopy[16];
                    *(uint64_t*)sprCopy = *(uint64_t*)uiSprite;
                    *(uint64_t*)(sprCopy + 8) = *(uint64_t*)(uiSprite + 8);
#ifdef _DEBUG
                    Debug_ImageElement_SetSprite((void*)imageRef, sprCopy);
#else
                    reinterpret_cast<UI::ImageElement*>((void*)imageRef)->SetSprite(reinterpret_cast<const UI::Sprite*>(sprCopy));  // B.3.6 bridge
#endif
                }

                childIdx++;
                childCount = reinterpret_cast<const UI::Element*>((void*)buttonRef)->ChildCount();  // B.3.6 bridge
            }
        }

        buttonIndex++;
    }

    // --- Step 8: Wire up radio button behavior ---
    reinterpret_cast<UI::RadioElement*>(*(void**)(s + 0xB8))->FindCheckBoxes();  // B.3.6 bridge

    // --- Step 9: Populate speed values array {1, 2, 5, 10, 20, 50} ---
    *(int32_t*)(s + 0xF4) = 0; // currentSpeedIndex = 0

    // B.3.2 bridge: ToolbarDialog+0xF8 is Collection::Array<GameColl::I32>
    // (stride 4, count +0x100, capacity +0x104).
    auto& speedRows = GameColl::ArrayAt<GameColl::I32>(s, 0xF8);
    speedRows.SetSize(speedRows.GetSize() + 1, GameColl::I32{1});
    speedRows.SetSize(speedRows.GetSize() + 1, GameColl::I32{2});
    speedRows.SetSize(speedRows.GetSize() + 1, GameColl::I32{5});
    speedRows.SetSize(speedRows.GetSize() + 1, GameColl::I32{10});
    // [NON-RETAIL] retail stops at 10x. 20x and 50x extend the sequence so the perf
    // harness (BANISHED_STRESS_SPEED) and the in-game speed-up button can push the
    // sim harder than retail allows; the speed cascade below is table-driven.
    speedRows.SetSize(speedRows.GetSize() + 1, GameColl::I32{20});
    speedRows.SetSize(speedRows.GetSize() + 1, GameColl::I32{50});

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
}

// The door at 0x1400b9910 (address_registry.cpp; fn_ToolbarDialog_Ctor in
// GameUpdateLoop::Startup): placement-news the class. parentPtr is an
// InternalBasePtr* the ctor copies, as retail does.
void* ToolbarDialog_Ctor_Impl(void* self, void* userInterface, void* dialog,
                              void* parentPtr, void* toolbarData) {
    Object::InternalPtr<UI::DialogInstance> parent;
    if (parentPtr)
        parent = *static_cast<const Object::InternalPtr<UI::DialogInstance>*>(parentPtr);
    return ::new (self) ToolbarDialog(static_cast<UI::UserInterface*>(userInterface),
                                      static_cast<const UI::Dialog*>(dialog), parent,
                                      reinterpret_cast<uint64_t>(toolbarData));
}

// ============================================================
// ToolbarDialog::Update (vtable +0x30) — Sync button states
// ============================================================
// Iterates toolbar items, updates button checked/enabled states
// based on current game state (speed, pause, group visibility).
// Then calls the secondary update helper for ShowGroup/FocusEntity buttons.
//
// Original: 0x1400b8d40.

void ToolbarDialog::OnUpdateLayout() {
    uintptr_t s = (uintptr_t)this;

    // Get game subsystem at gameWorld->+0x120
    void* gameWorld = *g_pGameWorldSingleton;
    if (!gameWorld) return;
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
                if (!elem) continue;

                // IsDerivedFrom check, then cast
                bool isDerived = fn_IsDerivedFrom(elem, *g_TypeId_CheckElement);
                void* typedElem = isDerived ? elem : nullptr;

                // InternalBasePtr::Ctor to create a local ref
                int64_t localRef;
                new (&localRef) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(typedElem));   // B.3.3 bridge

                // If button is currently checked (+0xD8 != 0), uncheck it
                if (localRef != 0 && *(uint8_t*)(localRef + 0xD8) != 0) {
                    // Traced because this is the ONLY place a toolbar button is
                    // silently unchecked from the per-frame update, which is what a
                    // "the button un-clicks itself" report looks like from outside.
                    // It fires only on the 1->0 transition, so it cannot flood.
                    *(uint8_t*)(localRef + 0xD8) = 0;
                }
            }
        }
        else if (actionType == 9) {
            // SpeedUp: enable/disable based on whether current speed < max speed
            void* elem = *(void**)&buttonArray[i];
            if (!elem) continue;
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
            if (!elem) continue;
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            int32_t* speedVals = *(int32_t**)(s + 0xF8);
            bool canSpeedDown = speedVals[0] < (int32_t)gameSpeed;
            fn(elem, canSpeedDown);
        }
        else if (actionType == 8) {
            // Unpause button: checked state = isPaused flag at gameSys->+0x30
            void* elem = *(void**)&buttonArray[i];
            if (!elem) continue;
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            uint8_t isPaused = *(uint8_t*)(gameSys + 0x30);
            fn(elem, isPaused);
        }
        else if (actionType == 7) {
            // Pause button: checked state = NOT paused
            void* elem = *(void**)&buttonArray[i];
            if (!elem) continue;
            typedef void (*SetChecked_t)(void* self, bool checked);
            SetChecked_t fn = (SetChecked_t)(*(uintptr_t*)(*(uintptr_t*)elem + 0x40));

            uint8_t isPaused = *(uint8_t*)(gameSys + 0x30);
            fn(elem, isPaused == 0);
        }
        else if (actionType == 10) {
            // SpeedDisplay: get first child (text element), format speed as "%ix"
            void* elem = *(void**)&buttonArray[i];
            if (!elem) continue;

            // Element::Child(elem, retBuf, 1) — child[1] is the text element
            uint8_t childBuf[16];
            ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(elem)->Child(1));  // B.3.6 bridge

            void* childElem = *(void**)childBuf;

            // IsDerivedFrom check for TextElement type (exref at 0x140151508)
            bool isDerived = fn_IsDerivedFrom(childElem, *g_TypeId_TextElement);
            void* textElem = isDerived ? childElem : nullptr;

            // Build local InternalPtr
            int64_t textRef;
            new (&textRef) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(textElem));   // B.3.3 bridge

            // Format speed string: "%ix" with gameSpeed value
            System::String fmtStr = System::String::Format("%ix", (uint64_t)gameSpeed);

            // TextElement::SetParameter(textRef, 0, &fmtStr)
            if (textRef != 0) {
                reinterpret_cast<UI::TextElement*>((void*)textRef)->SetParameter(0, fmtStr);  // B.3.6 bridge
            }
        }
    }

    // Call secondary update helper — handles ShowGroup and FocusEntity button enable states
    ToolbarDialog_UpdateButtons_Impl(this);
}

// ============================================================
// ToolbarDialog::OnEvent (vtable +0x40) — Handle toolbar clicks
// ============================================================
// Dispatches toolbar button click events to game actions.
// Event type 0x12 = button click, 0x16 = toolbar selection change.
//
// Original: 0x1400b92a0.

// Diagnostic only: set while TutWave_StepExecute is running a step's Undo (slot 10)
// loop. A toolbar command dispatched with this non-zero is being handled in the middle
// of a tutorial step transition, before the next step's Apply has armed.
extern "C" int g_TutUndoDepth;
extern "C" int g_TutUndoIndex;

void ToolbarDialog::OnElementEvent(Object::InternalPtr<UI::Element>* element, int32_t eventType, int32_t eventData, int32_t /*c*/, int32_t /*d*/, int32_t p7, void* /*userData*/) {
    uintptr_t s = (uintptr_t)this;

    // Get game subsystem
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gameSys = *(uintptr_t*)((uintptr_t)gameWorld + 0x120);

    if (eventType == 0x12) {
        // Button click: eventData = button index
        int32_t buttonIndex = eventData;

        // Get toolbar item from array
        int64_t* itemArray = *(int64_t**)(s + 0xD0);
        uintptr_t item = (uintptr_t)itemArray[buttonIndex];

        // CheckPtr resolves lazy/weak InternalBasePtr before dereference
        // IsDerivedFrom check on the element
        void* elemObj = *(void**)element;
        bool isDerived = fn_IsDerivedFrom(elemObj, *g_TypeId_CheckElement);
        void* typedElem = isDerived ? elemObj : nullptr;

        int64_t localElemRef;
        new (&localElemRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(typedElem));   // B.3.3 bridge

        int32_t actionType = *(int32_t*)(item + 0x48);

        char pathBuf_trace[128];

        switch (actionType) {
        case 0: {
            // Tool: open/close sub-toolbar dialog
            int64_t* subDialogPtr = (int64_t*)(s + 0xE8);

            // If there's already a sub-dialog open, close it
            if (*subDialogPtr != 0) {
                uint64_t closeRef = (uint64_t)*subDialogPtr;
                reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&closeRef));  // B.3.6 bridge
            }

            if (buttonIndex == *(int32_t*)(s + 0xF0)) {
                // Same button clicked again — toggle off
                *(int32_t*)(s + 0xF0) = -1;
                int64_t nullRef;
                new (&nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
                *subDialogPtr = nullRef;
            } else {
                // Open new sub-toolbar
                // Allocate new ToolbarDialog (0x108 bytes)
                void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0x108, 8);
                void* childDialog = nullptr;
                if (newDialog) {
                    // Construct child ToolbarDialog (recursive), parented to this one.
                    childDialog = ::new (newDialog) ToolbarDialog(
                        static_cast<UI::UserInterface*>(_ui), _dialog,
                        Object::InternalPtr<UI::DialogInstance>(this),
                        (uint64_t)item);        // toolbar item (group parent)
                }

                // Store child dialog reference
                int64_t childRef;
                new (&childRef) ::Object::InternalBasePtr(
                    reinterpret_cast<RtObject::Internal*>(childDialog));   // B.3.3 bridge
                *subDialogPtr = childRef;

                // Position the sub-dialog relative to toolbar
                // Get root element of sub-dialog
                void* subDialogInst = *(void**)subDialogPtr;
                void* subRoot = *(void**)((uintptr_t)subDialogInst + 0x50); // GetRoot inlined

                // Get padded size of sub-dialog
                int subWidth, subHeight;
                reinterpret_cast<UI::Element*>(subRoot)->GetPaddedSize(subWidth, subHeight);  // B.3.6 bridge

                // Get parent toolbar element position
                void* parentRadio = *(void**)(s + 0xB8);
                int parentBottom = *(int32_t*)((uintptr_t)parentRadio + 100); // +0x64

                void* selfRoot = *(void**)((uintptr_t)this + 0x50); // GetRoot inlined
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
                reinterpret_cast<UI::Element*>(subRoot2)->SetPadding(0, xPos, 0, parentBottom + selfTop);  // B.3.6 bridge

                // Record which button index is open
                *(int32_t*)(s + 0xF0) = buttonIndex;

                // Add the sub-dialog to the UI
                uint64_t addRef = (uint64_t)*subDialogPtr;
                reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->AddDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&addRef));  // B.3.6 bridge
            }
            break;
        }
        case 1: {
            // ShowGroup: toggle visibility of a game group
            void* groupId = nullptr;
            if (localElemRef != 0 && *(uint8_t*)(localElemRef + 0xD8) != 0) {
                groupId = *(void**)(item + 0x58);
            }
            // Must call ORIGINAL exe function — 15+ other callers share PlayerController state
            if (fn_Toolbar_ShowGroup) {
                fn_Toolbar_ShowGroup((void*)gameSys, (int64_t)groupId);
            } else {
                Toolbar_ShowGroup_Impl((void*)gameSys, (int64_t)groupId);
            }
            break;
        }
        case 2: {
            // Utility action
            uint8_t retBuf[16];
            Toolbar_Utility_Impl((void*)gameSys, retBuf, *(uint64_t*)(item + 0x58));
            break;
        }
        case 3: {
            // Open the in-game pause menu (Resume / Save / Load / Options /
            // Quit to Menu / Quit Game). gameSys is gameWorld+0x120 (EntityUI),
            // exactly what retail passes at 0x1400b96ae.
            EntityUI_OpenPauseMenu((void*)gameSys);
            break;
        }
        case 4: {
            // Focus on entity
            uintptr_t entityList = *(uintptr_t*)((uintptr_t)gameWorld + 0xF0);

            // Create null InternalPtr for parent
            int64_t parentRef;
            new (&parentRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge

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

            // Close sub-dialog and reset radio selection.
            // Retail resolves the parent InternalBasePtr BEFORE reading it:
            //   (*fn_InternalBasePtr_CheckPtr_slot)(param_1 + 0x28);
            //   pDVar6 = *(DialogInstance **)(param_1 + 0x28);
            // Our port had dropped that CheckPtr, so the read could see an
            // unresolved lazy/weak handle instead of the parent dialog.
            void* parentDialog = *(void**)(s + 0x28);
            bool isDerivedParent = fn_IsDerivedFrom(parentDialog, *g_TypeId_ToolbarDialogParent);
            void* typedParent = isDerivedParent ? parentDialog : nullptr;
            // No null guard: retail's FUN_1400b9730 derefs param_1+0xe8 unguarded, so
            // this cast is expected to always succeed. It used to fail because the
            // hand-built ToolbarDialog table's Type slot forwarded to the DialogInstance
            // base, making every ToolbarDialog report as its own base class. Since
            // B.3.8 the compiler table's Type() answers ToolbarDialog's own id.
            reinterpret_cast<ToolbarDialog*>(typedParent)->CloseSubDialog();   // B.3.8 bridge
            break;
        }
        case 6: {
            // SpeedDown
            ToolbarDialog_ChangeSpeed_Impl(this, -1);
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
            ToolbarDialog_ChangeSpeed_Impl(this, 1);
            break;
        }
        }
    }
    else if (eventType == 0x16 && eventData == 1000 && p7 >= 0) {
        // Toolbar selection change: p7 = selected item index
        int32_t selIndex = p7;
        int64_t* itemArray = *(int64_t**)(s + 0xD0);
        uintptr_t item = (uintptr_t)itemArray[selIndex];

        // CheckPtr resolves lazy/weak InternalBasePtr before dereference
        // Dynamic cast the element to get toolbar controller
        void* elemObj = *(void**)element;
        void* castResult = nullptr;
        // FUN_140010e70: IsDerivedFrom check + cast
        bool isDerived = fn_IsDerivedFrom(elemObj, *g_TypeId_CheckElement);
        if (isDerived) castResult = elemObj;

        int64_t localElemRef;
        new (&localElemRef) ::Object::InternalBasePtr(
            reinterpret_cast<RtObject::Internal*>(castResult));   // B.3.3 bridge

        int32_t actionType = *(int32_t*)(item + 0x48);

        // If not a Tool (case 0) and we have an open sub-dialog, close it
        if (actionType != 0 && *(int64_t*)(s + 0xE8) != 0) {
            uint64_t closeRef = *(uint64_t*)(s + 0xE8);
            reinterpret_cast<UI::UserInterface*>(*(void**)(s + 0x10))->CloseDialog(reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(&closeRef));  // B.3.6 bridge

            int64_t nullRef;
            new (&nullRef) ::Object::InternalBasePtr(nullptr);   // B.3.3 bridge
            *(int64_t*)(s + 0xE8) = nullRef;
            *(int32_t*)(s + 0xF0) = -1;
        }
    }
}

// ============================================================
// ENDGAME B.3.8 DLG: construction and teardown of the real class.
// ============================================================

// 0x1400b9890 (create 0x140021710's ctor): DialogInstance(), the table, then
// +0xB8..+0xE8 and +0xF8/+0x100 zeroed as qwords and +0xF4 as a dword. +0xF0
// (openSubIndex) is left as allocated, as retail leaves it.
ToolbarDialog::ToolbarDialog() {
    radioElement = 0;
    buttonElements = 0;
    buttonCount = 0;
    buttonCapacity = 0;
    toolbarItems = 0;
    itemCount = 0;
    itemCapacity = 0;
    toolbarGroupId = 0;
    subDialog = 0;
    currentSpeedIndex = 0;
    speedValues = 0;
    speedValueCount = 0;
    speedValueCapacity = 0;
}

// The ~ToolbarDialog body [0] 0x140021760 calls: the three arrays, then ~DialogInstance.
ToolbarDialog::~ToolbarDialog() {
    uintptr_t s = (uintptr_t)this;
    if (fn_MemFreeStatic) {
        fn_MemFreeStatic(*(void**)(s + 0xF8)); *(uint64_t*)(s + 0xF8) = 0; *(uint64_t*)(s + 0x100) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xD0)); *(uint64_t*)(s + 0xD0) = 0; *(uint64_t*)(s + 0xD8) = 0;
        fn_MemFreeStatic(*(void**)(s + 0xC0)); *(uint64_t*)(s + 0xC0) = 0; *(uint64_t*)(s + 0xC8) = 0;
    }
}

// [0] 0x140021760: the body above, MemFree on &1.
Object::Base* ToolbarDialog::DeletingDtor(unsigned flags) {
    this->~ToolbarDialog();
    if (flags & 1) Object::detail::InternalMemFree(this);
    return this;
}

// create 0x140021710: MemAlloc(0x108, 8), then the default ctor.
void* ToolbarDialog::CreateNewObject() {
    void* p = Object::detail::InternalMemAlloc(sizeof(ToolbarDialog), 8);
    if (!p) return nullptr;
    return static_cast<Object::Base*>(::new (p) ToolbarDialog());
}
// createInPlace (the tree's in-place build is kept, B8-13).
void* ToolbarDialog::CreateInPlaceObject(void* storage) {
    if (!storage) return nullptr;
    return static_cast<Object::Base*>(::new (storage) ToolbarDialog());
}

// The reflection record stays at the TU position of the UI_DIALOG_STUB row it
// replaces (ui_dialog_stub_factories.cpp, B38 D4).
DEFINE_EXE_VTABLE(ToolbarDialog, "_ZTV13ToolbarDialog")
