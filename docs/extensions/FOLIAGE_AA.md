# FOLIAGE_AA -- `ext.foliage_aa` (class A): smooth alpha-tested edges (leaves, needles, twigs), the plan

Opened 2026-10-14 (night) from the user's screenshot (`trees_left_zoomed_in_right_zoomed_out.png`:
stair-stepped canopy edges up close, a single-pixel dither over whole canopies zoomed out) and one
RenderDoc capture of the faithful renderer at the user's max settings. Nothing is built. This sheet
is the plan for a future session; §1 is what the user asked for, §2 the facts, the rest derived.
Blocks hand in under `handins/FAA*.md`.

## 1. The ask (the user, 2026-10-14)

1. Reduce the pixelation of tree leaves at max settings.
2. It is **another entry in the Options dialog's "Enhanced graphics" tab** (the tab AOMENU built,
   `src/ext/ambient_occlusion/options_tab.cpp`), next to the AO combo and slider.
3. Plan now, build in a future session.

Not yet ruled (the user's call, §8): the menu shape (on/off vs a 3-way combo), whether the
distance-fill term (§3 part 2) ships in the first version, whether the shadow-map resolution is
part of this or its own entry.

## 2. Facts (the capture + the kit; file:line are the tree's on 2026-10-14)

**The capture.** `C:/GOG Games/Banished/rdoc_foliage/forest1.rdc` (debug exe at `7dbcaaf0e`, seed
12345, `BANISHED_CAMERA_PIN=town,45`, `BANISHED_CAPTURE_INGAME=300`; 2560x1377, 577 draws). The
autostart town was in **winter**, so the deciduous canopies are clipped bare by the season term
below. The conifers, twigs and every program listed here are in the frame, but **a summer capture
is FAA-SPIKE's first job** (the look checks need full canopies). Per-draw state:
`dump_state.py` (tools/dx11_state_diff), a summariser grouped by shader pair.

**The cause is the alpha test, not texture resolution.** Every cutout program is
`clip(alpha - 0.5)` -> DXBC `discard_nz`, so each pixel is all-leaf or all-background. MSAA is on
(the user's `VideoMultisample=3` = 4x; main colour + depth `2560x1377 ms4`) but MSAA only antialiases
triangle edges; a leaf edge is inside the card's triangle, so all four samples are kept or all are
killed together. `AlphaToCoverageEnable` is FALSE in every draw (retail-faithful,
`src/render/dx11/renderstatedata.cpp:339`). Zoomed out it gets worse: the mip chain averages alpha,
so in the small mips most texels sit near or below 0.5 and canopies turn into a sparse
single-pixel dither that shimmers as the camera moves (the right half of the screenshot).

**The shader sources exist.** The mod kit (`ToolKit/resource/Material/`, git-ignored, see
`docs/reference/binaries.md` "Modding toolkit") ships every `.srsl` (Shining Rock's shading
language, compiled by `Tools-x64.exe` to 4 DXBC variants, `DETAIL0..3`, = `kShaderVariantCount`).
The capture's DXBC matches the source line for line. The cutout programs:

| Pass | Program (`file:entry`) | Material | Cutoff | Captured as |
|---|---|---|---|---|
| colour | `BillboardMaterial.srsl:Normal_ps` | deciduous canopies: camera-facing leaf cards | `(a - season*move) - 0.5` | ps 2788 / 2596, vs 4260, NoCull, 256x256 BC3 9 mips |
| colour | `BillboardMaterial.srsl:Normal_psevergreen` | `BillboardEvergreenMaterial` | `a - 0.5` | |
| colour | `OpaqueMaterial.srsl:Foliage_ps` | `FoliageMaterial` | `a - 0.5` | |
| colour | `OpaqueMaterial.srsl:Normal_psalpha` | `AlphaTestMaterial` (props, twigs) | `a - 0.5` | ps 2596 (`e3b37064`, eids 2177-2198) |
| colour | `OpaqueMaterial.srsl:Snow_alphatest`, `SnowDamage_alphatest` | conifers with snow (season lerp) | `a - 0.5` | ps 2932 (eids 2085-2100, snow texture t1) |
| depth / shadow | `BillboardMaterial.srsl:Depth_ps` / `Depth_psevergreen` | the shadow casters of the above | `(a - season) - 0.5` / `a - 0.5` | ps 724 / 768 into the 2048 shadow map |
| depth / shadow | `OpaqueMaterial.srsl:DepthAlpha_ps` | AlphaTest / Foliage shadow casters | **`a - 0.25`** (retail: shadows are fuller than the visible canopy) | ps 700 (cutoff not disassembled; the source says 0.25) |

- `pc.userPosition.z` is the **season leaf-loss** term (the canopy thins through autumn, bare in
  winter); `pc.userPosition.y` the fall-colour lerp; `move` (lightFogMove.w) marks the swaying
  card vertices. The replacement must keep both exactly; it changes only what happens at the cutoff.
- Every opaque program writes **`o0.w = 0`** (`output.color.w = 0.0`). The alpha-tested ones too.
- **The main target's alpha is load-bearing.** The water shader (ps 3180, eids 2949-2958) reads a
  copy of the frame and blends `o0.xyz = lerp(frame.rgb, water, frame.a)`
  (`mad o0.xyz, r1.wwww, r0.xyzx, r1.xyzx`). Anything that writes a non-zero alpha into the main
  colour target changes the water. So **A2C must run with the alpha channel write-masked off**.
  (D3D11 takes the coverage from the shader's SV_Target0.a whatever the write mask is. FAA-SPIKE
  verifies this on this driver.)
- **The render state is shared.** `AlphaTestMaterial` / `FoliageMaterial` use
  `Material\Opaque\OpaqueMaterial.rsc:rs`, the SAME state object as every solid mesh (whose shader
  writes alpha 0: A2C on that state would make every building invisible). A2C cannot be a
  property of the retail RenderState; it has to be bound together with the replacement program.
- Programs carry no name at run time: `Video::ShaderProgram` is `_textures` / `_constants` /
  `_byteCode` (`src/engine/video/ShaderProgram.h:244-249`); the D3D object is made in
  `PixelProgramData::PlatformCreate` (`src/render/dx11/shaderprograms.cpp:197`) and bound per
  draw in `PixelProgramData::Bind` (`:223`). How the replacement is keyed is spike question S2.
- No runtime shader compiler in the exe. The offline path exists: `tools/ao_shaders.py` (fxc ->
  `const BYTE g_x[]` headers). Banished's DXBC is `ps_4_0`.
- **Textures** are not the problem. Leaf cards are 256x256 BC3 with a full mip chain, the sampler
  is anisotropic 16x (`VideoTextureFilter=16`), and the interior of a canopy is sharp in the
  screenshot. Higher-resolution leaf art is asset work, out of scope.
- **Shadows** (a separate candidate the user named): one 2048x2048 D24S8 sun map
  (`src/engine/graphics/sunlight.cpp:342-349`, `0x800` hard-coded; `VideoShadowScalar=0` = no
  down-shift, already the max), fitted to the bounding sphere of the WHOLE view
  (`src/engine/graphics/sun_camera.cpp:299-305`, texel = 2*radius*skew/2048), sampled 3x3 PCF at
  one texel spacing (`lighting.srsl` `GetShadowHigh`, `DETAIL3`). Zoomed out, a shadow texel covers
  many screen pixels, so the dappled self-shadow inside the canopy is blocky. That is a different
  artefact from the edge stair-steps; §7 keeps it as its own optional entry.

## 3. The shape

**Part 1 -- alpha-to-coverage with a sharpened alpha (the fix; 4x MSAA makes it nearly free).**
For each colour-pass cutout program, an HLSL port of the `.srsl` entry (all four `DETAIL` variants;
the shadow PCF differs per detail level) that is byte-for-byte the retail lighting, season and fog
math, except at the cutoff:

```hlsl
float a  = diff.w - seasonCut;                      // seasonCut = 0 for the evergreen / alphatest forms
float aa = (a - 0.5) / max(fwidth(a), 1e-4) + 0.5;  // Golus' "sharpened alpha": a 1-pixel ramp at the cutoff
// no clip(): coverage carries the edge; keep clip(a - 0.5 + eps) only if the spike shows depth-fighting
o0 = float4(colour, saturate(aa));                  // .w = coverage, write-masked off (see §2)
```

bound with a **clone of the draw's blend state** that has `AlphaToCoverageEnable = TRUE` and
`RenderTargetWriteMask = RGB`. With 4x MSAA the edge resolves to 4 coverage levels, so a smooth
silhouette with no sorting and no blending. Opaque depth semantics are kept (each covered sample
writes depth), so AO and the Blended list keep working as today.

**Part 2 -- distance fill (the zoomed-out dither).** Scale alpha up with the mip level so a canopy
keeps its mip-0 coverage at distance: `a *= 1 + max(0, lod) * kMipScale` with
`lod = diffuse.CalculateLevelOfDetail(sampler, uv)` (needs `ps_4_1`; Golus' ~0.25 to start). The
alternative is rescaling each mip's alpha at texture load to preserve coverage (Castaño 2010), but
the leaf textures are BC3: decode / rescale / re-encode at load is dearer and touches the loader,
so the shader route is the default. Same programs, no extra pass.

**When MSAA is off** (`VideoMultisample=0`), A2C does nothing. Then the replacement keeps a
`clip` (retail look), or hashed alpha testing (Wyman & McGuire 2017) is the stretch option.

**Not changed.** The depth / shadow programs keep their hard cutoffs (a shadow map has no coverage
to blend); the season / fall-colour behaviour; the retail RenderState objects; the faithful
profile (with the row off nothing is created, one null test per program bind).

```
src/render/dx11/foliage_aa/      the D3D side, the AO precedent (src/render/dx11/ao/)
  cutout.hlsl                    the ported colour-pass entries, one file, DETAIL as a define
  shaders/*.h                    fxc output (ps_4_1), regenerated by tools/foliage_shaders.py
  foliage_aa_stage.cpp/.h        the substitution table, the A2C blend-state clones
src/ext/foliage_aa/              the extension: Sync(), params, the seam set/clear
src/ext/ambient_occlusion/options_tab.cpp   + one combo (the tab is shared; rename it out of ao/ when a 2nd entry lands)
```

- **Seam** (DESIGN §3 rule 5): one null-by-default static pointer tested in
  `PixelProgramData::Bind`. When set, and the program is in the substitution table, bind the
  replacement shader and the A2C clone of the current blend state; otherwise the retail path
  unchanged. The faithful path pays one null test per program bind.
- **Settings**: the bool row `ext.foliage_aa` (Extensions dialog checkbox, as every row) plus,
  if the user wants the 3-way combo, a valued key `ext.foliage_aa_mode` (1 = smooth edges,
  2 = + distance fill) through AOMENU's valued-key code.
- **Device reset**: the replacement shaders and blend clones are re-created with the device
  (tracked like the AO targets).
- **Mods**: a mod shipping its own alpha material has its own DXBC, so it is not in the table and
  keeps the retail look. Correct by construction; say so in the menu tooltip.

## 4. Blocks

| Block | Owner | What | Done when |
|---|---|---|---|
| **FAA-SPIKE** | one opus agent | §5's facts, in a worktree, nothing merged but the hand-in | `handins/FAA-SPIKE.md` answers every row of §5 with evidence; §6 filled |
| **FAA-CORE** | one opus agent | the HLSL ports (all variants), the fxc tool, the seam, the substitution table, the A2C blend clones, the bool row, part 1 only | `tools/build.sh run` clean; a SUMMER capture shows the replaced programs with A2C on and RT alpha untouched (water identical); `trace_oracle` differs ONLY in those draws; faithful stress unchanged, row-on stress recorded; gate at 0 |
| **FAA-MENU** | one agent (sonnet is enough: AOMENU is the pattern) | the combo in the Enhanced graphics tab, the valued key if ruled, Apply / Cancel | the combo changes the live frame; settings.cfg round-trips |
| **FAA-TUNE** | one opus agent, then the user by hand | part 2 (mip scale), the ramp width, the MSAA-off fallback, the look at street level and overview, spring / summer / autumn / winter | the user's look ruling; numbers in the hand-in |

FAA-CORE and FAA-MENU can run in parallel once the spike is in. Agents: opus for the port,
`--jobs 2`, one game run at a time, merge main before the final build, the coordinator runs the gate.

## 5. The spike's questions

| # | Question | Why | Evidence |
|---|---|---|---|
| S1 | A summer (and an autumn) capture of a forest edge at street level and at the overview zoom: which programs draw the canopies, how many draws / pixels each, and does anything other than §2's table `discard` in the colour pass? | the substitution list is complete | two `.rdc` + the summariser output; how to get the season (a save, or a seed + month flag) |
| S2 | How is a program identified at Bind: a DXBC hash table (sha1 of `_byteCode[i]`, computed once at `PlatformCreate`), or the factory's resource key (`...srsl:Normal_ps`) if `ExternalFactory` keeps it? Cost per bind? | the seam must be O(1) and mod-safe | the chosen key, a `[FoliageAA] mapped N programs` line listing them |
| S3 | Does a hand-ported HLSL of `BillboardMaterial.srsl:Normal_ps` (DETAIL3) with the cutoff unchanged produce pixel-identical output to retail's DXBC? (Do this first, before changing the cutoff: it proves the port.) Same cbuffer layout (`_PixelConstants_` cb0[5]), same t0/t3/t5 + samplers? | a faithful base | RenderDoc `debug_shader_at_pixel` / a pixel compare on ONE capture replayed with both shaders (a replay compare is deterministic, unlike two runs) |
| S4 | A2C with `RenderTargetWriteMask = RGB` on this driver: is the coverage from `o0.w` honoured and RT alpha left untouched? Water identical? | §2 alpha is load-bearing | the RT alpha channel before / after one replaced draw, the water draw's input |
| S5 | Does dropping `clip()` cause problems: depth written for zero-coverage pixels (no, D3D writes depth per covered sample, verify), early-Z, AO depth resolve (sample 0 / min of MSAA depth) seeing leaf-edge samples, the `SelectionMask` pass? | correctness around the canopy | captures with the AO row on and off |
| S6 | GPU cost of A2C + `fwidth` in the stress town, row on vs off, RTSS stopped, machine load CLEAN | class A must be cheap | the two stress rows |
| S7 | MSAA off: what does the ramp look like with A2C ignored? Keep `clip` in a `!MSAA` variant, or select per frame from `Configuration::multisample`? | the fallback | screenshots at `VideoMultisample=0` |

## 6. Decided by the spike (filled in when FAA-SPIKE hands in)

(empty)

## 7. Related, kept separate (not part of this row unless the user says so)

- **Shadow-map resolution** (`ext.shadow_res`?): `0x800` -> 4096 behind a row (two
  `sunlight.cpp` sites + the texel-snap divisor follows `Width()` already). 4x the shadow memory
  (16 -> 64 MB at D24S8, the same again for the B8G8R8A8 colour target `sunlight.cpp` creates alongside) and fill cost. Helps the blocky canopy self-shadow at overview zoom, not
  the edges. A cascade split would be a much bigger job.
- **Shadow PCF width**: the 3x3 taps at one texel are what the kit ships at `DETAIL3`; a wider
  rotated-disc kernel would soften the dappling. Needs the same program substitution (every
  receiver program), so it is cheap only AFTER FAA-CORE has built the substitution seam.
- **Higher-resolution leaf textures**: asset work; loose files override the `.pkg`. Out of scope.

## 8. For the user to rule before FAA-CORE

1. Menu: an On / Off checkbox, or a combo Off / Smooth edges / Smooth edges + distance fill?
2. Part 2 in the first version, or after FAA-TUNE has looked at part 1 alone?
3. Shadow resolution (§7): a second entry in the same tab, a separate row later, or not at all?
