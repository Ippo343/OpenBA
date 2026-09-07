#pragma once
// ============================================================================
// runtime/types/ui/NumberElement.h — UI::NumberElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D. NumberElement is a SIBLING of NumberEditElement, not related
// to it — it is a read-only numeric display (extends TextElement directly,
// not EditElement), proven by its own SetValue pair
// (0x180118d50 int / 0x180118db0 float), which never touch +0xC8/+0xE0/+0xF8
// the way EditElement's family does. It stores a TAGGED UNION of an int and a
// float value plus a discriminant:
//
//   +0xC8  _mode       int32   0 == _intValue is the live value, 1 == _floatValue
//   +0xD8  _intValue   int32   ?SetValue@NumberElement@UI@@QEAAXH@Z (0x180118d50)
//   +0xDC  _floatValue float   ?SetValue@NumberElement@UI@@QEAAXM@Z (0x180118db0)
//
// Both setters write BOTH fields on every real change (the "other" one is
// zeroed, not left stale) and flip _mode; both early-out when the incoming
// value equals the field already holding it AND _mode already agrees — see
// widgets_input.cpp for the exact guard, preserved byte-for-byte.
//
// The gap +0xC0..+0xC7 (TextElement's own leading field, e.g. its display
// System::String) and +0xCC..+0xD7 are not reversed by this lane.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI {

struct NumberElement {
    Element  base;               // +0x00 .. +0xBF
    uint8_t  _pad_c0_c8[0x08];   // +0xC0 .. +0xC7 — TextElement territory, not reversed here.
    int32_t  _mode;               // +0xC8
    uint8_t  _pad_cc_d8[0x0C];   // +0xCC .. +0xD7 — not reversed by this lane.
    int32_t  _intValue;           // +0xD8
    float    _floatValue;         // +0xDC
};

static_assert(offsetof(NumberElement, base)       == 0x00, "NumberElement::base @ +0x00");
static_assert(offsetof(NumberElement, _mode)      == 0xC8, "NumberElement::_mode @ +0xC8");
static_assert(offsetof(NumberElement, _intValue)  == 0xD8, "NumberElement::_intValue @ +0xD8");
static_assert(offsetof(NumberElement, _floatValue)== 0xDC, "NumberElement::_floatValue @ +0xDC");

} // namespace UI
