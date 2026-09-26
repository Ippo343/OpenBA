// ============================================================================
// runtime/src/ui/checkelement.cpp — UI::CheckElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7CheckElement@UI@@6B@` (retail 0x180157260),
// a UI::ButtonElement subclass that adds one bool and one extra child.
//
//   slot  method                       retail       where
//   ----  ---------------------------  -----------  --------------------------
//    0    vector deleting dtor         0x180048d80  deleting_dtor.cpp row
//    1    TypeName                     0x180048a80  here
//    2    Type                         0x180048a60  here
//    4    ComputeSize                  0x180111740  here
//    5    ComputeLayout                0x1801117b0  here
//    6    Draw                         0x180111800  here
//    7    Update                       0x180111870  here
//    8    ButtonElement::Enable        0x180110cc0  buttonelement.cpp
//
// plus ??0CheckElement@UI@@QEAA@XZ         0x1801116a0
//      ??0CheckElement@UI@@QEAA@AEBV01@@Z  0x180048ca0
//      ??1CheckElement@UI@@UEAA@XZ         0x180111700
//
// ENDGAME B.3.6 block C1: the six slot bodies are MEMBERS of
// `UI::CheckElement` (runtime/types/ui/CheckElement.h, which now owns the
// layout: ButtonElement's 0xd8, then the check BYTE at +0xd8 and seven bytes
// of tail padding). Every call into ButtonElement's slot bodies is a QUALIFIED
// member call (it used to be a `ResolveAddr` by address). `_children[5]` is
// the tick mark: the only child index past ButtonElement's five, drawn only
// while `_checked` is set.
// ============================================================================
#include "element_common.h"
#include "../../types/ui/CheckElement.h"
#include "../../types/input/Input.h"   // ENDGAME B.3.7 callers (K5): Input::KeyMap

using namespace rtuielem;

namespace {

constexpr int kChildTick = 5;

// B6-5: namespace-scope atomic door cache for the member bodies.
std::atomic<void*> g_checkTypeInfo{nullptr};
const uint8_t* CheckElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_checkTypeInfo, "?_typeInfo@CheckElement@UI@@0VBaseTypeInfo@Object@@A"));
}
// The ctor/dtor doors only.
void* VtCheckElement()      { return RT_DATA_SLOT(void*, "??_7CheckElement@UI@@6B@"); }
void* VtButtonElementBase() { return RT_DATA_SLOT(void*, "??_7ButtonElement@UI@@6B@"); }

using ElementCtorFn      = void* (*)(void* self);
using ButtonCopyCtorFn   = void* (*)(void* self, const void* other);
using ElementDtorFn      = void  (*)(void* self);

// Element::_flags bits Element.h does not name (see buttonelement.cpp).
constexpr uint32_t kFlagHover        = 0x00020000u;
constexpr uint32_t kFlagPressStarted = 0x00200000u;

}  // namespace

namespace UI {

uint32_t CheckElement::Type() const { return TypeIdOf(CheckElementTypeInfo()); }
const char* CheckElement::TypeName() const { return TypeNameOf(CheckElementTypeInfo()); }

// ============================================================================
// ?ComputeSize@CheckElement@UI@@UEAAXXZ   0x180111740
// ============================================================================
// Run the button's own sizing, then widen to fit the tick.
//
// The tick's contribution is its RAW `_layoutWidth`/`_layoutHeight` — NOT its
// padded size. This is the one place in the batch where a child's size is
// taken without the padding, and it is what 0x180111785..0x1801117a3 does.
//
// Bit 27 is cleared TWICE on this path: once by ButtonElement::ComputeSize and
// again at the end here. Reproduced; the second clear is a no-op unless the
// tick's own ComputeSize dirtied us back, which it can (it walks parents).
void CheckElement::ComputeSize() {
    if (((_flags >> 27) & 1u) == 0) return;

    ButtonElement::ComputeSize();

    Element* tick = _children._data[kChildTick];
    if (tick != nullptr) {
        tick->ComputeSize();
        const int w = tick->_layoutWidth;
        const int h = tick->_layoutHeight;
        if (_layoutWidth  < w) _layoutWidth  = w;
        if (_layoutHeight < h) _layoutHeight = h;
    }

    _flags &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@CheckElement@UI@@UEAAXHHHH@Z   0x1801117b0
// ============================================================================
// ButtonElement's layout, then the tick over the same rect. Retail's tail is a
// JUMP into the tick's slot 5 (0x1801117f0), which is why Ghidra loses the
// fourth argument; the disassembly shows all four plus the stack slot.
void CheckElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ButtonElement::ComputeLayout(x, y, w, h);

    Element* tick = _children._data[kChildTick];
    if (tick != nullptr)
        tick->ComputeLayout(_layoutLeft, _layoutTop, _layoutRight, _layoutBottom);
}

// ============================================================================
// ?Draw@CheckElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x180111800
// ============================================================================
// *** THE ALPHA IS HALVED TWICE WHEN THE DIM FLAG IS SET. *** This body halves
// it BEFORE calling ButtonElement::Draw, which tests the same bit on the same
// object and halves it again. That is what the shipped bytes do (0x180111800
// tests +0x34 and shifts, then 0x180111020 tests +0x34 and shifts), so a
// dimmed CheckElement is drawn at a quarter alpha and its tick at a quarter of
// that. Not a transcription slip — do not "fix" it.
void CheckElement::Draw(SpriteContext& context, uint8_t alpha) const {
    alpha = DimAlpha(this, alpha);

    ButtonElement::Draw(context, alpha);

    if (_checked != 0) {
        const Element* tick = _children._data[kChildTick];
        if (tick != nullptr) tick->Draw(context, alpha);
    }
}

// ============================================================================
// ?Update@CheckElement@UI@@   0x180111870
// ============================================================================
// Toggle on a click or on the shortcut key, then run ButtonElement::Update —
// which is what actually raises the activation event. The toggle happens
// FIRST, so the event a listener sees already reflects the new check state.
// UpdateState +0x04 is `b04` (the mouse-up edge), +0x28 `config` (the KeyMap).
void CheckElement::Update(const System::Time& time, UserInterface* ui,
                          Object::InternalPtr<DialogInstance>* dialog,
                          const UpdateState& state) {
    const uint32_t flags = _flags;
    bool toggle = false;
    if ((flags & kFlagDisabledMask) == 0) {
        if (state.b04 != 0 && (flags & kFlagHover) != 0 &&
            (flags & kFlagPressStarted) != 0) {
            toggle = true;
        } else {
            const int32_t key = _keyIndex;
            if (key >= 0 &&
                reinterpret_cast<const ::Input::KeyMap*>(state.config)->Pressed(0, key)) {  // B.3.7 bridge
                toggle = true;
            }
        }
    }
    if (toggle) _checked = (_checked == 0) ? 1 : 0;

    Object::InternalPtr<DialogInstance> dlgCopy = *dialog;
    ButtonElement::Update(time, ui, &dlgCopy, state);
}

}  // namespace UI

// ============================================================================
// The DOORS
// ============================================================================
extern "C" uint32_t RtUi_CheckElement_Type(const void* self) {
    return reinterpret_cast<const UI::CheckElement*>(self)->UI::CheckElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_CheckElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::CheckElement*>(self)->UI::CheckElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_CheckElement_ComputeSize(void* selfv) {
    reinterpret_cast<UI::CheckElement*>(selfv)->UI::CheckElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_CheckElement_ComputeLayout(void* selfv, int x, int y, int w, int h) {
    reinterpret_cast<UI::CheckElement*>(selfv)->UI::CheckElement::ComputeLayout(x, y, w, h);   // B.3.6 bridge
}
extern "C" void RtUi_CheckElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::CheckElement*>(selfv)->UI::CheckElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
extern "C" void RtUi_CheckElement_Update(void* selfv, const void* time, void* ui,
                                         void** dlg, const void* statev) {
    reinterpret_cast<UI::CheckElement*>(selfv)->UI::CheckElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),   // B.3.6 bridge
        *static_cast<const UI::Element::UpdateState*>(statev));
}

// ============================================================================
// Constructors / destructor — DOORS (B4b-2), unchanged
// ============================================================================
// The default constructor INLINES ButtonElement's body (Element ctor, the base
// vtable stamp, +0xc0 = -1, +0xc4 = 0) rather than calling ??0ButtonElement,
// then stamps its own vtable and clears +0xd8. The copy constructor DOES call
// ??0ButtonElement. Both spellings are retail's and are kept as they are.
extern "C" void* RtUi_CheckElement_Ctor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    RtAddr<ElementCtorFn>(0x180115e50ull)(self);
    P(self, 0x00) = VtButtonElementBase();
    I32(self, 0xc0) = -1;
    I32(self, 0xc4) = 0;
    P(self, 0x00) = VtCheckElement();
    self[offsetof(UI::CheckElement, _checked)] = 0;
    return self;
}

extern "C" void* RtUi_CheckElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ButtonCopyCtorFn>(0x180048460ull)(self, other);
    P(self, 0x00) = VtCheckElement();
    self[offsetof(UI::CheckElement, _checked)] = other[offsetof(UI::CheckElement, _checked)];
    return self;
}

// ??1CheckElement@UI@@UEAA@XZ  0x180111700 — own vtable, then the BASE vtable
// over it (dead, nothing reads it), then ~Element. Both stamps reproduced.
extern "C" void RtUi_CheckElement_Dtor(void* selfv) {
    P(selfv, 0x00) = VtCheckElement();
    P(selfv, 0x00) = VtButtonElementBase();
    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// Registration — slot 8 is ButtonElement's address and is NOT re-registered.
// ============================================================================
int RtUiCheckElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@CheckElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_CheckElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180048a80ull, (void*)&RtUi_CheckElement_TypeName);

    RegisterRuntimeExport("?Type@CheckElement@UI@@UEBAIXZ",
                          (void*)&RtUi_CheckElement_Type);          ++n;
    RegisterRuntimeAddr(0x180048a60ull, (void*)&RtUi_CheckElement_Type);

    RegisterRuntimeExport("?ComputeSize@CheckElement@UI@@UEAAXXZ",
                          (void*)&RtUi_CheckElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x180111740ull, (void*)&RtUi_CheckElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@CheckElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_CheckElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1801117b0ull, (void*)&RtUi_CheckElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@CheckElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_CheckElement_Draw);          ++n;
    RegisterRuntimeAddr(0x180111800ull, (void*)&RtUi_CheckElement_Draw);

    RegisterRuntimeExport(
        "?Update@CheckElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_CheckElement_Update);                          ++n;
    RegisterRuntimeAddr(0x180111870ull, (void*)&RtUi_CheckElement_Update);

    RegisterRuntimeExport("??0CheckElement@UI@@QEAA@XZ",
                          (void*)&RtUi_CheckElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1801116a0ull, (void*)&RtUi_CheckElement_Ctor);

    RegisterRuntimeExport("??0CheckElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_CheckElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x180048ca0ull, (void*)&RtUi_CheckElement_CopyCtor);

    RegisterRuntimeExport("??1CheckElement@UI@@UEAA@XZ",
                          (void*)&RtUi_CheckElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180111700ull, (void*)&RtUi_CheckElement_Dtor);
    return n;
}
