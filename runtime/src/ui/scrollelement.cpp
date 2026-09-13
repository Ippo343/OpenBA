// ============================================================================
// runtime/src/ui/scrollelement.cpp — UI::ScrollElement, the whole class
// ============================================================================
//
// `??_7ScrollElement@UI@@6B@` (retail 0x180156400):
//
//   0  vector deleting dtor  0x18003adc0  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x18003a850  here
//   2  Type                  0x18003a830  here
//   4  ComputeSize           0x18011ad80  here
//   5  ComputeLayout         0x18011af20  here
//   6  Draw                  0x18011b0b0  here
//   7  Update                0x18011b620  here
//
// plus ??0ScrollElement (0x18011a7f0) and ??1ScrollElement (0x18011a900).
//
// NOT HERE: `?GetBarLayout@ScrollElement@UI@@AEBAXAEAH000@Z` (0x18011a2d0) —
// on the UI non-slot callee list, reached by address by both Draw and Update.
// Nor the three one-line setters 0x18003a9d0/0x18003a9f0/0x18003aa10.
//
// ---------------------------------------------------------------------------
// LAYOUT. sizeof == 0x178 (deleting dtor `MOV EDX,0x178`). NINE UI::Sprites in
// three 3-slice sets, then six scalars — every one written by the ctor at
// 0x18011a7f0:
//
//   +0xC0 / +0xD0 / +0xE0    _track{Start,Middle,End}   the groove
//   +0xF0 / +0x100 / +0x110  _thumb{Start,Middle,End}   the thumb, idle
//   +0x120 / +0x130 / +0x140 _hot{Start,Middle,End}     the thumb, live
//   +0x150  _vertical        bool,   ctor 1
//   +0x154  _barColor        uint32, ctor 0xFFFFFFFF   } one qword store
//   +0x158  (unnamed)        uint32, ctor 0xFFFFFFFF   } of 0xFFFFFFFFFFFFFFFF
//   +0x160  _current         float,  ctor 0
//   +0x164  _displayPercent  float,  ctor 0.33333334f  ?SetDisplayPercent@
//   +0x168  _maxValue        float,  ctor 4.0f         ?SetMaxValue@
//   +0x16C  _dragAnchor      float,  ctor 0 (the qword store at +0x168)
//   +0x170  _dragValue       float,  ctor 0
//
// ~ScrollElement destroys NOTHING — it restamps the vtable and tail-calls
// ~Element. Nine Sprites are nine {ExternalPtr, index} pairs and own no heap.
//
// The float 1.0f that Update divides by is the rodata constant at
// 0x18016169c, read out of the shipped image (`00 00 80 3f`) rather than
// assumed from the shape of the expression.
// ============================================================================

#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kTrackStart  = 0xC0;
constexpr int kTrackMiddle = 0xD0;
constexpr int kTrackEnd    = 0xE0;
constexpr int kThumbStart  = 0xF0;
constexpr int kThumbMiddle = 0x100;
constexpr int kThumbEnd    = 0x110;
constexpr int kHotStart    = 0x120;
constexpr int kHotMiddle   = 0x130;
constexpr int kHotEnd      = 0x140;
constexpr int kVertical    = 0x150;
constexpr int kBarColor    = 0x154;
constexpr int kCurrent     = 0x160;
constexpr int kPercent     = 0x164;
constexpr int kMaxValue    = 0x168;
constexpr int kDragAnchor  = 0x16C;
constexpr int kDragValue   = 0x170;

// UpdateState
constexpr int kUsPressed  = 0x01;
constexpr int kUsReleased = 0x04;
constexpr int kUsDeltaX   = 0x0c;
constexpr int kUsDeltaY   = 0x10;
constexpr int kUsMouseX   = 0x18;
constexpr int kUsMouseY   = 0x1c;

// 0x18016169c — `00 00 80 3f`, read from the shipped .rdata.
constexpr float kOne = 1.0f;

// A 0x10-byte UI::Sprite copied to the stack, exactly as Draw does: two qword
// loads from the chosen 0x10-byte field.
struct SpriteCopy { void* sheet; int64_t index; };
static_assert(sizeof(SpriteCopy) == 0x10, "");

SpriteCopy LoadSprite(const void* self, int off) {
    SpriteCopy s;
    s.sheet = PTRc(self, off);
    s.index = *reinterpret_cast<const int64_t*>(B(self, off + 8));
    return s;
}

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@ScrollElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7ScrollElement@UI@@6B@"); }

using GetBarLayoutFn = void (*)(const void* self, int32_t* x0, int32_t* y0,
                                int32_t* x1, int32_t* y1);

}  // namespace

extern "C" uint32_t RtUi_ScrollElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_ScrollElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0ScrollElement@UI@@QEAA@XZ   0x18011a7f0
extern "C" void* RtUi_ScrollElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    const int sprites[9] = { kTrackStart, kTrackMiddle, kTrackEnd,
                             kThumbStart, kThumbMiddle, kThumbEnd,
                             kHotStart,   kHotMiddle,   kHotEnd };
    for (int off : sprites) {
        PTR(self, off + kSpriteSheet) = nullptr;
        I32(self, off + kSpriteIndex) = -1;
    }
    U8(self, kVertical)    = 1;
    U32(self, kBarColor)   = 0xffffffffu;
    U32(self, kBarColor + 4) = 0xffffffffu;   // the other half of the qword
    F32(self, kCurrent)    = 0.0f;
    F32(self, kPercent)    = 0.33333334f;     // 0x3eaaaaab
    F32(self, kMaxValue)   = 4.0f;            // 0x40800000
    F32(self, kDragAnchor) = 0.0f;            // the high half of that qword
    F32(self, kDragValue)  = 0.0f;
    return self;
}

// ??1ScrollElement@UI@@UEAA@XZ   0x18011a900
extern "C" void RtUi_ScrollElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@ScrollElement@UI@@UEAAXXZ   0x18011ad80
// ---------------------------------------------------------------------------
extern "C" void RtUi_ScrollElement_ComputeSize(void* self) {
    const uint32_t flags = U32(self, kElFlags);
    if ((flags & kFlagNeedsSizeCalc) == 0) return;

    if (U8(self, kVertical) != 0) {
        I32(self, kElLayoutWidth) = SpriteWidth(self, kTrackStart);
        const int32_t h = SpriteHeight(self, kTrackStart)
                        + SpriteHeight(self, kTrackMiddle)
                        + SpriteHeight(self, kTrackEnd);
        U32(self, kElFlags) = flags & ~kFlagNeedsSizeCalc;
        I32(self, kElLayoutHeight) = h;
        return;
    }

    I32(self, kElLayoutWidth) = SpriteWidth(self, kTrackStart)
                              + SpriteWidth(self, kTrackMiddle)
                              + SpriteWidth(self, kTrackEnd);
    I32(self, kElLayoutHeight) = SpriteHeight(self, kTrackStart);
    U32(self, kElFlags) = flags & ~kFlagNeedsSizeCalc;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@ScrollElement@UI@@UEAAXHHHH@Z   0x18011af20
//
// `_vertical` is read BEFORE ComputeSize runs (0x18011af3c), which matters: a
// ComputeSize that flipped it would not be seen here. Reproduced.
// ---------------------------------------------------------------------------
extern "C" void RtUi_ScrollElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                 int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    const uint8_t vertical = U8(self, kVertical);
    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    int32_t w = I32(self, kElLayoutWidth);
    int32_t h = I32(self, kElLayoutHeight);

    const bool stretchY = !(vertical == 0 && (U32(self, kElFlags) & kFlagStretchY) == 0);
    const bool stretchX =  (vertical == 0 || (U32(self, kElFlags) & kFlagStretchX) != 0);

    const int32_t align = I32(self, kElAlignment);
    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;

    if (stretchX) w = aw;
    if (stretchY) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - I32(self, kElRightPad)) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutRight)  = px + w;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutBottom) = py + h;
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@ScrollElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x18011b0b0
//
// The alpha is halved TWICE when the dim flag is set: once before the track
// colour and again before the bar colour, on the same register. Retail's.
// ---------------------------------------------------------------------------
extern "C" void RtUi_ScrollElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    uint32_t raw = U32(me, kElColor);
    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;
    const uint32_t trackColor = ModulateColor(raw, alpha);

    const int32_t left   = I32(me, kElLayoutLeft);
    const int32_t top    = I32(me, kElLayoutTop);
    const int32_t rightX = I32(me, kElLayoutRight);
    const int32_t botY   = I32(me, kElLayoutBottom);

    int32_t ex0, ey0, ex1, ey1;    // the END slice's rectangle, common tail
    if (U8(me, kVertical) == 0) {
        const int32_t w0 = SpriteWidth(me, kTrackStart);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackStart),
                                             left, top, left + w0, botY, trackColor);
        const int32_t w2 = SpriteWidth(me, kTrackEnd);
        const int32_t w0b = SpriteWidth(me, kTrackStart);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackMiddle),
                                             left + w0b, top, rightX - w2, botY, trackColor);
        ex0 = rightX - SpriteWidth(me, kTrackEnd);
        ey0 = top;
        ex1 = rightX;
        ey1 = botY;
    } else {
        const int32_t h0 = SpriteHeight(me, kTrackStart);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackStart),
                                             left, top, rightX, top + h0, trackColor);
        const int32_t h2 = SpriteHeight(me, kTrackEnd);
        const int32_t h0b = SpriteHeight(me, kTrackStart);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackMiddle),
                                             left, top + h0b, rightX, botY - h2, trackColor);
        ex0 = left;
        ey0 = botY - SpriteHeight(me, kTrackEnd);
        ex1 = rightX;
        ey1 = botY;
    }
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, B(me, kTrackEnd),
                                         ex0, ey0, ex1, ey1, trackColor);

    raw = U32(me, kBarColor);
    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;
    const uint32_t barColor = ModulateColor(raw, alpha);

    int32_t bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
    RtAddr<GetBarLayoutFn>(0x18011a2d0ull)(me, &bx0, &by0, &bx1, &by1);

    // The thumb uses the HOT set while the element is interactive and the
    // scroll actually has range.
    const bool hot = (U32(me, kElFlags) & kFlagInteractive) != 0
                     && F32(me, kMaxValue) > 0.0f;
    const SpriteCopy s0 = LoadSprite(me, hot ? kHotStart  : kThumbStart);
    const SpriteCopy s1 = LoadSprite(me, hot ? kHotMiddle : kThumbMiddle);
    const SpriteCopy s2 = LoadSprite(me, hot ? kHotEnd    : kThumbEnd);

    if (U8(me, kVertical) == 0) {
        const int32_t w0 = SpriteWidth(&s0, 0);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &s0, bx0, by0, bx0 + w0, by1, barColor);
        const int32_t w2  = SpriteWidth(&s2, 0);
        const int32_t w0b = SpriteWidth(&s0, 0);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &s1, bx0 + w0b, by0,
                                             bx1 - w2, by1, barColor);
        bx0 = bx1 - SpriteWidth(&s2, 0);
    } else {
        const int32_t h0 = SpriteHeight(&s0, 0);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &s0, bx0, by0, bx1, by0 + h0, barColor);
        const int32_t h2  = SpriteHeight(&s2, 0);
        const int32_t h0b = SpriteHeight(&s0, 0);
        RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &s1, bx0, by0 + h0b,
                                             bx1, by1 - h2, barColor);
        by0 = by1 - SpriteHeight(&s2, 0);
    }
    RtAddr<AddSprite5Fn>(0x18011d3c0ull)(ctx, &s2, bx0, by0, bx1, by1, barColor);
}

// ---------------------------------------------------------------------------
// slot 7 — ?Update@ScrollElement@UI@@ ...   0x18011b620
// ---------------------------------------------------------------------------
extern "C" void RtUi_ScrollElement_Update(void* self, const void* time, void* ui,
                                          void** dlgPtr, const void* state) {
    void* slot = *dlgPtr;
    ElementUpdate(self, time, ui, &slot, state);

    // Nothing to scroll and the default display fraction: park at zero.
    if (F32(self, kMaxValue) == 0.0f && F32(self, kPercent) == kOne) {
        F32(self, kCurrent) = 0.0f;
        return;
    }

    if (U8c(state, kUsPressed) != 0 && (U32(self, kElFlags) & kFlagInteractive) != 0) {
        int32_t bx0 = 0, by0 = 0, bx1 = 0, by1 = 0;
        RtAddr<GetBarLayoutFn>(0x18011a2d0ull)(self, &bx0, &by0, &bx1, &by1);

        const int32_t mx = I32c(state, kUsMouseX);
        const int32_t my = I32c(state, kUsMouseY);
        const bool vertical = U8(self, kVertical) != 0;
        bool played = false;

        if (mx < bx0 || bx1 < mx || my < by0 || by1 < my) {
            // Outside the thumb: page toward the click by one visible page.
            const float page = F32(self, kMaxValue)
                             / (kOne / F32(self, kPercent) - kOne);
            if ((vertical && by1 < my) || bx1 < mx) {
                F32(self, kCurrent) = F32(self, kCurrent) + page;
                played = true;
            } else if ((vertical && my < by0) || mx < bx0) {
                F32(self, kCurrent) = F32(self, kCurrent) - page;
                played = true;
            }
        } else {
            F32(self, kDragAnchor) = F32(self, kCurrent);
            SetFlags(self, U32(self, kElFlags) | kFlagDragging);
            played = true;
        }

        if (played) RtAddr<PlayFn>(0x1801223d0ull)(ui, 4);

        if (F32(self, kCurrent) < 0.0f) F32(self, kCurrent) = 0.0f;
        if (F32(self, kMaxValue) < F32(self, kCurrent))
            F32(self, kCurrent) = F32(self, kMaxValue);
    }

    const int32_t mx = I32c(state, kUsMouseX);
    const int32_t my = I32c(state, kUsMouseY);
    const bool isNear = !(mx < I32(self, kElLayoutLeft)   - 0x20 ||
                        I32(self, kElLayoutRight) + 0x20 < mx  ||
                        my < I32(self, kElLayoutTop)    - 0x20 ||
                        I32(self, kElLayoutBottom) + 0x20 < my);

    const uint32_t flags = U32(self, kElFlags);
    if ((flags & kFlagDragging) == 0 || isNear) {
        if ((flags & kFlagDragging) != 0 && U8c(state, kUsReleased) != 0) {
            SetFlags(self, flags & ~kFlagDragging);
            if (!isNear) return;
            RtAddr<PlayFn>(0x1801223d0ull)(ui, 5);
        }
        if (isNear && (U32(self, kElFlags) & kFlagDragging) != 0) {
            F32(self, kCurrent) = F32(self, kDragValue);
            const bool vertical = U8(self, kVertical) != 0;
            const int32_t span = vertical
                ? (I32(self, kElLayoutBottom) - I32(self, kElLayoutTop))
                : (I32(self, kElLayoutRight)  - I32(self, kElLayoutLeft));
            const float maxV = F32(self, kMaxValue);
            const int32_t delta = vertical ? I32c(state, kUsDeltaY) : I32c(state, kUsDeltaX);
            const float v = F32(self, kDragAnchor)
                - (static_cast<float>(delta)
                   / (static_cast<float>(span)
                      - static_cast<float>(span) * F32(self, kPercent))) * maxV;
            F32(self, kCurrent) = v;
            if (v < 0.0f) F32(self, kCurrent) = 0.0f;
            if (maxV < F32(self, kCurrent)) F32(self, kCurrent) = maxV;
            F32(self, kDragValue) = F32(self, kCurrent);
        }
    } else {
        F32(self, kCurrent) = F32(self, kDragAnchor);
    }
}

// ============================================================================
int RtUiScrollElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@ScrollElement@UI@@UEBAIXZ",
                          (void*)&RtUi_ScrollElement_Type);          ++n;
    RegisterRuntimeAddr(0x18003a830ull, (void*)&RtUi_ScrollElement_Type);

    RegisterRuntimeExport("?TypeName@ScrollElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_ScrollElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x18003a850ull, (void*)&RtUi_ScrollElement_TypeName);

    RegisterRuntimeExport("??0ScrollElement@UI@@QEAA@XZ",
                          (void*)&RtUi_ScrollElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x18011a7f0ull, (void*)&RtUi_ScrollElement_Ctor);

    RegisterRuntimeExport("??1ScrollElement@UI@@UEAA@XZ",
                          (void*)&RtUi_ScrollElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x18011a900ull, (void*)&RtUi_ScrollElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@ScrollElement@UI@@UEAAXXZ",
                          (void*)&RtUi_ScrollElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x18011ad80ull, (void*)&RtUi_ScrollElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@ScrollElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_ScrollElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x18011af20ull, (void*)&RtUi_ScrollElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@ScrollElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_ScrollElement_Draw);          ++n;
    RegisterRuntimeAddr(0x18011b0b0ull, (void*)&RtUi_ScrollElement_Draw);

    RegisterRuntimeExport(
        "?Update@ScrollElement@UI@@UEAAXAEBVTime@System@@PEAVUserInterface@2@"
        "V?$InternalPtr@VDialogInstance@UI@@@Object@@AEBUUpdateState@Element@2@@Z",
        (void*)&RtUi_ScrollElement_Update);                          ++n;
    RegisterRuntimeAddr(0x18011b620ull, (void*)&RtUi_ScrollElement_Update);

    return n;
}
