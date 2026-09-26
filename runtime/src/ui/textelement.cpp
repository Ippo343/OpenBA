// ============================================================================
// runtime/src/ui/textelement.cpp — UI::TextElement's vtable slots + dtor
// ============================================================================
//
// `??_7TextElement@UI@@6B@` (retail 0x180156b80):
//
//   0  vector deleting dtor  0x1800422f0  -> deleting_dtor.cpp row
//   1  TypeName              0x180041ef0  here
//   2  Type                  0x180041ed0  here
//   4  ComputeSize           0x18011f060  here
//   5  ComputeLayout         0x18011f120  here
//   6  Draw                  0x18011f280  here
//
// plus ??1TextElement@UI@@UEAA@XZ (0x18011eda0), here (a DOOR body).
//
// ENDGAME B.3.6 block C3: the five slot bodies are `UI::TextElement` members
// (runtime/types/ui/TextElement.h carries the layout, pinned from the ctor
// 0x18011eb80). The registered doors delegate with a QUALIFIED call. Calls onto
// `UI::Font` and `UI::SpriteContext` are direct member calls now (block D made
// both real classes); the border and slot-4 dispatches are C++ virtual calls
// (they read the object's MINTED table, exactly as retail's `CALL [RAX+..]`).
//
// NOT HERE, and deliberately:
//   * `??0TextElement@UI@@QEAA@XZ` (0x18011eb80) — element_bodies_b5.cpp.
//   * `?CacheText@` / `?SetParameterCount@` — textelement_cachetext.cpp.
//   * `?SetParameter@` / `?SetText@` — widgets_layout.cpp's doors (fragment pass).
//
// ~TextElement releases the cached string at +0xF8 and then destroys the
// parameter array at +0xC8 through 0x180010ab0 — the `~Array` variant that
// destroys ELEMENTS first (it calls 0x180011010 over {data,size} before
// freeing the buffer), which is right because the elements are System::Strings.
// That address is already registered by runtime/src/collection/array_destroy.cpp.
// ============================================================================

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "element_common.h"   // rtuielem::AtomicExport / ComputeColor, ResolveAddr
#include "../../types/ui/TextElement.h"
#include "../../types/ui/Font.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/system/String.h"

namespace {

std::atomic<void*> g_typeInfoSlot{nullptr};   // RULE B6-5

const uint8_t* TypeInfo() {
    return static_cast<const uint8_t*>(rtuielem::AtomicExport(
        g_typeInfoSlot, "?_typeInfo@TextElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7TextElement@UI@@6B@"); }

using ArrayDtorFn = void (*)(void* hdr);
using ElementDtorFn = void (*)(void* self);
using MemFreeFn = void (*)(void* p);

// System::String is ONE char* (runtime/types/system/String.h): the member
// fields are spelled `char*`, so the Font/SpriteContext calls see them as the
// String they are.
const System::String& AsString(char* const& field) {
    return *reinterpret_cast<const System::String*>(&field);
}

}  // namespace

namespace UI {

// slot 2 — 0x180041ed0 / slot 1 — 0x180041ef0: the minted BaseTypeInfo's first
// dword and its +8 name. TypeName is the key function (GCC emits the table here;
// nothing stamps it).
uint32_t TextElement::Type() const { return rtuielem::TypeIdOf(TypeInfo()); }
const char* TextElement::TypeName() const { return rtuielem::TypeNameOf(TypeInfo()); }

// Non-virtual, EMPTY: the dtor door below keeps retail's body.
TextElement::~TextElement() {}

// slot 4 — 0x18011f060
void TextElement::ComputeSize() {
    if ((_flags & kElementFlagNeedsSizeCalc) == 0) return;

    _font->ComputeStringLayout(AsString(_cachedText), _wrap != 0, _lineSpacing,
                               _maxWidth, _justify, _layoutWidth, _layoutHeight,
                               nullptr);

    if (_layoutWidth  < _minWidth)  _layoutWidth  = _minWidth;
    if (_layoutHeight < _minHeight) _layoutHeight = _minHeight;

    const int32_t cap = _maxWidth;
    if (cap > 0 && cap < _layoutWidth) _layoutWidth = cap;

    _flags &= ~kElementFlagNeedsSizeCalc;
}

// slot 5 — 0x18011f120
void TextElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
    if (Element* border = static_cast<Element*>(_border))
        border->ComputeLayout(x, y, right, bottom);            // virtual, slot 5

    ComputeSize();                                             // virtual, slot 4

    int32_t w = _layoutWidth;
    int32_t h = _layoutHeight;
    const int32_t align = _alignment;

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + _leftPad;
    int32_t py = y + _topPad;
    const int32_t aw = (right  - _rightPad)  - px;
    const int32_t ah = (bottom - _bottomPad) - py;

    if ((_flags & kElementFlagAlignArg1) != 0) w = aw;
    if ((_flags & kElementFlagAlignArg2) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - _rightPad) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - _bottomPad) - h;
    }

    _layoutRight  = px + w;
    _layoutTop    = py;
    _layoutLeft   = px;
    _layoutBottom = py + h;
}

// slot 6 — 0x18011f280
void TextElement::Draw(SpriteContext& ctx, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(ctx, alpha);                              // virtual, slot 6

    const uint32_t raw = _color;
    if ((_flags & kElementFlagDimColor) != 0) alpha >>= 1;

    Font* font = _font;
    const int32_t baseline = font->_lineHeight - font->_baseLine;

    if (_drawShadow != 0) {
        ctx.AddFont(Object::ExternalPtr<Font>(font), AsString(_cachedText), _wrap != 0,
                    _lineSpacing, _justify, _maxWidth,
                    _layoutLeft + 1, _layoutTop + 1 + baseline,
                    0xff000000u);
    }

    ctx.AddFont(Object::ExternalPtr<Font>(font), AsString(_cachedText), _wrap != 0,
                _lineSpacing, _justify, _maxWidth,
                _layoutLeft, baseline + _layoutTop,
                rtuielem::ComputeColor(raw, alpha));
}

}  // namespace UI

// ??1TextElement@UI@@UEAA@XZ   0x18011eda0 — a DOOR BODY (no real dtor).
extern "C" void RtUi_TextElement_Dtor(void* selfv) {
    UI::TextElement* self = reinterpret_cast<UI::TextElement*>(selfv);   // B.3.6 bridge
    *reinterpret_cast<void**>(selfv) = Vtable();

    char*& cached = self->_cachedText;                         // retail's asymmetric release
    if (cached != nullptr) {
        int32_t* rc = reinterpret_cast<int32_t*>(cached - 4);
        if (--*rc == 0) {
            cached = cached - 8;
            rtuielem::RtAddr<MemFreeFn>(0x1800c7ad0ull)(cached);
        } else {
            cached = nullptr;
        }
    }
    rtuielem::RtAddr<ArrayDtorFn>(0x180010ab0ull)(&self->_params);
    rtuielem::RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);    // ??1Element
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_TextElement_Type(const void* self) {
    return reinterpret_cast<const UI::TextElement*>(self)->UI::TextElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_TextElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::TextElement*>(self)->UI::TextElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_TextElement_ComputeSize(void* self) {
    reinterpret_cast<UI::TextElement*>(self)->UI::TextElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_TextElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                               int32_t right, int32_t bottom) {
    reinterpret_cast<UI::TextElement*>(self)->UI::TextElement::ComputeLayout(x, y, right, bottom);   // B.3.6 bridge
}
extern "C" void RtUi_TextElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::TextElement*>(self)->UI::TextElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
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
