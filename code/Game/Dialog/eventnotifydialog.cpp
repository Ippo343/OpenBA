// code/Game/Dialog/eventnotifydialog.cpp
// Clean reimplementation of EventNotifyDialog game logic.
//
// EventNotifyDialog (0xF0 bytes) — notification ribbon for game events.
// Extends DialogInstance (0xB8 base).
//
// Field layout:
//   +0xB8: (base field)
//   +0xC0: float smoothedYOffset (lerped ribbon position)
//   +0xC4: int32 lastEventIndex (last processed event log index)
//   +0xC8: void* notifyEntryArray (dynamic array data pointer)
//   +0xD0: int32 notifyEntryCount
//   +0xD4: int32 notifyEntryCapacity
//   +0xD8: void* activeEventArray (events being displayed, stride 0x30)
//   +0xE0: int32 activeEventCount
//   +0xE8: void* ribbonNotifyElement (ribbon container element)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// EventNotifyDialog::Slot4 (vtable +0x20) — Startup
// ============================================================
// Finds "ribbonNotify" -> stores at +0xE8.
// Finds "notifyContent" container element.
// For each child of notifyContent:
//   - Gets child element (group) -> entry[0]
//   - Finds "notifyImage" sub-element -> entry[1]
//   - Finds "notifyButton" sub-element -> entry[2]
//   - Sets button element ID to 1500 + index
//   - Pushes 0x18-byte entry into array at +0xC8
// Finally copies event log counter to +0xC4.
//
// Original: 0x1400686d0.

void EventNotifyDialog_Slot4(EventNotifyDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "ribbonNotify" -> store raw InternalPtr value at +0xE8
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "ribbonNotify");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        *(uint64_t*)(s + 0xE8) = *(uint64_t*)retBuf;
        fn_String_Dtor(nameStr);
    }

    // Find "notifyContent" element
    void* notifyContent = nullptr;
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "notifyContent");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        notifyContent = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // Iterate children of notifyContent
    int childCount = fn_Element_ChildCount(notifyContent);

    for (int i = 0; i < childCount; i++) {
        // Initialize entry: {group, image, button}
        uint64_t entry[3] = {0, 0, 0};

        // Get child[i] -> entry[0] (group element)
        {
            uint8_t retBuf[16];
            fn_Element_Child(notifyContent, retBuf, i);
            entry[0] = *(uint64_t*)retBuf;
        }

        // Find "notifyImage" in child[i] -> entry[1]
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "notifyImage");

            // Get child[i] again for the FindElement base
            uint8_t childBuf[16];
            fn_Element_Child(notifyContent, childBuf, i);
            void* child = *(void**)childBuf;

            uint8_t retBuf[16];
            fn_FindElement(child, retBuf, nameStr);
            void* found = *(void**)retBuf;
            fn_String_Dtor(nameStr);

            entry[1] = (uint64_t)found;
        }

        // Find "notifyButton" in child[i] -> entry[2]
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "notifyButton");

            uint8_t childBuf[16];
            fn_Element_Child(notifyContent, childBuf, i);
            void* child = *(void**)childBuf;

            uint8_t retBuf[16];
            fn_FindElement(child, retBuf, nameStr);
            void* found = *(void**)retBuf;
            fn_String_Dtor(nameStr);

            entry[2] = (uint64_t)found;
        }

        // Set button element ID to 1500 + i (0x5DC + i)
        fn_Element_SetElementId((void*)entry[2], i + 0x5DC);

        // Push entry into dynamic array at +0xC8
        // The array push function: (arrayBase, newCount, entryPtr)
        int newCount = *(int*)(s + 0xD0) + 1;
        fn_EventNotifyDialog_ArrayPush((void*)(s + 0xC8), newCount, entry);
    }

    // Copy event log counter: self+0xC4 = *(*(world+0x138) + 0x20)
    void* world = *g_pGameWorldSingleton;
    uintptr_t eventLog = *(uintptr_t*)((uintptr_t)world + 0x138);
    *(uint32_t*)(s + 0xC4) = *(uint32_t*)(eventLog + 0x20);
}
