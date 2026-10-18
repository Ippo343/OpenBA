/* third_party/freetype_build/banished_ftoption.h -- OpenBA's FreeType options
 * (FT_CONFIG_OPTIONS_H). The upstream defaults, then everything the run-time font
 * rasteriser does not want switched off: no compressed / WOFF streams (zlib, LZW,
 * bzip2, brotli), no PNG / SVG / colour-layer glyphs, no Mac resource forks, no
 * incremental loading, no GX variations, no embedded bitmap strikes, no BDF properties, and -- the tree's
 * "never getenv" rule -- no FREETYPE_PROPERTIES environment lookup. The bytecode
 * interpreter stays (TT_CONFIG_OPTION_BYTECODE_INTERPRETER, v35 and v40 selectable
 * through the "interpreter-version" property). */
#ifndef BANISHED_FTOPTION_H_
#define BANISHED_FTOPTION_H_

#include <freetype/config/ftoption.h>

#undef FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES
#undef FT_CONFIG_OPTION_USE_LZW
#undef FT_CONFIG_OPTION_USE_ZLIB
#undef FT_CONFIG_OPTION_USE_BZIP2
#undef FT_CONFIG_OPTION_USE_PNG
#undef FT_CONFIG_OPTION_USE_HARFBUZZ
#undef FT_CONFIG_OPTION_USE_BROTLI
#undef FT_CONFIG_OPTION_MAC_FONTS
#undef FT_CONFIG_OPTION_GUESSING_EMBEDDED_RFORK
#undef FT_CONFIG_OPTION_INCREMENTAL
#undef FT_CONFIG_OPTION_SVG
#undef TT_CONFIG_OPTION_EMBEDDED_BITMAPS
#undef TT_CONFIG_OPTION_COLOR_LAYERS
#undef TT_CONFIG_OPTION_GX_VAR_SUPPORT
#undef TT_CONFIG_OPTION_BDF
#undef TT_SUPPORT_COLRV1
#undef AF_CONFIG_OPTION_CJK
#undef AF_CONFIG_OPTION_INDIC

#endif /* BANISHED_FTOPTION_H_ */
