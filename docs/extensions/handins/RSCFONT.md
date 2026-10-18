# RSCFONT hand-in (KITFREE, 2026-10-17)

ext.sharp_ui's font variants `Font/Sharp<Kit>_<px>.rsc` rasterised inside the exe, on demand,
from the typeface FrancophilSans -- no loose `Font/SharpFont*` files, no mod kit. Branch
`worktree-agent-a25e03cb2a419cb70` from main `553d29afa`. Design: `docs/extensions/KITFREE.md`
§1 (the typeface ruling), §3 (this block).

## 1. What the kit did (found, not assumed)

The kit's rasteriser is NOT in `Tools-x64.exe` or `VideoCompiler-x64-profile.dll` (their GDI32
imports are `SetPixelFormat` / `ChoosePixelFormat` only): it is `UI::Font::Compile` in the
PROFILE Runtime DLL (`Runtime-x64-profile.dll` is the one kit binary naming
`GetGlyphOutlineW`; retail's `Font::Compile` is the `MOV EAX,1` stub font.cpp ports). Read back
from the kit's output (`build/ext_resources/WinData/Font/*.crs` decoded with
`tools/ext_font_check.py`'s format; the per-glyph `res/Build/Sharp*c<cp>.png` intermediates):

| Kit fact | Evidence |
|---|---|
| `CreateFontIndirect` with `lfHeight = +px` (CELL height) | at 32 px GDI gives tmHeight 32, ascent 26, descent 6; the kit's `_lineHeight` is 32 and `_baseLine` 6 (`-32` would give tmHeight 36 / descent 7, wrong) |
| `_baseLine` = `tmDescent` | 24 -> 5, 32 -> 6, 64 -> 12, 9 -> 2, all equal |
| `GetGlyphOutlineW(GGO_GRAY8_BITMAP, identity MAT2)` is the bitmap | every glyph PNG is gmBlackBoxX x gmBlackBoxY (0 size mismatches over 9 heights x 96 glyphs) and PIXEL-IDENTICAL with alpha = level * 4 - 1 (level 1..64; the kit's alpha set is {0, 3, 7, 11, ... 255}), RGB white |
| the space is GDI's own 1x1 empty box | `bytes = 0`, box 1x1, origin 0: the kit's `c32` sprite is 1x1 transparent with row (0, 5, 1) at 32 px = (orgX, inc - 1, bbX) |
| `_offsetY = gmptGlyphOrigin.y - gmBlackBoxY` | 768 / 768 rows exact |
| `_preAdvance = gmptGlyphOrigin.x`, `_postAdvance = gmBlackBoxX`, `_postKern = gmCellIncX - pre - post` | 720 / 768 rows exact; the other 48 (6.3 %) have one pixel moved between pre / post and kern, the advance (`pre + kern + post`) always equal |
| `_ellipsisWidth` / `_spaceWidth` | `MeasureLiteral`'s walk (font.cpp) over the rows: 15 / 6 at 32 px, 12 / 5 at 24: equal |
| the codepoint table | 170 entries, -1 unmapped, 96 glyphs = ASCII 32..126 + U+00A9 (`Font/CharacterSet.rsc`); glyph i is the sheet's sprite i named `c<cp>` |
| the names the Font carries | `_string20` face, `_string80` `Build/Sharp<Kit>_<px>Sheet.rsc`, `_field70/_field74` sheet w/h, `_string88` `Font/Sharp<Kit>_<px>.rsc:material`, `_string78` `Build/uiSharp<Kit>_<px>Image.png`; refs charset / sheet / material |

The 48 odd rows: for those glyphs the kit's `post` is `gmBlackBoxX - 1` and `pre` is the origin
or the origin + 1, i.e. one faint edge column of the GRAY8 box left out of the metrics (never
out of the bitmap). It is NOT a threshold on the GRAY8 column (columns of max level 8 are
trimmed while columns of level 4 are kept), not the GRAY4 / GRAY2 / BITMAP / METRICS box
(each fits a different subset: GRAY4 717 / 768, GRAY2 572, METRICS 290), not unhinted, not the
ABC widths (4 / 768). Fitting scripts and the per-column dump are in this session's scratchpad
(`scratchpad/rscfont/fit.py`, `fit2.py`, `edges.py`); the Ghidra read of the profile DLL's
`Font::Compile` would settle it and is the one open thread (§7). The effect of a row off by one:
the variant glyph is drawn one device pixel left or right of where the kit's would be, with the
same advance; below the rounding the user accepted ("within rounding", KITFREE.md §5).
`CompareVariant` in the unit test pins exactly this: offsetY and advance exact, pre / post / kern
within one.

## 2. What was built

| File | What |
|---|---|
| `src/ext/resources/font_raster.h` | the rasteriser contract and the measured rule (standard C++) |
| `src/platform/win/font_raster_win.cpp` | GDI: face lookup installed -> `<exe dir>/FrancophilSans.ttf` via `AddFontResourceExW(FR_PRIVATE)` once per process, `GetTextFaceW` verified against the name asked (GDI substitutes silently; a substitute is "absent"); `GetGlyphOutlineW(GGO_GRAY8)` per codepoint, DWORD-padded rows unpacked, level -> alpha; one mutex (GDI objects are per thread, the private-load state is shared) |
| `src/platform/posix/font_raster_posix.cpp` | the twin: "absent" (FreeType later) |
| `src/ext/resources/font_tables.{h,cpp}` | pure: the variant path grammar (`Font/Sharp<Kit>_<px>.rsc[:resource|material|texture]`, `Build/Sharp<Kit>_<px>Sheet.rsc`), the kit's names, `c<cp>` -> codepoint, `MeasureRun` (font.cpp's walk), the kit's sheet size rule |
| `src/ext/resources/font_sheet.h` + `font_sheet_stub.cpp` | the sheet-builder interface the provider calls (§4); the stub answers "no builder" |
| `src/ext/resources/font_provider.{h,cpp}` | the provider: prefixes `Font/SharpFont` and `Build/SharpFont`; base font FindOrLoaded as sharp_ui does, face checked on `_string20`, codepoints from the base sheet's sprite names in sprite order, glyphs cached per (Kit, px), the four objects built (sheet / material / texture by the builder, which registers them; the `UI::Font` filled field for field in Serialize's order and returned with its path), idempotent (an already-registered part is returned), every outcome one `[ExtRes]` line, timed |
| `src/ext/resources/resources_weak_stub.cpp` | TEMPORARY: weak `Register` / `Provide` / `SetPath` / `Publish` so this branch links before RSCTEXT's `resources.cpp`; **the coordinator deletes it at merge** (RSCTEXT confirmed its strong definitions of all four) |
| `src/ext/registry_boot.cpp` | `Ext::Resources::InitFonts()` after `Resolve` (one line + include) |
| `src/diag/kit_font_reader.h` | header-only: the `.crs` container (LZF) + the Font record, for the test and the harness |
| `src/diag/font_dump/font_dump.cpp`, CMake target `font_dump` | the by-eye harness (§5) |
| `tests/unit/ext_font_raster_test.cpp` (+ `sources.cmake`, `gdi32` on `unit` and `OpenBA`) | 3 tests (§5) |
| `tools/release_package.sh`, `tools/release/FrancophilSans_README.txt` | the release package (§6) |

Not retail, all under `src/ext/` + the two platform twins + diag; no `getenv`, no new flag, no
engine body touched, no door. Nothing in the tree is game content: the `.crs`, the glyph PNGs
and the TTF stay in `build/` / the user's directories / the release package.

## 3. The provider's run-time shape

`FindOrLoad("Font/SharpFont_32.rsc")` misses on disk -> RSCTEXT's seam -> `Provide` ->
`ProvideFont("Font/SharpFont_32.rsc:resource")`:

1. `FaceAvailable("FrancophilSans")` once per process (installed, else the private TTF beside the
   exe); absent = one `[ExtRes] font variants off: typeface FrancophilSans absent (<why>)` line,
   null forever, sharp_ui's `retail(no 32)` stands. Never another face.
2. the base `Font/Font.rsc` (`FindOrLoadAs<UI::Font>`); `_string20` must be FrancophilSans (a
   translation mod's font -> null, sharp_ui's `retail(face)`); its sheet's 96 sprite names
   `c<cp>` are the codepoints, in sprite order, so variant glyph i == base glyph i and the base's
   `_charToGlyph` is copied verbatim (sharp_ui's twin mapping by codepoint cannot miss);
3. `RasterizeFont` at 32 px (cached per (Kit, px) for the process: a rebuild after
   `ExternalFactory::Clean` reuses the bitmaps);
4. `BuildFontSheet` (§4) -> sheet + material + texture, registered under
   `Build/SharpFont_32Sheet.rsc:resource`, `Font/SharpFont_32.rsc:material`, `:texture`;
5. the `UI::Font` (`InternalMemAlloc` + placement new, as the exe's own builders do; `Font::operator
   delete` is `InternalMemFree`): face, `_lineHeight` 32, the kit's names, `_field28` = the base's
   CharacterSet, `_charToGlyph`, `_glyphs` rows, `_baseLine`, `_ellipsisWidth` / `_spaceWidth` by
   `MeasureRun`; `SetPath("Font/SharpFont_32.rsc:resource")`; returned, the seam registers it.

Asked for `:material` / `:texture` / the sheet first: the whole variant is built, the Font
`Publish`ed, the asked part returned (an `Add` of an already-registered object is a hit that
only sets the usage bit, external_add.cpp). Only the heights asked for are built -- four per
scale change, never the 54 the kit compiled. `_refs` stays empty (the kit's source-file
FILETIMEs; nothing reads it but Load / SaveText); the path stack is untouched (RSCTEXT's
answers).

**First-build cost** (this box, measured by the harness / the unit test, GDI alone): 96 glyphs at
32 px rasterise in **~1 ms** (the unit test rasterises five variants with a kit decode each in
well under 100 ms); the four-font set at one scale is therefore a few ms before the sheet
build. The sheet / texture half is RSCSPRITE's; the provider's `[ExtRes] font ... provided ...
first build, <ms>` line times the whole and will say.

## 4. The sheet interface RSCSPRITE's builder must honour (`font_sheet.h`)

`BuildFontSheet(FontSheetInput, FontSheetOutput&)`: N coverage bitmaps (`widths[i]` x
`heights[i]` bytes, top row first) with names `c<cp>`; a suggested sheet size (the kit's rule:
smallest of 128x128 .. 512x512 holding 64 * px^2); the three paths; the base material's path
(`Font/<Kit>Material.rsc`). The contract: sprite i IS bitmap i, exactly `widths[i]` x
`heights[i]` (a 1x1 fully transparent sprite is legal -- the space), named `names[i]`, padded
for filtering the kit's way (`_padForFiltering`, a transparent ring); texels white with alpha =
the byte; the material derives from the base `Graphics::MaterialInstance` by reference (its
`Material`, parameters, constants; the `diffuse` texture replaced by ours) and is the sheet's
`m_field30`; the three objects `SetPath` + `Publish`ed in dependency order (texture, material,
sheet); `sheetWidth` / `sheetHeight` report what was built (they go into `UI::Font::_field70 /
_field74`). Swap: implement `BuildFontSheet` over `sheet_builder.h` and delete
`font_sheet_stub.cpp`.

## 5. Tests

- `tools/build.sh unit --jobs 2`: **1155 tests, 0 failures, 0 skipped** (3 new).
  - `ext_font_variant_path_grammar`: the path grammar, the names, `c<cp>`, `MeasureRun` against
    the kit's 15 / 6, the sheet rule.
  - `ext_font_raster_matches_kit_variant`: `SharpFont_32`, `Font` (24), `SharpFontSmall_19`,
    `SharpFontMedium_12`, `SharpFontLarge_64` from `build/ext_resources/WinData/Font/` (the main
    checkout's, found from the worktree; SKIPs when absent or the face is absent): same face,
    height, baseline, glyph count, codepoints; every row's offsetY and advance exact, pre / post /
    kern within one; ellipsis and space widths exact. Reports `32 rows one pixel off between
    pre/post and kern` over the five (480 rows).
  - `ext_font_raster_rejects_absent_face`: an unknown face is refused (no substitution), a
    height out of range is refused.
- `font_dump` (`cmake --build build/cmake-debug -t font_dump`;
  `build/cmake-debug/font_dump.exe Font 32 <main>/build/ext_resources/WinData <out> <main>/build/ext_resources/res/Build`):
  prints the comparison (`Font at 32 px: ... 90 exact, 6 one pixel moved ..., 0 other; ours
  ellipsis 15 space 6`) and writes `ours_SharpFont_32.png` (our glyphs, shelf-packed in
  codepoint order) beside `kit_SharpFont_32.png` (the kit's packed sheet) for the by-eye
  compare: the glyphs are the same pixels (the kit packs by height). Scratchpad composites on
  black are in `C:/Users/USPC/AppData/Local/Temp/rscfont_out/{ours,kit}_view.png`.
- `tools/build.sh build --jobs 2`: see the final report (the exe links with the weak stub; the
  provider is compiled in; `[ExtRes]` lines appear only when a load misses, which needs
  RSCTEXT's seam).

### Metrics comparison, kit vs ours (the unit test's five variants)

| Variant | line | baseline | glyphs | ellipsis / space | offsetY | advance | rows with pre/post one off |
|---|---|---|---|---|---|---|---|
| SharpFont_32 | 32 = 32 | 6 = 6 | 96 = 96 | 15 / 6 = | 96 / 96 | 96 / 96 | 6 (&, 0, 7, W, k, v) |
| Font (24) | 24 = 24 | 5 = 5 | 96 = 96 | 12 / 5 = | 96 / 96 | 96 / 96 | 9 |
| SharpFontSmall_19 | 19 = 19 | 4 = 4 | 96 = 96 | = | 96 / 96 | 96 / 96 | 6 |
| SharpFontMedium_12 | 12 = 12 | 2 = 2 | 96 = 96 | = | 96 / 96 | 96 / 96 | 8 |
| SharpFontLarge_64 | 64 = 64 | 12 = 12 | 96 = 96 | = | 96 / 96 | 96 / 96 | 9 (one of them, '3' at 24, is the cross-machine case: the kit's 2014 row fits no GDI box on this box) |

Bitmaps: 0 size mismatches and pixel-identical where compared (`A`, `0` at 32 px, and the whole
sheet by eye).

## 6. The release package and the README lines

`tools/release_package.sh [--fonts <dir>] [--no-build] [flavour]` -> `build/release/{OpenBA.exe,
FrancophilSans.ttf, FrancophilSans-Bold.ttf, FrancophilSans_README.txt, LICENSE, README.md}` and
`OpenBA-<git describe>.zip`; refuses a `.crs` / `.pkg` / `.pkm` in the package. The fonts come
from the user's directory (`C:/Users/USPC/Downloads/francophil_sans/`, or `--fonts`), never the
tree. `tools/release/FrancophilSans_README.txt` is ours: author Manfred Klein (1932-2018), the
TTF name table's copyright line `Copyright (c) M Klein` (version 1.0, 2005-03-24, read from both
files' `name` tables), his Simple Terms of Use summarised (free for private and commercial use,
a donation to Doctors Without Borders asked when there is profit, redistribution only with the
font files as they are and not in font compilations for sale), and the statement that the
typeface is not part of OpenBA's GPL code.

**For README.md's licence section** (the coordinator adds; README.md is not touched here):

```
### Typeface

The user-interface typeface is **FrancophilSans** by Manfred Klein (1932-2018), used under
his "Simple Terms of Use": free for private and commercial use, with a donation to
[Doctors Without Borders](https://www.msf.org/) asked for when a use makes a profit -- if
OpenBA earns you anything, please honour that. The two `.ttf` files ship beside `OpenBA.exe`
in the release package only (with `FrancophilSans_README.txt`, Klein's terms), are loaded
privately by the exe when the face is not installed, and are not part of OpenBA's code or of
its GPL-3.0 licence. Without them the Sharp UI extension falls back to the game's own glyphs.
```

## 7. NOT done, and why

- **The in-game end test** (§5 of the brief): needs RSCTEXT's load-miss seam, which this branch
  does not have (the weak `Provide` is never called by anything), AND RSCSPRITE's builder (the
  stub returns false, so a provided variant has no sheet and the provider answers null). With
  both merged the test is: `tools/build.sh deploy`, move `WinData/Font/SharpFont*` and
  `WinData/Build/SharpFont*` to `_kitfree_off/`, run with `ext.sharp_ui=1` at a non-native
  scale, expect `[ExtRes] font SharpFont_32 provided: 96 glyphs ...` and `[SharpUI] ...
  fonts: Font->32 ...` (not `retail(no 32)`), screenshot, move back. Tested here through the
  harness and the unit test instead.
- **The 48 odd rows** (§1): the kit's exact pre/post rule is not recovered; a Ghidra read of
  `Runtime-x64-profile.dll`'s `UI::Font::Compile` (the profile DLL is not in the Ghidra project)
  would give it. Below the rounding ruled acceptable; the unit test pins the bound.
- **Linux**: the posix twin answers "absent" (KITFREE.md §3: FreeType later); the unit test
  SKIPs there; `font_dump` is Windows-only.
- **The weak stub** must be deleted at merge (RSCTEXT's `resources.cpp` defines the four with
  strong linkage; two strong definitions would not arise, but the file is dead code then).
- The TTF ships Regular and Bold; only Regular is rasterised (the game's fonts are all Regular).
  Bold rides along because Klein's terms ask for the font files to be passed on together.
