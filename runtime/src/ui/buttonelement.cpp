// ============================================================================
// runtime/src/ui/buttonelement.cpp — UI::ButtonElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7ButtonElement@UI@@6B@` (retail 0x180157310).
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x1800485b0  deleting_dtor.cpp row
//    1    TypeName              0x180048260  here
//    2    Type                  0x180048240  here
//    4    ComputeSize           0x180110d10  here
//    5    ComputeLayout         0x180110e70  here
//    6    Draw                  0x180111020  here
//    7    Update                0x1801110b0  here
//    8    Enable                0x180110cc0  here
//
// plus ??0ButtonElement@UI@@QEAA@XZ         0x180110c40
//      ??0ButtonElement@UI@@QEAA@AEBV01@@Z  0x180048460
//      ??1ButtonElement@UI@@UEAA@XZ         0x180110c90
//
// Slots 4/5/6/8 are ALSO ComboElement's and slot 8 is ALSO CheckElement's — the
// two subclasses repeat these addresses in their own tables. They are
// registered once, here.
//
// ENDGAME B.3.6 block C1: the seven slot bodies are MEMBERS of
// `UI::ButtonElement` (runtime/types/ui/ButtonElement.h, which now owns the
// layout this file used to carry as file-local constants — the per-field
// evidence is there). The registered doors below are one-line QUALIFIED
// delegations; the three ctor/dtor doors are kept as they were (B4b-2).
//
// The four colours are handed to `_children[4]`'s virtual `SetColor` (slot 10),
// which is what makes `_children[4]` the button's LABEL and `_children[0..3]`
// its four state backgrounds.
//
// *** NEITHER CONSTRUCTOR INITIALISES +0xc8..+0xd4. *** The default ctor writes
// only +0xc0 and +0xc4; the four colours come from ButtonDescription. Do not
// byte-compare a freshly constructed ButtonElement past +0xc8.
// ============================================================================
#include "element_common.h"
#include "../../types/ui/ButtonElement.h"

using namespace rtuielem;

namespace {

// `_children` indices this class dispatches on.
constexpr int kChildLabel = 4;

// B6-5: the member bodies' door caches are namespace-scope atomics.
std::atomic<void*> g_buttonTypeInfo{nullptr};
const uint8_t* ButtonElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_buttonTypeInfo, "?_typeInfo@ButtonElement@UI@@0VBaseTypeInfo@Object@@A"));
}
// The ctor doors only.
void* VtButtonElement() { return RT_DATA_SLOT(void*, "??_7ButtonElement@UI@@6B@"); }

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
using PlayFn            = void  (*)(void* ui, int soundId);
using KeyPressedFn      = bool  (*)(void* keyMap, int a, int b);
// Collection::Array<UI::UserInterface::Event>::SetSize(array, newSize, &fill)
using EventPushFn       = void  (*)(void* array, int newSize, const void* fill);

// UI::UserInterface (block E's class, still a struct): the pending-event array
// at +0x48 {data, size, capacity} (runtime/types/ui/UserInterface.h,
// `Collection::Array<Event> _events`).
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

// UI::UserInterface::Event, 0x30 bytes (the stride UserInterface.h pins from
// CleanRemovedDialogs). The seven stores at 0x180111356..0x18011139e.
struct Event {
    void*    dialog;      // +0x00  the InternalPtr<DialogInstance> value
    void*    element;     // +0x08
    int32_t  kind;        // +0x10
    int32_t  elementId;   // +0x14
    int32_t  x;           // +0x18
    int32_t  y;           // +0x1c
    int32_t  index;       // +0x20
    int32_t  pad24;       // +0x24  NEVER WRITTEN by this path
    uint64_t tail;        // +0x28
};
static_assert(sizeof(Event) == 0x30, "UI event record stride");

constexpr int32_t kEventActivated = 0x12;

// Element::_flags bits this class reads that Element.h does not name: bit 17
// is "the mouse is over" (Element::Update's BTS 0x11), bit 21 "a press started
// here", bit 22 is kElementFlagWasActivated.
constexpr uint32_t kFlagHover        = 0x00020000u;
constexpr uint32_t kFlagPressStarted = 0x00200000u;

}  // namespace

// ============================================================================
// The member bodies
// ============================================================================
namespace UI {

uint32_t ButtonElement::Type() const { return TypeIdOf(ButtonElementTypeInfo()); }
const char* ButtonElement::TypeName() const { return TypeNameOf(ButtonElementTypeInfo()); }

// ============================================================================
// ?ComputeSize@ButtonElement@UI@@UEAAXXZ   0x180110d10
// ============================================================================
// The union of the label's padded size and the current state background's,
// floored by the element's own minimum.
//
// TWO CALLS, TWO SHAPES, AND THAT IS RETAIL: the LABEL goes through the
// out-of-line `?GetPaddedSize@Element@UI@@` (0x180115f10, a member of
// UI::Element since block A) writing straight into +0x60/+0x64; the STATE
// BACKGROUND has the same function INLINED and compared field by field.
// Collapsing the two into one helper would hide that the first writes `this`
// and the second does not.
//
// `_children[4]` and `_children[_state]` are both dereferenced WITHOUT a bounds
// check — a ButtonElement always has five children because ButtonDescription
// builds them.
void ButtonElement::ComputeSize() {
    if (((_flags >> 27) & 1u) == 0) return;

    _layoutWidth  = 0;                                     // one qword store over
    _layoutHeight = 0;                                     // +0x60 and +0x64

    Element* label = _children._data[kChildLabel];
    if (label != nullptr) label->GetPaddedSize(_layoutWidth, _layoutHeight);

    Element* bg = _children._data[_state];
    if (bg != nullptr) {
        int w = 0, h = 0;
        PaddedSizeInline(bg, &w, &h);
        if (_layoutWidth  < w) _layoutWidth  = w;
        if (_layoutHeight < h) _layoutHeight = h;
    }

    if (_layoutWidth  < _minWidth)  _layoutWidth  = _minWidth;
    if (_layoutHeight < _minHeight) _layoutHeight = _minHeight;

    _flags &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z   0x180110e70
// ============================================================================
// The standard aligned body, then the LABEL laid out over the computed rect
// (from the registers still holding it) and the STATE BACKGROUND laid out over
// the rect RE-READ FROM THE FIELDS. The two are the same numbers; the second
// re-read is retail's and is kept.
void ButtonElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ElementComputeAlignedRect(this, x, y, w, h);

    Element* label = _children._data[kChildLabel];
    if (label != nullptr)
        label->ComputeLayout(_layoutLeft, _layoutTop, _layoutRight, _layoutBottom);

    Element* bg = _children._data[_state];
    if (bg != nullptr)
        bg->ComputeLayout(_layoutLeft, _layoutTop, _layoutRight, _layoutBottom);
}

// ============================================================================
// ?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x180111020
// ============================================================================
// Border, dim, then the STATE BACKGROUND and the LABEL — in that order, so the
// label paints on top.
void ButtonElement::Draw(SpriteContext& context, uint8_t alpha) const {
    const Element* border = static_cast<const Element*>(_border);
    if (border != nullptr) border->Draw(context, alpha);

    alpha = DimAlpha(this, alpha);

    const Element* bg = _children._data[_state];
    if (bg != nullptr) bg->Draw(context, alpha);

    const Element* label = _children._data[kChildLabel];
    if (label != nullptr) label->Draw(context, alpha);
}

// ============================================================================
// ?Update@ButtonElement@UI@@   0x1801110b0
// ============================================================================
// The whole button state machine. Reading order matters throughout: retail
// RE-READS `_flags` from memory between almost every step, because the dirty
// walk it runs in between writes to it.
//
//   Element::Update(this, time, ui, &dlgCopy, state)
//   if (_flags & hidden) return
//   prev = _state
//   _flags &= ~bit22                                 (was-activated, cleared)
//   if (state.mouseDown && (_flags & bit17))         bit17 == "mouse is over"
//        _flags |= bit21                             bit21 == "press started here"
//   _state = disabled ? 1
//          : (bit17 && bit21 && state.mouseHeld) ? 3
//          : bit17 ? 2 : 0                           ... and `colour` follows it
//   if (_state != prev) { SetNeedsSizeCalc(this);
//                         _children[_state]->ComputeLayout(rect) }
//   if (_children[4]) _children[4]->SetColor(colour)
//   if (!(_flags & bit17)) _flags &= ~bit21
//   if (_state != 1) {                               (not disabled)
//        if (state.mouseDown && bit17 && bit21)  UserInterface::Play(ui, 4)
//        if ((state.mouseUp && bit17 && bit21) || (_key >= 0 && KeyMap::Pressed(...))) {
//             _flags &= ~bit21 ; _flags |= bit22
//             push Event{dlg, this, 0x12, _elementId, mouseX, mouseY, 0, 0}
//             UserInterface::Play(ui, 5)
//        }
//   }
//
// UpdateState bytes read here (Element.h names them): +0x01 `buttonPressed`
// (the mouse-down edge), +0x04 `b04` (the mouse-up edge), +0x07 `leftDown`
// (held), +0x18/+0x1c the mouse, +0x28 `config` (the Input::KeyMap*).
//
// The base call is QUALIFIED (`Element::Update`): an unqualified one would
// dispatch through this object's table straight back here.
//
// *** THE `_children[_state]` DEREF AFTER A STATE CHANGE IS UNGUARDED IN
// RETAIL *** (0x180111223 loads the vtable with no null test, unlike the
// `_children[4]` test two instructions later). A ButtonElement always has its
// four state children; the asymmetry is retail's own and no guard is added.
void ButtonElement::Update(const System::Time& time, UserInterface* ui,
                           Object::InternalPtr<DialogInstance>* dialog,
                           const UpdateState& state) {
    Object::InternalPtr<DialogInstance> dlgCopy = *dialog;
    Element::Update(time, ui, &dlgCopy, state);

    uint32_t flags = _flags;
    if ((flags & kFlagHidden) != 0) return;

    const int prevState = _state;
    StoreFlags(this, flags & ~kElementFlagWasActivated);

    if (state.buttonPressed != 0) {
        const uint32_t f = _flags;
        if ((f & kFlagHover) != 0) StoreFlags(this, f | kFlagPressStarted);
    }

    uint32_t colour;
    flags = _flags;
    if ((flags & kFlagDisabledMask) != 0) {
        colour = _stateColor[1];
        _state = 1;
    } else if ((flags & kFlagHover) != 0 && (flags & kFlagPressStarted) != 0 &&
               state.leftDown != 0) {
        colour = _stateColor[3];
        _state = 3;
    } else if ((flags & kFlagHover) != 0) {
        colour = _stateColor[2];
        _state = 2;
    } else {
        colour = _stateColor[0];
        _state = 0;
    }

    if (_state != prevState) {
        rtuielem::SetNeedsSizeCalc(this);
        Element* bg = _children._data[_state];
        bg->ComputeLayout(_layoutLeft, _layoutTop, _layoutRight, _layoutBottom);
    }

    Element* label = _children._data[kChildLabel];
    if (label != nullptr) label->SetColor(colour);

    flags = _flags;
    if ((flags & kFlagHover) == 0) StoreFlags(this, flags & ~kFlagPressStarted);

    if (_state == 1) return;

    if (state.buttonPressed != 0) {
        const uint32_t f = _flags;
        if ((f & kFlagHover) != 0 && (f & kFlagPressStarted) != 0)
            RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);
    }

    bool fire = false;
    if (state.b04 != 0) {
        const uint32_t f = _flags;
        if ((f & kFlagHover) != 0 && (f & kFlagPressStarted) != 0) fire = true;
    }
    if (!fire) {
        const int32_t key = _keyIndex;
        if (key < 0) return;                                   // 1801112d4 JS -> end
        if (!RtAddr<KeyPressedFn>(0x1800a7a80ull)(state.config, 0, key)) return;
    }

    StoreFlags(this, _flags & ~kFlagPressStarted);
    StoreFlags(this, _flags |  kElementFlagWasActivated);

    Event ev;
    ev.dialog    = *reinterpret_cast<void* const*>(dialog);
    ev.element   = this;
    ev.kind      = kEventActivated;
    ev.elementId = _elementId;
    ev.x         = state.mouseX;
    ev.y         = state.mouseY;
    ev.index     = 0;
    ev.tail      = 0;
    uint8_t* u = reinterpret_cast<uint8_t*>(ui);
    RtAddr<EventPushFn>(0x180033690ull)(u + kUiEvents, I32(u, kUiEventCount) + 1, &ev);
    RtAddr<PlayFn>(0x1801223d0ull)(ui, 5);
}

// ============================================================================
// ?Enable@ButtonElement@UI@@UEAAX_N@Z   0x180110cc0
// ============================================================================
// Element::Enable first (QUALIFIED), then — only when the element ended up
// disabled — push the DISABLED colour (+0xcc) into the label. Retail's tail is
// an indirect JUMP through slot 10, which is why Ghidra prints "could not
// recover jumptable"; behaviourally a call plus return.
void ButtonElement::Enable(bool enable) {
    Element::Enable(enable);

    Element* label = _children._data[kChildLabel];
    if (label != nullptr && (_flags & kFlagDisabledMask) != 0)
        label->SetColor(_stateColor[1]);
}

}  // namespace UI

// ============================================================================
// The DOORS — registered names and addresses unchanged, each a one-line
// QUALIFIED delegation onto the member above.
// ============================================================================
extern "C" uint32_t RtUi_ButtonElement_Type(const void* self) {
    return reinterpret_cast<const UI::ButtonElement*>(self)->UI::ButtonElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_ButtonElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::ButtonElement*>(self)->UI::ButtonElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_ButtonElement_ComputeSize(void* selfv) {
    reinterpret_cast<UI::ButtonElement*>(selfv)->UI::ButtonElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_ButtonElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    reinterpret_cast<UI::ButtonElement*>(selfv)->UI::ButtonElement::ComputeLayout(x, y, w, h);   // B.3.6 bridge
}
extern "C" void RtUi_ButtonElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::ButtonElement*>(selfv)->UI::ButtonElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_ButtonElement_Update(void* selfv, const void* time, void* ui,
                                          void** dlg, const void* statev) {
    reinterpret_cast<UI::ButtonElement*>(selfv)->UI::ButtonElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),   // B.3.6 bridge
        *static_cast<const UI::Element::UpdateState*>(statev));
}
extern "C" void RtUi_ButtonElement_Enable(void* selfv, bool enable) {
    reinterpret_cast<UI::ButtonElement*>(selfv)->UI::ButtonElement::Enable(enable);   // B.3.6 bridge
}

// ============================================================================
// Constructors / destructor — DOORS (B4b-2), unchanged
// ============================================================================
extern "C" void* RtUi_ButtonElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtButtonElement();
    I32(self, offsetof(UI::ButtonElement, _keyIndex)) = -1;
    I32(self, offsetof(UI::ButtonElement, _state))    = 0;
    return self;
}

extern "C" void* RtUi_ButtonElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtButtonElement();
    for (int off = 0xc0; off <= 0xd4; off += 4)            // _keyIndex .. _stateColor[3]
        I32(self, off) = I32c(other, off);
    return self;
}

extern "C" void RtUi_ButtonElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtButtonElement();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiButtonElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@ButtonElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ButtonElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180048260ull, (void*)&RtUi_ButtonElement_TypeName);

    RegisterRuntimeExport("?Type@ButtonElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ButtonElement_Type);          ++n;
    RegisterRuntimeAddr(0x180048240ull, (void*)&RtUi_ButtonElement_Type);

    RegisterRuntimeExport("?ComputeSize@ButtonElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ButtonElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x180110d10ull, (void*)&RtUi_ButtonElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ButtonElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ButtonElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x180110e70ull, (void*)&RtUi_ButtonElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ButtonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ButtonElement_Draw);          ++n;
    RegisterRuntimeAddr(0x180111020ull, (void*)&RtUi_ButtonElement_Draw);

    RegisterRuntimeExport(
        "?Update@ButtonElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ButtonElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1801110b0ull, (void*)&RtUi_ButtonElement_Update);

    RegisterRuntimeExport("?Enable@ButtonElement@UI@@UEAAX_N@Z",
                          (void*)&RtUi_ButtonElement_Enable);        ++n;
    RegisterRuntimeAddr(0x180110cc0ull, (void*)&RtUi_ButtonElement_Enable);

    RegisterRuntimeExport("??0ButtonElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ButtonElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x180110c40ull, (void*)&RtUi_ButtonElement_Ctor);

    RegisterRuntimeExport("??0ButtonElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_ButtonElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180048460ull, (void*)&RtUi_ButtonElement_CopyCtor);

    RegisterRuntimeExport("??1ButtonElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ButtonElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180110c90ull, (void*)&RtUi_ButtonElement_Dtor);
    return n;
}
