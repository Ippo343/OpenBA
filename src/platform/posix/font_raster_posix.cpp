// ============================================================================
// src/platform/posix/font_raster_posix.cpp -- the posix twin of font_raster_win.cpp
// ============================================================================
// NOT RETAIL. No rasteriser off Windows yet (KITFREE.md §3: FreeType later):
// the face is "absent", so ext.sharp_ui keeps its retail(...) fallback.
// ============================================================================
#include "ext/resources/font_raster.h"

namespace Ext {
namespace Resources {

bool FaceAvailable(const char*, std::string* why) {
    if (why) *why = "no rasteriser on this platform";
    return false;
}

bool RasterizeFont(const char*, int32_t, const int32_t*, int32_t, RasterFont& out, std::string* why) {
    out.glyphs.clear();
    out.height = 0;
    out.baseLine = 0;
    if (why) *why = "no rasteriser on this platform";
    return false;
}

}  // namespace Resources
}  // namespace Ext
