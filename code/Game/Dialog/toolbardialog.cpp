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
//   +0xE0: (unused)
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

            // Element::Child(elem, retBuf, 0) — member function ABI
            uint8_t childBuf[16];
            fn_Element_Child(elem, childBuf, 0);

            void* childElem = *(void**)childBuf;

            // IsDerivedFrom check for TextElement type (exref at 0x140151508)
            // We cast and call SetParameter
            bool isDerived = fn_IsDerivedFrom(childElem, *g_TypeId_ToolbarElement);
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
    fn_ToolbarDialog_UpdateButtons(self);
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

                    // Construct child ToolbarDialog
                    childDialog = fn_ToolbarDialog_Ctor(
                        newDialog,
                        *(void**)(s + 0x10),    // UserInterface
                        *(void**)(s + 0x18),    // Dialog
                        &selfRef,               // parent InternalPtr
                        (void*)item             // toolbar item data
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
                int selfTop = *(int32_t*)(*(uintptr_t*)selfRoot + 0x24);

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
            fn_Toolbar_ShowGroup((void*)gameSys, (int64_t)groupId);
            break;
        }
        case 2: {
            // Utility action
            uint8_t retBuf[16];
            fn_Toolbar_Utility((void*)gameSys, retBuf, *(uint64_t*)(item + 0x58));
            break;
        }
        case 3: {
            // Toggle all groups
            fn_Toolbar_ToggleAll((void*)gameSys);
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
            fn_Toolbar_CenterOnEntity((void*)gameSys, &parentRef, entity);
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
