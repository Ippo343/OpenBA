// ============================================================================
// runtime/src/ui/labelelement.cpp — UI::LabelElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7LabelElement@UI@@6B@` (retail 0x180156ad0).
// A `UI::TextElement` subclass that pairs the text with one icon sprite and
// places the two relative to each other.
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x180042bb0  deleting_dtor.cpp row
//    1    TypeName              0x1800427f0  here
//    2    Type                  0x1800427d0  here
//    4    ComputeSize           0x1800d2490  here
//    5    ComputeLayout         0x1800d2330  here
//    6    Draw                  0x1800d1c80  here
//   10    SetColor              0x180042970  here
//
// plus ??0LabelElement@UI@@QEAA@XZ         0x1800d2620
//      ??0LabelElement@UI@@QEAA@AEBV01@@Z  0x180042a00
//      ??1LabelElement@UI@@UEAA@XZ         0x1800d25f0
//
// UI::TextElement itself (its ctor, dtor and three slots) is batch 4's; this
// file reaches `??0TextElement@UI@@` (0x18011eb80), its copy constructor
// (0x1800420d0) and `??1TextElement@UI@@` (0x18011eda0) BY ADDRESS, so a later
// port of them is picked up with no change here.
//
// LAYOUT — 0x130 bytes (`MOV EDX,0x130` at 0x180042be2), TextElement's 0x100
// plus twelve dwords. Every one is a constructor store at 0x1800d2620 or a read
// in Draw / ComputeSize:
//
//   +0x100 Sprite  the icon: sheet +0x100, index +0x108 (ctor -1)
//   +0x110 int32   the icon/text pair's INNER alignment, 0..8 (ctor 3)
//   +0x114 uint32  layout mode: 0 icon RIGHT of text, 1 icon LEFT,
//                  2 icon BELOW, 3 icon ABOVE. (ctor 1.)  ComputeSize only
//                  distinguishes `> 1` (stacked) from `<= 1` (side by side).
//   +0x118 int32   minimum text width (ctor 0, via the qword at +0x114)
//   +0x11c int32   the GAP between icon and text, in pixels (ctor 2)
//   +0x120 int32   icon width override,  <1 means "use the sprite" (ctor -1)
//   +0x124 int32   icon height override, <1 means "use the sprite" (ctor -1)
//   +0x128 uint32  the icon's colour — SetColor writes it AND Element::_color
//   +0x12c int32   the measured text width, an OUTPUT of ComputeSize
//
// The TextElement fields this file reads are named at their use: +0xc0 the
// `UI::Font*`, +0xe8/+0xe9 two byte flags, +0xec/+0xf0/+0xf4 three ints and
// +0xf8 the `System::String`.
//
// `??0LabelElement@UI@@QEAA@XZ` (0x1800d2620) is NOT here: it is already
// ported and registered, table-driven, by runtime/src/ui/ui_factory_wave28.cpp
// (the factory roster). One address, one owner.
//
// ENDGAME B.3.6 block C3: the six slot bodies are `UI::LabelElement` members
// (runtime/types/ui/LabelElement.h — the layout above, now asserted; the
// TextElement fields are TextElement's). The registered doors delegate with a
// QUALIFIED call. Font / SpriteContext / Sprite calls are direct member calls.
// ============================================================================
#include <atomic>

#include "element_common.h"
#include "../../types/ui/LabelElement.h"
#include "../../types/ui/Font.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/system/String.h"

using namespace rtuielem;

namespace {

std::atomic<void*> g_typeInfoSlot{nullptr};   // RULE B6-5

const uint8_t* LabelElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfoSlot, "?_typeInfo@LabelElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* VtLabelElement() { return RT_DATA_SLOT(void*, "??_7LabelElement@UI@@6B@"); }

using TextCopyCtorFn = void* (*)(void* self, const void* other);
using TextDtorFn     = void  (*)(void* self);

const System::String& AsString(char* const& field) {
    return *reinterpret_cast<const System::String*>(&field);
}

}  // namespace

namespace UI {

uint32_t LabelElement::Type() const { return TypeIdOf(LabelElementTypeInfo()); }
const char* LabelElement::TypeName() const { return TypeNameOf(LabelElementTypeInfo()); }

// Non-virtual, EMPTY: the dtor door keeps retail's body.
LabelElement::~LabelElement() {}

// The icon's drawn size: the override when positive, else the sprite's own.
// Both arms test the SAME field — the sprite INDEX — for the "no sprite" case,
// not the sheet pointer.
static int IconWidth(const LabelElement& e) {
    if (e._iconWidth > 0) return e._iconWidth;
    if (e._sprite.spriteIndex < 0) return 0;
    return e._sprite.Width();
}
static int IconHeight(const LabelElement& e) {
    if (e._iconHeight > 0) return e._iconHeight;
    if (e._sprite.spriteIndex < 0) return 0;
    return e._sprite.Height();
}

// ============================================================================
// ?ComputeSize@LabelElement@UI@@UEAAXXZ   0x1800d2490
// ============================================================================
// Measure the text (only when there IS a font AND a non-empty string), floor
// its width by _minTextWidth, then combine with the icon according to the mode:
//
//   mode >  1  (stacked)   width  = max(iconW, textW)
//                          height = gap + iconH + textH
//   mode <= 1  (side)      width  = gap + iconW + textW
//                          height = max(iconH, textH)
//
// `_textWidth` is set to the RAW measured width, BEFORE the floor — the floor
// only affects what is combined with the icon. Draw then reads `_textWidth`, so
// a label whose minimum width exceeds its text draws the text at its natural
// width inside a wider box.
void LabelElement::ComputeSize() {
    if (((_flags >> 27) & 1u) == 0) return;

    int textW = 0;
    int textH = 0;
    if (_font != nullptr) {
        char* str = _cachedText;
        if (str != nullptr && *reinterpret_cast<const int32_t*>(str - 8) > 0) {
            int w = 0, h = 0;
            _font->ComputeStringLayout(AsString(_cachedText), _wrap != 0,
                                       _lineSpacing, _maxWidth, 0,
                                       w, h, nullptr);
            _textWidth = w;
            textH = h;
            textW = w;
            if (w < _minTextWidth) textW = _minTextWidth;
        }
    }

    const int iconW = IconWidth(*this);
    const int iconH = IconHeight(*this);

    if (_mode > 1u) {
        int w = iconW;
        if (w < textW) w = textW;
        _flags &= ~kElementFlagNeedsSizeCalc;
        _layoutWidth  = w;
        _layoutHeight = _gap + iconH + textH;
        return;
    }

    int h = iconH;
    if (h < textH) h = textH;
    _flags &= ~kElementFlagNeedsSizeCalc;
    _layoutWidth  = _gap + iconW + textW;
    _layoutHeight = h;
}

// ============================================================================
// ?ComputeLayout@LabelElement@UI@@UEAAXHHHH@Z   0x1800d2330 — the standard
// Element alignment body (element_common.h), which dispatches slots 5/4.
// ============================================================================
void LabelElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ElementComputeAlignedRect(this, x, y, w, h);
}

// ============================================================================
// ?Draw@LabelElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d1c80
// ============================================================================
// Four layout modes, each placing the icon and the text inside the element's
// rectangle and then aligning the pair by `_align`. The four arms share a
// shape but not a body: retail emits them separately, and the differences are
// real — mode 0 aligns the icon on the X axis but pins it to the right edge,
// mode 2 pins it to the bottom, and so on.
//
// TRANSCRIBED LITERALLY, INCLUDING THE COLLAPSED ARMS. Several placements read
// as `((R - (R - w)) >> 1 - (w >> 1)) + (R - w)`, which is algebraically just
// `R - w`; the compiler left the centring expression in because the operands
// were already live. Kept as it is emitted.
//
// THE SHADOW PASS IS A SECOND AddFont AT (+1, +1) IN OPAQUE BLACK (0xff000000)
// drawn BEFORE the real text, gated by `_drawShadow`. Its colour is a CONSTANT.
//
// The text's Y is `(font.lineHeight - font.baseline) + placedY`, i.e. the
// placement computes a top edge and AddFont wants a baseline.
void LabelElement::Draw(SpriteContext& ctx, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(ctx, alpha);                            // virtual, slot 6

    const uint32_t baseColor = _color;
    alpha = DimAlpha(this, alpha);

    Font* font = _font;
    char* str  = _cachedText;

    int textW = 0, textH = 0;
    bool hasText = false;
    if (font != nullptr && str != nullptr &&
        *reinterpret_cast<const int32_t*>(str - 8) >= 1) {
        textW = _textWidth;
        textH = font->_lineHeight;
        hasText = true;
    }

    const int iconW = IconWidth(*this);
    const int iconH = IconHeight(*this);

    const uint32_t mode  = _mode;
    const int      align = _align;

    const int halfTextW = textW >> 1;
    const int halfTextH = textH >> 1;
    const int halfIconW = iconW >> 1;
    const int halfIconH = iconH >> 1;

    const int L = _layoutLeft;
    const int T = _layoutTop;
    const int R = _layoutRight;
    const int B = _layoutBottom;
    const int gap = _gap;

    int textX, textY, iconX, iconY;

    if (mode == 0) {
        // Icon on the RIGHT, text to its left.
        textX = L;
        iconY = T;
        const int textRight = (R - gap) - iconW;
        if (textRight < L + textW) textX = textRight - textW;
        int ty = T;
        if (B < textH + T) ty = B - textH;
        switch (align) {
        case 1: case 4: case 7: textX = (((textRight - textX) >> 1) - halfTextW) + textX; break;
        case 2: case 5: case 8: textX = textRight - textW;                                break;
        default: break;
        }
        if (align > 2) {
            if (align < 6)      ty = (((B - ty) >> 1) - halfTextH) + ty;
            else if (align < 9) ty = B - textH;
        }
        textY = ty;
        if (B < iconH + T) iconY = B - iconH;
        iconX = (((R - (R - iconW)) >> 1) - halfIconW) + (R - iconW);
        iconY = (((B - iconY) >> 1) - halfIconH) + iconY;
    } else if (mode == 1) {
        // Icon on the LEFT, text to its right.
        iconX = L;
        iconY = T;
        textX = gap + L + iconW;
        if (R < textX + textW) textX = R - textW;
        int ty = T;
        if (B < T + textH) ty = B - textH;
        switch (align) {
        case 1: case 4: case 7: textX = (((R - textX) >> 1) - halfTextW) + textX; break;
        case 2: case 5: case 8: textX = R - textW;                                break;
        default: break;
        }
        if (align > 2) {
            if (align < 6)      ty = (((B - ty) >> 1) - halfTextH) + ty;
            else if (align < 9) ty = B - textH;
        }
        textY = ty;
        if (B < T + iconH) iconY = B - iconH;
        iconY = (((B - iconY) >> 1) - halfIconH) + iconY;
    } else if (mode == 3) {
        // Icon ABOVE, text below it.
        iconX = L;
        iconY = T;
        textX = L;
        int ty = gap + T + iconH;
        if (R < L + textW) textX = R - textW;
        if (B < ty + textH) ty = B - textH;
        switch (align) {
        case 1: case 4: case 7: textX = (((R - textX) >> 1) - halfTextW) + textX; break;
        case 2: case 5: case 8: textX = R - textW;                                break;
        default: break;
        }
        if (align > 2) {
            if (align < 6)      ty = (((B - ty) >> 1) - halfTextH) + ty;
            else if (align < 9) ty = B - textH;
        }
        textY = ty;
        if (R < L + iconW) iconX = R - iconW;
        iconX = (((R - iconX) >> 1) - halfIconW) + iconX;
    } else {
        // Mode 2 (and anything else): icon BELOW, text above it.
        iconX = L;
        textX = L;
        int top = T;
        const int textBottom = (B - gap) - iconH;
        if (R < L + textW) textX = R - textW;
        if (textBottom < T + textH) top = textBottom - textH;
        switch (align) {
        case 1: case 4: case 7: textX = (((R - textX) >> 1) - halfTextW) + textX; break;
        case 2: case 5: case 8: textX = R - textW;                                break;
        default: break;
        }
        int ty = top;
        if (align > 2) {
            if (align < 6)      ty = (((textBottom - top) >> 1) - halfTextH) + top;
            else if (align < 9) ty = textBottom - textH;
        }
        textY = ty;
        if (R < L + iconW) iconX = R - iconW;
        iconX = (((R - iconX) >> 1) - halfIconW) + iconX;
        iconY = (((B - (B - iconH)) >> 1) - halfIconH) + (B - iconH);
    }

    ctx.AddSprite(_sprite, iconX, iconY, iconX + iconW, iconH + iconY, _iconColor);

    if (!hasText) return;

    const int baselineAdj = font->_lineHeight - font->_baseLine;

    if (_drawShadow != 0) {
        ctx.AddFont(Object::ExternalPtr<Font>(font), AsString(_cachedText), _wrap != 0,
                    _lineSpacing, _justify, _maxWidth,
                    textX + 1, baselineAdj + 1 + textY, 0xff000000u);
    }
    ctx.AddFont(Object::ExternalPtr<Font>(font), AsString(_cachedText), _wrap != 0,
                _lineSpacing, _justify, _maxWidth,
                textX, baselineAdj + textY,
                rtuielem::ComputeColor(baseColor, alpha));   // the inlined helper,
                                                             // not Element's member
}

// ============================================================================
// ?SetColor@LabelElement@UI@@UEAAXI@Z   0x180042970
// ============================================================================
// Two stores and no dirty walk: the icon colour AND `Element::_color`. It does
// NOT call `Element::SetColor` — it writes +0x38 itself.
void LabelElement::SetColor(uint32_t color) {
    _iconColor = color;
    _color     = color;
}

}  // namespace UI

// ============================================================================
// Constructor / destructor — DOOR BODIES (no real ctor/dtor, B4b-2)
// ============================================================================
extern "C" void* RtUi_LabelElement_CopyCtor(void* selfv, const void* otherv) {
    UI::LabelElement* self = reinterpret_cast<UI::LabelElement*>(selfv);               // B.3.6 bridge
    const UI::LabelElement* other = reinterpret_cast<const UI::LabelElement*>(otherv); // B.3.6 bridge
    RtAddr<TextCopyCtorFn>(0x1800420d0ull)(selfv, otherv);
    *reinterpret_cast<void**>(selfv) = VtLabelElement();
    // The sprite goes over as two raw qwords (pad included).
    __builtin_memcpy(&self->_sprite, &other->_sprite, sizeof(UI::Sprite));
    self->_align        = other->_align;
    self->_mode         = other->_mode;
    self->_minTextWidth = other->_minTextWidth;
    self->_gap          = other->_gap;
    self->_iconWidth    = other->_iconWidth;
    self->_iconHeight   = other->_iconHeight;
    self->_iconColor    = other->_iconColor;
    self->_textWidth    = other->_textWidth;
    return selfv;
}

extern "C" void RtUi_LabelElement_Dtor(void* selfv) {
    *reinterpret_cast<void**>(selfv) = VtLabelElement();
    RtAddr<TextDtorFn>(0x18011eda0ull)(selfv);
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_LabelElement_Type(const void* self) {
    return reinterpret_cast<const UI::LabelElement*>(self)->UI::LabelElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_LabelElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::LabelElement*>(self)->UI::LabelElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_LabelElement_ComputeSize(void* self) {
    reinterpret_cast<UI::LabelElement*>(self)->UI::LabelElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_LabelElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    reinterpret_cast<UI::LabelElement*>(self)->UI::LabelElement::ComputeLayout(x, y, w, h);   // B.3.6 bridge
}
extern "C" void RtUi_LabelElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::LabelElement*>(self)->UI::LabelElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_LabelElement_SetColor(void* self, uint32_t color) {
    reinterpret_cast<UI::LabelElement*>(self)->UI::LabelElement::SetColor(color);   // B.3.6 bridge
}

// ============================================================================
// Registration
// ============================================================================
int RtUiLabelElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@LabelElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_LabelElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x1800427f0ull, (void*)&RtUi_LabelElement_TypeName);

    RegisterRuntimeExport("?Type@LabelElement@UI@@UEBAIXZ",
                          (void*)&RtUi_LabelElement_Type);          ++n;
    RegisterRuntimeAddr(0x1800427d0ull, (void*)&RtUi_LabelElement_Type);

    RegisterRuntimeExport("?ComputeSize@LabelElement@UI@@UEAAXXZ",
                          (void*)&RtUi_LabelElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800d2490ull, (void*)&RtUi_LabelElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@LabelElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_LabelElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800d2330ull, (void*)&RtUi_LabelElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@LabelElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_LabelElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800d1c80ull, (void*)&RtUi_LabelElement_Draw);

    RegisterRuntimeExport("?SetColor@LabelElement@UI@@UEAAXI@Z",
                          (void*)&RtUi_LabelElement_SetColor);      ++n;
    RegisterRuntimeAddr(0x180042970ull, (void*)&RtUi_LabelElement_SetColor);

    RegisterRuntimeExport("??0LabelElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_LabelElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180042a00ull, (void*)&RtUi_LabelElement_CopyCtor);

    RegisterRuntimeExport("??1LabelElement@UI@@UEAA@XZ",
                          (void*)&RtUi_LabelElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800d25f0ull, (void*)&RtUi_LabelElement_Dtor);
    return n;
}
