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

// ############################################################################
// ENDGAME B.3 step 6 (block C5) — `UI::DocumentTextElement`, the REAL C++ class
// ############################################################################
//
// PARENT, read (B36_SLOTS.md section 3), recorded not used (RULE B6-7):
//     ?_TypeName@DocumentTextElement@UI@@CAPEBDXZ       @0x18003be10 -> "DocumentTextElement"
//     ?_ParentTypeName@DocumentTextElement@UI@@CAPEBDXZ @0x18003be20 -> "Element"
//
// `??_7DocumentTextElement@UI@@6B@` @ 0x1801566f0, TWELVE slots; sizeof 0x1c8
// (`MOV EDX,0x1c8` in the slot-0 vector deleting dtor 0x18003c650):
//   [0] 0x18003c650 deleting dtor (RT_DELDTOR row)  [1] 0x18003be50 TypeName
//   [2] 0x18003be30 Type        [3] Element::Persist (inherited)
//   [4] 0x1800cdb60 ComputeSize [5] 0x1800cda00 ComputeLayout
//   [6] 0x1800cd7a0 Draw        [7] 0x1800cd360 Update — OVERRIDDEN
//   [8..11] Element's (inherited)
//
// THE FIELDS are the offsets above (DocumentTextElementOffsets, every one read
// out of a retail body) under names, cross-asserted below, plus what the ctor
// (0x1800cf410, factory_ui1_tables.inc kDocumentTextElement), the copy ctor
// (0x18003c140) and the dtor (0x1800cf320) — documenttextelement.cpp's doors —
// store: the ctor writes +0xc0..+0x168 and +0x198..+0x1a8 and NOTHING in
// +0x170..+0x197 or +0x1b0..+0x1c7 (GenerateItems resets those before use); the
// copy ctor copies +0x170..+0x193 and +0x1b0..+0x1c7, so +0x10c, +0x194 and
// +0x1a9..+0x1af are written by nothing.
//
// DocumentItem (0x24) and Link (0x18) are retail's NESTED value types
// (`AEAUDocumentItem@12@` in GetNextItem's mangling). `UI::DocumentItem`
// remains as an alias for the transition.
//
// ABSTRACT (slot 0 `= 0`), NO real ctor (the `??0DocumentTextElement` door
// builds every live one; it wears the MINTED table).
// ############################################################################
#include <cstddef>
#include "Sprite.h"

namespace UI {

class DocumentTextElement : public Element {
public:
    // ---- the nested value types -------------------------------------------
    // The unit GetNextItem produces, GenerateItems appends and LayoutRow
    // positions. The stride is fixed by LayoutRow's `LEA RDX,[0x3 + RBX*0x8] ;
    // ADD RDX,RBX` addressing (9 dwords) and by the 0x24 step at 0x1800cdd81.
    struct DocumentItem {
        int32_t x;          // +0x00  written only by LayoutRow
        int32_t y;          // +0x04  written only by LayoutRow
        int32_t width;      // +0x08
        int32_t height;     // +0x0c
        int32_t first;      // +0x10  text: first char; sprite: sprite index
        int32_t last;       // +0x14  text: last char, inclusive
        int32_t color;      // +0x18  resolved from _colors at emit time
        int32_t index;      // +0x1c  text: font slot; sprite: sheet slot
        uint8_t kind;       // +0x20  0 text, 1 sprite, 2 capture, 3 blank/tab
        uint8_t byte21;     // +0x21  Update's underline: 0 none, 1 full, 2 trimmed
        uint8_t pad22[2];   // +0x22
    };
    // One clickable span. Its compiler-generated operator= (0x18003c760,
    // unexported) is ported in documenttext.cpp.
    struct Link {
        char*   text;       // +0x00  System::String, the target
        int32_t firstItem;  // +0x08  < 0 means "no link open"
        int32_t lastItem;   // +0x0c  one past the last item
        uint8_t armed;      // +0x10  pressed on this link
        uint8_t pad11[7];   // +0x11
        Link& operator=(const Link& src);                         // 0x18003c760
    };

    // ---- the twelve slots, in retail order --------------------------------
    Object::Base* DeletingDtor(unsigned flags) override = 0;      // [0] 0x18003c650
    const char* TypeName() const override;                        // [1] 0x18003be50
    uint32_t    Type() const override;                            // [2] 0x18003be30
    void ComputeSize() override;                                  // [4] 0x1800cdb60
    void ComputeLayout(int32_t x, int32_t y,
                       int32_t w, int32_t h) override;            // [5] 0x1800cda00
    void Draw(SpriteContext& context, uint8_t alpha) const override; // [6] 0x1800cd7a0
    // [7] 0x1800cd360 — the handle arrives as the ADDRESS of the caller's
    // temporary (Element.h's banner), hence the pointer.
    void Update(const System::Time& time, UserInterface* ui,
                Object::InternalPtr<DialogInstance>* dialog,
                const UpdateState& state) override;

    // ---- non-virtual public members ---------------------------------------
    // Bodies in runtime/src/ui/widgets_layout.cpp (doors; fragment pass).
    System::String GetLink(int32_t index) const;                  // 0x18003c0c0 RETSLOT
    void SetText(const System::String& text);                     // 0x18003c010
    void SetText(::UIString text);                                // 0x18003bfd0

    DocumentTextElement(const DocumentTextElement& src);          // 0x18003c140  door (copy ctor)
    DocumentTextElement& operator=(const DocumentTextElement& src); // 0x18003c3d0 not ported
    static uint32_t TypeId();                                     // not ported
    static Object::Base* CreateNewObject();                       // 0x18003bf40  factory door
    static Object::Base* CreateInPlaceObject(void* place);        // 0x18003bfc0  factory door

    // ---- the private text pipeline (`AEAA` in retail) ----------------------
    // Public here only because each keeps a registered door that reaches it.
    // Bodies in runtime/src/ui/documenttext.cpp.
    void    GenerateItems();                                      // 0x1800cf060
    bool    GetNextItem(DocumentItem& item);                      // 0x1800cdf40
    int32_t LayoutRow();                                          // 0x1800cdbc0
    int32_t FindNextWhiteSpace();                                 // 0x1800cefd0
    void    BeginLink(const System::String& target);              // 0x1800cdeb0
    void    EndLink();                                            // 0x1800cde00
    void    EndRow();                                             // 0x1800cddb0  not ported
    bool    IsWhiteSpace(char c) const;                           // 0x1800cf030  not ported

protected:
    DocumentTextElement() = default;   // NOT ??0 (0x1800cf410, a door)
    ~DocumentTextElement();            // ??1 0x1800cf320 — the door; declared only

private:
    static const char* _TypeName();                               // 0x18003be10
    static const char* _ParentTypeName();                         // 0x18003be20
    static Object::BaseTypeInfo _typeInfo;                        // minted

public:
    ::UIString _text;             // +0xc0   UI::String {table, index}: SetText(UI::String), the applier
    void*    _spriteSheet;        // +0xd0   ExternalPtr<UI::SpriteSheet>
    ::Sprite _underlineSprite;    // +0xd8   the 1px link underline (Draw)
    // +0xe8 Array<UI::Sprite>, one per font slot (^li's bullet). Raw header:
    // copy/destroy/resize are addressed helpers (0x180037020 / 0x180037310 /
    // 0x1800370c0), like every array below.
    void*    _frameTableData;     // +0xe8
    int32_t  _frameTableCount;    // +0xf0
    int32_t  _frameTableCapacity; // +0xf4
    uint32_t _linkColor;          // +0xf8   ctor 0xffeeee00
    uint32_t _linkHoverColor;     // +0xfc   ctor 0xffffff00
    uint32_t _linkPressedColor;   // +0x100  ctor 0xffcccc00
    int32_t  _tabWidth;           // +0x104  ctor 4; ^t advance, ^lb/^le indent step
    int32_t  _wrapWidth;          // +0x108  ctor 300; the default right edge
    uint32_t _pad10c;             // +0x10c  written by nothing
    void*    _fontsData;          // +0x110  Array<UI::Font*>; Font +0x50 line height, +0x54 baseline
    int32_t  _fontsCount;         // +0x118
    int32_t  _fontsCapacity;      // +0x11c
    void*    _sheetsData;         // +0x120  Array<UI::SpriteSheet*> (^i<digit>, sprite items)
    int32_t  _sheetsCount;        // +0x128
    int32_t  _sheetsCapacity;     // +0x12c
    void*    _colorsData;         // +0x130  Array<int32> (^c<digit>)
    int32_t  _colorsCount;        // +0x138
    int32_t  _colorsCapacity;     // +0x13c
    void*    _resourceList;       // +0x140  object whose +0x20/+0x28 array ^d searches
    void*    _itemsData;          // +0x148  Array<DocumentItem>, stride 0x24
    int32_t  _itemsCount;         // +0x150
    int32_t  _itemsCapacity;      // +0x154
    void*    _linksData;          // +0x158  Array<Link>, stride 0x18
    int32_t  _linksCount;         // +0x160
    int32_t  _linksCapacity;      // +0x164
    char*    _sourceText;         // +0x168  System::String, the flat text parsed
    // ---- the parse state (GenerateItems resets it; the ctor does not) ----
    int32_t  _textLength;         // +0x170  cached length of _sourceText
    int32_t  _textPos;            // +0x174
    int32_t  _penX;               // +0x178  row origin / left margin (^ml)
    int32_t  _hangIndent;         // +0x17c  folded into _penX at the next flush
    int32_t  _rightEdge;          // +0x180  ^mr; defaults to _wrapWidth
    int32_t  _unknown184;         // +0x184  saved/restored across ^lo only
    int32_t  _rowPenX;            // +0x188  running x inside the row being built
    int32_t  _penY;               // +0x18c
    int32_t  _rowFirstItem;       // +0x190
    uint32_t _pad194;             // +0x194  written by nothing
    Link     _openLink;           // +0x198  the link being accumulated (^ab ... ^ae)
    int32_t  _fontIndex;          // +0x1b0  ^f<digit>
    int32_t  _colorIndex;         // +0x1b4  ^c<digit>
    int32_t  _hAlign;             // +0x1b8  ^j : 0 left, 1 right, 2 centre
    int32_t  _vAlign;             // +0x1bc  ^v : 0 top, 1 centre, 2 baseline
    int32_t  _columnTop;          // +0x1c0  ^mb records, ^mc rewinds to
    int32_t  _columnMaxY;         // +0x1c4  ^mc / ^me high-water mark
};

// The transition alias documenttext.cpp's `using UI::DocumentItem;` names.
using DocumentItem = DocumentTextElement::DocumentItem;

static_assert(sizeof(DocumentTextElement) == 0x1c8,
              "UI::DocumentTextElement is 0x1c8 (MOV EDX,0x1c8 in 0x18003c650)");
static_assert(alignof(DocumentTextElement) == 8, "UI::DocumentTextElement aligns to 8");
static_assert(sizeof(DocumentTextElement::DocumentItem) == 0x24, "DocumentItem is 0x24 bytes");
static_assert(offsetof(DocumentTextElement::DocumentItem, width)  == 0x08, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, height) == 0x0c, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, first)  == 0x10, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, last)   == 0x14, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, color)  == 0x18, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, index)  == 0x1c, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, kind)   == 0x20, "");
static_assert(offsetof(DocumentTextElement::DocumentItem, byte21) == 0x21, "");
static_assert(sizeof(DocumentTextElement::Link) == DocumentTextElementOffsets::kLinkEntryStride, "");
static_assert(offsetof(DocumentTextElement::Link, firstItem) == 0x08, "");
static_assert(offsetof(DocumentTextElement::Link, lastItem)  == 0x0c, "");
static_assert(offsetof(DocumentTextElement::Link, armed)     == 0x10, "");

namespace DocumentTextElementOffsets {
using C = DocumentTextElement;
static_assert(offsetof(C, _text)            == kUiStringValue, "");
static_assert(offsetof(C, _spriteSheet)     == kSpriteSheet, "");
static_assert(offsetof(C, _underlineSprite) == kCaptureRefSheet, "");
static_assert(offsetof(C, _frameTableData)  == kFrameTable, "");
static_assert(offsetof(C, _linkColor)       == kInt0F8, "");
static_assert(offsetof(C, _linkHoverColor)  == kInt0FC, "");
static_assert(offsetof(C, _linkPressedColor)== kInt100, "");
static_assert(offsetof(C, _tabWidth)        == kTabWidth, "");
static_assert(offsetof(C, _wrapWidth)       == kWrapWidth, "");
static_assert(offsetof(C, _fontsData)       == kFontArray, "");
static_assert(offsetof(C, _sheetsData)      == kSheetArray, "");
static_assert(offsetof(C, _colorsData)      == kColorArray, "");
static_assert(offsetof(C, _resourceList)    == kResourceList, "");
static_assert(offsetof(C, _itemsData)       == kItemArrayData, "");
static_assert(offsetof(C, _itemsCount)      == kItemCount, "");
static_assert(offsetof(C, _itemsCapacity)   == kItemCapacity, "");
static_assert(offsetof(C, _linksData)       == kLinkArrayData, "");
static_assert(offsetof(C, _sourceText)      == kContentString, "");
static_assert(offsetof(C, _textLength)      == kTextLength, "");
static_assert(offsetof(C, _textPos)         == kTextPos, "");
static_assert(offsetof(C, _penX)            == kPenX, "");
static_assert(offsetof(C, _hangIndent)      == kHangIndent, "");
static_assert(offsetof(C, _rightEdge)       == kRightEdge, "");
static_assert(offsetof(C, _unknown184)      == kInt184, "");
static_assert(offsetof(C, _rowPenX)         == kRowPenX, "");
static_assert(offsetof(C, _penY)            == kPenY, "");
static_assert(offsetof(C, _rowFirstItem)    == kRowFirstItem, "");
static_assert(offsetof(C, _openLink)        == kLinkText, "");
static_assert(offsetof(C, _openLink) + offsetof(DocumentTextElement::Link, firstItem) == kLinkFirstItem, "");
static_assert(offsetof(C, _openLink) + offsetof(DocumentTextElement::Link, lastItem)  == kLinkLastItem, "");
static_assert(offsetof(C, _openLink) + offsetof(DocumentTextElement::Link, armed)     == kLinkByte1A8, "");
static_assert(offsetof(C, _fontIndex)       == kFontIndex, "");
static_assert(offsetof(C, _colorIndex)      == kColorIndex, "");
static_assert(offsetof(C, _hAlign)          == kHAlign, "");
static_assert(offsetof(C, _vAlign)          == kVAlign, "");
static_assert(offsetof(C, _columnTop)       == kColumnTop, "");
static_assert(offsetof(C, _columnMaxY)      == kColumnMaxY, "");
static_assert(offsetof(Element, _layoutHeight) == kElementHeight, "");
}  // namespace DocumentTextElementOffsets

} // namespace UI
