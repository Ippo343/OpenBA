# SHARPUI hand-in (2026-10-11)

The second extension: `ext.sharp_ui`, class A (no serialized state, no sim effect), default OFF,
display name "Sharp UI scaling", description "Draw the interface crisp at high UI scales:
pixel-snapped sprites and point-sampled integer upscaling." Spec: `INVENTORY.md` §5.3, amended by
the user's decision relayed mid-block (2026-10-11 night): with the extension on, the EFFECTIVE UI
scale snaps to an integer device-pixel ratio; the slider value stays retail's setting. Static gate
only: the game was NOT run by this block. Everything about how it looks is unverified until the
by-hand run (§9).

**SUPERSEDED 2026-10-12 by section 12 (SHARPUI2):** the user rejected the integer snap ("crisp but
unusable" at 2x). Sections 3-4, 6-7 and 9 describe the removed snap; section 12 is the current
design (no snap, scale-matched fonts).

## 1. Units first (the direction of the slider)

`UserInterface::_uiScale` is **layout units per device pixel**, not a magnification. The layout
space is `(int)(backBuffer * _uiScale)` (`element_common.h:169`, `userinterface_draw.cpp:197-201`)
and `UserInterface::Draw`'s orthographic camera maps it onto the whole back buffer; the mouse is
`raw * _uiScale` (`entityui.cpp:466`). So the **device-pixel ratio is 1 / _uiScale**:

| Options slider | `_uiScale` | ratio (device px per layout unit) | the UI |
|---|---|---|---|
| 75 % | 0.75 | 1.333 | bigger, magnified, linear-filtered: blurry |
| 100 % | 1.00 | 1 | native |
| 150 % | 1.50 | 0.667 | smaller, minified: blurry / thin strokes |
| (retail's 50 %, not on the slider) | 0.50 | **2** | "2x": integer upscaling |

The brief's "200 % option" is therefore the wrong end: retail's 200 % would be ratio 0.5 (an
integer DOWNscale, where point sampling drops every other texel). Integer upscaling is retail's
50 %, i.e. ratio 2. Within retail's 75-150 slider the only integer ratio is 100 % (ratio 1), where
sprites drawn at their size are already exact. That is why (a)+(b) alone help almost nowhere on
retail's slider, and why the user's snap decision is what makes them matter (§4).

## 2. What exists

| File | What |
|---|---|
| `src/ext/sharp_ui/sharp_math.h` / `.cpp` | Pure decisions, standard C++: `WantedRatio`, `FitRatio` (the 800x600 floor), `UIScaleForRatio`, `LayoutExtent`, `ChooseScale` (the effective scale), `ChooseDraw` (integer / snap / point for a frame). |
| `src/ext/sharp_ui/sharp_ui.h` / `.cpp` | The glue: `EffectiveUIScale(percent)` (derivation seam), `OwnsScale()` / `SyncScale(ui)` (the live re-derive), `Draw(ui, ctx)` (the wrapper around retail's `UserInterface::Draw`), the `[SharpUI]` trace and the overflow report. |
| `src/engine/ui/SharpDraw.h` | NOT RETAIL engine end: `UI::SharpDraw {pxPerUnitX/Y, unitsPerPxX/Y, point}`, `SnapToPixel`, `AxisIsPixelExact`, `BatchQuadIsPixelExact`. Standard C++. |
| `src/ext/registry.h` / `.cpp` | `Ext::Extensions::sharp_ui` and the row (Tier::Extension, Class::A, default false, formatVersion 0). |
| `tests/unit/ext_sharp_ui_test.cpp` | 7 tests (§8); `tests/unit/sources.cmake` links `sharp_math.cpp`. |

No hook vtable, no function pointer, no getenv, no `BANISHED_*` flag, no door, no new cast.

## 3. The seams

Outside `src/ext`, each is `if (Ext::Ext().sharp_ui ...) <ours> else <retail's line, unchanged>`
with an `EXT SEAM` comment; the toggle is read once per seam call.

| Site | Line | Ours | Retail's arm |
|---|---|---|---|
| `src/game/Dialog/optionsdialog.cpp`, `ApplyKeyMapToUI` (0x140099260; Startup of the game loop, the menu loop, the splash, RestoreVideoSettings) | 176-181 | `_uiScale = SharpUI::EffectiveUIScale(slider)` | `_uiScale = slider / 100.0f` |
| `src/game/Dialog/optionsdialog.cpp`, `ApplyGameSettings` (0x140072420, the slider's OK) | 835-839 | same | same |
| `src/game/Game/gameupdateloop.cpp`, `GameUpdateLoop::Update` step 9, before `UserInterface::Update` | 1026-1030 | `if (Ext().sharp_ui \|\| SharpUI::OwnsScale()) SharpUI::SyncScale(*ui)` | nothing |
| `src/game/Game/gameupdateloop.cpp`, `GameUpdateLoop::Draw` step 12 | 1502-1508 | `SharpUI::Draw(*ui, *ctx)` | `m_userInterface->Draw(*ctx)` |
| `src/game/Game/menudialogloopt_stubs.cpp`, `MenuDialogLoopT::Update`, before the base's `_ui.Update` | 417-420 | `SyncScale(_ui)` as above | nothing |
| `src/game/Game/menudialogloopt_stubs.cpp`, `MenuDialogLoopT::Draw` | 481-485 | `SharpUI::Draw(_ui, *ctx)` | `_ui.Draw(*ctx)` |

Engine side (DESIGN §3 rule 5: static data, null / false by default, `src/engine` never includes
`src/ext`):

| Site | Line | What |
|---|---|---|
| `src/engine/ui/SpriteContext.h` | end of the class | `static const SharpDraw* _extSharp;` (layout untouched; 0x88 assert stands) |
| `src/engine/ui/spritecontext_flush.cpp`, `SpriteContext::Flush` | 333 (definition), 377-397 (read once per flush; the batch's sampler choice when `point`), 469-478 (the corner snap, inside the axis-aligned arm, after retail's four lines) | |
| `src/engine/video/CompanionData.h` / `companion_data.cpp` | `Video::ImageBufferData` | `static bool _extPointSampling;` (false) |
| `src/render/dx11/imagebufferdata.cpp`, `ImageBufferData::Bind` (slot 4, 0x180002da0) | 617-664 | flag set and not a comparison image -> a POINT sampler with the image's address modes and LOD range (`AcquirePointSampler`), else retail's two lines |

**Faithful-path cost.** Per frame: one bool at the Draw seam, two loads (`sharp_ui ||
OwnsScale()`) at the Update seam. Per `Flush`: one static pointer load and one null test before the
batch; per axis-aligned quad one predictable `sharp != nullptr` branch on the hoisted local (the
rule's allowance). Per texture bind: one static bool load. Nothing else; retail's bodies
(`UserInterface::Draw`, the layout, ComputeStringLayout / ComputeLineLayout, SpriteCapture,
hit-testing) are not touched. With the extension off, every arm is retail's line.

**Not seamed: the splash.** `GameLoadDialogLoopT::Startup` overwrites the scale with
`900 / backBufferHeight` (`splash_loop_stubs.cpp:271`, retail 0x140099012): a full-screen loading
picture laid out at a fixed 900-unit height. Snapping it to an integer ratio would stop the picture
filling the screen (1080p: ratio 1.2 -> 1 leaves a 180-unit gap), a worse look than its blur, and
its draw is the engine's `DialogLoaderLoopT::Draw` (`loops.cpp:905`), which cannot call `src/ext`.
The splash stays retail's. Say if it should not.

## 4. The snap rule, the fit, and the live toggle (the user's decision)

**Rule** (`sharp_math.h`):
`wanted = ceil(100 / slider%)`, `fit = max k >= 1 with backBuffer / k >= 800 x 600`,
`ratio = min(wanted, fit)`, `_uiScale = 1 / ratio`. Over the slider: 75-99 % -> 2x, 100-150 % ->
1x. Why ceil: "nearest integer ratio" maps the whole slider (ratios 0.667..1.333) to 1x, so 2x
would be unreachable; the slider's below-100 half is the player asking for a bigger UI, and 2x is
the next integer up. Above 100 % retail shrinks the UI (ratio < 1); an integer ratio below 1 is a
point-sampled downscale that drops texels, so the effective scale never goes under 1x: with the
extension on, 101-150 % lose their shrink. Ratio 3 (4K, layout 1280x720) is not reachable from the
slider (it would need < 67 %); a "largest fitting ratio" rule could offer it -- a one-line change in
`ChooseScale`, not taken.

**The fit floor, 800x600.** Retail's display-mode list drops every mode narrower than 800 pixels
(`devicedata.cpp:1698`, retail `CMP dword ptr [RBX],0x320`); 800x600 at 100 % is the smallest
layout retail is built to show. The dialogs' own sizes could not be read statically: the `.crs`
element rects in `data0.pkg` are mostly 0 (the dialogs size themselves from their content at run
time, `DialogInstance::ComputeLayout`), so the fit is a floor plus a run-time check, not a measured
table:

| Display | 2x layout | fits the floor | effective at slider 75 % | retail at 75 % |
|---|---|---|---|---|
| 1920x1080 | 960x540 | no (540 < 600) | **1x, clamped** (1920x1080 layout, UI 25 % smaller than retail's 75 %) | 1440x810 |
| 2560x1440 | 1280x720 | yes | 2x | 1920x1080 |
| 3840x2160 | 1920x1080 | yes (3x also fits) | 2x | 2880x1620 |
| 2560x1080 | 1280x540 | no | 1x, clamped | 1920x810 |
| 1600x1200 | 800x600 | yes (exactly) | 2x | 1200x900 |

Note retail itself reaches layouts under the floor (1280x720 at 75 % is 960x540; 800x600 at 75 % is
600x450), so 960x540 is a layout retail's own slider can produce; whether the town hall, the
options and the load dialog fit there is not known from the data. The floor is one constant pair
(`kMinLayoutWidth / Height`). **The run answers it:** with the extension on, every frame checks each
open dialog's laid-out root against the layout space and prints, once per dialog per scale,
`[SharpUI] overflow dialog=<resource> rect=l,t,r,b layout=WxH`. A user run at 2560x1440 (2x,
1280x720) opening the town hall, Options, the save / load dialog and the largest building panels
settles whether 720 is enough; lowering the floor to 960x540 for 1080p is then a measured choice.
Below the fit the ratio is clamped to 1x and the trace says `clamped=1 fit=1x`.

**Live (class A).** `SyncScale` runs before `UserInterface::Update` in both loops while the
extension is on or still owns the scale: on, it writes the effective scale (the slider moved, the
toggle came on, the back buffer changed size); off, it restores retail's `slider / 100` once to the
UI it last set. Retail's `UserInterface::Update` relayouts every dialog every frame from `_uiScale`
(`userinterface_frame.cpp:273-312`), so the relayout is the one the slider's own apply relies on
(ApplyGameSettings writes `_uiScale` and runs one `GameUpdateLoop::Update(0)`). No restart, no reload.

## 5. (a) the snap and (b) point sampling, as built

(a) In `Flush`'s axis-aligned arm, after retail's corner computation, each corner becomes
`round(v * px) / px` in layout units (`px` = backBuffer / layout per axis), i.e. on a whole device
pixel. Rotated quads and lines are left alone. At an exact integer ratio every integer layout
coordinate is already on a pixel, so (a) changes nothing there; it matters for an odd back buffer at
2x (1365 / 682 = 2.0015 px per unit: without the snap, edges drift by up to a pixel across the
screen) and for any non-integer frame (a clamp never produces one, but the code does not assume it).

(b) Samplers are built at image creation (`CreateSamplerState`, retail 0x180002720) and the
`ImageBufferData` layout is pinned (0x38), so the point sampler is not stored: `Bind` asks D3D11 for
the point twin of the image's descriptor each bind (D3D11 returns the existing state object for an
identical descriptor: a lookup, nothing to rebuild after a device loss) and releases its reference
once bound. **The selection is per batch (per Flush), not per frame:** with the frame at ratio
>= 2, a flush point-samples only when every axis-aligned quad is pixel-exact (each texel covers a
whole number >= 1 of device pixels, or the stretch is >= 4, a 9-slice fill); otherwise the image's
own sampler. Reason: at 2x a dialog backdrop letterboxed to the screen (`userinterface_draw.cpp`
backdrop arm) or any picture drawn off-integer would turn blocky under POINT; glyphs, icons and
9-slice frames drawn at their size are pixel-exact and become crisp. When the flag changes, Flush
marks the context's 16 texture slots dirty (`Context::BindTextures` binds only dirty slots);
`SharpUI::Draw` clears the flag on exit and marks them again, so the next scene pass rebinds
retail's samplers.

**The INVENTORY's "UI flag on the ImageBuffer, set where a SpriteSheet / Font loads"** is replaced
by the UI-pass window: every image bound inside a wrapped `UserInterface::Draw` is a UI sheet's
(SpriteContext::Flush's material Setup is the only binder there), and it avoids writing a bit into
retail's `ImageBuffer::_flags`, which is serialized and tested by several masks (`0x70`, bits 6, 8,
9). A comparison (shadow) image is excluded in Bind regardless.

## 6. The trace

stderr (`standalone_error.log`), one line per change, never per frame:

```
[SharpUI] on scale=0.5000 ratio=2.0000 integer=1 snap=1 point=1 slider=75 wanted=2x fit=2x effective=2x clamped=0 bb=2560x1440 layout=1280x720
[SharpUI] on scale=1.0000 ratio=1.0000 integer=1 snap=1 point=0 slider=75 wanted=2x fit=1x effective=1x clamped=1 bb=1920x1080 layout=1920x1080
[SharpUI] off scale=0.7500 slider=75
[SharpUI] overflow dialog=Dialog/TownHall.rsc rect=-12,40,1292,700 layout=1280x720
```

`on` is printed when the extension engages and whenever `_uiScale` or the back buffer changes;
`point=1` means the frame MAY point-sample (each batch still decides, §5); `off` when it hands the
scale back. (The overflow line above is an illustration of the format, not a measurement.)

## 7. Where (a)+(b) help, honestly

- **With the snap (this block):** at 2x (2560x1440 and up, slider < 100 %) text and icons are
  drawn as exact 2x2 texel blocks: as sharp as the sheets allow, at 1x the same as retail's 100 %.
  At 1080p with the 800x600 floor the extension gives 1x: crisp but smaller than retail's 75 %.
- **Without the snap (retail's slider values):** (b) never engages (no integer ratio except 100 %,
  where nothing is gained), and (a) alone only makes the blur consistent from quad to quad (every
  instance of a glyph lands on the same sub-pixel phase); linear filtering at 1.333 or 0.667 still
  blurs. That is why the user's snap decision is the piece that delivers the look.
- **A 200 % / 50 % slider entry** (what a ratio-2 option on the slider would be without this
  extension) is a layout change retail never ran: retail's own slider apply would carry it
  (`ApplyGameSettings`), but it needs a slider range or step change in `optionsdialog.cpp:665`
  (`SetSliderRange(75, 150)`) and the same fit question as §4. Not done.

## 8. Results

- `tools/build.sh build debug --jobs 2`: exit 0 (a full build: the worktree started clean). No
  warning in any new or touched file (`sharp_ui.cpp`, `sharp_math.cpp`, `spritecontext_flush.cpp`,
  `imagebufferdata.cpp`, `companion_data.cpp`, `optionsdialog.cpp`, `gameupdateloop.cpp`,
  `menudialogloopt_stubs.cpp`, `registry.cpp`); the warnings printed are the base's
  (`-Wmisleading-indentation`, `saveloaddialog.cpp` unused, ...): 0 new.
- `tools/build.sh unit debug --jobs 2`: `1030 tests, 0 failures` (7 new `ext_sharp_ui_*`:
  wanted_ratio, fit_ratio, choose_scale, layout_is_exact (1/k gives `(int)(bb * scale) == bb / k` on
  eight common sizes), choose_draw, snap (on-pixel, at most half a pixel moved, monotonic, the odd
  back buffer's right edge), pixel_exact (glyph / icon / backdrop / 9-slice fill / minified)).
- `tools/acceptance/acceptance.sh`: `acceptance: 0 line(s) off target`.
- `python tools/castmetric.py src | tail -1`: `# castmetric total: 2` (no cast added).
- `tools/abi_audit.py`: OK (the 0x88 SpriteContext and 0x38 ImageBufferData asserts stand; the new
  members are statics).

## 9. The by-hand checklist (`tools/build.sh run-menu`, then a new game)

1. Extensions dialog: a row "Sharp UI scaling" under "Eye-level camera", class A, "live", off by
   default; on, OK: `ext.sharp_ui=1` in settings.cfg. The gate
   (`/profile:faithful`) logs `ext.sharp_ui=0 class A` and no `[SharpUI]` line.
2. At 2560x1440 or 4K, Options -> UI scale 75 %, OK. Extension off: retail's 1.333 (blurry). Turn
   it on (live): the UI jumps to 2x (bigger than 75 %), `[SharpUI] on ... effective=2x point=1`.
   Compare text, the toolbar icons, dialog frames: crisp 2x2 texel blocks, no blur, no seams between
   9-slice pieces, no shimmer while a dialog is dragged.
3. Main menu backdrop and the loading picture: they must still look filtered (smooth), not blocky
   (the per-batch rule keeps an off-integer picture linear). Same for any portrait / picture in a
   panel.
4. Open the town hall, Options, the save / load dialog, the largest building panels, the event log:
   any `[SharpUI] overflow` line names a dialog that does not fit 1280x720 (or the layout shown).
5. At 1920x1080: slider 75 % with the extension on gives `effective=1x clamped=1` (the UI smaller
   than retail's 75 %). Judge whether the 800x600 floor should drop to 960x540 (§4) after step 4's
   answer at 2560x1440 with the extension's 2x.
6. Slider 100 / 125 / 150 % with the extension on: always 1x (`wanted=1x`); retail's shrink at
   125-150 % is gone while it is on.
7. Toggle off while in game: `[SharpUI] off scale=0.7500`, the UI back to retail's 75 % at once;
   on again: 2x at once. No restart, no reload, no stuck point sampling in the 3D scene (terrain,
   buildings and the status icons must stay filtered after a toggle off).
8. Change the resolution in Options with the extension on: a new `[SharpUI] on` line with the new
   `bb=`, `fit=`, `effective=`.
9. Mouse: hover / click hit the right element at 2x everywhere (hit-testing follows `_uiScale`).
10. Rotating UI sprites (the loading / autosave spinner): still smooth (rotated quads are not
    snapped and keep linear).
11. Save at 2x, load in the faithful profile: nothing in the save differs (class A; the slider
    value in settings.cfg is the player's, never the effective one).

## 10. Stress comparison (the coordinator fills)

| Profile | simIters/frame or fps | crash / hang log | exit |
|---|---|---|---|
| faithful (`/profile:faithful`, label sharpui2_faithful) | sim 0.17 ms/iter, update p95 2.27 ms, 140 fps | none | 0 |
| everything on (`/profile:all`, label sharpui2_all; 2x at 2560x1377) | sim 0.17 ms/iter, update p95 2.30 ms, 140 fps | none | 0 |

## 11. The (c) plan: sharp text at any scale (NOT implemented)

**What retail's pipeline is.** A `UI::Font` (`Font.h`, 0x90 bytes) is a compiled resource: a
codepoint -> glyph table, 16-byte glyph metrics (`_preAdvance`, `_postAdvance`, `_postKern`,
`_offsetY`), `_lineHeight`, and an `ExternalPtr<SpriteSheet>` whose sprites are the glyph images in
one texture, plus a material. The shipped `Font\Font_resource.crs` names its source: face
`FrancophilSans`, glyphs rasterised by Tools-x64 into `Build\Fontc<NN>.png` and packed into
`Build\FontSheet.rsc` / `Build\uiFontImage.png`; four sizes ship (Font, FontSmall, FontMedium,
FontLarge, each with its own Texture / Material / Sheet). The TTF is not shipped and is not on this
machine (`C:/Windows/Fonts` has no FrancophilSans); Tools-x64.exe is not on this machine, and a loose
text `.rsc` is not loaded by the shipped loader (EXTSCAFFOLD §4).

**The shape that keeps layout untouched.** Layout reads only the retail Font and the retail sheet's
sprite sizes (ComputeStringLayout / ComputeLineLayout / ComputeCharacterPosition, `font.cpp`), and a
glyph quad's size in layout units is the retail sprite's size. So a sharp atlas never replaces the
Font: it is a **twin SpriteSheet** per font sheet, same sprite count and order, whose UVs point into
a texture rasterised at `ratio` x the size, plus its material. Only `SpriteContext::Flush` would
swap, through the same engine-side pointer: when `_extSharp` names a twin for `_currentSheet`, the
UV read (`sheet->m_sprites._data[idx].u0..v1`) and `current->m_field30->Setup` use the twin; the
positions stay the retail sprite sizes. Metrics are never divided because they are never replaced.
The twin is keyed by ratio (1x needs none) and built once per (font, ratio), cached for the session.

**Where the hi-res glyphs come from (three options, costed):**

| Option | How | Cost | Verdict |
|---|---|---|---|
| 1. Runtime rasteriser from the original face | stb_truetype (one header, public domain) rasterises each codepoint of the font's `CharacterSet` at `ratio x` the size and packs a new texture | needs FrancophilSans.ttf on the player's machine (a free font, but not ours to ship, and its version must match the metrics: advances come from retail's tables, so a different hinting moves glyphs within their cells by a pixel); ~400 lines (font load, raster, pack, upload, twin sheet), ~1 block | best quality; blocked on the TTF; risk: glyph shapes / baselines that do not match retail's cells |
| 2. Runtime rasteriser from a substitute face | the same with a font the system has (Segoe UI, DejaVu on Linux) | the same code; look changes (a different typeface in retail's metrics: letters overflow / underfill their advances) | a visible deviation, not a sharpening; no |
| 3. Upscale the existing sheet at load with an edge-aware filter | read the font texture's pixels (`ImageBuffer::CopyToTexture`, `buffers.cpp`), run a pixel-art / signed-distance upscale (e.g. xBR, or build an SDF from the alpha and render glyphs with a smoothstep in a UI pixel shader) at `ratio x`, upload as the twin's texture | ~300 lines CPU + the twin sheet; an SDF needs a new pixel program (a shader the DX11 backend must compile, and its Linux / DXVK twin) | viable without new assets; quality between 2x point and true rasterising; SDF gives smooth edges at any ratio including 1.333 |

Recommended: option 3 with an SDF, because it needs no file retail does not ship and also fixes the
non-integer ratios point sampling cannot. Order: (i) the twin-sheet swap in Flush (engine pointer,
no shader) with a plain 2x point-upscaled texture as the test atlas -- proves the plumbing with
pixels identical to today's 2x point; (ii) the SDF generator (CPU, at font load, cached); (iii) the
SDF pixel program, selected only for twin-sheet batches. Cost: about two blocks.

**Risks for (c).** ComputeStringLayout / ComputeLineLayout must keep reading the retail Font
(layout units) or every text wraps differently: the twin must never be visible to them. Kerning /
`_offsetY` are retail's, so a twin glyph must sit in exactly the retail cell (an SDF of the
retail glyph does by construction; a TTF raster may not). Fonts load through retail's resource
path (`Font_resource.crs`), so the twin is built lazily on first draw, not at load, to stay out of
the loader. Memory: four fonts x ratio^2 x the current texture (the largest is 47 KB compressed);
fine. SpriteCapture records sprite rects in layout units before Flush and is unaffected.

## 11b. Risks and what I did (SHARPUI; superseded in part by section 12)

| Risk (INVENTORY §5.3 and this block's own) | What I did |
|---|---|
| ComputeStringLayout / ComputeLineLayout metrics must stay in layout units | Untouched: nothing outside `Flush`'s vertex positions and the bound sampler changes; the effective scale is a different `_uiScale`, which the layout already handles (the user accepted it as a layout change). |
| Hit-testing uses `UIUnscaled` / `raw * _uiScale` | Untouched; it follows the effective `_uiScale` like the rest of the layout. |
| SpriteCapture and the clip region read `_uiScale` | Untouched; they record / clip in layout units before Flush. |
| A scale outside 75-150 is a layout retail never ran | Accepted by the user for the effective scale; bounded by the 800x600 floor; the overflow trace reports any dialog that does not fit. |
| Point sampling a stretched picture turns it blocky | Per-batch choice (§5): off-integer stretches keep the linear sampler. Heuristic: a picture stretched >= 4x on both axes in one batch with pixel-exact sprites would be point-sampled; none known in the UI. |
| The point sampler leaking into the 3D pass | The flag is static, set only inside a wrapped UI pass, cleared on exit with all 16 slots re-marked dirty. |
| Device loss | Nothing device-scoped is cached (D3D11 dedups the descriptor). |
| The faithful path must be byte-identical | Every seam's else-arm is retail's line; engine statics are null / false outside a wrapped pass. |

## 12. SHARPUI2 (the user's verdict: no snap; scale-matched fonts)

2026-10-12. The user's verdict on the screenshot: the integer snap gave 2x at slider 75%, crisp but
"blows it up to the point of being unusable"; the slider must mean what it always meant. The kit
changed the plan: its compiler RASTERISES a Font resource from an installed typeface at any
`_fontHeight`, so text can be compiled at the pixel height the screen needs. Static gate only: the
game was NOT run by this block (the brief forbids it); how it looks is unverified until 12.8.

### 12.1 Removed

- The scale snap, whole: `SyncScale`, `EffectiveUIScale`, `OwnsScale`, `ChooseScale`,
  `WantedRatio`, `FitRatio`, `UIScaleForRatio`, `ScaleChoice`, the 800x600 fit floor and the
  `[SharpUI] overflow` trace.
- The seams that served it: `optionsdialog.cpp` `ApplyKeyMapToUI` and `ApplyGameSettings` (the file
  is byte-identical to its pre-SHARPUI blob f95608c16: retail's `_uiScale = slider / 100` on every
  path), `GameUpdateLoop::Update` step 9 and `MenuDialogLoopT::Update` (nothing runs before the UI
  update any more). `_uiScale` is never written by this extension.
- Kept: the Draw seams (`GameUpdateLoop::Draw` step 12, `MenuDialogLoopT::Draw`), the pixel snap
  (a) and the point-sampling machinery (b). (b) only engages at an integer ratio >= 2, which the
  75-150% slider cannot reach (ratios 0.667..1.333), so it is inert. Kept rather than removed:
  removing it touches three more files (the DX11 `Bind`, `CompanionData`, `Flush`) for no change
  in behaviour, and each of its arms is one predictable test. Say if it should go.

### 12.2 The font variant set and the sheet sizes

The game's four fonts are the kit's `resource/Font/{Font,FontSmall,FontMedium,FontLarge}.rsc`:
FrancophilSans at 24, 14, 18 and 48 px (`_fontHeight`; it is `UI::Font::_lineHeight`, the second
field `Font::Serialize` reads, after the face name). The slider is continuous (any integer 75..150:
`SetSliderRange(75, 150)`, the value truncated to int), so px/unit is continuous in 0.667..1.333
and "a variant per slider step" is not a finite set. The set chosen: **every pixel height** from
round(H x 2/3) to round(H x 4/3), H itself excluded, with a 2-pixel step for the 48 px font. Why:
a variant is drawn one texel per device pixel at its own size (12.4), so the nearest pick's error
is a glyph up to half a step larger or smaller than the pen spacing the base font lays out: at most
0.5 px for the three small fonts, 1 px of ~48 for FontLarge (2%). A coarse ratio set (1.0 / 1.25 /
1.333 / 1.5 ...) would leave most of the slider with glyphs up to ~10% off their advances: visibly
crowded or gapped words.

| Font | base | variants | count | sheets (`_sheetWidth x _sheetHeight`) |
|---|---|---|---|---|
| Font | 24 | 16..32 | 16 | 128x128: 16; 256x128: 17-22; 256x256: 23, 25-32 |
| FontSmall | 14 | 9..19 | 10 | 128x128: 9-13, 15, 16; 256x128: 17-19 |
| FontMedium | 18 | 12..24 | 12 | 128x128: 12-16; 256x128: 17, 19-22; 256x256: 23, 24 |
| FontLarge | 48 | 32..64, even | 16 | 256x256: 32; 512x256: 34-44; 512x512: 46, 50-64 |

54 sources, `src/ext/ui/Font/Sharp<KitName>_<px>.rsc` (`SharpFont_32.rsc`,
`SharpFontSmall_19.rsc`), written by `tools/ext_fonts.py` (not edited by hand). Sheet: the smallest
of 128x128, 256x128, 256x256, 512x256, 512x512 with >= 64 x px^2 texels (the kit's own sheets hold
the 96 glyphs in 28-57 x px^2; the 19 px variant fills about 55% of 256x128, the 32 px about 60% of
256x256). The rule lives twice, in `sharp_math.cpp` (`VariantLow / High / Step`) and in the
generator; `ext_sharp_ui_variant_set` pins the counts (10 + 12 + 16 + 16).

**What the compiler emits per variant** (one file holds the Font as the unnamed object and its
material and texture as named objects; `Font/X.rsc:material` and `Font/X.rsc:texture` resolve):
`Font/X_resource.crs`, `Font/X_material.crs`, `Font/X_texture.crs` (the packed glyph image is
inside it: 31 KB for SharpFont_32, 74 KB for SharpFontLarge_64) and `Build/XSheet_resource.crs`.
Under `/pathres` it writes the intermediates `Build/Xc<NN>.png` (one per glyph),
`Build/uiXImage.png` and `Build/XSheet.rsc`. The PNGs are NOT deployed (the texture `.crs` carries
the pixels). Deployed loose by `--deploy`: 216 files (54 x 4); 2.1 MB with the dialog's 28.

### 12.3 The typeface finding

- `FrancophilSans` (and its Bold) were installed on this machine by the user mid-block, per-user:
  `%LOCALAPPDATA%/Microsoft/Windows/Fonts/FrancophilSans.ttf`, registered in the HKCU
  `Windows NT/CurrentVersion/Fonts` key as `FrancophilSans (TrueType)`. The kit's README (section
  2) says only "The game uses the font FrancophilSans. You may need it installed depending on what
  you are modding"; its translation example: "whatever font you want, has to be loaded into
  windows". There is no fallback setting: the compiler asks Windows for the face by name and
  **Windows substitutes another face silently**; the compile reports success.
- Evidence of the substitution: the main checkout's `build/ext_resources/` holds the EXTRSC-era
  compile made before the face was installed. Its 24 px Font sheet came out with an EMPTY sprite
  list (`Sprite _sprites [ ]`, a 257-byte image) and its FontSmall / FontMedium images differ from
  today's. Nothing in its logs says so.
- **The proof the compile now uses the game's face:** the `.crs` format was read off the output
  (u32 block count; per block {u32 offset, u32 uncompressed << 16 | compressed}; LZF blocks, the
  engine's `System::Compression::Decompress`) and the game's shipped `Font/Font_resource.crs` was
  decoded out of `data0.pkg`. Our compile of the kit's `Font/Font.rsc` with FrancophilSans
  installed is **byte-identical** to the shipped object from the face name to its end (2453 bytes:
  height, sheet / material / image names, the codepoint table, every glyph's advances and
  offsets). The pre-install compile fails the same comparison. The 32 px variant's sheet was also
  looked at beside the 24 px one: the same letterforms.
- **A machine dependency of the compile step, like the kit itself.** `tools/ext_resources.sh` now
  (1) refuses to compile when no `FrancophilSans (TrueType)` value exists in the HKCU or HKLM Fonts
  key, naming the font; (2) recompiles the kit's `Font/Font.rsc` on every run as the witness and
  runs `tools/ext_font_check.py`, which fails unless the witness's decoded glyph tables hash to the
  shipped object's (sha256 527f3175...), every variant names the face and its own height, and every
  variant sheet holds all 96 glyphs (a sheet too small compiles EMPTY without an error). A failed
  check deletes the variant outputs so they cannot survive as "up to date". Exercised against the
  main checkout's stale compile: `ext_font_check: FAILED -- the kit's Font/Font.rsc compiled to
  glyph tables that are NOT the game's ... Install FrancophilSans ...`, exit 1. **For the main
  checkout:** its next `tools/build.sh deploy` recompiles the witness (the face is installed now)
  and passes; if it does not, delete `build/ext_resources/`.
- A translation mod that replaces `Font/Font.rsc` with another face: `SharpUI` compares the loaded
  base font's face name (`_string20`) with FrancophilSans and keeps the retail draw for that font
  (`Font->retail(face)` in the trace).

### 12.4 The seam and the metric math

Engine side (DESIGN section 3 rule 5; retail bodies unchanged): `UI::SharpDraw` (ui/SharpDraw.h)
gains a table of `SharpSheetSwap {base, twin, twinSprite[], place[] or null, count}`, indexed by
the BASE sprite index. `SpriteContext::Flush` (`spritecontext_flush.cpp`), with `_extSharp` set:

| Where | What |
|---|---|
| once per flush, around retail's material line (375-384) | `swap = FindSwap(*sharp, _currentSheet)` (a scan of a few entries); with a swap the TWIN's material is set up (`swap->twin->m_field30->Setup`), else retail's line |
| the UV read (442-450) | after retail's read: the twin sprite's UVs (`twinSprite[idx]`; < 0 draws nothing) |
| the axis-aligned arm, after the snap (485-505) | with `place`: the glyph quad from `PlaceGlyph` (below) |

The faithful path: `_extSharp` null, so `swap` null; one more null test per flush and one per
record (`swap != nullptr`, a hoisted local). Game side: the two Draw seams call `SharpUI::Draw`,
which (re)selects on a change of `_uiScale`, the back buffer or `ExternalFactory::_generation`
(bumped by every `Clean`, the loop-change eviction that frees what it evicts: every cached pointer
is re-resolved through `FindOrLoad` after one), points `_extSharp` at the frame's table for the
duration of retail's `UserInterface::Draw`, and nulls it. **No retail body was copied**: a glyph's
layout record stays as retail made it; only its draw is redirected, in `Flush`.

**Why layout is identical by construction.** `ComputeStringLayout` / `ComputeLineLayout` /
`ComputeCharacterPosition`, wrapping, the ellipsis, `AddFont`'s culls, hit-testing and
SpriteCapture all read the RETAIL font and the retail sheet's sprite sizes, and all run before
`Flush`. The variant never reaches them; nothing is divided for layout because nothing in layout
changes.

**The metric math (draw only).** The base layout gives each glyph record `x1 = pen + preB` and
`y2 = baseline - offB` (ComputeLineLayout: `penX += _preAdvance + kern`, `_y = baseY - _offsetY -
spriteHeight`; `AddSprite` adds the sprite size). The variant's metrics (`preV`, `offV`, sprite
`wV x hV`) are in its own pixels, which are DEVICE pixels when it is drawn one texel per pixel.
With r = px/unit:

```
left   = round((x1 - preB) * r) + preV          (device px: the base pen, the variant bearing)
bottom = round((y2 + offB) * r) - offV          (the base baseline, the variant descent)
quad   = [left, left + wV) x [bottom - hV, bottom), divided by r back to layout units
```

The variant's metrics are divided by the ACTUAL px/unit (not by `variantHeight / baseHeight`):
that is what makes the quad exactly `wV x hV` device pixels with its corners on whole pixels -- a
texel per pixel, so the linear sampler returns the texel itself: crisp without point sampling. The
base pen positions (kerning included) are kept; only the glyph's own bearing, descent and size come
from the variant. `MakeGlyphPlace` / `PlaceGlyph` are pure and tested (`ext_sharp_ui_glyph_place`:
the 1080p / 75% case lands on 13 x 27 whole pixels; at ratio 1 with the base's own metrics the
quad is retail's quad).

**The pick** (`PickVariantHeight`): the compiled height nearest to `_lineHeight x px/unit`
(px/unit = back-buffer height / layout height); the retail font within half a pixel of it, and
also when the wanted height is more than one step outside the compiled range (never a clamp to the
largest variant); ties to the smaller height. 1080p: slider 75% (1.3333): Font 32, FontSmall 19,
FontMedium 24, FontLarge 64; 100%: all native; 110%: Font 22, FontLarge 44; 150%: Font 16,
FontSmall 9.

### 12.5 The 2x sprite convention (no art shipped)

- **Name:** a sprite sheet whose factory path is `<dir>/<Name>.rsc` (with or without `:resource`)
  has a 2x twin when `<dir>/<Name>2x.rsc` is compiled and deployed LOOSE (`Twin2xPath`; a
  `...2x.rsc` has no twin, a named object `X.rsc:ident` has none). The twin's sprites are matched
  to the base's BY NAME; a twin missing any of the base's sprites is not used (a stderr line says
  which).
- **When:** while the UI is magnified (px/unit > 1.01 on both axes: sliders below 100%). The quad
  stays retail's; only the material and the UVs come from the twin, so 2x art is sampled at
  2 / 1.333 = 1.5 texels per pixel at 75% (more detail than 1x art stretched 1.333x).
- **Lookup:** on each (re)selection `SharpUI` walks the loaded sprite sheets (`FindNext`), checks
  that `<data dir><compiled path of the twin>` exists (`IO::FileSystem::Exists`, so a missing twin
  never reaches the loader), then loads the candidates. A sheet loaded later (a dialog opened after
  the last scale / generation change) is picked up at the next change: a known limit.
- **Authoring 2x art:** copy the kit's sheet source (e.g. `Dialog/SpriteSheet.rsc`) to
  `src/ext/ui/Dialog/SpriteSheet2x.rsc`, double `_imageWidth` / `_imageHeight`, point each
  sprite's `_source` at a PNG twice the size (same `_name`), give it its own `_materialName` /
  `_imageName` and a material / texture pair (named objects in the same file work, as the fonts
  show), double any 9-slice `_border`, add it to `TARGETS` and its outputs to the `--deploy` list
  in `tools/ext_resources.sh`. Nothing in C++ changes.

### 12.6 The trace

stderr, one line when the extension engages (the first wrapped frame after a frame that was not
wrapped: `Video::Context::_frameIndex` skipped) and on every `_uiScale` / back-buffer change:

```
[SharpUI] on scale=0.7500 px/unit=1.3333 fonts: Font->32 FontSmall->19 FontMedium->24 FontLarge->64 2x=0
[SharpUI] on scale=1.0000 px/unit=1.0000 fonts: Font->native FontSmall->native FontMedium->native FontLarge->native 2x=0
```

`->retail(<why>)` = the retail font for that one: `no font`, `face` (a font mod), `no <px>` (the
variant is not deployed), `bad <px>` (loaded but not a FrancophilSans font of that height),
`frame` (a degenerate frame). `->32(-n)` = n base glyphs had no variant glyph (drawn blank; 0
expected: the same CharacterSet). These lines illustrate the format with the unit tests'
arithmetic; they are not from a run.

### 12.7 Results

- `tools/build.sh build debug --jobs 2`: exit 0; 0 warnings in any touched file (the 109 printed
  are the base's: array_destroy.cpp, widget_common.h, citizenai_*, ...).
- `tools/build.sh unit debug --jobs 2`: `1032 tests, 0 failures` (3 snap tests removed:
  wanted_ratio, fit_ratio, choose_scale; 5 new: variant_set, variant_pick, glyph_place, find_swap,
  twin_path; layout_is_exact / choose_draw / snap / pixel_exact kept).
- `tools/acceptance/acceptance.sh`: `acceptance: 0 line(s) off target`.
- `python tools/castmetric.py src | tail -1`: `# castmetric total: 2`.
- `python tools/abi_audit.py`: OK (SpriteContext's 0x88 unchanged; the new members are statics /
  an unpinned helper struct).
- `tools/ext_resources.sh` (first run 53 s; then 7 s with every variant up to date):
  ```
  ext_resources: /build Font/Font.rsc -> exit 0, 0 compiled, 0 problem line(s)
  ext_resources: font variants: 54 source(s), 54 compiled, 0 up to date
  ext_font_check: typeface FrancophilSans = the game's (Font/Font.rsc glyph tables byte-identical to data0.pkg); 54 variants, each 96 glyphs
  ext_resources: 244 file(s), 2144 KB: 28 in Dialog/ (Extensions*.crs), 216 font variant file(s) in Font/ + Build/ (SharpFont*)
  ```
  Every compile log: 0 Warning / Error / Failed / Unable / Assert lines. (The witness is compiled
  by the Dialog/Extensions.rsc build that runs first, hence "0 compiled" on its own line.)
  `--deploy` not run (it writes the game directory; `tools/build.sh deploy` does it).

### 12.8 What the user looks at (`tools/build.sh run-menu`, then a new game)

1. Options -> UI scale 75%, OK, extension OFF: retail's 75% (blurry text). Note a long tooltip's
   line breaks and the town hall table widths.
2. Extensions -> Sharp UI on (live). `standalone_error.log`: `[SharpUI] on scale=0.7500
   px/unit=1.3333 fonts: Font->32 FontSmall->19 FontMedium->24 FontLarge->64 2x=0`. The UI does
   NOT change size: same dialogs, same line breaks, same text widths as step 1; the text is crisp
   (no blur on stems). Icons and frames unchanged (1x art, still filtered; only the pixel snap).
3. Hover / click text buttons and list rows: hits land as with the extension off.
4. An edit field (the save name): the caret sits at the end of the typed text (the caret uses the
   base font's `ComputeCharacterPosition`).
5. Slider 100%: `Font->native ...`, retail's text exactly. 125% / 150%: smaller crisp text
   (`Font->19` / `Font->16`), the same layout as off.
6. Toggle off: retail's text at once, no size change; on again: one new `[SharpUI] on` line.
7. Start a game from the menu (a loop change: the factory `Clean`): text still crisp in game, no
   crash.
8. A font mod (a translation .pkm replacing `Font/Font.rsc`): `Font->retail(face)`, its text drawn
   by retail.

### 12.9 Doubts

- **Glyph vs advance.** The pen comes from the base font (integer layout units, kerning in layout
  units), the glyph from the variant: a glyph can sit up to ~0.5 px off where a font rasterised
  whole at that size would put it, and the nearest pick adds up to half a step of size error.
  Expected invisible; judge at 75% on long text. The alternative (lay out with the variant's
  metrics divided by r) changes layout, which the brief rules out.
  **2026-10-17: it was visible** (the user: the slider changes the letter spacing at every step).
  The per-glyph snap `round(pen * px)` was the cause, not the variant's tables; fixed by word runs
  (SHARP-SPACING.md): the base pen anchors each word, the variant's advances step inside it.
- **Hinting.** GDI hints each height on its own: stem weights differ between neighbouring heights.
  Moving the slider steps the weight, as a natively rendered UI does.
- **Loading inside Draw.** The variants load through `FindOrLoad` at the first wrapped frame of a
  scale (or generation): up to four Fonts with their sheets, materials and textures, on the main
  thread, before retail's `UserInterface::Draw` starts -- a one-frame hitch and a texture upload
  while the frame is open. Not run. If it misbehaves, the selection moves to the update side (a
  seam this block removed).
- **Factory lifetime.** Variants are loaded under the current loop's clean bit; a `Clean` at a
  loop change may evict them, and the generation check re-resolves them before the next wrapped
  draw. A `Clean` in the MIDDLE of a frame would leave stale pointers for that frame; none is known.
- **Linux.** The loose-file check builds `<data dir><compiled path>` with retail's backslash
  delimiters; whether the Linux bed's `Exists` resolves it is unverified (as EXTRSC's loose files).
- **2x twins** are scanned only at a selection, and nothing exercises the path (no art).
- **Deploy size and time.** 2.1 MB loose, 216 files; a fresh `build/ext_resources/` takes ~1 min
  (54 compiler launches); later runs skip up-to-date variants.

### 12.1 First deployed run (coordinator, 2026-10-12): the variants were not found

The trace read `Font->retail(no 32) ...` on every font. `LooseCompiledExists` had two defects:
`FileSystem::Exists` gates the platform stat behind `_platformExistsGate`, which is off during play
(only the packfile gate is set at startup), so a loose file always read as absent; and the name
it built was `<data>/<data>/Font/SharpFont_32_.crs` -- `GetCompiledPath` already returns the full
path under the data directory and wants the `File.rsc:name` form, a top-level resource being
named `resource`. Fixed: `:resource` appended when no name is given, `PlatformExists` on the
returned path. The trace now reads `Font->32 FontSmall->19 FontMedium->24 FontLarge->64` at 75%.

### 12.2 The user's look (2026-10-12): good; the dialog jump is retail's

"Overall looks good at all ui scales and in game the tooltips and render look good." The one
report: on Apply of a scale change the dialogs jump to a corner for an instant. A frame-timing
trace was put in `SharpUI::Draw` for the change frame and the three after it (removed again):

| change | sinceLastDraw (ms) | select (ms) | uiDraw (ms) | next three frames (ms) |
|---|---|---|---|---|
| menu, engage 85% | - | 15.9 | 0.1 | 6.2, 5.2, 5.2 |
| menu 85 -> 75 | 10.9 | 18.6 | 0.1 | 3.0, 6.1, 7.1 |
| menu 75 -> 150 | 11.4 | 6.5 | 0.1 | 2.0, 7.0, 7.0 |
| game 150 -> 75 (cached) | 11.5 | 1.1 | 0.1 | 3.0, 6.0, 6.9 |
| game 75 -> 150 (cached) | 11.6 | 0.3 | 0.1 | 2.0, 7.4, 6.7 |

No frame is stretched, and the user saw the jump with every extension off. It is retail's one
frame of stale layout size in `UserInterface::Update` (`docs/retail_bugs.md` §5), now the fifth fix
`fix.ui_scale_layout`. §12's "loading inside Draw" doubt is answered: up to 20 ms on the first
load of a scale, under 2 ms once the variants are cached by the factory.

## 13. Moving dialogs shimmered (fixed 2026-10-14)

**Report (the user):** with ext.sharp_ui on, the text of an open panel (a citizen's) shimmered while
the camera rotated.

**Cause.** An entity panel follows its entity on screen, so turning the camera (or the citizen
walking) moves the dialog every frame by whole layout units. The snap rounded every coordinate on
its own, `round(v * px)`. At a non-integer ratio (89 %: 2560 / 2278 = 1.1238 px per unit) that is not
translation-invariant: moving the dialog one unit re-rounds each glyph differently, and the gaps
between glyphs and rows change by a pixel from frame to frame. Retail draws the same text filtered
at fractional positions, so it slides smoothly instead.

**Fix.** Each dialog's quads are rounded against the dialog's own upper-left (`UI::SnapDevice` in
`src/engine/ui/SharpDraw.h`: `round((v - o) * px) + round(o * px)`). `UserInterface::Draw` sets the
origin per dialog (the root element's layout left / top) through the `_extSharp` seam and back to 0
for the fade and the cursor; pending quads of another origin are flushed first. A dialog now moves as
one picture in whole-pixel steps. Null `_extSharp` (the faithful profile) is untouched.

**Measured** (the user's settings, render thread on, a held citizen panel, the rotate key held,
consecutive screen captures aligned on the panel; bright text pixels changed against the first
frame): before 143 to 16,930 per frame; after 0 in all 15 frames while the panel travelled about
550 px. Unit test `ext_sharp_ui_snap_moves_whole` pins the invariance, with the old rounding as the
control.
