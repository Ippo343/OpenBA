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

#include "../../funcs/rt_port.h"
#include "../../types/ui/DocumentTextElement.h"

// ResolveAddr, forward-declared rather than pulled in from standalone/resolve.h
// — resolve.h drags in <windows.h> and PLAN.md §4.1 keeps it out of
// runtime/src/. This declaration must stay byte-identical to resolve.h's; the
// linker checks the two.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace O = UI::DocumentTextElementOffsets;
using UI::DocumentItem;

namespace {

template <typename Fn>
Fn RtAddr(uintptr_t ghidra) { return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra)); }

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

using MemFreeFn = void (*)(void*);
void MemFree(void* p) {
    RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(p);
}
using MemAllocFn = void* (*)(uint64_t size, int align);
void* MemAlloc(uint64_t size, int align) {
    return RtAddr<MemAllocFn>(0x1800c7a50ull)(size, align);
}

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
// ?SubString@String@System@@QEBA?AV12@HH@Z — 0x1800c9230. The last argument is
// the INCLUSIVE end index, not a length: its body computes `end - start + 1`
// and reserves that many characters (0x1800c9262). `ret` is zeroed on entry, so
// an uninitialised local is what retail passes.
using SubStringFn = char** (*)(char* const* self, char** ret, int start, int endInclusive);
char** SubString(char* const* self, char** ret, int start, int endInclusive) {
    return RtAddr<SubStringFn>(0x1800c9230ull)(self, ret, start, endInclusive);
}
// ?ToInteger@String@System@@SAHPEBD@Z — 0x18003f620, a static taking the raw
// char*, not the String.
using ToIntegerFn = int32_t (*)(const char*);
int32_t ToInteger(const char* s) { return RtAddr<ToIntegerFn>(0x18003f620ull)(s); }

// ?AdjustDelimiters@Filename@System@@ — 0x1800c6970, in place on a String slot.
using AdjustDelimitersFn = void (*)(char** self);
// ?GetPath@External@Object@@ — 0x180006250, hidden return slot in RDX.
using GetPathFn = char** (*)(const void* self, char** ret);
// ?GetPathFilename@Filename@System@@QEBA?AV12@_N@Z — 0x1800c6740, hidden return
// slot in RDX, `stripExtension` in R8B.
using GetPathFilenameFn = char** (*)(char* const* self, char** ret, bool stripExtension);
// ?IsDerivedFrom@Base@Object@@ — 0x1800c0390.
using IsDerivedFromFn = bool (*)(const void* self, uint32_t typeId);
// The CRT _stricmp at 0x18007eefc. Ghidra prints this call with ONE argument
// because RDX was already loaded by the length comparison two instructions
// earlier (0x1800ce9f4) and is not re-loaded — the documented dropped-argument
// trap. It is two arguments. runtime/src/crt/msvc_stricmp.cpp already owns this
// address, so RtAddr reaches our port rather than the DLL.
using StrICmpFn = int (*)(const char* a, const char* b);
// ?ComputeLineLayout@Font@UI@@ — 0x180116c70, eight arguments (four register,
// four stack). Retail passes (font, text, length, 0, 0, -1, 0, nullptr).
using ComputeLineLayoutFn = int32_t (*)(void* font, const char* text, int32_t length,
                                        int32_t a3, int32_t a4, int32_t a5,
                                        int32_t a6, void* layout);
// ??HSystem@@YA?AVString@0@AEBV10@PEBD@Z — 0x1800c92d0, `String + const char*`
// with a hidden return slot in RCX.
using ConcatCStrFn = char** (*)(char** ret, char* const* lhs, const char* rhs);
// ?GetSpriteIndexFromName@SpriteSheet@UI@@QEBAHAEBVString@System@@@Z — 0x1800031f0
using GetSpriteIndexFn = int32_t (*)(const void* sheet, char* const* name);
// ?GetIndexFromName@StringTable@UI@@ — 0x180002470
using GetIndexFromNameFn = int32_t (*)(const void* table, char* const* name);
// Array<DocumentItem>::SetSize(n, fill) — 0x18003df80 (element 0x24).
using ItemArrayPushFn = void (*)(void* array, int32_t n, const DocumentItem* fill);
// Array<Link>::SetSize(n, fill) — 0x18003e1a0 (element 0x18).
using LinkArrayPushFn = void (*)(void* array, int32_t n, const void* fill);

// The whitespace set the parser recognises, read out of the 64-bit BT mask
// `0x100002600` that all three scan loops materialise: bits 9, 10, 13 and 32 —
// tab, LF, CR and space. Nothing else counts, not even vertical tab or form
// feed, which is why the mask is kept as a mask instead of being re-spelled as
// an isspace() call.
constexpr uint64_t kWhitespaceMask = 0x100002600ull;
inline bool IsWhitespace(uint8_t c) {
    return c <= 0x20 && ((kWhitespaceMask >> c) & 1u) != 0;
}

// UI::SpriteCapture's BaseTypeInfo static; its first dword is the type id the
// ^d search compares against. Retail reads the global at 0x1801fa3f8 directly,
// which is that static's id field (name pointer 0x1801567d0 == "SpriteCapture").
uint32_t SpriteCaptureTypeId() {
    const uint8_t* ti = RT_DATA_SLOT(const uint8_t*,
                                     "?_typeInfo@SpriteCapture@UI@@0VBaseTypeInfo@Object@@A");
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}

}  // namespace

// forward declarations — GetNextItem calls LayoutRow, FindNextWhiteSpace,
// BeginLink, EndLink and itself.
extern "C" int32_t RtUi_DocumentTextElement_LayoutRow(void* selfv);
extern "C" int32_t RtUi_DocumentTextElement_FindNextWhiteSpace(void* selfv);
extern "C" void    RtUi_DocumentTextElement_BeginLink(void* selfv, char* const* name);
extern "C" void    RtUi_DocumentTextElement_EndLink(void* selfv);
extern "C" bool    RtUi_DocumentTextElement_GetNextItem(void* selfv, DocumentItem* out);

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
extern "C" int32_t RtUi_DocumentTextElement_FindNextWhiteSpace(void* selfv) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);
    int32_t pos = I32(self, O::kTextPos);                      // 1800cefd0
    const int32_t len = I32(self, O::kTextLength);             // 1800cefd7
    if (pos >= len) return pos;                                // 1800cefe4

    const char* text = static_cast<const char*>(PTR(self, O::kContentString));  // 1800cefe6
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
extern "C" void RtUi_DocumentTextElement_EndLink(void* selfv) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);

    const int32_t first = I32(self, O::kLinkFirstItem);        // 1800cde12
    if (first < 0) return;                                     // 1800cde1a

    const int32_t items = I32(self, O::kItemCount);            // 1800cde1c
    I32(self, O::kLinkLastItem) = items;                       // 1800cde22
    if (first < items) {                                       // 1800cde28
        void* array = self + O::kLinkArrayData;
        RtAddr<LinkArrayPushFn>(0x18003e1a0ull)(
            array, I32(array, 0x08) + 1, self + O::kLinkText); // 1800cde3f
    }

    // The empty Link: {char* = 0, int = -1, int = -1, bool = false}. Retail
    // writes it as one qword zero, one qword -1 (covering BOTH indices) and one
    // zero byte, which is why the two indices cannot be initialised separately
    // here without changing the store widths for no gain.
    struct Link { char* text; int32_t first; int32_t last; uint8_t flag; uint8_t pad[7]; };
    static_assert(sizeof(Link) == 0x18, "UI::DocumentTextElement::Link is 0x18 bytes");
    Link empty;
    empty.text = nullptr;                                      // 1800cde44
    Store64(&empty.first, -1);                                 // 1800cde4d  ONE QWORD
    empty.flag = 0;                                            // 1800cde56

    using LinkAssignFn = void* (*)(void* dst, const void* src);
    RtAddr<LinkAssignFn>(0x18003c760ull)(self + O::kLinkText, &empty);   // 1800cde67

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
extern "C" void RtUi_DocumentTextElement_BeginLink(void* selfv, char* const* name) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);

    RtUi_DocumentTextElement_EndLink(self);                    // 1800cdec0

    AssignString(&STR(self, O::kLinkText), *name);             // 1800cdec5..1800cdf0f

    const int32_t items = I32(self, O::kItemCount);            // 1800cdf12
    I32(self, O::kLinkLastItem)  = -1;                         // 1800cdf18
    I32(self, O::kLinkFirstItem) = items;                      // 1800cdf22
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
extern "C" int32_t RtUi_DocumentTextElement_LayoutRow(void* selfv) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);

    int32_t i         = I32(self, O::kRowFirstItem);           // 1800cdbca
    int32_t rowWidth  = 0;                                     // 1800cdbd1
    int32_t rowHeight = 0;                                     // 1800cdbd7

    if (i == I32(self, O::kItemCount)) {                       // 1800cdbda
        void* font = reinterpret_cast<void* const*>(PTR(self, O::kFontArray))
                         [I32(self, O::kFontIndex)];
        rowHeight = I32(font, O::kFontLineHeight);             // 1800cdbf4
    }

    {
        const int32_t count = I32(self, O::kItemCount);        // 1800cdbf8
        DocumentItem* items = static_cast<DocumentItem*>(PTR(self, O::kItemArrayData));
        for (int32_t k = i; k < count; ++k) {                  // 1800cdbff / 1800cdc32
            rowWidth += items[k].width;                        // 1800cdc20
            if (items[k].height > rowHeight)                   // 1800cdc24
                rowHeight = items[k].height;                   // 1800cdc27  CMOVG
        }
    }

    int32_t penX;
    const int32_t hAlign = I32(self, O::kHAlign);              // 1800cdc34
    if (hAlign == 0) {
        penX = I32(self, O::kPenX);                            // 1800cdc3e
    } else if (hAlign == 1) {
        penX = I32(self, O::kRightEdge) - rowWidth;            // 1800cdc4b
    } else {
        // (right + left)/2 - rowWidth/2, both truncating toward zero.
        penX = (I32(self, O::kRightEdge) + I32(self, O::kPenX)) / 2
             - rowWidth / 2;                                   // 1800cdc56..1800cdc71
    }

    while (i < I32(self, O::kItemCount)) {                     // 1800cdc73 / 1800cdd85
        const size_t off = static_cast<size_t>(i) * O::kItemStride;

        DocumentItem* item =
            reinterpret_cast<DocumentItem*>(B(PTR(self, O::kItemArrayData), 0) + off);
        item->x = penX;                                        // 1800cdc97

        // The array pointer is loaded a SECOND time here (0x1800cdc9b) before
        // the width is read. Same value today; transcribed, not folded.
        item = reinterpret_cast<DocumentItem*>(B(PTR(self, O::kItemArrayData), 0) + off);
        penX += item->width;                                   // 1800cdca9

        const int32_t vAlign = I32(self, O::kVAlign);          // 1800cdca2
        const int32_t penY   = I32(self, O::kPenY);
        if (vAlign == 2) {                                     // 1800cdcae  baseline
            if (item->kind == 0) {                             // 1800cdcb3
                void* font = reinterpret_cast<void* const*>(PTR(self, O::kFontArray))
                                 [item->index];
                item->y = penY - I32(font, O::kFontBaseline) + rowHeight;   // 1800cdcd8
            } else {
                item->y = penY - item->height + rowHeight;     // 1800cdcf1
            }
        } else if (vAlign == 0) {                              // 1800cdcfb  top
            if (item->kind == 0) {                             // 1800cdcff
                void* font = reinterpret_cast<void* const*>(PTR(self, O::kFontArray))
                                 [item->index];
                item->y = I32(font, O::kFontLineHeight)
                        - I32(font, O::kFontBaseline) + penY;  // 1800cdd25
            } else {
                item->y = penY;                                // 1800cdd33
            }
        } else {                                               // centre
            int32_t dy;
            if (item->kind == 0) {                             // 1800cdd3a
                void* font = reinterpret_cast<void* const*>(PTR(self, O::kFontArray))
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
void FlushRow(uint8_t* self) {
    I32(self, O::kPenY) += RtUi_DocumentTextElement_LayoutRow(self);
    I32(self, O::kRowFirstItem) = I32(self, O::kItemCount);
    I32(self, O::kPenX) += I32(self, O::kHangIndent);
    I32(self, O::kHangIndent) = 0;
    I32(self, O::kRowPenX) = I32(self, O::kPenX);
}
}  // namespace

// ============================================================================
// ?GetNextItem@DocumentTextElement@UI@@AEAA_NAEAUDocumentItem@12@@Z  0x1800cdf40
// ============================================================================
// Returns false at end of text, otherwise fills `out` with one item. Escapes
// that only mutate state do not produce an item: they loop back and keep
// parsing, which is what the `continue`s below are.
extern "C" bool RtUi_DocumentTextElement_GetNextItem(void* selfv, DocumentItem* out) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);
    char** const textSlot = &STR(self, O::kContentString);

    int32_t len = I32(self, O::kTextLength);                   // 1800cdf60

    // ---- leading whitespace, skipped before anything else ------------------
    if (I32(self, O::kTextPos) < len) {                        // 1800cdf71
        const char* text = *textSlot;                          // 1800cdf7a
        for (;;) {
            const int32_t pos = I32(self, O::kTextPos);        // 1800cdf81
            if (!IsWhitespace(static_cast<uint8_t>(text[pos]))) break;   // 1800cdf90
            I32(self, O::kTextPos) = pos + 1;                  // 1800cdfa2
            if (pos + 1 >= len) break;                         // 1800cdfa8
        }
    }
    if (I32(self, O::kTextPos) >= len) return false;           // 1800cdfb4 / 1800cdfb9

    for (;;) {
        // ---- one token: either an escape or a word -------------------------
        if ((*textSlot)[I32(self, O::kTextPos)] != 0x5e) {     // 1800cdfcf
            // ================================================================
            // A WORD. Everything up to the next whitespace or '^'.
            // ================================================================
            const int32_t start = I32(self, O::kTextPos);      // 1800cee57
            const int32_t textLen = I32(self, O::kTextLength); // 1800cee61
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
            RtAddr<ConcatCStrFn>(0x1800c92d0ull)(&measured, &word, " ");   // 1800ceeb7
            ReleaseString(&word);                              // 1800ceebd..1800ceeea

            void* font = reinterpret_cast<void* const*>(PTR(self, O::kFontArray))
                             [I32(self, O::kFontIndex)];       // 1800ceeea

            Store64(&out->width, 0);                           // 1800cef02  ONE QWORD

            const int32_t measuredLen = measured ? Length(measured) : 0;   // 1800cef0b
            out->width = RtAddr<ComputeLineLayoutFn>(0x180116c70ull)(
                font, measured, measuredLen, 0, 0, -1, 0, nullptr);        // 1800cef34
            out->height = I32(font, O::kFontLineHeight);       // 1800cef40
            out->first  = I32(self, O::kTextPos);              // 1800cef46
            out->last   = end - 1;                             // 1800cef4f
            out->color  = reinterpret_cast<const int32_t*>(PTR(self, O::kColorArray))
                              [I32(self, O::kColorIndex)];     // 1800cef63
            out->index  = I32(self, O::kFontIndex);            // 1800cef6f
            Store16(&out->kind, 0);                            // 1800cef72  ONE WORD

            I32(self, O::kTextPos) = end;                      // 1800cef78
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
            int32_t pos = I32(self, O::kTextPos) + 1;          // 1800cdfe0
            I32(self, O::kTextPos) = pos;
            if (pos == len) return false;                      // 1800cdfec
            char* text = *textSlot;                            // 1800cdfee
            const char sel = text[pos];                        // 1800cdff4

            if (sel == 'n') {                                  // 1800cdff4
                I32(self, O::kTextPos) = pos + 1;              // 1800cdffd
                FlushRow(self);                                // 1800ce006..1800ce035
            } else if (sel == 'p') {                           // 1800ce040
                I32(self, O::kTextPos) = pos + 1;              // 1800ce049
                FlushRow(self);                                // 1800ce052..1800ce081
                FlushRow(self);                                // 1800ce08a..1800ce0b9
            } else if (sel == 't') {                           // 1800ce0c4 -> 1800cee2b
                out->kind   = 3;                               // 1800cee2f
                out->width  = I32(self, O::kTabWidth);         // 1800cee3a
                out->height = 0;                               // 1800cee3e
                ++I32(self, O::kTextPos);                      // 1800cee42
                return true;
            } else if (sel == 's') {                           // 1800ce0ce -> 1800cee18
                out->kind = 3;                                 // 1800cee1c
                ++I32(self, O::kTextPos);                      // 1800cee20
                return true;
            } else if (sel == 'f' || sel == 'c') {             // 1800ce0d8 / 1800ce119
                // ^f<digit> font slot, ^c<digit> colour slot. A non-digit is
                // SWALLOWED, not rejected: the cursor still steps past it.
                const int field = (sel == 'f') ? O::kFontIndex : O::kColorIndex;
                ++pos;
                I32(self, O::kTextPos) = pos;                  // 1800ce0e0
                if (pos == len) return false;                  // 1800ce0e6
                const char d = text[pos];                      // 1800ce0f2
                if (d >= '0' && d <= '9')                      // 1800ce0f6 / 1800ce0fc
                    I32(self, field) = d - '0';                // 1800ce105
                I32(self, O::kTextPos) = pos + 1;              // 1800ce10e
            } else if (sel == 'j' || sel == 'v') {             // 1800ce15a / 1800ce204
                // ^j{l,r,c} horizontal, ^v{t,c,b} vertical. Both close the
                // current row FIRST if it has anything in it, because an
                // alignment change cannot apply retroactively.
                ++pos;
                I32(self, O::kTextPos) = pos;                  // 1800ce167
                if (pos == len) return false;                  // 1800ce16d
                if (I32(self, O::kRowFirstItem) != I32(self, O::kItemCount))
                    FlushRow(self);                            // 1800ce187..1800ce1b6
                text = *textSlot;                              // 1800ce1bc  re-read
                pos  = I32(self, O::kTextPos);
                const char m = text[pos];
                if (sel == 'j') {
                    if      (m == 'l') I32(self, O::kHAlign) = 0;   // 1800ce1cc
                    else if (m == 'r') I32(self, O::kHAlign) = 1;   // 1800ce1da
                    else if (m == 'c') I32(self, O::kHAlign) = 2;   // 1800ce1ec
                } else {
                    if      (m == 't') I32(self, O::kVAlign) = 0;   // 1800ce276
                    else if (m == 'c') I32(self, O::kVAlign) = 1;   // 1800ce284
                    else if (m == 'b') I32(self, O::kVAlign) = 2;   // 1800ce296
                }
                I32(self, O::kTextPos) = pos + 1;              // 1800ce1f9 / 1800ce2a3
            } else if (sel == 'm') {                           // 1800ce2ae
                ++pos;
                I32(self, O::kTextPos) = pos;                  // 1800ce2bb
                if (pos == len) return false;                  // 1800ce2c1
                if (I32(self, O::kRowFirstItem) != I32(self, O::kItemCount))
                    FlushRow(self);                            // 1800ce2db..1800ce30a
                text = *textSlot;                              // 1800ce310  re-read
                pos  = I32(self, O::kTextPos);
                const char m = text[pos];

                if (m == 'b') {                                // 1800ce31a  column begin
                    I32(self, O::kTextPos) = pos + 1;          // 1800ce322
                    const int32_t y = I32(self, O::kPenY);
                    I32(self, O::kColumnTop)  = y;             // 1800ce32e
                    I32(self, O::kColumnMaxY) = y;             // 1800ce334
                } else if (m == 'c') {                         // 1800ce33f  next column
                    I32(self, O::kTextPos) = pos + 1;          // 1800ce347
                    const int32_t y = I32(self, O::kPenY);
                    if (y > I32(self, O::kColumnMaxY))         // 1800ce353
                        I32(self, O::kColumnMaxY) = y;         // 1800ce35b
                    I32(self, O::kRowPenX) = I32(self, O::kPenX);   // 1800ce367
                    I32(self, O::kPenY) = I32(self, O::kColumnTop); // 1800ce373
                } else if (m == 'e') {                         // 1800ce37e  columns end
                    I32(self, O::kTextPos) = pos + 1;          // 1800ce386
                    const int32_t y = I32(self, O::kPenY);
                    if (y > I32(self, O::kColumnMaxY))         // 1800ce392
                        I32(self, O::kColumnMaxY) = y;         // 1800ce39a
                    I32(self, O::kPenY) = I32(self, O::kColumnMaxY); // 1800ce3a6
                    I32(self, O::kRowPenX) = I32(self, O::kPenX);   // 1800ce3b2
                } else if (m == 'l' || m == 'r') {             // 1800ce3bd / 1800ce453
                    // ^ml<int> / ^mr<int>: a decimal run up to the next
                    // whitespace. A MISSING run (the delimiter is already
                    // here) abandons the escape without touching anything.
                    const int32_t first = pos + 1;
                    I32(self, O::kTextPos) = first;            // 1800ce3ca
                    const int32_t ws = RtUi_DocumentTextElement_FindNextWhiteSpace(self);
                    if (ws <= first) break;                    // 1800ce3dc

                    char* number = nullptr;
                    SubString(textSlot, &number, first, ws - 1);   // 1800ce3f5
                    I32(self, O::kTextPos) = ws;               // 1800ce3fb
                    int32_t v = ToInteger(number);             // 1800ce406
                    if (m == 'l') {
                        if (v < 0) v = 0;                      // 1800ce412  CMOVS
                        I32(self, O::kPenX) = v;               // 1800ce415
                    } else {
                        if (v < 0) v = I32(self, O::kWrapWidth);   // 1800ce4a5
                        I32(self, O::kRightEdge) = v;          // 1800ce4ab
                    }
                    ReleaseString(&number);
                }
                // any other selector: fall through and keep parsing
            } else if (sel == 'l') {                           // 1800ce4e7
                ++pos;
                I32(self, O::kTextPos) = pos;                  // 1800ce4f3
                if (pos == len) return false;                  // 1800ce4f9
                const char m = text[pos];                      // 1800ce505

                if (m == 'b' || m == 'e') {                    // indent / outdent
                    if (I32(self, O::kRowFirstItem) != I32(self, O::kItemCount))
                        FlushRow(self);                        // 1800ce51c..1800ce54b
                    const int32_t step = I32(self, O::kTabWidth);
                    if (m == 'b') I32(self, O::kPenX) += step; // 1800ce557
                    else          I32(self, O::kPenX) -= step; // 1800ce5ba
                    ++I32(self, O::kTextPos);                  // 1800ce55d / 1800ce5c0
                } else if (m == 'i') {                         // 1800ce5cb -> 1800cec38
                    // ^li — the list bullet: the sprite the frame table holds
                    // for the CURRENT FONT SLOT. It is emitted as a HANGING
                    // item: its width is recorded as the hang indent and
                    // SUBTRACTED from the pen, so it sits left of the margin.
                    if (I32(self, O::kRowFirstItem) != I32(self, O::kItemCount))
                        FlushRow(self);                        // 1800cec49..1800cec78
                    ++I32(self, O::kTextPos);                  // 1800cec7e

                    const uint8_t* entry =
                        B(PTR(self, O::kFrameTable), 0)
                        + static_cast<size_t>(I32(self, O::kFontIndex)) * 0x10;  // 1800cec84
                    const int32_t sprite = I32(entry, 0x08);

                    int32_t w = 0, h = 0;
                    if (sprite >= 0) {                         // 1800cec9a / 1800cecba
                        const uint8_t* data = static_cast<const uint8_t*>(
                            PTR(PTR(entry, 0), O::kSheetSpriteData));
                        const uint8_t* s = data + static_cast<size_t>(sprite) * O::kSpriteStride;
                        w = I32(s, O::kSpriteWidth);           // 1800cecb1
                        h = I32(s, O::kSpriteHeight);          // 1800cecd0
                    }

                    I32(self, O::kHangIndent) = w;             // 1800cecd4
                    I32(self, O::kPenX) -= w;                  // 1800cecdb

                    out->color  = reinterpret_cast<const int32_t*>(PTR(self, O::kColorArray))
                                      [I32(self, O::kColorIndex)];   // 1800cecf7
                    out->height = h;                           // 1800cecfb
                    out->width  = w;                           // 1800cecff
                    out->index  = 0;                           // 1800ced03
                    out->kind   = 1;                           // 1800ced07
                    // The sprite index is re-read from the table rather than
                    // reused from `sprite` above (0x1800ced0c).
                    out->first  = *reinterpret_cast<const int32_t*>(
                        B(PTR(self, O::kFrameTable), 0)
                        + static_cast<size_t>(I32(self, O::kFontIndex)) * 0x10 + 8);
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
                    if (I32(self, O::kRowFirstItem) != I32(self, O::kItemCount))
                        FlushRow(self);                        // 1800ce5f0..1800ce61f

                    void* font = reinterpret_cast<void* const*>(PTR(self, O::kFontArray))
                                     [I32(self, O::kFontIndex)];
                    // A QUARTER of the line height, truncating toward zero:
                    // `CDQ ; AND EDX,3 ; ADD ; SAR 2`, which is C's `/ 4`.
                    I32(self, O::kPenY) += I32(font, O::kFontLineHeight) / 4;  // 1800ce643

                    const int32_t savedY     = I32(self, O::kPenY);            // 1800ce649
                    ++I32(self, O::kTextPos);                                  // 1800ce652
                    const int32_t savedPos   = I32(self, O::kTextPos);         // 1800ce658
                    const int32_t savedPenX  = I32(self, O::kPenX);
                    const int32_t savedHang  = I32(self, O::kHangIndent);
                    const int32_t savedRight = I32(self, O::kRightEdge);
                    const int32_t saved184   = I32(self, O::kInt184);
                    const int32_t savedRowX  = I32(self, O::kRowPenX);
                    const int32_t savedRow   = I32(self, O::kRowFirstItem);

                    char* savedLink = nullptr;                                 // 1800ce694
                    if (STR(self, O::kLinkText) != nullptr) {                  // 1800ce69f
                        savedLink = STR(self, O::kLinkText);
                        ++RefCount(savedLink);                                 // 1800ce6ab
                    }
                    const int32_t savedLinkFirst = I32(self, O::kLinkFirstItem);
                    const int32_t savedLinkLast  = I32(self, O::kLinkLastItem);
                    const uint8_t savedLinkByte  = U8 (self, O::kLinkByte1A8);
                    const int32_t savedFont      = I32(self, O::kFontIndex);
                    const int32_t savedColor     = I32(self, O::kColorIndex);
                    const int32_t savedHAlign    = I32(self, O::kHAlign);
                    const int32_t savedVAlign    = I32(self, O::kVAlign);
                    const int32_t savedColTop    = I32(self, O::kColumnTop);

                    DocumentItem probe;
                    const bool got =
                        RtUi_DocumentTextElement_GetNextItem(self, &probe);    // 1800ce707

                    I32(self, O::kTextPos)       = savedPos;                   // 1800ce712
                    I32(self, O::kPenX)          = savedPenX;
                    I32(self, O::kHangIndent)    = savedHang;
                    I32(self, O::kRightEdge)     = savedRight;
                    I32(self, O::kInt184)        = saved184;
                    I32(self, O::kRowPenX)       = savedRowX;
                    I32(self, O::kPenY)          = savedY;
                    I32(self, O::kRowFirstItem)  = savedRow;
                    AssignString(&STR(self, O::kLinkText), savedLink);         // 1800ce757
                    I32(self, O::kLinkFirstItem) = savedLinkFirst;             // 1800ce7a5
                    I32(self, O::kLinkLastItem)  = savedLinkLast;
                    U8 (self, O::kLinkByte1A8)   = savedLinkByte;
                    I32(self, O::kFontIndex)     = savedFont;
                    I32(self, O::kColorIndex)    = savedColor;
                    I32(self, O::kHAlign)        = savedHAlign;
                    I32(self, O::kVAlign)        = savedVAlign;
                    I32(self, O::kColumnTop)     = savedColTop;

                    if (got) {                                                 // 1800ce7ee
                        I32(self, O::kHangIndent) = probe.width;               // 1800ce7f5
                        I32(self, O::kPenX) -= probe.width;                    // 1800ce7fb
                    }
                    ReleaseString(&savedLink);                                 // 1800ce801..
                }
                // any other selector: fall through and keep parsing
            } else if (sel == 'a') {                            // 1800ce82a
                ++pos;
                I32(self, O::kTextPos) = pos;                   // 1800ce836
                if (pos == len) return false;                   // 1800ce83c
                const char m = text[pos];                       // 1800ce848

                if (m == 'b') {                                 // link begin
                    const int32_t first = pos + 1;
                    I32(self, O::kTextPos) = first;             // 1800ce851
                    const int32_t ws = RtUi_DocumentTextElement_FindNextWhiteSpace(self);
                    if (ws <= first) break;                     // 1800ce863

                    char* target = nullptr;
                    SubString(textSlot, &target, first, ws - 1);    // 1800ce87c
                    I32(self, O::kTextPos) = ws;                // 1800ce882
                    RtUi_DocumentTextElement_BeginLink(self, &target);  // 1800ce890
                    ReleaseString(&target);
                } else if (m == 'e') {                          // 1800ce8cc  link end
                    RtUi_DocumentTextElement_EndLink(self);     // 1800ce8d9
                    ++I32(self, O::kTextPos);                   // 1800ce8de
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
                I32(self, O::kTextPos) = first;                 // 1800ce8f6
                if (first == len) return false;                 // 1800ce8fc
                const int32_t ws = RtUi_DocumentTextElement_FindNextWhiteSpace(self);
                if (ws <= first) break;                         // 1800ce911

                char* sub = nullptr;
                SubString(textSlot, &sub, first, ws - 1);       // 1800ce92a

                char* wanted = nullptr;                         // 1800ce932
                if (sub != nullptr) {                           // 1800ce93a
                    wanted = sub;
                    ++RefCount(sub);                            // 1800ce944
                }
                RtAddr<AdjustDelimitersFn>(0x1800c6970ull)(&wanted);   // 1800ce94c
                ReleaseString(&sub);                            // 1800ce952..1800ce97f
                I32(self, O::kTextPos) = ws;                    // 1800ce97f

                int32_t index = 0;                              // 1800ce985
                void*   found = nullptr;
                bool    match = false;
                const void* list = PTR(self, O::kResourceList);
                if (I32(list, 0x28) > 0) {                      // 1800ce98e
                    size_t off = 0;                             // 1800ce99a
                    for (;;) {
                        // The list pointer is RE-READ every iteration.
                        void* obj = *reinterpret_cast<void* const*>(
                            B(PTR(PTR(self, O::kResourceList), 0x20), 0) + off);   // 1800ce9ab

                        const bool isCapture = RtAddr<IsDerivedFromFn>(0x1800c0390ull)(
                            obj, SpriteCaptureTypeId());        // 1800ce9b8
                        // Retail NULLS the pointer when the type does not match
                        // and then calls GetPath on it anyway (0x1800ce9ce
                        // takes RCX unchanged). Not a guard we get to add: the
                        // list only ever holds captures in practice, and a null
                        // here faults in retail exactly as it would here.
                        found = isCapture ? obj : nullptr;      // 1800ce9c1  CMOVNZ

                        char* path = nullptr;
                        RtAddr<GetPathFn>(0x180006250ull)(found, &path);       // 1800ce9ce
                        char* base = nullptr;
                        RtAddr<GetPathFilenameFn>(0x1800c6740ull)(&path, &base, true);  // 1800ce9de

                        const int32_t baseLen   = base   ? Length(base)   : 0; // 1800ce9e7
                        const int32_t wantedLen = wanted ? Length(wanted) : 0; // 1800ce9f9
                        if (baseLen != wantedLen) {             // 1800cea05
                            match = false;                      // 1800cea09
                        } else {
                            match = RtAddr<StrICmpFn>(0x18007eefcull)(base, wanted) == 0;
                        }

                        ReleaseString(&base);                   // 1800cea1a..1800cea47
                        ReleaseString(&path);                   // 1800cea47..1800cea78

                        if (match) break;                       // 1800cea78
                        ++index;                                // 1800cea7c
                        off += 8;                               // 1800cea82
                        if (index >= I32(PTR(self, O::kResourceList), 0x28)) break;  // 1800cea91
                    }
                }

                if (match) {                                    // 1800cea9c (index >= 0 always)
                    out->color  = reinterpret_cast<const int32_t*>(PTR(self, O::kColorArray))
                                      [I32(self, O::kColorIndex)];   // 1800ced3f
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
                I32(self, O::kTextPos) = pos;                   // 1800ceaeb
                if (pos == len) return false;                   // 1800ceaf1
                const char d = text[pos];                       // 1800ceafd
                if (d < '0' || d > '9') break;                  // 1800ceb01 / 1800ceb0b
                const int32_t slot = d - '0';                   // 1800ceb15

                const int32_t first = pos + 1;
                I32(self, O::kTextPos) = first;                 // 1800ceb1b
                if (first == len) return false;                 // 1800ceb21
                const int32_t ws = RtUi_DocumentTextElement_FindNextWhiteSpace(self);
                if (ws <= first) break;                         // 1800ceb35

                char* name = nullptr;
                SubString(textSlot, &name, first, ws - 1);      // 1800ceb4c
                I32(self, O::kTextPos) = ws;                    // 1800ceb55

                void* sheet = reinterpret_cast<void* const*>(PTR(self, O::kSheetArray))[slot];
                int32_t sprite = -1;
                if (sheet != nullptr) {                         // 1800ceb69
                    sprite = RtAddr<GetSpriteIndexFn>(0x1800031f0ull)(sheet, &name);
                }
                if (sheet != nullptr && sprite >= 0) {          // 1800ceb80
                    const uint8_t* data = static_cast<const uint8_t*>(
                        PTR(sheet, O::kSheetSpriteData));       // 1800cedb6
                    const uint8_t* s = data + static_cast<size_t>(sprite) * O::kSpriteStride;
                    out->color  = reinterpret_cast<const int32_t*>(PTR(self, O::kColorArray))
                                      [I32(self, O::kColorIndex)];   // 1800cedab
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
        len = I32(self, O::kTextLength);                        // 1800cebc0
        if (I32(self, O::kTextPos) < len) {                     // 1800cebc7
            const char* text = *textSlot;                       // 1800cebd0
            for (;;) {
                const int32_t p = I32(self, O::kTextPos);       // 1800cebe0
                if (!IsWhitespace(static_cast<uint8_t>(text[p]))) break;   // 1800cebef
                I32(self, O::kTextPos) = p + 1;                 // 1800cec01
                if (p + 1 >= len) break;                        // 1800cec07
            }
        }
        if (I32(self, O::kTextPos) >= len) return false;        // 1800cec1a
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
extern "C" void RtUi_DocumentTextElement_GenerateItems(void* selfv) {
    uint8_t* self = reinterpret_cast<uint8_t*>(selfv);

    if (PTR(self, O::kUiStringValue) != nullptr &&              // 1800cf087
        I32(self, O::kUiStringValue + 8) >= 0) {                // 1800cf090
        char* resolved = nullptr;
        using UiStringToStringFn = char** (*)(void* uiString, char** ret);
        RtAddr<UiStringToStringFn>(0x180035020ull)(self + O::kUiStringValue, &resolved);
        AssignString(&STR(self, O::kContentString), resolved);  // 1800cf0a4..1800cf0f7
        ReleaseString(&resolved);                               // 1800cf0fa..1800cf133
    }

    {
        const char* s = STR(self, O::kContentString);           // 1800cf133
        I32(self, O::kTextLength) = s ? Length(s) : 0;          // 1800cf13f / 1800cf147
    }
    // ONE QWORD each: kTextPos+kPenX, kRowPenX+kPenY, kFontIndex+kColorIndex,
    // and kVAlign+kColumnTop where kVAlign takes 2 and kColumnTop takes 0.
    Store64(B(self, O::kTextPos),   0);                        // 1800cf14d
    I32(self, O::kRightEdge) = I32(self, O::kWrapWidth);        // 1800cf15a
    Store64(B(self, O::kRowPenX),   0);                        // 1800cf160
    I32(self, O::kRowFirstItem) = 0;                            // 1800cf167
    I32(self, O::kHangIndent)   = 0;                            // 1800cf16e
    Store64(B(self, O::kFontIndex), 0);                        // 1800cf175
    I32(self, O::kHAlign) = 0;                                  // 1800cf17c
    Store64(B(self, O::kVAlign),    2);                        // 1800cf183

    DocumentItem item;
    while (RtUi_DocumentTextElement_GetNextItem(self, &item)) { // 1800cf196 / 1800cf2af
        const int32_t w = item.width;                           // 1800cf1a9
        if (I32(self, O::kRowPenX) + w > I32(self, O::kRightEdge))   // 1800cf1af
            FlushRow(self);                                     // 1800cf1ba..1800cf1ea
        I32(self, O::kRowPenX) += w;                            // 1800cf1f0

        RtAddr<ItemArrayPushFn>(0x18003df80ull)(
            self + O::kItemArrayData, I32(self, O::kItemCount) + 1, &item);  // 1800cf20a

        if (item.kind == 0) {                                   // 1800cf20f
            char* dead = nullptr;
            SubString(&STR(self, O::kContentString), &dead, item.first, item.last);
            ReleaseString(&dead);                               // 1800cf238..1800cf2a7
        }
    }

    I32(self, O::kPenY) += RtUi_DocumentTextElement_LayoutRow(self);   // 1800cf2c4
    const int32_t height = I32(self, O::kPenY);                 // 1800cf2ca
    I32(self, O::kRowFirstItem) = I32(self, O::kItemCount);     // 1800cf2d6
    I32(self, O::kPenX) += I32(self, O::kHangIndent);           // 1800cf2e2
    I32(self, O::kHangIndent) = 0;                              // 1800cf2e8
    I32(self, O::kRowPenX) = I32(self, O::kPenX);               // 1800cf2f5
    I32(self, O::kElementHeight) = height;                      // 1800cf2fb
}

// ============================================================================
// ?CreateInstance@DocumentTextDescription@UI@@QEBAX
//     V?$InternalPtr@VDocumentTextElement@UI@@@Object@@ PEBVDialog@2@@Z
// retail 0x1800cf520
// ============================================================================
// Returns VOID and takes the element BY VALUE — see the banner for why RDX is
// the parameter and not a hidden return slot, and why R8 (the Dialog) is never
// read. Unlike every other CreateInstance in this family this one does NOT
// allocate: the caller already built the element and handed it in.
//
// The description's layout, as used here:
//   +0x68 StringTable*   +0x70 String   the element's text reference
//   +0x78 SpriteSheet*   +0x80 String   the element's own sheet + sprite name
//   +0x88 Array<String>  +0x90 count    the frame-table sprite names
//   +0x98..+0xa8         five ints, copied straight across
//   +0xb0 / +0xc0        two arrays assigned by their own operator=
//   +0xd0 Array<int>     the colour table, copied by an INLINED assignment
//   +0xe0                the resource list pointer
extern "C" void RtUi_DocumentTextDescription_CreateInstance(const void* selfv,
                                                            void** elementPtr,
                                                            const void* /*dialog*/) {
    const uint8_t* self = reinterpret_cast<const uint8_t*>(selfv);

    // The shared ElementDescription applier runs on a LOCAL COPY of the
    // InternalPtr (retail copies it into a home slot at 0x1800cf548 and passes
    // that), not on the caller's slot.
    void* slot = *elementPtr;                                  // 1800cf545
    using EdCreateInstanceFn = void (*)(const void* self, void** elemPtr);
    RtAddr<EdCreateInstanceFn>(0x180115ad0ull)(self, &slot);   // 1800cf550

    // The one dword-written / qword-read stack slot, shared by all three sites.
    uint64_t indexSlot;                                        // [RSP+0x30]

    void* table = PTR(self, 0x68);                             // 1800cf555
    if (table != nullptr) {                                    // 1800cf559
        const int32_t idx = RtAddr<GetIndexFromNameFn>(0x180002470ull)(
            table, reinterpret_cast<char* const*>(B(self, 0x70)));
        __builtin_memcpy(&indexSlot, &idx, sizeof idx);        // 1800cf56a  DWORD store
    } else {
        const int32_t minusOne = -1;
        __builtin_memcpy(&indexSlot, &minusOne, sizeof minusOne);   // 1800cf570
    }

    uint8_t* elem = static_cast<uint8_t*>(*elementPtr);        // 1800cf578
    PTR(elem, O::kUiStringValue)     = table;                  // 1800cf57b
    Store64(B(elem, O::kUiStringValue + 8), (int64_t)indexSlot);  // 1800cf587

    elem = static_cast<uint8_t*>(*elementPtr);                 // 1800cf58e
    PTR(elem, O::kSpriteSheet) = PTR(self, 0x78);              // 1800cf595

    elem = static_cast<uint8_t*>(*elementPtr);
    using SetSizeFn = void (*)(void* array, int32_t n);
    RtAddr<SetSizeFn>(0x1800370c0ull)(elem + O::kFrameTable,
                                      I32(self, 0x90));        // 1800cf5ac

    const int32_t frames = I32(self, 0x90);
    for (int32_t i = 0; i < frames; ++i) {                     // 1800cf5b5 / 1800cf611
        char* const* names = *reinterpret_cast<char* const* const*>(B(self, 0x88));
        // The sheet is RE-READ from the description every iteration.
        void* sheet = PTR(self, 0x78);                         // 1800cf5ce
        if (sheet != nullptr) {                                // 1800cf5d2
            const int32_t idx = RtAddr<GetSpriteIndexFn>(0x1800031f0ull)(sheet, &names[i]);
            __builtin_memcpy(&indexSlot, &idx, sizeof idx);    // 1800cf5df  DWORD store
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&indexSlot, &minusOne, sizeof minusOne);   // 1800cf5e5
        }
        uint8_t* table2 = static_cast<uint8_t*>(
            PTR(static_cast<uint8_t*>(*elementPtr), O::kFrameTable));   // 1800cf5f0
        uint8_t* entry = table2 + static_cast<size_t>(i) * 0x10;
        PTR(entry, 0) = sheet;                                 // 1800cf5f7
        Store64(entry + 8, (int64_t)indexSlot);                // 1800cf600  QWORD store
    }

    {
        void* sheet = PTR(self, 0x78);                         // 1800cf613
        if (sheet != nullptr) {                                // 1800cf617
            const int32_t idx = RtAddr<GetSpriteIndexFn>(0x1800031f0ull)(
                sheet, reinterpret_cast<char* const*>(B(self, 0x80)));
            __builtin_memcpy(&indexSlot, &idx, sizeof idx);    // 1800cf62b
        } else {
            const int32_t minusOne = -1;
            __builtin_memcpy(&indexSlot, &minusOne, sizeof minusOne);   // 1800cf631
        }
        elem = static_cast<uint8_t*>(*elementPtr);             // 1800cf639
        PTR(elem, O::kCaptureRefSheet) = sheet;                // 1800cf63c
        Store64(B(elem, O::kCaptureRefIndex), (int64_t)indexSlot);   // 1800cf648
    }

    // Five ints, each through a FRESH load of the element pointer.
    I32(static_cast<uint8_t*>(*elementPtr), O::kInt0F8)   = I32(self, 0x98);   // 1800cf658
    I32(static_cast<uint8_t*>(*elementPtr), O::kInt0FC)   = I32(self, 0x9c);   // 1800cf667
    I32(static_cast<uint8_t*>(*elementPtr), O::kInt100)   = I32(self, 0xa0);   // 1800cf676
    I32(static_cast<uint8_t*>(*elementPtr), O::kTabWidth) = I32(self, 0xa4);   // 1800cf685
    I32(static_cast<uint8_t*>(*elementPtr), O::kWrapWidth)= I32(self, 0xa8);   // 1800cf694

    using ArrayAssignFn = void* (*)(void* dst, const void* src);
    RtAddr<ArrayAssignFn>(0x18003ce30ull)(
        static_cast<uint8_t*>(*elementPtr) + O::kFontArray,  B(self, 0xb0));   // 1800cf6ab
    RtAddr<ArrayAssignFn>(0x18003cf20ull)(
        static_cast<uint8_t*>(*elementPtr) + O::kSheetArray, B(self, 0xc0));   // 1800cf6c1

    // ------------------------------------------------------------------
    // The colour table: Array<int>::operator=, INLINED by retail rather than
    // called like the two above. Transcribed with its quirks intact —
    // the self-assignment test, the size stored as a sign-extended qword read
    // narrowed back to an int, the capacity written ONLY when the size is
    // positive, and the per-element copy that re-reads the destination pointer
    // every iteration and SKIPS the element when `data + offset` is null.
    // ------------------------------------------------------------------
    {
        uint8_t* dst = static_cast<uint8_t*>(*elementPtr) + O::kColorArray;   // 1800cf6c6
        const uint8_t* src = B(self, 0xd0);                                   // 1800cf6c9
        if (dst != reinterpret_cast<const uint8_t*>(src)) {                   // 1800cf6d7
            void* old = PTR(dst, 0);                                          // 1800cf6e0
            if (old != nullptr) MemFree(old);                                 // 1800cf6e6..1800cf718

            PTR(dst, 0) = nullptr;                                            // 1800cf71e
            Store64(dst + 8, 0);                                              // 1800cf721

            const int64_t n = I32(src, 0x08);                                 // 1800cf725
            I32(dst, 0x08) = static_cast<int32_t>(n);                         // 1800cf72a
            if (static_cast<int32_t>(n) > 0) {                                // 1800cf730
                I32(dst, 0x0c) = static_cast<int32_t>(n);                     // 1800cf732
                PTR(dst, 0) = MemAlloc(static_cast<uint64_t>(n) << 2, 4);     // 1800cf76a
            }
            for (int32_t i = 0; i < I32(dst, 0x08); ++i) {                    // 1800cf77e / 1800cf7a4
                const size_t off = static_cast<size_t>(i) * 4;
                int32_t* to = reinterpret_cast<int32_t*>(
                    static_cast<uint8_t*>(PTR(dst, 0)) + off);                // 1800cf788
                if (to == nullptr) continue;                                  // 1800cf78e  JZ
                *to = *reinterpret_cast<const int32_t*>(
                    static_cast<const uint8_t*>(PTR(src, 0)) + off);          // 1800cf797
            }
        }
    }

    PTR(static_cast<uint8_t*>(*elementPtr), O::kResourceList) = PTR(self, 0xe0);   // 1800cf7b0
}

// ============================================================================
// Registration. Both doors for every row: the mangled name (which is what the
// minted BaseTypeInfo and a minted vtable ask for first) and the address (which
// is what a ported caller's ResolveAddr literal asks for). Unconditional — no
// kill switch, no env gate.
// ============================================================================
int RtUiDocumentTextRegister() {
    int n = 0;

    RegisterRuntimeExport("?FindNextWhiteSpace@DocumentTextElement@UI@@AEAAHXZ",
                          (void*)&RtUi_DocumentTextElement_FindNextWhiteSpace);   ++n;
    RegisterRuntimeAddr(0x1800cefd0ull, (void*)&RtUi_DocumentTextElement_FindNextWhiteSpace);

    RegisterRuntimeExport("?EndLink@DocumentTextElement@UI@@AEAAXXZ",
                          (void*)&RtUi_DocumentTextElement_EndLink);              ++n;
    RegisterRuntimeAddr(0x1800cde00ull, (void*)&RtUi_DocumentTextElement_EndLink);

    RegisterRuntimeExport("?BeginLink@DocumentTextElement@UI@@AEAAXAEBVString@System@@@Z",
                          (void*)&RtUi_DocumentTextElement_BeginLink);            ++n;
    RegisterRuntimeAddr(0x1800cdeb0ull, (void*)&RtUi_DocumentTextElement_BeginLink);

    RegisterRuntimeExport("?LayoutRow@DocumentTextElement@UI@@AEAAHXZ",
                          (void*)&RtUi_DocumentTextElement_LayoutRow);            ++n;
    RegisterRuntimeAddr(0x1800cdbc0ull, (void*)&RtUi_DocumentTextElement_LayoutRow);

    RegisterRuntimeExport("?GetNextItem@DocumentTextElement@UI@@AEAA_NAEAUDocumentItem@12@@Z",
                          (void*)&RtUi_DocumentTextElement_GetNextItem);          ++n;
    RegisterRuntimeAddr(0x1800cdf40ull, (void*)&RtUi_DocumentTextElement_GetNextItem);

    RegisterRuntimeExport("?GenerateItems@DocumentTextElement@UI@@AEAAXXZ",
                          (void*)&RtUi_DocumentTextElement_GenerateItems);        ++n;
    RegisterRuntimeAddr(0x1800cf060ull, (void*)&RtUi_DocumentTextElement_GenerateItems);

    RegisterRuntimeExport(
        "?CreateInstance@DocumentTextDescription@UI@@QEBAXV?$InternalPtr@VDocumentTextElement@UI@@@Object@@PEBVDialog@2@@Z",
        (void*)&RtUi_DocumentTextDescription_CreateInstance);                     ++n;
    RegisterRuntimeAddr(0x1800cf520ull, (void*)&RtUi_DocumentTextDescription_CreateInstance);

    return n;
}
