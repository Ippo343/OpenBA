#pragma once
// ============================================================================
// runtime/types/ui/ProgressElement.h — UI::ProgressElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D. Two one-instruction setters pin two fields directly:
//
//   +0x124  _maxValue      float   ?SetMaximumValue@ProgressElement@UI@@QEAAXM@Z (0x180077ec0)
//   +0x128  _currentValue  float   ?SetCurrentValue@ProgressElement@UI@@QEAAXM@Z (0x180077ed0)
//
// Both are a single unconditional store, no compare, no dirty flag, no
// exported reader. `?SetProgressTextParameter@` (0x180119b30, held back — see
// WAVE7_LANE_D.md §3) resolves a CHILD element via the same "children[0]"
// pattern SpinnerElement uses and writes into THAT child's own parameter
// array; it touches no field of ProgressElement itself, so nothing about it
// is modeled here.
//
// *** THE GAP +0xC0..+0x123 IS REVERSED. *** It said "not reversed by this
// lane" until the elem_B batch ported the class (runtime/src/ui/
// progresselement.cpp, 2026-09-12) from the ctor (0x180119C40), ComputeSize
// (0x1801198F0), ComputeLayout (0x180119610) and Draw (0x1801192C0). It is six
// UI::Sprites and one colour:
//
//   +0xC0  _fillLeft    +0xD0 _fillMiddle    +0xE0  _fillRight     the bar
//   +0xF0  _trackLeft   +0x100 _trackMiddle  +0x110 _trackRight    the groove
//   +0x120 _backgroundColor  uint32, ?SetBackgroundColor@ 0x180077EE0
//
// The ctor writes all six sprites as {null sheet, index -1} and the colour as
// 0xFFFFFFFF, then 1.0f and 0.25f into the two floats below. sizeof is 0x130,
// from the vector deleting destructor's `MOV EDX,0x130` at 0x180078182.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"
#include "Sprite.h"

namespace UI {

struct ProgressElement {
    RtUI::Element  base;              // +0x00 .. +0xBF
    ::Sprite _fillLeft;         // +0xC0
    ::Sprite _fillMiddle;       // +0xD0
    ::Sprite _fillRight;        // +0xE0
    ::Sprite _trackLeft;        // +0xF0
    ::Sprite _trackMiddle;      // +0x100
    ::Sprite _trackRight;       // +0x110
    uint32_t _backgroundColor;  // +0x120
    float    _maxValue;         // +0x124
    float    _currentValue;     // +0x128
    uint32_t _pad12c;           // +0x12C  tail padding to 0x130
};

static_assert(sizeof(ProgressElement) == 0x130, "deleting dtor 0x180078182 strides 0x130");
static_assert(offsetof(ProgressElement, _fillLeft)        == 0xC0,  "");
static_assert(offsetof(ProgressElement, _trackLeft)       == 0xF0,  "");
static_assert(offsetof(ProgressElement, _backgroundColor) == 0x120, "");
static_assert(offsetof(ProgressElement, base)         == 0x00,  "ProgressElement::base @ +0x00");
static_assert(offsetof(ProgressElement, _maxValue)    == 0x124, "ProgressElement::_maxValue @ +0x124");
static_assert(offsetof(ProgressElement, _currentValue)== 0x128, "ProgressElement::_currentValue @ +0x128");

} // namespace UI
