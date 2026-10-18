#pragma once
// ============================================================================
// src/ext/resources/font_raster.h -- the run-time glyph rasteriser (KITFREE RSCFONT)
// ============================================================================
// NOT RETAIL. ext.sharp_ui's font variants (Font/Sharp<Kit>_<px>.rsc) are the
// game's four fonts rasterised at another pixel height. The mod kit did that
// with its profile Runtime DLL's Font::Compile, which is Win32 GDI:
// CreateFontIndirect with lfHeight = +px (the CELL height: tmHeight == px,
// so the line height is the height asked for and the baseline sits
// tmDescent above the line's bottom), then GetGlyphOutlineW(GGO_GRAY8_BITMAP)
// per codepoint. Measured against the kit's own output
// (docs/extensions/handins/RSCFONT.md): the bitmaps are pixel-identical with
// alpha = level * 4 - 1 (level 1..64; 0 stays 0), the glyph row is
//   preAdvance  = gmptGlyphOrigin.x
//   postAdvance = gmBlackBoxX            (the sprite's width)
//   postKern    = gmCellIncX - preAdvance - postAdvance
//   offsetY     = gmptGlyphOrigin.y - gmBlackBoxY   (sprite bottom above the baseline)
// and the space is GDI's own 1x1 empty box. 48 of 768 rows checked differ
// from the kit by one pixel moved between preAdvance / postAdvance and
// postKern (the advance is the same); the kit's rule for those is not
// recoverable from its output and the difference is below the rounding the
// user ruled acceptable.
//
// Standard C++ interface; the body is the platform seam
// src/platform/win/font_raster_win.cpp (GDI) with the posix twin
// src/platform/posix/font_raster_posix.cpp answering "absent".
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace Ext {
namespace Resources {

// The typeface the game's fonts are compiled from (the kit's Font/*.rsc
// `_fontName`); sharp_ui checks the same name on UI::Font::_string20.
constexpr const char* kGameFace = "FrancophilSans";

struct RasterGlyph {
    int32_t codepoint = 0;
    int32_t preAdvance = 0, postKern = 0, postAdvance = 0, offsetY = 0;   // UI::Font::Kerning, field for field
    int32_t width = 0, height = 0;    // the sprite: width x height coverage bytes, row-major, top row first
    std::vector<uint8_t> alpha;       // 0..255, the kit's level * 4 - 1 mapping already applied
};

struct RasterFont {
    std::string face;          // what GetTextFace returned (== the face asked for, or the call failed)
    int32_t     height = 0;    // the pixel height asked for == tmHeight == UI::Font::_lineHeight
    int32_t     baseLine = 0;  // tmDescent == UI::Font::_baseLine
    std::vector<RasterGlyph> glyphs;   // in the order of `codepoints`
};

// Rasterises `count` codepoints of `face` at `pixelHeight`. False (and `why`
// filled) when the face is not available (never a substitute: the face name
// GDI hands back is compared to the one asked for), when the height is not
// sane, or on a GDI failure. The face is looked up installed first, then as
// `<exe dir>/<face>.ttf` loaded privately (AddFontResourceEx FR_PRIVATE) --
// the release package ships it beside the exe; it is never in the git tree.
bool RasterizeFont(const char* face, int32_t pixelHeight, const int32_t* codepoints,
                   int32_t count, RasterFont& out, std::string* why);

// True when `face` can be selected (installed or the private file beside the
// exe); the private load is remembered for the process. `why` says which.
bool FaceAvailable(const char* face, std::string* why);

}  // namespace Resources
}  // namespace Ext
