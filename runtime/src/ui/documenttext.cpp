// ============================================================================
// runtime/src/ui/documenttext.cpp — UI::DocumentTextElement's private text
// pipeline, plus UI::DocumentTextDescription::CreateInstance
// ============================================================================
//
// Seven retail bodies, ported leaves-first so the two big ones call our own
// registered helpers rather than the DLL's:
//
//   0x1800cefd0  ?FindNextWhiteSpace@DocumentTextElement@UI@@AEAAHXZ
//   0x1800cde00  ?EndLink@DocumentTextElement@UI@@AEAAXXZ
//   0x1800cdeb0  ?BeginLink@DocumentTextElement@UI@@AEAAXAEBVString@System@@@Z
//   0x1800cdbc0  ?LayoutRow@DocumentTextElement@UI@@AEAAHXZ
//   0x1800cdf40  ?GetNextItem@DocumentTextElement@UI@@AEAA_NAEAUDocumentItem@12@@Z
//   0x1800cf060  ?GenerateItems@DocumentTextElement@UI@@AEAAXXZ
//   0x1800cf520  ?CreateInstance@DocumentTextDescription@UI@@QEBAXV?$InternalPtr@
//                    VDocumentTextElement@UI@@@Object@@PEBVDialog@2@@Z
//
// Offsets live in runtime/types/ui/DocumentTextElement.h, which this batch
// extended (append only); every constant there was read out of one of these
// seven bodies.
//
// ----------------------------------------------------------------------------
// WHAT THE PIPELINE IS
// ----------------------------------------------------------------------------
// GenerateItems converts the element's UI::String (a string-table reference at
// +0xc0) into a flat System::String at +0x168, resets the pen, and then pulls
// DocumentItems out of GetNextItem one at a time. Each item is appended to the
// +0x148 array; when the running row width would pass the right edge the row is
// flushed, which is where LayoutRow assigns x and y to every item of the row
// that just closed. GetNextItem is the lexer: it skips whitespace, and either
// cuts a word out of the text and measures it with the current font, or parses
// one `^` escape and mutates the style state.
//
// Escapes, in the order retail's dispatch chain tests them:
//   ^n  end row            ^p  end row twice (paragraph)
//   ^t  tab-width blank    ^s  zero-width blank
//   ^f<digit>  font slot   ^c<digit>  colour slot
//   ^j{l,r,c}  horizontal align     ^v{t,c,b}  vertical align
//   ^m{b,c,e}  column begin/continue/end
//   ^m{l,r}<int>           set left margin / right edge from a decimal run
//   ^l{b,e}    indent / outdent by kTabWidth
//   ^li        inline the current frame-table sprite
//   ^lo        LOOK AHEAD one item without committing (see below)
//   ^a{b,e}    begin / end a link
//   ^d<name>   a UI::SpriteCapture found by resource filename
//   ^i<digit><name>   a sprite from sheet slot <digit> by name
//
// ----------------------------------------------------------------------------
// ^lo IS A RECURSIVE CALL INTO GetNextItem, AND IT IS THE REASON THIS FILE
// REGISTERS BY ADDRESS AS WELL AS BY NAME.
// ----------------------------------------------------------------------------
// 0x1800ce707 is `CALL 0x1800cdf40` — GetNextItem calling itself. Retail saves
// eighteen fields of parse state (including an OWNING reference on the open
// link string), runs one whole nested parse to find out how wide the next item
// would be, restores all of it, and keeps only the width, which it turns into a
// hanging indent. Our body recurses directly into itself rather than through
// the seam, which is what retail does too.
//
// ----------------------------------------------------------------------------
// THE ONE ABI TRAP IN THIS BATCH: DocumentTextDescription::CreateInstance.
// ----------------------------------------------------------------------------
// The name is `QEBAX...` — it returns VOID, so there is NO hidden return slot.
// The `InternalPtr<DocumentTextElement>` is a BY-VALUE PARAMETER, and because
// it is a non-trivial class MSVC passes it by address, so the registers are
//     RCX = this (the description), RDX = &theInternalPtr, R8 = const Dialog*
// and the disassembly confirms it: `MOV R15,RDX` is written THROUGH at
// 0x1800cf57b (`MOV RCX,[R15]`), while R8 is never read at all — this class
// creates no child elements, exactly as ImageDescription's CreateInstance does
// not. Reading RDX as a return slot, or shifting the parameters by one, would
// be a wild write; this is the same shape the AnimatedImageDescription port
// documents, with the hidden slot ABSENT rather than present.
//
// SECOND TRAP IN THE SAME BODY, also shared with AnimatedImageDescription:
// ONE STACK SLOT IS WRITTEN AS A DWORD AND READ AS A QWORD. [RSP+0x30] takes a
// `MOV dword` at 0x1800cf56a / 0x1800cf570 / 0x1800cf5df / 0x1800cf5e5 /
// 0x1800cf62b / 0x1800cf631 and is read back with `MOV RAX,qword` at
// 0x1800cf582 / 0x1800cf5fb / 0x1800cf643, whose full eight bytes are then
// stored into the element. The upper half is whatever the previous occupant of
// that slot left behind, and retail reuses ONE slot for all three sites, so
// this file uses one uninitialised uint64_t for all three. The WIDTH of the
// store is the faithful part.
//
// ----------------------------------------------------------------------------
// NOT PORTED HERE, deliberately: ??1DocumentTextElement@UI@@ and
// ??1DocumentTextDescription@UI@@ are owned elsewhere in this wave.
// ============================================================================
#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_libm.h"   // rtcrt::StrICmp
#include "../../types/ui/DocumentTextElement.h"
#include "../../types/ui/SpriteCapture.h"   // B.3.9 TI-UI: SpriteCapture::TypeId()
#include "../../types/system/String.h"
#include "../../types/system/Filename.h"
#include "../../types/system/MemoryAllocator.h"
#include "../../types/object/External.h"
#include "../../types/collection/Collection.h"
#include "../../types/ui/Font.h"
#include "../../types/ui/SpriteSheet.h"
#include "../../types/ui/String.h"


namespace O = UI::DocumentTextElementOffsets;
using UI::DocumentItem;

namespace {

// ---- field accessors -------------------------------------------------------
inline uint8_t* B(void* p, int off) { return reinterpret_cast<uint8_t*>(p) + off; }
inline const uint8_t* B(const void* p, int off) {
    return reinterpret_cast<const uint8_t*>(p) + off;
}
inline int32_t&  I32(void* p, int off) { return *reinterpret_cast<int32_t*>(B(p, off)); }
inline int32_t   I32(const void* p, int off) { return *reinterpret_cast<const int32_t*>(B(p, off)); }
inline uint8_t&  U8 (void* p, int off) { return *reinterpret_cast<uint8_t*>(B(p, off)); }
inline void*&    PTR(void* p, int off) { return *reinterpret_cast<void**>(B(p, off)); }
inline void*     PTR(const void* p, int off) { return *reinterpret_cast<void* const*>(B(p, off)); }
inline char*&    STR(void* p, int off) { return *reinterpret_cast<char**>(B(p, off)); }

// Retail writes several ADJACENT FIELDS with ONE wide store — a qword covering
// two ints, a word covering two bytes. The width is the faithful part, so it is
// reproduced; memcpy rather than a punned pointer because the destinations are
// only four-byte aligned and belong to two different declared fields.
inline void Store64(void* dst, int64_t v) { __builtin_memcpy(dst, &v, sizeof v); }
inline void Store16(void* dst, uint16_t v) { __builtin_memcpy(dst, &v, sizeof v); }

// ---- System::String, the refcounted char* the whole DLL passes around -------
// The allocation is `[len:int32][refs:int32][chars...]`, so the refcount sits at
// data-4 and the length at data-8, and freeing means handing data-8 back.
inline int32_t& RefCount(char* s) { return *reinterpret_cast<int32_t*>(s - 4); }
inline int32_t  Length(const char* s) { return *reinterpret_cast<const int32_t*>(s - 8); }

// ?MemFree@MemoryAllocator@System@@SAXPEAX@Z (0x1800c7ad0), called directly.
void MemFree(void* p) { System::MemoryAllocator::MemFree(p); }

// Retail's String release, open-coded at fourteen sites in this file and
// IDENTICAL at every one of them — including the asymmetry the GroupDescription
// port documents: when the count reaches zero the SLOT is rewritten to data-8
// and that is what is freed, so the slot is left holding a freed pointer; when
// it does not reach zero the slot is NULLED. Normalising the two arms would be
// a divergence, so both are kept.
void ReleaseString(char** slot) {
    char* s = *slot;
    if (s == nullptr) return;                       // e.g. 1800ce420 TEST/JZ
    --RefCount(s);
    if (RefCount(s) == 0) {                         // e.g. 1800ce42c CMP/JNZ
        s -= 8;
        *slot = s;
        MemFree(s);
    } else {
        *slot = nullptr;
    }
}

// The assign-with-refcount retail inlines whenever a String FIELD takes a new
// value (BeginLink 0x1800cdec5, GetNextItem's ^lo restore 0x1800ce757,
// GenerateItems 0x1800cf0a4). Note the self-assignment test comes FIRST.
void AssignString(char** slot, char* value) {
    if (*slot == value) return;
    ReleaseString(slot);
    *slot = value;
    if (value != nullptr) ++RefCount(value);
}

// ---- the Runtime helpers these bodies call ---------------------------------
// A System::String / Filename held in a raw slot.
System::String& Str(char** slot) { return *reinterpret_cast<System::String*>(slot); }   // B.3.x bridge
const System::String& Str(char* const* slot) {
    return *reinterpret_cast<const System::String*>(slot);                            // B.3.x bridge
}
System::Filename& Path(char** slot) { return *reinterpret_cast<System::Filename*>(slot); }   // B.3.x bridge

// ?SubString@String@System@@QEBA?AV12@HH@Z — 0x1800c9230. The last argument is
// the INCLUSIVE end index, not a length: its body computes `end - start + 1`
// and reserves that many characters (0x1800c9262). `ret` is the hidden return
// slot the member's prvalue is placed in.
char** SubString(char* const* self, char** ret, int start, int endInclusive) {
    new (static_cast<void*>(ret)) System::String(Str(self).SubString(start, endInclusive));
    return ret;
}
// ?ToInteger@String@System@@SAHPEBD@Z — 0x18003f620, a static taking the raw
// char*, not the String.
int32_t ToInteger(const char* s) { return System::String::ToInteger(s); }

// Called by name below: Filename::AdjustDelimiters (0x1800c6970, private, in
// place), External::GetPath (0x180006250) and Filename::GetPathFilename
// (0x1800c6740) through their hidden return slots, the CRT _stricmp
// (0x18007eefc: TWO arguments, although Ghidra prints one -- RDX was loaded by
// the length comparison at 0x1800ce9f4 and is not re-loaded), Font::
// ComputeLineLayout (0x180116c70; retail passes (text, length, 0, 0, -1, 0,
// nullptr)), String + const char* (0x1800c92d0, retslot), SpriteSheet::
// GetSpriteIndexFromName (0x1800031f0), UI::String -> System::String
// (0x180035020, retslot) and the two Array SetSize-fill bodies 0x18003df80
// (DocumentItem, 0x24) and 0x18003e1a0 (Link, 0x18).

// The whitespace set the parser recognises, read out of the 64-bit BT mask
// `0x100002600` that all three scan loops materialise: bits 9, 10, 13 and 32 —
// tab, LF, CR and space. Nothing else counts, not even vertical tab or form
// feed, which is why the mask is kept as a mask instead of being re-spelled as
// an isspace() call.
constexpr uint64_t kWhitespaceMask = 0x100002600ull;
inline bool IsWhitespace(uint8_t c) {
    return c <= 0x20 && ((kWhitespaceMask >> c) & 1u) != 0;
}

// The ^d search compares against UI::SpriteCapture's type id: retail reads the
// global at 0x1801fa3f8 directly, the first dword of that class's `_typeInfo`
// -- `UI::SpriteCapture::TypeId()` (ENDGAME B.3.9 TI-UI).

}  // namespace

namespace UI {

// ============================================================================
// ?FindNextWhiteSpace@DocumentTextElement@UI@@AEAAHXZ   retail 0x1800cefd0
// ============================================================================
// Scans forward from the cursor for the first whitespace byte OR a '^', and
// returns its index; with nothing to scan it returns the cursor unchanged.
// It does NOT move the cursor — every caller writes the result back itself.
//
// The two counters are deliberate: RAX is the 64-bit index into the text and
// EDX the 32-bit value returned. They start equal and step together, so one
// int is enough here; the pair exists only because the load is indexed.
int32_t DocumentTextElement::FindNextWhiteSpace() {
    int32_t pos = _textPos;                      // 1800cefd0
    const int32_t len = _textLength;             // 1800cefd7
    if (pos >= len) return pos;                                // 1800cefe4

    const char* text = static_cast<const char*>(_sourceText);  // 1800cefe6
    for (;;) {
        const uint8_t c = static_cast<uint8_t>(text[pos]);     // 1800cf000
        if (IsWhitespace(c)) break;                            // 1800cf005..1800cf00f
        if (c == 0x5e) break;                                  // 1800cf011 '^'
        ++pos;                                                 // 1800cf017
        if (pos >= len) break;                                 // 1800cf01c
    }
    return pos;                                                // 1800cf021
}

// ============================================================================
// ?EndLink@DocumentTextElement@UI@@AEAAXXZ   retail 0x1800cde00
// ============================================================================
// Closes the link opened by BeginLink. If a link is open (kLinkFirstItem >= 0)
// its last item index is stamped, and if at least one item was actually emitted
// since it opened the whole 0x18-byte Link record — the String at +0x198 plus
// the two indices — is pushed into the +0x158 array. Then the record is reset
// to {null, -1, -1, 0} through the class's own assignment operator, which is
// what releases the String's reference.
//
// The reset is built as a temporary and assigned, not stored field by field:
// 0x18003c760 is the Link assignment operator and it owns the release of the
// old String. The temporary is then destroyed in the usual way.
void DocumentTextElement::EndLink() {

    const int32_t first = _openLink.firstItem;        // 1800cde12
    if (first < 0) return;                                     // 1800cde1a

    const int32_t items = _itemsCount;            // 1800cde1c
    _openLink.lastItem = items;                       // 1800cde22
    if (first < items) {                                       // 1800cde28
        void* array = &_linksData;
        rtcoll::RtColl_ArraySetSizeFill_18003e1a0(
            array, I32(array, 0x08) + 1, &_openLink); // 1800cde3f
    }

    // The empty Link: {char* = 0, int = -1, int = -1, bool = false}. Retail
    // writes it as one qword zero, one qword -1 (covering BOTH indices) and one
    // zero byte, which is why the two indices cannot be initialised separately
    // here without changing the store widths for no gain.
    Link empty;
    empty.text = nullptr;                                      // 1800cde44
    Store64(&empty.firstItem, -1);                             // 1800cde4d  ONE QWORD
    empty.armed = 0;                                           // 1800cde56

    _openLink = empty;                                         // 1800cde67  Link::operator= (was by address)

    ReleaseString(&empty.text);                                // 1800cde6d..1800cde95
}

// ============================================================================
// ?BeginLink@DocumentTextElement@UI@@AEAAXAEBVString@System@@@Z  0x1800cdeb0
// ============================================================================
// EndLink first (so `^ab` twice in a row closes the first link rather than
// leaking it), then take a reference on the new target string, then open the
// range at the CURRENT item count with an end of -1.
//
// The -1 is stored BEFORE the start index (0x1800cdf18 then 0x1800cdf22), and
// the two are separate dwords rather than one qword because the values differ.
void DocumentTextElement::BeginLink(const System::String& target) {
    char* const* name = reinterpret_cast<char* const*>(&target);   // one char*, retail's layout

    EndLink();                    // 1800cdec0

    AssignString(&_openLink.text, *name);             // 1800cdec5..1800cdf0f

    const int32_t items = _itemsCount;            // 1800cdf12
    _openLink.lastItem  = -1;                         // 1800cdf18
    _openLink.firstItem = items;                      // 1800cdf22
}

// ============================================================================
// ?LayoutRow@DocumentTextElement@UI@@AEAAHXZ   retail 0x1800cdbc0
// ============================================================================
// Positions every item from kRowFirstItem to the end of the array and returns
// the row's height, which the caller adds to the pen's y.
//
// Pass one measures the row: total width, and the tallest item. The seed for
// the height is 0 — EXCEPT for an EMPTY row (first == count), where it is the
// current font's line height, so a bare `^n` still advances by a line.
//
// Pass two walks the same items and writes x from the horizontal alignment and
// y from the vertical one. Three details that the decompiler smooths over and
// that are kept here:
//   * `this->_items` (+0x148) is RE-READ TWICE per iteration and the count once
//     more at the bottom; the loop does not hoist them.
//   * The centring divisions are two DIFFERENT operations. The horizontal one
//     is `CDQ ; SUB ; SAR 1`, i.e. C division truncating toward zero; the
//     vertical ones at 0x1800cdd61 / 0x1800cdd6d are BARE `SAR 1`, an
//     arithmetic shift that rounds toward negative infinity. They disagree for
//     odd negatives, so they are written differently here.
//   * The byte at item+0x20 selects the source of the item's baseline: a text
//     item (0) asks its FONT, anything else uses its own height.
int32_t DocumentTextElement::LayoutRow() {

    int32_t i         = _rowFirstItem;           // 1800cdbca
    int32_t rowWidth  = 0;                                     // 1800cdbd1
    int32_t rowHeight = 0;                                     // 1800cdbd7

    if (i == _itemsCount) {                       // 1800cdbda
        void* font = reinterpret_cast<void* const*>(_fontsData)
                         [_fontIndex];
        rowHeight = I32(font, O::kFontLineHeight);             // 1800cdbf4
    }

    {
        const int32_t count = _itemsCount;        // 1800cdbf8
        DocumentItem* items = static_cast<DocumentItem*>(_itemsData);
        for (int32_t k = i; k < count; ++k) {                  // 1800cdbff / 1800cdc32
            rowWidth += items[k].width;                        // 1800cdc20
            if (items[k].height > rowHeight)                   // 1800cdc24
                rowHeight = items[k].height;                   // 1800cdc27  CMOVG
        }
    }

    int32_t penX;
    const int32_t hAlign = _hAlign;              // 1800cdc34
    if (hAlign == 0) {
        penX = _penX;                            // 1800cdc3e
    } else if (hAlign == 1) {
        penX = _rightEdge - rowWidth;            // 1800cdc4b
    } else {
        // (right + left)/2 - rowWidth/2, both truncating toward zero.
        penX = (_rightEdge + _penX) / 2
             - rowWidth / 2;                                   // 1800cdc56..1800cdc71
    }

    while (i < _itemsCount) {                     // 1800cdc73 / 1800cdd85
        const size_t off = static_cast<size_t>(i) * O::kItemStride;

        DocumentItem* item =
            reinterpret_cast<DocumentItem*>(B(_itemsData, 0) + off);
        item->x = penX;                                        // 1800cdc97

        // The array pointer is loaded a SECOND time here (0x1800cdc9b) before
        // the width is read. Same value today; transcribed, not folded.
        item = reinterpret_cast<DocumentItem*>(B(_itemsData, 0) + off);
        penX += item->width;                                   // 1800cdca9

        const int32_t vAlign = _vAlign;          // 1800cdca2
        const int32_t penY   = _penY;
        if (vAlign == 2) {                                     // 1800cdcae  baseline
            if (item->kind == 0) {                             // 1800cdcb3
                void* font = reinterpret_cast<void* const*>(_fontsData)
                                 [item->index];
                item->y = penY - I32(font, O::kFontBaseline) + rowHeight;   // 1800cdcd8
            } else {
                item->y = penY - item->height + rowHeight;     // 1800cdcf1
            }
        } else if (vAlign == 0) {                              // 1800cdcfb  top
            if (item->kind == 0) {                             // 1800cdcff
                void* font = reinterpret_cast<void* const*>(_fontsData)
                                 [item->index];
                item->y = I32(font, O::kFontLineHeight)
                        - I32(font, O::kFontBaseline) + penY;  // 1800cdd25
            } else {
                item->y = penY;                                // 1800cdd33
            }
        } else {                                               // centre
            int32_t dy;
            if (item->kind == 0) {                             // 1800cdd3a
                void* font = reinterpret_cast<void* const*>(_fontsData)
                                 [item->index];
                // BARE arithmetic shift, not a division — see the banner.
                dy = (I32(font, O::kFontLineHeight)
                      - 2 * I32(font, O::kFontBaseline) + rowHeight) >> 1;  // 1800cdd61
            } else {
                dy = (rowHeight >> 1) - (item->height >> 1);    // 1800cdd6d
            }
            item->y = dy + penY;                               // 1800cdd7a
        }

        ++i;                                                   // 1800cdd7f
    }

    return rowHeight;                                          // 1800cdd9c
}

// ============================================================================
// The row flush, open-coded at TEN sites across GetNextItem and GenerateItems
// ============================================================================
// Always exactly these five steps, always in this order. It is written once
// here because ten transcriptions of the same eight instructions would be ten
// chances to mistype one of them, not because retail called anything.
namespace {
void FlushRow(DocumentTextElement* self) {
    self->_penY += self->LayoutRow();
    self->_rowFirstItem = self->_itemsCount;
    self->_penX += self->_hangIndent;
    self->_hangIndent = 0;
    self->_rowPenX = self->_penX;
}
}  // namespace

// ============================================================================
// ?GetNextItem@DocumentTextElement@UI@@AEAA_NAEAUDocumentItem@12@@Z  0x1800cdf40
// ============================================================================
// Returns false at end of text, otherwise fills `out` with one item. Escapes
// that only mutate state do not produce an item: they loop back and keep
// parsing, which is what the `continue`s below are.
bool DocumentTextElement::GetNextItem(DocumentItem& item) {
    DocumentItem* const out = &item;
    char** const textSlot = &_sourceText;

    int32_t len = _textLength;                   // 1800cdf60

    // ---- leading whitespace, skipped before anything else ------------------
    if (_textPos < len) {                        // 1800cdf71
        const char* text = *textSlot;                          // 1800cdf7a
        for (;;) {
            const int32_t pos = _textPos;        // 1800cdf81
            if (!IsWhitespace(static_cast<uint8_t>(text[pos]))) break;   // 1800cdf90
            _textPos = pos + 1;                  // 1800cdfa2
            if (pos + 1 >= len) break;                         // 1800cdfa8
        }
    }
    if (_textPos >= len) return false;           // 1800cdfb4 / 1800cdfb9

    for (;;) {
        // ---- one token: either an escape or a word -------------------------
        if ((*textSlot)[_textPos] != 0x5e) {     // 1800cdfcf
            // ================================================================
            // A WORD. Everything up to the next whitespace or '^'.
            // ================================================================
            const int32_t start = _textPos;      // 1800cee57
            const int32_t textLen = _textLength; // 1800cee61
            int32_t end = start;
            if (start < textLen) {                             // 1800cee6b
                const char* text = *textSlot;                  // 1800cee70
                int64_t k = start;
                for (;;) {
                    const uint8_t c = static_cast<uint8_t>(text[k]);
                    if (IsWhitespace(c)) break;                // 1800cee78
                    if (c == 0x5e) break;                      // 1800cee86
                    ++end; ++k;                                // 1800cee8d
                    if (end >= textLen) break;                 // 1800cee92
                }
            }

            char* word = nullptr;
            SubString(textSlot, &word, start, end - 1);        // 1800ceea2

            // The word is measured WITH A TRAILING SPACE appended (the literal
            // at 0x1801567e0 is a one-character " "), so the advance a word
            // contributes already includes the gap to the next one. That is
            // why no caller ever adds a space width of its own.
            char* measured = nullptr;
            new (static_cast<void*>(&measured)) System::String(Str(&word) + " ");   // 1800ceeb7
            ReleaseString(&word);                              // 1800ceebd..1800ceeea

            void* font = reinterpret_cast<void* const*>(_fontsData)
                             [_fontIndex];       // 1800ceeea

            Store64(&out->width, 0);                           // 1800cef02  ONE QWORD

            const int32_t measuredLen = measured ? Length(measured) : 0;   // 1800cef0b
            out->width = static_cast<const UI::Font*>(font)->ComputeLineLayout(   // 1800cef34
                measured, measuredLen, 0, 0, -1, 0, nullptr);
            out->height = I32(font, O::kFontLineHeight);       // 1800cef40
            out->first  = _textPos;              // 1800cef46
            out->last   = end - 1;                             // 1800cef4f
            out->color  = reinterpret_cast<const int32_t*>(_colorsData)
                              [_colorIndex];     // 1800cef63
            out->index  = _fontIndex;            // 1800cef6f
            Store16(&out->kind, 0);                            // 1800cef72  ONE WORD

            _textPos = end;                      // 1800cef78
            ReleaseString(&measured);                          // 1800cef7e..1800cefaf
            return true;                                       // 1800cefaf
        }

        // ====================================================================
        // AN ESCAPE. `^` then one selector byte, sometimes a second.
        // ====================================================================
        // The do/while(false) is the CONTROL FLOW, not a macro habit: every
        // abandoned escape in retail jumps to 0x1800cebc0 or 0x1800cebb7 — the
        // whitespace skip and the end-of-text test at the BOTTOM of this loop —
        // and never back to the top. `break` here is that jump.
        do {
            int32_t pos = _textPos + 1;          // 1800cdfe0
            _textPos = pos;
            if (pos == len) return false;                      // 1800cdfec
            char* text = *textSlot;                            // 1800cdfee
            const char sel = text[pos];                        // 1800cdff4

            if (sel == 'n') {                                  // 1800cdff4
                _textPos = pos + 1;              // 1800cdffd
                FlushRow(this);                                // 1800ce006..1800ce035
            } else if (sel == 'p') {                           // 1800ce040
                _textPos = pos + 1;              // 1800ce049
                FlushRow(this);                                // 1800ce052..1800ce081
                FlushRow(this);                                // 1800ce08a..1800ce0b9
            } else if (sel == 't') {                           // 1800ce0c4 -> 1800cee2b
                out->kind   = 3;                               // 1800cee2f
                out->width  = _tabWidth;         // 1800cee3a
                out->height = 0;                               // 1800cee3e
                ++_textPos;                      // 1800cee42
                return true;
            } else if (sel == 's') {                           // 1800ce0ce -> 1800cee18
                out->kind = 3;                                 // 1800cee1c
                ++_textPos;                      // 1800cee20
                return true;
            } else if (sel == 'f' || sel == 'c') {             // 1800ce0d8 / 1800ce119
                // ^f<digit> font slot, ^c<digit> colour slot. A non-digit is
                // SWALLOWED, not rejected: the cursor still steps past it.
                int32_t& field = (sel == 'f') ? _fontIndex : _colorIndex;
                ++pos;
                _textPos = pos;                  // 1800ce0e0
                if (pos == len) return false;                  // 1800ce0e6
                const char d = text[pos];                      // 1800ce0f2
                if (d >= '0' && d <= '9')                      // 1800ce0f6 / 1800ce0fc
                    field = d - '0';                          // 1800ce105
                _textPos = pos + 1;              // 1800ce10e
            } else if (sel == 'j' || sel == 'v') {             // 1800ce15a / 1800ce204
                // ^j{l,r,c} horizontal, ^v{t,c,b} vertical. Both close the
                // current row FIRST if it has anything in it, because an
                // alignment change cannot apply retroactively.
                ++pos;
                _textPos = pos;                  // 1800ce167
                if (pos == len) return false;                  // 1800ce16d
                if (_rowFirstItem != _itemsCount)
                    FlushRow(this);                            // 1800ce187..1800ce1b6
                text = *textSlot;                              // 1800ce1bc  re-read
                pos  = _textPos;
                const char m = text[pos];
                if (sel == 'j') {
                    if      (m == 'l') _hAlign = 0;   // 1800ce1cc
                    else if (m == 'r') _hAlign = 1;   // 1800ce1da
                    else if (m == 'c') _hAlign = 2;   // 1800ce1ec
                } else {
                    if      (m == 't') _vAlign = 0;   // 1800ce276
                    else if (m == 'c') _vAlign = 1;   // 1800ce284
                    else if (m == 'b') _vAlign = 2;   // 1800ce296
                }
                _textPos = pos + 1;              // 1800ce1f9 / 1800ce2a3
            } else if (sel == 'm') {                           // 1800ce2ae
                ++pos;
                _textPos = pos;                  // 1800ce2bb
                if (pos == len) return false;                  // 1800ce2c1
                if (_rowFirstItem != _itemsCount)
                    FlushRow(this);                            // 1800ce2db..1800ce30a
                text = *textSlot;                              // 1800ce310  re-read
                pos  = _textPos;
                const char m = text[pos];

                if (m == 'b') {                                // 1800ce31a  column begin
                    _textPos = pos + 1;          // 1800ce322
                    const int32_t y = _penY;
                    _columnTop  = y;             // 1800ce32e
                    _columnMaxY = y;             // 1800ce334
                } else if (m == 'c') {                         // 1800ce33f  next column
                    _textPos = pos + 1;          // 1800ce347
                    const int32_t y = _penY;
                    if (y > _columnMaxY)         // 1800ce353
                        _columnMaxY = y;         // 1800ce35b
                    _rowPenX = _penX;   // 1800ce367
                    _penY = _columnTop; // 1800ce373
                } else if (m == 'e') {                         // 1800ce37e  columns end
                    _textPos = pos + 1;          // 1800ce386
                    const int32_t y = _penY;
                    if (y > _columnMaxY)         // 1800ce392
                        _columnMaxY = y;         // 1800ce39a
                    _penY = _columnMaxY; // 1800ce3a6
                    _rowPenX = _penX;   // 1800ce3b2
                } else if (m == 'l' || m == 'r') {             // 1800ce3bd / 1800ce453
                    // ^ml<int> / ^mr<int>: a decimal run up to the next
                    // whitespace. A MISSING run (the delimiter is already
                    // here) abandons the escape without touching anything.
                    const int32_t first = pos + 1;
                    _textPos = first;            // 1800ce3ca
                    const int32_t ws = FindNextWhiteSpace();
                    if (ws <= first) break;                    // 1800ce3dc

                    char* number = nullptr;
                    SubString(textSlot, &number, first, ws - 1);   // 1800ce3f5
                    _textPos = ws;               // 1800ce3fb
                    int32_t v = ToInteger(number);             // 1800ce406
                    if (m == 'l') {
                        if (v < 0) v = 0;                      // 1800ce412  CMOVS
                        _penX = v;               // 1800ce415
                    } else {
                        if (v < 0) v = _wrapWidth;   // 1800ce4a5
                        _rightEdge = v;          // 1800ce4ab
                    }
                    ReleaseString(&number);
                }
                // any other selector: fall through and keep parsing
            } else if (sel == 'l') {                           // 1800ce4e7
                ++pos;
                _textPos = pos;                  // 1800ce4f3
                if (pos == len) return false;                  // 1800ce4f9
                const char m = text[pos];                      // 1800ce505

                if (m == 'b' || m == 'e') {                    // indent / outdent
                    if (_rowFirstItem != _itemsCount)
                        FlushRow(this);                        // 1800ce51c..1800ce54b
                    const int32_t step = _tabWidth;
                    if (m == 'b') _penX += step; // 1800ce557
                    else          _penX -= step; // 1800ce5ba
                    ++_textPos;                  // 1800ce55d / 1800ce5c0
                } else if (m == 'i') {                         // 1800ce5cb -> 1800cec38
                    // ^li — the list bullet: the sprite the frame table holds
                    // for the CURRENT FONT SLOT. It is emitted as a HANGING
                    // item: its width is recorded as the hang indent and
                    // SUBTRACTED from the pen, so it sits left of the margin.
                    if (_rowFirstItem != _itemsCount)
                        FlushRow(this);                        // 1800cec49..1800cec78
                    ++_textPos;                  // 1800cec7e

                    const uint8_t* entry =
                        B(_frameTableData, 0)
                        + static_cast<size_t>(_fontIndex) * 0x10;  // 1800cec84
                    const int32_t sprite = I32(entry, 0x08);

                    int32_t w = 0, h = 0;
                    if (sprite >= 0) {                         // 1800cec9a / 1800cecba
                        const uint8_t* data = static_cast<const uint8_t*>(
                            PTR(PTR(entry, 0), O::kSheetSpriteData));
                        const uint8_t* s = data + static_cast<size_t>(sprite) * O::kSpriteStride;
                        w = I32(s, O::kSpriteWidth);           // 1800cecb1
                        h = I32(s, O::kSpriteHeight);          // 1800cecd0
                    }

                    _hangIndent = w;             // 1800cecd4
                    _penX -= w;                  // 1800cecdb

                    out->color  = reinterpret_cast<const int32_t*>(_colorsData)
                                      [_colorIndex];   // 1800cecf7
                    out->height = h;                           // 1800cecfb
                    out->width  = w;                           // 1800cecff
                    out->index  = 0;                           // 1800ced03
                    out->kind   = 1;                           // 1800ced07
                    // The sprite index is re-read from the table rather than
                    // reused from `sprite` above (0x1800ced0c).
                    out->first  = *reinterpret_cast<const int32_t*>(
                        B(_frameTableData, 0)
                        + static_cast<size_t>(_fontIndex) * 0x10 + 8);
                    return true;                               // 1800ced25
                } else if (m == 'o') {
                    // ========================================================
                    // ^lo — LOOK AHEAD. Parse the next item with the whole
                    // state saved, throw the item away, keep only its width as
                    // a hanging indent. This is the recursion the banner warns
                    // about; the saved state includes an OWNING reference on
                    // the open link string, because the nested parse may
                    // legitimately open and close links of its own.
                    // ========================================================
                    if (_rowFirstItem != _itemsCount)
                        FlushRow(this);                        // 1800ce5f0..1800ce61f

                    void* font = reinterpret_cast<void* const*>(_fontsData)
                                     [_fontIndex];
                    // A QUARTER of the line height, truncating toward zero:
                    // `CDQ ; AND EDX,3 ; ADD ; SAR 2`, which is C's `/ 4`.
                    _penY += I32(font, O::kFontLineHeight) / 4;  // 1800ce643

                    const int32_t savedY     = _penY;            // 1800ce649
                    ++_textPos;                                  // 1800ce652
                    const int32_t savedPos   = _textPos;         // 1800ce658
                    const int32_t savedPenX  = _penX;
                    const int32_t savedHang  = _hangIndent;
                    const int32_t savedRight = _rightEdge;
                    const int32_t saved184   = _unknown184;
                    const int32_t savedRowX  = _rowPenX;
                    const int32_t savedRow   = _rowFirstItem;

                    char* savedLink = nullptr;                                 // 1800ce694
                    if (_openLink.text != nullptr) {                  // 1800ce69f
                        savedLink = _openLink.text;
                        ++RefCount(savedLink);                                 // 1800ce6ab
                    }
                    const int32_t savedLinkFirst = _openLink.firstItem;
                    const int32_t savedLinkLast  = _openLink.lastItem;
                    const uint8_t savedLinkByte  = _openLink.armed;
                    const int32_t savedFont      = _fontIndex;
                    const int32_t savedColor     = _colorIndex;
                    const int32_t savedHAlign    = _hAlign;
                    const int32_t savedVAlign    = _vAlign;
                    const int32_t savedColTop    = _columnTop;

                    DocumentItem probe;
                    const bool got =
                        GetNextItem(probe);    // 1800ce707

                    _textPos       = savedPos;                   // 1800ce712
                    _penX          = savedPenX;
                    _hangIndent    = savedHang;
                    _rightEdge     = savedRight;
                    _unknown184        = saved184;
                    _rowPenX       = savedRowX;
                    _penY          = savedY;
                    _rowFirstItem  = savedRow;
                    AssignString(&_openLink.text, savedLink);         // 1800ce757
                    _openLink.firstItem = savedLinkFirst;             // 1800ce7a5
                    _openLink.lastItem  = savedLinkLast;
                    _openLink.armed   = savedLinkByte;
                    _fontIndex     = savedFont;
                    _colorIndex    = savedColor;
                    _hAlign        = savedHAlign;
                    _vAlign        = savedVAlign;
                    _columnTop     = savedColTop;

                    if (got) {                                                 // 1800ce7ee
                        _hangIndent = probe.width;               // 1800ce7f5
                        _penX -= probe.width;                    // 1800ce7fb
                    }
                    ReleaseString(&savedLink);                                 // 1800ce801..
                }
                // any other selector: fall through and keep parsing
            } else if (sel == 'a') {                            // 1800ce82a
                ++pos;
                _textPos = pos;                   // 1800ce836
                if (pos == len) return false;                   // 1800ce83c
                const char m = text[pos];                       // 1800ce848

                if (m == 'b') {                                 // link begin
                    const int32_t first = pos + 1;
                    _textPos = first;             // 1800ce851
                    const int32_t ws = FindNextWhiteSpace();
                    if (ws <= first) break;                     // 1800ce863

                    char* target = nullptr;
                    SubString(textSlot, &target, first, ws - 1);    // 1800ce87c
                    _textPos = ws;                // 1800ce882
                    BeginLink(*reinterpret_cast<const System::String*>(&target));   // B.3.6 bridge  // 1800ce890
                    ReleaseString(&target);
                } else if (m == 'e') {                          // 1800ce8cc  link end
                    EndLink();     // 1800ce8d9
                    ++_textPos;                   // 1800ce8de
                }
                // any other selector: fall through and keep parsing
            } else if (sel == 'd') {
                // ================================================================
                // ^d<name> — a UI::SpriteCapture, found by walking the element's
                // resource list and comparing each entry's PATH FILENAME (with
                // its extension stripped) against <name>, case-insensitively.
                // The name goes through Filename::AdjustDelimiters first, so a
                // document may spell it with either slash.
                //
                // The length is compared BEFORE _stricmp, so a same-length
                // mismatch is the only case that pays for the string compare.
                // ================================================================
                const int32_t first = pos + 1;
                _textPos = first;                 // 1800ce8f6
                if (first == len) return false;                 // 1800ce8fc
                const int32_t ws = FindNextWhiteSpace();
                if (ws <= first) break;                         // 1800ce911

                char* sub = nullptr;
                SubString(textSlot, &sub, first, ws - 1);       // 1800ce92a

                char* wanted = nullptr;                         // 1800ce932
                if (sub != nullptr) {                           // 1800ce93a
                    wanted = sub;
                    ++RefCount(sub);                            // 1800ce944
                }
                System::FilenamePrivateAccess::AdjustDelimiters(Path(&wanted));   // 1800ce94c
                ReleaseString(&sub);                            // 1800ce952..1800ce97f
                _textPos = ws;                    // 1800ce97f

                int32_t index = 0;                              // 1800ce985
                void*   found = nullptr;
                bool    match = false;
                const void* list = _resourceList;
                if (I32(list, 0x28) > 0) {                      // 1800ce98e
                    size_t off = 0;                             // 1800ce99a
                    for (;;) {
                        // The list pointer is RE-READ every iteration.
                        void* obj = *reinterpret_cast<void* const*>(
                            B(PTR(_resourceList, 0x20), 0) + off);   // 1800ce9ab

                        const bool isCapture = reinterpret_cast<const Object::Base*>(obj)->IsDerivedFrom(UI::SpriteCapture::TypeId());        // 1800ce9b8
                        // Retail NULLS the pointer when the type does not match
                        // and then calls GetPath on it anyway (0x1800ce9ce
                        // takes RCX unchanged). Not a guard we get to add: the
                        // list only ever holds captures in practice, and a null
                        // here faults in retail exactly as it would here.
                        found = isCapture ? obj : nullptr;      // 1800ce9c1  CMOVNZ

                        char* path = nullptr;
                        new (static_cast<void*>(&path)) System::Filename(       // 1800ce9ce
                            static_cast<const Object::External*>(found)->GetPath());
                        char* base = nullptr;
                        new (static_cast<void*>(&base)) System::Filename(       // 1800ce9de
                            Path(&path).GetPathFilename(true));

                        const int32_t baseLen   = base   ? Length(base)   : 0; // 1800ce9e7
                        const int32_t wantedLen = wanted ? Length(wanted) : 0; // 1800ce9f9
                        if (baseLen != wantedLen) {             // 1800cea05
                            match = false;                      // 1800cea09
                        } else {
                            match = rtcrt::StrICmp(base, wanted) == 0;   // 0x18007eefc
                        }

                        ReleaseString(&base);                   // 1800cea1a..1800cea47
                        ReleaseString(&path);                   // 1800cea47..1800cea78

                        if (match) break;                       // 1800cea78
                        ++index;                                // 1800cea7c
                        off += 8;                               // 1800cea82
                        if (index >= I32(_resourceList, 0x28)) break;  // 1800cea91
                    }
                }

                if (match) {                                    // 1800cea9c (index >= 0 always)
                    out->color  = reinterpret_cast<const int32_t*>(_colorsData)
                                      [_colorIndex];   // 1800ced3f
                    out->height = I32(found, O::kCaptureHeight);     // 1800ced49
                    out->width  = I32(found, O::kCaptureWidth);      // 1800ced4f
                    out->index  = index;                             // 1800ced52
                    out->kind   = 2;                                 // 1800ced55
                    ReleaseString(&wanted);                          // 1800ced59..
                    return true;
                }
                ReleaseString(&wanted);                         // 1800ceaa6..
            } else if (sel == 'i') {
                // ================================================================
                // ^i<digit><name> — sprite <name> out of sheet slot <digit>. A
                // non-digit, an empty name, a null sheet or an unknown sprite
                // name all abandon the escape silently and keep parsing.
                // ================================================================
                ++pos;
                _textPos = pos;                   // 1800ceaeb
                if (pos == len) return false;                   // 1800ceaf1
                const char d = text[pos];                       // 1800ceafd
                if (d < '0' || d > '9') break;                  // 1800ceb01 / 1800ceb0b
                const int32_t slot = d - '0';                   // 1800ceb15

                const int32_t first = pos + 1;
                _textPos = first;                 // 1800ceb1b
                if (first == len) return false;                 // 1800ceb21
                const int32_t ws = FindNextWhiteSpace();
                if (ws <= first) break;                         // 1800ceb35

                char* name = nullptr;
                SubString(textSlot, &name, first, ws - 1);      // 1800ceb4c
                _textPos = ws;                    // 1800ceb55

                void* sheet = reinterpret_cast<void* const*>(_sheetsData)[slot];
                int32_t sprite = -1;
                if (sheet != nullptr) {                         // 1800ceb69
                    sprite = static_cast<const UI::SpriteSheet*>(sheet)->GetSpriteIndexFromName(Str(&name));
                }
                if (sheet != nullptr && sprite >= 0) {          // 1800ceb80
                    const uint8_t* data = static_cast<const uint8_t*>(
                        PTR(sheet, O::kSheetSpriteData));       // 1800cedb6
                    const uint8_t* s = data + static_cast<size_t>(sprite) * O::kSpriteStride;
                    out->color  = reinterpret_cast<const int32_t*>(_colorsData)
                                      [_colorIndex];   // 1800cedab
                    out->height = I32(s, O::kSpriteHeight);     // 1800cedbe
                    out->width  = I32(s, O::kSpriteWidth);      // 1800cedca
                    out->index  = slot;                         // 1800cedce
                    out->kind   = 1;                            // 1800cedd2
                    out->first  = sprite;                       // 1800cedd7
                    ReleaseString(&name);                       // 1800ceddb..
                    return true;
                }
                ReleaseString(&name);                           // 1800ceb86..
            }
            // else: an unknown selector. The cursor already stepped past the
            // '^', so the selector byte itself becomes part of the next word.
        } while (false);

        // ---- back to the top: whitespace, then end-of-text -----------------
        len = _textLength;                        // 1800cebc0
        if (_textPos < len) {                     // 1800cebc7
            const char* text = *textSlot;                       // 1800cebd0
            for (;;) {
                const int32_t p = _textPos;       // 1800cebe0
                if (!IsWhitespace(static_cast<uint8_t>(text[p]))) break;   // 1800cebef
                _textPos = p + 1;                 // 1800cec01
                if (p + 1 >= len) break;                        // 1800cec07
            }
        }
        if (_textPos >= len) return false;        // 1800cec1a
    }
}

// ============================================================================
// ?GenerateItems@DocumentTextElement@UI@@AEAAXXZ   retail 0x1800cf060
// ============================================================================
// Rebuilds the whole item list from the element's text and publishes the
// resulting height back to UI::Element (+0x64).
//
// The UI::String at +0xc0 is a {table, index} reference. When BOTH halves are
// live — a non-null table AND a non-negative index — its resolved text replaces
// the flat System::String at +0x168; otherwise +0x168 is left as whatever
// SetText(System::String&) put there. That is the whole reason the class keeps
// two string fields (runtime/types/ui/DocumentTextElement.h's original note).
//
// THE SUBSTRING INSIDE THE LOOP IS DEAD AND IS KEPT. At 0x1800cf21a retail cuts
// the item's text out of the document and releases it one instruction block
// later without reading it. It allocates and frees; removing it would be a
// divergence in allocator traffic, and it is one call, so it stays.
void DocumentTextElement::GenerateItems() {

    if (*reinterpret_cast<void* const*>(&_text) != nullptr &&  // 1800cf087  table
        *reinterpret_cast<const int32_t*>(
            reinterpret_cast<const uint8_t*>(&_text) + 8) >= 0) {   // 1800cf090  index
        char* resolved = nullptr;
        new (static_cast<void*>(&resolved)) System::String(      // 0x180035020, retslot
            reinterpret_cast<const UI::String*>(&_text)->operator System::String());   // B.3.x bridge
        AssignString(&_sourceText, resolved);  // 1800cf0a4..1800cf0f7
        ReleaseString(&resolved);                               // 1800cf0fa..1800cf133
    }

    {
        const char* s = _sourceText;           // 1800cf133
        _textLength = s ? Length(s) : 0;          // 1800cf13f / 1800cf147
    }
    // ONE QWORD each: kTextPos+kPenX, kRowPenX+kPenY, kFontIndex+kColorIndex,
    // and kVAlign+kColumnTop where kVAlign takes 2 and kColumnTop takes 0.
    Store64(&_textPos,   0);                        // 1800cf14d
    _rightEdge = _wrapWidth;        // 1800cf15a
    Store64(&_rowPenX,   0);                        // 1800cf160
    _rowFirstItem = 0;                            // 1800cf167
    _hangIndent   = 0;                            // 1800cf16e
    Store64(&_fontIndex, 0);                        // 1800cf175
    _hAlign = 0;                                  // 1800cf17c
    Store64(&_vAlign,    2);                        // 1800cf183

    DocumentItem item;
    while (GetNextItem(item)) { // 1800cf196 / 1800cf2af
        const int32_t w = item.width;                           // 1800cf1a9
        if (_rowPenX + w > _rightEdge)   // 1800cf1af
            FlushRow(this);                                     // 1800cf1ba..1800cf1ea
        _rowPenX += w;                            // 1800cf1f0

        rtcoll::RtColl_ArraySetSizeFill_18003df80(
            &_itemsData, _itemsCount + 1, &item);  // 1800cf20a

        if (item.kind == 0) {                                   // 1800cf20f
            char* dead = nullptr;
            SubString(&_sourceText, &dead, item.first, item.last);
            ReleaseString(&dead);                               // 1800cf238..1800cf2a7
        }
    }

    _penY += LayoutRow();   // 1800cf2c4
    const int32_t height = _penY;                 // 1800cf2ca
    _rowFirstItem = _itemsCount;     // 1800cf2d6
    _penX += _hangIndent;           // 1800cf2e2
    _hangIndent = 0;                              // 1800cf2e8
    _rowPenX = _penX;               // 1800cf2f5
    _layoutHeight = height;                      // 1800cf2fb
}


// ============================================================================
// UI::DocumentTextElement::Link::operator=   0x18003c760   (batch D1)
// ============================================================================
// The compiler-generated copy assignment for the 0x18-byte Link record
// {System::String* @+0x00, int32 @+0x08, int32 @+0x0c, bool @+0x10}.
//
//   180013c76a  RAX = self->text
//   180013c773  if (RAX != src->text) {            <-- the self-assign test
//   180013c77d      release RAX through the SAME asymmetric arms ReleaseString
//                   implements (slot keeps text-8 when the count hits zero,
//                   slot is NULLED when it does not)
//   180013c79b      self->text = src->text; if (it) ++refcount
//               }
//   180013c7a9  the three trailing fields, ALWAYS — the early jump lands here,
//               not at the return
//   180013c7bc  return self
//
// The String half is `AssignString` verbatim (test, release, adopt), which is
// why it is called rather than re-inlined: the two must not drift.
DocumentTextElement::Link& DocumentTextElement::Link::operator=(const Link& src) {
    AssignString(&text, src.text);                             // 18003c76a..18003c7a8
    firstItem = src.firstItem;                                 // 18003c7a9
    lastItem  = src.lastItem;                                  // 18003c7af
    armed     = src.armed;                                     // 18003c7b5
    return *this;                                              // 18003c7bc  MOV RAX,RBX
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// delegation to the member (non-virtual, so no qualification is load-bearing;
// spelled qualified for uniformity).
// ============================================================================
using UI::DocumentTextElement;

extern "C" int32_t RtUi_DocumentTextElement_FindNextWhiteSpace(void* self) {
    return reinterpret_cast<DocumentTextElement*>(self)->DocumentTextElement::FindNextWhiteSpace();
}
extern "C" void RtUi_DocumentTextElement_EndLink(void* self) {
    reinterpret_cast<DocumentTextElement*>(self)->DocumentTextElement::EndLink();
}
extern "C" void RtUi_DocumentTextElement_BeginLink(void* self, char* const* name) {
    reinterpret_cast<DocumentTextElement*>(self)->DocumentTextElement::BeginLink(
        *reinterpret_cast<const System::String*>(name));   // B.3.6 bridge
}
extern "C" int32_t RtUi_DocumentTextElement_LayoutRow(void* self) {
    return reinterpret_cast<DocumentTextElement*>(self)->DocumentTextElement::LayoutRow();
}
extern "C" bool RtUi_DocumentTextElement_GetNextItem(void* self, DocumentItem* out) {
    return reinterpret_cast<DocumentTextElement*>(self)->DocumentTextElement::GetNextItem(*out);
}
extern "C" void RtUi_DocumentTextElement_GenerateItems(void* self) {
    reinterpret_cast<DocumentTextElement*>(self)->DocumentTextElement::GenerateItems();
}
extern "C" void* RtUi_DocumentTextElement_LinkAssign(void* dst, const void* src) {
    return &(reinterpret_cast<DocumentTextElement::Link*>(dst)->operator=(
        *reinterpret_cast<const DocumentTextElement::Link*>(src)));
}
