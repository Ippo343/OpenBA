// code/Game/Dialog/followdialog.cpp
// Clean reimplementation of FollowDialog game logic.
//
// FollowDialog (0xD8 bytes) — camera follow dialog.
// Extends DialogInstance (0xB8 base).
// +0xB8 = entity InternalBasePtr, +0xC0 = (unused here),
// +0xC8 = textFollow element, +0xD0 = textAction element.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>

// ============================================================
// FollowDialog::Slot4 (vtable +0x20) — Setup follow camera UI
// ============================================================
// Finds buttonClose (ID 100), textFollow (+0xC8), textAction (+0xD0).
// Sets textFollow parameter 0 to the followed entity's name.
// Sets follow-active flag on the game world subsystem at +0x120 → +0x38.
//
// Original: 0x140069340.

void FollowDialog_Slot4(FollowDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "buttonClose" and set element ID 100
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonClose");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        if (elem) {
            fn_Element_SetElementId(elem, 100);
        }
    }

    // Find "textFollow" → store at +0xC8, set parameter 0 to entity name
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textFollow");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        *(void**)(s + 0xC8) = elem;

        // Get entity at +0xB8, read its name at +0x28
        void* entity = *(void**)(s + 0xB8);
        if (entity && elem) {
            fn_TextElement_SetParameter(elem, 0, (uint8_t*)((uintptr_t)entity + 0x28));
        }
    }

    // Find "textAction" → store at +0xD0
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textAction");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);

        *(void**)(s + 0xD0) = elem;
    }

    // Set follow-active flag: *(gameWorld+0x120)+0x38 = 1
    void* gameWorld = *g_pGameWorldSingleton;
    void* subsys = *(void**)((uintptr_t)gameWorld + 0x120);
    *(uint8_t*)((uintptr_t)subsys + 0x38) = 1;
}

// ============================================================
// FollowDialog::Slot7 (vtable +0x38) — Update: follow camera position
// ============================================================
// Reads position from the followed entity's CharacterComponent and
// updates the camera target position. Checks for Escape key to close.
//
// Original: 0x140069250.

void FollowDialog_Slot7(FollowDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Get the followed entity from InternalBasePtr at +0xB8
    // CheckPtr validates the reference, then read entity+0x10 to get inner ref
    uintptr_t entityRef = *(uintptr_t*)(s + 0xB8);
    uintptr_t entityInner = *(uintptr_t*)(entityRef + 0x10);

    // Find CharacterComponent on the entity
    uint8_t compBuf[16];
    fn_FindComponent_Character((void*)entityInner, compBuf);
    void* charComp = *(void**)compBuf;

    // Read character position: x at +0x158, z at +0x15C on the component's internal data
    uintptr_t compData = *(uintptr_t*)charComp;
    uint32_t posX = *(uint32_t*)(compData + 0x158);
    uint32_t posZ = *(uint32_t*)(compData + 0x15C);

    // Write position to camera subsystem: gameWorld->+0xE0 -> {+0x1C, +0x20, +0x24}
    void* world = *g_pGameWorldSingleton;
    uintptr_t cameraSys = *(uintptr_t*)((uintptr_t)world + 0xE0);
    *(uint32_t*)(cameraSys + 0x1C) = posX;
    *(uint32_t*)(cameraSys + 0x20) = posZ;
    *(uint32_t*)(cameraSys + 0x24) = 0;

    // Check Escape key (key ID 8) — if pressed, close the follow dialog
    void* keyMap = *(void**)*g_pKeyMapSingleton;
    if (fn_KeyMap_Pressed(keyMap, 0, 8)) {
        fn_FollowDialog_Close(self);
    }
}
