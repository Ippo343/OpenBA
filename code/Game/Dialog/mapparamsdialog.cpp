#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>

// ============================================================
// MapParamsDialog::Startup (0x14006b810) — Slot4
// ============================================================
// Finds all UI elements by name, stores element pointers at fixed offsets.
// Sets button IDs (NewName=0, OK=1, Cancel=2).
// Populates fields from the game world state: map name, seed,
// terrain, terrain size, climate, condition, disaster text.
//
// Original: 0x14006b810
void MapParamsDialog_Startup(MapParamsDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // --- Find and store all named elements ---

    // editName → +0xB8
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "editName");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xB8) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // numberSeed → +0xC0
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "numberSeed");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xC0) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // textCondition → +0xE0
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textCondition");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xE0) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // textTerrain → +0xC8
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textTerrain");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xC8) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // textTerrainSize → +0xD0
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textTerrainSize");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xD0) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // textClimate → +0xD8
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textClimate");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xD8) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // textDisaster → +0xE8
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textDisaster");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(void**)(s + 0xE8) = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // --- Find button elements (local vars, not stored on self) ---
    void* buttonNewName = nullptr;
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonNewName");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        buttonNewName = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    void* buttonCancel = nullptr;
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonCancel");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        buttonCancel = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    void* buttonOk = nullptr;
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonOk");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        buttonOk = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // Store OK button at +0xF0
    *(void**)(s + 0xF0) = buttonOk;

    // --- Set element IDs: NewName=0, OK=1, Cancel=2 ---
    if (buttonNewName) fn_Element_SetElementId(buttonNewName, 0);
    if (buttonOk) fn_Element_SetElementId(buttonOk, 1);
    if (buttonCancel) fn_Element_SetElementId(buttonCancel, 2);

    // Set cancel button close behavior flag (+0xC0 on the element = 8)
    if (buttonCancel) {
        *(uint32_t*)((uintptr_t)buttonCancel + 0xC0) = 8;
    }

    // --- Populate fields from game world ---
    void* gameWorld = *g_pGameWorldSingleton;
    uintptr_t gw = (uintptr_t)gameWorld;

    // Set map name on editName: fn_GameWorld_GetMapName copies gameWorld+0x28 → String
    {
        uint8_t nameBuf[8];
        fn_GameWorld_GetMapName(gameWorld, nameBuf);
        void* editName = *(void**)(s + 0xB8);
        if (editName) fn_EditElement_SetText(editName, nameBuf);
        fn_String_Dtor(nameBuf);
    }

    // Set seed on numberSeed
    {
        int seed = *(int*)(gw + 0x30);
        void* numberSeed = *(void**)(s + 0xC0);
        if (numberSeed) fn_NumberElement_SetValueInt(numberSeed, seed);
    }

    // Set text fields using UI::String (12-byte struct: {ExternalPtr<StringTable>, int})
    // Pattern: read StringTable ptr and string index from description objects,
    // construct a UI::String on the stack, call TextElement::SetText

    // textCondition — from description at gameWorld+0x40, text at desc+0x20/0x28
    {
        uintptr_t desc = *(uintptr_t*)(gw + 0x40);
        if (desc) {
            uint8_t uiStr[16] = {0};
            memcpy(uiStr, (void*)(desc + 0x20), 8);      // ExternalPtr<StringTable>
            memcpy(uiStr + 8, (void*)(desc + 0x28), 4);   // int index
            void* textElem = *(void**)(s + 0xE0);
            if (textElem) fn_TextElement_SetText(textElem, uiStr);
        }
    }

    // textTerrain — from description at gameWorld+0x48, text at desc+0x20/0x28
    {
        uintptr_t desc = *(uintptr_t*)(gw + 0x48);
        if (desc) {
            uint8_t uiStr[16] = {0};
            memcpy(uiStr, (void*)(desc + 0x20), 8);
            memcpy(uiStr + 8, (void*)(desc + 0x28), 4);
            void* textElem = *(void**)(s + 0xC8);
            if (textElem) fn_TextElement_SetText(textElem, uiStr);
        }
    }

    // textTerrainSize — uses the element's own StringTable at element+0xd8
    // and the terrain size int from gameWorld+0x34
    {
        void* textElem = *(void**)(s + 0xD0);
        if (textElem) {
            uintptr_t elem = (uintptr_t)textElem;
            uint32_t terrainSize = *(uint32_t*)(gw + 0x34);
            uint8_t uiStr[16] = {0};
            memcpy(uiStr, (void*)(elem + 0xd8), 8);       // ExternalPtr<StringTable>
            *(uint32_t*)(uiStr + 8) = terrainSize;         // int index
            fn_TextElement_SetText(textElem, uiStr);
        }
    }

    // textClimate — from description at gameWorld+0x50, text at desc+0x20/0x28
    {
        uintptr_t desc = *(uintptr_t*)(gw + 0x50);
        if (desc) {
            uint8_t uiStr[16] = {0};
            memcpy(uiStr, (void*)(desc + 0x20), 8);
            memcpy(uiStr + 8, (void*)(desc + 0x28), 4);
            void* textElem = *(void**)(s + 0xD8);
            if (textElem) fn_TextElement_SetText(textElem, uiStr);
        }
    }

    // textDisaster — from element's own StringTable, bool from gameWorld+0xD0 bit 6
    {
        void* textElem = *(void**)(s + 0xE8);
        if (textElem) {
            uintptr_t elem = (uintptr_t)textElem;
            uint32_t disasterFlags = *(uint32_t*)(gw + 0xD0);
            uint32_t disasterEnabled = ((disasterFlags >> 6) & 1) != 0 ? 1 : 0;
            uint8_t uiStr[16] = {0};
            memcpy(uiStr, (void*)(elem + 0xd8), 8);
            *(uint32_t*)(uiStr + 8) = disasterEnabled;
            fn_TextElement_SetText(textElem, uiStr);
        }
    }
}

// MapParamsDialog::Update (0x14006b790) — Slot6
// Checks if editName field has text; enables/disables OK button accordingly.
// Calls button vtable[8](button, hasText) which is likely Element::SetEnabled.
void MapParamsDialog_Update(MapParamsDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // OK button element at self+0xF0
    void* okButton = *(void**)(s + 0xF0);

    // editName element at self+0xB8
    void* editName = *(void**)(s + 0xB8);

    // Get text data pointer from editName element (+0xC8 = String internal data)
    void* textData = *(void**)((uint8_t*)editName + 0xC8);

    // System::String stores length at data-8 (COW refcounted)
    int textLen = 0;
    if (textData != nullptr) {
        textLen = *(int*)((uint8_t*)textData - 8);
    }

    // Enable/disable OK button based on text length
    // Virtual call: vtable[8](button, enabled)
    typedef void (*fn_VSlot8_t)(void* self, bool enabled);
    void** vtable = *(void***)okButton;
    ((fn_VSlot8_t)vtable[8])(okButton, textLen > 0);
}
