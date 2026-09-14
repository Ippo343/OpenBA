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
#include "Dialog/dialog_array_bridge.h"
#include "engine_funcs.h"
#include "system/String.h"
#include <cstdint>

// ============================================================
// File-local helpers
// ============================================================

// Get the help topics ExternalList from the game world singleton
// (retail: `*(ExternalList**)(g_pGameWorldSingleton + 0xa0)`, verified against the
// disassembly of 0x140069f38 — the singleton IS the GameUpdateLoop at .rdata
// 0x1401c3dd8). Standalone deviation: the singleton is null before the game loop
// exists (and the list itself can be null if the help resources never loaded), so
// return null rather than fault; every caller below treats null as "no topics".
static void* GetHelpExternalList() {
    if (!g_pGameWorldSingleton) return nullptr;
    void* world = *g_pGameWorldSingleton;
    if (!world) return nullptr;
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

    // Standalone guard (retail has none — its .rsc always yields both elements and
    // the world always owns the help list). If an element lookup failed (a null
    // element type-id makes FindElementType store null even when the element
    // exists) this would deref null at combo+0x118 / inside SetText.
    if (!extList || !comboElem || !*(void**)(s + 0x38) || !*(void**)(s + 0x58))
        return;
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

    // History array at +0x68: Collection::Array<{int32 pageIndex, float scrollY}>
    // (stride 8, count +0x70, capacity +0x74) inside a struct §B.2 has not
    // converted, so it is reached at its offset. B.3.2 bridge for both the
    // truncate (was HelpUI_ArrayResize_Impl, FUN_140011d50) and the append below.
    //
    // The element is DlgI32Pair and NOT DlgElem<8>, for the two reasons its
    // declaration in dialog_array_bridge.h spells out: retail's helper passed 4
    // — not 8 — as MemAlloc's alignment argument for this array, and it left a
    // GROWN tail uninitialised, which only a trivially default-constructible
    // element reproduces.
    auto& hist = DlgArrayI32Pair(s, 0x68);

    // Truncate history to (historyIndex + 1) entries — removes forward history.
    int historyIndex = *(int*)(s + 0x64);
    hist.SetSize(historyIndex + 1);

    // Push new history entry: {pageResIndex, scrollY=0.0f} at the new end.
    hist.SetSize(hist.GetSize() + 1, DlgI32Pair{pageResIndex, 0});  // scrollY = 0.0f

    // Set history index to last entry
    *(int*)(s + 0x64) = hist.GetSize() - 1;
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

// Takes ownership of searchStr (by value) and destroys it on return — the RAII
// destructor at scope exit reproduces retail's manual release on every path.
static int HelpUI_FindLinkTarget(System::String searchStr) {
    void* extList = GetHelpExternalList();
    if (!extList) { return -1; }   // standalone guard
    int count = fn_ExternalList_GetResourceCount(extList);

    for (int i = 0; i < count; i++) {
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* entry = *(void**)((uintptr_t)resourceArray + (int64_t)i * 8);

        bool isStringTable = fn_IsDerivedFrom(entry, *g_TypeId_StringTable);
        if (isStringTable && entry != nullptr) {
            // GetText at index 0 = topic key/name for matching (non-trivial return)
            System::String nameStr;
            fn_StringTable_GetText(entry, &nameStr, 0);

            bool match = (searchStr == nameStr);

            if (match) {
                return i;
            }
        }

        // Re-check count each iteration (matches original)
        count = fn_ExternalList_GetResourceCount(extList);
    }

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
    // (standalone guard: SetElementId derefs its `this`, so skip a failed lookup)
    if (*(void**)(s + 0x40)) fn_Element_SetElementId(*(void**)(s + 0x40), 0xE74);
    if (*(void**)(s + 0x38)) fn_Element_SetElementId(*(void**)(s + 0x38), 0xE75);

    // Find "buttonBack" by name -> +0x48, set ID 0xE76
    void* backBtn;
    {
        System::String nameStr("buttonBack");
        fn_FindElement(dialogElem, retBuf, &nameStr);
        backBtn = *(void**)retBuf;
    }
    derived = fn_IsDerivedFrom(backBtn, *g_TypeId_HelpButtonElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? backBtn : nullptr);
    *(uint64_t*)(s + 0x48) = *(uint64_t*)bpBuf;

    // Find "buttonForward" by name -> +0x50, set ID 0xE77
    void* fwdBtn;
    {
        System::String nameStr("buttonForward");
        fn_FindElement(dialogElem, retBuf, &nameStr);
        fwdBtn = *(void**)retBuf;
    }
    derived = fn_IsDerivedFrom(fwdBtn, *g_TypeId_HelpButtonElem);
    fn_InternalBasePtr_Ctor(bpBuf, derived ? fwdBtn : nullptr);
    *(uint64_t*)(s + 0x50) = *(uint64_t*)bpBuf;

    if (*(void**)(s + 0x48)) fn_Element_SetElementId(*(void**)(s + 0x48), 0xE76);
    if (*(void**)(s + 0x50)) fn_Element_SetElementId(*(void**)(s + 0x50), 0xE77);

    // Populate combo with help topic names from ExternalList
    void* extList = GetHelpExternalList();
    void* comboElem = *(void**)(s + 0x40);
    if (!extList || !comboElem) return;    // standalone guard
    int topicCount = fn_ExternalList_GetResourceCount(extList);

    for (int i = 0; i < topicCount; i++) {
        void* resourceArray = *(void**)((uintptr_t)extList + 0x20);
        void* entry = *(void**)((uintptr_t)resourceArray + (int64_t)i * 8);

        bool isStringTable = fn_IsDerivedFrom(entry, *g_TypeId_StringTable);
        if (isStringTable && entry != nullptr) {
            // Get topic name (text index 1) — non-trivial return
            System::String topicName;
            fn_StringTable_GetText(entry, &topicName, 1);

            // Create default sprite
            uint8_t sprite[32];
            fn_Sprite_Default_Ctor(sprite);

            // Add item to combo: (sprite, name, userData=index)
            fn_ComboElement_AddItem(comboElem, sprite, &topicName, (void*)(intptr_t)i);
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

    typedef void (*fn_SetEnabled_t)(void*, bool);

    // Enable prev button if not on first page
    // (standalone guard: a failed "buttonBack"/"buttonForward" lookup leaves the
    // field null, and retail dereferences it unconditionally)
    void* prevBtn = (void*)self->data48;
    if (prevBtn) {
        void** vtable1 = *(void***)prevBtn;
        ((fn_SetEnabled_t)vtable1[8])(prevBtn, pageIndex > 0);
    }

    // Enable next button if not on last page
    void* nextBtn = (void*)self->data50;
    if (nextBtn) {
        void** vtable2 = *(void***)nextBtn;
        ((fn_SetEnabled_t)vtable2[8])(nextBtn, pageIndex < pageCount - 1);
    }
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

// Standalone guard helper: every navigation branch below (link / back / forward)
// dereferences the scroll source (+0x30), the document (+0x38), the combo (+0x40),
// the container (+0x58) and the history array (+0x68) without a null check, exactly
// as retail does. If setup could not find an element, treat the event as unhandled.
static bool HelpUI_NavReady(uintptr_t s) {
    return *(void**)(s + 0x30) && *(void**)(s + 0x38) && *(void**)(s + 0x40) &&
           *(void**)(s + 0x58) && *(void**)(s + 0x68);
}

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

    if (id != 0xE75 && id != 0xE76 && id != 0xE77)
        return;
    if (!HelpUI_NavReady(s))
        return;   // standalone guard, see above

    if (id == 0xE75) {
        // Document link clicked — navigate to linked page
        void* docElem = *(void**)(s + 0x38);

        // Get the clicked link's text (non-trivial return)
        System::String linkStr;
        fn_DocumentTextElement_GetLink(docElem, &linkStr, linkIndex);

        // FindLinkTarget takes its own copy (by value) and destroys it; returns
        // the matching index or -1.
        int targetIdx = HelpUI_FindLinkTarget(linkStr);

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

// ============================================================
// HelpUI::Slot5 (vtable +0x28) — No-op
// ============================================================
