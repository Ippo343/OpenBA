# third_party/freetype_build -- FreeType (the submodule third_party/freetype, tag VER-2-13-3)
# built as one static C library with the minimal module set the run-time font rasteriser
# needs (src/platform/common/font_raster_freetype.cpp, KITFREE FREETYPE): the TrueType
# driver (with its bytecode interpreter, v35 and v40), the sfnt table loader, the anti-aliased
# "smooth" renderer and the monochrome "raster1" renderer (GDI's 8x8 point-sampled coverage is
# rendered through it at 8x scale; ftbbox.c gives the exact outline box that path uses). No
# glyph names (psnames), no auto-hinter (autofit: measured further from GDI than the native
# path, FREETYPE.md), no zlib, bzip2, png, HarfBuzz or brotli: banished_ftoption.h
# switches those off, so no WOFF / compressed-font support, which the rasteriser never wants.
# README.banished.md has the licence choice (FTL) and the size figures.
#
# The library reads no environment (FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES is off: the
# tree's "never getenv" rule). Built -O2 -w in every flavour, like wmadec.
enable_language(C)

set(FT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../freetype)
if(NOT EXISTS ${FT_ROOT}/include/freetype/freetype.h)
  message(FATAL_ERROR "third_party/freetype is empty: run `git submodule update --init`")
endif()

set(FREETYPE_SRCS
  ${FT_ROOT}/src/base/ftbase.c
  ${FT_ROOT}/src/base/ftinit.c
  ${FT_ROOT}/src/base/ftsystem.c
  ${FT_ROOT}/src/base/ftdebug.c
  ${FT_ROOT}/src/base/ftbbox.c
  ${FT_ROOT}/src/sfnt/sfnt.c
  ${FT_ROOT}/src/truetype/truetype.c
  ${FT_ROOT}/src/smooth/smooth.c
  ${FT_ROOT}/src/raster/raster.c)

add_library(freetype_min STATIC ${FREETYPE_SRCS})
set_target_properties(freetype_min PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)
# The two config headers are PUBLIC: every translation unit that includes <ft2build.h>
# (the rasteriser, the unit test) must see the same options and module list.
target_include_directories(freetype_min
  PUBLIC ${FT_ROOT}/include ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(freetype_min
  PUBLIC  FT_CONFIG_OPTIONS_H=<banished_ftoption.h> FT_CONFIG_MODULES_H=<banished_ftmodule.h>
  PRIVATE FT2_BUILD_LIBRARY NDEBUG)
target_compile_options(freetype_min PRIVATE -O2 -w)
