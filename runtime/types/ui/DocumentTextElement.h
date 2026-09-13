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

// ============================================================================
// The PRIVATE TEXT PIPELINE's fields, added for runtime/src/ui/documenttext.cpp.
// ============================================================================
// Every constant below is read straight out of one of these seven retail bodies
// and nothing is inferred from a name:
//   GetNextItem        0x1800cdf40   GenerateItems      0x1800cf060
//   LayoutRow          0x1800cdbc0   FindNextWhiteSpace 0x1800cefd0
//   BeginLink          0x1800cdeb0   EndLink            0x1800cde00
//   DocumentTextDescription::CreateInstance 0x1800cf520
//
// The parser is a `^`-escape machine over a flat System::String. It appends
// fixed-size DocumentItems to the item array, flushes a row when the pen passes
// the right edge, and LayoutRow then gives every item of the finished row its x
// and y. The escapes it understands, in the order retail tests them:
//   ^n ^p ^t ^s ^f<d> ^c<d> ^j{l,r,c} ^v{t,c,b} ^m{b,c,e,l,r} ^l{b,e,i,o}
//   ^a{b,e} ^d<name> ^i<d><name>

namespace UI { namespace DocumentTextElementOffsets {

// ---- geometry the element publishes back to UI::Element --------------------
constexpr int kElementHeight   = 0x64;  // GenerateItems' last store (0x1800cf2fb)

// ---- description-supplied resources ---------------------------------------
constexpr int kSpriteSheet     = 0xD0;  // UI::SpriteSheet* (CreateInstance)
constexpr int kCaptureRefSheet = 0xD8;  // {SpriteSheet*, int} pair, 16 bytes
constexpr int kCaptureRefIndex = 0xE0;
constexpr int kFrameTable      = 0xE8;  // Array<{SpriteSheet*, int}>, stride 0x10
constexpr int kInt0F8          = 0xF8;
constexpr int kInt0FC          = 0xFC;
constexpr int kInt100          = 0x100;
constexpr int kTabWidth        = 0x104; // ^t advance and the ^lb / ^le indent step
constexpr int kWrapWidth       = 0x108; // the default right edge
constexpr int kFontArray       = 0x110; // ptr to Font*[] ; Font +0x50 = line
                                        // height, +0x54 = baseline
constexpr int kSheetArray      = 0x120; // ptr to SpriteSheet*[] (^i<digit>)
constexpr int kColorArray      = 0x130; // ptr to int[] (^c<digit> picks one)
constexpr int kResourceList    = 0x140; // the object list ^d<name> searches;
                                        // +0x20 data, +0x28 count

// ---- the generated item array ---------------------------------------------
constexpr int kItemArrayData   = 0x148;
constexpr int kItemCount       = 0x150;
constexpr int kItemCapacity    = 0x154;
constexpr int kItemStride      = 0x24;

// ---- the parse cursor ------------------------------------------------------
constexpr int kTextLength      = 0x170; // cached length of the +0x168 string
constexpr int kTextPos         = 0x174;

// ---- the pen ---------------------------------------------------------------
constexpr int kPenX            = 0x178; // row origin / left margin (^ml sets it)
constexpr int kHangIndent      = 0x17C; // folded into kPenX at the next row flush
constexpr int kRightEdge       = 0x180; // ^mr sets it; defaults to kWrapWidth
constexpr int kInt184          = 0x184; // saved and restored across ^lo only
constexpr int kRowPenX         = 0x188; // running x inside the row being built
constexpr int kPenY            = 0x18C;
constexpr int kRowFirstItem    = 0x190; // index of the first item of this row

// ---- the link being accumulated (^ab ... ^ae) ------------------------------
// One 0x18-byte Link: the String at +0x198 plus the item range [+0x1a0, +0x1a4],
// pushed into the +0x158 array by EndLink. kLinkEntryStride above is its size.
constexpr int kLinkText        = 0x198;
constexpr int kLinkFirstItem   = 0x1A0; // < 0 means "no link open"
constexpr int kLinkLastItem    = 0x1A4;
constexpr int kLinkByte1A8     = 0x1A8;

// ---- the style state machine ----------------------------------------------
constexpr int kFontIndex       = 0x1B0; // ^f<digit>, also the ^li frame slot
constexpr int kColorIndex      = 0x1B4; // ^c<digit>
constexpr int kHAlign          = 0x1B8; // ^j : 0 left, 1 right, 2 centre
constexpr int kVAlign          = 0x1BC; // ^v : 0 top, 1 centre, 2 baseline
constexpr int kColumnTop       = 0x1C0; // ^mb records, ^mc rewinds to
constexpr int kColumnMaxY      = 0x1C4; // ^mc / ^me high-water mark

// ---- UI::Font and UI::SpriteSheet fields these bodies reach into -----------
constexpr int kFontLineHeight  = 0x50;
constexpr int kFontBaseline    = 0x54;
constexpr int kSheetSpriteData = 0x20;  // SpriteSheet's sprite array, stride 0x38
constexpr int kSpriteStride    = 0x38;
constexpr int kSpriteWidth     = 0x1C;
constexpr int kSpriteHeight    = 0x20;
// UI::SpriteCapture, what ^d<name> resolves to, reports its size directly.
constexpr int kCaptureWidth    = 0x50;
constexpr int kCaptureHeight   = 0x54;

} } // namespace UI::DocumentTextElementOffsets

namespace UI {

// UI::DocumentTextElement::DocumentItem — 0x24 bytes, the unit GetNextItem
// produces, GenerateItems appends and LayoutRow positions. The stride is fixed
// by LayoutRow's `LEA RDX,[0x3 + RBX*0x8] ; ADD RDX,RBX` addressing (9 dwords)
// and by the 0x24 step at 0x1800cdd81.
struct DocumentItem {
    int32_t x;          // +0x00  written only by LayoutRow
    int32_t y;          // +0x04  written only by LayoutRow
    int32_t width;      // +0x08
    int32_t height;     // +0x0c
    int32_t first;      // +0x10  text: first char; sprite: sprite index
    int32_t last;       // +0x14  text: last char, inclusive
    int32_t color;      // +0x18  resolved from kColorArray at emit time
    int32_t index;      // +0x1c  text: font slot; sprite: sheet slot
    uint8_t kind;       // +0x20  0 text, 1 sprite, 2 capture, 3 blank/tab
    uint8_t byte21;     // +0x21  cleared together with kind by one word store
    uint8_t pad22[2];   // +0x22
};
static_assert(sizeof(DocumentItem) == 0x24, "DocumentItem is 0x24 bytes");
static_assert(offsetof(DocumentItem, width)  == 0x08, "DocumentItem::width");
static_assert(offsetof(DocumentItem, height) == 0x0c, "DocumentItem::height");
static_assert(offsetof(DocumentItem, first)  == 0x10, "DocumentItem::first");
static_assert(offsetof(DocumentItem, last)   == 0x14, "DocumentItem::last");
static_assert(offsetof(DocumentItem, color)  == 0x18, "DocumentItem::color");
static_assert(offsetof(DocumentItem, index)  == 0x1c, "DocumentItem::index");
static_assert(offsetof(DocumentItem, kind)   == 0x20, "DocumentItem::kind");
static_assert(offsetof(DocumentItem, byte21) == 0x21, "DocumentItem::byte21");

} // namespace UI
