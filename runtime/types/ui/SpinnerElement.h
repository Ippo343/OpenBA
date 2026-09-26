#pragma once
// ============================================================================
// runtime/types/ui/SpinnerElement.h — UI::SpinnerElement, binary-compatible layout
// ============================================================================
// Wave 7 lane D.
//
// SpinnerElement is a THIN WRAPPER. Every one of its seven exports (retail
// 0x1800d7c30..0x1800d7e10) opens the same way:
//
//     MOV RAX,[RCX+0xa0]      ; UI::Element::_children._data  (Element.h, lane A)
//     MOV RBX,[RAX]           ; _children._data[0] — the FIRST CHILD, not a
//                             ;   dedicated field of SpinnerElement's own
//     MOV EDX,[<typeid global>]
//     CALL IsDerivedFrom
//
// i.e. the "value" widget is the spinner's own children[0], read through the
// INHERITED Element array, and cast to NumberEditElement only if
// IsDerivedFrom agrees. This explains the existing warning in
// engine/funcs/engine_funcs.h ("SetMinMax dereferences +0xa0 which is NULL on
// SliderElements"): a SliderElement's _children is empty, so _data is null and
// the load through it faults — exactly the CLAUDE.md "faithful crash", not a
// bug to guard against.
//
// SpinnerElement's OWN additional state — beyond the inherited Element (0xC0)
// and the (inherited) _children array used above — is exactly one field:
//
//   +0xC0  _increment   int32   ?SetIncrement@SpinnerElement@UI@@QEAAXH@Z
//                                (0x1800d7d30) — one store, no exported reader.
//
// No ctor/dtor was decompiled for this lane and no accessor reads past +0xC4,
// so sizeof(SpinnerElement) is NOT asserted as "the true retail size" — only
// the one proven field's offset. The port's replay oracle compares
// sizeof(SpinnerElement) bytes (see widgets_input.cpp), which is therefore a
// LOWER BOUND on retail's real object, not the whole thing — stated in
// docs/runtime_port/WAVE7_LANE_D.md §4.
// ============================================================================

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI {

struct SpinnerElement {
    RtUI::Element base;          // +0x00 .. +0xBF
    int32_t _increment;    // +0xC0
};

static_assert(offsetof(SpinnerElement, base)       == 0x00, "SpinnerElement::base @ +0x00");
static_assert(offsetof(SpinnerElement, _increment) == 0xC0, "SpinnerElement::_increment @ +0xC0");

} // namespace UI
