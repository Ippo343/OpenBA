#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// EventPopupDialog::Slot4 (0x140068C20) — Setup
// Finds imageIcon, textEvent, buttonOk elements.
// Reads event entry from world event list using index at self+0xB8.
// Sets imageIcon sprite to "EventIcon<N>", sets textEvent parameter 0
// to event text, sets buttonOk element ID to 200.
void EventPopupDialog_Slot4(EventPopupDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find imageIcon element
    uint8_t nameStr[8], retBuf[16];
    fn_String_Ctor_CStr(nameStr, "imageIcon");
    fn_DialogInstance_FindElement(self, retBuf, nameStr);
    void* imageIcon = *(void**)retBuf;
    fn_String_Dtor(nameStr);

    // Find textEvent element
    fn_String_Ctor_CStr(nameStr, "textEvent");
    fn_DialogInstance_FindElement(self, retBuf, nameStr);
    void* textEvent = *(void**)retBuf;
    fn_String_Dtor(nameStr);

    // Find buttonOk element
    fn_String_Ctor_CStr(nameStr, "buttonOk");
    fn_DialogInstance_FindElement(self, retBuf, nameStr);
    void* buttonOk = *(void**)retBuf;
    fn_String_Dtor(nameStr);

    // Get event entry from world event list
    // world->+0x138 -> event manager, +0x18 -> event array
    // Each entry is 0x28 bytes; event index at self+0xB8
    void* world = *g_pGameWorldSingleton;
    uint8_t* eventMgr = *(uint8_t**)((uint8_t*)world + 0x138);
    uint8_t* eventArray = *(uint8_t**)(eventMgr + 0x18);
    int32_t eventIndex = *(int32_t*)(s + 0xB8);
    uint8_t* eventEntry = eventArray + (int64_t)eventIndex * 0x28;

    // Format sprite name "EventIcon<iconIdx>"
    uint32_t iconIdx = *(uint32_t*)(eventEntry + 0x20);
    uint8_t fmtStr[8];
    fn_String_Format(fmtStr, "EventIcon%i", iconIdx);

    // Get current sprite from imageIcon (16 bytes: sheetPtr + index)
    uint8_t oldSprite[16];
    fn_ImageElement_GetSprite(imageIcon, oldSprite);
    void* sheetPtr = *(void**)oldSprite;

    // Build new sprite: same sheet, new index from name
    int newIndex = -1;
    if (sheetPtr) {
        newIndex = fn_SpriteSheet_GetSpriteIndexFromName(sheetPtr, fmtStr);
    }

    // Construct 16-byte Sprite: {sheetPtr, index(4b) + pad(4b)}
    uint8_t newSprite[16];
    *(void**)newSprite = sheetPtr;
    *(int32_t*)(newSprite + 8) = newIndex;
    *(int32_t*)(newSprite + 12) = 0;

    // Set new sprite on imageIcon
    fn_ImageElement_SetSprite(imageIcon, newSprite);
    fn_String_Dtor(fmtStr);

    // Set text parameter 0 on textEvent to event text string at eventEntry+0x08
    fn_TextElement_SetParameter(textEvent, 0, eventEntry + 0x08);

    // Set button ID 200 on buttonOk
    fn_Element_SetElementId(buttonOk, 200);
}
