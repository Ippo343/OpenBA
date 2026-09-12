// ============================================================================
// runtime/src/ui/progresselement.cpp — UI::ProgressElement, the whole class
// ============================================================================
//
// `??_7ProgressElement@UI@@6B@` (retail 0x180159180):
//
//   0  vector deleting dtor  0x180078150  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x180077d40  here
//   2  Type                  0x180077d20  here
//   4  ComputeSize           0x1801198f0  here
//   5  ComputeLayout         0x180119610  here
//   6  Draw                  0x1801192c0  here
//
// plus ??0ProgressElement (0x180119c40) and ??1ProgressElement (0x180077ef0).
//
// ---------------------------------------------------------------------------
// LAYOUT. sizeof == 0x130 (the deleting destructor's `MOV EDX,0x130`). SIX
// UI::Sprites and three scalars, every one written by the ctor:
//
//   +0xC0  _fillLeft    Sprite    }  the FILL (the coloured bar), drawn with
//   +0xD0  _fillMiddle  Sprite    }  Element::_color
//   +0xE0  _fillRight   Sprite    }
//   +0xF0  _trackLeft   Sprite    }  the TRACK (the groove), drawn with
//   +0x100 _trackMiddle Sprite    }  _backgroundColor
//   +0x110 _trackRight  Sprite    }
//   +0x120 _backgroundColor uint32, ctor 0xFFFFFFFF
//                                ?SetBackgroundColor@ 0x180077ee0
//   +0x124 _maxValue        float,  ctor 1.0f    ?SetMaximumValue@ 0x180077ec0
//   +0x128 _currentValue    float,  ctor 0.25f   ?SetCurrentValue@ 0x180077ed0
//
// runtime/types/ui/ProgressElement.h already pinned the last two from their
// one-instruction setters and left +0xC0..+0x123 as "not reversed by this
// lane"; it is reversed now, and the header says so.
//
// ~ProgressElement does NOT restamp the vtable — it is a bare tail jump to
// ~Element (five instructions at 0x180077ef0, disassembled). That asymmetry
// with its own ctor is retail's and is reproduced.
// ============================================================================

#include "widget_common.h"
#include "../../types/ui/ProgressElement.h"

using namespace rtui;

namespace {

constexpr int kFillLeft    = 0xC0;
constexpr int kFillMiddle  = 0xD0;
constexpr int kFillRight   = 0xE0;
constexpr int kTrackLeft   = 0xF0;
constexpr int kTrackMiddle = 0x100;
constexpr int kTrackRight  = 0x110;
constexpr int kBackColor   = 0x120;
constexpr int kMaxValue    = 0x124;
constexpr int kCurValue    = 0x128;

static_assert(kMaxValue == offsetof(UI::ProgressElement, _maxValue), "");
static_assert(kCurValue == offsetof(UI::ProgressElement, _currentValue), "");

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ProgressElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7ProgressElement@UI@@6B@"); }

}  // namespace

extern "C" uint32_t RtUi_ProgressElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ProgressElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0ProgressElement@UI@@QEAA@XZ   0x180119c40
extern "C" void RtUi_ProgressElement_Ctor(void* self) {
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    const int sprites[6] = { kFillLeft, kFillMiddle, kFillRight,
                             kTrackLeft, kTrackMiddle, kTrackRight };
    for (int off : sprites) {
        PTR(self, off + kSpriteSheet) = nullptr;
        I32(self, off + kSpriteIndex) = -1;
    }
    U32(self, kBackColor) = 0xffffffffu;
    F32(self, kMaxValue)  = 1.0f;      // 0x3f800000
    F32(self, kCurValue)  = 0.25f;     // 0x3e800000
}

// ??1ProgressElement@UI@@UEAA@XZ   0x180077ef0 — tail jump, no restamp.
extern "C" void RtUi_ProgressElement_Dtor(void* self) {
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@ProgressElement@UI@@UEAAXXZ   0x1801198f0
// ---------------------------------------------------------------------------
extern "C" void RtUi_ProgressElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    const int32_t fillW  = SpriteWidth(self, kFillLeft)
                         + SpriteWidth(self, kFillMiddle)
                         + SpriteWidth(self, kFillRight);
    int32_t trackW = SpriteWidth(self, kTrackLeft)
                   + SpriteWidth(self, kTrackMiddle)
                   + SpriteWidth(self, kTrackRight);

    int32_t height = 0;
    if (I32(self, kElChildCount) > 0) {
        void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
        if (child != nullptr) {
            int32_t cw = 0;
            InlineGetPaddedSize(child, &cw, &height);
        }
    }

    if (trackW < fillW) trackW = fillW;
    I32(self, kElLayoutWidth) = trackW;

    int32_t spriteH = SpriteHeight(self, kTrackLeft);
    const int32_t fillH = SpriteHeight(self, kFillLeft);
    if (spriteH < fillH) spriteH = fillH;
    if (height  < spriteH) height = spriteH;

    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElLayoutHeight) = height;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@ProgressElement@UI@@UEAAXHHHH@Z   0x180119610
// ---------------------------------------------------------------------------
extern "C" void RtUi_ProgressElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                   int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    int32_t h = I32(self, kElLayoutHeight);
    const int32_t align = I32(self, kElAlignment);
    if (right  < I32(self, kElLayoutWidth) + x) x = right - I32(self, kElLayoutWidth);
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;

    // Only the vertical stretch bit is tested; the width always stretches.
    if ((U32(self, kElFlags) & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 2: case 5: case 8:
            px = (right - I32(self, kElRightPad)) - aw;
            break;
        default:
            break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutRight)  = aw + px;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutBottom) = py + h;

    if (I32(self, kElChildCount) <= 0) return;
    void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
    if (child == nullptr) return;

    int32_t cw = 0, ch = 0;
    InlineGetPaddedSize(child, &cw, &ch);

    int32_t left  = I32(self, kElLayoutLeft);
    int32_t barW  = static_cast<int32_t>(
        (F32(self, kCurValue) / F32(self, kMaxValue))
        * static_cast<float>(I32(self, kElLayoutRight) - left));
    if (barW < I32(self, kElLayoutWidth)) barW = I32(self, kElLayoutWidth);

    uint32_t color;
    if (F32(self, kCurValue) == 0.0f) {
        if (I32(self, kFillLeft + kSpriteIndex) < 0) {
            color = 0xffffffffu;
        } else {
            left += SpriteWidth(self, kFillLeft);
            color = 0xffffffffu;
        }
    } else {
        const int32_t capW = SpriteWidth(self, kFillRight);
        if (barW + cw < (I32(self, kElLayoutRight) - left) - capW) {
            color = 0xffffffffu;
        } else {
            color = 0xff000000u;
            left  = (left - SpriteWidth(self, kFillRight)) - cw;
        }
        left += barW;
    }

    void* c0 = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
    reinterpret_cast<SetColorSlotFn>(VTable(c0)[kVtSetColor / 8])(c0, color);
    c0 = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(c0)[kVtComputeLayout / 8])(
        c0, left, I32(self, kElLayoutTop), left + cw, I32(self, kElLayoutBottom));
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@ProgressElement@UI@@UEBAXAEAVSpriteContext@2@E@Z  0x1801192c0
//
// THE ALPHA IS HALVED TWICE when the dim flag is set. Retail tests
// `flags & 0x40000` once before each of the two colour modulations and shifts
// the SAME register both times, so a dimmed progress bar's fill is at a
// quarter alpha, not a half. Transcribed, not corrected.
// ---------------------------------------------------------------------------
extern "C" void RtUi_ProgressElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    uint32_t raw = U32(me, kBackColor);
    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;
    const uint32_t trackColor = ModulateColor(raw, alpha);

    raw = U32(me, kElColor);
    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;
    const uint32_t fillColor = ModulateColor(raw, alpha);

    const int32_t left   = I32(me, kElLayoutLeft);
    const int32_t top    = I32(me, kElLayoutTop);
    const int32_t rightX = I32(me, kElLayoutRight);

    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, kTrackLeft), left, top, trackColor);

    const int32_t midH  = SpriteHeight(me, kTrackMiddle);
    const int32_t rightW = SpriteWidth(me, kTrackRight);
    const int32_t leftW  = SpriteWidth(me, kTrackLeft);
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackMiddle),
                                         left + leftW, top,
                                         rightX - rightW, top + midH, trackColor);

    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, kTrackRight),
                                         rightX - SpriteWidth(me, kTrackRight), top,
                                         trackColor);

    if (F32(me, kCurValue) > 0.0f) {
        int32_t barW = static_cast<int32_t>(
            (F32(me, kCurValue) / F32(me, kMaxValue))
            * static_cast<float>(rightX - left));
        if (barW < I32(me, kElLayoutWidth)) barW = I32(me, kElLayoutWidth);

        RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, kFillLeft), left, top, fillColor);

        const int32_t fMidH = SpriteHeight(me, kFillMiddle);
        const int32_t fCapW = SpriteWidth(me, kFillRight);
        const int32_t fLefW = SpriteWidth(me, kFillLeft);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kFillMiddle),
                                             left + fLefW, top,
                                             (left - fCapW) + barW, top + fMidH, fillColor);

        RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, kFillRight),
                                             (left - SpriteWidth(me, kFillRight)) + barW,
                                             top, fillColor);
    }

    if (I32(me, kElChildCount) > 0) {
        void* child = reinterpret_cast<void**>(PTR(me, kElChildren))[0];
        if (child != nullptr)
            reinterpret_cast<DrawSlotFn>(VTable(child)[kVtDraw / 8])(child, ctx, alpha);
    }
}

// ============================================================================
int RtUiProgressElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ProgressElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ProgressElement_Type);          ++n;
    RegisterRuntimeAddr(0x180077d20ull, (void*)&RtUi_ProgressElement_Type);

    RegisterRuntimeExport("?TypeName@ProgressElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ProgressElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180077d40ull, (void*)&RtUi_ProgressElement_TypeName);

    RegisterRuntimeExport("??0ProgressElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ProgressElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x180119c40ull, (void*)&RtUi_ProgressElement_Ctor);

    RegisterRuntimeExport("??1ProgressElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ProgressElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x180077ef0ull, (void*)&RtUi_ProgressElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@ProgressElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ProgressElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1801198f0ull, (void*)&RtUi_ProgressElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ProgressElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ProgressElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x180119610ull, (void*)&RtUi_ProgressElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ProgressElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ProgressElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1801192c0ull, (void*)&RtUi_ProgressElement_Draw);

    return n;
}
