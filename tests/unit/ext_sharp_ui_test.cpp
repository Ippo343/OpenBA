// ============================================================================
// tests/unit/ext_sharp_ui_test.cpp -- ext.sharp_ui's pure parts
// ============================================================================
// src/ext/sharp_ui/sharp_math.{h,cpp} (the effective-scale rule, the fit floor,
// the integer-ratio / point decision) and src/engine/ui/SharpDraw.h (the
// device-pixel snap), without a device.
// ============================================================================
#include "unit.h"
#include "ext/sharp_ui/sharp_math.h"
#include "ui/SharpDraw.h"

#include <cmath>
#include <initializer_list>

namespace {

bool Near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

}  // namespace

UNIT_TEST(ext_sharp_ui_wanted_ratio) {
    using SharpUI::WantedRatio;
    CHECK(WantedRatio(75) == 2);    // the slider's bottom: a bigger UI -> 2x
    CHECK(WantedRatio(99) == 2);
    CHECK(WantedRatio(100) == 1);
    CHECK(WantedRatio(125) == 1);   // a smaller UI: never below 1x
    CHECK(WantedRatio(150) == 1);
    CHECK(WantedRatio(50) == 2);    // outside the slider: exact
    CHECK(WantedRatio(34) == 3);
    CHECK(WantedRatio(0) == 1);     // a broken settings value
    CHECK(WantedRatio(-5) == 1);
}

UNIT_TEST(ext_sharp_ui_fit_ratio) {
    using SharpUI::FitRatio;
    CHECK(FitRatio(1920, 1080) == 1);   // 960x540 is under the 800x600 floor
    CHECK(FitRatio(2560, 1440) == 2);   // 1280x720
    CHECK(FitRatio(3840, 2160) == 3);   // 1280x720 at 3x
    CHECK(FitRatio(2560, 1080) == 1);   // ultrawide: the height decides
    CHECK(FitRatio(1600, 1200) == 2);   // exactly the floor at 2x
    CHECK(FitRatio(1280, 720) == 1);
    CHECK(FitRatio(800, 600) == 1);
    CHECK(FitRatio(640, 480) == 1);     // below the floor: still 1x, never smaller
}

UNIT_TEST(ext_sharp_ui_choose_scale) {
    using SharpUI::ChooseScale;
    SharpUI::ScaleChoice c = ChooseScale(75, 2560, 1440);
    CHECK(c.wanted == 2 && c.fit == 2 && c.ratio == 2 && !c.clamped);
    CHECK(Near(c.uiScale, 0.5f));

    c = ChooseScale(75, 1920, 1080);   // wanted 2x, does not fit: clamped to 1x
    CHECK(c.wanted == 2 && c.fit == 1 && c.ratio == 1 && c.clamped);
    CHECK(Near(c.uiScale, 1.0f));

    c = ChooseScale(75, 3840, 2160);   // fits 3x, the slider asks 2x
    CHECK(c.ratio == 2 && !c.clamped);

    c = ChooseScale(150, 3840, 2160);
    CHECK(c.ratio == 1 && !c.clamped && Near(c.uiScale, 1.0f));
}

UNIT_TEST(ext_sharp_ui_layout_is_exact) {
    // 1 / k as _uiScale must give retail's (int)(bb * scale) == bb / k exactly
    // on the common sizes, or the ratio is off by a column and nothing is
    // integer any more.
    struct Size { int w, h; };
    for (Size s : { Size{1280, 720}, Size{1920, 1080}, Size{2560, 1440}, Size{3840, 2160},
                    Size{1600, 1200}, Size{1366, 768}, Size{3440, 1440}, Size{5120, 2880} }) {
        for (int k : {1, 2, 3, 4}) {
            if (s.w % k != 0 || s.h % k != 0) continue;
            const float scale = SharpUI::UIScaleForRatio(k);
            CHECK(SharpUI::LayoutExtent(s.w, scale) == s.w / k);
            CHECK(SharpUI::LayoutExtent(s.h, scale) == s.h / k);
        }
    }
}

UNIT_TEST(ext_sharp_ui_choose_draw) {
    using SharpUI::ChooseDraw;
    SharpUI::DrawChoice d = ChooseDraw(0.5f, 2560, 1440);
    CHECK(d.layoutWidth == 1280 && d.layoutHeight == 720);
    CHECK(d.integer && d.integerRatio == 2 && d.snap && d.point);
    CHECK(Near(d.ratio, 2.0f));

    d = ChooseDraw(1.0f, 1920, 1080);   // 1x: snap, linear (nothing to gain from point)
    CHECK(d.integer && d.integerRatio == 1 && d.snap && !d.point);

    d = ChooseDraw(0.75f, 1920, 1080);  // retail's 75%: ratio 1.333, not integer
    CHECK(!d.integer && d.snap && !d.point && d.integerRatio == 0);

    d = ChooseDraw(1.5f, 1920, 1080);   // retail's 150%: ratio 0.667, minified
    CHECK(!d.integer && !d.point);

    d = ChooseDraw(0.5f, 1365, 767);    // odd back buffer at 2x: 682x383, ratio ~2.0015
    CHECK(d.integer && d.integerRatio == 2 && d.point);

    d = ChooseDraw(0.0f, 1920, 1080);   // degenerate (a never-applied scale): retail's frame
    CHECK(!d.snap && !d.point && !d.integer);
}

UNIT_TEST(ext_sharp_ui_snap) {
    using UI::SnapToPixel;
    // Integer ratio: every integer layout coordinate is already on a pixel.
    for (int v = -3; v < 40; ++v)
        CHECK(Near(SnapToPixel(static_cast<float>(v), 2.0f, 0.5f), static_cast<float>(v)));

    // Ratio 1.333 (retail 75%): the result lands on whole device pixels.
    const float px = 4.0f / 3.0f, inv = 0.75f;
    for (int v = 0; v < 64; ++v) {
        const float s = SnapToPixel(static_cast<float>(v), px, inv);
        const float dev = s * px;
        CHECK(Near(dev, std::floor(dev + 0.5f), 1e-3f));
        CHECK(std::fabs(s - static_cast<float>(v)) <= 0.5f * inv + 1e-4f);   // moved by at most half a pixel
    }

    // Monotonic: a quad never inverts, shared edges stay shared.
    float prev = SnapToPixel(0.0f, px, inv);
    for (int v = 1; v < 64; ++v) {
        const float s = SnapToPixel(static_cast<float>(v), px, inv);
        CHECK(s >= prev);
        prev = s;
    }

    // Odd back buffer at 2x (1365 / 682): the right edge lands on the last pixel.
    const float pxOdd = 1365.0f / 682.0f;
    const float e = SnapToPixel(682.0f, pxOdd, 1.0f / pxOdd);
    CHECK(Near(e * pxOdd, 1365.0f, 1e-2f));
}

UNIT_TEST(ext_sharp_ui_pixel_exact) {
    using UI::AxisIsPixelExact;
    // 2x: a 16-texel icon drawn at its 16 layout units = 32 px: exact.
    CHECK(AxisIsPixelExact(16, 2.0f, 16));
    // A glyph drawn at its size at 3x: exact.
    CHECK(AxisIsPixelExact(7, 3.0f, 7));
    // A 1920-texel backdrop letterboxed into 1280 layout units at 2x (1.333): filtered.
    CHECK(!AxisIsPixelExact(1280, 2.0f, 1920));
    // A 9-slice edge, 8 texels stretched to 203 layout units at 2x (50.75): a fill.
    CHECK(AxisIsPixelExact(203, 2.0f, 8));
    // A sprite drawn at half size at 2x (1 px per texel): exact.
    CHECK(AxisIsPixelExact(8, 2.0f, 16));
    // ... at a third of its size (0.667 px per texel): minified, filtered.
    CHECK(!AxisIsPixelExact(6, 2.0f, 18));
    // Mirrored (x2 < x1) and empty sprites.
    CHECK(AxisIsPixelExact(-16, 2.0f, 16));
    CHECK(AxisIsPixelExact(5, 2.0f, 0));

    UI::SharpDraw s{2.0f, 2.0f, 0.5f, 0.5f, true};
    CHECK(UI::BatchQuadIsPixelExact(s, 10, 10, 26, 30, 16, 20));
    CHECK(!UI::BatchQuadIsPixelExact(s, 0, 0, 1280, 720, 1920, 1080));
}
