#pragma once
// ============================================================================
// runtime/src/ui/widget_common.h — shared plumbing for the UI::*Element bodies
// ============================================================================
//
// The fourteen widget classes ported in the "elem_B" batch (LayoutElement,
// ListElement, NumberEditElement, NumberElement, PageElement, ProgressElement,
// RadioElement, RibbonElement, ScrollElement, SliderElement, SpinnerElement,
// TabElement, TableElement, TextElement) each get ONE .cpp next to this file.
// What they all share lives here and nowhere else:
//
//   * the byte accessors every body needs (these objects are addressed by
//     offset, not by a C++ struct, because most of the fourteen have no
//     complete layout header and inventing one would be a guess),
//   * the small set of Runtime functions they CALL rather than reimplement,
//     each reached through ResolveAddr so a sibling port of the same address
//     is picked up automatically
//     (reference_resolveaddr_consults_port_registry_first.md),
//   * `InlineGetPaddedSize`, which is `?GetPaddedSize@Element@UI@@`
//     (0x180115f10) INLINED — the compiler inlined it into a dozen of these
//     bodies and the inlined copy is what is transcribed, not a call.
//
// NOT here, deliberately: the UI text-layout helpers (Font::ComputeLineLayout
// 0x180116c70, Font::ComputeStringLayout 0x180116fd0, DocumentTextElement's
// GetNextItem/SelectCharacter family) and the per-class private helpers that
// the batch brief assigns to other agents (LayoutElement::GetChildLayout
// 0x1800d2dc0, ListElement::DrawHighlight 0x1800d3f70, ScrollElement::
// GetBarLayout 0x18011a2d0, SliderElement::UpdateNumber 0x1800d6c20,
// TableElement::GetRowHeight/GetColumnWidth 0x18011e090/0x18011e1a0,
// SpriteContext::AddSprite/AddFont). Those are REACHED, by address, and the
// address literal is written at each use so the census can see it.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../funcs/rt_port.h"

// ResolveAddr lives in standalone/resolve.h, which drags in <windows.h>; PLAN
// §4.1 keeps that out of runtime/src. Declared here exactly as ~30 sibling
// runtime files declare it; the linker checks the two agree.
enum class Module : int { Exe = 0, Runtime = 1, VideoDX11 = 2, COUNT = 3 };
uintptr_t ResolveAddr(Module mod, uintptr_t ghidra_addr);

namespace rtui {

// ---------------------------------------------------------------------------
// Byte accessors.
// ---------------------------------------------------------------------------
inline uint8_t*  B(void* p, int off)       { return static_cast<uint8_t*>(p) + off; }
inline const uint8_t* B(const void* p, int off) { return static_cast<const uint8_t*>(p) + off; }

inline int32_t&  I32(void* p, int off) { return *reinterpret_cast<int32_t*>(B(p, off)); }
inline uint32_t& U32(void* p, int off) { return *reinterpret_cast<uint32_t*>(B(p, off)); }
inline float&    F32(void* p, int off) { return *reinterpret_cast<float*>(B(p, off)); }
inline uint8_t&  U8 (void* p, int off) { return *reinterpret_cast<uint8_t*>(B(p, off)); }
inline void*&    PTR(void* p, int off) { return *reinterpret_cast<void**>(B(p, off)); }

inline int32_t  I32c(const void* p, int off) { return *reinterpret_cast<const int32_t*>(B(p, off)); }
inline uint32_t U32c(const void* p, int off) { return *reinterpret_cast<const uint32_t*>(B(p, off)); }
inline float    F32c(const void* p, int off) { return *reinterpret_cast<const float*>(B(p, off)); }
inline uint8_t  U8c (const void* p, int off) { return *reinterpret_cast<const uint8_t*>(B(p, off)); }
inline void*    PTRc(const void* p, int off) { return *reinterpret_cast<void* const*>(B(p, off)); }

inline void** VTable(const void* o) {
    return *reinterpret_cast<void** const*>(o);
}

template <typename Fn>
inline Fn RtAddr(uintptr_t ghidra) {
    return reinterpret_cast<Fn>(ResolveAddr(Module::Runtime, ghidra));
}

// ---------------------------------------------------------------------------
// UI::Element field offsets. Every one of them is asserted against
// runtime/types/ui/Element.h at the bottom of this header, so a change there
// breaks the build here rather than silently mis-addressing fourteen widgets.
// ---------------------------------------------------------------------------
constexpr int kElLeftPad      = 0x18;
constexpr int kElRightPad     = 0x1c;
constexpr int kElTopPad       = 0x20;
constexpr int kElBottomPad    = 0x24;
constexpr int kElMinWidth     = 0x28;
constexpr int kElMinHeight    = 0x2c;
constexpr int kElAlignment    = 0x30;
constexpr int kElFlags        = 0x34;
constexpr int kElColor        = 0x38;
constexpr int kElLayoutWidth  = 0x60;
constexpr int kElLayoutHeight = 0x64;
constexpr int kElLayoutLeft   = 0x68;
constexpr int kElLayoutTop    = 0x6c;
constexpr int kElLayoutRight  = 0x70;
constexpr int kElLayoutBottom = 0x74;
constexpr int kElElementId    = 0x88;
constexpr int kElDialogParent = 0x90;
constexpr int kElParent       = 0x98;
constexpr int kElChildren     = 0xa0;   // Array<Element*>::_data
constexpr int kElChildCount   = 0xa8;   // Array<Element*>::_size
constexpr int kElBorder       = 0xb0;

// Element vtable byte offsets (Element.h's twelve-slot table).
constexpr int kVtComputeSize   = 0x20;
constexpr int kVtComputeLayout = 0x28;
constexpr int kVtDraw          = 0x30;
constexpr int kVtUpdate        = 0x38;
constexpr int kVtEnable        = 0x40;
constexpr int kVtSetColor      = 0x50;

// Element::_flags bits used by these bodies (names from Element.h).
constexpr uint32_t kFlagHidden        = 0x00000001;
constexpr uint32_t kFlagStretchX      = 0x00000004;
constexpr uint32_t kFlagStretchY      = 0x00000008;
constexpr uint32_t kFlagSizeToScreenH = 0x00000010;   // height <- screen height
constexpr uint32_t kFlagSizeToScreenW = 0x00000020;   // width  <- screen width
constexpr uint32_t kFlagDimColor      = 0x00040000;
constexpr uint32_t kFlagInteractive   = 0x00020000;   // bit 17, tested by every
                                                      // widget Update before it
                                                      // will react to the mouse
constexpr uint32_t kFlagWasActivated  = 0x00400000;   // bit 22
constexpr uint32_t kFlagDragging      = 0x00200000;   // bit 21, set while a
                                                      // slider/scroll thumb is
                                                      // being dragged
constexpr uint32_t kFlagNeedsSizeCalc = 0x08000000;

// ---------------------------------------------------------------------------
// Element::SetNeedsSizeCalc (0x180035470), inlined everywhere.
//   for (n = e; n; n = n->_parent) n->_flags |= 0x08000000;
// The shipped form is a do/while that loads the parent slot into a temporary
// first; identical, and the temporary costs nothing to drop.
// ---------------------------------------------------------------------------
inline void DirtyWalk(void* e) {
    for (void* n = e; n != nullptr; n = PTR(n, kElParent))
        U32(n, kElFlags) |= kFlagNeedsSizeCalc;
}

// Set/clear flag bits with retail's "only walk when bit 0 changed" guard.
inline void SetFlags(void* e, uint32_t nf) {
    const uint32_t of = U32(e, kElFlags);
    U32(e, kElFlags) = nf;
    if (((nf ^ of) & kFlagHidden) != 0) DirtyWalk(e);
}

// ---------------------------------------------------------------------------
// `?ComputeColor@Element@UI@@` (0x180035490), inlined into every widget Draw:
//   if (flags & 0x00040000) alpha >>= 1;
//   return ((c >> 8 & 0xffff0000) * alpha ^ c) & 0xffffff
//          ^ (c >> 8 & 0xff0000) * alpha;
// Transcribed exactly as the shipped expression, parenthesised the same way —
// the two multiplies are by the (already halved) alpha byte and the xor
// ordering is load-bearing.
// ---------------------------------------------------------------------------
inline uint32_t ModulateColor(uint32_t c, uint8_t alpha) {
    return ((((c >> 8) & 0xffff0000u) * alpha) ^ c) & 0xffffffu
           ^ (((c >> 8) & 0xff0000u) * alpha);
}

// ---------------------------------------------------------------------------
// The Runtime functions these bodies call.
// ---------------------------------------------------------------------------
using ElementUpdateFn  = void (*)(void* self, const void* time, void* ui,
                                  void* dlgInternalPtr, const void* updateState);
using ElementCtorFn    = void (*)(void* self);
using ElementDtorFn    = void (*)(void* self);
using GetPaddedSizeFn  = void (*)(void* self, int32_t* w, int32_t* h);
using AdjustUpperLeftFn= void (*)(void* self, int32_t dx, int32_t dy);
using PlayFn           = void (*)(void* ui, int32_t soundId);
using ArrayDtorFn      = void (*)(void* hdr);
using ArraySetSizeFn   = void (*)(void* hdr, int32_t n);
using ArraySetSizeFillFn = void (*)(void* hdr, int32_t n, const void* fill);
using MemFreeFn        = void (*)(void* p);
using ToIntegerFn      = int32_t (*)(const char* s);
using IsDerivedFromFn  = bool (*)(const void* self, uint32_t typeId);
using ComputeSizeSlotFn   = void (*)(void* self);
using ComputeLayoutSlotFn = void (*)(void* self, int32_t x, int32_t y, int32_t r, int32_t b);
using DrawSlotFn          = void (*)(const void* self, void* ctx, uint8_t alpha);
using EnableSlotFn        = void (*)(void* self, bool on);
using SetColorSlotFn      = void (*)(void* self, uint32_t color);

// `?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHI@Z`      0x18011cf00
using AddSprite3Fn = void (*)(void* ctx, const void* sprite, int32_t x, int32_t y, uint32_t c);
// `?AddSprite@SpriteContext@UI@@QEAAXAEBVSprite@2@HHHHI@Z`    0x18011d3c0
using AddSprite5Fn = void (*)(void* ctx, const void* sprite, int32_t x0, int32_t y0,
                              int32_t x1, int32_t y1, uint32_t c);
// `?AddFont@SpriteContext@UI@@QEAAXV?$ExternalPtr@VFont@UI@@@Object@@AEBVString@System@@
//   _NHW4Justify@Font@2@HHHI@Z`                               0x18011ccb0
using AddFontFn    = void (*)(void* ctx, void* font, const void* str, uint8_t wrap,
                              int32_t a, int32_t justify, int32_t width,
                              int32_t x, int32_t y, uint32_t c);

inline void ElementUpdate(void* self, const void* time, void* ui, void* dlg, const void* st) {
    RtAddr<ElementUpdateFn>(0x180116270ull)(self, time, ui, dlg, st);
}
inline void ElementCtor(void* self) { RtAddr<ElementCtorFn>(0x180115e50ull)(self); }
inline void ElementDtor(void* self) { RtAddr<ElementDtorFn>(0x1801161a0ull)(self); }
inline void MemFree(void* p)        { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }

// ---------------------------------------------------------------------------
// System::String release, inlined by every one of these bodies that owns one.
// The body pointer's refcount is the dword at [-4] and the block base is
// [-8] (Element.h records the same shape for Element::_name):
//     if (p) { if (--p[-1] == 0) { field = p - 8; MemFree(field); }
//              else field = 0; }
// Note the ELSE branch: retail nulls the field when the string SURVIVES and
// leaves it pointing at the freed block when it does not. That looks backwards
// and it is what the shipped code does — ~NumberElement, ~TextElement,
// NumberEditElement::FinalizeText and NumberElement::CacheText all do it.
// ---------------------------------------------------------------------------
inline void ReleaseStringField(void* self, int off) {
    uint8_t* p = static_cast<uint8_t*>(PTR(self, off));
    if (p == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(p - 4);
    if (--*rc == 0) {
        PTR(self, off) = static_cast<uint8_t*>(PTR(self, off)) - 8;
        MemFree(PTR(self, off));
    } else {
        PTR(self, off) = nullptr;
    }
}

// The same release on a local (the temporary System::String a Format result
// leaves on the stack).
inline void ReleaseStringLocal(void** slot) {
    uint8_t* p = static_cast<uint8_t*>(*slot);
    if (p == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(p - 4);
    if (--*rc == 0) { *slot = p - 8; MemFree(*slot); }
}

// Assign a System::String body pointer into a field with retail's identity
// guard (`if (old != new)`), releasing the old and acquiring the new.
inline void AssignStringField(void* self, int off, void* nv) {
    uint8_t* old = static_cast<uint8_t*>(PTR(self, off));
    if (old == nv) return;
    if (old != nullptr) {
        int32_t* rc = reinterpret_cast<int32_t*>(old - 4);
        if (--*rc == 0) {
            PTR(self, off) = static_cast<uint8_t*>(PTR(self, off)) - 8;
            MemFree(PTR(self, off));
        } else {
            PTR(self, off) = nullptr;
        }
    }
    PTR(self, off) = nv;
    if (nv != nullptr) ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(nv) - 4);
}

// ---------------------------------------------------------------------------
// A UI::Sprite's pixel size, as the widget Draw/ComputeSize bodies read it —
// INLINE, never through `?Width@Sprite@UI@@` (0x180030bf0):
//
//     sheet   = *(SpriteSheet**)(sprite + 0x00)      runtime/types/ui/Sprite.h
//     index   =  *(int32_t*)   (sprite + 0x08)
//     entries = *(uint8_t**)   (sheet  + 0x20)       SpriteSheet::_sprites
//     w = *(int32_t*)(entries + index*0x38 + 0x1c)
//     h = *(int32_t*)(entries + index*0x38 + 0x20)
//
// The 0x38 stride and the +0x1c/+0x20 pair are runtime/types/ui/SpriteSheet.h's
// SpriteInfo. Every call site guards with `if (index < 0) 0`, and the SHEET is
// never null-checked — that unguarded deref is retail's and stays.
// ---------------------------------------------------------------------------
constexpr int kSpriteSheet  = 0x00;
constexpr int kSpriteIndex  = 0x08;
constexpr int kSheetEntries = 0x20;
constexpr int kSpriteInfoStride = 0x38;
constexpr int kSpriteInfoW  = 0x1c;
constexpr int kSpriteInfoH  = 0x20;

inline int32_t SpriteWidth(const void* self, int spriteOff) {
    const int32_t idx = I32c(self, spriteOff + kSpriteIndex);
    if (idx < 0) return 0;
    void* sheet = PTRc(self, spriteOff + kSpriteSheet);
    uint8_t* entries = static_cast<uint8_t*>(PTR(sheet, kSheetEntries));
    return *reinterpret_cast<int32_t*>(entries + idx * kSpriteInfoStride + kSpriteInfoW);
}
inline int32_t SpriteHeight(const void* self, int spriteOff) {
    const int32_t idx = I32c(self, spriteOff + kSpriteIndex);
    if (idx < 0) return 0;
    void* sheet = PTRc(self, spriteOff + kSpriteSheet);
    uint8_t* entries = static_cast<uint8_t*>(PTR(sheet, kSheetEntries));
    return *reinterpret_cast<int32_t*>(entries + idx * kSpriteInfoStride + kSpriteInfoH);
}

// ---------------------------------------------------------------------------
// `?_singleton@Device@Video@@0PEAV12@EA` — the symbol holds a Device*, so the
// screen size is two loads: [singleton] then +4 / +8.
// ---------------------------------------------------------------------------
inline void* VideoDevice() {
    void** slot = RT_DATA_SLOT(void**, "?_singleton@Device@Video@@0PEAV12@EA");
    return slot ? *slot : nullptr;
}
inline int32_t ScreenWidth()  { return I32(VideoDevice(), 4); }
inline int32_t ScreenHeight() { return I32(VideoDevice(), 8); }

// The UI scale of the element's ROOT dialog: walk _parent to the top, then
// root->_dialogParent[+0x10] + 0xdc, read as float.
inline float RootUiScale(void* e) {
    void* n = e;
    while (PTR(n, kElParent) != nullptr) n = PTR(n, kElParent);
    return F32(PTR(PTR(n, kElDialogParent), 0x10), 0xdc);
}

// ---------------------------------------------------------------------------
// `?GetPaddedSize@Element@UI@@QEAAXAEAH0@Z` (0x180115f10), INLINED. The
// compiler pasted this into a dozen of the bodies below; each one is
// transcribed as a call to this function rather than a twelfth copy.
//
//   child->ComputeSize();                      // vtable slot 4
//   w = child->_layoutWidth  + _leftPad + _rightPad;
//   h = child->_layoutHeight + _topPad  + _bottomPad;
//   if (w < _minWidth)  w = _minWidth;
//   if (h < _minHeight) h = _minHeight;
//   if (flags & 0x30) {  root walk;
//       if (flags & 0x20) w = (int)((float)screenW * uiScale);
//       if (flags & 0x10) h = (int)((float)screenH * uiScale); }
//
// A DEPARTURE, stated rather than hidden: several call sites use only w or
// only h and the compiler dropped the other arm. Computing both is extra
// READS only — no store, no call, no observable difference — and it keeps one
// copy of a sequence that is otherwise pasted twelve times.
// ---------------------------------------------------------------------------
inline void InlineGetPaddedSize(void* child, int32_t* w, int32_t* h) {
    void** vt = *reinterpret_cast<void***>(child);
    reinterpret_cast<ComputeSizeSlotFn>(vt[kVtComputeSize / 8])(child);

    const uint32_t flags = U32(child, kElFlags);
    int32_t cw = I32(child, kElLayoutWidth)  + I32(child, kElLeftPad) + I32(child, kElRightPad);
    int32_t ch = I32(child, kElLayoutHeight) + I32(child, kElTopPad)  + I32(child, kElBottomPad);
    if (cw < I32(child, kElMinWidth))  cw = I32(child, kElMinWidth);
    if (ch < I32(child, kElMinHeight)) ch = I32(child, kElMinHeight);
    if ((flags & (kFlagSizeToScreenW | kFlagSizeToScreenH)) != 0) {
        const float scale = RootUiScale(child);
        if ((flags & kFlagSizeToScreenW) != 0)
            cw = static_cast<int32_t>(static_cast<float>(ScreenWidth()) * scale);
        if ((flags & kFlagSizeToScreenH) != 0)
            ch = static_cast<int32_t>(static_cast<float>(ScreenHeight()) * scale);
    }
    *w = cw;
    *h = ch;
}

// ---------------------------------------------------------------------------
// `?GetAlignedPosition@Element@UI@@` (0x180115ff0), inlined into every one of
// the fourteen ComputeLayout bodies as the same switch on _alignment. The
// shipped shape, transcribed once:
//
//   if (right  < w + x) x = right  - w;      // w/h are the element's own
//   if (bottom < h + y) y = bottom - h;      //   _layoutWidth/_layoutHeight
//   px = x + _leftPad;  py = y + _topPad;
//   aw = (right  - _rightPad)  - px;
//   ah = (bottom - _bottomPad) - py;
//   if (flags & 4) w = aw;                   // "stretch X"
//   if (flags & 8) h = ah;                   // "stretch Y"
//   switch (align) { 1,4,7: px += (aw>>1) - (w>>1);
//                    2,5,8: px  = (right - _rightPad) - w; }
//   if (align > 2) { align < 6 : py += (ah>>1) - (h>>1);
//                    align < 9 : py  = (bottom - _bottomPad) - h; }
//
// NOT every widget runs the whole thing: LayoutElement/ListElement/
// ProgressElement/ScrollElement/SliderElement each drop one or both stretch
// tests or one arm of the switch, so this helper is NOT shared — each body
// spells out exactly the arms its own disassembly has. What IS shared is the
// arithmetic order, which is why the derivation is written down here once.
// ---------------------------------------------------------------------------

}  // namespace rtui

// The offsets above are the header's, not a second opinion.
#include "../../types/ui/Element.h"
static_assert(rtui::kElLeftPad      == offsetof(UI::Element, _leftPad), "");
static_assert(rtui::kElRightPad     == offsetof(UI::Element, _rightPad), "");
static_assert(rtui::kElTopPad       == offsetof(UI::Element, _topPad), "");
static_assert(rtui::kElBottomPad    == offsetof(UI::Element, _bottomPad), "");
static_assert(rtui::kElMinWidth     == offsetof(UI::Element, _minWidth), "");
static_assert(rtui::kElMinHeight    == offsetof(UI::Element, _minHeight), "");
static_assert(rtui::kElAlignment    == offsetof(UI::Element, _alignment), "");
static_assert(rtui::kElFlags        == offsetof(UI::Element, _flags), "");
static_assert(rtui::kElColor        == offsetof(UI::Element, _color), "");
static_assert(rtui::kElLayoutWidth  == offsetof(UI::Element, _layoutWidth), "");
static_assert(rtui::kElLayoutHeight == offsetof(UI::Element, _layoutHeight), "");
static_assert(rtui::kElLayoutLeft   == offsetof(UI::Element, _layoutLeft), "");
static_assert(rtui::kElLayoutTop    == offsetof(UI::Element, _layoutTop), "");
static_assert(rtui::kElLayoutRight  == offsetof(UI::Element, _layoutRight), "");
static_assert(rtui::kElLayoutBottom == offsetof(UI::Element, _layoutBottom), "");
static_assert(rtui::kElElementId    == offsetof(UI::Element, _elementId), "");
static_assert(rtui::kElDialogParent == offsetof(UI::Element, _dialogParent), "");
static_assert(rtui::kElParent       == offsetof(UI::Element, _parent), "");
static_assert(rtui::kElChildren     == offsetof(UI::Element, _children), "");
static_assert(rtui::kElChildCount   == offsetof(UI::Element, _children) + 8, "");
static_assert(rtui::kElBorder       == offsetof(UI::Element, _border), "");
static_assert(rtui::kFlagHidden        == UI::kElementFlagHidden, "");
static_assert(rtui::kFlagNeedsSizeCalc == UI::kElementFlagNeedsSizeCalc, "");
static_assert(rtui::kFlagDimColor      == UI::kElementFlagDimColor, "");
static_assert(rtui::kFlagSizeToScreenH == UI::kElementFlagSizeToScreenH, "");
static_assert(rtui::kFlagSizeToScreenW == UI::kElementFlagSizeToScreenW, "");
static_assert(rtui::kFlagWasActivated  == UI::kElementFlagWasActivated, "");
static_assert(sizeof(UI::Element) == 0xC0, "the fourteen widgets all start their own fields at +0xC0");
