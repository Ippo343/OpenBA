// ============================================================================
// src/platform/common/cursor_planes.cpp -- a Win32 AND / XOR cursor as ARGB pixels
// ============================================================================
// Phase D.4 SDL step W5. The table is in cursor_planes.h.
// ============================================================================

#include "cursor_planes.h"

namespace Platform {
namespace Cursor {

int32_t PlaneStride(int32_t width) {
    return ((width + 15) / 16) * 2;
}

void PlanesToArgb(const uint8_t* andBytes, const uint8_t* xorBytes, int32_t width,
                  int32_t height, uint32_t inverted, uint32_t* argb, int32_t pitchPixels) {
    const int32_t stride = PlaneStride(width);

    for (int32_t y = 0; y < height; ++y) {
        uint32_t* const out = argb + static_cast<int64_t>(y) * pitchPixels;
        for (int32_t x = 0; x < width; ++x) {
            const int32_t  byte = y * stride + x / 8;
            const uint8_t  bit  = static_cast<uint8_t>(0x80u >> (x % 8));
            const bool     a    = (andBytes[byte] & bit) != 0;
            const bool     o    = (xorBytes[byte] & bit) != 0;
            out[x] = a ? (o ? inverted : kArgbTransparent)
                       : (o ? kArgbWhite : kArgbBlack);
        }
    }
}

}  // namespace Cursor
}  // namespace Platform
