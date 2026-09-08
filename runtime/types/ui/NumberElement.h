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
//
// *** +0xD0 IS NOT PADDING. Wave 9 lane M, MEASURED. ***
// `_pad_cc_d8` covers +0xCC..+0xD7, and the eight bytes at +0xD0 inside it hold
// an OWNING, REFCOUNTED System::String data pointer — a live heap address with
// a {length, refcount} header at [-8]. Measured by probing every divergence of
// UI::NumberElement::SetValueInt on the tutorial routes: after the call, that
// qword points at a freshly allocated block whose header reads len/rc and whose
// payload is the formatted number ("4" for value 4, "272" for 272, and so on
// for all ten observed calls). See docs/runtime_port/WAVE9_LANE_M.md Finding 1.
//
// THREE HEADERS IN THIS DIRECTORY DISAGREE ABOUT THESE BYTES, and that
// disagreement is its own defect, recorded here rather than silently resolved:
//   - THIS header             : +0xCC..+0xD7 unreversed padding
//   - TextElement.h           : +0xC8 Array<System::String>.data, +0xD0 its
//                               int32 `size`, +0xD4 its int32 `capacity`
//                               (derived from call sites at 0x18011e9b0)
//   - LabelElement.h          : an older "TextElement base (0xC0..0xFF)" table
//                               that TextElement.h already says was never
//                               cross-checked
// The observed 8-byte owning pointer fits none of the three. Nothing has been
// changed here on the strength of one function's behaviour — a written-extent
// fact belongs to a WRITER, not to a type
// (reference_never_written_needs_by_whom.md) — but ANY lane that mints a
// NumberElement vtable or constructs one of these at cutover must resolve this
// first: a field documented as padding that actually owns an allocation is how
// a ctor ships a leak or a double free.
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
