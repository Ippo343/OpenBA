#pragma once
// Shared citizen icon display helpers.
// Used by CitizenUI and StatusBarUI (and potentially others) for health/happiness icons.
//
// Originally at:
//   CitizenUI_UpdateIconDisplay — FUN_14006aed0
//   CitizenUI_BuildIconArray   — FUN_14006abf0

#include "engine_funcs.h"
#include <cstdint>

// ============================================================
// CitizenUI_UpdateIconDisplay (0x14006aed0)
// ============================================================
// Updates a status icon array (health hearts or happiness stars).
// iconArrayBase: pointer to icon group struct (0x20 bytes):
//   +0x00: ptr to whole-icon InternalBasePtr[] (fully filled icons)
//   +0x08: count (int32)
//   +0x0C: capacity (int32)
//   +0x10: ptr to half-icon InternalBasePtr[] (half-filled icons)
//   +0x18: count2 (int32)
//   +0x1C: cap2 (int32)
//
// fillLevel = round((currentVal / maxVal) * (count * 2))
// For each icon slot i:
//   whole[i]: visible if its double-index < fillLevel
//   half[i]:  visible only if its double-index == fillLevel
inline void CitizenUI_UpdateIconDisplay_Impl(uintptr_t iconArrayBase, float currentVal, float maxVal)
{
    uintptr_t* wholeArr = *(uintptr_t**)(iconArrayBase);
    int         count    = *(int*)(iconArrayBase + 8);
    uintptr_t* halfArr  = *(uintptr_t**)(iconArrayBase + 0x10);

    if (count <= 0) return;

    int fillLevel = (int)((currentVal / maxVal) * (float)(count * 2) + 0.5f);

    int iVar2 = 1;  // double-index (1-based, step 2)
    for (int i = 0; i < count; i++) {
        uintptr_t offset = (uintptr_t)i * 8;

        // Whole icon: show if iVar2 < fillLevel
        {
            uintptr_t ibpAddr = (uintptr_t)wholeArr + offset;
            fn_InternalBasePtr_CheckPtr((void*)ibpAddr);
            void* elem = *(void**)ibpAddr;
            fn_Element_SetFlagsOnOff(elem,
                (iVar2 >= fillLevel) ? 1 : 0,
                (iVar2 < fillLevel) ? 1 : 0);
        }

        // Half icon: show only if iVar2 == fillLevel
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
// param1: icon group struct base (0x20 bytes, same layout as above)
// groupElemIBP: InternalBasePtr to the parent group element
inline void CitizenUI_BuildIconArray_Impl(uintptr_t param1, uintptr_t groupElemIBP)
{
    fn_InternalBasePtr_CheckPtr((void*)groupElemIBP);
    void* groupElem = *(void**)groupElemIBP;
    if (!groupElem) return;
    int childCount = fn_Element_ChildCount(groupElem);
    if (childCount <= 0) return;

    for (int childIdx = 0; childIdx < childCount; childIdx++) {
        // --- Whole icon element ---
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "whole");

            uint8_t childBuf[16];
            fn_Element_Child(groupElem, childBuf, childIdx);
            fn_InternalBasePtr_CheckPtr((void*)childBuf);
            void* childElem = *(void**)childBuf;

            uint8_t foundBuf[16] = {};
            fn_FindElement(childElem, foundBuf, nameStr);
            fn_String_Dtor(nameStr);

            fn_InternalBasePtr_CheckPtr((void*)foundBuf);
            void* wholeElem = *(void**)foundBuf;

            uint8_t ibpBuf[8];
            fn_InternalBasePtr_Ctor(ibpBuf, wholeElem);

            // Push to whole array (dynamic array at param1+0)
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
            int copyStart = (oldPtr == curPtr) ? (int)oldCount : 0;
            for (int k = copyStart; k < (int)newCount; k++) {
                uint64_t* dst = (uint64_t*)((uintptr_t)curPtr + k * 8);
                if (dst) {
                    *dst = 0;
                    if (k < (int)oldCount)
                        *dst = *(uint64_t*)((uintptr_t)oldPtr + k * 8);
                    else
                        *dst = *(uint64_t*)ibpBuf;
                }
            }
            if (oldPtr != curPtr) fn_MemFreeStatic(oldPtr);
        }

        // --- Half icon element ---
        {
            uint8_t nameStr[8];
            fn_String_Ctor_CStr(nameStr, "half");

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

            // Push to half array (at param1+0x10)
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
                    if (k < (int)oldCount)
                        *dst = *(uint64_t*)((uintptr_t)oldPtr + k * 8);
                    else
                        *dst = *(uint64_t*)ibpBuf;
                }
            }
            if (oldPtr != curPtr) fn_MemFreeStatic(oldPtr);
        }

        // Re-check group element (may have been reallocated)
        fn_InternalBasePtr_CheckPtr((void*)groupElemIBP);
        groupElem = *(void**)groupElemIBP;
        childCount = fn_Element_ChildCount(groupElem);
    }
}
