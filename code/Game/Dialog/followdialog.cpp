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
// FollowDialog::Slot6 (vtable +0x30) — Draw: update profession and action text
// ============================================================
// Called every frame to refresh the follow dialog's text elements:
//   - textFollow (+0xC8): parameter 1 set to current profession description string
//   - textAction (+0xD0): text set to current action string by index
//
// Logic mirrors CitizenUI Slot7's profession/action text update block.
// The citizen entity's CitizenAI component is stored at +0xB8 (InternalBasePtr).
//
// Original: 0x140068F70.

void FollowDialog_Slot6(FollowDialog* self, void* /*p2*/, void* /*p3*/, void* /*p4*/) {
    uintptr_t s = (uintptr_t)self;

    // Get CitizenAI inner ptr from InternalBasePtr at +0xB8
    uintptr_t ai = *(uintptr_t*)(s + 0xB8);

    // --- Profession text (parameter 1 on textFollow at +0xC8) ---
    // If isChild (age < childThreshold) OR hasWorkplace: show child/worker profession
    // Otherwise: show current profession description
    bool isChild = (*(float*)(ai + 0x3C) < *(float*)(*(uintptr_t*)(ai + 0x288) + 0x44));
    bool hasWorkplace = (*(uintptr_t*)(ai + 0x208) != 0);

    {
        uintptr_t profEntry;
        if (isChild || hasWorkplace) {
            // ProfessionList at gameWorld+0xB0 -> +0x40 (child) or +0x48 (worker)
            uintptr_t profListBase = *(uintptr_t*)(*(uintptr_t*)*g_pGameWorldSingleton + 0xB0);
            if (*(uintptr_t*)(ai + 0x208) == 0) {
                profEntry = *(uintptr_t*)(profListBase + 0x40);  // child profession
            } else {
                profEntry = *(uintptr_t*)(profListBase + 0x48);  // worker profession
            }
        } else {
            profEntry = *(uintptr_t*)(ai + 0x1F0);  // current profession description
        }

        // Build UI::String from profession entry's string table (+0x20) and name key (+0x30)
        // Convert to System::String and set as parameter 1 on textFollow
        uint8_t uiStr[16];
        fn_UIString_Ctor(uiStr, *(uint64_t*)(profEntry + 0x20), (void*)(profEntry + 0x30));
        uint8_t sysStr[8];
        fn_UIString_ToSystemString(uiStr, sysStr);

        void* textFollow = *(void**)(s + 0xC8);
        fn_TextElement_SetParameter(textFollow, 1, sysStr);

        // Cleanup System::String
        fn_String_Dtor(sysStr);
    }

    // --- Action text (textAction at +0xD0, set by index from element's string table) ---
    // Find ProfessionComponent on the entity (entity ref at ai+0x10)
    uintptr_t entityRef = *(uintptr_t*)(ai + 0x10);
    uint64_t profCompRef = 0;
    fn_FindProfessionComponent((void*)entityRef, &profCompRef);

    // Read action index from ProfessionComponent+0x28
    int actionIndex = *(int*)(*(uintptr_t*)&profCompRef + 0x28);

    // Special case: action 9 (idle/searching for home)
    // Check if citizen's home tile entity has a different profession — if so, show index 0x10
    if (actionIndex == 9) {
        uintptr_t homeRef = 0;
        int homeTile = *(int*)(ai + 0x30);
        if (homeTile >= 0) {
            // Look up entity at home tile in world grid at gameWorld+0x118
            void* worldGrid = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x118);
            uint64_t foundRef = 0;
            fn_WorldGrid_FindEntity(worldGrid, &foundRef, homeTile);
            homeRef = foundRef;
        }
        if (homeRef != 0) {
            // Get profession description from home entity via vtable[10] (offset +0x50)
            uintptr_t homeEntity = homeRef;
            typedef void* (*fn_VGetDesc_t)(void* self, void* retval);
            void** homeVtable = *(void***)(void*)homeEntity;
            fn_VGetDesc_t vGetDesc = (fn_VGetDesc_t)homeVtable[10];
            uint64_t descRef = 0;
            vGetDesc((void*)homeEntity, &descRef);
            uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
            if (*(uintptr_t*)&descRef != profDesc) {
                actionIndex = 0x10;  // profession mismatch — show "looking for work" string
            }
        }
    }

    // Build UI::String from textAction element's own string table at +0xD8, by index
    {
        void* textAction = *(void**)(s + 0xD0);
        uintptr_t elemPtr = (uintptr_t)textAction;
        uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
        uint8_t uiStr[16];
        fn_UIString_Ctor_Index(uiStr, stringTable, actionIndex);
        uint8_t textVal[16];
        *(uint64_t*)textVal = *(uint64_t*)uiStr;
        *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        fn_TextElement_SetText(textAction, textVal);
    }
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
