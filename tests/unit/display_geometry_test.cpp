// ============================================================================
// tests/unit/display_geometry_test.cpp -- outer / client arithmetic, the display probe
// ============================================================================
// Phase D.4 SDL step W4. src/platform/common/display_geometry.cpp carries the Win32
// rect arithmetic Video::Device::AdjustDisplay was transcribed against, for the SDL
// backend that places client areas: AdjustWindowRect's growth, SetWindowPos's outer rect
// turned back into a client rect, and MonitorFromRect(DEFAULTTONULL)'s "on no monitor"
// answer that SDL_GetDisplayForRect cannot give. Fake display bounds, no SDL.
// ============================================================================
#include "unit.h"

#include <cstdint>

#include "display_geometry.h"

using Platform::Rect;
using Platform::Window::ClientRectToOuter;
using Platform::Window::FrameBorders;
using Platform::Window::OuterToClient;
using Platform::Window::RectTouchesAnyDisplay;

namespace {

// A Windows 10 WS_OVERLAPPEDWINDOW frame at 96 dpi: 8 px sides and bottom, 31 px top.
constexpr FrameBorders kFrame{31, 8, 8, 8};

Rect R(int32_t l, int32_t t, int32_t r, int32_t b) { return Rect{l, t, r, b}; }

}  // namespace

UNIT_TEST(display_geometry_client_to_outer_is_adjustwindowrect) {
    Rect r = R(0, 0, 1280, 720);
    ClientRectToOuter(&r, kFrame);
    CHECK_EQ(r.left, -8);
    CHECK_EQ(r.top, -31);
    CHECK_EQ(r.right, 1288);
    CHECK_EQ(r.bottom, 728);
    // A borderless window's frame is empty: the rect is unchanged.
    Rect p = R(0, 0, 1920, 1080);
    ClientRectToOuter(&p, FrameBorders{0, 0, 0, 0});
    CHECK_EQ(p.right, 1920);
    CHECK_EQ(p.bottom, 1080);
}

UNIT_TEST(display_geometry_outer_to_client_round_trip) {
    Rect r = R(0, 0, 1280, 720);
    ClientRectToOuter(&r, kFrame);
    int32_t cx = 0, cy = 0, cw = 0, ch = 0;
    // AdjustDisplay's SetWindowPos(offX, offY, r.w, r.h) with the outer size.
    OuterToClient(100, 50, r.right - r.left, r.bottom - r.top, kFrame, &cx, &cy, &cw, &ch);
    CHECK_EQ(cx, 108);
    CHECK_EQ(cy, 81);
    CHECK_EQ(cw, 1280);
    CHECK_EQ(ch, 720);
}

// Retail's persisted origin is the CLIENT origin (MoveWindow: ClientToScreen of 0,0), and
// AdjustDisplay places the OUTER rect there: each save-and-reopen moves the client by the
// frame. The arithmetic must reproduce that, not "fix" it.
UNIT_TEST(display_geometry_saved_origin_drift_is_retail) {
    const int32_t savedX = 300, savedY = 200;     // Configuration::windowX / Y
    Rect r = R(0, 0, 1024, 768);
    ClientRectToOuter(&r, kFrame);
    int32_t cx = 0, cy = 0, cw = 0, ch = 0;
    OuterToClient(savedX, savedY, r.right - r.left, r.bottom - r.top, kFrame, &cx, &cy, &cw, &ch);
    CHECK_EQ(cx, savedX + kFrame.left);
    CHECK_EQ(cy, savedY + kFrame.top);
}

UNIT_TEST(display_geometry_probe_on_two_displays) {
    const Rect displays[2] = { R(0, 0, 1920, 1080), R(1920, 0, 4480, 1440) };
    CHECK(RectTouchesAnyDisplay(R(100, 100, 900, 700), displays, 2));
    CHECK(RectTouchesAnyDisplay(R(2000, 100, 2800, 700), displays, 2));
    // Straddling the seam touches both.
    CHECK(RectTouchesAnyDisplay(R(1800, 100, 2100, 700), displays, 2));
    // Partly off the desktop still touches a display (MonitorFromRect is non-NULL).
    CHECK(RectTouchesAnyDisplay(R(-500, -500, 10, 10), displays, 2));
}

UNIT_TEST(display_geometry_probe_off_every_display_is_null) {
    const Rect displays[2] = { R(0, 0, 1920, 1080), R(1920, 0, 4480, 1440) };
    // Below the shorter display, left of the taller one's span: no monitor.
    CHECK(!RectTouchesAnyDisplay(R(100, 1100, 900, 1400), displays, 2));
    // Far off.
    CHECK(!RectTouchesAnyDisplay(R(-5000, -5000, -4000, -4000), displays, 2));
    // Sharing only an edge is no shared area.
    CHECK(!RectTouchesAnyDisplay(R(-800, 0, 0, 600), displays, 2));
    // An empty rect touches nothing; no displays, nothing either.
    CHECK(!RectTouchesAnyDisplay(R(100, 100, 100, 700), displays, 2));
    CHECK(!RectTouchesAnyDisplay(R(100, 100, 900, 700), displays, 0));
}

// AdjustDisplay's probe as retail builds it: `bottom` takes the X offset (0x1800f7f9c,
// EDI not ESI). With a far-right saved X and a small Y on a single display the probe
// collapses to an empty rect and reads "off every monitor", so the window is centred.
UNIT_TEST(display_geometry_retail_probe_bottom_uses_x_offset) {
    const Rect one[1] = { R(0, 0, 1920, 1080) };
    Rect rect = R(0, 0, 800, 600);
    ClientRectToOuter(&rect, kFrame);
    const int32_t offX = 100, offY = 2000;   // a Y below the display, an X on it
    Rect probe;
    probe.left   = rect.left   + offX;
    probe.right  = rect.right  + offX;
    probe.top    = rect.top    + offY;
    probe.bottom = rect.bottom + offX;       // retail's
    // top 1969 > bottom 708: empty, so no monitor; a correct probe would also miss here.
    CHECK(!RectTouchesAnyDisplay(probe, one, 1));
    // With a Y on the display and a large X, retail's bottom grows past top: on screen.
    const int32_t offX2 = 1000, offY2 = 100;
    probe.left   = rect.left   + offX2;
    probe.right  = rect.right  + offX2;
    probe.top    = rect.top    + offY2;
    probe.bottom = rect.bottom + offX2;
    CHECK(RectTouchesAnyDisplay(probe, one, 1));
}
