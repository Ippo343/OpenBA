# third_party/freetype_build -- FreeType for the run-time font rasteriser

`third_party/freetype` is the official FreeType mirror (https://github.com/freetype/freetype)
as a git submodule pinned at tag **VER-2-13-3** (commit `42608f77f20749dd6ddc9e0536788eaad70ea4b5`,
2024-08-10), unmodified. This directory is OpenBA's build of it: `CMakeLists.txt` compiles a
hand-picked set of the upstream aggregate sources into the static library `freetype_min`, and
the two headers replace upstream's option and module lists through `FT_CONFIG_OPTIONS_H` /
`FT_CONFIG_MODULES_H` (both PUBLIC definitions, so every user of `<ft2build.h>` sees the same
configuration). Block FREETYPE of KITFREE (`docs/extensions/KITFREE.md`, hand-in
`docs/extensions/handins/FREETYPE.md`).

## Licence

FreeType is dual-licensed: the FreeType License (FTL, BSD-style with an advertising clause)
or the GNU GPL v2 or later (`third_party/freetype/LICENSE.TXT`, `docs/FTL.TXT`, `docs/GPLv2.TXT`).
**OpenBA takes FreeType under the FTL.** OpenBA itself is GPL-3.0; the FTL is GPL-compatible
and asks for one thing in return: a credit in the documentation of a product that uses it.
README.md's licence list carries that credit ("Portions of this software are copyright
(c) The FreeType Project (www.freetype.org). All rights reserved.").

## What is built

| Source | Module | Why |
|---|---|---|
| `src/base/ftbase.c`, `ftinit.c`, `ftsystem.c`, `ftdebug.c` | base | the library, streams, FT_Load_Glyph, FT_Outline_* |
| `src/base/ftbbox.c` | base | `FT_Outline_Get_BBox`, the exact box the 8x8 path trims from |
| `src/sfnt/sfnt.c` | sfnt | the TrueType table loader (cmap, hmtx, OS/2 for the cell metrics) |
| `src/truetype/truetype.c` | truetype | the TrueType driver with its bytecode interpreter (v35 and v40) |
| `src/smooth/smooth.c` | smooth | exact-area anti-aliasing (`Sampling::ExactArea`) |
| `src/raster/raster.c` | raster1 | the monochrome scan converter the GDI-like 8x8 supersample is counted from |

Everything else is off, through `banished_ftoption.h`: zlib / LZW / bzip2 / brotli streams
(no WOFF), PNG / SVG / colour glyphs, embedded bitmap strikes, GX variations, Mac resource
forks, incremental loading, BDF, PostScript glyph names, and **the `FREETYPE_PROPERTIES`
environment lookup** (the tree's "never getenv" rule: the library never reads the environment).
No system library is needed beyond the C runtime. The auto-hinter was tried for the GDI
comparison and dropped (further from GDI than the native path, FREETYPE.md).

Built `-O2 -w` in every flavour like `wmadec` (the warnings are upstream's).

## Size

`size` of the archive's members (x86_64, GCC 13, -O2): ftbase 46.3 KB, sfnt 34.3 KB, truetype
42.5 KB, smooth 8.0 KB, raster1 8.9 KB, ftbbox 1.5 KB, the rest under 2 KB -- **141 KB of
text** linked into the exe (the earlier set with psnames + autofit was 253 KB). The exe-level
figures are in FREETYPE.md.

## Updating

`cd third_party/freetype && git fetch --depth 1 origin tag VER-2-x-y && git checkout VER-2-x-y`,
then rebuild; if a new release adds a `src/<module>/*.c` aggregate the modules here need, add
it to `CMakeLists.txt` and `banished_ftmodule.h` together.
