// ============================================================================
// src/platform/common/display_geometry.cpp -- outer / client rect arithmetic, display probe
// ============================================================================
// Phase D.4 SDL step W4. See display_geometry.h. Pure.
// ============================================================================

#include "display_geometry.h"

namespace Platform {
namespace Window {

void ClientRectToOuter(Rect* r, const FrameBorders& b) {
    r->left   -= b.left;
    r->top    -= b.top;
    r->right  += b.right;
    r->bottom += b.bottom;
}

void OuterToClient(int32_t x, int32_t y, int32_t w, int32_t h, const FrameBorders& b,
                   int32_t* clientX, int32_t* clientY, int32_t* clientW, int32_t* clientH) {
    *clientX = x + b.left;
    *clientY = y + b.top;
    *clientW = w - b.left - b.right;
    *clientH = h - b.top - b.bottom;
}

bool RectTouchesAnyDisplay(const Rect& r, const Rect* displays, int count) {
    if (r.right <= r.left || r.bottom <= r.top) return false;
    for (int i = 0; i < count; ++i) {
        const Rect& d = displays[i];
        if (r.left < d.right && d.left < r.right && r.top < d.bottom && d.top < r.bottom)
            return true;
    }
    return false;
}

}  // namespace Window
}  // namespace Platform
