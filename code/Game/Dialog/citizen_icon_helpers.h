#pragma once
// Shared citizen icon display helpers.
// Used by CitizenUI and StatusBarUI (and potentially others) for health/happiness icons.
//
// Originally at:
//   CitizenUI_UpdateIconDisplay — FUN_14006aed0
//   CitizenUI_BuildIconArray   — FUN_14006abf0

#include "Component/coll_bridge.h"
#include "collection/Collection.h"   // Collection::Array
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
// The icon group (0x20 bytes in its owners CitizenUI / StatusBarUI): two
// Collection::Array<InternalPtr<Element>>, the whole icons (fully filled) and
// the half icons, passed as the two arrays.
//
// fillLevel = round((currentVal / maxVal) * (count * 2))
// For each icon slot i:
//   whole[i]: visible if its double-index < fillLevel
//   half[i]:  visible only if its double-index == fillLevel
inline void CitizenUI_UpdateIconDisplay_Impl(const Collection::Array<UI::Element*>& whole,
                                             const Collection::Array<UI::Element*>& half,
                                             float currentVal, float maxVal)
{
    UI::Element** wholeArr = whole._data;
    int           count    = whole._size;
    UI::Element** halfArr  = half._data;

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
// whole / half: the icon group's two arrays (as above)
// groupElemIBP: the InternalPtr to the parent group element (re-read per pass)
inline void CitizenUI_BuildIconArray_Impl(Collection::Array<UI::Element*>& whole,
                                          Collection::Array<UI::Element*>& half,
                                          const Object::InternalPtr<UI::Element>* groupElemIBP)
{
    UI::Element* groupElem = groupElemIBP->Get();
    if (!groupElem) return;
    int childCount = groupElem->ChildCount();
    if (childCount <= 0) return;

    for (int childIdx = 0; childIdx < childCount; childIdx++) {
        // --- Whole icon element ---
        {
            System::String nameStr("whole");

            UI::Element* childElem = groupElem->Child(childIdx).Get();
            UI::Element* wholeElem = childElem->FindElement(nameStr).Get();

            // Push to the whole array. The old copy loop wrote `*dst = 0;` then
            // unconditionally overwrote the same qword — a transcription
            // artifact with no net effect.
            whole.SetSize(whole.GetSize() + 1, wholeElem);
        }

        // --- Half icon element ---
        {
            System::String nameStr("half");

            UI::Element* groupElem2 = groupElemIBP->Get();
            UI::Element* childElem = groupElem2->Child(childIdx).Get();
            UI::Element* halfElem = childElem->FindElement(nameStr).Get();

            // Push to the half array.
            half.SetSize(half.GetSize() + 1, halfElem);
        }

        // Re-check group element (may have been reallocated)
        groupElem = groupElemIBP->Get();
        childCount = groupElem->ChildCount();
    }
}
