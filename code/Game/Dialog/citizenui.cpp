// code/Game/Dialog/citizenui.cpp
// Clean reimplementation of CitizenUI panel methods.
//
// CitizenUI (0xE8 bytes): Citizen info panel.
// Layout:
//   +0x30 imageMale (InternalBasePtr)
//   +0x38 imageFemale
//   +0x40 textName
//   +0x48 numberAge
//   +0x50 textProfession
//   +0x58 textEducatedYes
//   +0x60 textEducatedNo
//   +0x68 textClothing
//   +0x70 textTool
//   +0x78 buttonProfession
//   +0x80 buttonHome
//   +0x88 buttonFollow
//   +0x90 textNameIs
//   +0x98 textAction
//   +0xA0 heartsIconArray (0x20 bytes: ptr, count, cap, ptr2, count2, cap2)
//   +0xC0 starsIconArray  (0x20 bytes)
//   +0xE0 sub-dialog InternalBasePtr (follow-dialog reference)

#include "Dialog/ui_panels.h"
#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// CitizenUI_UpdateIconDisplay (0x14006aed0)
// ============================================================
// Updates a status icon array (health hearts or happiness stars).
// param_1: icon array base (layout: ptr@+0, count@+8int, _, ptr2@+16, count2@+24int, _)
//   Two arrays: "whole" icons (fully filled) and "half" icons (half-filled).
//   Each array entry is an InternalBasePtr (8 bytes).
// param_2: currentVal  (current stat value, e.g. health 0..maxVal)
// param_3: maxVal      (e.g. description's max health)
//
// fillLevel = round((currentVal / maxVal) * (iconCount * 2))
// For icon index i (1-based, step 2):
//   whole array[i]: visible if i >= fillLevel (full or partial icon)
//   half  array[i]: visible if i == fillLevel (exactly half-filled icon)
static void CitizenUI_UpdateIconDisplay_Impl(uintptr_t iconArrayBase, float currentVal, float maxVal)
{
    // iconArray layout:
    //   +0x00 ptr to whole-icon InternalBasePtr array
    //   +0x08 count (int32 at +0x8)
    //   +0x10 (capacity, unused)
    //   +0x10 ptr to half-icon InternalBasePtr array (at +0x10? see layout)
    //
    // From decompile: param_1[0] = whole array base, param_1[1] = count
    //                 param_1[2] = half array base,  param_1[3] = count2
    // But the layout is packed: 0x20 bytes total per icon group:
    //   int64 ptr (8), int32 count (4), int32 cap (4), int64 ptr2 (8), int32 cnt2 (4), int32 cap2 (4)
    // So: ptr=iconArrayBase+0, count=iconArrayBase+8, ptr2=iconArrayBase+16, count2=iconArrayBase+24

    uintptr_t* wholeArr = *(uintptr_t**)(iconArrayBase);           // ptr to InternalBasePtr[]
    int         count    = *(int*)(iconArrayBase + 8);             // number of icon slots
    uintptr_t* halfArr  = *(uintptr_t**)(iconArrayBase + 0x10);   // ptr to InternalBasePtr[]

    if (count <= 0) return;

    // fillLevel = (int)((currentVal / maxVal) * (count * 2) + 0.5f)
    // Note: DAT_140172114 = 0.5f (rounding constant)
    int fillLevel = (int)((currentVal / maxVal) * (float)(count * 2) + 0.5f);

    int iVar2 = 1;      // current icon "double-index" (1-based, increments by 2)
    for (int i = 0; i < count; i++) {
        uintptr_t offset = (uintptr_t)i * 8;

        // Whole icon: show (flag off bit1) if iVar2 < fillLevel; hide if iVar2 >= fillLevel
        {
            uintptr_t ibpAddr = (uintptr_t)wholeArr + offset;
            fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            void* elem = *(void**)ibpAddr;
            fn_Element_SetFlagsOnOff(elem,
                (iVar2 >= fillLevel) ? 1 : 0,
                (iVar2 < fillLevel) ? 1 : 0);
        }

        // Half icon: show if iVar2 == fillLevel; hide otherwise
        {
            uintptr_t ibpAddr = (uintptr_t)halfArr + offset;
            fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            void* elem = *(void**)ibpAddr;
            fn_Element_SetFlagsOnOff(elem,
                (iVar2 != fillLevel) ? 1 : 0,
                (iVar2 == fillLevel) ? 1 : 0);
        }

        iVar2 += 2;
    }
}

// ============================================================
// CitizenUI_BuildIconArray (0x14006abf0)
// ============================================================
// Builds two InternalBasePtr arrays (whole-icon and half-icon) from
// a group element's children. Each child has sub-elements named
// "whole" and "half" found via FindElement.
//
// Layout of param_1 (iconArrayBase, 0x20 bytes):
//   +0x00: ptr to whole[] (InternalBasePtr)
//   +0x08: count (int32) at int offset 1 of longlong* (i.e. +0x08)
//   +0x0C: capacity (int32) at +0x0C
//   +0x10: ptr to half[]
//   +0x18: count2 (int32)
//   +0x1C: cap2 (int32)
static void CitizenUI_BuildIconArray_Impl(uintptr_t param1, uintptr_t groupElemIBP)
{
    fn_InternalBasePtr_CheckPtr((void*)groupElemIBP);
    void* groupElem = *(void**)groupElemIBP;
    int childCount = fn_Element_ChildCount(groupElem);
    if (childCount <= 0) return;

    // Layout of param1 (two dynamic arrays, each: ptr@+0,count@+4(in an int),cap@+4next):
    // From decompile: param_1 is longlong*, param_1[1] has count in lower 32 bits
    //   param_1[0] = whole ptr, lower int of param_1[1] = whole count, upper int of param_1[1] = whole cap
    //   param_1[2] = half ptr,  lower int of param_1[3] = half count,  upper int of param_1[3] = half cap
    // Actually the struct is 0x20 bytes:
    //   +0x00 ptr64, +0x08 count32+cap32, +0x10 ptr64, +0x18 count32+cap32

    for (int childIdx = 0; childIdx < childCount; childIdx++) {
        // --- Whole icon element ---
        {
            // Build String "whole"
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "whole");

            // Get child element by index
            uint8_t childBuf[16];
            fn_Element_Child(groupElem, childBuf, childIdx);
            fn_InternalBasePtr_CheckPtr((void*)childBuf);
            void* childElem = *(void**)childBuf;

            // FindElement "whole" in child
            uint8_t foundBuf[16] = {};
            fn_FindElement(childElem, foundBuf, nameStr);
            fn_String_Dtor(nameStr);

            fn_InternalBasePtr_CheckPtr((void*)foundBuf);
            void* wholeElem = *(void**)foundBuf;

            // Check IsDerivedFrom Element (use g_TypeId_ToolUI as placeholder — actually
            // the original checks _typeInfo_exref which resolves to Element type)
            // We skip the IsDerivedFrom check and trust the element is valid.
            // Wrap in InternalBasePtr
            uint8_t ibpBuf[8];
            fn_InternalBasePtr_Ctor(ibpBuf, wholeElem);

            // Push to whole array (dynamic array at param1+0):
            // count at param1+8 (int), cap at param1+12 (int), ptr at param1+0
            uint32_t oldCount = *(uint32_t*)(param1 + 8);
            uint32_t newCount = oldCount + 1;
            void*    oldPtr   = *(void**)(param1);
            *(uint32_t*)(param1 + 8) = newCount;

            int oldCap = *(int*)(param1 + 12);
            if (oldCap < (int)newCount) {
                int newCap = (oldCount != 0) ? (int)newCount * 3 : (int)newCount;
                *(int*)(param1 + 12) = newCap;
                void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);
                *(void**)(param1) = newBuf;
            }

            void* curPtr = *(void**)(param1);
            // Copy old + write new entry
            int copyStart = (oldPtr == curPtr) ? (int)oldCount : 0;
            for (int k = copyStart; k < (int)newCount; k++) {
                uint64_t* dst = (uint64_t*)((uintptr_t)curPtr + k * 8);
                if (dst) {
                    *dst = 0;
                    if (k < (int)oldCount) {
                        *dst = *(uint64_t*)((uintptr_t)oldPtr + k * 8);
                    } else {
                        *dst = *(uint64_t*)ibpBuf;
                    }
                }
            }
            if (oldPtr != curPtr) {
                fn_MemFreeStatic(oldPtr);
            }
        }

        // --- Half icon element ---
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "half");

            // Re-fetch child (CheckPtr pattern)
            fn_InternalBasePtr_CheckPtr((void*)groupElemIBP);
            void* groupElem2 = *(void**)groupElemIBP;
            uint8_t childBuf[16];
            fn_Element_Child(groupElem2, childBuf, childIdx);
            fn_InternalBasePtr_CheckPtr((void*)childBuf);
            void* childElem = *(void**)childBuf;

            uint8_t foundBuf[16] = {};
            fn_FindElement(childElem, foundBuf, nameStr);
            fn_String_Dtor(nameStr);

            fn_InternalBasePtr_CheckPtr((void*)foundBuf);
            void* halfElem = *(void**)foundBuf;

            uint8_t ibpBuf[8];
            fn_InternalBasePtr_Ctor(ibpBuf, halfElem);

            // Push to half array (at param1+16)
            uintptr_t halfBase = param1 + 0x10;
            uint32_t oldCount = *(uint32_t*)(halfBase + 8);
            uint32_t newCount = oldCount + 1;
            void*    oldPtr   = *(void**)(halfBase);
            *(uint32_t*)(halfBase + 8) = newCount;

            int oldCap = *(int*)(halfBase + 12);
            if (oldCap < (int)newCount) {
                int newCap = (oldCount != 0) ? (int)newCount * 3 : (int)newCount;
                *(int*)(halfBase + 12) = newCap;
                void* newBuf = fn_MemAlloc(g_pMemAllocSingleton, (uint64_t)newCap * 8, 8);
                *(void**)(halfBase) = newBuf;
            }

            void* curPtr = *(void**)(halfBase);
            int copyStart = (oldPtr == curPtr) ? (int)oldCount : 0;
            for (int k = copyStart; k < (int)newCount; k++) {
                uint64_t* dst = (uint64_t*)((uintptr_t)curPtr + k * 8);
                if (dst) {
                    *dst = 0;
                    if (k < (int)oldCount) {
                        *dst = *(uint64_t*)((uintptr_t)oldPtr + k * 8);
                    } else {
                        *dst = *(uint64_t*)ibpBuf;
                    }
                }
            }
            if (oldPtr != curPtr) {
                fn_MemFreeStatic(oldPtr);
            }
        }

        // Re-check child count for loop condition
        fn_InternalBasePtr_CheckPtr((void*)groupElemIBP);
        groupElem = *(void**)groupElemIBP;
        childCount = fn_Element_ChildCount(groupElem);
    }
}

// ============================================================
// WorldGrid_FindEntity (0x1400f9a60)
// ============================================================
// BST search: finds entity at tile index param_3 in world grid tree.
// worldGrid+0x30 = root node ptr.
// Node layout (piVar1): *piVar1 = key (tile index), *(piVar1+6) = left, *(piVar1+8) = right
// On match: retval = *(piVar1+2) (entity InternalBasePtr, 8 bytes)
// Returns param_2 (retval ptr).
static void* WorldGrid_FindEntity_Impl(void* worldGrid, void* retval, int tileIndex)
{
    int* piVar1 = *(int**)((uintptr_t)worldGrid + 0x30);
    while (true) {
        if (piVar1 == nullptr) {
            fn_InternalBasePtr_Ctor(retval, nullptr);
            return retval;
        }
        if (*piVar1 <= tileIndex) {
            if (tileIndex <= *piVar1) {
                // Match
                if (piVar1 != nullptr) {
                    *(uint64_t*)retval = 0;
                    *(uint64_t*)retval = *(uint64_t*)(piVar1 + 2);
                    return retval;
                }
                fn_InternalBasePtr_Ctor(retval, nullptr);
                return retval;
            }
            // Go right
            piVar1 = *(int**)(piVar1 + 8);
        } else {
            // Go left
            piVar1 = *(int**)(piVar1 + 6);
        }
    }
}

// ============================================================
// Helper: Find element by name in dialog instance, return InternalPtr value
// ============================================================
static uint64_t FindDialogElem(void* dialogInst, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint64_t result = 0;
    fn_DialogInstance_FindElement(dialogInst, &result, nameStr);
    fn_String_Dtor(nameStr);
    return result;
}

// Helper: Find element by name in element tree, return InternalPtr value
static uint64_t FindElemInTree(void* elem, const char* name) {
    uint8_t nameStr[8];
    fn_String_Ctor_CStr(nameStr, name);
    uint64_t result = 0;
    fn_FindElement(elem, &result, nameStr);
    fn_String_Dtor(nameStr);
    return result;
}

// ============================================================
// Slot4: Setup — finds all named elements, stores pointers, sets button IDs
// ============================================================
// Original: 0x1400651A0
//
// Finds 15 named UI elements and stores them in CitizenUI fields.
// Elements found via DialogInstance::FindElement go through the full dialog tree;
// elements found via Element::FindElement use the parent element at +0x20.
// groupHearts and groupStars are processed into icon arrays by FUN_14006abf0.
// Three buttons get element IDs: buttonProfession(0x227), buttonHome(0x226),
// buttonFollow(0x228).

void CitizenUI_Slot4(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;
    void* dialogInst = *(void**)(s + 0x18);
    void* parentElem = *(void**)(s + 0x20);

    // --- Image elements (found via DialogInstance) ---
    *(uint64_t*)(s + 0x30) = FindDialogElem(dialogInst, "imageMale");
    *(uint64_t*)(s + 0x38) = FindDialogElem(dialogInst, "imageFemale");

    // --- Text/number elements (found via Element tree from +0x20) ---
    *(uint64_t*)(s + 0x48) = FindElemInTree(parentElem, "numberAge");
    *(uint64_t*)(s + 0x40) = FindElemInTree(parentElem, "textName");
    *(uint64_t*)(s + 0x50) = FindElemInTree(parentElem, "textProfession");
    *(uint64_t*)(s + 0x60) = FindElemInTree(parentElem, "textEducatedNo");
    *(uint64_t*)(s + 0x58) = FindElemInTree(parentElem, "textEducatedYes");
    *(uint64_t*)(s + 0x68) = FindElemInTree(parentElem, "textClothing");
    *(uint64_t*)(s + 0x70) = FindElemInTree(parentElem, "textTool");
    *(uint64_t*)(s + 0x90) = FindElemInTree(parentElem, "textNameIs");
    *(uint64_t*)(s + 0x98) = FindElemInTree(parentElem, "textAction");

    // --- Icon groups (heart/star arrays built from group children) ---
    {
        uint64_t groupHearts = FindElemInTree(parentElem, "groupHearts");
        CitizenUI_BuildIconArray_Impl(s + 0xA0, (uintptr_t)&groupHearts);
    }
    {
        uint64_t groupStars = FindElemInTree(parentElem, "groupStars");
        CitizenUI_BuildIconArray_Impl(s + 0xC0, (uintptr_t)&groupStars);
    }

    // --- Button elements (found via DialogInstance, with element IDs) ---
    *(uint64_t*)(s + 0x78) = FindDialogElem(dialogInst, "buttonProfession");
    *(uint64_t*)(s + 0x80) = FindDialogElem(dialogInst, "buttonHome");
    *(uint64_t*)(s + 0x88) = FindDialogElem(dialogInst, "buttonFollow");

    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x78), 0x227);
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x80), 0x226);
    fn_Element_SetElementId((void*)*(uint64_t*)(s + 0x88), 0x228);
}

// ============================================================
// Slot6: Close sub-dialog if open, then clear the reference.
// ============================================================
// Original: 0x140065130

void CitizenUI_Slot6(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;

    uint64_t subDialog = *(uint64_t*)(s + 0xE0);
    if (subDialog != 0) {
        // Get the UserInterface from DialogInstance (+0x18) -> +0x10
        void* dialogInst = (void*)*(uint64_t*)(s + 0x18);
        void* userInterface = *(void**)((uintptr_t)dialogInst + 0x10);
        fn_UI_CloseDialog(userInterface, (void*)(s + 0xE0));
    }

    // Reset +0xE0 to null InternalBasePtr
    uint64_t nullPtr = 0;
    fn_InternalBasePtr_Ctor(&nullPtr, nullptr);
    *(uint64_t*)(s + 0xE0) = nullPtr;
}

// ============================================================
// Slot7: Update — refreshes citizen info display
// ============================================================
// Original: 0x140064AE0
//
// Gets the CitizenAIComponent from the entity at +0x28. Updates:
// - Gender icon visibility (male/female based on +0x38 flag)
// - Education status (compares age +0x210 vs description +0x118)
// - Name text (from CitizenAI +0x28 System::String)
// - Age number (from +0x3C float * age multiplier)
// - Clothing text (from +0x1E4 index into string table at element +0xD8/+0xE0)
// - Tool text (from +0xAC/+0xA8 tool count and comparison with description +0x110)
// - Profession text (from +0x1F0 profession description, or fallback if child/worker)
// - Health hearts (FUN_14006aed0 with +0x50 value vs description +0x70)
// - Happiness stars (FUN_14006aed0 with +0x80 value vs description +0xA4)
// - Profession button enabled state (based on workplace/job assignment)
// - Home button virtual call (likely sets enabled state)
// - Name-is text parameter
// - Action text (based on current state from profession component)

void CitizenUI_Slot7(CitizenUI* self) {
    uintptr_t s = (uintptr_t)self;

    // Get entity from data source (+0x28), cast to ComponentEntity
    void* entityBase = *(void**)(s + 0x28);
    bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
    void* entity = isEntity ? entityBase : nullptr;
    uint64_t entityRef = 0;
    fn_InternalBasePtr_Ctor(&entityRef, entity);

    // Find CitizenAIComponent
    uint64_t citizenAIRef = 0;
    fn_FindComponent_CitizenAI((void*)entityRef, &citizenAIRef);
    uintptr_t ai = (uintptr_t)citizenAIRef;

    // Get CitizenDescription from CitizenAI (+0x18), check IsDerivedFrom
    void* descBase = *(void**)(ai + 0x18);
    bool isDesc = fn_IsDerivedFrom(descBase, *g_TypeId_CitizenDescription);
    void* citizenDesc = isDesc ? descBase : nullptr;

    // --- Gender icon visibility ---
    // +0x38 on CitizenAI: 0 = female, nonzero = male
    bool isMale = (*(int*)(ai + 0x38) != 0);
    // imageFemale (+0x38): visible if male, imageMale (+0x30): visible if female... wait
    // Actually: if isMale, show imageMale (hide female), else show imageFemale (hide male)
    if (isMale) {
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x38), 1, 0);  // hide imageFemale
    } else {
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x38), 0, 1);  // show imageFemale
    }
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x30), isMale ? 0 : 1, isMale ? 1 : 0);

    // --- Education visibility ---
    // age at +0x210 (int), education age threshold at description+0x118 via CitizenAI+0x288
    bool isEducated = (*(int*)(ai + 0x210) < *(int*)(*(uintptr_t*)(ai + 0x288) + 0x118));
    if (isEducated) {
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x60), 0, 1);  // show textEducatedNo
    } else {
        fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x60), 1, 0);  // hide textEducatedNo
    }
    fn_Element_SetFlagsOnOff((void*)*(uint64_t*)(s + 0x58),
                             isEducated ? 1 : 0, isEducated ? 0 : 1);

    // --- Name text ---
    // CitizenAI+0x28 is the citizen's name (System::String)
    fn_TextElement_SetParameter((void*)*(uint64_t*)(s + 0x40), 0, (void*)(ai + 0x28));

    // --- Age number ---
    float ageFloat = *(float*)(ai + 0x3c);
    int ageYears = (int)(ageFloat * *g_pAgeMultiplier);
    fn_NumberElement_SetValueInt((void*)*(uint64_t*)(s + 0x48), ageYears);

    // --- Clothing text (string table lookup) ---
    {
        void* clothingElem = (void*)*(uint64_t*)(s + 0x68);
        uint32_t clothingIdx = *(uint32_t*)(ai + 0x1e4);
        uintptr_t elemPtr = (uintptr_t)clothingElem;
        // Build UI::String from element's string table (+0xD8) and string entry (+0xE0)
        uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
        uint8_t uiStr[16];
        fn_UIString_Ctor_Index(uiStr, stringTable, clothingIdx);
        uint8_t textVal[16];
        *(uint64_t*)textVal = *(uint64_t*)uiStr;
        *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        fn_TextElement_SetText(clothingElem, textVal);
    }

    // --- Tool text ---
    // tool count at +0xAC, tool item at +0xA8
    // If tool count < 1: char value = 0
    // Else: compare tool item (+0xA8) with description+0x110; result = 1 if same, 2 if different
    int toolValue;
    if (*(int*)(ai + 0xAC) < 1) {
        toolValue = 0;
    } else {
        int toolItem = *(int*)(ai + 0xA8);
        void* descBase2 = *(void**)(ai + 0x18);
        bool isDesc2 = fn_IsDerivedFrom(descBase2, *g_TypeId_CitizenDescription);
        void* desc2 = isDesc2 ? descBase2 : nullptr;
        toolValue = (toolItem != *(int*)((uintptr_t)desc2 + 0x110)) + 1;
    }
    {
        void* toolElem = (void*)*(uint64_t*)(s + 0x70);
        uintptr_t elemPtr = (uintptr_t)toolElem;
        uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
        uint8_t uiStr[16];
        fn_UIString_Ctor_Index(uiStr, stringTable, toolValue);
        uint8_t textVal[16];
        *(uint64_t*)textVal = *(uint64_t*)uiStr;
        *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        fn_TextElement_SetText(toolElem, textVal);
    }

    // --- Profession text ---
    // If age < child threshold (CitizenAI+0x288 -> +0x44) or has workplace (+0x208 != 0):
    //   If workplace (+0x208) == 0: use ProfessionList+0x40 (child profession)
    //   Else: use ProfessionList+0x48 (worker profession)
    //   Build UI::String from profession+0x20 (string table) and profession+0x30 (name)
    // Else: use +0x1F0 (current profession description)
    {
        void* profElem = (void*)*(uint64_t*)(s + 0x50);
        uintptr_t profListBase = *(uintptr_t*)(*(uintptr_t*)*g_pGameWorldSingleton + 0xB0);

        bool isChild = (*(float*)(ai + 0x3c) < *(float*)(*(uintptr_t*)(ai + 0x288) + 0x44));
        bool hasWorkplace = (*(uintptr_t*)(ai + 0x208) != 0);

        uint8_t textVal[16];
        if (isChild || hasWorkplace) {
            uintptr_t profEntry;
            if (*(uintptr_t*)(ai + 0x208) == 0) {
                profEntry = *(uintptr_t*)(profListBase + 0x40);  // child
            } else {
                profEntry = *(uintptr_t*)(profListBase + 0x48);  // worker with workplace
            }
            uint64_t strTable = *(uint64_t*)(profEntry + 0x20);
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, strTable, (void*)(profEntry + 0x30));
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        } else {
            uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
            uint64_t strTable = *(uint64_t*)(profDesc + 0x20);
            uint8_t uiStr[16];
            fn_UIString_Ctor(uiStr, strTable, (void*)(profDesc + 0x30));
            *(uint64_t*)textVal = *(uint64_t*)uiStr;
            *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        }
        fn_TextElement_SetText(profElem, textVal);
    }

    // --- Health hearts and happiness stars ---
    CitizenUI_UpdateIconDisplay_Impl(s + 0xA0,
        *(float*)(ai + 0x50), *(float*)((uintptr_t)citizenDesc + 0x70));
    CitizenUI_UpdateIconDisplay_Impl(s + 0xC0,
        *(float*)(ai + 0x80), *(float*)((uintptr_t)citizenDesc + 0xA4));

    // --- Profession button enabled state ---
    // Enabled if workplace (+0x248) or job entity (+0x208) exists
    bool profEnabled = (*(uintptr_t*)(ai + 0x248) != 0) || (*(uintptr_t*)(ai + 0x208) != 0);
    {
        void* profBtn = (void*)*(uint64_t*)(s + 0x78);
        typedef void (*fn_VSetEnabled_t)(void* self, bool enabled);
        void** vtable = *(void***)profBtn;
        fn_VSetEnabled_t vfunc = (fn_VSetEnabled_t)vtable[8]; // vtable+0x40
        vfunc(profBtn, profEnabled);
    }

    // --- Home button virtual call ---
    {
        void* homeBtn = (void*)*(uint64_t*)(s + 0x80);
        typedef void (*fn_VSetEnabled_t)(void* self, bool enabled);
        void** vtable = *(void***)homeBtn;
        fn_VSetEnabled_t vfunc = (fn_VSetEnabled_t)vtable[8]; // vtable+0x40
        // Original passes result of CheckPtr on CitizenAI (always true if we got here)
        vfunc(homeBtn, true);
    }

    // --- textNameIs: set name parameter ---
    fn_TextElement_SetParameter((void*)*(uint64_t*)(s + 0x90), 0, (void*)(ai + 0x28));

    // --- Action text ---
    // Get entity reference from CitizenAI+0x10, find profession component
    uintptr_t entityFromAI = *(uintptr_t*)(ai + 0x10);
    uint64_t profCompRef = 0;
    fn_FindProfessionComponent((void*)entityFromAI, &profCompRef);

    // Read action index from profession component (+0x28)
    int actionIndex = *(int*)(*(uintptr_t*)&profCompRef + 0x28);

    // Special case: if action is 9 (idle/searching), check if citizen has a home tile
    if (actionIndex == 9) {
        uintptr_t homeRef = 0;
        int homeTile = *(int*)(ai + 0x30);
        if (homeTile >= 0) {
            // Look up entity at home tile via world grid
            void* worldGrid = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x118);
            uint64_t foundRef = 0;
            WorldGrid_FindEntity_Impl(worldGrid, &foundRef, homeTile);
            homeRef = foundRef;
        }
        if (homeRef != 0) {
            // Get profession description from home entity, compare with citizen's
            uintptr_t homeEntity = homeRef;
            typedef void* (*fn_VGetDesc_t)(void* self, void* retval);
            void** homeVtable = *(void***)((void*)homeEntity);
            fn_VGetDesc_t vGetDesc = (fn_VGetDesc_t)homeVtable[10]; // vtable+0x50
            uint64_t descRef = 0;
            vGetDesc((void*)homeEntity, &descRef);
            uintptr_t profDesc = *(uintptr_t*)(ai + 0x1F0);
            if (*(uintptr_t*)&descRef != profDesc) {
                actionIndex = 0x10;  // "looking for work" or similar
            }
        }
    }

    // Set action text from string table at element +0xD8
    {
        void* actionElem = (void*)*(uint64_t*)(s + 0x98);
        uintptr_t elemPtr = (uintptr_t)actionElem;
        uint64_t stringTable = *(uint64_t*)(elemPtr + 0xD8);
        uint8_t uiStr[16];
        fn_UIString_Ctor_Index(uiStr, stringTable, actionIndex);
        uint8_t textVal[16];
        *(uint64_t*)textVal = *(uint64_t*)uiStr;
        *(uint64_t*)(textVal + 8) = *(uint64_t*)(uiStr + 8);
        fn_TextElement_SetText(actionElem, textVal);
    }
}

// ============================================================
// Slot9: Button dispatch — profession (0x227), home (0x226), follow (0x228)
// ============================================================
// Original: 0x1400650F0
//
// Handles three button clicks:
// - buttonProfession (0x227): Navigate camera to citizen's workplace or job location
// - buttonHome (0x226): Navigate camera to citizen's home building
// - buttonFollow (0x228): Open follow dialog to track this citizen
//
// Navigation uses fn_Toolbar_CenterOnEntity (FUN_140103040) which moves
// the camera to the entity's build footprint center.

void CitizenUI_Slot9(CitizenUI* self, uint64_t /*param2*/, int param3, int param4) {
    // Only handle event type 0x12 (button click)
    if (param3 != 0x12) return;

    uintptr_t s = (uintptr_t)self;

    if (param4 == 0x227) {
        // --- buttonProfession: go to workplace/job ---
        void* entityBase = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        fn_InternalBasePtr_Ctor(&entityRef, entity);

        uint64_t aiRef = 0;
        fn_FindComponent_CitizenAI((void*)entityRef, &aiRef);
        uintptr_t ai = (uintptr_t)aiRef;

        uint64_t targetRef;
        uint64_t dialogRef;
        if (*(uintptr_t*)(ai + 0x208) != 0) {
            // Has workplace: navigate to workplace entity
            uintptr_t workplace = *(uintptr_t*)(ai + 0x208);
            targetRef = workplace;
            // Get dialog from workplace entity +0x10
            dialogRef = *(uint64_t*)(workplace + 0x10);
        } else {
            if (*(uintptr_t*)(ai + 0x248) == 0) return;
            // Has job entity (+0x248): navigate there
            targetRef = *(uintptr_t*)(ai + 0x248);
            dialogRef = *(uint64_t*)(s + 0x18);
        }

        void* gameSubsys = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x120);
        fn_Toolbar_CenterOnEntity(gameSubsys, &dialogRef, &targetRef);
    }
    else if (param4 == 0x226) {
        // --- buttonHome: go to home building ---
        void* entityBase = *(void**)(s + 0x28);
        bool isEntity = fn_IsDerivedFrom(entityBase, *g_TypeId_ComponentEntity);
        void* entity = isEntity ? entityBase : nullptr;
        uint64_t entityRef = 0;
        fn_InternalBasePtr_Ctor(&entityRef, entity);

        uint64_t aiRef = 0;
        fn_FindComponent_CitizenAI((void*)entityRef, &aiRef);
        uintptr_t ai = (uintptr_t)aiRef;

        uintptr_t homeEntity = *(uintptr_t*)(ai + 0x238);
        if (homeEntity == 0) return;

        uint64_t targetRef = homeEntity;
        uint64_t dialogRef = *(uint64_t*)(s + 0x18);
        void* gameSubsys = *(void**)((uintptr_t)*g_pGameWorldSingleton + 0x120);
        fn_Toolbar_CenterOnEntity(gameSubsys, &dialogRef, &targetRef);
    }
    else if (param4 == 0x228) {
        // --- buttonFollow: open follow dialog ---
        // This creates a FollowDialog (0xD8 bytes) from Dialog/Follow.rsc and adds
        // it to the UI. The FollowDialog constructor (FUN_1400695a0) requires hidden
        // stack parameters (entity ptr + null ptr) due to MSVC calling convention,
        // making it impractical to call from GCC without assembly glue.
        // Delegated to the original exe function.
        if (fn_CitizenUI_Slot9_Original) {
            fn_CitizenUI_Slot9_Original(self, 0, param3, param4);
        }
    }
}
