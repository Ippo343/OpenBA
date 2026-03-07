// code/Game/Dialog/goaldialog.cpp
// Clean reimplementation of GoalDialog game logic.
//
// GoalDialog (0xC8 bytes) — achievement goal progress display.
// Extends DialogInstance (0xB8 base).
// +0xB8 = goal entry array (stride 0x18), +0xC0 = goal count.
//
// Decompiled from Application-x64.exe.

#include "Dialog/ui_dialogs.h"
#include "engine_funcs.h"
#include <cstdint>
#include <cstring>

// ============================================================
// File-local helper: GoalDialog_ArrayPush_Impl
// ============================================================
// Resizes dynamic array at +0xB8 to newCount entries (stride 0x18 bytes).
// Entry layout: 3 × void* (imageComplete, imageFail, textGoal) = 0x18 bytes.
// The actual GoalDialog::Slot4 inlines this logic; it does NOT use FUN_140011750.
// (FUN_140011750 has 0x30 stride with String at +0x20 — different from this.)
//
// arrayBase layout: [data* 8][count int32][capacity int32]
static void GoalDialog_ArrayPush_Impl(void* arrayBase, int newCount, void* entry3) {
    uintptr_t* dataPtr  = (uintptr_t*)arrayBase;
    int* countPtr       = (int*)((uintptr_t)arrayBase + 8);
    int* capacityPtr    = (int*)((uintptr_t)arrayBase + 0xc);
    int oldCount        = *countPtr;

    if (newCount == oldCount) return;

    void* oldData = (void*)*dataPtr;
    *countPtr = newCount;

    if (*capacityPtr < newCount) {
        *capacityPtr = newCount;
        if (oldCount != 0 && newCount == oldCount + 1) {
            *capacityPtr = newCount * 3;
        }
        void* newData = fn_MemAlloc(g_pMemAllocSingleton, (int64_t)*capacityPtr * 0x18, 8);
        *dataPtr = (uintptr_t)newData;
    }

    // Copy entries: [startCopy..newCount-1] from old buffer or from entry3 template
    int startCopy = (oldData == (void*)*dataPtr) ? oldCount : 0;
    for (int i = startCopy; i < newCount; i++) {
        uint64_t* dst = (uint64_t*)(*dataPtr + (uintptr_t)i * 0x18);
        if (dst != nullptr) {
            uint64_t* src = (i < oldCount)
                ? (uint64_t*)((uintptr_t)oldData + (uintptr_t)i * 0x18)
                : (uint64_t*)entry3;
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
        }
    }

    if (oldData != (void*)*dataPtr) {
        fn_MemFree(g_pMemAllocSingleton, oldData);
    }
}

// ============================================================
// GoalDialog::Slot4 (vtable +0x20) — Setup: populate goal table
// ============================================================
// Finds "buttonOk" (ID 100), "tableGoal", and "textScenario".
// Gets GoalList from game world (+0x150). For each goal:
//   - Creates a row element from dialog template[0]
//   - Inserts into tableGoal via SetChild
//   - Finds "textGoal", "imageComplete", "imageFail" sub-elements
//   - Sets goal text on textGoal via UI::String
//   - Hides both complete/fail images initially
//   - Pushes {imageComplete, imageFail, textGoal} into array at +0xB8
// If scenario has an Achievement (+0x20), sets title on textScenario.
// Otherwise hides textScenario.
//
// Entry layout at +0xB8 (stride 0x18 per entry):
//   +0x00: Element* imageComplete (shown when goal is met)
//   +0x08: Element* imageFail (shown when goal has failed)
//   +0x10: TextElement* textGoal (goal description text)
//
// Original: 0x140069810.

void GoalDialog_Slot4(GoalDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Find "buttonOk" and set element ID 100
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "buttonOk");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        void* buttonOk = *(void**)retBuf;
        fn_String_Dtor(nameStr);
        if (buttonOk) fn_Element_SetElementId(buttonOk, 100);
    }

    // Get GoalList from game world singleton at +0x150
    uintptr_t goalListObj = *(uintptr_t*)((uintptr_t)(*g_pGameWorldSingleton) + 0x150);

    if (goalListObj != 0) {
        // GoalList is at goalListObj+0x10
        void* goalList = (void*)(goalListObj + 0x10);

        // Find "tableGoal"
        void* tableGoal = nullptr;
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "tableGoal");
            uint8_t retBuf[16];
            fn_DialogInstance_FindElement(self, retBuf, nameStr);
            tableGoal = *(void**)retBuf;
            fn_String_Dtor(nameStr);
        }

        int goalCount = fn_GoalList_GetGoalCount(goalList);
        void* dialog = *(void**)(s + 0x18); // Dialog* at self+0x18

        for (int i = 0; i < goalCount; i++) {
            // Create row element from dialog template[0]
            uint8_t templateBuf[16] = {0};
            fn_Dialog_GetTemplate(dialog, templateBuf, 0);
            void* elemDesc = *(void**)templateBuf;

            // Call ElementDescription::Create(this, retval, dialog) via vtable+0x40
            uint8_t newElemBuf[16] = {0};
            if (elemDesc) {
                void** vtbl = *(void***)elemDesc;
                typedef void (*CreateFn)(void*, void*, void*);
                ((CreateFn)vtbl[8])(elemDesc, newElemBuf, dialog);
            }

            // Insert the new element as a child of the table
            if (tableGoal) {
                fn_TableElement_SetChild2(tableGoal, newElemBuf, i);
            }

            // Find sub-elements within the new row
            void* newElem = *(void**)newElemBuf;
            void* imageComplete = nullptr;
            void* imageFail = nullptr;
            void* textGoal = nullptr;

            // Find "textGoal"
            {
                uint8_t nameStr[8];
                fn_String_Ctor_CStr(nameStr, "textGoal");
                uint8_t retBuf[16];
                fn_FindElement(newElem, retBuf, nameStr);
                textGoal = *(void**)retBuf;
                fn_String_Dtor(nameStr);
            }

            // Find "imageComplete"
            {
                uint8_t nameStr[8];
                fn_String_Ctor_CStr(nameStr, "imageComplete");
                uint8_t retBuf[16];
                fn_FindElement(newElem, retBuf, nameStr);
                imageComplete = *(void**)retBuf;
                fn_String_Dtor(nameStr);
            }

            // Find "imageFail"
            {
                uint8_t nameStr[8];
                fn_String_Ctor_CStr(nameStr, "imageFail");
                uint8_t retBuf[16];
                fn_FindElement(newElem, retBuf, nameStr);
                imageFail = *(void**)retBuf;
                fn_String_Dtor(nameStr);
            }

            // Get goal text and set on textGoal element
            {
                // Get GoalInstance at index i
                uint8_t goalInstBuf[16] = {0};
                fn_GoalList_GetGoal(goalList, goalInstBuf, i);
                void* goalInstPtr = *(void**)goalInstBuf;

                // GoalInstance -> Goal at +0x18
                void* goal = *(void**)((uintptr_t)goalInstPtr + 0x18);

                // Get goal text (non-trivial return: fills retval with System::String)
                uint8_t textStr[8] = {0};
                fn_Goal_GetText(goal, textStr);

                // Read StringTable from Goal+0x20 (ExternalPtr<StringTable>)
                uint64_t stringTable = *(uint64_t*)((uintptr_t)goal + 0x20);

                // Construct UI::String from StringTable + text
                uint8_t uiStr[16] = {0};
                fn_UIString_Ctor(uiStr, stringTable, textStr);

                // Set text on textGoal element
                if (textGoal) {
                    fn_TextElement_SetText(textGoal, uiStr);
                }

                // Clean up the System::String from GetText
                fn_String_Dtor(textStr);
            }

            // Hide both complete and fail images initially
            if (imageComplete) fn_Element_SetFlagsOnOff(imageComplete, 1, 0);
            if (imageFail) fn_Element_SetFlagsOnOff(imageFail, 1, 0);

            // Push entry {imageComplete, imageFail, textGoal} to array at +0xB8
            uint64_t entry[3];
            entry[0] = (uint64_t)imageComplete;
            entry[1] = (uint64_t)imageFail;
            entry[2] = (uint64_t)textGoal;

            int newCount = *(int*)(s + 0xC0) + 1;
            GoalDialog_ArrayPush_Impl((void*)(s + 0xB8), newCount, entry);
        }
    }

    // Find "textScenario" element
    void* textScenario = nullptr;
    {
        uint8_t nameStr[8];
        fn_String_Ctor_CStr(nameStr, "textScenario");
        uint8_t retBuf[16];
        fn_DialogInstance_FindElement(self, retBuf, nameStr);
        textScenario = *(void**)retBuf;
        fn_String_Dtor(nameStr);
    }

    // If GoalList has an Achievement at +0x20, set its title on textScenario
    if (goalListObj != 0 && *(void**)(goalListObj + 0x20) != nullptr) {
        void* achievement = *(void**)(goalListObj + 0x20);

        // Get title string (non-trivial return)
        uint8_t titleStr[8] = {0};
        fn_Achievement_GetTitle(achievement, titleStr);

        // Read StringTable from Achievement+0x20
        uint64_t stringTable = *(uint64_t*)((uintptr_t)achievement + 0x20);

        // Construct UI::String from StringTable + title
        uint8_t uiStr[16] = {0};
        fn_UIString_Ctor(uiStr, stringTable, titleStr);

        // Convert UI::String to System::String for SetParameter
        uint8_t sysStr[8] = {0};
        fn_UIString_ToSystemString(uiStr, sysStr);

        // Set as parameter 0 on textScenario
        if (textScenario) {
            fn_TextElement_SetParameter(textScenario, 0, sysStr);
        }

        fn_String_Dtor(sysStr);
        fn_String_Dtor(titleStr);
    } else {
        // No achievement/scenario — hide textScenario
        if (textScenario) fn_Element_SetFlagsOnOff(textScenario, 1, 0);
    }
}

// ============================================================
// GoalDialog::Slot6 (vtable +0x30) — Update goal entry visibility
// ============================================================
// Gets the GoalList from game world (+0x150), iterates each goal entry.
// For each goal: checks IsMet/HasFailed, sets visibility flags on
// corresponding UI elements (entry+0 for met, entry+8 for failed).
//
// Entry layout at +0xB8 (stride 0x18 per entry):
//   +0x00: Element* metIndicator (shown when goal is met)
//   +0x08: Element* failedIndicator (shown when goal has failed)
//   +0x10: (unused in Slot6)
//
// Original: 0x140069640.

void GoalDialog_Slot6(GoalDialog* self) {
    uintptr_t s = (uintptr_t)self;

    // Get GoalList from game world singleton at +0x150
    void* world = *g_pGameWorldSingleton;
    uintptr_t goalListObj = *(uintptr_t*)((uintptr_t)world + 0x150);

    int count = *(int*)(s + 0xC0);
    uintptr_t entryArray = *(uintptr_t*)(s + 0xB8);

    for (int i = 0; i < count; i++) {
        uintptr_t entryOffset = (uintptr_t)i * 0x18;

        // Get goal at index i from the GoalList
        uint64_t goalInst = 0;
        fn_GoalList_GetGoal((void*)(goalListObj + 0x10), &goalInst, i);

        // Check if goal is met and if it has failed
        bool isMet = fn_GoalInstance_IsMet((void*)goalInst);
        bool hasFailed = fn_GoalInstance_HasFailed((void*)goalInst);

        // Met indicator: show if met, hide if not
        void* metElem = *(void**)(entryArray + entryOffset);
        fn_Element_SetFlagsOnOff(metElem, isMet ? 0 : 1, isMet ? 1 : 0);

        // Failed indicator: show if failed, hide if not
        void* failedElem = *(void**)(entryArray + 8 + entryOffset);
        fn_Element_SetFlagsOnOff(failedElem, hasFailed ? 0 : 1, hasFailed ? 1 : 0);
    }
}
