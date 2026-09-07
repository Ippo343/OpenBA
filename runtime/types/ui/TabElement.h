#pragma once
// UI::TabElement -- offsets used by Wave 7 lane E.
//
// Evidence (Runtime-x64.dll addresses):
//   SetActiveTab(i)  0x18004c890  guard on this+0xf8 != i; sets this+0xf8=i;
//                                 tail-calls the private (but exported)
//                                 ?UpdateContainers@TabElement@UI@@AEAAXXZ
//                                 (0x1800d9710), NOT reimplemented here --
//                                 called via RT_ORIGINAL_ONCE.
//   AddTab(...)      0x1800da130  HELD BACK -- see WAVE7_LANE_E.md sec2. Reads
//                                 this+0xa0/+0xa8 (the shared Element/
//                                 ContainerElement children Array, same field
//                                 PageElement::SetActivePage below iterates)
//                                 and this+0xc0 (a running tab count it
//                                 increments), but the body constructs a
//                                 temporary ElementDescription/DialogInstance
//                                 and is not a leaf accessor.

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI { namespace TabElementOffsets {

constexpr int kActiveTab  = 0xF8;  // int32_t -- TabElement's own field
// Element::_children (Collection::Array<Element*>), confirmed by Element.h
// (lane A). AddTab reads it directly; not used by the one function this lane
// ported (SetActiveTab).
constexpr int kChildrenData  = offsetof(UI::Element, _children);
constexpr int kChildrenCount = kChildrenData + 8; // Collection::Array<T>::_size

} } // namespace UI::TabElementOffsets
