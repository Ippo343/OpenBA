#pragma once
// ============================================================================
// runtime/types/ui/NumberEditElement.h — UI::NumberEditElement layout
// ============================================================================
// Wave 7 lane D. NumberEditElement extends EditElement (this file) with
// exactly two proven fields:
//
//   +0xF8  _min  int32   ?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z (0x1800d4b70) arg 1
//   +0xFC  _max  int32   ?SetMinMax@NumberEditElement@UI@@QEAAXHH@Z (0x1800d4b70) arg 2
//
// The gap between EditElement's last proven byte (+0xE0) and +0xF8 belongs to
// EditElement/TextElement fields this lane did not reverse. The padding array
// below is sized as `0xF8 - sizeof(EditElement)`, and the static_asserts at
// the bottom are the actual proof: if EditElement.h's shape ever changes such
// that sizeof(EditElement) != 0xE8, offsetof(_min) stops being 0xF8 and the
// build fails here rather than silently mis-laying the struct.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "EditElement.h"

namespace UI {

struct NumberEditElement {
    EditElement base;                                  // +0x00 .. sizeof(EditElement)-1
    uint8_t     _pad_editend_f8[0xF8 - sizeof(EditElement)];
    int32_t     _min;                                   // +0xF8
    int32_t     _max;                                   // +0xFC
};

static_assert(sizeof(EditElement) == 0xE8, "EditElement's compiler-padded size, cited by the pad array above");
static_assert(offsetof(NumberEditElement, base) == 0x00, "NumberEditElement::base @ +0x00");
static_assert(offsetof(NumberEditElement, _min) == 0xF8, "NumberEditElement::_min @ +0xF8");
static_assert(offsetof(NumberEditElement, _max) == 0xFC, "NumberEditElement::_max @ +0xFC");

} // namespace UI
