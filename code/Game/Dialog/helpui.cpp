// engine/src/ui/helpui.cpp
// Clean reimplementation of HelpUI game logic.
//
// HelpUI (0x78 bytes) — help page navigation panel.
// Extends DialogController (0x30 base). Fields:
//   +0x30: first element (InternalBasePtr) — scroll position source at elem+0x160
//   +0x38: DocumentTextElement (InternalBasePtr), ID=0xE75
//   +0x40: ComboElement (InternalBasePtr), ID=0xE74
//   +0x48: buttonBack (InternalBasePtr), ID=0xE76
//   +0x50: buttonForward (InternalBasePtr), ID=0xE77
//   +0x58: container element (InternalBasePtr) — scroll offsets
//   +0x60: current page resource index (int32_t)
//   +0x64: current history index (int32_t)
//   +0x68: history array pointer (8-byte entries: int32_t pageIndex + float scrollY)
//   +0x70: history count (int32_t)
//   +0x74: history capacity (int32_t)
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// File-local helpers
// ============================================================

// Get the help topics ExternalList from the game world singleton
static void* GetHelpExternalList() {
    void* world = *g_pGameWorldSingleton;
    return *(void**)((uintptr_t)world + 0xa0);
}

// ============================================================
// HelpUI_LoadPage (FUN_140069f20) — Load help page content
// ============================================================
// Gets the currently selected combo item, looks up the corresponding
// StringTable resource, sets its content (text index 2) on the document
// element, resets scroll to top, and pushes a history entry.
//
// Original: 0x140069f20.

static void HelpUI_LoadPage(HelpUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* extList = GetHelpExternalList();

    // Get currently selected combo item
    void* comboElem = *(void**)(s + 0x40);
    int currentItem = *(int*)((uintptr_t)comboElem + 0x118);
    void* itemData = fn_ComboElement_GetItemData(comboElem, currentItem);
    int pageResIndex = (int)(intptr_t)itemData;

    // Look up StringTable resource from ExternalList
    void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
    void* stringTable = *(void**)((uintptr_t)resourceArray + (int64_t)pageResIndex * 8);

    // Check IsDerivedFrom (StringTable type)
    bool isStringTable = fn_IsDerivedFrom(stringTable, *g_TypeId_StringTable);
    void* validTable = isStringTable ? stringTable : nullptr;

    // Construct UI::String with text index 2 (help content body)
    uint8_t uiString[16] = {0};
    fn_UIString_Ctor_Index(uiString, (uint64_t)validTable, 2);

    // Set document text
    void* docElem = *(void**)(s + 0x38);
    fn_DocumentTextElement_SetText_UIString(docElem, uiString);

    // Reset scroll offsets to (0, 0)
    void* containerElem = *(void**)(s + 0x58);
    fn_ContainerElement_SetOffsets(containerElem, 0, 0);

    // Store current page resource index
    *(int*)(s + 0x60) = pageResIndex;

    // Resize history array to (historyIndex + 1) entries
    int historyIndex = *(int*)(s + 0x64);
    fn_HelpUI_ArrayResize((void*)(s + 0x68), historyIndex + 1);

    // Push new history entry: {pageResIndex, scrollY=0.0f} at the new end
    int count = *(int*)(s + 0x70);
    int newCount = count + 1;
    int capacity = *(int*)(s + 0x74);
    void* histArray = *(void**)(s + 0x68);

    if (newCount != count) {
        *(int*)(s + 0x70) = newCount;
        if (capacity < newCount) {
            int newCapacity = newCount;
            if (count != 0 && newCount == count + 1) {
                newCapacity = newCount * 3;
            }
            *(int*)(s + 0x74) = newCapacity;
            void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)newCapacity * 8, 4);
            *(void**)(s + 0x68) = newBuf;

            // Copy existing entries if buffer changed
            if (histArray != newBuf) {
                for (int i = 0; i < count; i++) {
                    *(uint64_t*)((uintptr_t)newBuf + i * 8) =
                        *(uint64_t*)((uintptr_t)histArray + i * 8);
                }
            }
        }

        // Initialize new entry with {pageResIndex, 0.0f}
        void* curBuf = *(void**)(s + 0x68);
        int32_t initEntry[2] = { pageResIndex, 0 };  // scrollY = 0.0f
        *(uint64_t*)((uintptr_t)curBuf + (int64_t)count * 8) = *(uint64_t*)initEntry;

        // Free old buffer if it changed
        if (histArray != *(void**)(s + 0x68)) {
            fn_MemFree(g_pMemAllocSingleton, histArray);
        }
    }

    // Set history index to last entry
    *(int*)(s + 0x64) = *(int*)(s + 0x70) - 1;
}

// ============================================================
// HelpUI_FindLinkTarget (FUN_14006a140) — Find page by link text
// ============================================================
// Iterates the help topics ExternalList, gets text at index 1
// (topic name) from each StringTable, compares with the search
// string. Returns the matching resource index, or -1 if not found.
//
// Note: Destroys the input search string (param_2) on return.
//
// Original: 0x14006a140.

static int HelpUI_FindLinkTarget(uint8_t* searchStr) {
    void* extList = GetHelpExternalList();
    int count = fn_ExternalList_GetResourceCount(extList);

    for (int i = 0; i < count; i++) {
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* entry = *(void**)((uintptr_t)resourceArray + (int64_t)i * 8);

        bool isStringTable = fn_IsDerivedFrom(entry, *g_TypeId_StringTable);
        if (isStringTable && entry != nullptr) {
            // GetText at index 0 = topic key/name for matching (non-trivial return)
            uint8_t nameStr[8] = {0};
            fn_StringTable_GetText(entry, nameStr, 0);

            bool match = fn_String_Equal(searchStr, nameStr);
            fn_String_Dtor(nameStr);

            if (match) {
                fn_String_Dtor(searchStr);
                return i;
            }
        }

        // Re-check count each iteration (matches original)
        count = fn_ExternalList_GetResourceCount(extList);
    }

    fn_String_Dtor(searchStr);
    return -1;
}

// ============================================================
// HelpUI::Slot4 (vtable +0x20) — Setup / initialization
// ============================================================
// Finds typed UI elements from the dialog:
//   - First generic element stored at +0x30
//   - DocumentTextElement at +0x38, ID=0xE75
//   - ComboElement at +0x40, ID=0xE74
//   - ContainerElement at +0x58
// Finds named elements:
//   - "buttonBack" at +0x48, ID=0xE76
//   - "buttonForward" at +0x50, ID=0xE77
// Populates combo with help topic names from ExternalList.
// Calls LoadPage to display the first topic.
//
// Original: 0x14006A500.

void HelpUI_Slot4(HelpUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogElem = *(void**)(s + 0x20);

    // FindElementType calls use different type IDs from _typeInfo_exref.
    // Each finds a different typed element in the dialog tree.
    // Order matches original: +0x30 first, then +0x40, +0x38, +0x58.

    uint8_t retBuf[16] = {0};
    uint8_t bpBuf[8] = {0};
    void* elem;
    bool derived;

    // 1. Find first element (type 0x1401517e8) -> +0x30
    fn_Element_FindElementType(dialogElem, retBuf, *g_TypeId_HelpFirstElem);
    elem = *(void**)retBuf;
    derived = fn_IsDerivedFrom(elem, *g_TypeId_HelpFirstElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? elem : nullptr);
    *(uint64_t*)(s + 0x30) = *(uint64_t*)bpBuf;

    // 2. Find ComboElement (type 0x140151690) -> +0x40
    fn_Element_FindElementType(dialogElem, retBuf, *g_TypeId_HelpComboElem);
    elem = *(void**)retBuf;
    derived = fn_IsDerivedFrom(elem, *g_TypeId_HelpComboElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? elem : nullptr);
    *(uint64_t*)(s + 0x40) = *(uint64_t*)bpBuf;

    // 3. Find DocumentTextElement (type 0x1401517f0) -> +0x38
    fn_Element_FindElementType(dialogElem, retBuf, *g_TypeId_HelpDocTextElem);
    elem = *(void**)retBuf;
    derived = fn_IsDerivedFrom(elem, *g_TypeId_HelpDocTextElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? elem : nullptr);
    *(uint64_t*)(s + 0x38) = *(uint64_t*)bpBuf;

    // 4. Find ContainerElement (type 0x1401517f8) -> +0x58
    fn_Element_FindElementType(dialogElem, retBuf, *g_TypeId_HelpContainerElem);
    elem = *(void**)retBuf;
    derived = fn_IsDerivedFrom(elem, *g_TypeId_HelpContainerElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? elem : nullptr);
    *(uint64_t*)(s + 0x58) = *(uint64_t*)bpBuf;

    // Set element IDs on combo and document elements
    fn_Element_SetElementId(*(void**)(s + 0x40), 0xE74);
    fn_Element_SetElementId(*(void**)(s + 0x38), 0xE75);

    // Find "buttonBack" by name -> +0x48, set ID 0xE76
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, "buttonBack");
    fn_FindElement(dialogElem, retBuf, nameStr);
    void* backBtn = *(void**)retBuf;
    derived = fn_IsDerivedFrom(backBtn, *g_TypeId_HelpButtonElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? backBtn : nullptr);
    *(uint64_t*)(s + 0x48) = *(uint64_t*)bpBuf;
    fn_String_Dtor(nameStr);

    // Find "buttonForward" by name -> +0x50, set ID 0xE77
    fn_String_Ctor_CStr(nameStr, "buttonForward");
    fn_FindElement(dialogElem, retBuf, nameStr);
    void* fwdBtn = *(void**)retBuf;
    derived = fn_IsDerivedFrom(fwdBtn, *g_TypeId_HelpButtonElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? fwdBtn : nullptr);
    *(uint64_t*)(s + 0x50) = *(uint64_t*)bpBuf;
    fn_String_Dtor(nameStr);

    fn_Element_SetElementId(*(void**)(s + 0x48), 0xE76);
    fn_Element_SetElementId(*(void**)(s + 0x50), 0xE77);

    // Populate combo with help topic names from ExternalList
    void* extList = GetHelpExternalList();
    int topicCount = fn_ExternalList_GetResourceCount(extList);
    void* comboElem = *(void**)(s + 0x40);

    for (int i = 0; i < topicCount; i++) {
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* entry = *(void**)((uintptr_t)resourceArray + (int64_t)i * 8);

        bool isStringTable = fn_IsDerivedFrom(entry, *g_TypeId_StringTable);
        if (isStringTable && entry != nullptr) {
            // Get topic name (text index 1) — non-trivial return
            uint8_t topicName[8] = {0};
            fn_StringTable_GetText(entry, topicName, 1);

            // Create default sprite
            uint8_t sprite[32];
            fn_Sprite_Default_Ctor(sprite);

            // Add item to combo: (sprite, name, userData=index)
            fn_ComboElement_AddItem(comboElem, sprite, topicName, (void*)(intptr_t)i);
            fn_String_Dtor(topicName);
        }

        // Re-check count each iteration (matches original)
        topicCount = fn_ExternalList_GetResourceCount(extList);
    }

    // Select first item and load its content
    fn_ComboElement_SetCurrentItem(comboElem, 0);
    HelpUI_LoadPage(self);
}

// ============================================================
// HelpUI::Slot7 (vtable +0x38) — Update handler
// ============================================================
// Enables/disables prev/next buttons based on current page index.
//
// Original: 0x14006A230.

void HelpUI_Slot7(HelpUI* self) {
    int pageIndex = *(int*)((uintptr_t)self + 0x64);
    int pageCount = *(int*)((uintptr_t)self + 0x70);

    // Enable prev button if not on first page
    void* prevBtn = (void*)self->data48;
    void** vtable1 = *(void***)prevBtn;
    typedef void (*fn_SetEnabled_t)(void*, bool);
    ((fn_SetEnabled_t)vtable1[8])(prevBtn, pageIndex > 0);

    // Enable next button if not on last page
    void* nextBtn = (void*)self->data50;
    void** vtable2 = *(void***)nextBtn;
    ((fn_SetEnabled_t)vtable2[8])(nextBtn, pageIndex < pageCount - 1);
}

// ============================================================
// HelpUI::Slot9 (vtable +0x48) — Notification handler
// ============================================================
// Handles UI events:
//   Message 0x12 (button/link click):
//     - 0xE75 (document link): get link text, find target page,
//       save scroll position to history, navigate to target
//     - 0xE76 (back): go to previous history entry, restore scroll
//     - 0xE77 (forward): go to next history entry, restore scroll
//   Message 0x14 (combo selection change):
//     - 0xE74: topic changed, reload page content
//
// Original: 0x14006A290.

void HelpUI_Slot9(HelpUI* self, uint64_t param2, int msg, int id,
                   int param5, int param6, int linkIndex) {
    (void)param2; (void)param5; (void)param6;
    uintptr_t s = (uintptr_t)self;

    if (msg == 0x14 && id == 0xE74) {
        // Combo selection changed — reload page
        HelpUI_LoadPage(self);
        return;
    }

    if (msg != 0x12)
        return;

    if (id == 0xE75) {
        // Document link clicked — navigate to linked page
        void* docElem = *(void**)(s + 0x38);

        // Get the clicked link's text (non-trivial return)
        uint8_t linkStr[8] = {0};
        fn_DocumentTextElement_GetLink(docElem, linkStr, linkIndex);

        // Copy the link string for FindLinkTarget (which destroys it)
        uint8_t searchStr[8] = {0};
        fn_String_Ctor_Copy(searchStr, linkStr);

        // FindLinkTarget destroys searchStr and returns index or -1
        int targetIdx = HelpUI_FindLinkTarget(searchStr);

        if (targetIdx >= 0) {
            // Save current scroll position to history
            void* firstElem = *(void**)(s + 0x30);
            int histIdx = *(int*)(s + 0x64);
            void* histArray = *(void**)(s + 0x68);
            float scrollPos = *(float*)((uintptr_t)firstElem + 0x160);
            *(float*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4) = scrollPos;

            // Navigate to target page
            void* comboElem = *(void**)(s + 0x40);
            fn_ComboElement_SetCurrentItem(comboElem, targetIdx);
            HelpUI_LoadPage(self);
        }

        fn_String_Dtor(linkStr);
        return;
    }

    if (id == 0xE76) {
        // Back button — go to previous history entry
        void* firstElem = *(void**)(s + 0x30);
        int histIdx = *(int*)(s + 0x64);
        void* histArray = *(void**)(s + 0x68);

        // Save current scroll position before navigating
        int scrollVal = *(int*)((uintptr_t)firstElem + 0x160);
        *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4) = scrollVal;

        // Move back one entry
        histIdx--;
        *(int*)(s + 0x64) = histIdx;

        // Get page index from history
        int pageIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);

        // Look up StringTable from ExternalList
        void* extList = GetHelpExternalList();
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* stringTable = *(void**)((uintptr_t)resourceArray + (int64_t)pageIdx * 8);

        bool isStringTable = fn_IsDerivedFrom(stringTable, *g_TypeId_StringTable);
        void* validTable = isStringTable ? stringTable : nullptr;

        // Construct UI::String with text index 2 (content) and set on document
        uint8_t uiString[16] = {0};
        fn_UIString_Ctor_Index(uiString, (uint64_t)validTable, 2);

        void* docElem = *(void**)(s + 0x38);
        fn_DocumentTextElement_SetText_UIString(docElem, uiString);

        // Restore scroll position from history
        histArray = *(void**)(s + 0x68);
        histIdx = *(int*)(s + 0x64);
        float savedScroll = *(float*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4);
        void* containerElem = *(void**)(s + 0x58);
        fn_ContainerElement_SetOffsets(containerElem, 0, (int)savedScroll);

        // Update combo to show current page
        int comboIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);
        void* comboElem = *(void**)(s + 0x40);
        fn_ComboElement_SetCurrentItem(comboElem, comboIdx);
        return;
    }

    if (id == 0xE77) {
        // Forward button — go to next history entry
        void* firstElem = *(void**)(s + 0x30);
        int histIdx = *(int*)(s + 0x64);
        void* histArray = *(void**)(s + 0x68);

        // Save current scroll position before navigating
        int scrollVal = *(int*)((uintptr_t)firstElem + 0x160);
        *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4) = scrollVal;

        // Move forward one entry
        histIdx++;
        *(int*)(s + 0x64) = histIdx;

        // Get page index from history
        int pageIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);

        // Look up StringTable from ExternalList
        void* extList = GetHelpExternalList();
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* stringTable = *(void**)((uintptr_t)resourceArray + (int64_t)pageIdx * 8);

        bool isStringTable = fn_IsDerivedFrom(stringTable, *g_TypeId_StringTable);
        void* validTable = isStringTable ? stringTable : nullptr;

        // Construct UI::String with text index 2 (content) and set on document
        uint8_t uiString[16] = {0};
        fn_UIString_Ctor_Index(uiString, (uint64_t)validTable, 2);

        void* docElem = *(void**)(s + 0x38);
        fn_DocumentTextElement_SetText_UIString(docElem, uiString);

        // Restore scroll position from history
        histArray = *(void**)(s + 0x68);
        histIdx = *(int*)(s + 0x64);
        float savedScroll = *(float*)((uintptr_t)histArray + (int64_t)histIdx * 8 + 4);
        void* containerElem = *(void**)(s + 0x58);
        fn_ContainerElement_SetOffsets(containerElem, 0, (int)savedScroll);

        // Update combo to show current page
        int comboIdx = *(int*)((uintptr_t)histArray + (int64_t)histIdx * 8);
        void* comboElem = *(void**)(s + 0x40);
        fn_ComboElement_SetCurrentItem(comboElem, comboIdx);
        return;
    }
}
