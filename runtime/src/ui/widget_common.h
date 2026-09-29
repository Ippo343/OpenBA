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
//   * the small set of Runtime functions they CALL rather than reimplement
//     (called by name since B3R RT-UI),
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
// SpriteContext::AddSprite/AddFont). Those are called by name at each use.
// ============================================================================

#include <cstddef>
#include <cstdint>

#include "../../types/video/Device.h"   // ENDGAME B.3.9 SS1: Video::Device::_singleton
#include "../../types/ui/Element.h"     // B3X W0: InlineGetPaddedSize calls ComputeSize by name
#include "../../types/ui/DialogInstance.h"   // RootUiScale
#include "../../types/ui/UserInterface.h"    // RootUiScale
#include "../../types/system/MemoryAllocator.h"   // B3R RT-UI: MemFree, direct

namespace rtui {

// ---------------------------------------------------------------------------
// Byte accessors. B.4 LEFT: their one remaining caller is listelement.cpp
// (block R-UI-a: the grid view's extents / spacing / vertical byte); they go
// with it.
// ---------------------------------------------------------------------------
inline uint8_t*  B(void* p, int off)       { return static_cast<uint8_t*>(p) + off; }

inline int32_t&  I32(void* p, int off) { return *reinterpret_cast<int32_t*>(B(p, off)); }
inline uint8_t&  U8 (void* p, int off) { return *reinterpret_cast<uint8_t*>(B(p, off)); }
inline void*&    PTR(void* p, int off) { return *reinterpret_cast<void**>(B(p, off)); }

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
inline void DirtyWalk(::UI::Element* e) {
    for (::UI::Element* n = e; n != nullptr; n = n->_parent)
        n->_flags |= kFlagNeedsSizeCalc;
}

// Set/clear flag bits with retail's "only walk when bit 0 changed" guard.
inline void SetFlags(::UI::Element* e, uint32_t nf) {
    const uint32_t of = e->_flags;
    e->_flags = nf;
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
// `?_singleton@Device@Video@@0PEAV12@EA` — the symbol holds a Device*, so the
// screen size is two loads: [singleton] then +4 / +8.
// ---------------------------------------------------------------------------
//
// ENDGAME B.3.9 SS1 (B39_RULES D7): the class static now, read fresh every
// call -- InlineGetPaddedSize below reaches this from member bodies that run
// on the LOADER THREAD (B9-10: no lazy cache).
inline int32_t ScreenWidth()  { return ::Video::Device::_singleton->backBufferWidth; }   // +4
inline int32_t ScreenHeight() { return ::Video::Device::_singleton->backBufferHeight; }  // +8

// The UI scale of the element's ROOT dialog: walk _parent to the top, then
// root->_dialogParent->_ui->_uiScale (+0x90, +0x10, +0xdc). B.4 LEFT:
// Element::_dialogParent and DialogInstance::_ui are `void*` in block R-UI-a's
// headers.
inline float RootUiScale(const ::UI::Element* e) {
    const ::UI::Element* n = e;
    while (n->_parent != nullptr) n = n->_parent;
    const auto* dialog = static_cast<const ::UI::DialogInstance*>(n->_dialogParent);
    return static_cast<const ::UI::UserInterface*>(dialog->_ui)->_uiScale;
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
inline void InlineGetPaddedSize(::UI::Element* child, int32_t* w, int32_t* h) {
    child->ComputeSize();                                   // Element slot 4, by name

    const uint32_t flags = child->_flags;
    int32_t cw = child->_layoutWidth  + child->_leftPad + child->_rightPad;
    int32_t ch = child->_layoutHeight + child->_topPad  + child->_bottomPad;
    if (cw < child->_minWidth)  cw = child->_minWidth;
    if (ch < child->_minHeight) ch = child->_minHeight;
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

// The flag bits above are the header's, not a second opinion.
#include "../../types/ui/Element.h"
static_assert(rtui::kFlagHidden        == UI::kElementFlagHidden, "");
static_assert(rtui::kFlagNeedsSizeCalc == UI::kElementFlagNeedsSizeCalc, "");
static_assert(rtui::kFlagDimColor      == UI::kElementFlagDimColor, "");
static_assert(rtui::kFlagSizeToScreenH == UI::kElementFlagSizeToScreenH, "");
static_assert(rtui::kFlagSizeToScreenW == UI::kElementFlagSizeToScreenW, "");
static_assert(rtui::kFlagWasActivated  == UI::kElementFlagWasActivated, "");
static_assert(sizeof(UI::Element) == 0xC0, "the fourteen widgets all start their own fields at +0xC0");
