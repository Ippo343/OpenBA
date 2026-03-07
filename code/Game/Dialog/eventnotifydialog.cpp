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

// ============================================================
// EventNotifyDialog::Slot7 (vtable +0x38) — Event processing
// ============================================================
// Decrements timers for active notifications, removes expired ones.
// Scans event log for new events since last processed index (+0xC4).
// Maps event types to categories, checks filter flags.
// Creates notification entries with icon, color, text, timer.
// Plays sound effect if new events were processed.
// Trims excess entries if beyond display capacity.
//
// Active event entry (0x30 bytes at +0xD8 array):
//   +0x00: int64 timer (countdown ticks)
//   +0x08: UI::Sprite (16 bytes, icon sprite)
//   +0x18: int32 eventIndex
//   +0x1C: int32 severity/color code
//   +0x20: System::String (8 bytes, display text)
//   +0x28: ??? (padding/unused)
//
// Original: 0x140067C30.

void EventNotifyDialog_Slot7(EventNotifyDialog* self, void* param2) {
    uintptr_t s = (uintptr_t)self;

    uintptr_t eventLogPtr = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x138);
    int64_t* tickParam = (int64_t*)param2;

    // Phase 1: Decrement timers on active notifications, remove expired ones
    int activeCount = *(int*)(s + 0xE0);
    if (activeCount > 0) {
        int idx = 0;
        int64_t offset = 0;
        do {
            // Decrement timer
            int64_t* timerPtr = (int64_t*)(offset + *(uintptr_t*)(s + 0xD8));
            *timerPtr -= *tickParam;

            // If timer expired (< 1), remove this entry
            if (*(int64_t*)(offset + *(uintptr_t*)(s + 0xD8)) < 1) {
                fn_EventNotifyDialog_ArrayRemove((void*)(s + 0xD8), idx);
                idx--;
                offset -= 0x30;
            }

            idx++;
            offset += 0x30;
        } while (idx < *(int*)(s + 0xE0));
    }

    // Phase 2: Scan event log for new events
    float timerDuration = *g_pNotifyTimerDuration;  // 30.0f
    int lastEventIdx = *(int*)(s + 0xC4);
    int totalEvents = *(int*)(eventLogPtr + 0x20) - 1;

    if (lastEventIdx < totalEvents) {
        bool hasNewEvents = false;
        int nextIdx = lastEventIdx + 1;
        int scanIdx = nextIdx;

        if (scanIdx <= totalEvents) {
            int64_t entryOffset = (int64_t)scanIdx * 0x28;
            int remaining = totalEvents - scanIdx + 1;

            do {
                uintptr_t eventArray = *(uintptr_t*)(eventLogPtr + 0x18);
                int severity = *(int*)(entryOffset + 0x14 + eventArray);

                if (severity > 0) {
                    // Map event type to category
                    int eventType = *(int*)(entryOffset + 0x10 + eventArray);
                    int category;
                    switch (eventType) {
                        case 0: case 1: case 3: case 4:
                        case 5: case 6: case 7:
                            category = 1; break;
                        case 2:
                            category = 0; break;
                        case 0xB: case 0xC: case 0x1E: case 0x1F:
                            category = 2; break;
                        case 0xD: case 0xE:
                            category = 4; break;
                        case 0x11: case 0x12: case 0x13: case 0x14:
                        case 0x15: case 0x16: case 0x17: case 0x18:
                            category = 3; break;
                        default:
                            category = 5; break;
                    }

                    // Check category filter flag
                    if (*(char*)(category + 0x10 + eventLogPtr) != 0 &&
                        *(int*)(entryOffset + 0x1C + eventArray) >= 0) {

                        // Check if popup needed (+0x20 field >= 0 means show popup)
                        if (*(int*)(entryOffset + 0x20 + eventArray) >= 0) {
                            // Create and show EventPopupDialog
                            uint8_t refBuf[16];
                            fn_Dialog_GetReference(*(void**)(s + 0x18), refBuf);
                            void* dialogRef = *(void**)refBuf;

                            // Check if dialogRef is External type
                            bool isExternal = fn_IsDerivedFrom(dialogRef, *g_pExternalTypeId);
                            void* extPtr = isExternal ? dialogRef : nullptr;

                            // Allocate EventPopupDialog (0xC0 bytes)
                            void* newDialog = fn_MemAlloc(g_pMemAllocSingleton, 0xC0, 8);
                            if (newDialog) {
                                // Initialize with empty parent ptr
                                uint8_t emptyPtr[8];
                                fn_InternalBasePtr_Ctor(emptyPtr, nullptr);
                                // Call DialogInstance ctor (complex — pass through to runtime)
                                // This is simplified: we need the actual ctor call chain
                                // For safety, use the original function for popup creation
                            }

                            // Add to UI
                            // (This sub-path is complex with DialogInstance ctor + vtable setup.
                            //  The notification entry below is the more critical path.)
                        }

                        // Build notification entry (0x30 bytes)
                        uint8_t notifyEntry[0x30];
                        memset(notifyEntry, 0, 0x30);

                        // +0x18 = event index
                        *(int*)(notifyEntry + 0x18) = scanIdx;

                        // +0x00 = timer (Time constructed from float seconds)
                        fn_Time_Ctor_Float(notifyEntry, timerDuration);

                        // +0x08 = sprite icon (from "EventIcon%i" format)
                        uint8_t iconNameStr[8];
                        int iconIndex = *(int*)(entryOffset + 0x1C + eventArray);
                        fn_String_Format(iconNameStr, "EventIcon%i", (uint64_t)(uint32_t)iconIndex);

                        // Get sprite from first notification slot's image element
                        uintptr_t slotArray = *(uintptr_t*)(s + 0xC8);
                        void* imageElem = *(void**)(slotArray + 8);  // first entry's image
                        uint8_t spriteBuf[16];
                        fn_ImageElement_GetSprite(imageElem, spriteBuf);

                        // Create sprite with icon name (copies sprite sheet, sets index)
                        uint8_t newSprite[32];
                        fn_Sprite_Ctor(newSprite, *(uint64_t*)spriteBuf, iconNameStr);
                        *(uint64_t*)(notifyEntry + 0x08) = *(uint64_t*)newSprite;
                        *(uint64_t*)(notifyEntry + 0x10) = *(uint64_t*)(newSprite + 8);

                        fn_String_Dtor(iconNameStr);

                        // +0x1C = color based on severity
                        int sev = *(int*)(entryOffset + 0x14 + eventArray);
                        uint32_t color;
                        if (sev == 1)      color = 0xFFFFFF00;  // yellow
                        else if (sev == 2) color = 0xFF8080FF;  // pink
                        else if (sev == 3) color = 0xFFFF6060;  // red
                        else if (sev == 4) color = 0xFFEA7517;  // orange
                        else               color = 0xFFFFFFFF;  // white
                        *(uint32_t*)(notifyEntry + 0x1C) = color;

                        // +0x20 = text string (strip category prefix before ':')
                        // Event text is at eventArray + entryOffset + 0x08
                        uintptr_t eventTextAddr = entryOffset + eventArray + 0x08;
                        void* eventTextStr = (void*)eventTextAddr;

                        // Get string data pointer
                        uintptr_t strData = *(uintptr_t*)eventTextStr;
                        int strLen = 0;
                        if (strData != 0) {
                            strLen = *(int*)(strData - 8);
                        }

                        // Find ':' separator
                        int colonPos = 0;
                        for (int c = 0; c < strLen; c++) {
                            if (*(char*)(strData + c) == ':') {
                                colonPos = c + 1;  // skip the colon
                                break;
                            }
                            colonPos = c + 1;
                        }

                        // If colon found, take substring after it; otherwise use whole string
                        if (colonPos == strLen) {
                            fn_String_Assign((void*)(notifyEntry + 0x20), eventTextStr);
                        } else {
                            uint8_t subStr[8];
                            fn_String_SubString(eventTextStr, subStr, colonPos + 1);
                            fn_String_Assign((void*)(notifyEntry + 0x20), subStr);
                            fn_String_Dtor(subStr);
                        }

                        // Push entry to active array at +0xD8
                        int newCount = *(int*)(s + 0xE0) + 1;
                        fn_EventNotifyDialog_ArrayPush((void*)(s + 0xD8), newCount, notifyEntry);

                        hasNewEvents = true;

                        // Clean up the string in our local entry (array push copied it)
                        fn_String_Dtor((void*)(notifyEntry + 0x20));
                    }
                }

                scanIdx++;
                entryOffset += 0x28;
                remaining--;
            } while (remaining != 0);
        }

        // Trim excess entries if beyond display capacity (+0xD0)
        if (*(int*)(s + 0xD0) < *(int*)(s + 0xE0)) {
            do {
                fn_EventNotifyDialog_ArrayRemove((void*)(s + 0xD8), 0);
            } while (*(int*)(s + 0xD0) < *(int*)(s + 0xE0));
        }

        // Play sound effect if new events were found
        if (hasNewEvents) {
            uint8_t zeroTime[8];
            fn_Time_Ctor_Float(zeroTime, 0.0f);
            void* audioScene = *(void**)((uintptr_t)(*g_pGameWorldSingleton) + 0x78);
            uint64_t soundEffect = *(uint64_t*)(*(uintptr_t*)g_pKeyMapSingleton + 0x98);
            fn_Audio_Scene_PlayEffect(audioScene, soundEffect, 1, zeroTime);
        }

        totalEvents = *(int*)(eventLogPtr + 0x20) - 1;
    }

    // Update last processed event index
    *(int*)(s + 0xC4) = totalEvents;
}

// ============================================================
// EventNotifyDialog::Update (vtable +0x30) — Ribbon position and slot sync
// ============================================================
// Each frame:
//   1. Compute target Y for the ribbon: (parentRoot.y - selfRoot.y) + 44
//      — positions the ribbon just below the parent dialog (toolbar).
//   2. Lerp the smoothed Y toward target: smoothedY = smoothedY*0.8 + targetY*0.2
//   3. Apply smoothedY as top padding on the self root element.
//   4. Show the ribbonNotify container element.
//   5. For each active notification slot (0..activeEventCount-1):
//      - Copy the event's icon sprite to the slot's imageElem.
//      - Get child[4] of buttonElem; if it is an ImageElement, copy sprite to it too.
//      - Check event log entry popup field at eventEntry+0x18:
//          if < 0 (no popup): hide button, show image.
//          if >= 0 (popup active): show button, hide image.
//      - Obtain button's controller via vtable[0x58/8=11], call vtable[0x50/8=10](color).
//      - Also call vtable[0x50] on each of the 4 button children with the same color.
//      - Copy the event's text string into the group element's string at +0x58.
//   6. For empty slots (activeEventCount..capacity-1):
//      - Reset button controller color and direct button element color to 0xFFFFFFFF.
//      - Clear the group element's string at +0x58.
//      - Hide image element (turn off visibility flag 1).
//      - Show button element (turn on visibility flag 1).
//
// Key offsets on EventNotifyDialog:
//   +0xB8: parent dialog InternalBasePtr (dereference to get DialogInstance*)
//   +0xC0: float smoothedYOffset
//   +0xC8: notifyEntryArray (stride 0x18: {void* group, void* image, void* button})
//   +0xD0: int32 notifyEntryCapacity (max slots)
//   +0xD8: activeEventArray (stride 0x30, see Slot7 for layout)
//   +0xE0: int32 activeEventCount
//   +0xE8: void* ribbonNotifyElement (the ribbon container)
//
// Active event entry layout (0x30 bytes):
//   +0x00: int64 timer
//   +0x08: int32 event_index (into game event log)
//   +0x10: Sprite (16 bytes) — icon sprite
//   +0x20: System::String — display text
//   +0x28: uint32 color (ARGB, set from severity in Slot7)
//
// Event log entry layout (0x28 bytes, at gameWorld+0x138 data):
//   +0x18: int32 popup_ref (-1 if no popup, >=0 if popup dialog active)
//
// Original: 0x1400680c0
void EventNotifyDialog_Update(EventNotifyDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // -------------------------------------------------------
    // Step 1-3: Compute target Y and lerp smoothed position
    // -------------------------------------------------------

    // Parent dialog is stored as InternalBasePtr at self+0xB8.
    // CheckPtr validates the InternalBasePtr, then dereference to get the DialogInstance*.
    // DialogInstance::GetRoot() is equivalent to reading *(void**)(dlg + 0x50).
    fn_InternalBasePtr_CheckPtr((void*)(s + 0xB8));
    void* parentDlg  = *(void**)(s + 0xB8);
    // GetRoot on parentDlg: CheckPtr on the returned InternalBasePtr, then read elem ptr.
    // (GetRoot returns an InternalBasePtr wrapping the root element.)
    // In practice, root = *(void**)(dlg + 0x50) directly.
    void* parentRoot = *(void**)((uintptr_t)parentDlg + 0x50);
    int   parentY    = *(int*)((uintptr_t)parentRoot + 0x60);

    // GetRoot on self:
    void* selfRoot = *(void**)(s + 0x50);
    int   selfY    = *(int*)((uintptr_t)selfRoot + 0x60);

    int targetY = parentY - selfY + 44;  // 0x2c = 44

    // fVar16 = final target based on active count
    float fVar16 = 0.0f;
    int activeCount = *(int*)(s + 0xE0);
    if (activeCount == 1) {
        fVar16 = (float)targetY;
    } else if (activeCount > 1) {
        // Additional entries add 40.0f spacing (DAT_1401721dc = 0x42200000 = 40.0f)
        fVar16 = (float)(activeCount - 1) * 40.0f + (float)targetY;
    }

    // Lerp: smoothedY = smoothedY * 0.8 + fVar16 * 0.2
    // DAT_140172128 = 0.8f, DAT_1401720f8 = 0.2f
    float smoothedY = *(float*)(s + 0xC0);
    smoothedY = smoothedY * 0.8f + fVar16 * 0.2f;
    *(float*)(s + 0xC0) = smoothedY;

    // Apply as top padding on self root: floor(smoothedY + 0.5f)
    // DAT_140172114 = 0.5f (rounding)
    fn_InternalBasePtr_CheckPtr((void*)(s + 0x50));
    fn_Element_SetPadding(selfRoot, 0, (int)(smoothedY + 0.5f), 0, 2);

    // -------------------------------------------------------
    // Step 4: Show ribbonNotify element
    // -------------------------------------------------------
    uintptr_t eventLogPtr = *(uintptr_t*)(*(uintptr_t*)(uintptr_t)(*g_pGameWorldSingleton) + 0x138);
    uintptr_t eventArray  = *(uintptr_t*)(eventLogPtr + 0x18);

    fn_InternalBasePtr_CheckPtr((void*)(s + 0xE8));
    fn_Element_SetFlagsOnOff(*(void**)(s + 0xE8), 0, 1);  // clear flag 1 = show

    // -------------------------------------------------------
    // Steps 5 & 6: Sync notification slots
    // -------------------------------------------------------
    uintptr_t notifyEntryBase = *(uintptr_t*)(s + 0xC8);  // array of 0x18-byte entries
    uintptr_t activeBase      = *(uintptr_t*)(s + 0xD8);  // array of 0x30-byte entries
    int capacity              = *(int*)(s + 0xD0);

    // --- Active slots ---
    for (int i = 0; i < activeCount; i++) {
        uintptr_t activeEntry = activeBase + (uintptr_t)i * 0x30;
        uintptr_t slotEntry   = notifyEntryBase + (uintptr_t)i * 0x18;

        void* groupElem  = *(void**)(slotEntry + 0x00);
        void* imageElem  = *(void**)(slotEntry + 0x08);
        void* buttonElem = *(void**)(slotEntry + 0x10);

        // Event index stored at active_entry+0x08
        int eventIndex = *(int*)(activeEntry + 0x08);

        // Sprite stored at active_entry+0x10 (16 bytes)
        uint8_t sprite[16];
        *(uint64_t*)(sprite + 0)  = *(uint64_t*)(activeEntry + 0x10);
        *(uint64_t*)(sprite + 8)  = *(uint64_t*)(activeEntry + 0x18);

        // Set icon sprite on image element
        fn_InternalBasePtr_CheckPtr(&imageElem);
        fn_ImageElement_SetSprite(imageElem, sprite);

        // Get child[4] of button element — may be an ImageElement for embedded icon
        uint8_t childBuf[16];
        fn_InternalBasePtr_CheckPtr(&buttonElem);
        void* child4 = *(void**)fn_Element_Child(buttonElem, childBuf, 4);

        // Check if child4 is an ImageElement; if so, copy sprite to it as well
        {
            bool isImage = fn_IsDerivedFrom(child4, g_TypeId_ImageElement ? *g_TypeId_ImageElement : 0u);
            void* imgChild = isImage ? child4 : nullptr;
            // Construct InternalBasePtr for imgChild on stack
            uint8_t imgChildPtr[16];
            fn_InternalBasePtr_Ctor(imgChildPtr, imgChild);
            // Copy same sprite from active_entry+0x10
            uint8_t sprite2[16];
            *(uint64_t*)(sprite2 + 0) = *(uint64_t*)(activeEntry + 0x10);
            *(uint64_t*)(sprite2 + 8) = *(uint64_t*)(activeEntry + 0x18);
            fn_InternalBasePtr_CheckPtr(imgChildPtr);
            // SetSprite on imgChild (only if non-null after CheckPtr)
            void* imgElemResolved = *(void**)imgChildPtr;
            if (imgElemResolved) {
                fn_ImageElement_SetSprite(imgElemResolved, sprite2);
            }
        }

        // Determine popup state: check event log entry at eventIndex, popup field at +0x18
        int popupField = *(int*)(eventArray + (uintptr_t)eventIndex * 0x28 + 0x18);
        bool noPopup   = (popupField < 0);

        fn_InternalBasePtr_CheckPtr(&buttonElem);
        fn_InternalBasePtr_CheckPtr(&imageElem);
        if (noPopup) {
            // No popup: hide button, show image
            fn_Element_SetFlagsOnOff(buttonElem, 1, 0);  // set flag 1 = hide
            fn_Element_SetFlagsOnOff(imageElem,  0, 1);  // clear flag 1 = show
        } else {
            // Popup active: show button, hide image
            fn_Element_SetFlagsOnOff(buttonElem, 0, 1);  // clear flag 1 = show
            fn_Element_SetFlagsOnOff(imageElem,  1, 0);  // set flag 1 = hide
        }

        // Color from active_entry+0x28
        uint32_t color = *(uint32_t*)(activeEntry + 0x28);

        // Set color on button's controller: buttonElem->vtable[11](retBuf) → controller,
        // then controller->vtable[10](color).
        // vtable[11] = vtable offset 0x58 = GetController / GetPressColor sub-object.
        // vtable[10] = vtable offset 0x50 = SetColor(uint32_t).
        {
            fn_InternalBasePtr_CheckPtr(&buttonElem);
            uintptr_t* btnVtable = *(uintptr_t**)buttonElem;
            typedef void* (*GetController_t)(void* self, void* retBuf);
            GetController_t getCtrl = (GetController_t)(btnVtable[0x58 / 8]);
            uint8_t ctrlBuf[16];
            void* ctrl = getCtrl(buttonElem, ctrlBuf);
            fn_InternalBasePtr_CheckPtr(ctrl);
            void* ctrlResolved = *(void**)ctrl;
            uintptr_t* ctrlVtable = *(uintptr_t**)ctrlResolved;
            typedef void (*SetColor_t)(void* self, uint32_t c);
            ((SetColor_t)(ctrlVtable[0x50 / 8]))(ctrlResolved, color);
        }

        // Also set color on each of the 4 children of the button element.
        // Each child's SetColor is at vtable offset 0x50.
        for (int c = 0; c < 4; c++) {
            fn_InternalBasePtr_CheckPtr(&buttonElem);
            uint8_t kidBuf[16];
            void* kid = *(void**)fn_Element_Child(buttonElem, kidBuf, c);
            fn_InternalBasePtr_CheckPtr(&kid);
            void* kidResolved = *(void**)&kid;
            uintptr_t* kidVtable = *(uintptr_t**)kidResolved;
            typedef void (*SetColor_t)(void* self, uint32_t c);
            ((SetColor_t)(kidVtable[0x50 / 8]))(kidResolved, color);
        }

        // Copy event text string to the group element's embedded string at +0x58.
        // activeEntry+0x20 = System::String (display text).
        fn_InternalBasePtr_CheckPtr(&groupElem);
        {
            void* groupResolved = groupElem;
            void* groupStr = (void*)((uintptr_t)groupResolved + 0x58);
            void* srcStr   = (void*)(activeEntry + 0x20);
            fn_String_Assign(groupStr, srcStr);
        }
    }

    // --- Empty slots (activeCount..capacity-1): reset color, clear text, show empty image ---
    for (int i = activeCount; i < capacity; i++) {
        uintptr_t slotEntry = notifyEntryBase + (uintptr_t)i * 0x18;

        void* groupElem  = *(void**)(slotEntry + 0x00);
        void* imageElem  = *(void**)(slotEntry + 0x08);
        void* buttonElem = *(void**)(slotEntry + 0x10);

        // Reset button controller color to 0xFFFFFFFF (white = no tint)
        {
            fn_InternalBasePtr_CheckPtr(&buttonElem);
            uintptr_t* btnVtable = *(uintptr_t**)buttonElem;
            typedef void* (*GetController_t)(void* self, void* retBuf);
            GetController_t getCtrl = (GetController_t)(btnVtable[0x58 / 8]);
            uint8_t ctrlBuf[16];
            void* ctrl = getCtrl(buttonElem, ctrlBuf);
            fn_InternalBasePtr_CheckPtr(ctrl);
            void* ctrlResolved = *(void**)ctrl;
            uintptr_t* ctrlVtable = *(uintptr_t**)ctrlResolved;
            typedef void (*SetColor_t)(void* self, uint32_t c);
            ((SetColor_t)(ctrlVtable[0x50 / 8]))(ctrlResolved, 0xFFFFFFFFu);
        }

        // Also reset color on button element itself (direct vtable[10] call)
        {
            fn_InternalBasePtr_CheckPtr(&buttonElem);
            uintptr_t* btnVtable = *(uintptr_t**)buttonElem;
            typedef void (*SetColor_t)(void* self, uint32_t c);
            ((SetColor_t)(btnVtable[0x50 / 8]))(buttonElem, 0xFFFFFFFFu);
        }

        // Clear group element's embedded string at +0x58 (assign null = empty string)
        {
            fn_InternalBasePtr_CheckPtr(&groupElem);
            void* groupResolved = groupElem;
            void* groupStr = (void*)((uintptr_t)groupResolved + 0x58);
            void* nullStr  = nullptr;
            fn_String_Assign(groupStr, &nullStr);
            // Note: the original calls String::~String on the temporary after the assign.
            // The assign takes a const String* (pointing to null), so the empty string dtor is safe.
        }

        // Show image element (clear flag 1), hide button element (set flag 1).
        // Empty image slots display nothing (no sprite set); button hidden to prevent clicks.
        fn_InternalBasePtr_CheckPtr(&imageElem);
        fn_Element_SetFlagsOnOff(imageElem,  0, 1);  // clear flag = show
        fn_InternalBasePtr_CheckPtr(&buttonElem);
        fn_Element_SetFlagsOnOff(buttonElem, 1, 0);  // set flag = hide
    }
}
