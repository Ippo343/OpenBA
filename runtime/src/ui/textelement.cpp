// ============================================================================
// runtime/src/ui/textelement.cpp — UI::TextElement's vtable slots + dtor
// ============================================================================
//
// `??_7TextElement@UI@@6B@` (retail 0x180156b80):
//
//   0  vector deleting dtor  0x1800422f0  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180041ef0  here
//   2  Type                  0x180041ed0  here
//   4  ComputeSize           0x18011f060  here
//   5  ComputeLayout         0x18011f120  here
//   6  Draw                  0x18011f280  here
//
// plus ??1TextElement@UI@@UEAA@XZ (0x18011eda0), here.
//
// NOT HERE, and deliberately:
//   * `??0TextElement@UI@@QEAA@XZ` (0x18011eb80) — being ported for the
//     cutover boot wall by the Description-B batch; this file reaches it by
//     NAME nowhere and by address nowhere, so there is no second owner.
//   * `?CacheText@TextElement@UI@@AEAAXXZ` (0x18011ec10) — already ported,
//     runtime/src/ui/textelement_cachetext.cpp.
//   * `?SetParameter@` / `?SetParameterCount@` / `?SetText@` — lane E's, in
//     widgets_layout.cpp.
//   * `?ComputeStringLayout@Font@UI@@` (0x180116fd0) and
//     `?AddFont@SpriteContext@UI@@` (0x18011ccb0) — the UI-text agent's;
//     reached by address.
//
// ---------------------------------------------------------------------------
// LAYOUT. sizeof == 0x100 (deleting dtor `MOV EDX,0x100`).
// runtime/types/ui/TextElement.h already pinned +0xC8/+0xD0/+0xD4 (the
// Array<System::String> of format parameters), +0xD8 (the UI::String source
// text) and +0xF8 (the cached System::String). This port adds the rest, from
// ComputeSize (0x18011F060), Draw (0x18011F280) and ~TextElement (0x18011EDA0):
//
//   +0xC0  _font        ExternalPtr<UI::Font>.  TextElement.h says "+0xC0..0xC8
//                       is read by neither of this lane's two functions and is
//                       NOT claimed here (LabelElement.h's old table guessed
//                       'font ptr'; that guess is unverified)". IT IS VERIFIED
//                       NOW, and LabelElement.h's guess was right: ComputeSize
//                       loads it into RCX for ComputeStringLayout and Draw
//                       reads its +0x50/+0x54 line metrics.
//   +0xE8  _drawShadow  bool — Draw emits a second, black, +1/+1 pass first
//   +0xE9  _wrap        bool — ComputeStringLayout / AddFont argument 3
//   +0xEC  _lineSpacing int32 — argument 4
//   +0xF0  _maxWidth    int32 — argument 5, AND the clamp ComputeSize applies
//                       to the measured width when it is positive
//   +0xF4  _justify     UI::Font::Justify
//
// ~TextElement releases the cached string at +0xF8 and then destroys the
// parameter array at +0xC8 through 0x180010ab0 — the `~Array` variant that
// destroys ELEMENTS first (it calls 0x180011010 over {data,size} before
// freeing the buffer), which is right because the elements are System::Strings.
// That address is already registered by runtime/src/collection/array_destroy.cpp.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/TextElement.h"

using namespace rtui;

namespace {

constexpr int kFont        = 0xC0;
constexpr int kParams      = 0xC8;
constexpr int kDrawShadow  = 0xE8;
constexpr int kWrap        = 0xE9;
constexpr int kLineSpacing = 0xEC;
constexpr int kMaxWidth    = 0xF0;
constexpr int kJustify     = 0xF4;
constexpr int kCachedText  = 0xF8;

static_assert(kParams     == UI::TextElementOffsets::kParamArrayData, "");
static_assert(kCachedText == UI::TextElementOffsets::kCachedText, "");

constexpr int kFontLineHeight = 0x50;
constexpr int kFontBaseline   = 0x54;

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7TextElement@UI@@6B@"); }

// `?ComputeStringLayout@Font@UI@@QEBAXAEBVString@System@@_NHHW4Justify@12@AEAH3PEAULayout@12@@Z`
using ComputeStringLayoutFn = void (*)(void* font, const void* str, uint8_t wrap,
                                       int32_t lineSpacing, int32_t maxWidth,
                                       int32_t justify, int32_t* w, int32_t* h,
                                       void* layout);

}  // namespace

extern "C" uint32_t RtUi_TextElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_TextElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??1TextElement@UI@@UEAA@XZ   0x18011eda0
extern "C" void RtUi_TextElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    ReleaseStringField(self, kCachedText);
    RtAddr<ArrayDtorFn>(0x180010ab0ull)(B(self, kParams));
    ElementDtor(self);
}

// slot 4 — 0x18011f060
extern "C" void RtUi_TextElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    RtAddr<ComputeStringLayoutFn>(0x180116fd0ull)(
        PTR(self, kFont), B(self, kCachedText), U8(self, kWrap),
        I32(self, kLineSpacing), I32(self, kMaxWidth), I32(self, kJustify),
        reinterpret_cast<int32_t*>(B(self, kElLayoutWidth)),
        reinterpret_cast<int32_t*>(B(self, kElLayoutHeight)),
        nullptr);

    if (I32(self, kElLayoutWidth)  < I32(self, kElMinWidth))
        I32(self, kElLayoutWidth)  = I32(self, kElMinWidth);
    if (I32(self, kElLayoutHeight) < I32(self, kElMinHeight))
        I32(self, kElLayoutHeight) = I32(self, kElMinHeight);

    const int32_t cap = I32(self, kMaxWidth);
    if (cap > 0 && cap < I32(self, kElLayoutWidth))
        I32(self, kElLayoutWidth) = cap;

    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
}

// slot 5 — 0x18011f120
extern "C" void RtUi_TextElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                               int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    int32_t w = I32(self, kElLayoutWidth);
    int32_t h = I32(self, kElLayoutHeight);
    const int32_t align = I32(self, kElAlignment);

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;

    if ((U32(self, kElFlags) & kFlagStretchX) != 0) w = aw;
    if ((U32(self, kElFlags) & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - I32(self, kElRightPad)) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutRight)  = px + w;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutBottom) = py + h;
}

// slot 6 — 0x18011f280
extern "C" void RtUi_TextElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    const uint32_t raw = U32(me, kElColor);
    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;

    void* font = PTR(me, kFont);
    const int32_t baseline = I32(font, kFontLineHeight) - I32(font, kFontBaseline);

    if (U8(me, kDrawShadow) != 0) {
        RtAddr<AddFontFn>(0x18011ccb0ull)(
            ctx, font, B(me, kCachedText), U8(me, kWrap),
            I32(me, kLineSpacing), I32(me, kJustify), I32(me, kMaxWidth),
            I32(me, kElLayoutLeft) + 1,
            I32(me, kElLayoutTop) + 1 + baseline,
            0xff000000u);
    }

    RtAddr<AddFontFn>(0x18011ccb0ull)(
        ctx, font, B(me, kCachedText), U8(me, kWrap),
        I32(me, kLineSpacing), I32(me, kJustify), I32(me, kMaxWidth),
        I32(me, kElLayoutLeft),
        baseline + I32(me, kElLayoutTop),
        ModulateColor(raw, alpha));
}

// ============================================================================
int RtUiTextElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@TextElement@UI@@UEBAIXZ",
                          (void*)&RtUi_TextElement_Type);            ++n;
    RegisterRuntimeAddr(0x180041ed0ull, (void*)&RtUi_TextElement_Type);

    RegisterRuntimeExport("?TypeName@TextElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_TextElement_TypeName);        ++n;
    RegisterRuntimeAddr(0x180041ef0ull, (void*)&RtUi_TextElement_TypeName);

    RegisterRuntimeExport("??1TextElement@UI@@UEAA@XZ",
                          (void*)&RtUi_TextElement_Dtor);            ++n;
    RegisterRuntimeAddr(0x18011eda0ull, (void*)&RtUi_TextElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@TextElement@UI@@UEAAXXZ",
                          (void*)&RtUi_TextElement_ComputeSize);     ++n;
    RegisterRuntimeAddr(0x18011f060ull, (void*)&RtUi_TextElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@TextElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_TextElement_ComputeLayout);   ++n;
    RegisterRuntimeAddr(0x18011f120ull, (void*)&RtUi_TextElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@TextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_TextElement_Draw);            ++n;
    RegisterRuntimeAddr(0x18011f280ull, (void*)&RtUi_TextElement_Draw);

    return n;
}
