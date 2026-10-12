// ============================================================================
// src/ext/sharp_ui/sharp_math.h -- ext.sharp_ui's pure decisions
// ============================================================================
// NOT RETAIL. Standard C++ only (tests/unit/ext_sharp_ui_test.cpp links
// sharp_math.cpp on its own). docs/extensions/handins/SHARPUI.md has the why.
//
// UNITS. Retail's `UserInterface::_uiScale` is LAYOUT UNITS PER DEVICE PIXEL:
// the layout space is (int)(backBuffer * _uiScale) (userinterface_draw.cpp,
// element_common.h:169) and UserInterface::Draw's orthographic camera maps it
// onto the whole back buffer. So the device-pixel RATIO (device pixels per
// layout unit) is 1 / _uiScale: the Options slider's 75% (_uiScale 0.75) is
// ratio 1.333 (a bigger UI), 150% is ratio 0.667 (a smaller UI). An integer
// ratio k >= 2 is _uiScale 1/k: "2x" is retail's 50%, outside its 75-150 slider.
//
// THE SNAP RULE (the user's decision, 2026-10-11 night: with the extension on
// the EFFECTIVE scale is an integer ratio; the slider's value is kept as is):
//   wanted = ceil(100 / slider%)        75..99% -> 2x, 100..150% -> 1x
//   fit    = the largest k with backBuffer / k >= kMinLayoutWidth x
//            kMinLayoutHeight (retail's own floor, 800x600 at 100%), >= 1
//   ratio  = min(wanted, fit)            (clamped when wanted > fit)
//   _uiScale = 1 / ratio
// ============================================================================
#pragma once

#include <cstdint>

namespace SharpUI {

// Retail's smallest supported layout at its default 100%: the display-mode
// list drops every mode narrower than 800 pixels (devicedata.cpp:1698, retail
// `CMP dword ptr [RBX],0x320`), and 800x600 is the smallest such mode.
constexpr int kMinLayoutWidth  = 800;
constexpr int kMinLayoutHeight = 600;

// ceil(100 / percent); 1 for a non-positive percent (a broken settings value).
int WantedRatio(int percent);

// The largest integer ratio whose layout space is at least the floor; 1 when
// even 1x is below it (the floor never makes the UI smaller than 1x).
int FitRatio(int backBufferWidth, int backBufferHeight);

// 1 / ratio: the _uiScale for an integer ratio (ratio >= 1).
float UIScaleForRatio(int ratio);

// The layout extent retail derives from a back-buffer extent and _uiScale:
// (int)((float)extent * scale), exactly element_common.h:169's arithmetic.
int32_t LayoutExtent(int32_t backBufferExtent, float uiScale);

struct ScaleChoice {
    int   percent;     // the slider value (settings)
    int   wanted;      // WantedRatio(percent)
    int   fit;         // FitRatio(back buffer)
    int   ratio;       // min(wanted, fit), >= 1
    bool  clamped;     // wanted > fit
    float uiScale;     // UIScaleForRatio(ratio)
};
ScaleChoice ChooseScale(int percent, int backBufferWidth, int backBufferHeight);

// What a frame drawn at `uiScale` gets.
struct DrawChoice {
    int32_t layoutWidth, layoutHeight;   // LayoutExtent of each axis
    float   pxPerUnitX, pxPerUnitY;      // backBuffer / layout, per axis (0 when degenerate)
    float   ratio;                       // 1 / uiScale (the trace's ratio)
    int     integerRatio;                // round(pxPerUnitX) when `integer`, else 0
    bool    integer;   // both axes within kIntegerTolerance of the same integer >= 1
    bool    snap;      // snap quad corners to whole device pixels (any sane frame)
    bool    point;     // point-sample the UI sheets: integer ratio >= 2 only
};
constexpr float kIntegerTolerance = 0.01f;
DrawChoice ChooseDraw(float uiScale, int32_t backBufferWidth, int32_t backBufferHeight);

}  // namespace SharpUI
