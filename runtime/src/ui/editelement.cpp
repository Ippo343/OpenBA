// ============================================================================
// runtime/src/ui/editelement.cpp — UI::EditElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7EditElement@UI@@6B@`.
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x18003fed0  deleting_dtor.cpp row
//    1    TypeName              0x18003fae0  here
//    2    Type                  0x18003fac0  here
//    4    ComputeSize           0x1800d0c10  here
//    5    ComputeLayout         0x1800d0ab0  here
//    6    Draw                  0x1800d0820  here
//    7    Update                0x1800d0220  here
//   12    FinalizeText          0x1800d0da0  here
//
// plus ??0EditElement@UI@@QEAA@AEBV01@@Z  0x18003fc90 (door body)
//      ??1EditElement@UI@@UEAA@XZ         0x1800d0e50 (door body)
//
// Slots 4/5/6/7 are ALSO NumberEditElement's (it inherits them); registered
// once, here.
//
// *** THE VTABLE IS THIRTEEN SLOTS, NOT TWELVE. *** `?FinalizeText@` is slot 12
// (+0x60), INTRODUCED by this class. Update calls it through `[vtable + 0x60]`
// at 0x1800d0396; `?FinalizeText@NumberEditElement@` (0x1800d4a60) overrides it.
//
// ENDGAME B.3.6 block C3: every slot body is a `UI::EditElement` member
// (runtime/types/ui/EditElement.h — the 0xf8 layout, pinned from the ctor
// 0x1800d0f10; the old header's "size not asserted" and NumberEditElement.h's
// 0xE8 are gone). The registered doors delegate with a QUALIFIED call. The
// base `Element::Update` call is the qualified member; slot 12 is a C++
// virtual call (it reads the MINTED table, exactly as retail's
// `CALL [RAX+0x60]`); Font / SpriteContext calls are direct member calls.
//
// NOT HERE: `?SetText@EditElement@UI@@` (widgets_input.cpp's door) and
// `?SelectCharacter@` (element_bodies_b5.cpp's door, reached by address); the
// constructor 0x1800d0f10 (factory_ui1.cpp).
// ============================================================================
#include <atomic>

#include "element_common.h"
#include "../../types/ui/EditElement.h"
#include "../../types/ui/Font.h"
#include "../../types/ui/SpriteContext.h"
#include "../../types/system/String.h"

using namespace rtuielem;

namespace {

// Input::Device — the per-key "pressed this frame" byte table. The keyboard
// block starts at +0x6c6 with a stride of 0x18 and 0x69 entries, which is the
// loop bound at 0x1800d07f6. Two bytes per key (+0x00 and +0x02) are tested
// together everywhere: press and repeat.
constexpr int kKeyBase   = 0x6c6;
constexpr int kKeyStride = 0x18;
constexpr int kKeyCount  = 0x69;
constexpr int kKeyEscape = 0xc66;
constexpr int kKeyEnter  = 0xf7e;
constexpr int kKeyLeft   = 0xea6;
constexpr int kKeyRight  = 0xed6;
constexpr int kKeyHome   = 0xe16;
constexpr int kKeyEnd    = 0xe5e;
constexpr int kKeyBack   = 0xba6;
constexpr int kKeyDelete = 0xe46;

// UI::UserInterface
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

constexpr int32_t kEventEditFinished = 0x15;

// RULE B6-5: namespace-scope atomic slots, never a first-call static.
std::atomic<void*> g_typeInfoSlot{nullptr};
std::atomic<void*> g_inputDeviceSlot{nullptr};
std::atomic<void*> g_inputConfigSlot{nullptr};
std::atomic<void*> g_secondsPerTickSlot{nullptr};
std::atomic<void*> g_memFreeSlot{nullptr};

const uint8_t* EditElementTypeInfo() {
    return static_cast<const uint8_t*>(AtomicExport(
        g_typeInfoSlot, "?_typeInfo@EditElement@UI@@0VBaseTypeInfo@Object@@A"));
}
void* VtEditElement() { return RT_DATA_SLOT(void*, "??_7EditElement@UI@@6B@"); }

void** InputDeviceSlot() {
    return static_cast<void**>(AtomicExport(
        g_inputDeviceSlot, "?_singleton@Device@Input@@0PEAV12@EA"));
}
void** InputConfigSlot() {
    return static_cast<void**>(AtomicExport(
        g_inputConfigSlot, "?_singleton@Configuration@Input@@0PEAV12@EA"));
}
double SecondsPerTick() {
    double* p = static_cast<double*>(AtomicExport(
        g_secondsPerTickSlot, "?_secondsPerTick@Timer@System@@0NA"));
    return p ? *p : 0.0;
}

using MemFreeFn     = void  (*)(void* p);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn = void  (*)(void* self);
using EventPushFn   = void  (*)(void* array, int newSize, const void* fill);
// System::String, reached by address.
using StrAppendCharFn = void* (*)(void* self, void* tmpOut, char c);
using StrRemoveAtFn   = void** (*)(void* self, void** out, int index);
using StrAssignFn     = void* (*)(void* self, const void* other);
using StrAssignCStrFn = void* (*)(void* self, const char* s);
using StrSetLengthFn  = void* (*)(void* self, int n);
using StrInsertAtFn   = void** (*)(void* self, void** out, int index, void* const* str);
using MemcpyFn        = void* (*)(void* dst, const void* src, uint64_t n);
using SelectCharFn    = uint8_t (*)(const void* self, int keyIndex);

void MemFree(void* p) {
    reinterpret_cast<MemFreeFn>(AtomicExport(
        g_memFreeSlot, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z"))(p);
}

struct Event {
    void*    dialog;
    void*    element;
    int32_t  kind;
    int32_t  elementId;
    int32_t  x;
    int32_t  y;
    int32_t  index;
    int32_t  pad24;
    uint64_t tail;
};
static_assert(sizeof(Event) == 0x30, "UI event record stride");

// `System::String`'s length, the way every body in this file reads it: the
// dword at [data - 8], and ZERO when the handle is null.
int StrLen(const void* s) {
    if (s == nullptr) return 0;
    return *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(s) - 8);
}

const System::String& AsString(void* const& field) {
    return *reinterpret_cast<const System::String*>(&field);
}
const System::String& AsString(char* const& field) {
    return *reinterpret_cast<const System::String*>(&field);
}

void ReleaseString(void** slot) {
    char* s = static_cast<char*>(*slot);
    if (s == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(s - 4);
    --*rc;
    if (*rc == 0) {
        *slot = s - 8;
        MemFree(*slot);
    } else {
        *slot = nullptr;
    }
}

// The inlined `System::String::operator=` the three edit paths share:
// `if (dst != src) { release dst; dst = src; addref }`. The pointer COMPARE
// comes first, so assigning a String to itself does not touch the refcount.
void AssignStringInline(void** dst, void* const* src) {
    if (*dst == *src) return;
    if (*dst != nullptr) {
        int32_t* rc = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*dst) - 4);
        --*rc;
        if (*rc == 0) {
            *dst = static_cast<uint8_t*>(*dst) - 8;
            MemFree(*dst);
        } else {
            *dst = nullptr;
        }
    }
    *dst = *src;
    if (*dst != nullptr) ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(*dst) - 4);
}

// Retail's password mask: append '*' to a fresh String once per character of
// the real text. `System::String::operator+=` (0x1800c89e0) takes a HIDDEN
// TEMPORARY SLOT as its second argument and leaves a String in it that the
// caller must release — which is why the loop has a release in its body.
void* BuildMask(int length) {
    void* masked = nullptr;
    for (int i = 0; i < length; ++i) {
        void* tmp = nullptr;
        RtAddr<StrAppendCharFn>(0x1800c89e0ull)(&masked, &tmp, '*');
        ReleaseString(&tmp);
    }
    return masked;
}

}  // namespace

namespace UI {

uint32_t EditElement::Type() const { return TypeIdOf(EditElementTypeInfo()); }
const char* EditElement::TypeName() const { return TypeNameOf(EditElementTypeInfo()); }

// Non-virtual, EMPTY: the dtor door keeps retail's body.
EditElement::~EditElement() {}

// ============================================================================
// ?ComputeSize@EditElement@UI@@UEAAXXZ   0x1800d0c10
// ============================================================================
// One line of text, measured with `UI::Font::ComputeLineLayout` (0x180116c70),
// floored by the element's minimum WIDTH only — the height is the font's line
// height with no floor at all.
//
// In password mode the mask is built FIRST and measured instead, so a password
// box is as wide as its asterisks, not as its secret.
void EditElement::ComputeSize() {
    if (((_flags >> 27) & 1u) == 0) return;

    Font* font = _font;

    if (_password == 0) {
        char* text = _text;
        _layoutWidth = 0; _layoutHeight = 0;                 // one qword store
        _layoutWidth  = font->ComputeLineLayout(text, StrLen(text), 0, 0, -1, 0, nullptr);
        _layoutHeight = font->_lineHeight;
    } else {
        void* masked = BuildMask(StrLen(_text));
        _layoutWidth = 0; _layoutHeight = 0;                 // one qword store
        _layoutWidth  = font->ComputeLineLayout(static_cast<const char*>(masked),
                                                StrLen(masked), 0, 0, -1, 0, nullptr);
        _layoutHeight = font->_lineHeight;
        ReleaseString(&masked);
    }

    if (_layoutWidth < _minWidth) _layoutWidth = _minWidth;
    _flags &= ~kElementFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@EditElement@UI@@UEAAXHHHH@Z   0x1800d0ab0 — the standard
// Element alignment body (element_common.h), which dispatches slots 5/4.
// ============================================================================
void EditElement::ComputeLayout(int32_t x, int32_t y, int32_t w, int32_t h) {
    ElementComputeAlignedRect(this, x, y, w, h);
}

// ============================================================================
// ?Draw@EditElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d0820
// ============================================================================
// *** THE DISABLED TEST IS NOT THE USUAL ONE. *** This one returns early when
// EITHER disabled bit is set UNLESS bit 24 (system-disabled) is ALSO set — i.e.
// a user-disabled edit box draws nothing, but a system-disabled one still
// draws. Read off 0x1800d085d..0x1800d086e.
//
// The text is drawn at the element's left edge and a baseline of
// `top - font.baseline + font.lineHeight`, and the caret — a literal "|" —
// one pixel LEFT of the character position the caret index resolves to.
void EditElement::Draw(SpriteContext& ctx, uint8_t alpha) const {
    if (Element* border = static_cast<Element*>(_border))
        border->Draw(ctx, alpha);                            // virtual, slot 6

    const uint32_t flags = _flags;
    if ((flags & kFlagDisabledMask) != 0 && ((flags >> 24) & 1u) == 0) return;

    const uint32_t baseColor = _color;
    if ((flags & kElementFlagDimColor) != 0) alpha = static_cast<uint8_t>(alpha >> 1);
    const uint32_t colour = rtuielem::ComputeColor(baseColor, alpha);

    // The string actually drawn: the mask in password mode, otherwise a
    // REFERENCE to the real text (retail bumps its refcount and releases it at
    // the end, so the field itself is untouched).
    void* shown = nullptr;
    if (_password != 0) {
        shown = BuildMask(StrLen(_text));
    } else if (_text != nullptr) {
        shown = _text;
        ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(shown) - 4);
    }

    Font* font = _font;
    const int baseline = _layoutTop - font->_baseLine + font->_lineHeight;
    ctx.AddFont(Object::ExternalPtr<Font>(font), AsString(shown), false, 0, 0, -1,
                _layoutLeft, baseline, colour);

    if (_blinkOn != 0 && _active != 0) {
        const int cx = _font->ComputeCharacterPosition(AsString(shown), _caret) - 1;

        // THE CARET'S STRING IS BUILT FROM A C LITERAL. Retail passes the DLL's
        // own `"|"` at 0x180156804; our own literal is the same bytes and
        // `System::String::Set` copies them either way.
        void* caret = nullptr;
        static const char kCaretText[] = "|";
        RtAddr<StrAssignCStrFn>(0x1800c95d0ull)(&caret, kCaretText);

        Font* f = _font;
        const int y = _layoutTop - f->_baseLine + f->_lineHeight;
        ctx.AddFont(Object::ExternalPtr<Font>(_font), AsString(caret), false, 0, 0, -1,
                    _layoutLeft + cx, y, colour);
        ReleaseString(&caret);
    }

    ReleaseString(&shown);
}

// ============================================================================
// ?Update@EditElement@UI@@   0x1800d0220
// ============================================================================
// Focus, caret movement, editing and character insertion, all driven straight
// off `Input::Device::_singleton`'s key table rather than through the update
// state — the element reads the keyboard itself.
//
// FOCUS is taken on a click inside (and the caret placed at the clicked
// character, found by walking `ComputeCharacterPosition` until it passes the
// cursor), and dropped on a click OUTSIDE, on Escape, or on Enter. Taking and
// dropping it INCREMENTS and DECREMENTS a counter inside
// `Input::Configuration::_singleton` — the engine's "text input is active,
// stop treating keys as game bindings" latch.
//
// DROPPING focus calls virtual slot 12 (`FinalizeText`) and raises event 0x15.
void EditElement::Update(const System::Time& time, UserInterface* ui,
                         Object::InternalPtr<DialogInstance>* dlg,
                         const UpdateState& st) {
    {
        // The base body gets its OWN copy of the handle, by address (B6-11).
        Object::InternalPtr<DialogInstance> dlgCopy = *dlg;
        UI::Element::Update(time, ui, &dlgCopy, st);
    }

    void** devSlot = InputDeviceSlot();
    const uint8_t* dev = devSlot ? static_cast<const uint8_t*>(*devSlot) : nullptr;

    if ((_flags & kFlagDisabledMask) != 0) return;

    // ---- take focus -------------------------------------------------------
    if (st.buttonPressed != 0 && ((_flags >> 17) & 1u) != 0) {
        if (_active == 0) {
            void** cfg = InputConfigSlot();
            ++**reinterpret_cast<int32_t**>(*cfg);
        }
        _active = 1;
        _blinkOn = 1;
        _blinkAccumulator = 0;
        _caret = StrLen(_text);

        const int relX = st.mouseX - _layoutLeft;
        for (int i = 0; i < StrLen(_text); ++i) {
            const int cx = _font->ComputeCharacterPosition(AsString(_text), i + 1);
            if (relX >= 0 && relX <= cx) { _caret = i; break; }
        }
    }

    if (_active == 0) return;

    // ---- drop focus -------------------------------------------------------
    const bool clickedAway = st.buttonPressed != 0 && (_flags & 0x20000u) == 0;
    if (clickedAway || dev[kKeyBase] != 0 || dev[kKeyEscape] != 0 || dev[kKeyEnter] != 0) {
        _active = 0;
        void** cfg = InputConfigSlot();
        --**reinterpret_cast<int32_t**>(*cfg);

        FinalizeText();                                      // virtual, slot 12
        SetNeedsSizeCalc();

        Event ev;
        ev.dialog    = *reinterpret_cast<void* const*>(dlg);
        ev.element   = this;
        ev.kind      = kEventEditFinished;
        ev.elementId = _elementId;
        ev.x         = st.mouseX;
        ev.y         = st.mouseY;
        ev.index     = 0;
        ev.tail      = 0;
        RtAddr<EventPushFn>(0x180033690ull)(reinterpret_cast<uint8_t*>(ui) + kUiEvents,
                                            I32(ui, kUiEventCount) + 1, &ev);
    }

    if (_active == 0) return;

    // ---- blink ------------------------------------------------------------
    _blinkAccumulator += *reinterpret_cast<const int64_t*>(&time);
    if (_blinkAccumulator > static_cast<int64_t>(0.5 / SecondsPerTick())) {
        _blinkOn = (_blinkOn == 0) ? 1 : 0;
        _blinkAccumulator = 0;
    }

    // ---- caret movement ---------------------------------------------------
    if (dev[kKeyLeft] != 0 || dev[kKeyLeft + 2] != 0) {
        _caret -= 1;
        if (_caret < 0) _caret = 0;
    }
    if (dev[kKeyRight] != 0 || dev[kKeyRight + 2] != 0) {
        _caret += 1;
        const int len = StrLen(_text);
        if (_caret > len) _caret = len;
    }
    if (dev[kKeyHome] != 0) _caret = 0;
    if (dev[kKeyEnd]  != 0) _caret = StrLen(_text);

    // ---- backspace / delete ----------------------------------------------
    const auto removeAt = RtAddr<StrRemoveAtFn>(0x1800c86b0ull);
    const auto assign   = RtAddr<StrAssignFn>(0x1800c9810ull);

    if ((dev[kKeyBack] != 0 || dev[kKeyBack + 2] != 0) && _caret > 0) {
        void* tmp = nullptr;
        void** ret = removeAt(&_text, &tmp, _caret - 1);
        assign(&_text, ret);
        ReleaseString(&tmp);
        _caret -= 1;
        SetNeedsSizeCalc();
    }

    if (dev[kKeyDelete] != 0 || dev[kKeyDelete + 2] != 0) {
        if (_caret < StrLen(_text)) {
            void* tmp = nullptr;
            void** ret = removeAt(&_text, &tmp, _caret);
            assign(&_text, ret);
            ReleaseString(&tmp);
            SetNeedsSizeCalc();
        }
    }

    // ---- character insertion ----------------------------------------------
    // `?SelectCharacter@` (0x1800cfd40) maps a key index to the character it
    // produces under the current modifiers, or 0 for "not a character key".
    // Its body is element_bodies_b5.cpp's door (declared on the class,
    // undefined), reached by address.
    for (int k = 0; k < kKeyCount; ++k) {
        const int off = kKeyBase + k * kKeyStride;
        if (dev[off] == 0 && dev[off + 2] == 0) continue;

        const uint8_t ch = RtAddr<SelectCharFn>(0x1800cfd40ull)(this, k);
        if (ch == 0) continue;

        if (StrLen(_text) >= _maxLength) continue;

        // The filter: when it is a non-empty String, the character must appear
        // in it. When it is null or empty, anything goes.
        char* filter = _filter;
        const int filterLen = StrLen(filter);
        if (filter != nullptr && filterLen > 0) {
            bool found = false;
            for (int m = 0; m < filterLen; ++m) {
                if (static_cast<uint8_t>(_filter[m]) == ch) { found = true; break; }
            }
            if (!found) continue;
        }

        // Build a one-character System::String. Retail does this with a
        // 2-byte stack buffer, two hand-rolled strlen loops and a memcpy of
        // `min(strlen + 1, length + 1)`; transcribed rather than replaced by a
        // one-liner so the length arithmetic stays visible.
        char buf[2];
        buf[0] = static_cast<char>(ch);
        buf[1] = '\0';

        // Both counts are DO-WHILE loops in retail (0x1800d06c0 and
        // 0x1800d06e5): they step once before testing.
        int n = 0;
        int64_t idx = 0;
        do { ++n; ++idx; } while (buf[idx] != 0);

        void* one = nullptr;
        RtAddr<StrSetLengthFn>(0x1800c8220ull)(&one, n);

        const int cap = n + 1;
        int srcLen = 0;
        {
            const char* p = buf;
            do { ++srcLen; ++p; } while (*p != 0);
        }
        ++srcLen;
        const int copy = (cap < srcLen) ? cap : srcLen;
        RtAddr<MemcpyFn>(0x18008a290ull)(one, buf, static_cast<uint64_t>(copy));
        if (srcLen > cap) static_cast<char*>(one)[idx] = 0;

        void* ins = nullptr;
        void** ret = RtAddr<StrInsertAtFn>(0x1800c8510ull)(&_text, &ins, _caret, &one);
        AssignStringInline(reinterpret_cast<void**>(&_text), ret);
        ReleaseString(&ins);
        ReleaseString(&one);

        _caret += 1;
        SetNeedsSizeCalc();
    }
}

// ============================================================================
// ?FinalizeText@EditElement@UI@@UEAAXXZ   0x1800d0da0 — slot 12, INTRODUCED
// ============================================================================
// A bare `RET`. It exists so `UI::NumberEditElement` can override it
// (0x1800d4a60) and clamp its value when the box loses focus. Ported as the
// empty body it is — see reference_retail_ships_empty_bodies.md.
void EditElement::FinalizeText() {}

}  // namespace UI

// ============================================================================
// Constructor / destructor — DOOR BODIES (no real ctor/dtor, B4b-2)
// ============================================================================
extern "C" void* RtUi_EditElement_CopyCtor(void* selfv, const void* otherv) {
    UI::EditElement* self = reinterpret_cast<UI::EditElement*>(selfv);               // B.3.6 bridge
    const UI::EditElement* other = reinterpret_cast<const UI::EditElement*>(otherv); // B.3.6 bridge
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(selfv, otherv);
    *reinterpret_cast<void**>(selfv) = VtEditElement();
    self->_font = other->_font;

    self->_text = nullptr;
    if (other->_text != nullptr) {
        self->_text = other->_text;
        ++*reinterpret_cast<int32_t*>(self->_text - 4);
    }
    self->_maxLength = other->_maxLength;
    self->_password  = other->_password;

    self->_filter = nullptr;
    if (other->_filter != nullptr) {
        self->_filter = other->_filter;
        ++*reinterpret_cast<int32_t*>(self->_filter - 4);
    }
    self->_active  = other->_active;
    self->_caret   = other->_caret;
    self->_blinkOn = other->_blinkOn;
    self->_blinkAccumulator = other->_blinkAccumulator;
    return selfv;
}

// ??1EditElement@UI@@UEAA@XZ  0x1800d0e50 — own vtable, DROP the text-input
// latch if this element still holds it, release the filter and the text, then
// ~Element. The latch decrement is the destructor's half of Update's pairing;
// destroying a focused edit box without it deafens the game.
extern "C" void RtUi_EditElement_Dtor(void* selfv) {
    UI::EditElement* self = reinterpret_cast<UI::EditElement*>(selfv);   // B.3.6 bridge
    *reinterpret_cast<void**>(selfv) = VtEditElement();

    if (self->_active != 0) {
        void** cfg = InputConfigSlot();
        --**reinterpret_cast<int32_t**>(*cfg);
    }
    ReleaseString(reinterpret_cast<void**>(&self->_filter));
    ReleaseString(reinterpret_cast<void**>(&self->_text));

    RtAddr<ElementDtorFn>(0x1801161a0ull)(selfv);
}

// ============================================================================
// The registered doors — one QUALIFIED delegation each.
// ============================================================================
extern "C" uint32_t RtUi_EditElement_Type(const void* self) {
    return reinterpret_cast<const UI::EditElement*>(self)->UI::EditElement::Type();   // B.3.6 bridge
}
extern "C" const char* RtUi_EditElement_TypeName(const void* self) {
    return reinterpret_cast<const UI::EditElement*>(self)->UI::EditElement::TypeName();   // B.3.6 bridge
}
extern "C" void RtUi_EditElement_ComputeSize(void* self) {
    reinterpret_cast<UI::EditElement*>(self)->UI::EditElement::ComputeSize();   // B.3.6 bridge
}
extern "C" void RtUi_EditElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    reinterpret_cast<UI::EditElement*>(self)->UI::EditElement::ComputeLayout(x, y, w, h);   // B.3.6 bridge
}
extern "C" void RtUi_EditElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    reinterpret_cast<const UI::EditElement*>(self)->UI::EditElement::Draw(   // B.3.6 bridge
        *static_cast<UI::SpriteContext*>(ctx), alpha);
}
// R9 is the ADDRESS of the caller's InternalPtr<DialogInstance> (B6-11).
extern "C" void RtUi_EditElement_Update(void* self, const void* time, void* ui,
                                        void** dlg, const void* state) {
    reinterpret_cast<UI::EditElement*>(self)->UI::EditElement::Update(   // B.3.6 bridge
        *static_cast<const System::Time*>(time), static_cast<UI::UserInterface*>(ui),
        reinterpret_cast<Object::InternalPtr<UI::DialogInstance>*>(dlg),
        *static_cast<const UI::Element::UpdateState*>(state));
}
extern "C" void RtUi_EditElement_FinalizeText(void* self) {
    reinterpret_cast<UI::EditElement*>(self)->UI::EditElement::FinalizeText();   // B.3.6 bridge
}

// ============================================================================
// Registration
// ============================================================================
int RtUiEditElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@EditElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_EditElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x18003fae0ull, (void*)&RtUi_EditElement_TypeName);

    RegisterRuntimeExport("?Type@EditElement@UI@@UEBAIXZ",
                          (void*)&RtUi_EditElement_Type);          ++n;
    RegisterRuntimeAddr(0x18003fac0ull, (void*)&RtUi_EditElement_Type);

    RegisterRuntimeExport("?ComputeSize@EditElement@UI@@UEAAXXZ",
                          (void*)&RtUi_EditElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800d0c10ull, (void*)&RtUi_EditElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@EditElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_EditElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800d0ab0ull, (void*)&RtUi_EditElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@EditElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_EditElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800d0820ull, (void*)&RtUi_EditElement_Draw);

    RegisterRuntimeExport(
        "?Update@EditElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_EditElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1800d0220ull, (void*)&RtUi_EditElement_Update);

    RegisterRuntimeExport("?FinalizeText@EditElement@UI@@UEAAXXZ",
                          (void*)&RtUi_EditElement_FinalizeText);  ++n;
    RegisterRuntimeAddr(0x1800d0da0ull, (void*)&RtUi_EditElement_FinalizeText);

    RegisterRuntimeExport("??0EditElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_EditElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x18003fc90ull, (void*)&RtUi_EditElement_CopyCtor);

    RegisterRuntimeExport("??1EditElement@UI@@UEAA@XZ",
                          (void*)&RtUi_EditElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800d0e50ull, (void*)&RtUi_EditElement_Dtor);
    return n;
}
