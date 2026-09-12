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
// plus ??0EditElement@UI@@QEAA@XZ         0x1800d0f10
//      ??0EditElement@UI@@QEAA@AEBV01@@Z  0x18003fc90
//      ??1EditElement@UI@@UEAA@XZ         0x1800d0e50
//
// Slots 4/5/6/7 are ALSO NumberEditElement's (batch 4 repeats these four
// addresses); registered once, here.
//
// *** THE VTABLE IS THIRTEEN SLOTS, NOT TWELVE. *** `?FinalizeText@` is slot 12
// (+0x60) — one past the end of `UI::Element`'s table as
// runtime/types/ui/Element.h enumerates it. Update calls it through
// `[vtable + 0x60]` at 0x1800d0396, which is how the extra slot is known to
// exist; `?FinalizeText@NumberEditElement@` (0x1800d4a60) is the override that
// proves it is virtual.
//
// `?SetText@EditElement@UI@@` is ALREADY PORTED in
// runtime/src/ui/widgets_input.cpp.
//
// LAYOUT — 0xf8 bytes (`MOV EDX,0xf8` at 0x18003feef). runtime/types/ui/
// EditElement.h already pins `_text` at +0xc8 and the flag at +0xe0; this file
// names the rest from the constructor at 0x1800d0f10 and the four slots:
//
//   +0xc0 UI::Font*      the font (TextElement's slot; EditElement does not
//                        derive from TextElement, it repeats the field)
//   +0xc8 System::String the edited text                       (header: _text)
//   +0xd0 int32          the maximum length, in characters
//   +0xd4 uint8          PASSWORD MODE: draw '*' per character instead
//   +0xd8 System::String the allowed-character filter; empty means "any"
//   +0xe0 uint8          FOCUSED                              (header: _flagE0)
//   +0xe4 int32          the caret position, in characters
//   +0xe8 uint8          caret blink phase (visible when non-zero)
//   +0xf0 int64          blink accumulator, in ticks
//
// `??0EditElement@UI@@QEAA@XZ` (0x1800d0f10) is NOT here: it is already
// ported and registered, table-driven, by runtime/src/ui/factory_ui1.cpp
// (the factory roster). One address, one owner.
// ============================================================================
#include "widget_common.h"
#include "../../types/ui/EditElement.h"

using namespace rtui;

static_assert(offsetof(UI::EditElement, _text)   == 0xC8, "");
static_assert(offsetof(UI::EditElement, _flagE0) == 0xE0, "");

namespace {

constexpr int kFont     = 0xc0;
constexpr int kText     = 0xc8;
constexpr int kMaxLen   = 0xd0;
constexpr int kPassword = 0xd4;
constexpr int kFilter   = 0xd8;
constexpr int kFocused  = 0xe0;
constexpr int kCaret    = 0xe4;
constexpr int kBlinkOn  = 0xe8;
constexpr int kBlinkAcc = 0xf0;

// UI::Font
constexpr int kFontLineHeight = 0x50;
constexpr int kFontBaseline   = 0x54;

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

// UI::Element::UpdateState
constexpr int kStateMouseDown = 0x01;
constexpr int kStateMouseX    = 0x18;
constexpr int kStateMouseY    = 0x1c;

// UI::UserInterface
constexpr int kUiEvents     = 0x48;
constexpr int kUiEventCount = 0x50;

constexpr int32_t kEventEditFinished = 0x15;
constexpr int kSlotFinalizeText = 12;   // +0x60

RTUI_TYPE_SLOTS(EditElement)

void** InputDeviceSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Device@Input@@0PEAV12@EA");
}
void** InputConfigSlot() {
    return RT_DATA_SLOT(void**, "?_singleton@Configuration@Input@@0PEAV12@EA");
}
double SecondsPerTick() {
    double* p = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return p ? *p : 0.0;
}

using MemFreeFn     = void  (*)(void* p);
using ElemUpdateFn  = void  (*)(void* self, const void* time, void* ui,
                                void** dlg, const void* state);
using ElementCtorFn = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn = void  (*)(void* self);
using FinalizeFn    = void  (*)(void* self);
using EventPushFn   = void  (*)(void* array, int newSize, const void* fill);
// UI::Font, the UI-text agent's class; reached by address.
using ComputeLineLayoutFn = int (*)(void* font, const void* text, int len,
                                    int a, int b, int c, int d, void* layout);
using ComputeCharPosFn    = int (*)(void* font, const void* str, int index);
using AddFontFn = void (*)(void* ctx, void* font, const void* str, int wrap,
                           int a, int b, int c, int x, int y, uint32_t color);
// System::String, reached by address.
using StrAppendCharFn = void* (*)(void* self, void* tmpOut, char c);
using StrRemoveAtFn   = void** (*)(void* self, void** out, int index);
using StrAssignFn     = void* (*)(void* self, const void* other);
using StrAssignCStrFn = void* (*)(void* self, const char* s);
using StrSetLengthFn  = void* (*)(void* self, int n);
using StrInsertAtFn   = void** (*)(void* self, void** out, int index, void* const* str);
using MemcpyFn        = void* (*)(void* dst, const void* src, uint64_t n);
using SelectCharFn    = uint8_t (*)(void* self, int keyIndex);

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

void ReleaseString(void** slot) {
    char* s = static_cast<char*>(*slot);
    if (s == nullptr) return;
    int32_t* rc = reinterpret_cast<int32_t*>(s - 4);
    --*rc;
    if (*rc == 0) {
        *slot = s - 8;
        RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(*slot);
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
            RT_EXPORT_ONCE(MemFreeFn, "?MemFree@MemoryAllocator@System@@SAXPEAX@Z")(*dst);
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

RTUI_TYPE_BODIES(EditElement)

// ============================================================================
// ?ComputeSize@EditElement@UI@@UEAAXXZ   0x1800d0c10
// ============================================================================
// One line of text, measured with `UI::Font::ComputeLineLayout` (0x180116c70),
// floored by the element's minimum WIDTH only — the height is the font's line
// height with no floor at all.
//
// In password mode the mask is built FIRST and measured instead, so a password
// box is as wide as its asterisks, not as its secret.
extern "C" void RtUi_EditElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    void* font = P(self, kFont);
    const auto lineLayout = RtAddr<ComputeLineLayoutFn>(0x180116c70ull);

    if (self[kPassword] == 0) {
        void* text = P(self, kText);
        *reinterpret_cast<uint64_t*>(self + kElemLayoutWidth) = 0;
        I32(self, kElemLayoutWidth)  = lineLayout(font, text, StrLen(text),
                                                  0, 0, -1, 0, nullptr);
        I32(self, kElemLayoutHeight) = I32(font, kFontLineHeight);
    } else {
        void* masked = BuildMask(StrLen(P(self, kText)));
        *reinterpret_cast<uint64_t*>(self + kElemLayoutWidth) = 0;
        I32(self, kElemLayoutWidth)  = lineLayout(font, masked, StrLen(masked),
                                                  0, 0, -1, 0, nullptr);
        I32(self, kElemLayoutHeight) = I32(font, kFontLineHeight);
        ReleaseString(&masked);
    }

    if (I32(self, kElemLayoutWidth) < I32(self, kElemMinWidth))
        I32(self, kElemLayoutWidth) = I32(self, kElemMinWidth);
    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@EditElement@UI@@UEAAXHHHH@Z   0x1800d0ab0
// ============================================================================
extern "C" void RtUi_EditElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    ElementComputeAlignedRect(self, x, y, w, h);
}

// ============================================================================
// ?Draw@EditElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d0820
// ============================================================================
// *** THE DISABLED TEST IS NOT THE USUAL ONE. *** Every other Draw in this
// batch ignores the disabled bits. This one returns early when EITHER disabled
// bit is set UNLESS bit 24 (system-disabled) is ALSO set — i.e. a
// user-disabled edit box draws nothing, but a system-disabled one still draws.
// Read off 0x1800d085d..0x1800d086e; inverting it hides every disabled field.
//
// The text is drawn at the element's left edge and a baseline of
// `top - font.baseline + font.lineHeight`, and the caret — a literal "|" —
// one pixel LEFT of the character position the caret index resolves to.
//
// THE CARET'S STRING IS BUILT FROM A C LITERAL. Retail passes the DLL's own
// `"|"` at 0x180156804; our own literal is the same bytes and
// `System::String::operator=` copies them either way.
extern "C" void RtUi_EditElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    uint8_t* self = const_cast<uint8_t*>(static_cast<const uint8_t*>(selfv));

    void* border = P(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    const uint32_t flags = U32(self, kElemFlags);
    if ((flags & kFlagDisabledMask) != 0 && ((flags >> 24) & 1u) == 0) return;

    const uint32_t baseColor = U32(self, kElemColor);
    if ((flags & kFlagDimColor) != 0) alpha = static_cast<uint8_t>(alpha >> 1);
    const uint32_t colour = ComputeColor(baseColor, alpha);

    // The string actually drawn: the mask in password mode, otherwise a
    // REFERENCE to the real text (retail bumps its refcount and releases it at
    // the end, so the field itself is untouched).
    void* shown = nullptr;
    if (self[kPassword] != 0) {
        shown = BuildMask(StrLen(P(self, kText)));
    } else if (P(self, kText) != nullptr) {
        shown = P(self, kText);
        ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(shown) - 4);
    }

    void* font = P(self, kFont);
    const int baseline = I32(self, kElemLayoutTop) - I32(font, kFontBaseline) +
                         I32(font, kFontLineHeight);
    const auto addFont = RtAddr<AddFontFn>(0x18011ccb0ull);
    addFont(ctx, font, &shown, 0, 0, 0, -1, I32(self, kElemLayoutLeft), baseline, colour);

    if (self[kBlinkOn] != 0 && self[kFocused] != 0) {
        const int cx = RtAddr<ComputeCharPosFn>(0x180116ee0ull)(
            P(self, kFont), &shown, I32(self, kCaret)) - 1;

        void* caret = nullptr;
        static const char kCaretText[] = "|";
        RtAddr<StrAssignCStrFn>(0x1800c95d0ull)(&caret, kCaretText);

        void* f = P(self, kFont);
        const int y = I32(self, kElemLayoutTop) - I32(f, kFontBaseline) +
                      I32(f, kFontLineHeight);
        addFont(ctx, P(self, kFont), &caret, 0, 0, 0, -1,
                I32(self, kElemLayoutLeft) + cx, y, colour);
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
// stop treating keys as game bindings" latch. Getting that pairing wrong leaves
// the game permanently unable to read a key, or permanently deaf to text.
//
// DROPPING focus calls virtual slot 12 (`FinalizeText`) and raises event 0x15.
extern "C" void RtUi_EditElement_Update(void* selfv, const void* time, void* ui,
                                        void** dlg, const void* statev) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* st = static_cast<const uint8_t*>(statev);

    {
        void* dlgCopy = *dlg;
        RtAddr<ElemUpdateFn>(0x180116270ull)(self, time, ui, &dlgCopy, st);
    }

    void** devSlot = InputDeviceSlot();
    const uint8_t* dev = devSlot ? static_cast<const uint8_t*>(*devSlot) : nullptr;

    if ((U32(self, kElemFlags) & kFlagDisabledMask) != 0) return;

    // ---- take focus -------------------------------------------------------
    if (st[kStateMouseDown] != 0 && ((U32(self, kElemFlags) >> 17) & 1u) != 0) {
        if (self[kFocused] == 0) {
            void** cfg = InputConfigSlot();
            ++**reinterpret_cast<int32_t**>(*cfg);
        }
        self[kFocused] = 1;
        self[kBlinkOn] = 1;
        *reinterpret_cast<int64_t*>(self + kBlinkAcc) = 0;
        I32(self, kCaret) = StrLen(P(self, kText));

        const int relX = I32c(st, kStateMouseX) - I32(self, kElemLayoutLeft);
        for (int i = 0; i < StrLen(P(self, kText)); ++i) {
            const int cx = RtAddr<ComputeCharPosFn>(0x180116ee0ull)(
                P(self, kFont), self + kText, i + 1);
            if (relX >= 0 && relX <= cx) { I32(self, kCaret) = i; break; }
        }
    }

    if (self[kFocused] == 0) return;

    // ---- drop focus -------------------------------------------------------
    const bool clickedAway = st[kStateMouseDown] != 0 &&
                             (U32(self, kElemFlags) & 0x20000u) == 0;
    if (clickedAway || dev[kKeyBase] != 0 || dev[kKeyEscape] != 0 || dev[kKeyEnter] != 0) {
        self[kFocused] = 0;
        void** cfg = InputConfigSlot();
        --**reinterpret_cast<int32_t**>(*cfg);

        VSlot<FinalizeFn>(self, kSlotFinalizeText)(self);
        SetNeedsSizeCalc(self);

        Event ev;
        ev.dialog    = *dlg;
        ev.element   = self;
        ev.kind      = kEventEditFinished;
        ev.elementId = I32(self, kElemElementId);
        ev.x         = I32c(st, kStateMouseX);
        ev.y         = I32c(st, kStateMouseY);
        ev.index     = 0;
        ev.tail      = 0;
        RtAddr<EventPushFn>(0x180033690ull)(static_cast<uint8_t*>(ui) + kUiEvents,
                                            I32(ui, kUiEventCount) + 1, &ev);
    }

    if (self[kFocused] == 0) return;

    // ---- blink ------------------------------------------------------------
    int64_t& acc = *reinterpret_cast<int64_t*>(self + kBlinkAcc);
    acc += *static_cast<const int64_t*>(time);
    if (acc > static_cast<int64_t>(0.5 / SecondsPerTick())) {
        self[kBlinkOn] = (self[kBlinkOn] == 0) ? 1 : 0;
        acc = 0;
    }

    // ---- caret movement ---------------------------------------------------
    if (dev[kKeyLeft] != 0 || dev[kKeyLeft + 2] != 0) {
        I32(self, kCaret) -= 1;
        if (I32(self, kCaret) < 0) I32(self, kCaret) = 0;
    }
    if (dev[kKeyRight] != 0 || dev[kKeyRight + 2] != 0) {
        I32(self, kCaret) += 1;
        const int len = StrLen(P(self, kText));
        if (I32(self, kCaret) > len) I32(self, kCaret) = len;
    }
    if (dev[kKeyHome] != 0) I32(self, kCaret) = 0;
    if (dev[kKeyEnd]  != 0) I32(self, kCaret) = StrLen(P(self, kText));

    // ---- backspace / delete ----------------------------------------------
    const auto removeAt = RtAddr<StrRemoveAtFn>(0x1800c86b0ull);
    const auto assign   = RtAddr<StrAssignFn>(0x1800c9810ull);

    if ((dev[kKeyBack] != 0 || dev[kKeyBack + 2] != 0) && I32(self, kCaret) > 0) {
        void* tmp = nullptr;
        void** ret = removeAt(self + kText, &tmp, I32(self, kCaret) - 1);
        assign(self + kText, ret);
        ReleaseString(&tmp);
        I32(self, kCaret) -= 1;
        SetNeedsSizeCalc(self);
    }

    if (dev[kKeyDelete] != 0 || dev[kKeyDelete + 2] != 0) {
        if (I32(self, kCaret) < StrLen(P(self, kText))) {
            void* tmp = nullptr;
            void** ret = removeAt(self + kText, &tmp, I32(self, kCaret));
            assign(self + kText, ret);
            ReleaseString(&tmp);
            SetNeedsSizeCalc(self);
        }
    }

    // ---- character insertion ----------------------------------------------
    // `?SelectCharacter@` (0x1800cfd40) maps a key index to the character it
    // produces under the current modifiers, or 0 for "not a character key".
    // It belongs to the UI-text agent and is reached by address.
    for (int k = 0; k < kKeyCount; ++k) {
        const int off = kKeyBase + k * kKeyStride;
        if (dev[off] == 0 && dev[off + 2] == 0) continue;

        const uint8_t ch = RtAddr<SelectCharFn>(0x1800cfd40ull)(self, k);
        if (ch == 0) continue;

        if (StrLen(P(self, kText)) >= I32(self, kMaxLen)) continue;

        // The filter: when it is a non-empty String, the character must appear
        // in it. When it is null or empty, anything goes.
        void* filter = P(self, kFilter);
        const int filterLen = StrLen(filter);
        if (filter != nullptr && filterLen > 0) {
            bool found = false;
            for (int m = 0; m < filterLen; ++m) {
                if (static_cast<const uint8_t*>(P(self, kFilter))[m] == ch) { found = true; break; }
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
        // 0x1800d06e5): they step once before testing, so the count is one
        // more than a leading-NUL-stopping while loop would give.
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
        void** ret = RtAddr<StrInsertAtFn>(0x1800c8510ull)(
            self + kText, &ins, I32(self, kCaret), &one);
        AssignStringInline(reinterpret_cast<void**>(self + kText), ret);
        ReleaseString(&ins);
        ReleaseString(&one);

        I32(self, kCaret) += 1;
        SetNeedsSizeCalc(self);
    }
}

// ============================================================================
// ?FinalizeText@EditElement@UI@@UEAAXXZ   0x1800d0da0
// ============================================================================
// A bare `RET`. It exists so `UI::NumberEditElement` can override it
// (0x1800d4a60) and clamp its value when the box loses focus. Ported as the
// empty body it is — see reference_retail_ships_empty_bodies.md.
extern "C" void RtUi_EditElement_FinalizeText(void*) {}

// ============================================================================
// Constructors / destructor
// ============================================================================

extern "C" void* RtUi_EditElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtEditElement();
    P(self, kFont) = Pc(other, kFont);

    P(self, kText) = nullptr;
    if (Pc(other, kText) != nullptr) {
        P(self, kText) = Pc(other, kText);
        ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(P(self, kText)) - 4);
    }
    I32(self, kMaxLen) = I32c(other, kMaxLen);
    self[kPassword] = other[kPassword];

    P(self, kFilter) = nullptr;
    if (Pc(other, kFilter) != nullptr) {
        P(self, kFilter) = Pc(other, kFilter);
        ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(P(self, kFilter)) - 4);
    }
    self[kFocused] = other[kFocused];
    I32(self, kCaret) = I32c(other, kCaret);
    self[kBlinkOn] = other[kBlinkOn];
    *reinterpret_cast<uint64_t*>(self + kBlinkAcc) =
        *reinterpret_cast<const uint64_t*>(other + kBlinkAcc);
    return self;
}

// ??1EditElement@UI@@UEAA@XZ  0x1800d0e50 — own vtable, DROP the text-input
// latch if this element still holds it, release the filter and the text, then
// ~Element. The latch decrement is the destructor's half of Update's pairing;
// destroying a focused edit box without it deafens the game.
extern "C" void RtUi_EditElement_Dtor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    P(self, 0x00) = VtEditElement();

    if (self[kFocused] != 0) {
        void** cfg = InputConfigSlot();
        --**reinterpret_cast<int32_t**>(*cfg);
    }
    ReleaseString(reinterpret_cast<void**>(self + kFilter));
    ReleaseString(reinterpret_cast<void**>(self + kText));

    RtAddr<ElementDtorFn>(0x1801161a0ull)(self);
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
