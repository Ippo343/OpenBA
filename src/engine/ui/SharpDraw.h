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
//     drawn at their size times the ratio are point-sampled. (Retail's 75-150%
//     slider never gives an integer ratio >= 2, so this stays off in practice.)
//   * when `swaps` names the batch's sheet (FindSwap), the batch is drawn from
//     that sheet's TWIN: the twin's material is set up instead of the sheet's
//     and every record's UVs come from the twin's sprite `twinSprite[index]`.
//     A twin with `place` (a font variant: the same glyphs rasterised at the
//     device pixel height) also moves each glyph quad to the variant glyph's
//     own size in device pixels, anchored at the base glyph's pen and baseline
//     (PlaceGlyph). A twin without `place` (a 2x sprite sheet) keeps retail's
//     quad. The records -- and so ComputeStringLayout / ComputeLineLayout,
//     hit-testing and SpriteCapture, which all run before Flush -- never see a
//     twin: layout is the base font's, unit for unit.
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

class SpriteSheet;

// One base glyph drawn from a font variant. Base metrics in LAYOUT units (the
// retail Font's), variant metrics in DEVICE pixels (the variant is rasterised
// at the device pixel height, so its texels are device pixels):
//   pen      = x1 + originX     (x1 = pen + the base _preAdvance: ComputeLineLayout)
//   baseline = y2 + originY     (y2 = baseline - the base _offsetY)
//   left     = round(pen * px) + devX           devX = the variant _preAdvance
//   bottom   = round(baseline * px) + devY      devY = -(the variant _offsetY)
//   the quad = [left, left + devW) x [bottom - devH, bottom), devW x devH the
//              variant sprite: one texel per device pixel.
struct SharpGlyphPlace {
    int32_t originX, originY;   // layout units
    int32_t devX, devY;         // device pixels
    int32_t devW, devH;         // device pixels
};

// A sheet drawn from its twin (see the banner). `twinSprite` and `place` (when
// set) have `count` entries, indexed by the BASE sprite index; a twin index < 0
// draws nothing (zero UVs, as retail's negative index does).
struct SharpSheetSwap {
    const SpriteSheet*     base;
    const SpriteSheet*     twin;
    const int32_t*         twinSprite;
    const SharpGlyphPlace* place;   // null: retail's quad (a 2x sprite twin)
    int32_t                count;
};

struct SharpDraw {
    float pxPerUnitX;   // device pixels per layout unit, x (back buffer / layout width)
    float pxPerUnitY;   // ... y
    float unitsPerPxX;  // 1 / pxPerUnitX
    float unitsPerPxY;  // 1 / pxPerUnitY
    bool  point;        // the frame is at an integer ratio >= 2: batches may point-sample
    const SharpSheetSwap* swaps;   // the twins in force this frame (may be null)
    int32_t               swapCount;
};

// The swap for `sheet`, or null. Called once per Flush, never per record.
inline const SharpSheetSwap* FindSwap(const SharpDraw& s, const SpriteSheet* sheet) {
    for (int32_t i = 0; i < s.swapCount; ++i)
        if (s.swaps[i].base == sheet) return &s.swaps[i];
    return nullptr;
}

// A glyph quad in layout units (the corners Flush emits before the half-pixel).
struct SharpQuad { float x1, y1, x2, y2; };
inline SharpQuad PlaceGlyph(const SharpGlyphPlace& g, int32_t x1, int32_t y2,
                            float pxPerUnitX, float pxPerUnitY,
                            float unitsPerPxX, float unitsPerPxY) {
    const float left   = std::floor(static_cast<float>(x1 + g.originX) * pxPerUnitX + 0.5f) +
                         static_cast<float>(g.devX);
    const float bottom = std::floor(static_cast<float>(y2 + g.originY) * pxPerUnitY + 0.5f) +
                         static_cast<float>(g.devY);
    SharpQuad q;
    q.x1 = left * unitsPerPxX;
    q.y1 = (bottom - static_cast<float>(g.devH)) * unitsPerPxY;
    q.x2 = (left + static_cast<float>(g.devW)) * unitsPerPxX;
    q.y2 = bottom * unitsPerPxY;
    return q;
}

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
