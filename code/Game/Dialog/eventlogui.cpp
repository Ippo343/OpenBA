// engine/src/ui/eventlogui.cpp
// Clean reimplementation of EventLogUI game logic.
//
// EventLogUI (0x80 bytes) — event log panel.
// Extends DialogController (0x30 base).
// +0x30 = buttonBottom, +0x38 = buttonPrevious, +0x40 = buttonNext,
// +0x48 = scroll offset (int), +0x50/+0x58 = display text element array (ptrs),
// +0x60/+0x68 = display button element array (ptrs),
// +0x70/+0x78 = notification button array (ptrs, dynamic).
//
// Element IDs: buttonPrevious=100, buttonNext=0x65, buttonBottom=0x66,
//              notification buttons=0x67..0x6B, display rows>=0x6C.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// EventLogUI::Slot5 (vtable +0x28) — Setup event log display
// ============================================================
// Finds navigation buttons, "display" container, notification buttons.
// Stores button ptrs, resizes display arrays, sets element IDs.
//
// Original: 0x140067640.

void EventLogUI_Slot5(EventLogUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);
    void* dialogInst = *(void**)(s + 0x18);

    // --- Find navigation buttons ---
    // "buttonPrevious" -> +0x38
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "buttonPrevious");
        fn_FindElement(dialogElem, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        uint8_t bpBuf[8];
        fn_InternalBasePtr_Ctor(bpBuf, elem);
        *(uint64_t*)(s + 0x38) = *(uint64_t*)bpBuf;
    }

    // "buttonNext" -> +0x40
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "buttonNext");
        fn_FindElement(dialogElem, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        uint8_t bpBuf[8];
        fn_InternalBasePtr_Ctor(bpBuf, elem);
        *(uint64_t*)(s + 0x40) = *(uint64_t*)bpBuf;
    }

    // "buttonBottom" -> +0x30
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "buttonBottom");
        fn_FindElement(dialogElem, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        uint8_t bpBuf[8];
        fn_InternalBasePtr_Ctor(bpBuf, elem);
        *(uint64_t*)(s + 0x30) = *(uint64_t*)bpBuf;
    }

    // Set element IDs on buttons
    fn_Element_SetElementId(*(void**)(s + 0x38), 100);   // buttonPrevious = 100
    fn_Element_SetElementId(*(void**)(s + 0x40), 0x65);  // buttonNext = 0x65
    fn_Element_SetElementId(*(void**)(s + 0x30), 0x66);  // buttonBottom = 0x66

    // --- Find "display" container element ---
    void* displayElem;
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "display");
        fn_FindElement(dialogElem, retBuf, nameStr);
        void* elem = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        uint8_t bpBuf[8];
        fn_InternalBasePtr_Ctor(bpBuf, elem);
        displayElem = *(void**)bpBuf;
    }

    // Resize display text array (+0x50) and display button array (+0x60) to row count
    int displayRowCount = fn_Element_ChildCount(displayElem);
    fn_ExeArrayResize((void*)(s + 0x50), displayRowCount);
    fn_ExeArrayResize((void*)(s + 0x60), displayRowCount);

    // Iterate display rows: store child text element in +0x50 array,
    // find button child, store in +0x60 array, set element IDs
    int rowCount = *(int*)(s + 0x58);
    for (int i = 0; i < rowCount; i++) {
        // Get child text element (direct child at index i)
        uint8_t childBuf[8];
        fn_Element_Child(displayElem, childBuf, i);
        *(uint64_t*)(*(uintptr_t*)(s + 0x50) + i * 8) = *(uint64_t*)childBuf;

        // Get button child: Child(display, i) -> find typed element in that child
        fn_Element_Child(displayElem, childBuf, i);
        void* rowChild = *(void**)childBuf;
        // The button child is obtained by IsDerivedFrom check (generic element type)
        uint8_t bpBuf[8];
        fn_InternalBasePtr_Ctor(bpBuf, rowChild);
        *(uint64_t*)(*(uintptr_t*)(s + 0x60) + i * 8) = *(uint64_t*)bpBuf;

        // Set element ID on the button: 0x6C + index
        fn_Element_SetElementId(*(void**)(*(uintptr_t*)(s + 0x60) + i * 8), 0x6C + i);
    }

    // Reset scroll offset to 0
    *(int*)(s + 0x48) = 0;

    // --- Find "notifybuttons" container in dialog instance ---
    void* notifyContainer;
    {
        uint8_t nameStr[8], retBuf[16];
        fn_String_Ctor_CStr(nameStr, "notifybuttons");
        fn_DialogInstance_FindElement(dialogInst, retBuf, nameStr);
        notifyContainer = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // Iterate notification button children, store in dynamic array at +0x70, set IDs
    int notifyCount = fn_Element_ChildCount(notifyContainer);
    void* eventLogSingleton = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);

    for (int i = 0; i < notifyCount; i++) {
        // Get child button element
        uint8_t childBuf[8];
        fn_Element_Child(notifyContainer, childBuf, i);
        void* childElem = *(void**)childBuf;
        uint8_t bpBuf[8];
        fn_InternalBasePtr_Ctor(bpBuf, childElem);
        void* btnElem = *(void**)bpBuf;

        // Set element ID: 0x67 + index
        fn_Element_SetElementId(btnElem, 0x67 + i);

        // Copy notification flag from event log singleton: singleton[i] + 0x10 -> button element +0xD8
        *(uint64_t*)((uintptr_t)btnElem + 0xd8) =
            *(uint64_t*)((uintptr_t)eventLogSingleton + 0x10 + i * sizeof(uintptr_t));

        // Push button ptr to notification array at +0x70
        // The +0x70 field is a dynamic array: +0x70=data, +0x78=count, +0x7C=capacity
        uint32_t count = *(uint32_t*)(s + 0x78);
        uint32_t newCount = count + 1;
        *(uint32_t*)(s + 0x78) = newCount;
        if (*(int*)(s + 0x7c) < (int)newCount) {
            *(uint32_t*)(s + 0x7c) = (count != 0) ? newCount * 3 : newCount;
            void* oldData = *(void**)(s + 0x70);
            void* newData = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)*(int*)(s + 0x7c) * 8, 8);
            *(void**)(s + 0x70) = newData;
            // Copy existing entries if buffer changed
            if (oldData != newData) {
                for (uint32_t j = 0; j < count; j++) {
                    ((uint64_t*)newData)[j] = ((uint64_t*)oldData)[j];
                }
                fn_MemFreeStatic(oldData);
            }
        }
        // Store the new button ptr
        ((uint64_t*)*(void**)(s + 0x70))[count] = (uint64_t)btnElem;
    }
}

// ============================================================
// EventLogUI::Slot7 (vtable +0x38) — Update event log entries
// ============================================================
// Iterates display rows from bottom to top, filling each with event
// data from the event log singleton. Updates text parameters and
// button visibility based on scroll offset. Enables/disables
// prev/next scroll buttons.
//
// Original: 0x140067440.

void EventLogUI_Slot7(EventLogUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Event log singleton: g_pGameWorldSingleton -> +0x138
    uintptr_t eventLog = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
    int totalEvents = *(int*)(eventLog + 0x20);  // total event count

    int displayCount = *(int*)(s + 0x58);  // number of display rows
    int scrollOffset = *(int*)(s + 0x48);

    // Fill rows from bottom (displayCount-1) to top (0)
    for (int i = displayCount - 1; i >= 0; i--) {
        int eventIdx = (totalEvents - scrollOffset - 1) - i;

        void* textElem = *(void**)(*(uintptr_t*)(s + 0x50) + i * 8);
        void* btnElem = *(void**)(*(uintptr_t*)(s + 0x60) + i * 8);

        if (eventIdx < 0) {
            // No event for this row — blank it out
            uint8_t emptyStr[8];
            fn_String_Ctor_CStr(emptyStr, "");
            fn_TextElement_SetParameter(textElem, 0, emptyStr);
            fn_String_Dtor(emptyStr);
            // Call Update(visible=false) on button element: vtable[8]
            void** vtbl = *(void***)btnElem;
            typedef void (*VCall_Update_t)(void*, int);
            ((VCall_Update_t)vtbl[8])(btnElem, 0);
        } else {
            // Get event entry: eventLog+0x18 = event array, each entry is 0x28 bytes
            uintptr_t eventArray = *(uintptr_t*)(eventLog + 0x18);
            uintptr_t eventEntry = eventArray + (uintptr_t)eventIdx * 0x28;

            // Set text parameter from event entry +0x08 (event text)
            fn_TextElement_SetParameter(textElem, 0, (void*)(eventEntry + 0x08));

            // Set text color from event entry +0x00 (color/flags value)
            // vtable[10] (0x50): SetColor virtual call
            void** vtbl = *(void***)textElem;
            typedef void (*VCall_SetColor_t)(void*, uint32_t);
            ((VCall_SetColor_t)vtbl[10])(textElem, *(uint32_t*)eventEntry);

            // Call Update(visible=true) on button element
            void** btnVtbl = *(void***)btnElem;
            typedef void (*VCall_Update_t)(void*, int);
            ((VCall_Update_t)btnVtbl[8])(btnElem, 0);
        }
    }

    // Enable/disable buttonBottom (+0x30) based on scroll offset > 0
    {
        void* btnBottom = *(void**)(s + 0x30);
        void** vtbl = *(void***)btnBottom;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnBottom, scrollOffset > 0);
    }

    // Enable/disable buttonNext (+0x40) based on scroll offset > 0
    {
        void* btnNext = *(void**)(s + 0x40);
        void** vtbl = *(void***)btnNext;
        typedef void (*VCall_Enable_t)(void*, int);
        ((VCall_Enable_t)vtbl[8])(btnNext, scrollOffset > 0);
    }
}

// ============================================================
// EventLogUI::Slot9 (vtable +0x48) — Button dispatch
// ============================================================
// Handles scroll buttons, notification toggles, and row click navigation.
//
// Original: 0x1400675D0.

void EventLogUI_Slot9(EventLogUI* self, void* param2, int msg, int id) {
    (void)param2;
    if (msg != 0x12) return;

    uintptr_t s = (uintptr_t)self;
    int* scrollOffset = (int*)(s + 0x48);

    if (id == 100) {
        // buttonPrevious — scroll to older events
        (*scrollOffset)++;
    } else if (id == 0x65) {
        // buttonNext — scroll to newer events
        (*scrollOffset)--;
    } else if (id == 0x66) {
        // buttonBottom — jump to most recent
        *scrollOffset = 0;
    } else if (id >= 0x67 && id <= 0x6B) {
        // Notification toggle buttons (5 categories)
        // FUN_140067320: reads flag from button element at +0xD8, copies to event log global +0x10+index
        {
            int btnIdx = id - 0x67;
            // Button ptr is stored in the notification array at +0x70 (array of 8-byte ptrs)
            void* ibp = (void*)(*(uintptr_t*)(s + 0x70) + (uintptr_t)btnIdx * 8);
            fn_InternalBasePtr_CheckPtr(ibp);
            void* btnElem = *(void**)ibp;
            // Read button's flag from +0xD8 and write to event log singleton +0x10+btnIdx
            uintptr_t eventLog = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
            *(uint8_t*)(eventLog + 0x10 + btnIdx) = *(uint8_t*)((uintptr_t)btnElem + 0xD8);
        }
        return;
    } else if (id >= 0x6C) {
        // Display row click — navigate camera to event location
        // FUN_140067370: computes tile index from event log, calls terrain pick, sets camera target
        {
            int rowIdx = id - 0x6C;
            // Get event log
            uintptr_t eventLog = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
            int totalEvents = *(int*)(eventLog + 0x20);
            int displayCount = *(int*)(s + 0x58);
            int scrollOffset = *(int*)(s + 0x48);
            int eventIdx = (totalEvents - displayCount - scrollOffset) + rowIdx;
            // Get tile index from event entry at entryArray + eventIdx*0x28 + 0x18
            uintptr_t entryArray = *(uintptr_t*)(eventLog + 0x18);
            int tileIdx = *(int*)(entryArray + 0x18 + (uintptr_t)eventIdx * 0x28);
            if (tileIdx >= 0) {
                int* mapGrid = *(int**)((uintptr_t)(*g_pGameWorldSingleton) + 0xF8);
                int gridSize = *mapGrid;
                float halfSize = (float)(gridSize >> 1) - 0.5f;
                float inXY[3] = {
                    (float)(tileIdx % gridSize) - halfSize,
                    (float)(tileIdx / gridSize) - halfSize,
                    0.0f
                };
                float outPos[3] = {};
                if (fn_MapGetTerrainPos) {
                    fn_MapGetTerrainPos(mapGrid, outPos, inXY, '\0', nullptr);
                } else {
                    outPos[0] = inXY[0]; outPos[1] = inXY[1]; outPos[2] = 0.0f;
                }
                uintptr_t camTarget = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0xE0);
                *(float*)(camTarget + 0x1C) = outPos[0];
                *(float*)(camTarget + 0x20) = outPos[1];
                *(float*)(camTarget + 0x24) = outPos[2];
            }
        }
        return;
    }

    // Clamp scroll offset to >= 0
    if (*scrollOffset < 0) *scrollOffset = 0;
}

// EventLogUI_Slot6 — inline in ui_panels.h (3-byte no-op, 0x140067630)
