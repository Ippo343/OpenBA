// ============================================================================
// runtime/src/ui/sliderelement.cpp — UI::SliderElement, the whole class
// ============================================================================
//
// `??_7SliderElement@UI@@6B@` (retail 0x180157580):
//
//   0  vector deleting dtor  0x18004a180  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180049d20  here
//   2  Type                  0x180049d00  here
//   4  ComputeSize           0x1800d7380  here
//   5  ComputeLayout         0x1800d6fc0  here
//   6  Draw                  0x1800d6d40  here
//   7  Update                0x1800d67e0  here
//
// plus ??0SliderElement (0x1800d7570) and ??1SliderElement (0x180049ee0).
//
// NOT HERE: `?UpdateNumber@SliderElement@UI@@AEAAXXZ` (0x1800d6c20) — on the
// UI non-slot callee list; Update tail-calls it by address. Nor
// ?GetSliderBounds@ (0x1800d7270) or ?SetParameter@ (0x1800d6ca0).
//
// LAYOUT: runtime/types/ui/SliderElement.h (sizeof 0x130, every field a
// store in the ctor at 0x1800d7570).
//
// ENDGAME B.3.6 block C5: the six slot bodies MOVED into the members of the
// real `UI::SliderElement`; each registered door is a one-line QUALIFIED
// delegation. The ctor / dtor stay doors (B4b-2).
//
// ~SliderElement destroys nothing and does NOT restamp the vtable — five
// instructions at 0x180049ee0, a bare tail jump to ~Element.
// ============================================================================

#include <atomic>

#include "widget_common.h"
#include "../../types/ui/SliderElement.h"

using namespace rtui;

namespace {

constexpr int kTrackStart  = 0xC0;
constexpr int kTrackMiddle = 0xD0;
constexpr int kTrackEnd    = 0xE0;
constexpr int kThumb       = 0xF0;
constexpr int kThumbHot    = 0x100;
constexpr int kTrackOffY   = 0x110;
constexpr int kThumbMargin = 0x114;
constexpr int kMinimum     = 0x118;
constexpr int kMaximum     = 0x11C;
constexpr int kSteps       = 0x120;
constexpr int kHovered     = 0x124;
constexpr int kValue       = 0x128;
static_assert(kTrackStart  == offsetof(UI::SliderElement, _trackStart), "");
static_assert(kThumbHot    == offsetof(UI::SliderElement, _thumbHot), "");
static_assert(kTrackOffY   == offsetof(UI::SliderElement, _trackOffsetY), "");
static_assert(kThumbMargin == offsetof(UI::SliderElement, _thumbMargin), "");
static_assert(kMinimum     == offsetof(UI::SliderElement, _minimum), "");
static_assert(kMaximum     == offsetof(UI::SliderElement, _maximum), "");
static_assert(kSteps       == offsetof(UI::SliderElement, _steps), "");
static_assert(kHovered     == offsetof(UI::SliderElement, _hovered), "");
static_assert(kValue       == offsetof(UI::SliderElement, _value), "");

constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

struct WidgetEvent {
    void*    dialog;
    void*    element;
    int32_t  kind;
    int32_t  elementId;
    int32_t  mouseX;
    int32_t  mouseY;
    int32_t  value;
    int32_t  pad24;
    int64_t  tail;
};
static_assert(sizeof(WidgetEvent) == 0x30, "");

// RULE B6-5: namespace-scope atomics, never a function-local first-call cache.
void* AtomicExport(std::atomic<void*>& slot, const char* mangled) {
    void* p = slot.load(std::memory_order_acquire);
    if (p == nullptr) {
        void* scratch = nullptr;
        p = RtResolveCached(mangled, &scratch);
        if (p != nullptr) slot.store(p, std::memory_order_release);
    }
    return p;
}
std::atomic<void*> g_typeInfo{nullptr};
std::atomic<void*> g_vtable{nullptr};
const uint8_t* SliderTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfo, "?_typeInfo@SliderElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* VtSliderElement() { return AtomicExport(g_vtable, "??_7SliderElement@UI@@6B@"); }

// ?UpdateNumber@SliderElement@UI@@AEAAXXZ (0x1800d6c20) — its body is
// element_bodies_b5.cpp's door (fragment pass); reached by address.
using UpdateNumberFn = void (*)(void* self);

int32_t SpriteW(const ::Sprite& s) { return SpriteWidth(&s, 0); }
int32_t SpriteH(const ::Sprite& s) { return SpriteHeight(&s, 0); }

// The thumb travel, computed identically by ComputeLayout, Draw and Update:
//   span = (_layoutRight - 2*(thumbW/2 + _thumbMargin)) - _layoutLeft
//   pos  = span * ((_value - _minimum) / (_maximum - _minimum))      (0 if flat)
int32_t ThumbOffset(const UI::SliderElement& s, int32_t thumbW) {
    const float range = s._maximum - s._minimum;
    if (range == 0.0f) return 0;
    const int32_t span = (s._layoutRight + ((thumbW >> 1) + s._thumbMargin) * -2)
                         - s._layoutLeft;
    return static_cast<int32_t>(static_cast<float>(span)
                                * ((s._value - s._minimum) / range));
}

void QueueEvent(UI::SliderElement* self, void* ui,
                Object::InternalPtr<UI::DialogInstance>* dialog,
                const UI::Element::UpdateState& state) {
    WidgetEvent ev;
    ev.dialog    = dialog->Get();
    ev.element   = self;
    ev.kind      = 0x17;
    ev.elementId = self->_elementId;
    ev.mouseX    = state.mouseX;
    ev.mouseY    = state.mouseY;
    ev.value     = 0;
    ev.tail      = 0;
    // UserInterface's event array (block E's class): raw offsets, by address.
    RtAddr<ArraySetSizeFillFn>(0x180033690ull)(
        B(ui, kUiEvents), I32(ui, kUiEventCount) + 1, &ev);
}

}  // namespace

namespace UI {

// [1] 0x180049d20 / [2] 0x180049d00
const char* SliderElement::TypeName() const {
    const char* v = nullptr; __builtin_memcpy(&v, SliderTypeInfo() + 8, sizeof v); return v;
}
uint32_t SliderElement::Type() const {
    uint32_t v = 0; __builtin_memcpy(&v, SliderTypeInfo(), sizeof v); return v;
}

// [4] 0x1800d7380
void SliderElement::ComputeSize() {
    if ((_flags & kFlagNeedsSizeCalc) == 0) return;

    const int32_t trackW = SpriteW(_trackStart) + SpriteW(_trackMiddle) + SpriteW(_trackEnd);
    int32_t width = SpriteW(_thumb);

    int32_t height = 0;
    if (_children.GetSize() > 0) {
        Element* child = _children.GetData()[0];
        if (child != nullptr) {
            int32_t cw = 0;
            InlineGetPaddedSize(child, &cw, &height);
        }
    }

    if (width < trackW) width = trackW;
    _layoutWidth = width;

    int32_t spriteH = SpriteH(_trackStart);
    const int32_t thumbH = SpriteH(_thumb);
    if (spriteH < thumbH) spriteH = thumbH;
    if (height  < spriteH) height = spriteH;

    _flags &= ~kFlagNeedsSizeCalc;
    _layoutHeight = height;
}

// [5] 0x1800d6fc0
void SliderElement::ComputeLayout(int32_t x, int32_t y, int32_t right, int32_t bottom) {
    if (_border != nullptr)
        reinterpret_cast<Element*>(_border)->ComputeLayout(x, y, right, bottom);   // B.3.6 bridge — slot 5

    ComputeSize();   // slot 4, VIRTUAL — `CALL [RAX+0x20]` in retail

    int32_t h = _layoutHeight;
    const int32_t align = _alignment;
    if (right  < _layoutWidth + x) x = right - _layoutWidth;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + _leftPad;
    int32_t py = y + _topPad;
    const int32_t ah = (bottom - _bottomPad) - py;
    const int32_t aw = (right  - _rightPad)  - px;

    if ((_flags & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 2: case 5: case 8:
            px = (right - _rightPad) - aw;
            break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - _bottomPad) - h;
    }

    _layoutTop    = py;
    _layoutLeft   = px;
    _layoutRight  = aw + px;
    _layoutBottom = py + h;

    Element* child = _children.GetData()[0];
    if (child == nullptr) return;

    int32_t cw = 0, ch = 0;
    InlineGetPaddedSize(child, &cw, &ch);

    int32_t pos = _layoutLeft + _thumbMargin + ThumbOffset(*this, SpriteW(_thumb));

    int32_t q = SpriteW(_thumb) + pos;
    if (q + cw < _layoutRight) q = _thumbMargin + q;
    else                       q = pos - cw;

    _children.GetData()[0]->ComputeLayout(q, _trackOffsetY + _layoutTop,
                                          q + cw, _layoutBottom);   // slot 5
}

// [6] 0x1800d6d40
void SliderElement::Draw(SpriteContext& context, uint8_t alpha) const {
    void* const ctx = &context;
    if (_border != nullptr)
        reinterpret_cast<const Element*>(_border)->Draw(context, alpha);   // B.3.6 bridge — slot 6

    const uint32_t raw = _color;
    if ((_flags & kFlagDimColor) != 0) alpha >>= 1;
    const uint32_t c = ModulateColor(raw, alpha);

    const int32_t left   = _layoutLeft;
    const int32_t rightX = _layoutRight;
    const int32_t top    = _layoutTop;
    const int32_t ty     = top + _trackOffsetY;

    // UI::SpriteContext::AddSprite (block D's class) — by address, doors stay.
    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, &_trackStart, left, ty, c);

    const int32_t midH = SpriteH(_trackMiddle);
    const int32_t endW = SpriteW(_trackEnd);
    const int32_t sttW = SpriteW(_trackStart);
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &_trackMiddle,
                                         left + sttW, ty,
                                         rightX - endW, ty + midH, c);

    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, &_trackEnd,
                                         rightX - SpriteW(_trackEnd), ty, c);

    // The thumb's TRAVEL is measured with the idle sprite's width even when
    // the hot sprite is the one drawn (0x1800d6f3d reads +0xF0 before the
    // hover swap at 0x1800d6f5f). Reproduced.
    const int32_t pos = ThumbOffset(*this, SpriteW(_thumb));
    const ::Sprite& thumb = (_hovered != 0) ? _thumbHot : _thumb;
    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, &thumb, _thumbMargin + pos + left, top, c);

    const Element* child = _children.GetData()[0];
    if (child != nullptr)
        child->Draw(context, alpha);   // slot 6
}

// ---------------------------------------------------------------------------
// [7] ?Update@SliderElement@UI@@ ...   0x1800d67e0
// ---------------------------------------------------------------------------
void SliderElement::Update(const System::Time& time, UserInterface* ui,
                           Object::InternalPtr<DialogInstance>* dialog,
                           const UpdateState& state) {
    // The base Update, QUALIFIED (this object's slot 7 is this very body), with
    // the handle copied into a temporary whose ADDRESS is passed — retail's shape.
    Object::InternalPtr<DialogInstance> slot = *dialog;
    Element::Update(time, ui, &slot, state);

    const int32_t thumbW = SpriteW(_thumb);
    const int32_t thumbH = SpriteH(_thumb);
    const float   range  = _maximum - _minimum;

    const int32_t pos = _layoutLeft + _thumbMargin + ThumbOffset(*this, thumbW);

    const int32_t mx = state.mouseX;
    const int32_t my = state.mouseY;
    const bool live  = state.buttonPressed != 0 && (_flags & kFlagInteractive) != 0;

    if (mx < pos || pos + thumbW < mx ||
        my < _layoutTop || _layoutTop + thumbH < my) {
        _hovered = 0;
        if (live) {
            bool moved = true;
            if (pos + thumbW < mx) {
                _value = range / static_cast<float>(_steps) + _value;
            } else if (mx < pos) {
                _value = _value - range / static_cast<float>(_steps);
            } else {
                moved = false;   // inside the thumb band horizontally: no page
            }
            if (moved) {
                QueueEvent(this, ui, dialog, state);
                RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);
            }
        }
    } else {
        _hovered = 1;
        if (live) {
            _dragAnchor = _value;
            SetFlags(this, _flags | kFlagDragging);
            RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);
        }
    }

    const bool isNear = !(mx < _layoutLeft   - 0x20 ||
                          _layoutRight + 0x20 < mx  ||
                          my < _layoutTop    - 0x20 ||
                          _layoutBottom + 0x20 < my);

    const uint32_t flags = _flags;
    if ((flags & kFlagDragging) != 0 && !isNear) {
        _value = _dragAnchor;
        RtAddr<UpdateNumberFn>(0x1800d6c20ull)(this);
        return;
    }

    bool doDrag = true;
    if ((flags & kFlagDragging) != 0 && state.b04 != 0) {       // +0x04: the release
        SetFlags(this, flags & ~kFlagDragging);
        if (isNear) RtAddr<PlayFn>(0x1801223d0ull)(ui, 5);
        else        doDrag = false;
    }

    if (doDrag && isNear && (_flags & kFlagDragging) != 0) {
        const int32_t w = SpriteW(_thumb);
        const int32_t span = (_layoutRight + ((w >> 1) + _thumbMargin) * -2) - _layoutLeft;
        const float v = _dragAnchor
            - (static_cast<float>(state.dragDx) / static_cast<float>(span))
              * (_maximum - _minimum);
        if (v != _value) {
            _value = v;
            QueueEvent(this, ui, dialog, state);
        }
    }

    if (_value < _minimum) _value = _minimum;
    if (_maximum < _value) _value = _maximum;

    RtAddr<UpdateNumberFn>(0x1800d6c20ull)(this);
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, QUALIFIED delegations.
// ============================================================================
extern "C" uint32_t RtUi_SliderElement_Type(const void* self) {
    return reinterpret_cast<const UI::SliderElement*>(self)->UI::SliderElement::Type();
}
extern "C" const char* RtUi_SliderElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::SliderElement*>(self)->UI::SliderElement::TypeName();
}
extern "C" void RtUi_SliderElement_ComputeSize(void* self) {
    reinterpret_cast<UI::SliderElement*>(self)->UI::SliderElement::ComputeSize();
}
extern "C" void RtUi_SliderElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                 int32_t right, int32_t bottom) {
    reinterpret_cast<UI::SliderElement*>(self)->UI::SliderElement::ComputeLayout(
        x, y, right, bottom);
}
extern "C" void RtUi_SliderElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::SliderElement*>(self)->UI::SliderElement::Draw(
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_SliderElement_Update(void* self, const void* time, void* ui,
                                          void** dlgPtr, const void* state) {
    reinterpret_cast<UI::SliderElement*>(self)->UI::SliderElement::Update(
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlgPtr),   // B.3.6 bridge
        *static_cast<const UI::Element::UpdateState*>(state));
}

// ??0SliderElement@UI@@QEAA@XZ   0x1800d7570 — a DOOR (B4b-2), unchanged.
extern "C" void* RtUi_SliderElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = VtSliderElement();
    const int sprites[5] = { kTrackStart, kTrackMiddle, kTrackEnd, kThumb, kThumbHot };
    for (int off : sprites) {
        PTR(self, off + kSpriteSheet) = nullptr;
        I32(self, off + kSpriteIndex) = -1;
    }
    I32(self, kTrackOffY)   = 0;
    I32(self, kThumbMargin) = 0;
    F32(self, kMinimum)     = 0.0f;
    F32(self, kMaximum)     = 1.0f;     // 0x3f800000
    I32(self, kSteps)       = 10;
    U8(self, kHovered)      = 0;
    F32(self, kValue)       = 0.25f;    // 0x3e800000
    return self;
}

// ??1SliderElement@UI@@UEAA@XZ   0x180049ee0 — tail jump, no restamp. A DOOR.
extern "C" void RtUi_SliderElement_Dtor(void* self) {
    ElementDtor(self);
}

// ============================================================================
int RtUiSliderElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@SliderElement@UI@@UEBAIXZ",
                          (void*)&RtUi_SliderElement_Type);          ++n;
    RegisterRuntimeAddr(0x180049d00ull, (void*)&RtUi_SliderElement_Type);

    RegisterRuntimeExport("?TypeName@SliderElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_SliderElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180049d20ull, (void*)&RtUi_SliderElement_TypeName);

    RegisterRuntimeExport("??0SliderElement@UI@@QEAA@XZ",
                          (void*)&RtUi_SliderElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1800d7570ull, (void*)&RtUi_SliderElement_Ctor);

    RegisterRuntimeExport("??1SliderElement@UI@@UEAA@XZ",
                          (void*)&RtUi_SliderElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180049ee0ull, (void*)&RtUi_SliderElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@SliderElement@UI@@UEAAXXZ",
                          (void*)&RtUi_SliderElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800d7380ull, (void*)&RtUi_SliderElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@SliderElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_SliderElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800d6fc0ull, (void*)&RtUi_SliderElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@SliderElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_SliderElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800d6d40ull, (void*)&RtUi_SliderElement_Draw);

    RegisterRuntimeExport(
        "?Update@SliderElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_SliderElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1800d67e0ull, (void*)&RtUi_SliderElement_Update);

    return n;
}
