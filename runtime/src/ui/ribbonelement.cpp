// ============================================================================
// runtime/src/ui/ribbonelement.cpp — UI::RibbonElement, the whole class
// ============================================================================
//
// `??_7RibbonElement@UI@@6B@` (retail 0x180156e70):
//
//   0  vector deleting dtor  0x180044ac0  -> deleting_dtor.cpp row (this batch)
//   1  TypeName              0x1800446e0  here
//   2  Type                  0x1800446c0  here
//   4  ComputeSize           0x1800d6190  here
//   5  ComputeLayout         0x1800d6410  here
//   6  Draw                  0x1800d6740  here
//
// plus ??0RibbonElement (0x1800d5d10) and ??1RibbonElement (0x1800d5d70).
//
// NOT HERE: `?AddItem@RibbonElement@UI@@` (0x1800448e0), `?IsVertical@`
// (0x180044970) and `?GetCellLayout@` (0x1800d5db0) — not vtable slots, and
// AddItem is on the UI non-slot callee list.
//
// ---------------------------------------------------------------------------
// LAYOUT. sizeof == 0xD8 (deleting dtor `MOV EDX,0xd8`). The ctor writes a
// TWO-BYTE store at +0xC0 and the three bodies read both halves separately:
//
//   +0xC0  _vertical  bool   lay the children out top-to-bottom
//   +0xC1  _uniform   bool   force every cell to the largest child's extent
//   +0xC4  _spacing   int32  gap between adjacent VISIBLE children
//   +0xC8  _extents   Collection::Array<int32>, one entry per child
//                            (data +0xC8, size +0xD0) — the ONE member
//                            ~RibbonElement destroys, via the shared `~Array`
//                            copy at 0x18001c750
//
// `_extents` is kept the same length as UI::Element::_children by ComputeSize
// itself (`if (_extents._size != _children._size) SetSize(...)`), which is why
// every loop below indexes it with the CHILD count.
//
// UI::ListElement's +0xF0 points at one of these and reads exactly +0xC0,
// +0xC4 and +0xC8 through it — corroboration of all three offsets from a
// second class.
// ============================================================================

#include "widget_common.h"

using namespace rtui;

namespace {

constexpr int kVertical = 0xC0;
constexpr int kUniform  = 0xC1;
constexpr int kSpacing  = 0xC4;
constexpr int kExtents  = 0xC8;
constexpr int kExtCount = 0xD0;

const uint8_t* TypeInfo() {
    return RT_DATA_SLOT(const uint8_t*,
                        "?_typeInfo@RibbonElement@UI@@0VBaseTypeInfo@Object@@A");
}
void* Vtable() { return RT_DATA_SLOT(void*, "??_7RibbonElement@UI@@6B@"); }

}  // namespace

extern "C" uint32_t RtUi_RibbonElement_Type(const void*) {
    const uint8_t* ti = TypeInfo();
    uint32_t v = 0; __builtin_memcpy(&v, ti, sizeof v); return v;
}
extern "C" const char* RtUi_RibbonElement_TypeName(const void*) {
    const uint8_t* ti = TypeInfo();
    const char* v = nullptr; __builtin_memcpy(&v, ti + 8, sizeof v); return v;
}

// ??0RibbonElement@UI@@QEAA@XZ   0x1800d5d10
extern "C" void* RtUi_RibbonElement_Ctor(void* self) {   // returns this in RAX, as every MSVC ctor does
    ElementCtor(self);
    PTR(self, 0x00) = Vtable();
    U8(self, kVertical) = 0;          // one 2-byte store covers both bools
    U8(self, kUniform)  = 0;
    I32(self, kSpacing) = 0;
    PTR(self, kExtents)     = nullptr;
    PTR(self, kExtents + 8) = nullptr;
    return self;
}

// ??1RibbonElement@UI@@UEAA@XZ   0x1800d5d70
extern "C" void RtUi_RibbonElement_Dtor(void* self) {
    PTR(self, 0x00) = Vtable();
    RtAddr<ArrayDtorFn>(0x18001c750ull)(B(self, kExtents));
    ElementDtor(self);
}

// ---------------------------------------------------------------------------
// slot 4 — ?ComputeSize@RibbonElement@UI@@UEAAXXZ   0x1800d6190
// ---------------------------------------------------------------------------
extern "C" void RtUi_RibbonElement_ComputeSize(void* self) {
    if ((U32(self, kElFlags) & kFlagNeedsSizeCalc) == 0) return;

    if (I32(self, kExtCount) != I32(self, kElChildCount))
        RtAddr<ArraySetSizeFn>(0x18002fea0ull)(B(self, kExtents), I32(self, kElChildCount));

    int32_t maxW = 0, maxH = 0, sumW = 0, sumH = 0, visible = 0;

    const int32_t n = I32(self, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        int32_t* ext = reinterpret_cast<int32_t*>(PTR(self, kExtents));
        ext[i] = 0;

        void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        if (child == nullptr || (U32(child, kElFlags) & kFlagHidden) != 0) continue;

        int32_t w = 0, h = 0;
        InlineGetPaddedSize(child, &w, &h);

        if (maxH < h) maxH = h;
        if (maxW < w) maxW = w;
        sumW += w;
        sumH += h;

        ext = reinterpret_cast<int32_t*>(PTR(self, kExtents));
        ext[i] = (U8(self, kVertical) != 0) ? h : w;

        if (w > 0 && h > 0) ++visible;
    }

    if (U8(self, kUniform) != 0) {
        sumH = I32(self, kElChildCount) * maxH;
        sumW = I32(self, kElChildCount) * maxW;
        const int32_t m = I32(self, kExtCount);
        for (int32_t i = 0; i < m; ++i)
            reinterpret_cast<int32_t*>(PTR(self, kExtents))[i] =
                (U8(self, kVertical) != 0) ? maxH : maxW;
    }

    int32_t gaps = visible - 1;
    if (gaps < 0) gaps = 0;
    const int32_t totalGap = I32(self, kSpacing) * gaps;

    if (U8(self, kVertical) == 0) maxW = totalGap + sumW;
    I32(self, kElLayoutWidth) = maxW;
    if (U8(self, kVertical) != 0) maxH = totalGap + sumH;
    U32(self, kElFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElLayoutHeight) = maxH;
}

// ---------------------------------------------------------------------------
// slot 5 — ?ComputeLayout@RibbonElement@UI@@UEAAXHHHH@Z   0x1800d6410
// ---------------------------------------------------------------------------
extern "C" void RtUi_RibbonElement_ComputeLayout(void* self, int32_t x, int32_t y,
                                                 int32_t right, int32_t bottom) {
    if (void* border = PTR(self, kElBorder))
        reinterpret_cast<ComputeLayoutSlotFn>(VTable(border)[kVtComputeLayout / 8])(
            border, x, y, right, bottom);

    reinterpret_cast<ComputeSizeSlotFn>(VTable(self)[kVtComputeSize / 8])(self);

    int32_t w = I32(self, kElLayoutWidth);
    int32_t h = I32(self, kElLayoutHeight);
    const int32_t align = I32(self, kElAlignment);

    if (right  < w + x) x = right  - w;
    if (bottom < h + y) y = bottom - h;

    int32_t px = x + I32(self, kElLeftPad);
    int32_t py = y + I32(self, kElTopPad);
    const int32_t aw = (right  - I32(self, kElRightPad))  - px;
    const int32_t ah = (bottom - I32(self, kElBottomPad)) - py;

    if ((U32(self, kElFlags) & kFlagStretchX) != 0) w = aw;
    if ((U32(self, kElFlags) & kFlagStretchY) != 0) h = ah;

    switch (align) {
        case 1: case 4: case 7: px = ((aw >> 1) - (w >> 1)) + px; break;
        case 2: case 5: case 8: px = (right - I32(self, kElRightPad)) - w; break;
        default: break;
    }
    if (align > 2) {
        if (align < 6)      py = ((ah >> 1) - (h >> 1)) + py;
        else if (align < 9) py = (bottom - I32(self, kElBottomPad)) - h;
    }

    I32(self, kElLayoutLeft)   = px;
    I32(self, kElLayoutRight)  = px + w;
    I32(self, kElLayoutTop)    = py;
    I32(self, kElLayoutBottom) = py + h;

    int32_t cursor = 0;
    const int32_t n = I32(self, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        const int32_t extent = reinterpret_cast<int32_t*>(PTR(self, kExtents))[i];
        void* child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        if (child == nullptr || (U32(child, kElFlags) & kFlagHidden) != 0) continue;

        int32_t cw = 0, ch = 0;
        InlineGetPaddedSize(child, &cw, &ch);

        child = reinterpret_cast<void**>(PTR(self, kElChildren))[i];
        const bool vertical = U8(self, kVertical) != 0;
        const bool last     = (i == I32(self, kElChildCount) - 1);

        if (!vertical) {
            int32_t end = I32(self, kElLayoutLeft) + extent + cursor;
            if ((U32(self, kElFlags) & kFlagStretchX) != 0 && last)
                end = I32(self, kElLayoutRight);
            reinterpret_cast<ComputeLayoutSlotFn>(VTable(child)[kVtComputeLayout / 8])(
                child, I32(self, kElLayoutLeft) + cursor, I32(self, kElLayoutTop),
                end, I32(self, kElLayoutBottom));
        } else {
            int32_t end = I32(self, kElLayoutTop) + extent + cursor;
            if ((U32(self, kElFlags) & kFlagStretchY) != 0 && last)
                end = I32(self, kElLayoutBottom);
            reinterpret_cast<ComputeLayoutSlotFn>(VTable(child)[kVtComputeLayout / 8])(
                child, I32(self, kElLayoutLeft), I32(self, kElLayoutTop) + cursor,
                I32(self, kElLayoutRight), end);
        }

        cursor += extent;
        if (cw > 0 && ch > 0) cursor += I32(self, kSpacing);
    }
}

// ---------------------------------------------------------------------------
// slot 6 — ?Draw@RibbonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d6740
// ---------------------------------------------------------------------------
extern "C" void RtUi_RibbonElement_Draw(const void* self, void* ctx, uint8_t alpha) {
    void* me = const_cast<void*>(self);
    if (void* border = PTR(me, kElBorder))
        reinterpret_cast<DrawSlotFn>(VTable(border)[kVtDraw / 8])(border, ctx, alpha);

    if ((U32(me, kElFlags) & kFlagDimColor) != 0) alpha >>= 1;

    const int32_t n = I32(me, kElChildCount);
    for (int32_t i = 0; i < n; ++i) {
        void* child = reinterpret_cast<void**>(PTR(me, kElChildren))[i];
        if (child != nullptr && (U32(child, kElFlags) & kFlagHidden) == 0)
            reinterpret_cast<DrawSlotFn>(VTable(child)[kVtDraw / 8])(child, ctx, alpha);
    }
}

// ============================================================================
int RtUiRibbonElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?Type@RibbonElement@UI@@UEBAIXZ",
                          (void*)&RtUi_RibbonElement_Type);          ++n;
    RegisterRuntimeAddr(0x1800446c0ull, (void*)&RtUi_RibbonElement_Type);

    RegisterRuntimeExport("?TypeName@RibbonElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_RibbonElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x1800446e0ull, (void*)&RtUi_RibbonElement_TypeName);

    RegisterRuntimeExport("??0RibbonElement@UI@@QEAA@XZ",
                          (void*)&RtUi_RibbonElement_Ctor);          ++n;
    RegisterRuntimeAddr(0x1800d5d10ull, (void*)&RtUi_RibbonElement_Ctor);

    RegisterRuntimeExport("??1RibbonElement@UI@@UEAA@XZ",
                          (void*)&RtUi_RibbonElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800d5d70ull, (void*)&RtUi_RibbonElement_Dtor);

    RegisterRuntimeExport("?ComputeSize@RibbonElement@UI@@UEAAXXZ",
                          (void*)&RtUi_RibbonElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800d6190ull, (void*)&RtUi_RibbonElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@RibbonElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_RibbonElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800d6410ull, (void*)&RtUi_RibbonElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@RibbonElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_RibbonElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800d6740ull, (void*)&RtUi_RibbonElement_Draw);

    return n;
}
