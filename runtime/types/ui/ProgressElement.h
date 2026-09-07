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
// The gap +0xC0..+0x123 is not reversed by this lane.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI {

struct ProgressElement {
    Element base;                    // +0x00 .. +0xBF
    uint8_t _pad_c0_124[0x64];      // +0xC0 .. +0x123 — not reversed by this lane.
    float   _maxValue;               // +0x124
    float   _currentValue;           // +0x128
};

static_assert(offsetof(ProgressElement, base)         == 0x00,  "ProgressElement::base @ +0x00");
static_assert(offsetof(ProgressElement, _maxValue)    == 0x124, "ProgressElement::_maxValue @ +0x124");
static_assert(offsetof(ProgressElement, _currentValue)== 0x128, "ProgressElement::_currentValue @ +0x128");

} // namespace UI
