# SHARP-SPACING hand-in (2026-10-17)

ext.sharp_ui: letter spacing that changed at every step of the Options UI-scale slider.
Branch `worktree-agent-a44e6e8d26aba3553`, on main `d15c1560c`. No new row, no new flag; the fix is
inside the existing `ext.sharp_ui` draw (`src/engine/ui/SharpDraw.h`, `spritecontext_flush.cpp`,
`src/ext/sharp_ui/`). SHARPUI.md section 12 has the extension's design; this note changes only how a
glyph's device pen is chosen.

## 1. The problem

The user (2026-10-17): with ext.sharp_ui on, dragging the UI-scale slider changes the text's letter
spacing at every step; with it off it does not. Retail scales one 24 px bitmap font, so every gap
scales with the slider. The lead's first reading: inherent to the design, because each kit-compiled
variant carries its own hinted advances.

## 2. What the code actually did (the variant tables are NOT the cause)

Layout never sees a variant: `ComputeLineLayout` lays out every string with the BASE font, and only
`SpriteContext::Flush` swaps the glyph image (SHARPUI.md 12.4). So design (a) of the brief -- "lay out
with the base font's advances scaled, draw the variant's bitmaps" -- was already what shipped. The
advances the variants carry were never used for anything.

What moved the letters was the snap. Each glyph's device pen was `round(pen * px)` on its own
(`PlaceGlyph`). At a fractional ratio two neighbouring pens round independently, so the gap between
two letters is the variant's own hinted gap plus or minus a pixel, at random along the line, and the
pattern of those stray pixels is a function of `px`: it reshuffles at every slider step. That is the
"spacing changes at every step" the user saw. The same snap also hit the NATIVE band (the pick within
half a pixel of the font's own height, 98-102 % for Font, 96-104 % for FontSmall): there no variant
was installed and retail's quad was drawn with its four corners snapped, which stretches a 13-texel
glyph into 13 or 14 pixels -- resampled AND unevenly placed.

## 3. The advance-table evidence

Decoded from the compiled fonts (`build/ext_resources/WinData/Font/*_resource.crs`, the LZF `.crs`
decoder of `tools/ext_font_check.py`; the glyph table is the last `n x (pre, kern, post, offY)` rows
before baseline / ellipsis / space, the ink sizes from `res/Build/*Sheet.rsc`). Script:
`scratchpad/sharp-spacing/advances.py` (session scratch; the numbers are reproduced here).

Font (24 px, 96 glyphs, 49 with a non-zero `_postKern`): per glyph, the variant's advance minus the
base advance times `h / 24`, over ASCII 33..126:

| variant px | r | mean | sd | max | ink width mean d | free-run drift of a 43-glyph line (px) |
|---|---|---|---|---|---|---|
| 16 | 0.667 | -0.02 | 0.37 | 0.67 | +0.22 | -5.0 |
| 19 | 0.792 | +0.20 | 0.29 | 0.88 | +0.33 | +6.1 |
| 21 | 0.875 | +0.30 | 0.36 | 1.12 | +0.32 | +10.9 |
| 23 | 0.958 | -0.10 | 0.47 | 0.83 | -0.02 | -12.4 |
| 26 | 1.083 | +0.09 | 0.33 | 0.83 | -0.04 | +1.8 |
| 29 | 1.208 | +0.30 | 0.50 | 1.46 | +0.03 | +5.9 |
| 32 | 1.333 | +0.01 | 0.50 | 1.00 | -0.40 | -11.0 |

FontSmall (14 px): the same shape, means +0.02..+0.45, sd 0.28..0.51, max up to 1.5 px (variant 19).
So: the hinter rounds each size's advances and stems on its own (sd about a rounding's 0.3-0.5 px),
and some sizes are systematically wider (+0.3 px per glyph at 21 px) or narrower in ink (-0.4 px at
30-32 px). A line drawn FREELY with a variant's advances drifts up to 12 px from retail's width over
43 glyphs: that is why the variant advances cannot simply replace the layout (design (c) and the
"lay out with the variant" alternative are out), and why the brief's (c) -- rewriting the variant's
tables to base x r -- would change nothing: those tables are not read.

Simulated per-glyph letter gaps for two lines ("User Interface Scale", "The quick brown fox...") at
every slider step 75..150 (the same script), device pixels:

| measure (average over the steps) | Font, old snap | Font, word runs | FontSmall, old | FontSmall, runs |
|---|---|---|---|---|
| gap minus the variant's own hinted gap, sd (positional unevenness) | 0.49 | 0.28 | 0.52 | 0.27 |
| gap minus retail's fractional gap, sd | 0.59 | 0.54 | 0.53 | 0.50 |
| native band (98-102 %), gap minus hinted, sd | 0.27-0.40 | 0.00-0.02 | -- | -- |

The first row is the stray-pixel randomness the fix removes; the second is what remains against
retail, and it is the hinter's (different ink widths per size), which no draw-side choice can
restore: the lead's "inherent" holds for that part, with the number attached.

## 4. The design chosen: word runs, anchored on the base pen

`UI::SharpRun` / `SharpRunPen` (`src/engine/ui/SharpDraw.h`; standard C++, unit-tested). In Flush,
for the placed glyphs of one batch:

* the first glyph of a word snaps its base pen as before (`SnapDevice`, against the dialog origin);
* each following glyph's device pen is the previous one plus the previous glyph's VARIANT advance
  (`_preAdvance + _postAdvance + _postKern`, now carried in `SharpGlyphPlace`), as long as the base
  layout is contiguous: the next base pen is exactly the last pen plus its base advance, the same
  baseline, no space on either side;
* the run is held within `kSharpRunTolerance` = 1 device pixel of the base pen's ideal (fractional)
  position: past it the pen is nudged by whole pixels. So a word is never more than a pixel wider or
  narrower than retail's, and the next word, which snaps its own base pen, lands where retail puts it;
* a space, a baseline change, a pen the layout did not step to (a new string, the label's shadow
  pass, which is a second AddFont at +1,+1), a rotated quad or a record without a place ends the run.

Plus the native pick is now the font swapped for ITSELF (`SelectFont`): its glyphs go one texel per
pixel through the same run, instead of retail's quad with snapped corners. At ratio 1 the self-swap
reproduces retail's quad exactly (unit test), so 100 % is unchanged.

The faithful profile is untouched: `_extSharp` null leaves `SharpRun` unused (one stack object per
flush). `tools/acceptance/acceptance.sh`: 0 lines off target.

Rejected:
* (b) rasterise with advance-preserving hinting: the kit's Font resource has no such field
  (`ToolKit/resource/Font/Font.rsc`: face, height, sheet, names), and nothing in `Tools-x64.exe` /
  `CompileWin-x64-profile.dll` strings names a hinting or antialiasing switch; a runtime rasteriser
  needs the TTF (SHARPUI.md 11).
* (c) overwrite the variants' advance tables: they are never read; no effect.
* lay out with the variant's advances: changes layout (wrapping, widths, hit-testing) and drifts up to
  12 px per line against retail (the table above).
* draw the variant at the fractional pen: filtered = blurred, the thing the extension removes.
* a larger tolerance (free word runs): a 9-letter word at 123 % would eat 3.9 px of a 4 px space.

## 5. Measured before / after (the deployed exe, 2560 x 1377 window, the user's settings.cfg with
`ext.sharp_ui=1`; `GameUIScale` set per run, `BANISHED_AUTOMENU_BUTTON=4` opens Options on the
Enhanced tab; `tools/capture_window.ps1`; the label column measured by bright-column profile,
`scratchpad/sharp-spacing/measure.py`; the user's settings.cfg restored after)

| scale | row | width before / after (px) | what the 4x zoom shows |
|---|---|---|---|
| 117 % (FontSmall 12) | "Max shadow resolution" | 84 / 84 | before: "resol ution" (a stray 2 px gap); after: even |
| 117 % | "Seamless ambient loops" | 90 / 90 | before: "am bient"; after: even |
| 99 % (native) | "Max shadow resolution" | 100 / 99 | before: resampled glyphs, "resolu tion"; after: one texel per pixel, even |
| 99 % | "Seamless ambient loops" | 107 / 106 | before: the 't' and 'l' of "ambient loops" touch; after: even |
| 84 % (FontSmall 17) | all three rows | 31/30, 116/117, 125/125 | both even at this step (its pens happened to round well); after no worse |

Row widths are retail's within a pixel at every step (the tolerance at work). Trace lines of the
runs: `[SharpUI] on scale=1.1700 px/unit=0.8547 fonts: Font->21 FontSmall->12 FontMedium->15
FontLarge->42`, `scale=0.9900 ... all native`, `scale=0.8400 ... Font->29 FontSmall->17 ...`; no
crash (standalone_crash.log empty), every run killed by PID after its capture.

Unit: `tools/build.sh unit --jobs 2` 1152 tests, 0 failures; new `ext_sharp_ui_word_run` (the hinted
step every time where the old rounding stepped 14,14,14,14,14,15; the nudge at 1.83 px ahead; the
breaks; ratio 1 exact; translation against a dialog origin), `ext_sharp_ui_glyph_place` extended.

## 6. What is left

* The user's look: the slider dragged live with the fix (this hand-in's captures are three fixed
  steps). The remaining per-step change is the hinter's ink widths (section 3, row 2) and shows as
  a whole word looking a touch tighter or looser between steps, not as uneven letters.
* The shadow pass: its run starts from its own snapped pen (+1,+1 layout units), so its offset from
  the text is 1 or 2 px per WORD now (it was per glyph). Not measured.
* FontLarge (2 px variant steps) and the Edit / DocumentText elements: the same code path, not
  looked at separately.
* `kSharpRunTolerance` = 1.0 is reasoned (word width within a pixel of retail's), not tuned.
