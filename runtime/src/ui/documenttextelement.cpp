// ============================================================================
// runtime/src/ui/documenttextelement.cpp — UI::DocumentTextElement
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7DocumentTextElement@UI@@6B@` (retail
// 0x1801566f0). The rich-text block: a laid-out run of text ITEMS, some of
// which are inline sprites or sprite captures, with clickable LINK spans over
// them.
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x18003c650  deleting_dtor.cpp row
//    1    TypeName              0x18003be50  here
//    2    Type                  0x18003be30  here
//    4    ComputeSize           0x1800cdb60  here
//    5    ComputeLayout         0x1800cda00  here
//    6    Draw                  0x1800cd7a0  here
//    7    Update                0x1800cd360  here
//
// plus ??0DocumentTextElement@UI@@QEAA@XZ         0x1800cf410
//      ??0DocumentTextElement@UI@@QEAA@AEBV01@@Z  0x18003c140
//      ??1DocumentTextElement@UI@@UEAA@XZ         0x1800cf320
//
// `?SetText@` (both overloads) and `?GetLink@` are ALREADY PORTED in
// runtime/src/ui/widgets_layout.cpp; `?GenerateItems@` (0x1800cf060), the
// layout pass ComputeSize calls, belongs to the UI-text agent and is reached by
// address.
//
// ---------------------------------------------------------------------------
// LAYOUT — 0x1c8 bytes (`MOV EDX,0x1c8` at 0x18003c66f). Enumerated from the
// constructor at 0x1800cf410 and the destructor at 0x1800cf320; the three
// offsets runtime/types/ui/DocumentTextElement.h already pins are marked.
//
//   +0x0c0 UI::String   the by-value text (header: kUiStringValue)
//   +0x0d8 UI::Sprite   the 1px UNDERLINE sprite (sheet +0x0d8, index +0x0e0)
//   +0x0e8 Array<UI::Sprite>       inline sprite table
//   +0x0f8 uint32       link colour, normal   (ctor 0xffeeee00)
//   +0x0fc uint32       link colour, hovered  (ctor 0xffffff00)
//   +0x100 uint32       link colour, pressed  (ctor 0xffcccc00)
//   +0x104 int32        ctor 4
//   +0x108 int32        the WRAP WIDTH, ctor 300 — ComputeSize floors
//                       `_layoutWidth` by it
//   +0x110 Array<UI::Font*>        the font table items index into
//   +0x120 Array<UI::Sprite>       the sprite table inline images index into
//   +0x130 Array<...>              a third table, destroyed by 0x18003db40
//   +0x140 ExternalPtr             an object whose +0x20 is the array inline
//                                  CAPTURES index into
//   +0x148 Array<Item>  stride 0x24 — the laid-out run (see below)
//   +0x158 Array<Link>  stride 0x18 (header: kLinkArrayData)
//   +0x168 System::String  the source text (header: kContentString)
//   +0x170 .. +0x197  NOT WRITTEN BY THE CONSTRUCTOR; copied by the copy ctor
//   +0x198 System::String
//   +0x1a0 int32 -1, +0x1a4 int32 -1, +0x1a8 uint8 0
//   +0x1b0 .. +0x1c7  six more dwords, copied by the copy ctor
//
// ITEM (stride 0x24), from Draw and Update:
//   +0x00 int32 x       +0x04 int32 y      +0x08 int32 width   +0x0c int32 height
//   +0x10 int32 substring START (kind 0) / sprite INDEX (kind 1)
//   +0x14 int32 substring LENGTH
//   +0x18 uint32 colour  +0x1c int32 table index
//   +0x20 uint8 kind: 0 text, 1 sprite, 2 capture
//   +0x21 uint8 underline: 0 none, 1 full width, 2 width minus the font's +0x5c
//
// LINK (stride 0x18):
//   +0x08 int32 first item   +0x0c int32 one past last   +0x10 uint8 armed
//
// `??0DocumentTextElement@UI@@QEAA@XZ` (0x1800cf410) is NOT here: it is already
// ported and registered, table-driven, by runtime/src/ui/factory_ui1.cpp
// (the factory roster). One address, one owner.
// ============================================================================
#include "widget_common.h"
#include "../../types/ui/DocumentTextElement.h"

using namespace rtui;

namespace {

constexpr int kUnderlineSprite = 0x0d8;
constexpr int kColorNormal     = 0x0f8;
constexpr int kColorHover      = 0x0fc;
constexpr int kColorPressed    = 0x100;
constexpr int kWrapWidth       = 0x108;
constexpr int kFonts           = 0x110;
constexpr int kSprites         = 0x120;
constexpr int kCaptureOwner    = 0x140;
constexpr int kItems           = 0x148;
constexpr int kItemCount       = 0x150;
constexpr int kLinks           = UI::DocumentTextElementOffsets::kLinkArrayData;  // 0x158
constexpr int kLinkCount       = 0x160;
constexpr int kSourceText      = UI::DocumentTextElementOffsets::kContentString;  // 0x168

static_assert(kLinks == 0x158 && kSourceText == 0x168,
              "DocumentTextElement.h must still pin the link array and the source string");

constexpr int kItemStride = 0x24;
constexpr int kLinkStride = UI::DocumentTextElementOffsets::kLinkEntryStride;   // 0x18

// UI::Element::UpdateState
constexpr int kStateMouseDown = 0x01;
constexpr int kStateMouseUp   = 0x04;
constexpr int kStateMouseHeld = 0x07;
constexpr int kStateMouseX    = 0x18;
constexpr int kStateMouseY    = 0x1c;

// UI::UserInterface
constexpr int kUiEvents      = 0x48;
constexpr int kUiEventCount  = 0x50;
constexpr int kUiAudioScene  = 0x90;
constexpr int kUiHoverSound  = 0xb8;
constexpr int kUiClickSound  = 0xc0;

// UI::Font
constexpr int kFontLineHeight = 0x50;
constexpr int kFontBaseline   = 0x54;
constexpr int kFontUnderlineTrim = 0x5c;

constexpr int32_t kEventActivated = 0x12;

RTUI_TYPE_SLOTS(DocumentTextElement)

uint32_t SpriteCaptureTypeId() {
    const uint32_t* p = RT_DATA_SLOT(const uint32_t*,
        "?_typeInfo@SpriteCapture@UI@@0VBaseTypeInfo@Object@@A");
    return p ? *p : 0u;
}
double SecondsPerTick() {
    double* p = RT_DATA_SLOT(double*, "?_secondsPerTick@Timer@System@@0NA");
    return p ? *p : 0.0;
}

using IsDerivedFromFn = bool  (*)(const void* self, uint32_t typeId);
using ClearArrayFn    = void* (*)(void* array);
using GenerateItemsFn = void  (*)(void* self);
using SubStringFn     = void* (*)(const void* str, void* out, int start, int length);
using MemFreeFn       = void  (*)(void* p);
using AddFontFn       = void (*)(void* ctx, void* font, const void* str, int wrap,
                                 int a, int b, int c, int x, int y, uint32_t color);
using AddSpritePtFn   = void (*)(void* ctx, const void* sprite, int x, int y, uint32_t color);
using AddSpriteRectFn = void (*)(void* ctx, const void* sprite,
                                 int l, int t, int r, int b, uint32_t color);
using CaptureDrawFn   = void (*)(void* capture, void* ctx, int x, int y);
using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
using ArrayDtorFn       = void* (*)(void* array);
using ArrayCopyFn       = void* (*)(void* dst, const void* src);
using EventPushFn       = void  (*)(void* array, int newSize, const void* fill);

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

// `System::String::~String` inlined: release one reference, and when it reaches
// zero rewrite the FIELD to `data - 8` and free that. The asymmetry (nulling
// the field on the non-zero arm, leaving a freed pointer on the zero arm) is
// retail's and is the same one runtime/src/ui/groupdescription.cpp documents.
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

// `UI::UserInterface::Play`, INLINED by this body (ButtonElement calls the
// out-of-line 0x1801223d0 instead). Walk the audio scene's 256 voice slots for
// one whose +0x68 is zero, take it, and stamp a generation counter into it.
//
// `(int64)(0.0 / _secondsPerTick)` is retail's start time. It is zero, and it
// is computed the way retail computes it rather than written as 0, because the
// division is what the bytes do — and because `_secondsPerTick` being zero
// would make it a NaN there too.
void PlayInline(void* ui, int soundField) {
    void* sound = Pc(ui, soundField);
    if (sound == nullptr) return;
    uint8_t* scene = static_cast<uint8_t*>(P(ui, kUiAudioScene));
    if (scene == nullptr) return;

    const int64_t start = static_cast<int64_t>(0.0 / SecondsPerTick());
    uint8_t* voices = static_cast<uint8_t*>(P(scene, 0x10));
    for (int64_t i = 0; i < 0x100; ++i) {
        uint8_t* v = voices + i * 0x70;
        if (*reinterpret_cast<const int32_t*>(v + 0x68) != 0) continue;
        P(v, 0x00) = sound;
        *reinterpret_cast<int64_t*>(v + 0x10) = start;
        I32(v, 0x08) = 1;
        I32(scene, 0x08) += 1;
        I32(v, 0x68) = static_cast<int32_t>(i) * 2 | (I32(scene, 0x08) << 9);
        return;
    }
}

}  // namespace

RTUI_TYPE_BODIES(DocumentTextElement)

// ============================================================================
// ?ComputeSize@DocumentTextElement@UI@@UEAAXXZ   0x1800cdb60
// ============================================================================
// Throw away the previous item and link lists, floor the width at the wrap
// width, zero the height, and let `GenerateItems` (0x1800cf060) re-lay the text
// and fill both lists — it is what actually writes `_layoutHeight`.
//
// NOTE THE ASYMMETRY: the WIDTH is a floor (`if (_layoutWidth < _wrapWidth)`),
// so a document inside a wider box keeps the box's width, while the HEIGHT is
// unconditionally reset to 0 before the layout pass.
extern "C" void RtUi_DocumentTextElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;

    RtAddr<ClearArrayFn>(0x18003d1c0ull)(self + kItems);
    RtAddr<ClearArrayFn>(0x18003d430ull)(self + kLinks);

    if (I32(self, kElemLayoutWidth) < I32(self, kWrapWidth))
        I32(self, kElemLayoutWidth) = I32(self, kWrapWidth);
    I32(self, kElemLayoutHeight) = 0;

    RtAddr<GenerateItemsFn>(0x1800cf060ull)(self);

    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
}

// ============================================================================
// ?ComputeLayout@DocumentTextElement@UI@@UEAAXHHHH@Z   0x1800cda00
// ============================================================================
extern "C" void RtUi_DocumentTextElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    ElementComputeAlignedRect(self, x, y, w, h);
}

// ============================================================================
// ?Draw@DocumentTextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800cd7a0
// ============================================================================
// One pass over the item list. There is NO alpha modulation anywhere in this
// body: each item carries its own colour (Update computes it), and the `alpha`
// parameter is loaded, halved by the dim flag and then never used — the halving
// is dead code in the shipped function and is reproduced only in that it costs
// nothing.
//
// The three item kinds:
//   0  TEXT     — a SUBSTRING of `_sourceText` (start +0x10, length +0x14),
//                 drawn with `_fonts[+0x1c]`, then optionally an underline bar
//                 one pixel below the baseline. The temporary String is
//                 released before the next item.
//   1  SPRITE   — a UI::Sprite built ON THE STACK from `_sprites[+0x1c]` and
//                 the item's +0x10, drawn at natural size in OPAQUE WHITE.
//   2  CAPTURE  — a `UI::SpriteCapture` fetched from `_captureOwner->[+0x20]`,
//                 type-tested, and given its own Draw. A failed type test
//                 yields null and retail calls through it.
extern "C" void RtUi_DocumentTextElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    const auto addFont   = RtAddr<AddFontFn>(0x18011ccb0ull);
    const auto addPt     = RtAddr<AddSpritePtFn>(0x18011cf00ull);
    const auto addRect   = RtAddr<AddSpriteRectFn>(0x18011d3c0ull);
    const auto subString = RtAddr<SubStringFn>(0x1800c9230ull);

    const int32_t count = I32c(self, kItemCount);
    for (int32_t i = 0; i < count; ++i) {
        const uint8_t* item = static_cast<const uint8_t*>(Pc(self, kItems)) +
                              static_cast<int64_t>(i) * kItemStride;
        const uint8_t kind = item[0x20];
        const int L = I32c(self, kElemLayoutLeft);
        const int T = I32c(self, kElemLayoutTop);

        if (kind == 0) {
            void* tmp = nullptr;
            subString(self + kSourceText, &tmp,
                      *reinterpret_cast<const int32_t*>(item + 0x10),
                      *reinterpret_cast<const int32_t*>(item + 0x14));

            void* font = static_cast<void* const*>(Pc(self, kFonts))[
                             *reinterpret_cast<const int32_t*>(item + 0x1c)];
            addFont(ctx, font, &tmp, 0, 0, 0, -1,
                    *reinterpret_cast<const int32_t*>(item + 0x00) + L,
                    *reinterpret_cast<const int32_t*>(item + 0x04) + T,
                    *reinterpret_cast<const uint32_t*>(item + 0x18));

            const uint8_t underline = item[0x21];
            if (underline == 1 || underline == 2) {
                int width = *reinterpret_cast<const int32_t*>(item + 0x08);
                if (underline == 2) {
                    const void* f = static_cast<void* const*>(Pc(self, kFonts))[
                                        *reinterpret_cast<const int32_t*>(item + 0x1c)];
                    width -= I32c(f, kFontUnderlineTrim);
                }
                const int x = *reinterpret_cast<const int32_t*>(item + 0x00) + L;
                const int y = *reinterpret_cast<const int32_t*>(item + 0x04) + T;
                addRect(ctx, self + kUnderlineSprite, x, y + 1, width + x, y + 2,
                        *reinterpret_cast<const uint32_t*>(item + 0x18));
            }
            ReleaseString(&tmp);
        } else if (kind == 1) {
            struct { void* sheet; int32_t index; int32_t pad; } sprite;
            sprite.sheet = static_cast<void* const*>(Pc(self, kSprites))[
                               *reinterpret_cast<const int32_t*>(item + 0x1c)];
            sprite.index = *reinterpret_cast<const int32_t*>(item + 0x10);
            addPt(ctx, &sprite,
                  L + *reinterpret_cast<const int32_t*>(item + 0x00),
                  T + *reinterpret_cast<const int32_t*>(item + 0x04),
                  0xffffffffu);
        } else if (kind == 2) {
            void* obj = static_cast<void* const*>(Pc(Pc(self, kCaptureOwner), 0x20))[
                            *reinterpret_cast<const int32_t*>(item + 0x1c)];
            void* capture = RtAddr<IsDerivedFromFn>(0x1800c0390ull)(obj, SpriteCaptureTypeId())
                                ? obj : nullptr;
            RtAddr<CaptureDrawFn>(0x1800d9230ull)(
                capture, ctx,
                L + *reinterpret_cast<const int32_t*>(item + 0x00),
                T + *reinterpret_cast<const int32_t*>(item + 0x04));
        }
    }
}

// ============================================================================
// ?Update@DocumentTextElement@UI@@   0x1800cd360
// ============================================================================
// Link hit testing and colouring, one link span at a time.
//
// For each link: walk its items looking for one under the cursor. A TEXT item's
// vertical extent is shifted by `font.baseline - font.lineHeight` because its
// stored Y is a baseline, not a top edge; sprite and capture items are not.
// Then colour every TEXT item in the span, arm the underline on them, and raise
// the hover / click sounds and the activation event.
//
// *** THE UNDERLINE FLAG ON THE LAST ITEM OF A PRESSED LINK IS 2, NOT 1. ***
// `if (j == last - 1 && pressed) item[0x21] = 2 else item[0x21] = pressed`
// (0x1800cd5xx). Draw reads that 2 as "full width minus the font's +0x5c", i.e.
// the trailing space is not underlined. A port that writes `pressed` uniformly
// draws one glyph too much of rule on every link.
//
// `UI::UserInterface::Play` is INLINED here, twice — see PlayInline above.
extern "C" void RtUi_DocumentTextElement_Update(void* selfv, const void* time, void* ui,
                                                void** dlg, const void* statev) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* st = static_cast<const uint8_t*>(statev);

    {
        void* dlgCopy = *dlg;
        using ElemUpdateFn = void (*)(void*, const void*, void*, void**, const void*);
        RtAddr<ElemUpdateFn>(0x180116270ull)(self, time, ui, &dlgCopy, st);
    }

    if ((U32(self, kElemFlags) & kFlagHidden) != 0) return;

    const int32_t linkCount = I32(self, kLinkCount);
    for (int32_t li = 0; li < linkCount; ++li) {
        uint8_t* link = static_cast<uint8_t*>(P(self, kLinks)) +
                        static_cast<int64_t>(li) * kLinkStride;

        uint32_t colour = U32(self, kColorNormal);
        bool hit = false;
        bool pressed = false;

        if ((U32(self, kElemFlags) & 0x20000u) != 0) {
            int32_t j = I32(link, 0x08);
            const int32_t end = I32(link, 0x0c);
            for (; j < end; ++j) {
                uint8_t* item = static_cast<uint8_t*>(P(self, kItems)) +
                                static_cast<int64_t>(j) * kItemStride;
                int y0 = I32(item, 0x04) + I32(self, kElemLayoutTop);
                int y1 = I32(item, 0x0c) + y0;
                if (item[0x20] == 0) {
                    const void* f = static_cast<void* const*>(P(self, kFonts))[I32(item, 0x1c)];
                    const int adj = I32c(f, kFontBaseline) - I32c(f, kFontLineHeight);
                    y0 += adj;
                    y1 += adj;
                }
                const int x0 = I32(item, 0x00) + I32(self, kElemLayoutLeft);
                const int x1 = I32(item, 0x08) + x0;
                if (x0 <= I32c(st, kStateMouseX) && I32c(st, kStateMouseX) < x1 &&
                    y0 <= I32c(st, kStateMouseY) && I32c(st, kStateMouseY) < y1) {
                    hit = true;
                    break;
                }
            }
        }

        if (st[kStateMouseDown] != 0 && hit) link[0x10] = 1;

        if ((U32(self, kElemFlags) & kFlagDisabledMask) == 0) {
            if (hit) {
                colour = (link[0x10] != 0 && st[kStateMouseHeld] != 0)
                             ? U32(self, kColorPressed) : U32(self, kColorHover);
                pressed = true;
            } else {
                link[0x10] = 0;
            }
        } else {
            colour = U32(self, kColorNormal);
            if (!hit) link[0x10] = 0;
        }

        for (int32_t j = I32(link, 0x08); j < I32(link, 0x0c); ++j) {
            uint8_t* item = static_cast<uint8_t*>(P(self, kItems)) +
                            static_cast<int64_t>(j) * kItemStride;
            if (item[0x20] != 0) continue;
            U32(item, 0x18) = colour;
            if (j == I32(link, 0x0c) - 1 && pressed) item[0x21] = 2;
            else                                     item[0x21] = pressed ? 1 : 0;
        }

        if (st[kStateMouseDown] != 0 && hit && link[0x10] != 0)
            PlayInline(ui, kUiHoverSound);

        if (st[kStateMouseUp] != 0 && hit && link[0x10] != 0) {
            Event ev;
            ev.dialog    = *dlg;
            ev.element   = self;
            ev.kind      = kEventActivated;
            ev.elementId = I32(self, kElemElementId);
            ev.x         = I32c(st, kStateMouseX);
            ev.y         = I32c(st, kStateMouseY);
            ev.index     = li;
            ev.tail      = 0;
            RtAddr<EventPushFn>(0x180033690ull)(static_cast<uint8_t*>(ui) + kUiEvents,
                                                I32(ui, kUiEventCount) + 1, &ev);
            PlayInline(ui, kUiClickSound);
        }
    }
}

// ============================================================================
// Constructors / destructor
// ============================================================================

extern "C" void* RtUi_DocumentTextElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtDocumentTextElement();

    for (int off = 0xc0; off <= 0xe0; off += 8)
        *reinterpret_cast<uint64_t*>(self + off) = *reinterpret_cast<const uint64_t*>(other + off);

    *reinterpret_cast<uint64_t*>(self + 0xe8) = 0;
    *reinterpret_cast<uint64_t*>(self + 0xf0) = 0;
    RtAddr<ArrayCopyFn>(0x180037020ull)(self + 0xe8, other + 0xe8);

    for (int off = 0xf8; off <= 0x108; off += 4)
        I32(self, off) = I32c(other, off);

    *reinterpret_cast<uint64_t*>(self + 0x110) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x118) = 0;
    RtAddr<ArrayCopyFn>(0x18003ce30ull)(self + 0x110, other + 0x110);

    *reinterpret_cast<uint64_t*>(self + 0x120) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x128) = 0;
    RtAddr<ArrayCopyFn>(0x18003cf20ull)(self + 0x120, other + 0x120);

    *reinterpret_cast<uint64_t*>(self + 0x130) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x138) = 0;
    RtAddr<ArrayCopyFn>(0x18003d010ull)(self + 0x130, other + 0x130);

    *reinterpret_cast<uint64_t*>(self + 0x140) = *reinterpret_cast<const uint64_t*>(other + 0x140);

    *reinterpret_cast<uint64_t*>(self + 0x148) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x150) = 0;
    RtAddr<ArrayCopyFn>(0x18003d0f0ull)(self + 0x148, other + 0x148);

    *reinterpret_cast<uint64_t*>(self + 0x158) = 0;
    *reinterpret_cast<uint64_t*>(self + 0x160) = 0;
    RtAddr<ArrayCopyFn>(0x18003d360ull)(self + 0x158, other + 0x158);

    // The two String fields: zero, then take a reference if the source has one.
    P(self, 0x168) = nullptr;
    if (Pc(other, 0x168) != nullptr) {
        P(self, 0x168) = Pc(other, 0x168);
        ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(P(self, 0x168)) - 4);
    }
    for (int off = 0x170; off <= 0x190; off += 4)
        I32(self, off) = I32c(other, off);
    P(self, 0x198) = nullptr;
    if (Pc(other, 0x198) != nullptr) {
        P(self, 0x198) = Pc(other, 0x198);
        ++*reinterpret_cast<int32_t*>(static_cast<uint8_t*>(P(self, 0x198)) - 4);
    }
    I32(self, 0x1a0) = I32c(other, 0x1a0);
    I32(self, 0x1a4) = I32c(other, 0x1a4);
    self[0x1a8] = other[0x1a8];
    for (int off = 0x1b0; off <= 0x1c4; off += 4)
        I32(self, off) = I32c(other, off);
    return self;
}

// ??1DocumentTextElement@UI@@UEAA@XZ  0x1800cf320 — own vtable, both Strings
// released, six arrays destroyed in retail's order, then a TAIL JUMP to
// ~Element.
extern "C" void RtUi_DocumentTextElement_Dtor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    P(self, 0x00) = VtDocumentTextElement();

    ReleaseString(reinterpret_cast<void**>(self + 0x198));
    ReleaseString(reinterpret_cast<void**>(self + 0x168));

    RtAddr<ArrayDtorFn>(0x18003e310ull)(self + 0x158);
    RtAddr<ArrayDtorFn>(0x18003e0c0ull)(self + 0x148);
    RtAddr<ArrayDtorFn>(0x18003db40ull)(self + 0x130);
    RtAddr<ArrayDtorFn>(0x18003da00ull)(self + 0x120);
    RtAddr<ArrayDtorFn>(0x18003d8c0ull)(self + 0x110);
    RtAddr<ArrayDtorFn>(0x180037310ull)(self + 0x0e8);

    RtAddr<ElementDtorFn>(0x1801161a0ull)(self);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiDocumentTextElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@DocumentTextElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_DocumentTextElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x18003be50ull, (void*)&RtUi_DocumentTextElement_TypeName);

    RegisterRuntimeExport("?Type@DocumentTextElement@UI@@UEBAIXZ",
                          (void*)&RtUi_DocumentTextElement_Type);          ++n;
    RegisterRuntimeAddr(0x18003be30ull, (void*)&RtUi_DocumentTextElement_Type);

    RegisterRuntimeExport("?ComputeSize@DocumentTextElement@UI@@UEAAXXZ",
                          (void*)&RtUi_DocumentTextElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800cdb60ull, (void*)&RtUi_DocumentTextElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@DocumentTextElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_DocumentTextElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800cda00ull, (void*)&RtUi_DocumentTextElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@DocumentTextElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_DocumentTextElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800cd7a0ull, (void*)&RtUi_DocumentTextElement_Draw);

    RegisterRuntimeExport(
        "?Update@DocumentTextElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_DocumentTextElement_Update);                          ++n;
    RegisterRuntimeAddr(0x1800cd360ull, (void*)&RtUi_DocumentTextElement_Update);

    RegisterRuntimeExport("??0DocumentTextElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_DocumentTextElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x18003c140ull, (void*)&RtUi_DocumentTextElement_CopyCtor);

    RegisterRuntimeExport("??1DocumentTextElement@UI@@UEAA@XZ",
                          (void*)&RtUi_DocumentTextElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800cf320ull, (void*)&RtUi_DocumentTextElement_Dtor);
    return n;
}
