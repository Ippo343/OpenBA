// ============================================================================
// src/ext/sharp_ui/sharp_math.cpp -- ext.sharp_ui's pure decisions
// ============================================================================
// NOT RETAIL. sharp_math.h has the contract.
// ============================================================================
#include "ext/sharp_ui/sharp_math.h"

#include <cmath>

namespace SharpUI {

int WantedRatio(int percent) {
    if (percent <= 0) return 1;
    const int k = (100 + percent - 1) / percent;
    return k < 1 ? 1 : k;
}

int FitRatio(int backBufferWidth, int backBufferHeight) {
    const int kx = backBufferWidth / kMinLayoutWidth;
    const int ky = backBufferHeight / kMinLayoutHeight;
    const int k = kx < ky ? kx : ky;
    return k < 1 ? 1 : k;
}

float UIScaleForRatio(int ratio) {
    if (ratio < 1) ratio = 1;
    return 1.0f / static_cast<float>(ratio);
}

int32_t LayoutExtent(int32_t backBufferExtent, float uiScale) {
    return static_cast<int32_t>(static_cast<float>(backBufferExtent) * uiScale);
}

ScaleChoice ChooseScale(int percent, int backBufferWidth, int backBufferHeight) {
    ScaleChoice c;
    c.percent = percent;
    c.wanted  = WantedRatio(percent);
    c.fit     = FitRatio(backBufferWidth, backBufferHeight);
    c.clamped = c.wanted > c.fit;
    c.ratio   = c.clamped ? c.fit : c.wanted;
    c.uiScale = UIScaleForRatio(c.ratio);
    return c;
}

DrawChoice ChooseDraw(float uiScale, int32_t backBufferWidth, int32_t backBufferHeight) {
    DrawChoice c;
    c.layoutWidth  = LayoutExtent(backBufferWidth, uiScale);
    c.layoutHeight = LayoutExtent(backBufferHeight, uiScale);
    c.ratio        = uiScale > 0.0f ? 1.0f / uiScale : 0.0f;
    c.pxPerUnitX   = 0.0f;
    c.pxPerUnitY   = 0.0f;
    c.integerRatio = 0;
    c.integer      = false;
    c.snap         = false;
    c.point        = false;
    if (c.layoutWidth <= 0 || c.layoutHeight <= 0 || backBufferWidth <= 0 || backBufferHeight <= 0)
        return c;   // degenerate: retail's own frame, nothing to snap to

    c.pxPerUnitX = static_cast<float>(backBufferWidth) / static_cast<float>(c.layoutWidth);
    c.pxPerUnitY = static_cast<float>(backBufferHeight) / static_cast<float>(c.layoutHeight);
    c.snap = true;

    const float rx = std::floor(c.pxPerUnitX + 0.5f);
    const float ry = std::floor(c.pxPerUnitY + 0.5f);
    if (rx >= 1.0f && rx == ry &&
        std::fabs(c.pxPerUnitX - rx) <= kIntegerTolerance &&
        std::fabs(c.pxPerUnitY - ry) <= kIntegerTolerance) {
        c.integer      = true;
        c.integerRatio = static_cast<int>(rx);
        c.point        = c.integerRatio >= 2;
    }
    return c;
}

}  // namespace SharpUI
