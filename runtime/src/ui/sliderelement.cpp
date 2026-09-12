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
// ---------------------------------------------------------------------------
// LAYOUT. sizeof == 0x130 (deleting dtor `MOV EDX,0x130`), every field from
// the ctor at 0x1800d7570:
//
//   +0xC0 / +0xD0 / +0xE0   _track{Start,Middle,End}   the groove, 3-slice
//   +0xF0                   _thumb        UI::Sprite
//   +0x100                  _thumbHot     UI::Sprite (used while _hovered)
//   +0x110  _trackOffsetY   int32, ctor 0   } one qword store of 0
//   +0x114  _thumbMargin    int32, ctor 0   }
//   +0x118  _minimum        float, ctor 0.0f   ?SetMinimumValue@ 0x180049ea0
//   +0x11C  _maximum        float, ctor 1.0f   ?SetMaximumValue@ 0x180049eb0
//   +0x120  _steps          int32, ctor 10   (a page click moves range/steps)
//   +0x124  _hovered        bool,  ctor 0
//   +0x128  _value          float, ctor 0.25f  ?SetCurrentValue@ 0x180049ec0
//   +0x12C  _dragAnchor     float
//
// runtime/types/ui/SliderElement.h said "exactly an Element with no extra
// state, as far as this lane's evidence goes". That was an honest statement of
// a 2026-09-07 lane's evidence and it is now superseded; the header says so.
//
// ~SliderElement destroys nothing and does NOT restamp the vtable — five
// instructions at 0x180049ee0, a bare tail jump to ~Element.
// ============================================================================

#include "widget_common.h"

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
constexpr int kDragAnchor  = 0x12C;

constexpr int kUsPressed  = 0x01;
constexpr int kUsReleased = 0x04;
constexpr int kUsDeltaX   = 0x0c;
constexpr int kUsMouseX   = 0x18;
constexpr int kUsMouseY   = 0x1c;

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

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@SliderElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7SliderElement@UI@@6B@"); }

using UpdateNumberFn = void (*)(void* self);

// The thumb travel, computed identically by ComputeLayout, Draw and Update:
//   span = (_layoutRight - 2*(thumbW/2 + _thumbMargin)) - _layoutLeft
//   pos  = span * ((_value - _minimum) / (_maximum - _minimum))      (0 if flat)
int32_t ThumbOffset(void* self, int32_t thumbW) {
    const float range = F32(self, kMaximum) - F32(self, kMinimum);
    if (range == 0.0f) return 0;
    const int32_t span = (I32(self, kElLayoutRight)
                          + ((thumbW >> 1) + I32(self, kThumbMargin)) * -2)
                         - I32(self, kElLayoutLeft);
    return static_cast<int32_t>(static_cast<float>(span)
                                * ((F32(self, kValue) - F32(self, kMinimum)) / range));
}

void QueueEvent(void* self, void* ui, void** dlgPtr, const void* state) {
    WidgetEvent ev;
    ev.dialog    = *dlgPtr;
    ev.element   = self;
    ev.kind      = 0x17;
    ev.elementId = I32(self, kElElementId);
    ev.mouseX    = I32c(state, kUsMouseX);
    ev.mouseY    = I32c(state, kUsMouseY);
    ev.value     = 0;
    ev.tail      = 0;
    RtAddr<ArraySetSizeFillFn>(0x180033690ull)(
        B(ui, kUiEvents), I32(ui, kUiEventCount) + 1, &ev);
}

}  // namespace

extern "C" uint32_t RtUi_SliderElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_SliderElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0SliderElement@UI@@QEAA@XZ   0x1800d7570
extern "C" void RtUi_SliderElement_Ctor(void* self) {
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
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
}

// ??1SliderElement@UI@@UEAA@XZ   0x180049ee0 — tail jump, no restamp.
extern "C" void RtUi_SliderElement_Dtor(void* self) {
    ElementDtor(self);
}

// slot 4 — 0x1800d7380
extern "C" void RtUi_SliderElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    const int32_t trackW = SpriteWidth(self, kTrackStart)
                         + SpriteWidth(self, kTrackMiddle)
                         + SpriteWidth(self, kTrackEnd);
    int32_t width = SpriteWidth(self, kThumb);

    int32_t height = 0;
    if (I32(self, kElChildCount) > 0) {
        void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
        if (child != nullptr) {
            int32_t cw = 0;
            InlineGetPaddedSize(child, &cw, &height);
        }
    }

    if (width < trackW) width = trackW;
    I32(self, kElLayoutWidth) = width;

    int32_t spriteH = SpriteHeight(self, kTrackStart);
    const int32_t thumbH = SpriteHeight(self, kThumb);
    if (spriteH < thumbH) spriteH = thumbH;
    if (height  < spriteH) height = spriteH;

    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElLayoutHeight) = height;
}

// slot 5 — 0x1800d6fc0
extern "C" void RtUi_SliderElement_ComputeLayout(void* self, int32_t x, int32_t y,
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

    if ((U32(self, kElFlags) & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 2: case 5: case 8:
            px = (right - I32(self, kElRightPad)) - aw;
            break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutRight)  = aw + px;
    I32(self, kElLayoutBottom) = py + h;

    void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
    if (child == nullptr) return;

    int32_t cw = 0, ch = 0;
    InlineGetPaddedSize(child, &cw, &ch);

    int32_t pos = I32(self, kElLayoutLeft) + I32(self, kThumbMargin)
                + ThumbOffset(self, SpriteWidth(self, kThumb));

    int32_t q = SpriteWidth(self, kThumb) + pos;
    if (q + cw < I32(self, kElLayoutRight)) q = I32(self, kThumbMargin) + q;
    else                                    q = pos - cw;

    child = reinterpret_cast<void**>(PTR(self, kElChildren))[0];
    reinterpret_cast<ComputeLayoutSlotFn>(VTable(child)[kVtComputeLayout / 8])(
        child, q, I32(self, kTrackOffY) + I32(self, kElLayoutTop),
        q + cw, I32(self, kElLayoutBottom));
}

// slot 6 — 0x1800d6d40
extern "C" void RtUi_SliderElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    const uint32_t raw = U32(me, kElColor);
    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;
    const uint32_t c = ModulateColor(raw, alpha);

    const int32_t left   = I32(me, kElLayoutLeft);
    const int32_t rightX = I32(me, kElLayoutRight);
    const int32_t top    = I32(me, kElLayoutTop);
    const int32_t ty     = top + I32(me, kTrackOffY);

    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, kTrackStart), left, ty, c);

    const int32_t midH = SpriteHeight(me, kTrackMiddle);
    const int32_t endW = SpriteWidth(me, kTrackEnd);
    const int32_t sttW = SpriteWidth(me, kTrackStart);
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackMiddle),
                                         left + sttW, ty,
                                         rightX - endW, ty + midH, c);

    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, kTrackEnd),
                                         rightX - SpriteWidth(me, kTrackEnd), ty, c);

    // The thumb's TRAVEL is measured with the idle sprite's width even when
    // the hot sprite is the one drawn (0x1800d6f3d reads +0xF0 before the
    // hover swap at 0x1800d6f5f). Reproduced.
    const int32_t pos = ThumbOffset(me, SpriteWidth(me, kThumb));
    const int thumbOff = (U8(me, kHovered) != 0) ? kThumbHot : kThumb;
    RtAddr<AddSprite3Fn>(0x18011cf00ull)(ctx, B(me, thumbOff),
                                         I32(me, kThumbMargin) + pos + left, top, c);

    void* child = reinterpret_cast<void**>(PTR(me, kElChildren))[0];
    if (child != nullptr)
        reinterpret_cast<DrawSlotFn>(VTable(child)[kVtDraw / 8])(child, ctx, alpha);
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@SliderElement@UI@@ ...   0x1800d67e0
// ---------------------------------------------------------------------------
extern "C" void RtUi_SliderElement_Update(void* self, const void* time, void* ui,
                                          void** dlgPtr, const void* state) {
    void* slot = *dlgPtr;
    ElementUpdate(self, time, ui, &slot, state);

    const int32_t thumbW = SpriteWidth(self, kThumb);
    const int32_t thumbH = SpriteHeight(self, kThumb);
    const float   range  = F32(self, kMaximum) - F32(self, kMinimum);

    const int32_t pos = I32(self, kElLayoutLeft) + I32(self, kThumbMargin)
                      + ThumbOffset(self, thumbW);

    const int32_t mx = I32c(state, kUsMouseX);
    const int32_t my = I32c(state, kUsMouseY);
    const bool live  = U8c(state, kUsPressed) != 0
                       && (U32(self, kElFlags) & kFlagInteractive) != 0;

    if (mx < pos || pos + thumbW < mx ||
        my < I32(self, kElLayoutTop) || I32(self, kElLayoutTop) + thumbH < my) {
        U8(self, kHovered) = 0;
        if (live) {
            bool moved = true;
            if (pos + thumbW < mx) {
                F32(self, kValue) = range / static_cast<float>(I32(self, kSteps))
                                    + F32(self, kValue);
            } else if (mx < pos) {
                F32(self, kValue) = F32(self, kValue)
                                    - range / static_cast<float>(I32(self, kSteps));
            } else {
                moved = false;   // inside the thumb band horizontally: no page
            }
            if (moved) {
                QueueEvent(self, ui, dlgPtr, state);
                RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);
            }
        }
    } else {
        U8(self, kHovered) = 1;
        if (live) {
            F32(self, kDragAnchor) = F32(self, kValue);
            SetFlags(self, U32(self, kElFlags) | kFlagDragging);
            RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);
        }
    }

    const bool isNear = !(mx < I32(self, kElLayoutLeft)   - 0x20 ||
                          I32(self, kElLayoutRight) + 0x20 < mx  ||
                          my < I32(self, kElLayoutTop)    - 0x20 ||
                          I32(self, kElLayoutBottom) + 0x20 < my);

    const uint32_t flags = U32(self, kElFlags);
    if ((flags & kFlagDragging) != 0 && !isNear) {
        F32(self, kValue) = F32(self, kDragAnchor);
        RtAddr<UpdateNumberFn>(0x1800d6c20ull)(self);
        return;
    }

    bool doDrag = true;
    if ((flags & kFlagDragging) != 0 && U8c(state, kUsReleased) != 0) {
        SetFlags(self, flags & ~kFlagDragging);
        if (isNear) RtAddr<PlayFn>(0x1801223d0ull)(ui, 5);
        else        doDrag = false;
    }

    if (doDrag && isNear && (U32(self, kElFlags) & kFlagDragging) != 0) {
        const int32_t w = SpriteWidth(self, kThumb);
        const int32_t span = (I32(self, kElLayoutRight)
                              + ((w >> 1) + I32(self, kThumbMargin)) * -2)
                             - I32(self, kElLayoutLeft);
        const float v = F32(self, kDragAnchor)
            - (static_cast<float>(I32c(state, kUsDeltaX)) / static_cast<float>(span))
              * (F32(self, kMaximum) - F32(self, kMinimum));
        if (v != F32(self, kValue)) {
            F32(self, kValue) = v;
            QueueEvent(self, ui, dlgPtr, state);
        }
    }

    if (F32(self, kValue) < F32(self, kMinimum)) F32(self, kValue) = F32(self, kMinimum);
    if (F32(self, kMaximum) < F32(self, kValue)) F32(self, kValue) = F32(self, kMaximum);

    RtAddr<UpdateNumberFn>(0x1800d6c20ull)(self);
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
