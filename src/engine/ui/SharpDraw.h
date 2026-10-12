// ============================================================================
// src/engine/ui/SharpDraw.h -- the engine end of ext.sharp_ui (NOT RETAIL)
// ============================================================================
// docs/extensions/DESIGN.md §3 rule 5: an engine-side seam is a null-by-default
// static data pointer the game side sets. `UI::SpriteContext::_extSharp`
// (SpriteContext.h) points at one of these for the duration of one
// UI::UserInterface::Draw, and only while ext.sharp_ui is on (the game side's
// SharpUI::Draw sets it and nulls it again). SpriteContext::Flush reads the
// pointer ONCE per flush; null (always, in the faithful profile) = retail's
// vertices and retail's samplers, byte for byte. Non-null:
//   * each axis-aligned quad's corners move to the nearest whole device pixel;
//   * when `point` is set (an integer ratio >= 2), the flush picks the sampler
//     for its batch: POINT (Video::ImageBufferData::_extPointSampling) when
//     every axis-aligned quad in it is drawn pixel-exact (BatchQuadIsPixelExact),
//     else the image's own (linear) sampler. A picture stretched to the screen
//     (a dialog backdrop) stays filtered; glyphs, icons and 9-slice frames
//     drawn at their size times the ratio are point-sampled.
//
// The vertex positions Flush emits are in LAYOUT units; UserInterface::Draw's
// orthographic camera maps layout [0, W) x [0, H) onto the back buffer, so a
// layout coordinate v sits at device pixel v * pxPerUnit.
//
// Standard C++ only: tests/unit/ext_sharp_ui_test.cpp includes it.
// ============================================================================
#pragma once

#include <cmath>
#include <cstdint>

namespace UI {

struct SharpDraw {
    float pxPerUnitX;   // device pixels per layout unit, x (back buffer / layout width)
    float pxPerUnitY;   // ... y
    float unitsPerPxX;  // 1 / pxPerUnitX
    float unitsPerPxY;  // 1 / pxPerUnitY
    bool  point;        // the frame is at an integer ratio >= 2: batches may point-sample
};

// v (layout units) moved to the nearest whole device pixel, back in layout
// units: round(v * px) / px. Halves round up (floor(x + 0.5)), the same way for
// every quad, so two quads that share an edge in layout space share it in
// device space.
inline float SnapToPixel(float v, float pxPerUnit, float unitsPerPx) {
    return std::floor(v * pxPerUnit + 0.5f) * unitsPerPx;
}

// A stretch at least this large is a fill (a 9-slice edge or centre, a bar):
// its texels are uniform along the stretch, where POINT and LINEAR agree, and
// POINT keeps the other axis crisp.
constexpr float kSharpStretchFill = 4.0f;
constexpr float kSharpExactTolerance = 0.01f;

// One axis of one quad: `layoutExtent` layout units drawn from `spriteExtent`
// texels. Pixel-exact when each texel covers a whole number (>= 1) of device
// pixels, or when it is a fill (above). A zero / negative sprite extent has no
// texels to misplace.
inline bool AxisIsPixelExact(int32_t layoutExtent, float pxPerUnit, int32_t spriteExtent) {
    if (spriteExtent <= 0) return true;
    float m = static_cast<float>(layoutExtent) * pxPerUnit / static_cast<float>(spriteExtent);
    if (m < 0.0f) m = -m;
    if (m >= kSharpStretchFill) return true;
    const float r = std::floor(m + 0.5f);
    return r >= 1.0f && std::fabs(m - r) <= kSharpExactTolerance * r;
}

inline bool BatchQuadIsPixelExact(const SharpDraw& s, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                  int32_t spriteWidth, int32_t spriteHeight) {
    return AxisIsPixelExact(x2 - x1, s.pxPerUnitX, spriteWidth) &&
           AxisIsPixelExact(y2 - y1, s.pxPerUnitY, spriteHeight);
}

}  // namespace UI
