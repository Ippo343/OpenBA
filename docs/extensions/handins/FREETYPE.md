# FREETYPE hand-in (KITFREE, 2026-10-18)

A FreeType rasteriser as the posix twin of the GDI one (`src/platform/win/font_raster_win.cpp`,
RSCFONT), built so it can replace GDI on Windows if its output matches -- with the
comparison that says how close it comes. Branch `worktree-agent-a1b2bcaf7e72d785b` from main
`db0f4c5f3`, merged with main `5b8daddbc` (the kit's exact GDI rule: ABC widths on a display
DC) before the final comparison. Design: `docs/extensions/KITFREE.md` §3 (RSCFONT: "a posix
twin later with FreeType").

## 1. Vendoring and licence

- `third_party/freetype`: git submodule, the official mirror https://github.com/freetype/freetype,
  shallow, pinned at tag **VER-2-13-3** (`42608f77f20749dd6ddc9e0536788eaad70ea4b5`), unmodified.
- `third_party/freetype_build/`: OpenBA's build of it (`CMakeLists.txt`, `banished_ftoption.h`,
  `banished_ftmodule.h`, `README.banished.md`): one static C library `freetype_min` from six
  upstream aggregate sources -- base (+ `ftbbox.c`), sfnt, truetype (bytecode interpreter v35 +
  v40), smooth, raster1. No zlib / bzip2 / png / HarfBuzz / brotli (no WOFF), no psnames, no
  autofit, no embedded bitmaps, GX, SVG, colour, Mac forks, and **`FT_CONFIG_OPTION_ENVIRONMENT_PROPERTIES`
  off** (FreeType would otherwise `getenv("FREETYPE_PROPERTIES")` in `FT_Init_FreeType`; the tree's
  rule). Built `-O2 -w` in every flavour like wmadec. Linked into `OpenBA` (Windows and Linux)
  and `unit`. The Windows build stays green.
- **Licence: the FreeType License (FTL).** FreeType is FTL / GPLv2+ dual-licensed; the FTL is
  GPL-compatible and asks only for a credit. **For README.md's licence list** (the coordinator
  adds; README.md is not touched here):

  ```
  ### FreeType

  The run-time font rasteriser uses [FreeType](https://www.freetype.org/) 2.13.3
  (`third_party/freetype`, the FreeType License). Portions of this software are copyright
  (c) The FreeType Project (www.freetype.org). All rights reserved.
  ```

### Size cost

| What | Size |
|---|---|
| `libfreetype_min.a` text (`size`, x86_64 GCC 13 -O2) | **141 KB** (140,888 bytes) (ftbase 46.3, truetype 42.5, sfnt 34.3, raster1 8.9, smooth 8.0, ftbbox 1.5, init/system/debug 1.6) |
| the first cut with psnames + autofit | 253 KB (psnames' Adobe glyph list alone 64.7 KB; both dropped) |
| `OpenBA.exe` debug | see §5 (the final build's figure against main's exe of the same day) |

## 2. What was built

| File | What |
|---|---|
| `src/platform/common/font_raster_freetype.{h,cpp}` | the FreeType rasteriser: `RasterizeFile(path, face, px, codepoints, Options)` to `RasterFont` (font_raster.h's contract), `FamilyName(path)`, `DefaultOptions()`; one process-wide `FT_Library` under a mutex |
| `src/platform/posix/font_raster_posix.cpp` | the posix twin, no longer "absent": `FaceAvailable` / `RasterizeFont` over `FindFaceFile` + `RasterizeFile`; the TTF found **by file name** (`<exe dir>/FrancophilSans.ttf`, then `~/.fonts`, `~/.local/share/fonts`, `/usr/local/share/fonts`, `/usr/share/fonts`, three levels deep), no fontconfig; home from `getpwuid_r`, never `$HOME`; the family name checked (`FrancophilSans`, a wrong file is "absent"); one lookup per process |
| `src/platform/win/font_raster_win.cpp` | + `FreeTypeRaster::FindFaceFile` for Windows (beside the exe, then the font registry's "<face> (TrueType)" value, HKCU full path / HKLM name under the Fonts directory) -- so the comparison test, and a future FreeType-on-Windows switch, find the installed file |
| `tests/unit/ext_font_raster_test.cpp` | + 3 tests (§4); `sources.cmake` + the FreeType source |
| `CMakeLists.txt` | `add_subdirectory(third_party/freetype_build)`, `freetype_min` on OpenBA (both platforms) and unit |

NOT RETAIL, all under `src/platform/` + `third_party/` + tests; no `getenv`, no new flag, no
engine body touched. Nothing in the tree is game content; the TTF stays in the user's
directory / beside the exe.

## 3. The rule FreeType has to reproduce, and how

GDI's contract (font_raster.h, the kit's `UI::Font::Compile`): `CreateFontW(+px)` = CELL height
(tmHeight == px, tmAscent / tmDescent = OS/2 usWinAscent / usWinDescent scaled and rounded),
`GetGlyphOutlineW(GGO_GRAY8_BITMAP)` for the box and the 65-level coverage, `GetCharABCWidthsW`
for pre / post / kern. FreeType sizes the EM, so:

- **Scale**: `s = px * unitsPerEm / (winAscent + winDescent)` (hhea when there is no OS/2),
  `ppem = round(s)` (`Sizing::IntegerPpem`). FrancophilSans: upem 1000, win 917 / 216, so 32 px
  -> 28.24 -> 28 ppem; baseline = round(216 * 28 / 1000) = 6. Every baseline and every advance
  matches GDI at every height tried with this rule (8 / 8, 768 / 768); the fractional and the
  "fitted" ppem variants lose the advances (685 / 768, 722 / 768).
- **Hinting**: the face has no `fpgm` / `prep` / `cvt` -- it is unhinted -- so the interpreter
  version (v35 / v40), the native and the no-hinting paths give the same outline (the three
  rows of the table are identical; the only hinted-path effect, phantom-point rounding, is a
  no-op because `lsb == xMin` for every glyph). The auto-hinter moves everything (339 / 768
  boxes) and was dropped.
- **Coverage**: GDI's GRAY8 is NOT an area. Dumping both: every edge level GDI gives is a
  multiple of 8 / 64 on straight edges (a 3-pixel-wide stem is 48 / 64 / 40 in GDI; the exact
  area says 50 / 64 / 40), i.e. an **8x8 point-sampled supersample** (65 levels = 64 samples
  + 1). FreeType's smooth renderer cannot produce that; `Sampling::Grid8x8` renders it with the
  monochrome `raster1` module: the device-ppem 26.6 outline scaled by 8, scan-converted (a
  subsample is set when its centre is inside, drop-out control off), the set subsamples counted
  per pixel, alpha = count * 4 - 1.
- **Box**: GDI's box leaves out a row / column the outline touches but no subsample centre
  falls in (the 'e' column at 2 / 64 coverage, the 'l' row 0.02 px below the baseline): the
  8x8 bitmap is trimmed to the pixels a subsample hit. Untrimmed: 631 / 768 boxes; trimmed:
  729 / 768.
- **Blank glyph** (the space): GDI's 1x1 empty box at the origin, kern = advance - 1.
- **ABC widths**: FreeType has none. `pre` = the box's left, `post` = the box width; on the
  rows where GDI's abcB is the box less one (RSCFONT's "48 odd rows", ~6 %) FreeType cannot
  agree. Those rows are the bulk of the 45 / 768 metric rows that differ (offsetY alone is
  759 / 768).

## 4. Tests (`tools/build.sh unit --jobs 2`)

`1165 tests, 0 failures, 1 skipped` (the skip is RSCSPRITE's kit cross-check, unrelated). New:

- `ext_font_raster_freetype_rasterises` (every platform; SKIPs when the TTF is not found by
  name): the family name is `FrancophilSans`; 32 px gives height 32, baseline 6, 96 glyphs, the
  space a 1x1 blank box, 'A' a sane box with alpha topping at 255; a wrong face name, a height
  out of range and a missing file are refused; the platform contract (`RasterizeFont`, GDI on
  Windows) agrees on height, baseline and glyph count.
- `ext_font_raster_freetype_vs_gdi` (Windows): 96 glyphs x 8 heights (9, 12, 16, 19, 24, 32,
  48, 64 px) through GDI and through FreeType per option set; the table below is its output.
- `ext_font_raster_freetype_vs_kit_variant`: FreeType (default options) against the kit's
  compiled `.crs` for the five RSCFONT variants, counted (never failed).

### GDI vs FreeType, 768 glyphs (96 x 8 heights), per option set

"bitmaps identical" = same box AND every alpha equal; "pixels differ" over the glyphs whose
boxes have the same size; "rows" = pre / post / kern / offsetY all equal.

| Option set | bitmaps identical | boxes same size | pixels differ | off by one level | max level step (of 64) | rows | advance | offsetY | baseline |
|---|---|---|---|---|---|---|---|---|---|
| **grid8 int-ppem native v40 (DEFAULT)** | **188 / 768** | **729 / 768** | **5484 / 173490 (3.2 %)** | 2773 | 14 (alpha 56) | 723 / 768 | 768 / 768 | 759 / 768 | 8 / 8 |
| grid8 int-ppem native v35 | 188 | 729 | 5484 | 2773 | 14 | 723 | 768 | 759 | 8 |
| grid8 int-ppem unhinted | 188 | 729 | 5484 | 2773 | 14 | 723 | 768 | 759 | 8 |
| grid8 frac-ppem unhinted | 18 | 600 | 28947 / 111795 | 5112 | 64 | 567 | 685 | 752 | 8 |
| grid8 fit-ppem native v40 | 185 | 651 | 5393 / 104442 | 1839 | 56 | 658 | 722 | 756 | 8 |
| area int-ppem native v40 (smooth, quantised) | 8 | 631 | 32232 / 153497 (21 %) | 15447 | 7 (alpha 28) | 583 | 768 | 715 | 8 |
| area int-ppem unhinted | 8 | 631 | 32232 | 15447 | 7 | 583 | 768 | 715 | 8 |
| area frac-ppem unhinted | 8 | 542 | 30215 / 95733 | 6266 | 60 | 484 | 685 | 710 | 8 |
| area fit-ppem native v40 | 8 | 555 | 23344 / 86634 | 11089 | 52 | 527 | 722 | 711 | 8 |

Variants measured and REMOVED from the code (each further from GDI than the default):

| Tried | bitmaps identical | pixels differ | Verdict |
|---|---|---|---|
| auto-hinter, light / normal (autofit) | 12 / 9 | 18547 / 14197 of fewer same-size boxes; 339 / 187 boxes | moves every outline |
| 8x8 grid from an outline loaded at 8x the size (3 more bits) | 88 | 7924 | GDI samples the device-ppem 26.6 outline |
| sample phase +-1/4, +-1/2 subsample in x or y | 16..45 | 22533..34414 | the samples sit at the subsample centres |
| conic chords with the TrueType converter's 2^n rule, tolerance 1/4 subsample .. 4 px | 128 .. 116 | 7048 .. 26491 | not a flattening difference |
| font units -> 26.6 by floor / truncate instead of round | 68 / 75 | 14392 / 14197 | FreeType's rounding is GDI's |
| raster1's drop-out control on / untrimmed box | 188 / 152 | 5484 / 4723 of 631 boxes | no effect / GDI trims |

Per height, the default set:

| px | bitmaps identical | boxes same size | pixels differ | max level step | rows | offsetY |
|---|---|---|---|---|---|---|
| 9 | 42 / 96 | 92 | 124 / 2137 | 10 | 95 | 96 |
| 12 | 26 | 89 | 229 / 3580 | 14 | 86 | 90 |
| 16 | 47 | 93 | 185 / 5727 | 10 | 91 | 96 |
| 19 | 26 | 91 | 252 / 7898 | 8 | 92 | 96 |
| 24 | 24 | 86 | 270 / 11123 | 14 | 90 | 96 |
| 32 | 11 | 94 | 822 / 20393 | 9 | 92 | 96 |
| 48 | 8 | 91 | 1491 / 45316 | 8 | 86 | 94 |
| 64 | 4 | 93 | 2111 / 77316 | 13 | 91 | 95 |

What the residual looks like (32 px dump): the glyph shapes are the same pixels; where they
differ it is one subsample (one level of 64) on a curved edge, FreeType brighter on 698 pixels
to GDI's 124, i.e. GDI's scan converter sets slightly fewer samples on convex curves. A
systematic cause was looked for (the removed variants above) and not found; what is left is
the Windows TrueType scan converter's own curve arithmetic, which is not reproducible without
reimplementing it. The max step of 14 levels is a single pixel at a stem end where one whole
subsample row is in or out.

### FreeType (default) vs the kit's compiled variants

| Variant | line | baseline | rows exact | within one (pre / post) | advance | offsetY |
|---|---|---|---|---|---|---|
| SharpFont_32 | 32 = 32 | 6 = 6 | 92 / 96 | 96 / 96 | 96 / 96 | 96 / 96 |
| Font (24) | 24 = 24 | 5 = 5 | 90 / 96 | 96 / 96 | 96 / 96 | 96 / 96 |
| SharpFontSmall_19 | 19 = 19 | 4 = 4 | 92 / 96 | 96 / 96 | 96 / 96 | 96 / 96 |
| SharpFontMedium_12 | 12 = 12 | 2 = 2 | 86 / 96 | 90 / 96 | 96 / 96 | 90 / 96 |
| SharpFontLarge_64 | 64 = 64 | 12 = 12 | 91 / 96 | 95 / 96 | 96 / 96 | 95 / 96 |

(For scale: the GDI rasteriser is 5472 / 5472 rows exact against the same oracle since main
`5b8daddbc`.) Every line height, baseline and advance is exact; the rows off are the box /
ABC quirk above and, at 12 and 64 px, a handful of glyphs whose box is one row or column
different.

## 5. Build results

- Windows: `tools/build.sh unit --jobs 2` exit 0, 1165 / 0 / 1 (above); `tools/build.sh build
  --jobs 2` exit 0 (debug exe, FreeType linked in; the figure below).
- **Linux: NOT reached.** `wsl -e bash` fails on this box today: `WSL2 is unable to start since
  virtualization is not enabled on this machine` (`HCS_E_HYPERV_NOT_INSTALLED`; the hypervisor
  was switched off for the uProf IBS sessions, memory `reference_uprof_ibs_method`). The posix
  twin and the FreeType source are standard C++ / POSIX (`readlink`, `getpwuid_r`, `opendir`,
  `stat`) and the FreeType sources are the same on both platforms, but they have not been
  compiled or run on Linux here. First thing on the bed once WSL is back:
  `cmake --build build/linux -t unit && build/linux/unit` with `FrancophilSans.ttf` beside
  `unit` (or in `~/.fonts`), expecting `ext_font_raster_freetype_rasterises` to run (not skip)
  and the GDI comparison to skip.

## 6. Recommendation

- **As the posix twin: ship it.** It is the only rasteriser Linux will have; its line heights,
  baselines and advances are GDI's exactly, its boxes agree on 95 % of the glyphs and its
  coverage differs by one level of 64 on 3 % of the pixels -- invisible in a UI sheet, and
  well inside the "within rounding" the user accepted for RSCFONT.
- **Replacing GDI on Windows: not on these numbers, not for correctness reasons.** GDI is now
  5472 / 5472 exact against the kit; FreeType would trade that for ~3 % one-level pixel
  differences and the 6 % of rows where the ABC width is not the box. There is no correctness
  reason to switch; the one reason would be a single rasteriser on both platforms. If that is
  wanted, the switch is mechanical: make `font_raster_win.cpp`'s `RasterizeFont` call
  `FreeTypeRaster::RasterizeFile` over `FindFaceFile` (the lookup is already there); nothing
  else changes.

## 7. NOT done

- The Linux build and run (§5): the bed is unreachable.
- The last 3 % of pixels / 5 % of boxes: no rule found (§4); would need the Windows scan
  converter's curve subdivision, which is not public.
- The ABC-width rows (post = box - 1 on ~6 % of glyphs): FreeType has no ABC widths; not
  reproducible from the outline alone without the realisation rule GDI applies (RSCFONT §1,
  still not recovered).
- `font_dump` (RSCFONT's by-eye harness) still rasterises with GDI only; a `--freetype` switch
  would be a ten-line addition if a by-eye compare is wanted.
- README.md's FreeType credit (§1) is the coordinator's edit.
