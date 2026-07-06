// code/Game/Dialog/toolui.cpp
// Clean reimplementation of ToolUI panel methods.
//
// ToolUI (0x58 bytes): Active tool/entity panel container.
// Extends DialogController (0x30 base).
// Layout:
//   +0x18 = dialog instance (InternalBasePtr, from DialogController)
//   +0x20 = dialog element (InternalBasePtr, from DialogController)
//   +0x28 = data source (InternalBasePtr, from DialogController)
//   +0x30 = description ptr (BuildComponent or Entity->description)
//   +0x38 = labelTitle element (InternalBasePtr)
//   +0x40 = flags: +0x40=pinned, +0x41=dragging
//   +0x44 = drag offset X (int32)
//   +0x48 = drag offset Y (int32)
//   +0x4C = flags: +0x4C=hasUIDesc, +0x4D=drag pending
//   +0x50 = pending mouse X (int32)
//   +0x54 = pending mouse Y (int32)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>
#ifdef STANDALONE_EXE
#include <cstdio>
#include <cstdlib>
#endif

// Forward declaration — defined in entityui.cpp
extern void EntityUI_SelectEntity_Impl(void* playerController, void* entityPtr,
                                        uint32_t x, uint32_t y);

// ============================================================
// UIDesc_AddControllers_Impl (was FUN_14012e100)
// ============================================================
// Iterates the controller entries in a UIDescription and registers each one
// with the dialog instance. Each entry is 0x28 bytes:
//   +0x00: uint32_t controllerTypeId
//   +0x08: ExternalPtr<ElementDescription> elementDesc
//   +0x10: ExternalPtr<DialogControllerConfig> config
//   +0x18: ExternalPtr<ElementDescription> tabDesc
//   +0x20: System::String elementName (8 bytes)
//
// For each entry, looks up the named element in the dialog. If the element
// is a TabElement, creates a new tab page via AddTab and uses the 5-arg
// AddController overload with the tab page. Otherwise, uses the 6-arg
// AddController overload with the element description and name string.
//
// UIDescription layout:
//   +0x30: void* entries array pointer
//   +0x38: int entry count

void UIDesc_AddControllers_Impl(void* uiDesc, void* dialogInstPtr, void* dataSource) {
    uintptr_t desc = (uintptr_t)uiDesc;
    int count = *(int*)(desc + 0x38);
    if (count <= 0) return;

    uintptr_t entries = *(uintptr_t*)(desc + 0x30);
#ifdef STANDALONE_EXE
    if (std::getenv("BANISHED_MAP_TRACE")) {
        printf("[UIDescAdd] uiDesc=%p count=%d entries=%p dataSource=%p\n",
               uiDesc, count, (void*)entries, dataSource);
        for (int j = 0; j < count && j < 4; j++) {
            uintptr_t e = entries + (uintptr_t)j * 0x28;
            uint64_t nameStrVal = *(uint64_t*)(e + 0x20);
            printf("[UIDescAdd]   entry[%d]@%p type=%u elemDesc=%p config=%p tabDesc=%p "
                   "nameStrPtr=0x%llx\n", j, (void*)e, *(uint32_t*)e,
                   *(void**)(e + 0x08), *(void**)(e + 0x10), *(void**)(e + 0x18),
                   (unsigned long long)nameStrVal);
        }
        fflush(stdout);
    }
#endif

    for (int i = 0; i < count; i++) {
        uintptr_t entry = entries + (uintptr_t)i * 0x28;

        // Look up the named element in the dialog
        fn_InternalBasePtr_CheckPtr(dialogInstPtr);
        void* dialogInst = *(void**)dialogInstPtr;

        void* foundElement = nullptr;
        void* nameStr = (void*)(entry + 0x20);
        fn_DialogInstance_FindElement(dialogInst, &foundElement, nameStr);

        if (foundElement == nullptr) {
            // No matching element found — use 6-arg AddController (non-tab path)
            uint32_t typeId = *(uint32_t*)entry;
            fn_InternalBasePtr_CheckPtr(dialogInstPtr);
            dialogInst = *(void**)dialogInstPtr;
            fn_DialogInstance_AddController6(
                dialogInst,
                typeId,
                dataSource,
                *(void**)(entry + 0x10),   // controllerConfig
                *(void**)(entry + 0x08),   // elementDesc
                (void*)(entry + 0x20));    // nameStr
            continue;
        }

        // Check if the found element is a TabElement
        fn_InternalBasePtr_CheckPtr(&foundElement);
        bool isTabElement = fn_IsA(foundElement, *g_TypeId_TabElement);

        if (!isTabElement) {
            // Not a tab — use 6-arg AddController (non-tab path)
            uint32_t typeId = *(uint32_t*)entry;
            fn_InternalBasePtr_CheckPtr(dialogInstPtr);
            dialogInst = *(void**)dialogInstPtr;
            fn_DialogInstance_AddController6(
                dialogInst,
                typeId,
                dataSource,
                *(void**)(entry + 0x10),   // controllerConfig
                *(void**)(entry + 0x08),   // elementDesc
                (void*)(entry + 0x20));    // nameStr
            continue;
        }

        // Element is a TabElement — cast via IsDerivedFrom and use tab path
        fn_InternalBasePtr_CheckPtr(&foundElement);
        bool derived = fn_IsDerivedFrom(foundElement, *g_TypeId_TabElement);
        void* tabElement = derived ? foundElement : nullptr;

        // Wrap in InternalBasePtr
        void* tabPtr = nullptr;
        fn_InternalBasePtr_Ctor(&tabPtr, tabElement);

        // Create a new tab page: AddTab(tabElement, retval, tabDesc, elementDesc)
        // Non-trivial return ABI: this=RCX, retval=RDX, tabDesc=R8, elementDesc=R9
        fn_InternalBasePtr_CheckPtr(&tabPtr);
        void* tabPage = nullptr;
        fn_TabElement_AddTab(
            tabPtr,              // this (TabElement)
            &tabPage,            // hidden return: InternalPtr<Element>
            *(void**)(entry + 0x18),  // tabDesc
            *(void**)(entry + 0x08)); // elementDesc

        // Use 5-arg AddController with the tab page element
        uint32_t typeId = *(uint32_t*)entry;
        fn_InternalBasePtr_CheckPtr(dialogInstPtr);
        dialogInst = *(void**)dialogInstPtr;
        fn_DialogInstance_AddControllerTab(
            dialogInst,
            typeId,
            dataSource,
            *(void**)(entry + 0x10),  // controllerConfig
            &tabPage);                // tab page element
    }
}

// ============================================================
// ToolUI::Slot4 (vtable +0x20) — Setup/populate tool panel
// ============================================================
// Checks data source type: if BuildComponent, stores directly at +0x30.
// If ComponentEntity, stores entity->description at +0x30.
// Finds "labelTitle" element and sets title text from BuildRequirementComponent.
// Finds "buttonClose" and sets ID 1.
// Finds "borderTitle" and configures drag handle (flags 0x10000, ID 0).
// Searches description for UIDescription and registers sub-controllers.
//
// Original: 0x14007BD70.

void ToolUI_Slot4(ToolUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Determine description source from +0x28
    void* dataSource = *(void**)(s + 0x28);
    bool isBuildComp = fn_IsDerivedFrom(dataSource, *g_TypeId_BuildComponent);

    if (isBuildComp && dataSource != nullptr) {
        // Data source IS the description — check again and store
        void* ds2 = *(void**)(s + 0x28);
        bool check = fn_IsDerivedFrom(ds2, *g_TypeId_BuildComponent);
        *(void**)(s + 0x30) = check ? ds2 : nullptr;
    } else {
        // Check if data source is a ComponentEntity
        void* ds2 = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(ds2, *g_TypeId_ComponentEntity);
        if (isEntity && ds2 != nullptr) {
            void* ds3 = *(void**)(s + 0x28);
            bool check = fn_IsDerivedFrom(ds3, *g_TypeId_ComponentEntity);
            void* entity = check ? ds3 : nullptr;
            // Store entity's description (+0x28) at self+0x30
            *(void**)(s + 0x30) = *(void**)((uintptr_t)entity + 0x28);
        }
    }

    // If the data source resolved to no description (+0x30 still null), there is
    // nothing to populate — the title/UIDescription sections below dereference
    // desc+0x30 unconditionally, so bail. In the exe the data source is always a
    // BuildComponent or a ComponentEntity so +0x30 is set; this guards the
    // standalone case where a raw description could reach here.
    if (*(void**)(s + 0x30) == nullptr) return;

    // Find "labelTitle" element via DialogInstance
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "labelTitle");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        *(void**)(s + 0x38) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // Set title text from BuildRequirementComponent in description
    if (*(void**)(s + 0x38) != nullptr) {
        void* desc = *(void**)(s + 0x30);
        int descCount = *(int*)((uintptr_t)desc + 0x30);
        uintptr_t descArray = *(uintptr_t*)((uintptr_t)desc + 0x28);

        for (int i = 0; i < descCount; i++) {
            void* entry = *(void**)(descArray + (uintptr_t)i * 8);
            // Get TypeId via vtable[2]
            typedef int (*fn_GetTypeId_t)(void*);
            void** vtbl = *(void***)entry;
            int typeId = ((fn_GetTypeId_t)vtbl[2])(entry);

            if (typeId == (int)*g_TypeId_BuildRequirementComponent) {
                bool check = fn_IsDerivedFrom(entry, *g_TypeId_BuildRequirementComponent);
                if (check && entry != nullptr) {
                    // Create UI::String from entry's string table (+0x30) and name (+0x38)
                    uint8_t uiStrBuf[16];
                    fn_UIString_Ctor(uiStrBuf, *(uint64_t*)((uintptr_t)entry + 0x30),
                                     (void*)((uintptr_t)entry + 0x38));
                    // Set text on labelTitle element
                    fn_InternalBasePtr_CheckPtr((void*)(s + 0x38));
                    fn_TextElement_SetText(*(void**)(s + 0x38), uiStrBuf);
                }
                break;
            }
        }
    }

    // Find "buttonClose" element, set ID 1
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "buttonClose");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        void* closeBtn = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (closeBtn != nullptr) {
            fn_InternalBasePtr_CheckPtr(&closeBtn);
            fn_Element_SetElementId(closeBtn, 1);
        }
    }

    // Find "borderTitle" element — drag handle
    {
        void* borderTitle = nullptr;
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "borderTitle");
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialogInst = *(void**)(s + 0x18);
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        borderTitle = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (borderTitle == nullptr) {
            *(uint8_t*)(s + 0x40) = 0;
        } else {
            fn_InternalBasePtr_CheckPtr(&borderTitle);
            fn_Element_SetElementId(borderTitle, 0);
            fn_InternalBasePtr_CheckPtr(&borderTitle);
            fn_Element_SetFlagsOnOff(borderTitle, 0x10000, 0);
            *(uint16_t*)(s + 0x40) = 1;
        }
    }

    // Clear drag-pending flag
    *(uint8_t*)(s + 0x4C) = 0;

    // Search description for UIDescription and register sub-controllers
    {
        void* desc = *(void**)(s + 0x30);
        int descCount = *(int*)((uintptr_t)desc + 0x30);
        uintptr_t descArray = *(uintptr_t*)((uintptr_t)desc + 0x28);
#ifdef STANDALONE_EXE
        if (std::getenv("BANISHED_MAP_TRACE"))
            printf("[ToolUI_Slot4] desc(self+0x30)=%p descCount=%d uiDescTid=%u\n",
                   desc, descCount, g_TypeId_UIDescription ? *g_TypeId_UIDescription : 0);
#endif

        for (int i = 0; i < descCount; i++) {
            void* entry = *(void**)(descArray + (uintptr_t)i * 8);
            typedef int (*fn_GetTypeId_t)(void*);
            void** vtbl = *(void***)entry;
            int typeId = ((fn_GetTypeId_t)vtbl[2])(entry);
#ifdef STANDALONE_EXE
            if (std::getenv("BANISHED_MAP_TRACE"))
                printf("[ToolUI_Slot4]   descEntry[%d]=%p tid=%d\n", i, entry, typeId);
#endif

            if (typeId == (int)*g_TypeId_UIDescription) {
                bool check = fn_IsDerivedFrom(entry, *g_TypeId_UIDescription);
                if (!check || entry == nullptr) return;

                // Register sub-controllers from UIDescription
                void* dialogInstPtr = (void*)(s + 0x18);
                UIDesc_AddControllers_Impl(entry, dialogInstPtr,
                                           *(void**)(s + 0x28));
                return;
            }
        }
    }
}

// ============================================================
// ToolUI::Slot8 (vtable +0x40) — Mouse drag handler
// ============================================================
// Handles two drag modes:
// 1. Pending drag (+0x4D): waits for mouse to move >5px, then starts drag.
// 2. Active drag (+0x41): moves dialog to follow mouse, updates entity
//    tracking BST position. Stops when mouse button released.
//
// Original: 0x14007B9B0.

void ToolUI_Slot8(ToolUI* self) {
    uintptr_t s = (uintptr_t)self;

#ifdef STANDALONE_EXE
    // --- Standalone drag-enable for entity / citizen info panels ---------------
    // Retail arms the entity panel's title-bar drag through EntityUI's per-frame
    // input handler (exe 0x140066AD0 EntityUI::Slot8), which UNCONDITIONALLY sets
    // this ToolUI controller's +0x4C (drag-enable) = 1 every frame. In standalone
    // that EntityUI input slot is a StubNop — its +0x40 per-frame dispatch runs the
    // page-switching Slot7 instead — so +0x4C is never set and the entity/citizen
    // panel can never arm a drag (Slot9's mouse-down falls through both the +0x40
    // borderTitle branch and the +0x4C pending branch). Utility dialogs are
    // unaffected: they drag via the borderTitle handle (+0x40, set in Slot4) and
    // their datasource is a ComponentDescription, not a ComponentEntity.
    //
    // Mirror EntityUI's drag-enable here, in ToolUI's OWN per-frame slot (this
    // function is g_vt[8], dispatched every frame by DialogInstance::OnUpdate), but
    // only for entity dialogs (datasource at +0x28 is a ComponentEntity). This arms
    // the pending-drag path that Slot9's mouse-down triggers and this function then
    // follows. In DLL mode the real EntityUI handler does it, so this is SA-only.
    if (g_TypeId_ComponentEntity) {
        void* ds = *(void**)(s + 0x28);
        if (ds && fn_IsDerivedFrom(ds, *g_TypeId_ComponentEntity))
            *(uint8_t*)(s + 0x4C) = 1;
    }
#endif

    // Pending drag: check mouse movement threshold
    if (*(char*)(s + 0x4D) != 0) {
        void* keyMap = ***(void****)g_pKeyMapObjectExref;

        // Get mouse X, scaled by dialog scale factor
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisX = fn_KeyMap_GetValue(keyMap, 0, 0);
        float scaleX = *(float*)((uintptr_t)dialogElem + 0xDC);

        // Get mouse Y
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisY = fn_KeyMap_GetValue(keyMap, 0, 1);
        float scaleY = *(float*)((uintptr_t)dialogElem + 0xDC);

        int mouseX = (int)((float)(int)axisX * scaleX);
        int mouseY = (int)((float)(int)axisY * scaleY);

        int deltaX = mouseX - *(int*)(s + 0x50);
        int deltaY = mouseY - *(int*)(s + 0x54);
        if (deltaX < 0) deltaX = -deltaX;
        if (deltaY < 0) deltaY = -deltaY;

        if (deltaX > 5 || deltaY > 5) {
            // Start drag: calc offset from dialog origin
            *(uint8_t*)(s + 0x4D) = 0;
            *(uint8_t*)(s + 0x40) = 1;

            // FUN_14007b940 inline: set dragging flag, calc offsets
            *(uint8_t*)(s + 0x41) = 1;
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
            *(int*)(s + 0x44) = *(int*)(s + 0x50) - *(int*)(*(uintptr_t*)(s + 0x18) + 0x88);
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
            *(int*)(s + 0x48) = *(int*)(s + 0x54) - *(int*)(*(uintptr_t*)(s + 0x18) + 0x8C);
        }

        // Check if mouse button released
        short btn = fn_KeyMap_GetValue(keyMap, 0, 5);
        if (btn == 0) {
            *(uint8_t*)(s + 0x4D) = 0;
        }
    }

    // Active drag: move dialog to follow mouse
    if (*(char*)(s + 0x41) != 0 && *(char*)(s + 0x40) != 0) {
        void* keyMap = ***(void****)g_pKeyMapObjectExref;

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        if (!*(uintptr_t*)(s + 0x18)) { /* dialog instance null, skip drag */ }
        else {
        void* dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisX = fn_KeyMap_GetValue(keyMap, 0, 0);
        float scaleX = *(float*)((uintptr_t)dialogElem + 0xDC);

        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        dialogElem = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        short axisY = fn_KeyMap_GetValue(keyMap, 0, 1);
        float scaleY = *(float*)((uintptr_t)dialogElem + 0xDC);

        int newX = (int)((float)(int)axisX * scaleX) - *(int*)(s + 0x44);
        int newY = (int)((float)(int)axisY * scaleY) - *(int*)(s + 0x48);
        if (newX < 0) newX = 0;
        if (newY < 0) newY = 0;

        // Move dialog
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        fn_DialogInstance_SetUpperLeft(*(void**)(s + 0x18), newX, newY, false);

        // Update entity tracking BST with new position
        uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
        uintptr_t bstRoot = *(uintptr_t*)(worldObj + 0x50);

        // BST lookup by entity key (+0x30)
        uint64_t entityKey = *(uint64_t*)(s + 0x30);
        uintptr_t node = bstRoot;
        while (node != 0) {
            uint64_t nodeKey = *(uint64_t*)node;
            if (entityKey < nodeKey) {
                node = *(uintptr_t*)(node + 0x20);  // left child
            } else if (entityKey > nodeKey) {
                node = *(uintptr_t*)(node + 0x28);  // right child
            } else {
                // Found: update position
                *(int*)(node + 0x08) = newX;
                *(int*)(node + 0x0C) = newY;
                break;
            }
        }

        // If entity is a ComponentEntity, update via SelectEntity
        void* dataSource = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(dataSource, *g_TypeId_ComponentEntity);
        if (isEntity && dataSource != nullptr) {
            void* ds2 = *(void**)(s + 0x28);
            bool check = fn_IsDerivedFrom(ds2, *g_TypeId_ComponentEntity);
            void* entity = check ? ds2 : nullptr;

            uint8_t entityBuf[8];
            fn_InternalBasePtr_Ctor(entityBuf, entity);

            EntityUI_SelectEntity_Impl(
                *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x120),
                entityBuf, newX, newY);
        }

        // Check if mouse button released
        short btn = fn_KeyMap_GetValue(keyMap, 0, 5);
        if (btn == 0) {
            *(uint8_t*)(s + 0x41) = 0;
        }
        } // end else (dialog instance not null)
    }
}

// ============================================================
// ToolUI::Slot9 (vtable +0x48) — Event handler
// ============================================================
// Handles:
//   msg=2, id=0: mouse down — start drag or pending drag
//   msg=5: mouse up — cancel drag state
//   msg=8..10: focus events — bring dialog to front
//   msg=0x12, id=1: close button — close dialog, remove from tracking
//
// Original: 0x14007BC30.

void ToolUI_Slot9(ToolUI* self, void* /*param2*/, int msg, int id,
                   int mouseX, int mouseY) {
    uintptr_t s = (uintptr_t)self;

    if (msg == 2) {
        if (id == 0) {
            if (*(char*)(s + 0x40) != 0) {
                // Already pinned — start drag immediately
                // FUN_14007b940 inline
                *(uint8_t*)(s + 0x41) = 1;
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
                *(int*)(s + 0x44) = mouseX - *(int*)(*(uintptr_t*)(s + 0x18) + 0x88);
                fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
                *(int*)(s + 0x48) = mouseY - *(int*)(*(uintptr_t*)(s + 0x18) + 0x8C);
                return;
            }

            if (*(char*)(s + 0x4C) != 0) {
                // Has UI desc — start pending drag
                *(uint8_t*)(s + 0x4D) = 1;
                *(int*)(s + 0x50) = mouseX;
                *(int*)(s + 0x54) = mouseY;
                return;
            }
        }
    } else if (msg == 5) {
        // Mouse up — cancel both drag modes
        *(uint8_t*)(s + 0x41) = 0;
        *(uint8_t*)(s + 0x4D) = 0;
        return;
    } else if (msg > 7 && msg < 0x0B) {
        // Focus events (8, 9, 10) — bring dialog to front
        uint64_t dialogRef = *(uint64_t*)(s + 0x18);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* userInterface = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        fn_UI_BringDialogToFront(userInterface, (void*)&dialogRef);
        return;
    } else if (msg == 0x12 && id == 1) {
        // Close button — close dialog and remove from entity tracking
#ifdef STANDALONE_EXE
        // Standalone dialogs/controllers are only partially initialized. Validate the
        // pointers this path dereferences before use so a bad field can't fault: the
        // CloseDialog fn, the world singleton (walked for the tracking BST below), and
        // the dialog-instance pointer behind the IBP at +0x18 (deref'd for its
        // UserInterface at +0x10). Range check matches CitUI_OkPtr (canonical Win64
        // user-space heap window). In DLL mode these are always live, so this is inert.
        if (!fn_UI_CloseDialog || !g_pGameWorldSingleton || !*g_pGameWorldSingleton)
            return;
        {
            fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
            uintptr_t di = *(uintptr_t*)(s + 0x18);
            if (di < 0x10000 || di > 0x00007FFFFFFFFFFFULL) return;
        }
#endif
        uint64_t dialogRef = *(uint64_t*)(s + 0x18);
        fn_InternalBasePtr_CheckPtr((void*)(s + 0x18));
        void* userInterface = *(void**)((*(uintptr_t*)(s + 0x18)) + 0x10);
        fn_UI_CloseDialog(userInterface, (void*)&dialogRef);

        // Remove from entity tracking BST
        uintptr_t worldObj = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x120);
        uintptr_t bstRoot = *(uintptr_t*)(worldObj + 0x50);

        uint64_t entityKey = *(uint64_t*)(s + 0x30);
        uintptr_t node = bstRoot;
        while (node != 0) {
            uint64_t nodeKey = *(uint64_t*)node;
            if (entityKey < nodeKey) {
                node = *(uintptr_t*)(node + 0x20);
            } else if (entityKey > nodeKey) {
                node = *(uintptr_t*)(node + 0x28);
            } else {
                // Found: clear pin flag
                *(uint8_t*)(node + 0x10) = 0;
                return;
            }
        }
    }
}
