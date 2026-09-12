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
// ============================================================================
#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kIcon        = 0x100;
constexpr int kIconIndex   = 0x108;
constexpr int kAlign       = 0x110;
constexpr int kMode        = 0x114;
constexpr int kMinTextW    = 0x118;
constexpr int kGap         = 0x11c;
constexpr int kIconW       = 0x120;
constexpr int kIconH       = 0x124;
constexpr int kIconColor   = 0x128;
constexpr int kTextWidth   = 0x12c;

// UI::TextElement
constexpr int kFont        = 0xc0;
constexpr int kShadow      = 0xe8;   // uint8: draw a 1px black shadow copy
constexpr int kWrap        = 0xe9;   // uint8, forwarded to ComputeStringLayout
constexpr int kTextArgA    = 0xec;
constexpr int kTextArgB    = 0xf0;
constexpr int kTextArgC    = 0xf4;
constexpr int kText        = 0xf8;   // System::String

// UI::Font
constexpr int kFontLineHeight = 0x50;
constexpr int kFontBaseline   = 0x54;

RTUI_TYPE_SLOTS(LabelElement)

// Owned by the UI-text agent; reached by address.
//   ?ComputeStringLayout@Font@UI@@ 0x180116fd0
//     (font, const String&, bool, int, int, int, int* outW, int* outH, void*)
using ComputeStringLayoutFn = void (*)(void* font, const void* str, int wrap,
                                       int a, int b, int c,
                                       int* outW, int* outH, void* layout);
// ?AddSprite@SpriteContext@UI@@ (rect form) and ?AddFont@SpriteContext@UI@@
using AddSpriteRectFn = void (*)(void* ctx, const void* sprite,
                                 int l, int t, int r, int b, uint32_t color);
using AddFontFn = void (*)(void* ctx, void* font, const void* str, int wrap,
                           int a, int b, int c, int x, int y, uint32_t color);

using TextCtorFn     = void* (*)(void* self);
using TextCopyCtorFn = void* (*)(void* self, const void* other);
using TextDtorFn     = void  (*)(void* self);

}  // namespace

RTUI_TYPE_BODIES(LabelElement)

namespace {

// The icon's drawn size: the override when positive, else the sprite's own.
// Both arms test the SAME field — the sprite INDEX at +0x108 — for the
// "no sprite" case, not the sheet pointer.
int IconWidth(const void* self) {
    const int32_t ov = I32c(self, kIconW);
    if (ov > 0) return ov;
    if (I32c(self, kIconIndex) < 0) return 0;
    return SpriteW(static_cast<const uint8_t*>(self) + kIcon);
}
int IconHeight(const void* self) {
    const int32_t ov = I32c(self, kIconH);
    if (ov > 0) return ov;
    if (I32c(self, kIconIndex) < 0) return 0;
    return SpriteH(static_cast<const uint8_t*>(self) + kIcon);
}

}  // namespace

// ============================================================================
// ?ComputeSize@LabelElement@UI@@UEAAXXZ   0x1800d2490
// ============================================================================
// Measure the text (only when there IS a font AND a non-empty string), floor
// its width by +0x118, then combine with the icon according to the mode:
//
//   mode >  1  (stacked)   width  = max(iconW, textW)
//                          height = gap + iconH + textH
//   mode <= 1  (side)      width  = gap + iconW + textW
//                          height = max(iconH, textH)
//
// `_textWidth` (+0x12c) is set to the RAW measured width, BEFORE the +0x118
// floor — the floor only affects what is combined with the icon. Draw then
// reads +0x12c, so a label whose minimum width exceeds its text draws the text
// at its natural width inside a wider box.
extern "C" void RtUi_LabelElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    int textW = 0;
    int textH = 0;
    void* font = P(self, kFont);
    if (font != nullptr) {
        void* str = P(self, kText);
        if (str != nullptr && *reinterpret_cast<const int32_t*>(
                                  static_cast<const uint8_t*>(str) - 8) > 0) {
            int w = 0, h = 0;
            RtAddr<ComputeStringLayoutFn>(0x180116fd0ull)(
                font, self + kText, self[kWrap],
                I32(self, kTextArgA), I32(self, kTextArgB), 0,
                &w, &h, nullptr);
            I32(self, kTextWidth) = w;
            textH = h;
            textW = w;
            if (w < I32(self, kMinTextW)) textW = I32(self, kMinTextW);
        }
    }

    const int iconW = IconWidth(self);
    const int iconH = IconHeight(self);

    if (U32(self, kMode) > 1u) {
        int w = iconW;
        if (w < textW) w = textW;
        U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
        I32(self, kElemLayoutWidth)  = w;
        I32(self, kElemLayoutHeight) = I32(self, kGap) + iconH + textH;
        return;
    }

    int h = iconH;
    if (h < textH) h = textH;
    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElemLayoutWidth)  = I32(self, kGap) + iconW + textW;
    I32(self, kElemLayoutHeight) = h;
}

// ============================================================================
// ?ComputeLayout@LabelElement@UI@@UEAAXHHHH@Z   0x1800d2330
// ============================================================================
extern "C" void RtUi_LabelElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    ElementComputeAlignedRect(self, x, y, w, h);
}

// ============================================================================
// ?Draw@LabelElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d1c80
// ============================================================================
// Four layout modes, each placing the icon and the text inside the element's
// rectangle and then aligning the pair by `_align` (+0x110). The four arms
// share a shape but not a body: retail emits them separately, and the
// differences are real — mode 0 aligns the icon on the X axis but pins it to
// the right edge, mode 2 pins it to the bottom, and so on.
//
// TRANSCRIBED LITERALLY, INCLUDING THE COLLAPSED ARMS. Several placements read
// as `((R - (R - w)) >> 1 - (w >> 1)) + (R - w)`, which is algebraically just
// `R - w`; the compiler left the centring expression in because the operands
// were already live. Simplifying it is safe today and stops being a
// transcription, so it is kept as it is emitted.
//
// THE SHADOW PASS IS A SECOND AddFont AT (+1, +1) IN OPAQUE BLACK (0xff000000)
// drawn BEFORE the real text, gated by the byte at +0xe8. Its colour is a
// CONSTANT — it does not go through ComputeColor and does not fade with the
// element's alpha.
//
// The text's Y is `(font.lineHeight - font.baseline) + placedY`, i.e. the
// placement computes a top edge and AddFont wants a baseline.
extern "C" void RtUi_LabelElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    const uint32_t baseColor = U32c(self, kElemColor);
    alpha = DimAlpha(self, alpha);

    void* font = Pc(self, kFont);
    void* str  = Pc(self, kText);

    int textW = 0, textH = 0;
    bool hasText = false;
    if (font != nullptr && str != nullptr &&
        *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(str) - 8) >= 1) {
        textW = I32c(self, kTextWidth);
        textH = I32c(font, kFontLineHeight);
        hasText = true;
    }

    const int iconW = IconWidth(self);
    const int iconH = IconHeight(self);

    const uint32_t mode  = U32c(self, kMode);
    const int      align = I32c(self, kAlign);

    const int halfTextW = textW >> 1;
    const int halfTextH = textH >> 1;
    const int halfIconW = iconW >> 1;
    const int halfIconH = iconH >> 1;

    const int L = I32c(self, kElemLayoutLeft);
    const int T = I32c(self, kElemLayoutTop);
    const int R = I32c(self, kElemLayoutRight);
    const int B = I32c(self, kElemLayoutBottom);
    const int gap = I32c(self, kGap);

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

    RtAddr<AddSpriteRectFn>(0x18011d3c0ull)(
        ctx, self + kIcon, iconX, iconY, iconX + iconW, iconH + iconY,
        U32c(self, kIconColor));

    if (!hasText) return;

    const auto addFont = RtAddr<AddFontFn>(0x18011ccb0ull);
    const int baselineAdj = I32c(font, kFontLineHeight) - I32c(font, kFontBaseline);

    if (self[kShadow] != 0) {
        addFont(ctx, font, self + kText, self[kWrap],
                I32c(self, kTextArgA), I32c(self, kTextArgC), I32c(self, kTextArgB),
                textX + 1, baselineAdj + 1 + textY, 0xff000000u);
    }
    addFont(ctx, font, self + kText, self[kWrap],
            I32c(self, kTextArgA), I32c(self, kTextArgC), I32c(self, kTextArgB),
            textX, baselineAdj + textY, ComputeColor(baseColor, alpha));
}

// ============================================================================
// ?SetColor@LabelElement@UI@@UEAAXI@Z   0x180042970
// ============================================================================
// Two stores and no dirty walk: the icon colour AND `Element::_color`. Note it
// does NOT call `Element::SetColor` — it writes +0x38 itself, which is the same
// thing the base does, and there is no difference to reproduce.
extern "C" void RtUi_LabelElement_SetColor(void* self, uint32_t color) {
    U32(self, kIconColor) = color;
    U32(self, kElemColor) = color;
}

// ============================================================================
// Constructors / destructor
// ============================================================================
// The constructor's +0x114 store is a QWORD of 1, covering `_mode = 1` AND
// `_minTextWidth = 0`; the +0x120 store is a QWORD of -1, covering both icon
// size overrides. Written here as the four fields they cover.
extern "C" void* RtUi_LabelElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<TextCtorFn>(0x18011eb80ull)(self);
    P(self, 0x00) = VtLabelElement();
    *reinterpret_cast<uint64_t*>(self + kIcon) = 0;
    I32(self, kIconIndex) = -1;
    I32(self, kAlign)     = 3;
    U32(self, kMode)      = 1;
    I32(self, kMinTextW)  = 0;
    I32(self, kGap)       = 2;
    I32(self, kIconW)     = -1;
    I32(self, kIconH)     = -1;
    U32(self, kIconColor) = 0xffffffffu;
    I32(self, kTextWidth) = 0;
    return self;
}

extern "C" void* RtUi_LabelElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<TextCopyCtorFn>(0x1800420d0ull)(self, other);
    P(self, 0x00) = VtLabelElement();
    *reinterpret_cast<uint64_t*>(self + 0x100) = *reinterpret_cast<const uint64_t*>(other + 0x100);
    *reinterpret_cast<uint64_t*>(self + 0x108) = *reinterpret_cast<const uint64_t*>(other + 0x108);
    for (int off = kAlign; off <= kTextWidth; off += 4)
        I32(self, off) = I32c(other, off);
    return self;
}

extern "C" void RtUi_LabelElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtLabelElement();
    RtAddr<TextDtorFn>(0x18011eda0ull)(selfv);
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

    RegisterRuntimeExport("??0LabelElement@UI@@QEAA@XZ",
                          (void*)&RtUi_LabelElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1800d2620ull, (void*)&RtUi_LabelElement_Ctor);

    RegisterRuntimeExport("??0LabelElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_LabelElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180042a00ull, (void*)&RtUi_LabelElement_CopyCtor);

    RegisterRuntimeExport("??1LabelElement@UI@@UEAA@XZ",
                          (void*)&RtUi_LabelElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800d25f0ull, (void*)&RtUi_LabelElement_Dtor);
    return n;
}
