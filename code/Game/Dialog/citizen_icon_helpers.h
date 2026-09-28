#pragma once
// Shared citizen icon display helpers.
// Used by CitizenUI and StatusBarUI (and potentially others) for health/happiness icons.
//
// Originally at:
//   CitizenUI_UpdateIconDisplay — FUN_14006aed0
//   CitizenUI_BuildIconArray   — FUN_14006abf0

#include "Component/coll_bridge.h"
#include "ui/UserInterface.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "runtime/types/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
#include <new>  // ENDGAME B.3.6 UI callers (K3)

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
            void* elem = *(void**)ibpAddr;
            reinterpret_cast<UI::Element*>(elem)->SetFlagsOnOff(
                (iVar2 >= fillLevel) ? 1 : 0,
                (iVar2 < fillLevel) ? 1 : 0);  // B.3.6 bridge
        }

        // Half icon: show only if iVar2 == fillLevel
        {
            uintptr_t ibpAddr = (uintptr_t)halfArr + offset;
            void* elem = *(void**)ibpAddr;
            reinterpret_cast<UI::Element*>(elem)->SetFlagsOnOff(
                (iVar2 != fillLevel) ? 1 : 0,
                (iVar2 == fillLevel) ? 1 : 0);  // B.3.6 bridge
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
    void* groupElem = *(void**)groupElemIBP;
    if (!groupElem) return;
    int childCount = reinterpret_cast<const UI::Element*>(groupElem)->ChildCount();  // B.3.6 bridge
    if (childCount <= 0) return;

    for (int childIdx = 0; childIdx < childCount; childIdx++) {
        // --- Whole icon element ---
        {
            System::String nameStr("whole");

            uint8_t childBuf[16];
            ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(groupElem)->Child(childIdx));  // B.3.6 bridge
            void* childElem = *(void**)childBuf;

            uint8_t foundBuf[16] = {};
            ::new (foundBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(childElem)->FindElement(nameStr));  // B.3.6 bridge

            void* wholeElem = *(void**)foundBuf;

            uint8_t ibpBuf[8];
            new (ibpBuf) ::Object::InternalBasePtr(
                reinterpret_cast<RtObject::Internal*>(wholeElem));   // B.3.3 bridge

            // Push to whole array (dynamic array at param1+0).
            // B.3.2 bridge: param1+0 is Collection::Array<{InternalBasePtr}>
            // (stride 8, count +8, capacity +0xC) inside a struct §B.2 has not
            // converted, so it is reached at its offset. The old copy loop wrote
            // `*dst = 0;` then unconditionally overwrote the same qword — a
            // transcription artifact with no net effect; only the second write
            // (the copied-old-entry or the new IBP) mattered.
            auto& wholeArr = GameColl::ArrayAt<GameColl::Bytes<8, 8>>(param1, 0);
            wholeArr.SetSize(wholeArr.GetSize() + 1, GameColl::Bytes<8, 8>(*(uint64_t*)ibpBuf));
        }

        // --- Half icon element ---
        {
            System::String nameStr("half");

            void* groupElem2 = *(void**)groupElemIBP;
            uint8_t childBuf[16];
            ::new (childBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(groupElem2)->Child(childIdx));  // B.3.6 bridge
            void* childElem = *(void**)childBuf;

            uint8_t foundBuf[16] = {};
            ::new (foundBuf) Object::InternalPtr<UI::Element>(reinterpret_cast<UI::Element*>(childElem)->FindElement(nameStr));  // B.3.6 bridge

            void* halfElem = *(void**)foundBuf;

            uint8_t ibpBuf[8];
            new (ibpBuf) ::Object::InternalBasePtr(reinterpret_cast<RtObject::Internal*>(halfElem));   // B.3.3 bridge

            // Push to half array (at param1+0x10).
            // B.3.2 bridge: param1+0x10 is Collection::Array<{InternalBasePtr}>
            // (stride 8, count +0x18, capacity +0x1C) inside a struct §B.2 has not
            // converted, so it is reached at its offset. Same transcription-artifact
            // double-write as the whole array above; only the second write mattered.
            uintptr_t halfBase = param1 + 0x10;
            auto& halfArr = GameColl::ArrayAt<GameColl::Bytes<8, 8>>(halfBase, 0);
            halfArr.SetSize(halfArr.GetSize() + 1, GameColl::Bytes<8, 8>(*(uint64_t*)ibpBuf));
        }

        // Re-check group element (may have been reallocated)
        groupElem = *(void**)groupElemIBP;
        childCount = reinterpret_cast<const UI::Element*>(groupElem)->ChildCount();  // B.3.6 bridge
    }
}
