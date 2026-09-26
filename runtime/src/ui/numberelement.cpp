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
//
// ENDGAME B.3.6 block C3: the five slot bodies and CacheText are
// `UI::NumberElement` members; the doors delegate with a QUALIFIED call. The
// ctor/dtor stay DOOR bodies (no real ctor), and the ctor door calls the
// CacheText member through the bridge. Font / SpriteContext calls are direct.
// ============================================================================

#include <atomic>

#include "element_common.h"
#include "../../types/ui/NumberElement.h"
#include "../../types/ui/Font.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/system/String.h"

using namespace rtuielem;

namespace {

std::atomic<void*> g_typeInfoSlot{nullptr};   // RULE B6-5

const uint8_t* TypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfoSlot, "?_typeInfo@NumberElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7NumberElement@UI@@6B@"); }

using CtorFn = void* (*)(void* self);
using DtorFn = void  (*)(void* self);
using MemFreeFn = void (*)(void* p);
// `?Format@String@System@@SA?AV12@PEBDZZ` — hidden return slot first.
using FormatIntFn    = void* (*)(void* ret, const char* fmt, int64_t v);
using FormatDoubleFn = void* (*)(void* ret, const char* fmt, double v);

void MemFree(void* p) { RtAddr<MemFreeFn>(0x1800c7ad0ull)(p); }

// The inlined String::operator= and the two releases, retail's asymmetric
// shape (see widget_common.h, which these mirror).
void AssignString(char*& field, char* nv) {
    char* old = field;
    if (old == nv) return;
    if (old != nullptr) {
        int32_t* rc = reinterpret_cast<int32_t*>(old - 4);
        if (--*rc == 0) {
            field = field - 8;
            MemFree(field);
        } else {
            field = nullptr;
        }
    }
    field = nv;
    if (nv != nullptr) ++*reinterpret_cast<int32_t*>(nv - 4);
}
void ReleaseStringLocal(void** slot) {
    uint8_t* p = static_cast<uint8_t*>(*slot);
    if (p == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(p - 4);
    if (--*rc == 0) { *slot = p - 8; MemFree(*slot); }
}
void ReleaseStringField(char*& field) {
    char* p = field;
    if (p == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(p - 4);
    if (--*rc == 0) {
        field = field - 8;
        MemFree(field);
    } else {
        field = nullptr;
    }
}

}  // namespace

namespace UI {

uint32_t NumberElement::Type() const { return TypeIdOf(TypeInfo()); }
const char* NumberElement::TypeName() const { return TypeNameOf(TypeInfo()); }

// Non-virtual, EMPTY: the dtor door keeps retail's body.
NumberElement::~NumberElement() {}

// ---------------------------------------------------------------------------
// ?CacheText@NumberElement@UI@@AEAAXXZ   0x1801189f0
//
// The four format literals live at 0x180159144 / 0x180154848 / 0x180159148 /
// 0x180159150 and are reproduced as string literals — a rodata literal is the
// one kind of constant that CAN be retyped, because its bytes are its value.
// ---------------------------------------------------------------------------
void NumberElement::CacheText() {
    void* tmp = nullptr;
    if (_mode == 0) {
        const char* fmt = (_showSign != 0) ? "%+i" : "%i";
        RtAddr<FormatIntFn>(0x1800c9730ull)(&tmp, fmt, _intValue);
        AssignString(_cachedText, static_cast<char*>(tmp));
    } else {
        const char* fmt = (_showSign != 0) ? "%+.2f" : "%.2f";
        RtAddr<FormatDoubleFn>(0x1800c9730ull)(&tmp, fmt, static_cast<double>(_floatValue));
        AssignString(_cachedText, static_cast<char*>(tmp));
    }
    ReleaseStringLocal(&tmp);
}

// slot 4 — 0x180119000
void NumberElement::ComputeSize() {
    if ((_flags & kElementFlagNeedsSizeCalc) == 0) return;

    Font* font = _font;
    _layoutWidth  = 0;      // one qword store clears both
    _layoutHeight = 0;

    const char* text = _cachedText;
    int32_t len = 0;
    if (text != nullptr)
        len = *reinterpret_cast<const int32_t*>(text - 8);     // System::String length

    _layoutWidth  = font->ComputeLineLayout(text, len, 0, 0, -1, 0, nullptr);
    _layoutHeight = font->_lineHeight;
    _flags &= ~kElementFlagNeedsSizeCalc;
}

// slot 5 — 0x180119080
void NumberElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
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

// slot 6 — 0x1801191e0
void NumberElement::Draw(SpriteContext& ctx, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(ctx, alpha);                              // virtual, slot 6

    const uint32_t flags = _flags;
    // `(flags & 0x1800000) == 0 || (flags >> 0x18 & 1) != 0` — draw when the
    // element is fully enabled, OR when it is SYSTEM-disabled (bit 24), which
    // still paints; a plain Disabled (bit 23) alone suppresses it.
    if ((flags & 0x01800000u) != 0 && ((flags >> 24) & 1) == 0) return;

    const uint32_t color = _color;
    if ((flags & kElementFlagDimColor) != 0) alpha >>= 1;

    Font* font = _font;
    ctx.AddFont(Object::ExternalPtr<Font>(font),
                *reinterpret_cast<const System::String*>(&_cachedText),
                false, 0, 0, -1,
                _layoutLeft,
                (font->_lineHeight - font->_baseLine) + _layoutTop,
                rtuielem::ComputeColor(color, alpha));
}

}  // namespace UI

// ??0NumberElement@UI@@QEAA@XZ   0x180118c70 — a DOOR BODY (no real ctor).
// RETURNS `this`, as every MSVC constructor does in RAX: NumberDescription::
// CreateInstance (0x180118f81) keeps RAX as the element and hands it to
// ElementDescription::CreateInstance, which reads its +0x10.
extern "C" void* RtUi_NumberElement_Ctor(void* selfv) {
    RtAddr<CtorFn>(0x180115e50ull)(selfv);                     // ??0Element
    UI::NumberElement* self = reinterpret_cast<UI::NumberElement*>(selfv);   // B.3.6 bridge
    *reinterpret_cast<void**>(selfv) = Vtable();
    self->_font       = nullptr;
    self->_mode       = 0;
    self->_cachedText = nullptr;
    self->_intValue   = 0;
    self->_floatValue = 0.0f;
    self->_showSign   = 0;
    self->UI::NumberElement::CacheText();
    return selfv;
}

// ??1NumberElement@UI@@UEAA@XZ   0x180118ce0 — a DOOR BODY.
extern "C" void RtUi_NumberElement_Dtor(void* selfv) {
    UI::NumberElement* self = reinterpret_cast<UI::NumberElement*>(selfv);   // B.3.6 bridge
    *reinterpret_cast<void**>(selfv) = Vtable();
    ReleaseStringField(self->_cachedText);
    RtAddr<DtorFn>(0x1801161a0ull)(selfv);                     // ??1Element
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_NumberElement_Type(const void* self) {
    return reinterpret_cast<const UI::NumberElement*>(self)->UI::NumberElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_NumberElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::NumberElement*>(self)->UI::NumberElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_NumberElement_CacheText(void* self) {
    reinterpret_cast<UI::NumberElement*>(self)->UI::NumberElement::CacheText();   // B.3.6 bridge
}
extern "C" void RtUi_NumberElement_ComputeSize(void* self) {
    reinterpret_cast<UI::NumberElement*>(self)->UI::NumberElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_NumberElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                 int32_t right, int32_t bottom) {
    reinterpret_cast<UI::NumberElement*>(self)->UI::NumberElement::ComputeLayout(x, y, right, bottom);   // B.3.6 bridge
}
extern "C" void RtUi_NumberElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::NumberElement*>(self)->UI::NumberElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
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
