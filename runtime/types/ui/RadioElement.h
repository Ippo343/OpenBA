#pragma once
// ============================================================================
// runtime/types/ui/RadioElement.h — UI::RadioElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D.
//
//   +0xC0  _checkBoxes  Collection::Array<UI::Element*>   data+0xC0/size+0xC8/cap+0xCC
//   +0xD0  _activeIndex int32
//
// Proof: `?FindCheckBoxes@RadioElement@UI@@AEAAX...@Z` (private, 0x1800d5750,
// held back — see WAVE7_LANE_D.md §3) grows this array with the same
// `FUN_xxx(&array, *(int*)(this+0xc8) + 1, &found)` shape ComboElement's
// arrays use. `?SetActiveRadio@` (0x1800d56f0, PORTED) reads the count from
// `*(int*)(this+0xc8)` (== `_checkBoxes._size`, i.e. `_checkBoxes` base +0x08)
// and the buffer from `*(long*)(this+0xc0)` (== `_checkBoxes._data`), and
// writes the index to `*(int*)(this+0xd0)`.
//
// The per-checkbox byte SetActiveRadio clears (`*(byte*)(checkbox+0xd8) = 0`)
// belongs to the CHECKBOX class, not RadioElement — that class is not one of
// this lane's eight and is addressed by raw offset in widgets_input.cpp
// rather than invented here.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"
#include "../collection/Collection.h"

namespace UI {

struct RadioElement {
    Element base;                                 // +0x00 .. +0xBF
    Collection::Array<Element*> _checkBoxes;       // +0xC0
    int32_t _activeIndex;                          // +0xD0
};

static_assert(offsetof(RadioElement, base)         == 0x00, "RadioElement::base @ +0x00");
static_assert(offsetof(RadioElement, _checkBoxes)  == 0xC0, "RadioElement::_checkBoxes @ +0xC0");
static_assert(offsetof(RadioElement, _activeIndex) == 0xD0, "RadioElement::_activeIndex @ +0xD0");

} // namespace UI
