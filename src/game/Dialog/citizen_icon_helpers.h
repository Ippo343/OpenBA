#pragma once
// Shared citizen icon display helpers.
// Used by CitizenUI and StatusBarUI (and potentially others) for health/happiness icons.
//
// Originally at:
//   CitizenUI_UpdateIconDisplay — FUN_14006aed0
//   (the group's Init, FUN_14006abf0, is TownHall_GroupRatingInit: Dialog/RatingGroup.h)

#include "collection/Collection.h"   // Collection::Array
#include "ui/UserInterface.h"
#include "system/String.h"
#include <cstdint>
#include "object/Internal.h"   // ENDGAME B.3.3 step 6B
#include "src/engine/ui/Element.h"  // ENDGAME B.3.6 UI callers (K3)
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
