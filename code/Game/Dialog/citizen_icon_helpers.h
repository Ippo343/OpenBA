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
    // LEFT: the icon group is raw storage (H5) in its owners (CitizenUI,
    // StatusBarUI): its two Array<InternalPtr<Element>> are read at their offsets.
    UI::Element** wholeArr = *(UI::Element***)(iconArrayBase);
    int           count    = *(int*)(iconArrayBase + 8);
    UI::Element** halfArr  = *(UI::Element***)(iconArrayBase + 0x10);

    if (count <= 0) return;

    int fillLevel = (int)((currentVal / maxVal) * (float)(count * 2) + 0.5f);

    int iVar2 = 1;  // double-index (1-based, step 2)
    for (int i = 0; i < count; i++) {
        // Whole icon: show if iVar2 < fillLevel
        wholeArr[i]->SetFlagsOnOff(
            (iVar2 >= fillLevel) ? 1 : 0,
            (iVar2 < fillLevel) ? 1 : 0);

        // Half icon: show only if iVar2 == fillLevel
        halfArr[i]->SetFlagsOnOff(
            (iVar2 != fillLevel) ? 1 : 0,
            (iVar2 == fillLevel) ? 1 : 0);

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
    // LEFT: the caller hands the ADDRESS of its InternalPtr to the group element.
    UI::Element* groupElem = *(UI::Element**)groupElemIBP;
    if (!groupElem) return;
    int childCount = groupElem->ChildCount();
    if (childCount <= 0) return;

    for (int childIdx = 0; childIdx < childCount; childIdx++) {
        // --- Whole icon element ---
        {
            System::String nameStr("whole");

            UI::Element* childElem = groupElem->Child(childIdx).Get();
            UI::Element* wholeElem = childElem->FindElement(nameStr).Get();

            // Push to whole array (dynamic array at param1+0).
            // B.3.2 bridge: param1+0 is Collection::Array<InternalPtr<UI::Element>>
            // (stride 8, count +8, capacity +0xC) inside a struct the owner keeps as
            // raw storage (H5), so it is reached at its offset. The old copy loop wrote
            // `*dst = 0;` then unconditionally overwrote the same qword — a
            // transcription artifact with no net effect.
            auto& wholeArr = GameColl::ArrayAt<UI::Element*>(param1, 0);
            wholeArr.SetSize(wholeArr.GetSize() + 1, wholeElem);
        }

        // --- Half icon element ---
        {
            System::String nameStr("half");

            UI::Element* groupElem2 = *(UI::Element**)groupElemIBP;
            UI::Element* childElem = groupElem2->Child(childIdx).Get();
            UI::Element* halfElem = childElem->FindElement(nameStr).Get();

            // Push to half array (at param1+0x10): the same Array, count +0x18,
            // capacity +0x1C.
            uintptr_t halfBase = param1 + 0x10;
            auto& halfArr = GameColl::ArrayAt<UI::Element*>(halfBase, 0);
            halfArr.SetSize(halfArr.GetSize() + 1, halfElem);
        }

        // Re-check group element (may have been reallocated)
        groupElem = *(UI::Element**)groupElemIBP;
        childCount = groupElem->ChildCount();
    }
}
