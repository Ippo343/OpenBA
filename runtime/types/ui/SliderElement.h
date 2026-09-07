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

namespace UI {

struct SliderElement {
    Element base;   // +0x00 .. +0xBF — no additional field proven by this lane.
};

static_assert(offsetof(SliderElement, base) == 0x00, "SliderElement::base @ +0x00");

} // namespace UI
