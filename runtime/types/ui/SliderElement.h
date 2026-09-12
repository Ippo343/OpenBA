#pragma once
// ============================================================================
// runtime/types/ui/SliderElement.h — UI::SliderElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D. `?SetParameter@SliderElement@UI@@QEAAXHAEBVString@System@@@Z`
// (0x1800d6ca0, this lane's only SliderElement export, HELD BACK — see
// WAVE7_LANE_D.md §3) resolves a CHILD via the same "children[0]" pattern
// SpinnerElement/ProgressElement use and writes only into that CHILD's own
// parameter array and calls its CacheText. It never reads or writes a field
// of SliderElement itself, so no additional field of SliderElement's own is
// proven or claimed by this lane — it is, as far as this lane's evidence
// goes, exactly an Element with no extra state.
//
// (This is also the class the existing engine/funcs/engine_funcs.h warning
// names: "Do NOT call [SpinnerElement::SetMinMax] on SliderElement — ...
// dereferences +0xa0 which is NULL on SliderElements", i.e. SliderElement's
// _children is empty — consistent with it having no value-holding child of
// its own the way SpinnerElement does.)
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"

// *** SUPERSEDED, 2026-09-12. *** The paragraph above is a true statement about
// what ?SetParameter@ alone could prove, and it is no longer the whole
// evidence: the elem_B batch ported the class (runtime/src/ui/
// sliderelement.cpp) from ??0SliderElement (0x1800D7570), ComputeSize
// (0x1800D7380), ComputeLayout (0x1800D6FC0), Draw (0x1800D6D40) and Update
// (0x1800D67E0). SliderElement has 0x70 bytes of its own state, and its
// _children[0] IS a real child (the optional numeric readout UpdateNumber
// refreshes) — so the old "its _children is empty" note beside
// SpinnerElement::SetMinMax describes a slider with no readout configured,
// not the class.
#include "Sprite.h"

namespace UI {

struct SliderElement {
    Element  base;              // +0x00 .. +0xBF
    ::Sprite _trackStart;       // +0xC0
    ::Sprite _trackMiddle;      // +0xD0
    ::Sprite _trackEnd;         // +0xE0
    ::Sprite _thumb;            // +0xF0
    ::Sprite _thumbHot;         // +0x100  drawn instead of _thumb while hovered
    int32_t  _trackOffsetY;     // +0x110  the groove's y offset inside the row
    int32_t  _thumbMargin;      // +0x114  inset at both ends of the travel
    float    _minimum;          // +0x118  ?SetMinimumValue@ 0x180049EA0
    float    _maximum;          // +0x11C  ?SetMaximumValue@ 0x180049EB0
    int32_t  _steps;            // +0x120  ctor 10; a page click moves range/steps
    uint8_t  _hovered;          // +0x124
    uint8_t  _pad125[3];        // +0x125
    float    _value;            // +0x128  ?SetCurrentValue@ 0x180049EC0
    float    _dragAnchor;       // +0x12C  _value at the moment the drag began
};

static_assert(sizeof(SliderElement) == 0x130, "deleting dtor 0x18004a1b2 strides 0x130");
static_assert(offsetof(SliderElement, base)         == 0x00,  "SliderElement::base @ +0x00");
static_assert(offsetof(SliderElement, _trackStart)  == 0xC0,  "");
static_assert(offsetof(SliderElement, _thumb)       == 0xF0,  "");
static_assert(offsetof(SliderElement, _trackOffsetY)== 0x110, "");
static_assert(offsetof(SliderElement, _minimum)     == 0x118, "");
static_assert(offsetof(SliderElement, _steps)       == 0x120, "");
static_assert(offsetof(SliderElement, _hovered)     == 0x124, "");
static_assert(offsetof(SliderElement, _value)       == 0x128, "");
static_assert(offsetof(SliderElement, _dragAnchor)  == 0x12C, "");

} // namespace UI
