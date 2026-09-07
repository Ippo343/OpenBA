#pragma once
// UI::TableElement -- offsets used by Wave 7 lane E, proven against retail
// Runtime-x64.dll disassembly (docs/runtime_port/WAVE7_LANE_E.md has the
// per-offset call sites).
//
// Element.h (lane A) landed while this lane's recon was in progress and
// CONFIRMS the inference this header originally had to make independently:
// sizeof(UI::Element) == 0xC0, Element::_flags @ +0x34, Element::_parent
// @ +0x98, and Element::_children (Collection::Array<Element*>) @ +0xA0 --
// exactly the fields TableElement::SetChild/SetSize touch below. The shared
// fields are now expressed via offsetof(UI::Element, ...) rather than a
// second copy of the magic numbers; only TableElement's OWN new fields
// (+0xC0 and up) are this header's own literals.
//
// Evidence (Runtime-x64.dll addresses):
//   GetRowCount     0x180079450  return *(int*)(this+0xc0)
//   GetColumnCount  0x180079460  return *(int*)(this+0xc4)
//   SetChild(e,col) 0x18011daa0  this+0xa0 = Collection::Array<InternalPtr<Element>>
//                                 head (data ptr at this+0xa0, indexed *8);
//                                 dirty chain via this+0x34 / this+0x98
//   SetChild(e,c,r) 0x18011db50  index = GetColumnCount()*col + row, delegates
//                                 to the two-arg SetChild
//   SetSize(r,c)    0x18011db90  this+0xc0=r, this+0xc4=c; resizes the two
//                                 int32 Arrays at +0xd0/+0xe0 (row/col extents)
//                                 and the cell Array at +0xa0 to r*c cells

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI { namespace TableElementOffsets {

constexpr int kCells      = offsetof(UI::Element, _children); // 0xA0 -- Element's OWN children array; TableElement's cell grid IS this array
constexpr int kRowCount   = 0xC0;  // int32_t -- TableElement's own field
constexpr int kColCount   = 0xC4;  // int32_t -- TableElement's own field
constexpr int kRowSizes   = 0xD0;  // Collection::Array<int32_t>, 0x10 bytes -- TableElement's own field
constexpr int kColSizes   = 0xE0;  // Collection::Array<int32_t>, 0x10 bytes -- TableElement's own field
constexpr int kDirtyFlags = offsetof(UI::Element, _flags);  // 0x34
constexpr int kParent     = offsetof(UI::Element, _parent); // 0x98

// UI::Element's kElementFlagNeedsSizeCalc (Element.h), OR'd into kDirtyFlags
// by every mutator in this lane and walked up the kParent chain.
constexpr uint32_t kDirtyBit = UI::kElementFlagNeedsSizeCalc;

} } // namespace UI::TableElementOffsets
