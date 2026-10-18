#pragma once
// ============================================================================
// src/platform/common/font_raster_freetype.h -- the FreeType glyph rasteriser
// (KITFREE FREETYPE)
// ============================================================================
// NOT RETAIL. The portable engine behind ext/resources/font_raster.h's
// contract: the posix twin (src/platform/posix/font_raster_posix.cpp) is this
// over a TTF file found by name, and on Windows the unit test rasterises the
// same heights with it and with GDI (src/platform/win/font_raster_win.cpp) to
// measure how close the two come (docs/extensions/handins/FREETYPE.md).
//
// The contract to meet is GDI's (font_raster.h): CreateFontIndirect with
// lfHeight = +px is a CELL height -- tmHeight == px, tmAscent / tmDescent the
// OS/2 usWinAscent / usWinDescent scaled and rounded -- then
// GetGlyphOutlineW(GGO_GRAY8_BITMAP): the black box, its origin, the cell
// advance, 65 coverage levels. FreeType sizes the EM, not the cell, so the
// scale is derived here: s = px * unitsPerEm / (winAscent + winDescent)
// (hhea when there is no OS/2 table, GDI's own fallback). GDI's 65 levels are
// an 8x8 point-sampled supersample, not an area (measured: every edge level
// GDI gives is a multiple of 8/64 where the exact area is not), and its box
// leaves out a row or column the outline touches without a subsample centre
// in it; `Sampling::Grid8x8` renders that way through the monochrome
// renderer at 8x. `Options` picks the sizing, the hinting and the sampling:
// the comparison test tries them all; `DefaultOptions()` is the closest set.
//
// Thread-safe (one process-wide FT_Library under one mutex). Reads no
// environment (the library is built without FREETYPE_PROPERTIES support).
// ============================================================================

#include <cstdint>
#include <string>

#include "ext/resources/font_raster.h"

namespace Ext {
namespace Resources {
namespace FreeTypeRaster {

// How the cell height becomes a FreeType size.
enum class Sizing : uint8_t {
    IntegerPpem,      // ppem = round(s): FT_Set_Pixel_Sizes (the TrueType rule, head flag 3)
    FractionalPpem,   // ppem = s to 1/64: FT_Set_Char_Size (an unhinted load keeps the fraction)
    FittedPpem,       // the integer ppem nearest s whose round(ascent) + round(descent) == px
};

// How the outline is hinted before rendering.
enum class Hinting : uint8_t {
    Native,       // the font's own bytecode (FT_LOAD_TARGET_NORMAL | FT_LOAD_NO_AUTOHINT)
    None,         // FT_LOAD_NO_HINTING
    AutoLight,    // FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_LIGHT
    AutoNormal,   // FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL
};

// How the coverage of a pixel is measured.
enum class Sampling : uint8_t {
    ExactArea,    // FreeType's smooth renderer: the exact area covered, 256 levels
    Grid8x8,      // GDI's GGO_GRAY8: the outline rendered monochrome at 8x and the set subsamples
                  // counted per pixel (0..64); the box trimmed to the pixels a subsample hit
};

struct Options {
    Sizing   sizing      = Sizing::IntegerPpem;
    Hinting  hinting     = Hinting::Native;
    Sampling sampling    = Sampling::Grid8x8;
    int32_t  interpreter = 40;     // TT_INTERPRETER_VERSION_35 or _40 (only a hinted font tells them apart)
    bool     quantize    = true;   // ExactArea: 256 levels -> GDI's 65 (alpha = level * 4 - 1); false keeps 8-bit coverage
    bool     dropouts    = false;  // Grid8x8: raster1's drop-out control on the 8x scan (GDI's own is unknown)
    bool     trim        = true;   // Grid8x8: trim the box to the touched pixels (GDI's box); false keeps the outline's box
};

// The option set the GDI comparison found closest (FREETYPE.md's table).
const Options& DefaultOptions();

// Rasterises `count` codepoints of the face in `path` at the cell height
// `pixelHeight`, GDI's contract (font_raster.h): `out.height` == px,
// `out.baseLine` == the rounded descent, every glyph row pre / post / kern /
// offsetY from the rendered box, an empty glyph as GDI's 1x1 blank box.
// False (and `why`) when the file does not open, its family name is not
// `face` (compared without case, as GDI's GetTextFace check does), the height
// is not sane, or a glyph fails to render.
bool RasterizeFile(const char* path, const char* face, int32_t pixelHeight, const int32_t* codepoints,
                   int32_t count, const Options& options, RasterFont& out, std::string* why);

// The family name of the face in `path` (its name table), or false.
bool FamilyName(const char* path, std::string& family, std::string* why);

// Platform lookup of `<face>.ttf` by file name (never fontconfig): beside the
// exe first, then the user's and the system's font directories
// (src/platform/posix/font_raster_posix.cpp: ~/.fonts, ~/.local/share/fonts,
// /usr/share/fonts, /usr/local/share/fonts; src/platform/win/font_raster_win.cpp:
// the font registry's file for "<face> (TrueType)"). `why` names what was tried.
bool FindFaceFile(const char* face, std::string& path, std::string* why);

}  // namespace FreeTypeRaster
}  // namespace Resources
}  // namespace Ext
