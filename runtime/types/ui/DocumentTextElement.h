#pragma once
// UI::DocumentTextElement -- offsets used by Wave 7 lane E.
//
// Evidence (Runtime-x64.dll addresses):
//   SetText(UI::String)        0x18003bfd0  this+0xc0=param[0]; this+0xc8=param[1]
//                                (raw UI::String value, 16 bytes); dirty-chain walk.
//   SetText(System::String&)   0x18003c010  resets this+0xc0/0xc8 to {0,-1}
//                                (a String-index reset, same shape as
//                                UI::Sprite's {ptr,-1}); manually refcounts a
//                                SEPARATE plain System::String at this+0x168
//                                (release old, take new, refcount at ptr-4);
//                                dirty-chain walk.
//   GetLink(index)             0x18003c0c0  this+0x158 = link array data ptr,
//                                stride 0x18 (0x18-byte entries, String at
//                                entry+0x00); returns entry with the ref
//                                bumped, or an empty String if the slot is
//                                null. No bounds check observed on `index`
//                                (matches SpriteSheet::GetSpriteNameFromIndex's
//                                documented "no bounds checking" pattern).

#include <cstdint>
#include "Element.h"

namespace UI { namespace DocumentTextElementOffsets {

constexpr int kUiStringValue = 0xC0;  // UI::String, 16 bytes (0xC0, 0xC8) -- the by-value overload's storage, DocumentTextElement's own field
constexpr int kContentString = 0x168; // refcounted System::String* (plain, not UI::String) -- DocumentTextElement's own field
constexpr int kLinkArrayData = 0x158; // pointer to an array of 0x18-byte entries; entry+0x00 = System::String -- DocumentTextElement's own field
constexpr int kLinkEntryStride = 0x18;
constexpr int kDirtyFlags    = offsetof(UI::Element, _flags);  // 0x34, confirmed by Element.h (lane A)
constexpr int kParent        = offsetof(UI::Element, _parent); // 0x98, confirmed by Element.h (lane A)
constexpr uint32_t kDirtyBit = UI::kElementFlagNeedsSizeCalc;

} } // namespace UI::DocumentTextElementOffsets
