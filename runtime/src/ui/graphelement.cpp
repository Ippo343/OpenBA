// ============================================================================
// runtime/src/ui/graphelement.cpp — UI::GraphElement, the whole class
// ============================================================================
//
// 2026-09-12, vtable batch 3. `??_7GraphElement@UI@@6B@` (retail 0x1801569d0).
//
//   slot  method                retail       where
//   ----  --------------------  -----------  ---------------------------------
//    0    vector deleting dtor  0x1800408b0  deleting_dtor.cpp row
//    1    TypeName              0x180040460  here
//    2    Type                  0x180040440  here
//    4    ComputeSize           0x1800d1810  here
//    5    ComputeLayout         0x1800d16b0  here
//    6    Draw                  0x1800d1360  here
//
// plus ??0GraphElement@UI@@QEAA@XZ         0x1800d1880
//      ??0GraphElement@UI@@QEAA@AEBV01@@Z  0x1800406d0
//      ??1GraphElement@UI@@UEAA@XZ         0x1800d1840
//
// `?AddGraph@`, `?AddGraphPoint@` and `?RemoveGraphs@` are ALREADY PORTED in
// runtime/src/ui/widgets_layout.cpp (Waves 7/9); the series record layout is in
// runtime/types/ui/GraphElement.h. This file adds only the vtable rows.
//
// LAYOUT — 0x100 bytes (`MOV EDX,0x100` at 0x1800408e2). Every field below is
// a store in the constructor at 0x1800d1880 or a read in Draw:
//
//   +0xc0 int32   requested width     (ctor 100)
//   +0xc4 int32   requested height    (ctor 100)
//   +0xc8 int32   tick overhang, in pixels, outside the frame (ctor 4)
//   +0xcc int32   ctor -1; not read by any of the three slots here
//   +0xd0 Sprite  the 1x1 line sprite: sheet +0xd0, index +0xd8 (ctor -1)
//   +0xe0 float   the value that maps to full height (ctor 1.0)
//   +0xe4 int32   vertical grid divisions   (ctor 12)
//   +0xe8 int32   horizontal grid divisions (ctor 4)
//   +0xf0 Array<GraphSeries> — GraphElement.h
//
// `??0GraphElement@UI@@QEAA@XZ` (0x1800d1880) is NOT here: it is already
// ported and registered, table-driven, by runtime/src/ui/factory_ui1.cpp
// (the factory roster). One address, one owner.
// ============================================================================
#include "element_common.h"
#include "../../types/ui/GraphElement.h"

using namespace rtuielem;

namespace {

constexpr int kReqWidth  = 0xc0;
constexpr int kReqHeight = 0xc4;
constexpr int kTickSize  = 0xc8;
constexpr int kLineSprite = 0xd0;
constexpr int kMaxValue  = 0xe0;
constexpr int kDivsX     = 0xe4;
constexpr int kDivsY     = 0xe8;
constexpr int kSeries    = UI::GraphElementOffsets::kGraphsData;    // 0xf0
constexpr int kSeriesCnt = UI::GraphElementOffsets::kGraphsCount;   // 0xf8

static_assert(kSeries == 0xf0 && kSeriesCnt == 0xf8,
              "GraphElement.h must still pin the series Array at +0xf0");

RTELEM_TYPE_SLOTS(GraphElement)

// UI::SpriteContext::AddLine(ctx, const Sprite&, x0, y0, x1, y1, color)
using AddLineFn = void (*)(void* ctx, const void* sprite,
                           int x0, int y0, int x1, int y1, uint32_t color);

using ElementCtorFn     = void* (*)(void* self);
using ElementCopyCtorFn = void* (*)(void* self, const void* other);
using ElementDtorFn     = void  (*)(void* self);
using SeriesArrayFn     = void* (*)(void* array);                 // ~Array<GraphSeries>
using SeriesCopyFn      = void* (*)(void* dst, const void* src);

// 0x180161694 and 0x18016169c, read out of the DLL's float pool for this batch.
constexpr float kHalf = 0.5f;
constexpr float kOne  = 1.0f;

}  // namespace

RTELEM_TYPE_BODIES(GraphElement)

// ============================================================================
// ?ComputeSize@GraphElement@UI@@UEAAXXZ   0x1800d1810
// ============================================================================
// A graph is exactly the size it was told to be. Note the store ORDER: the flag
// clear sits BETWEEN the two size stores (0x1800d1826), not after them.
extern "C" void RtUi_GraphElement_ComputeSize(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    if (((U32(self, kElemFlags) >> 27) & 1u) == 0) return;
    I32(self, kElemLayoutWidth) = I32(self, kReqWidth);
    U32(self, kElemFlags) &= ~kFlagNeedsSizeCalc;
    I32(self, kElemLayoutHeight) = I32(self, kReqHeight);
}

// ============================================================================
// ?ComputeLayout@GraphElement@UI@@UEAAXHHHH@Z   0x1800d16b0
// ============================================================================
extern "C" void RtUi_GraphElement_ComputeLayout(void* self, int x, int y, int w, int h) {
    ElementComputeAlignedRect(self, x, y, w, h);
}

// ============================================================================
// ?Draw@GraphElement@UI@@UEBAXAEAVSpriteContext@2@E@Z   0x1800d1360
// ============================================================================
// Frame, grid, series — all of it `SpriteContext::AddLine` (0x18011cbe0) with
// the one line sprite at +0xd0.
//
// FOUR THINGS THE BYTES SAY THAT A REWRITE WOULD GET WRONG:
//
//  1. The grid loops run `divisions + 1` times (`while (i <= _divs)`), so a
//     graph with 4 horizontal divisions draws 5 lines — both edges included.
//     They are guarded by `if (_divs >= 0)`, so a NEGATIVE division count draws
//     nothing while ZERO still draws one line at the top edge.
//  2. The grid step is recomputed by ADDING `(far - near) / divs` to a running
//     float, not by multiplying the index. Float error accumulates, and it
//     accumulates the same way here.
//  3. Each grid coordinate is `(int)(running + 0.5f)` — a truncation of the
//     biased value, not a round-half-even.
//  4. The X grid lines are drawn BELOW the frame (from `bottom` to
//     `bottom + tick`) and the Y grid lines extend LEFT of it (from
//     `left - tick`), i.e. the ticks hang outside the box on two sides only.
//
// The series scale `(1.0f / _maxValue) * _layoutHeight` is computed ONCE,
// before the loop, and each point is `bottom - (int)(value * scale)`.
extern "C" void RtUi_GraphElement_Draw(const void* selfv, void* ctx, uint8_t alpha) {
    const uint8_t* self = static_cast<const uint8_t*>(selfv);

    void* border = Pc(self, kElemBorder);
    if (border != nullptr)
        VSlot<DrawFn>(border, kSlotDraw)(border, ctx, alpha);

    const uint32_t base = U32c(self, kElemColor);
    alpha = DimAlpha(self, alpha);
    const uint32_t c = ComputeColor(base, alpha);

    const void* sprite = self + kLineSprite;
    const auto line = RtAddr<AddLineFn>(0x18011cbe0ull);

    const int L = I32c(self, kElemLayoutLeft);
    const int T = I32c(self, kElemLayoutTop);
    const int R = I32c(self, kElemLayoutRight);
    const int B = I32c(self, kElemLayoutBottom);

    line(ctx, sprite, L, T, L, B, c);
    line(ctx, sprite, L, T, R, T, c);
    line(ctx, sprite, L, B, R, B, c);
    line(ctx, sprite, R, T, R, B, c);

    // ---- horizontal grid, `_divsY + 1` lines, ticks to the LEFT -----------
    {
        const int y0 = T;
        const int divs = I32c(self, kDivsY);
        const int y1 = B;
        float run = static_cast<float>(y0);
        if (divs >= 0) {
            for (int i = 0; i <= I32c(self, kDivsY); ++i) {
                const int yy = static_cast<int>(run + kHalf);
                line(ctx, sprite, L - I32c(self, kTickSize), yy, R, yy, c);
                run += static_cast<float>(y1 - y0) / static_cast<float>(divs);
            }
        }
    }

    // ---- vertical grid, `_divsX + 1` lines, ticks BELOW --------------------
    {
        const int x0 = L;
        const int divs = I32c(self, kDivsX);
        const int x1 = R;
        float run = static_cast<float>(x0);
        if (divs >= 0) {
            for (int i = 0; i <= I32c(self, kDivsX); ++i) {
                const int xx = static_cast<int>(run + kHalf);
                line(ctx, sprite, xx, B, xx, I32c(self, kTickSize) + B, c);
                run += static_cast<float>(x1 - x0) / static_cast<float>(divs);
            }
        }
    }

    // ---- the series --------------------------------------------------------
    const float scale = (kOne / *reinterpret_cast<const float*>(self + kMaxValue)) *
                        static_cast<float>(I32c(self, kElemLayoutHeight));

    const int32_t seriesCount = I32c(self, kSeriesCnt);
    for (int32_t s = 0; s < seriesCount; ++s) {
        const uint8_t* rec = static_cast<const uint8_t*>(Pc(self, kSeries)) +
                             static_cast<int64_t>(s) * UI::GraphElementOffsets::kSeriesStride;
        const int width = I32c(self, kElemLayoutWidth);
        const int segments = *reinterpret_cast<const int32_t*>(rec + 0x10) - 1;
        float xf = static_cast<float>(L);
        for (int64_t p = 0; p < segments; ++p) {
            const int32_t* pts = static_cast<const int32_t*>(
                *reinterpret_cast<void* const*>(rec + 0x08));
            const float xn = xf + static_cast<float>(width) / static_cast<float>(segments);
            line(ctx, sprite,
                 static_cast<int>(xf),
                 B - static_cast<int>(static_cast<float>(pts[p]) * scale),
                 static_cast<int>(xn),
                 B - static_cast<int>(static_cast<float>(pts[p + 1]) * scale),
                 *reinterpret_cast<const uint32_t*>(rec + 0x00));
            xf = xn;
        }
    }
}

// ============================================================================
// Constructors / destructor
// ============================================================================

extern "C" void* RtUi_GraphElement_CopyCtor(void* selfv, const void* otherv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    const uint8_t* other = static_cast<const uint8_t*>(otherv);
    RtAddr<ElementCopyCtorFn>(0x1800358c0ull)(self, other);
    P(self, 0x00) = VtGraphElement();
    I32(self, 0xc0) = I32c(other, 0xc0);
    I32(self, 0xc4) = I32c(other, 0xc4);
    I32(self, 0xc8) = I32c(other, 0xc8);
    I32(self, 0xcc) = I32c(other, 0xcc);
    *reinterpret_cast<uint64_t*>(self + 0xd0) = *reinterpret_cast<const uint64_t*>(other + 0xd0);
    *reinterpret_cast<uint64_t*>(self + 0xd8) = *reinterpret_cast<const uint64_t*>(other + 0xd8);
    I32(self, 0xe0) = I32c(other, 0xe0);
    I32(self, 0xe4) = I32c(other, 0xe4);
    I32(self, 0xe8) = I32c(other, 0xe8);
    *reinterpret_cast<uint64_t*>(self + kSeries)    = 0;
    *reinterpret_cast<uint64_t*>(self + kSeriesCnt) = 0;
    RtAddr<SeriesCopyFn>(0x1800415e0ull)(self + kSeries, other + kSeries);
    return self;
}

// ??1GraphElement@UI@@UEAA@XZ  0x1800d1840 — own vtable, destroy the series
// array (0x1800416d0, the same helper `?RemoveGraphs@` tail-calls), ~Element.
extern "C" void RtUi_GraphElement_Dtor(void* selfv) {
    uint8_t* self = static_cast<uint8_t*>(selfv);
    P(self, 0x00) = VtGraphElement();
    RtAddr<SeriesArrayFn>(0x1800416d0ull)(self + kSeries);
    RtAddr<ElementDtorFn>(0x1801161a0ull)(self);
}

// ============================================================================
// Registration
// ============================================================================
int RtUiGraphElementRegister() {
    int n = 0;
    RegisterRuntimeExport("?TypeName@GraphElement@UI@@UEBAPEBDXZ",
                          (void*)&RtUi_GraphElement_TypeName);      ++n;
    RegisterRuntimeAddr(0x180040460ull, (void*)&RtUi_GraphElement_TypeName);

    RegisterRuntimeExport("?Type@GraphElement@UI@@UEBAIXZ",
                          (void*)&RtUi_GraphElement_Type);          ++n;
    RegisterRuntimeAddr(0x180040440ull, (void*)&RtUi_GraphElement_Type);

    RegisterRuntimeExport("?ComputeSize@GraphElement@UI@@UEAAXXZ",
                          (void*)&RtUi_GraphElement_ComputeSize);   ++n;
    RegisterRuntimeAddr(0x1800d1810ull, (void*)&RtUi_GraphElement_ComputeSize);

    RegisterRuntimeExport("?ComputeLayout@GraphElement@UI@@UEAAXHHHH@Z",
                          (void*)&RtUi_GraphElement_ComputeLayout); ++n;
    RegisterRuntimeAddr(0x1800d16b0ull, (void*)&RtUi_GraphElement_ComputeLayout);

    RegisterRuntimeExport("?Draw@GraphElement@UI@@UEBAXAEAVSpriteContext@2@E@Z",
                          (void*)&RtUi_GraphElement_Draw);          ++n;
    RegisterRuntimeAddr(0x1800d1360ull, (void*)&RtUi_GraphElement_Draw);

    RegisterRuntimeExport("??0GraphElement@UI@@QEAA@AEBV01@@Z",
                          (void*)&RtUi_GraphElement_CopyCtor);      ++n;
    RegisterRuntimeAddr(0x1800406d0ull, (void*)&RtUi_GraphElement_CopyCtor);

    RegisterRuntimeExport("??1GraphElement@UI@@UEAA@XZ",
                          (void*)&RtUi_GraphElement_Dtor);          ++n;
    RegisterRuntimeAddr(0x1800d1840ull, (void*)&RtUi_GraphElement_Dtor);
    return n;
}
