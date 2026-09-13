// ============================================================================
// runtime/src/ui/numberelement.cpp — UI::NumberElement, the whole class
// ============================================================================
//
// `??_7NumberElement@UI@@6B@` (retail 0x1801574d0):
//
//   0  vector deleting dtor  0x18004ab80  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x18004a820  here
//   2  Type                  0x18004a800  here
//   4  ComputeSize           0x180119000  here
//   5  ComputeLayout         0x180119080  here
//   6  Draw                  0x1801191e0  here
//
// plus ??0NumberElement (0x180118c70), ??1NumberElement (0x180118ce0) and the
// private `?CacheText@NumberElement@UI@@AEAAXXZ` (0x1801189f0) — which IS this
// file's, unlike TextElement's same-named 0x18011ec10.
//
// The layout is runtime/types/ui/NumberElement.h and is complete; this port is
// what let that header's struct catch up with its own banner.
//
// NOT HERE: `?ComputeLineLayout@Font@UI@@` (0x180116c70) and
// `?AddFont@SpriteContext@UI@@` (0x18011ccb0), both on the UI non-slot callee
// list; reached by address.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/NumberElement.h"

using namespace rtui;

namespace {

constexpr int kFont       = 0xC0;
constexpr int kMode       = 0xC8;
constexpr int kCachedText = 0xD0;
constexpr int kIntValue   = 0xD8;
constexpr int kFloatValue = 0xDC;
constexpr int kShowSign   = 0xE0;

static_assert(kFont       == offsetof(UI::NumberElement, _font), "");
static_assert(kMode       == offsetof(UI::NumberElement, _mode), "");
static_assert(kCachedText == offsetof(UI::NumberElement, _cachedText), "");
static_assert(kIntValue   == offsetof(UI::NumberElement, _intValue), "");
static_assert(kFloatValue == offsetof(UI::NumberElement, _floatValue), "");
static_assert(kShowSign   == offsetof(UI::NumberElement, _showSign), "");

// UI::Font fields the two text bodies read. +0x50 is the line height and
// +0x54 the ascent/descent correction Draw subtracts from it; both are read
// exactly this way by TextElement::Draw as well.
constexpr int kFontLineHeight = 0x50;
constexpr int kFontBaseline   = 0x54;

// System::String block header: [-8] length, [-4] refcount (String.h).
constexpr int kStrLenFromBody = -8;

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@NumberElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7NumberElement@UI@@6B@"); }

// `?ComputeLineLayout@Font@UI@@QEBAHPEBDHHHHHPEAULayout@12@@Z`  0x180116c70
using ComputeLineLayoutFn = int32_t (*)(void* font, const char* text, int32_t len,
                                        int32_t a, int32_t b, int32_t c, int32_t d,
                                        void* layout);
// `?Format@String@System@@SA?AV12@PEBDZZ` — hidden return slot first.
using FormatIntFn    = void* (*)(void* ret, const char* fmt, int64_t v);
using FormatDoubleFn = void* (*)(void* ret, const char* fmt, double v);

}  // namespace

extern "C" uint32_t RtUi_NumberElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_NumberElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ---------------------------------------------------------------------------
// ?CacheText@NumberElement@UI@@AEAAXXZ   0x1801189f0
//
// The four format literals live at 0x180159144 / 0x180154848 / 0x180159148 /
// 0x180159150 and are reproduced as string literals — a rodata literal is the
// one kind of constant that CAN be retyped, because its bytes are its value.
// ---------------------------------------------------------------------------
extern "C" void RtUi_NumberElement_CacheText(void* self) {
    void* tmp = nullptr;
    if (I32(self, kMode) == 0) {
        const char* fmt = (U8(self, kShowSign) != 0) ? "%+i" : "%i";
        RtAddr<FormatIntFn>(0x1800c9730ull)(&tmp, fmt, I32(self, kIntValue));
        AssignStringField(self, kCachedText, tmp);
    } else {
        const char* fmt = (U8(self, kShowSign) != 0) ? "%+.2f" : "%.2f";
        RtAddr<FormatDoubleFn>(0x1800c9730ull)(
            &tmp, fmt, static_cast<double>(F32(self, kFloatValue)));
        AssignStringField(self, kCachedText, tmp);
    }
    ReleaseStringLocal(&tmp);
}

// ??0NumberElement@UI@@QEAA@XZ   0x180118c70
extern "C" void RtUi_NumberElement_Ctor(void* self) {
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    PTR(self, kFont) = nullptr;
    I32(self, kMode) = 0;
    PTR(self, kCachedText) = nullptr;
    I32(self, kIntValue)   = 0;
    F32(self, kFloatValue) = 0.0f;
    U8(self, kShowSign)    = 0;
    RtUi_NumberElement_CacheText(self);
}

// ??1NumberElement@UI@@UEAA@XZ   0x180118ce0
extern "C" void RtUi_NumberElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    ReleaseStringField(self, kCachedText);
    ElementDtor(self);
}

// slot 4 — 0x180119000
extern "C" void RtUi_NumberElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    void* font = PTR(self, kFont);
    I32(self, kElLayoutWidth)  = 0;      // one qword store clears both
    I32(self, kElLayoutHeight) = 0;

    const char* text = static_cast<const char*>(PTR(self, kCachedText));
    int32_t len = 0;
    if (text != nullptr)
        len = *reinterpret_cast<const int32_t*>(text + kStrLenFromBody);

    I32(self, kElLayoutWidth) =
        RtAddr<ComputeLineLayoutFn>(0x180116c70ull)(font, text, len, 0, 0, -1, 0, nullptr);
    I32(self, kElLayoutHeight) = I32(font, kFontLineHeight);
    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
}

// slot 5 — 0x180119080
extern "C" void RtUi_NumberElement_ComputeLayout(void* self, int32_t x, int32_t y,
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

// slot 6 — 0x1801191e0
extern "C" void RtUi_NumberElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    const uint32_t flags = U32(me, kElFlags);
    // `(flags & 0x1800000) == 0 || (flags >> 0x18 & 1) != 0` — draw when the
    // element is fully enabled, OR when it is SYSTEM-disabled (bit 24), which
    // still paints; a plain Disabled (bit 23) alone suppresses it.
    if ((flags & 0x01800000u) != 0 && ((flags >> 24) & 1) == 0) return;

    const uint32_t color = U32(me, kElColor);
    if ((flags & kFlagDimColor) != 0) alpha >>= 1;

    void* font = PTR(me, kFont);
    RtAddr<AddFontFn>(0x18011ccb0ull)(
        ctx, font, B(me, kCachedText), 0, 0, 0, -1,
        I32(me, kElLayoutLeft),
        (I32(font, kFontLineHeight) - I32(font, kFontBaseline)) + I32(me, kElLayoutTop),
        ModulateColor(color, alpha));
}

// ============================================================================
int RtUiNumberElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@NumberElement@UI@@UEBAIXZ",
                          (void*)&RtUi_NumberElement_Type);            ++n;
    RegisterRuntimeAddr(0x18004a800ull, (void*)&RtUi_NumberElement_Type);

    RegisterRuntimeExport("?TypeName@NumberElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_NumberElement_TypeName);        ++n;
    RegisterRuntimeAddr(0x18004a820ull, (void*)&RtUi_NumberElement_TypeName);

    RegisterRuntimeExport("??0NumberElement@UI@@QEAA@XZ",
                          (void*)&RtUi_NumberElement_Ctor);            ++n;
    RegisterRuntimeAddr(0x180118c70ull, (void*)&RtUi_NumberElement_Ctor);

    RegisterRuntimeExport("??1NumberElement@UI@@UEAA@XZ",
                          (void*)&RtUi_NumberElement_Dtor);            ++n;
    RegisterRuntimeAddr(0x180118ce0ull, (void*)&RtUi_NumberElement_Dtor);

    RegisterRuntimeExport("?CacheText@NumberElement@UI@@AEAAXXZ",
                          (void*)&RtUi_NumberElement_CacheText);       ++n;
    RegisterRuntimeAddr(0x1801189f0ull, (void*)&RtUi_NumberElement_CacheText);

    RegisterRuntimeExport("?ComputeSize@NumberElement@UI@@UEAAXXZ",
                          (void*)&RtUi_NumberElement_ComputeSize);     ++n;
    RegisterRuntimeAddr(0x180119000ull, (void*)&RtUi_NumberElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@NumberElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_NumberElement_ComputeLayout);   ++n;
    RegisterRuntimeAddr(0x180119080ull, (void*)&RtUi_NumberElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@NumberElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_NumberElement_Draw);            ++n;
    RegisterRuntimeAddr(0x1801191e0ull, (void*)&RtUi_NumberElement_Draw);

    return n;
}
