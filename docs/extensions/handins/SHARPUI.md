# SHARPUI hand-in (2026-10-11)

The second extension: `ext.sharp_ui`, class A (no serialized state, no sim effect), default OFF,
display name "Sharp UI scaling", description "Draw the interface crisp at high UI scales:
pixel-snapped sprites and point-sampled integer upscaling." Spec: `INVENTORY.md` §5.3, amended by
the user's decision relayed mid-block (2026-10-11 night): with the extension on, the EFFECTIVE UI
scale snaps to an integer device-pixel ratio; the slider value stays retail's setting. Static gate
only: the game was NOT run by this block. Everything about how it looks is unverified until the
by-hand run (§9).

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
| faithful (`/profile:faithful`) | | | |
| everything on (`/profile:all`) | | | |

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

## 12. Risks and what I did

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
