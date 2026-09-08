#pragma once
// UI::TextElement -- offsets used by Wave 7 lane E.
//
// *** This DISAGREES with the TextElement-base comment block that has lived
// in LabelElement.h since an earlier era (that file's "TextElement base
// (0xC0..0xFF)" table). That table was never cross-checked against a call
// site; this header's offsets are, and the fix is applied at LabelElement.h's
// source per PLAN.md sec6 step 8 (see the note left there). ***
//
// Evidence (Runtime-x64.dll addresses):
//   SetParameter(i,s)  0x18011e9b0  guards -1<i<*(int*)(this+0xd0); writes
//                                    System::String::operator=(
//                                      *(String**)(this+0xc8) + i*8, s)
//                                    i.e. an Array<System::String> whose DATA
//                                    pointer is at this+0xc8 and whose element
//                                    COUNT is at this+0xd0 -- matching
//                                    Collection::Array<T> {data,size,cap} laid
//                                    out from base +0xc8 (data +0xc8, size
//                                    +0xd0, cap +0xd4). Then dirty-chain walk,
//                                    then CacheText(this).
//   SetText(String)    0x180042070  this+0xd8 = param[0]; this+0xe0 = param[1]
//                                    (the raw UI::String value, 16 bytes);
//                                    dirty-chain walk; CacheText(this).
//   CacheText (private, but exported: ?CacheText@TextElement@UI@@AEAAXXZ,
//              0x18011ec10) reads/writes this+0xd0 (param count), converts
//              the UI::String at this+0xd8 to a System::String, and manages a
//              cached-formatted-text pointer at this+0xf8. NOT reimplemented
//              here -- called via RT_ORIGINAL_ONCE, see widgets_layout.cpp.
//   SetParameterCount (private, exported: 0x18011e8e0) likewise called
//              through, not reimplemented.
//
// this+0xc0..0xc8 (8 bytes) is read by neither of this lane's two functions
// and is NOT claimed here (LabelElement.h's old table guessed "font ptr" for
// it; that guess is unverified by this lane and left alone).

#include <cstdint>
#include "Element.h"

namespace UI { namespace TextElementOffsets {

// *** kParamCount CONTRADICTED BY MEASUREMENT on a NumberElement. Wave 9 lane M. ***
// Probing UI::NumberElement::SetValueInt on the tutorial routes shows the eight
// bytes at +0xD0 holding an OWNING, REFCOUNTED System::String data pointer (a
// heap address with a {length, refcount} header at [-8], payload = the formatted
// number), not an int32 size followed by an int32 capacity. Both readings cannot
// be right for the same offset. This header's offsets are the ones derived from
// call sites (0x18011e9b0) and are LEFT AS THEY ARE -- a NumberElement is not
// necessarily laid out like the TextElement this table was read from, and
// nothing here has been re-derived. NumberElement.h carries the same note and
// the evidence is in docs/runtime_port/WAVE9_LANE_M.md Finding 1. Resolve this
// before any lane mints or constructs either class at cutover.
constexpr int kParamArrayData = 0xC8;  // Collection::Array<System::String>.data -- TextElement's own field
constexpr int kParamCount     = 0xD0;  // Collection::Array<System::String>.size (int32_t) -- TextElement's own field
constexpr int kParamCapacity  = 0xD4;  // Collection::Array<System::String>.capacity (int32_t) -- inferred from the Array<T> shape, not read by either ported function
constexpr int kTextString     = 0xD8;  // UI::String, 16 bytes (0xD8, 0xE0) -- TextElement's own field
constexpr int kCachedText     = 0xF8;  // refcounted System::String data ptr, owned by CacheText -- TextElement's own field
constexpr int kDirtyFlags     = offsetof(UI::Element, _flags);  // 0x34, confirmed by Element.h (lane A)
constexpr int kParent         = offsetof(UI::Element, _parent); // 0x98, confirmed by Element.h (lane A)
constexpr uint32_t kDirtyBit  = UI::kElementFlagNeedsSizeCalc;

} } // namespace UI::TextElementOffsets
