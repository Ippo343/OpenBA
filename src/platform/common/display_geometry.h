#pragma once
// ============================================================================
// src/platform/common/display_geometry.h -- outer / client rect arithmetic, display probe
// ============================================================================
// Phase D.4 SDL step W4 (plan §A.3). Video::Device::AdjustDisplay is retail's Win32
// arithmetic on OUTER window rects: AdjustWindowRect grows the wanted client rect by the
// frame, SetWindowPos places the outer rect, MonitorFromRect(DEFAULTTONULL) asks whether
// a rect touches any monitor. SDL positions and sizes the CLIENT area and has no "is this
// rect on a display" call (SDL_GetDisplayForRect never answers "none": it returns the
// closest display). These pure helpers carry the arithmetic between the two so the SDL
// backend (src/platform/sdl/display_sdl.cpp) can keep retail's outer-rect behaviour, and
// so the unit tests can check it on fake bounds (tests/unit/display_geometry_test.cpp).
// No OS header, no SDL header.
// ============================================================================

#include <cstdint>

#include "platform_window.h"

namespace Platform {
namespace Window {

// The frame around the client area, in pixels, each side >= 0 (SDL_GetWindowBordersSize's
// four values; AdjustWindowRect's growth).
struct FrameBorders {
    int32_t top;
    int32_t left;
    int32_t bottom;
    int32_t right;
};

// AdjustWindowRect's arithmetic: grow a client rect by the frame, in place.
void ClientRectToOuter(Rect* inOut, const FrameBorders& b);

// SetWindowPos's arithmetic, reversed: the client rect an outer rect at (x, y), w x h
// leaves inside a frame `b`.
void OuterToClient(int32_t x, int32_t y, int32_t w, int32_t h, const FrameBorders& b,
                   int32_t* clientX, int32_t* clientY, int32_t* clientW, int32_t* clientH);

// MonitorFromRect(rect, MONITOR_DEFAULTTONULL) != NULL: does `rect` share any area with
// one of `count` display rects? An empty rect (right <= left or bottom <= top) touches
// nothing.
bool RectTouchesAnyDisplay(const Rect& rect, const Rect* displays, int count);

}  // namespace Window
}  // namespace Platform
