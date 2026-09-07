#pragma once
// UI::PageElement -- offsets used by Wave 7 lane E.
//
// Evidence (Runtime-x64.dll addresses):
//   GetActivePage()    0x1800d5100  return *(int*)(this+0xc0)
//   SetActivePage(i)   0x1800d5110  guard: -2<i<*(int*)(this+0xa8) (the count
//                                   of the shared Element/ContainerElement
//                                   children Array -- same field TabElement
//                                   reads at +0xa8) AND i != current. Sets
//                                   this+0xc0=i, dirty-chain walk on `this`,
//                                   then for every child in the Array whose
//                                   data pointer is this+0xa0: toggles bit 0
//                                   of the CHILD's OWN dirty-flags word
//                                   (child+0x34) -- SET (OR 1) for every child
//                                   whose index != the new active page, CLEAR
//                                   (AND ~1) for the one whose index == the
//                                   new active page -- and dirty-chain-walks
//                                   that child (child+0x34 |= 0x8000000,
//                                   climbing child+0x98) whenever bit 0
//                                   actually flipped. Bit 0 of a child's own
//                                   dirty word reads as "hidden"/"not the
//                                   active page".

#include <cstdint>
#include <cstddef>
#include "Element.h"

namespace UI { namespace PageElementOffsets {

constexpr int kActivePage    = 0xC0;  // int32_t -- PageElement's own field
constexpr int kChildrenData  = offsetof(UI::Element, _children);       // confirmed by Element.h (lane A)
constexpr int kChildrenCount = kChildrenData + 8;                       // Collection::Array<T>::_size
constexpr int kDirtyFlags    = offsetof(UI::Element, _flags);           // 0x34; bit 0 here means "not the active page" (UI::kElementFlagHidden)
constexpr int kParent        = offsetof(UI::Element, _parent);          // 0x98
constexpr uint32_t kDirtyBit  = UI::kElementFlagNeedsSizeCalc;
constexpr uint32_t kHiddenBit = UI::kElementFlagHidden;

} } // namespace UI::PageElementOffsets
